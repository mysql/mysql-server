# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+
# |                             tbl43.inc                              |
# |                        Copyright 2006 by MySQL AB                  |
# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+
--disable_query_log
CREATE TABLE "AAABINOE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABMRGE" varchar(4) NOT NULL default '0000',
  "AAABINWK" varchar(4) NOT NULL default '0000',
  "AAABMQKL" varchar(4) NOT NULL default '0000',
  "AAABINXG" decimal(13,3) NOT NULL default '0.000',
  "AAABINXH" varchar(3) NOT NULL default ' ',
  "AAABMOUK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIN","AAABMRGE","AAABINWK","AAABMQKL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABINOS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABINPA" varchar(12) NOT NULL default ' ',
  "AAABINPB" varchar(4) NOT NULL default ' ',
  "AAABINPC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIN","AAABINPA","AAABINPB","AAABINPC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABINPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABMRIO" varchar(14) NOT NULL default ' ',
  "AAABMICK" varchar(4) NOT NULL default '0000',
  "AAABINPW" varchar(8) NOT NULL default '00000000',
  "AAABMICS" varchar(4) NOT NULL default '0000',
  "AAABMRIF" varchar(1) NOT NULL default ' ',
  "AAABMRIG" varchar(1) NOT NULL default ' ',
  "AAABMRIH" varchar(12) NOT NULL default ' ',
  "AAABMRII" varchar(8) NOT NULL default '00000000',
  "AAABMRIJ" varchar(12) NOT NULL default ' ',
  "AAABMRIK" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRIN","AAABMRIO","AAABMICK","AAABINPW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABINPX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABIRXN" varchar(4) NOT NULL default ' ',
  "AAABMOUK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIN","AAABIRXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABINQF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABISUD" varchar(4) NOT NULL default '0000',
  "AAABINQW" varchar(8) NOT NULL default '00000000',
  "AAABINQX" varchar(8) NOT NULL default '00000000',
  "AAABMRIO" varchar(14) NOT NULL default ' ',
  "AAABMOUK" varchar(1) NOT NULL default ' ',
  "AAABMRIH" varchar(12) NOT NULL default ' ',
  "AAABMRII" varchar(8) NOT NULL default '00000000',
  "AAABMRIJ" varchar(12) NOT NULL default ' ',
  "AAABMRIK" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRIN","AAABISUD","AAABINQW","AAABINQX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABINQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABMQKL" varchar(4) NOT NULL default '0000',
  "AAABINXG" decimal(13,3) NOT NULL default '0.000',
  "AAABINXH" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIN","AAABMQKL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABINRH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABISTE" varchar(4) NOT NULL default '0000',
  "AAABINRL" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABISTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABINRP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABISTE" varchar(4) NOT NULL default '0000',
  "AAABMOUK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIN","AAABISTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABINRX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABINTW" varchar(4) NOT NULL default '0000',
  "AAABINTI" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIN","AAABINTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABINSF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABINTW" varchar(4) NOT NULL default '0000',
  "AAABINSZ" varchar(10) NOT NULL default ' ',
  "AAABMRIO" varchar(14) NOT NULL default ' ',
  "AAABINSM" varchar(8) NOT NULL default '00000000',
  "AAABINSN" decimal(5,2) NOT NULL default '0.00',
  "AAABINSO" decimal(12,2) NOT NULL default '0.00',
  "AAABINSP" varchar(5) NOT NULL default ' ',
  "AAABMOUK" varchar(1) NOT NULL default ' ',
  "AAABMRIH" varchar(12) NOT NULL default ' ',
  "AAABMRII" varchar(8) NOT NULL default '00000000',
  "AAABMRIJ" varchar(12) NOT NULL default ' ',
  "AAABMRIK" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRIN","AAABINTW","AAABINSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABINTA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABINTW" varchar(4) NOT NULL default '0000',
  "AAABINTF" varchar(8) NOT NULL default '00000000',
  "AAABMRIO" varchar(14) NOT NULL default ' ',
  "AAABINTH" varchar(10) NOT NULL default ' ',
  "AAABINTI" varchar(25) NOT NULL default ' ',
  "AAABINTJ" decimal(13,3) NOT NULL default '0.000',
  "AAABINTK" varchar(3) NOT NULL default ' ',
  "AAABINTL" decimal(12,2) NOT NULL default '0.00',
  "AAABINTM" varchar(5) NOT NULL default ' ',
  "AAABMRIF" varchar(1) NOT NULL default ' ',
  "AAABMRIG" varchar(1) NOT NULL default ' ',
  "AAABMRIH" varchar(12) NOT NULL default ' ',
  "AAABMRII" varchar(8) NOT NULL default '00000000',
  "AAABMRIJ" varchar(12) NOT NULL default ' ',
  "AAABMRIK" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRIN","AAABINTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABINTX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABMRIL","AAABMRIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABINUC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABINVC" varchar(10) NOT NULL default ' ',
  "AAABINUF" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABINVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABINUI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABIRXK" varchar(4) NOT NULL default ' ',
  "AAABIRXL" varchar(2) NOT NULL default ' ',
  "AAABIRXM" varchar(2) NOT NULL default ' ',
  "AAABINVC" varchar(10) NOT NULL default ' ',
  "AAABISTM" varchar(2) NOT NULL default ' ',
  "AAABISTN" varchar(2) NOT NULL default ' ',
  "AAABINVF" varchar(8) NOT NULL default '00000000',
  "AAABINUS" varchar(8) NOT NULL default '00000000',
  "AAABISTJ" varchar(2) NOT NULL default ' ',
  "AAABMRFP" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRIN","AAABIRXK","AAABIRXL","AAABIRXM","AAABINVC","AAABISTM","AAABISTN","AAABINVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABINVG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABISTM" varchar(2) NOT NULL default ' ',
  "AAABINVK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABISTM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABINVO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABMRGE" varchar(4) NOT NULL default '0000',
  "AAABISTS" varchar(4) NOT NULL default '0000',
  "AAABINVT" varchar(50) NOT NULL default ' ',
  "AAABMOUF" varchar(4) NOT NULL default '0000',
  "AAABMRIF" varchar(1) NOT NULL default ' ',
  "AAABMRIG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIN","AAABMRGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABINWA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABMRGE" varchar(4) NOT NULL default '0000',
  "AAABINWK" varchar(4) NOT NULL default '0000',
  "AAABINWF" varchar(25) NOT NULL default ' ',
  "AAABMOUK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIN","AAABMRGE","AAABINWK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABINWL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABMRGE" varchar(4) NOT NULL default '0000',
  "AAABMRIO" varchar(14) NOT NULL default ' ',
  "AAABMICK" varchar(4) NOT NULL default '0000',
  "AAABINXA" varchar(8) NOT NULL default '00000000',
  "AAABMICS" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRIN","AAABMRGE","AAABMRIO","AAABMICK","AAABINXA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABINXB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABMRGE" varchar(4) NOT NULL default '0000',
  "AAABMQKL" varchar(4) NOT NULL default '0000',
  "AAABINXG" decimal(13,3) NOT NULL default '0.000',
  "AAABINXH" varchar(3) NOT NULL default ' ',
  "AAABMOUK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIN","AAABMRGE","AAABMQKL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABINXN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMGGF" varchar(30) NOT NULL default ' ',
  "AAABINXS" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMRIM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABINXW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABISTS" varchar(4) NOT NULL default '0000',
  "AAABINYA" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABISTS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABINYE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABISUD" varchar(4) NOT NULL default '0000',
  "AAABINYI" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABISUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABINYM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABISTY" varchar(4) NOT NULL default '0000',
  "AAABINYQ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABISTY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABINYU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABISUM" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABISUM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABINYZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABISUM" varchar(4) NOT NULL default '0000',
  "AAABINZD" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABISUM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABINZH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNXA" varchar(4) NOT NULL default '0000',
  "AAABMINM" varchar(13) NOT NULL default '0000000000000',
  "AAABMIMR" varchar(4) NOT NULL default '0000',
  "AAABINZS" varchar(30) NOT NULL default ' ',
  "AAABINZN" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNXA","AAABMINM","AAABMIMR","AAABINZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABINZT" (
  "AAABMCUW" varchar(3) NOT NULL default '000',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMCUW","AAABMQIJ","AAABMPQE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIOAA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKWJY" varchar(3) NOT NULL default ' ',
  "AAABIOAK" varchar(12) NOT NULL default ' ',
  "AAABIOAE" varchar(12) NOT NULL default ' ',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABJTAJ" varchar(6) NOT NULL default '000000',
  "AAABIOAH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKWJY","AAABIOAK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIOAL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLDM" varchar(4) NOT NULL default ' ',
  "AAABMEJX" varchar(4) NOT NULL default '0000',
  "AAABMODF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLDM","AAABMEJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIOAT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABIOBO" varchar(30) NOT NULL default ' ',
  "AAABIOBE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM","AAABIOBO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIOBP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABMNJQ" varchar(4) NOT NULL default ' ',
  "AAABLCOI" varchar(1) NOT NULL default ' ',
  "AAABIOCA" varchar(1) NOT NULL default ' ',
  "AAABIOCB" varchar(1) NOT NULL default ' ',
  "AAABIOCC" varchar(1) NOT NULL default ' ',
  "AAABIOCD" varchar(1) NOT NULL default ' ',
  "AAABIOCE" varchar(12) NOT NULL default ' ',
  "AAABIOCF" varchar(8) NOT NULL default '00000000',
  "AAABKOVS" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIOCP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABIOCT" varchar(30) NOT NULL default ' ',
  "AAABIOCU" varchar(15) NOT NULL default ' ',
  "AAABIOCV" varchar(15) NOT NULL default ' ',
  "AAABMMBE" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMNVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIODA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABLECQ" varchar(30) NOT NULL default ' ',
  "AAABMLQP" varchar(8) NOT NULL default ' ',
  "AAABMREN" varchar(40) NOT NULL default ' ',
  "AAABMREO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRGQ","AAABLECQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIODM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOSU" varchar(40) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMLMY" varchar(6) NOT NULL default '000000',
  "AAABMLNA" varchar(2) NOT NULL default '00',
  "AAABMLNB" varchar(1) NOT NULL default ' ',
  "AAABMLNC" varchar(1) NOT NULL default ' ',
  "AAABMLND" varchar(1) NOT NULL default ' ',
  "AAABMLNF" varchar(1) NOT NULL default ' ',
  "AAABMLNG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOSU","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIOEA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLOSU" varchar(40) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABIOEF" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLOSU","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIOEK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOSU" varchar(40) NOT NULL default ' ',
  "AAABMORY" varchar(15) NOT NULL default ' ',
  "AAABMKDE" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABIOET" varchar(4) NOT NULL default '0000',
  "AAABMORN" varchar(15) NOT NULL default ' ',
  "AAABMORO" varchar(15) NOT NULL default ' ',
  "AAABMORP" varchar(15) NOT NULL default ' ',
  "AAABMORQ" varchar(15) NOT NULL default ' ',
  "AAABMORR" varchar(15) NOT NULL default ' ',
  "AAABMORS" varchar(30) NOT NULL default ' ',
  "AAABMKCE" decimal(1,0) NOT NULL default '0',
  "AAABMKCF" varchar(1) NOT NULL default ' ',
  "AAABMKCG" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOSU","AAABMORY","AAABMKDE","AAABMRDC","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIOFJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIOGA" varchar(8) NOT NULL default '00000000',
  "AAABIOFM" varchar(30) NOT NULL default ' ',
  "AAABIOFN" varchar(30) NOT NULL default ' ',
  "AAABIOFO" varchar(30) NOT NULL default ' ',
  "AAABIOFP" varchar(30) NOT NULL default ' ',
  "AAABIOFQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIOGA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIOFT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIOGA" varchar(8) NOT NULL default '00000000',
  "AAABLRTL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABIOGA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIOGB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABMGSM" varchar(8) NOT NULL default '00000000',
  "AAABMPUP" varchar(10) NOT NULL default ' ',
  "AAABMPUQ" varchar(6) NOT NULL default '000000',
  "AAABMPPH" varchar(8) NOT NULL default '00000000',
  "AAABMPPI" varchar(8) NOT NULL default '00000000',
  "AAABMPSF" varchar(1) NOT NULL default ' ',
  "AAABMPSG" varchar(1) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMPPC","AAABMPPD","AAABMPPE","AAABMGSM","AAABMPUP","AAABMPUQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIOGX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUP" varchar(10) NOT NULL default ' ',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABIOHD" varchar(10) NOT NULL default ' ',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMRDD" varchar(1) NOT NULL default ' ',
  "AAABMPPH" varchar(8) NOT NULL default '00000000',
  "AAABMPPI" varchar(8) NOT NULL default '00000000',
  "AAABMPPJ" varchar(4) NOT NULL default ' ',
  "AAABMPPK" varchar(4) NOT NULL default ' ',
  "AAABMPPL" varchar(2) NOT NULL default ' ',
  "AAABMPPM" varchar(2) NOT NULL default ' ',
  "AAABMPPN" varchar(4) NOT NULL default ' ',
  "AAABMPPO" varchar(8) NOT NULL default ' ',
  "AAABMPPP" varchar(8) NOT NULL default '00000000',
  "AAABMPPQ" varchar(12) NOT NULL default ' ',
  "AAABMPPR" varchar(8) NOT NULL default '00000000',
  "AAABMPPS" varchar(6) NOT NULL default '000000',
  "AAABMPUM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPUP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIOHY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUP" varchar(10) NOT NULL default ' ',
  "AAABMPUQ" varchar(6) NOT NULL default '000000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPQG" varchar(4) NOT NULL default ' ',
  "AAABMPQH" varchar(6) NOT NULL default '000000',
  "AAABMPQI" varchar(4) NOT NULL default '0000',
  "AAABMPQJ" varchar(8) NOT NULL default ' ',
  "AAABMPQK" varchar(4) NOT NULL default '0000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMRCJ" varchar(8) NOT NULL default '00000000',
  "AAABMPQQ" varchar(4) NOT NULL default '0000',
  "AAABMPQR" varchar(4) NOT NULL default ' ',
  "AAABMPQS" varchar(1) NOT NULL default ' ',
  "AAABMPQT" varchar(3) NOT NULL default ' ',
  "AAABMPQU" varchar(1) NOT NULL default ' ',
  "AAABMPQV" varchar(10) NOT NULL default ' ',
  "AAABMPQW" varchar(1) NOT NULL default ' ',
  "AAABMPQX" varchar(6) NOT NULL default '000000',
  "AAABMPQY" varchar(1) NOT NULL default ' ',
  "AAABMPQZ" varchar(8) NOT NULL default '00000000',
  "AAABMPRA" varchar(1) NOT NULL default ' ',
  "AAABMPRB" varchar(1) NOT NULL default ' ',
  "AAABMPRC" varchar(4) NOT NULL default '0000',
  "AAABMPRD" varchar(8) NOT NULL default ' ',
  "AAABMPRE" varchar(6) NOT NULL default '000000',
  "AAABMPRF" varchar(4) NOT NULL default ' ',
  "AAABMPRG" varchar(2) NOT NULL default ' ',
  "AAABMPRH" varchar(2) NOT NULL default ' ',
  "AAABMPRI" varchar(4) NOT NULL default ' ',
  "AAABMPRJ" varchar(4) NOT NULL default ' ',
  "AAABMPRK" varchar(13) NOT NULL default '0000000000000',
  "AAABMPRL" varchar(10) NOT NULL default ' ',
  "AAABMPRM" varchar(10) NOT NULL default ' ',
  "AAABMPRN" varchar(10) NOT NULL default ' ',
  "AAABMPRO" varchar(10) NOT NULL default ' ',
  "AAABMPRP" varchar(10) NOT NULL default ' ',
  "AAABMPRQ" varchar(10) NOT NULL default ' ',
  "AAABMPRR" varchar(10) NOT NULL default ' ',
  "AAABMPRS" varchar(10) NOT NULL default ' ',
  "AAABMPRT" varchar(10) NOT NULL default ' ',
  "AAABMPRU" varchar(10) NOT NULL default ' ',
  "AAABMPRV" varchar(10) NOT NULL default ' ',
  "AAABMPRW" varchar(8) NOT NULL default '00000000',
  "AAABMPRX" decimal(16,2) NOT NULL default '0.00',
  "AAABMPRY" varchar(5) NOT NULL default ' ',
  "AAABMPRZ" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMPSB" varchar(18) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABMPSD" varchar(4) NOT NULL default ' ',
  "AAABMPSE" varchar(1) NOT NULL default ' ',
  "AAABMPSF" varchar(1) NOT NULL default ' ',
  "AAABMPSG" varchar(1) NOT NULL default ' ',
  "AAABMPSH" varchar(1) NOT NULL default ' ',
  "AAABMPSI" varchar(4) NOT NULL default ' ',
  "AAABMPSJ" varchar(2) NOT NULL default ' ',
  "AAABMPSK" varchar(8) NOT NULL default '00000000',
  "AAABMPSL" varchar(8) NOT NULL default '00000000',
  "AAABMPSM" varchar(8) NOT NULL default '00000000',
  "AAABMPSN" varchar(8) NOT NULL default '00000000',
  "AAABMPSO" varchar(4) NOT NULL default ' ',
  "AAABMPSP" varchar(2) NOT NULL default ' ',
  "AAABMPSQ" varchar(2) NOT NULL default ' ',
  "AAABMPSR" varchar(4) NOT NULL default ' ',
  "AAABMPSS" varchar(4) NOT NULL default ' ',
  "AAABMPST" varchar(13) NOT NULL default '0000000000000',
  "AAABMPSU" decimal(16,2) NOT NULL default '0.00',
  "AAABMPSV" varchar(5) NOT NULL default ' ',
  "AAABMPSW" decimal(9,5) NOT NULL default '0.00000',
  "AAABMPSX" varchar(4) NOT NULL default ' ',
  "AAABMPSY" decimal(7,3) NOT NULL default '0.000',
  "AAABMPSZ" decimal(7,3) NOT NULL default '0.000',
  "AAABMPTA" decimal(7,3) NOT NULL default '0.000',
  "AAABMPTB" decimal(7,3) NOT NULL default '0.000',
  "AAABMPTC" decimal(16,2) NOT NULL default '0.00',
  "AAABMPTD" varchar(5) NOT NULL default ' ',
  "AAABMPTE" varchar(6) NOT NULL default ' ',
  "AAABMPTF" varchar(10) NOT NULL default ' ',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAABMPTH" varchar(1) NOT NULL default '0',
  "AAABMPTI" varchar(6) NOT NULL default '000000',
  "AAABMPTJ" int(11) NOT NULL default '0',
  "AAABMPTK" varchar(18) NOT NULL default ' ',
  "AAABMPTL" varchar(18) NOT NULL default ' ',
  "AAABMPTM" decimal(15,5) NOT NULL default '0.00000',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAABMPTO" decimal(15,5) NOT NULL default '0.00000',
  "AAABMPTP" varchar(3) NOT NULL default ' ',
  "AAABMPTQ" decimal(16,2) NOT NULL default '0.00',
  "AAABMPTR" varchar(5) NOT NULL default ' ',
  "AAABMPTS" varchar(10) NOT NULL default ' ',
  "AAABMPTT" varchar(10) NOT NULL default ' ',
  "AAABMPTU" varchar(12) NOT NULL default ' ',
  "AAABMPTV" varchar(10) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABMPTX" decimal(13,2) NOT NULL default '0.00',
  "AAABMPTY" decimal(13,2) NOT NULL default '0.00',
  "AAABMPTZ" varchar(5) NOT NULL default ' ',
  "AAABMPUA" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMPUC" varchar(50) NOT NULL default ' ',
  "AAABMPUD" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPUE" varchar(6) NOT NULL default ' ',
  "AAABMPUF" varchar(6) NOT NULL default ' ',
  "AAABMPUG" varchar(8) NOT NULL default '00000000',
  "AAABMPUH" varchar(3) NOT NULL default ' ',
  "AAABMPUI" varchar(5) NOT NULL default ' ',
  "AAABMPUJ" decimal(15,5) NOT NULL default '0.00000',
  "AAABMPUK" decimal(16,2) NOT NULL default '0.00',
  "AAABMPUL" varchar(4) NOT NULL default '0000',
  "AAABMPUM" varchar(1) NOT NULL default ' ',
  "AAABMPUN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPUP","AAABMPUQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIOMQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMNVB" varchar(4) NOT NULL default '0000',
  "AAABMMOQ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIONL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMNVB" varchar(4) NOT NULL default '0000',
  "AAABMMTL" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIOOG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMMPU" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIOPA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABIOPK" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIOPU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABIOQE" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIOQO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABIOQY" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIORI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMNVB" varchar(4) NOT NULL default '0000',
  "AAABMMRY" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIOSD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMQTO" varchar(13) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIOSX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMRFB" varchar(5) NOT NULL default ' ',
  "AAABIQJD" varchar(2) NOT NULL default ' ',
  "AAABIOTJ" decimal(16,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIOTT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIOUN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABIOUX" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIOVH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABIQJD" varchar(2) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABIOVT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIOWD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABIOWN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIOWX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABIOXH" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIOXR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABIOYB" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIOYL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMKLV" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIOZF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMQRG" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIOZZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMMZS" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPAT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABMNLX" varchar(2) NOT NULL default ' ',
  "AAABMNLY" varchar(4) NOT NULL default ' ',
  "AAABMNNR" varchar(12) NOT NULL default ' ',
  "AAABMNNC" varchar(25) NOT NULL default ' ',
  "AAABMNLP" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPBT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMNEB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPCN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMLBI" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPDH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMNDG" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPEB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMNTS" varchar(4) NOT NULL default '0000',
  "AAABMNVB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPEW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMNEE" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPFQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABIQJD" varchar(2) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPGL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMLPE" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPHF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMHCF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMPSJ" varchar(2) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPIU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMCUU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPJO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAABIQJD" varchar(2) NOT NULL default ' ',
  "AAABIPKA" decimal(15,5) NOT NULL default '0.00000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPKK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMPSD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPLE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMNIF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPLY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMLXL" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPMS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMNVB" varchar(4) NOT NULL default '0000',
  "AAABMNIU" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPNN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABIPNX" varchar(1) NOT NULL default ' ',
  "AAABIPNY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPOI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMMAR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPPC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABIQJD" varchar(2) NOT NULL default ' ',
  "AAABKOEQ" decimal(7,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPPX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMEPN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPQR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMQVS" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPRL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMNNW" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPSF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMQVR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPSZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMNOB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPTT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMMHY" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPUN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMQWS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPVH" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABLXVF" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPWB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPWV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMNOG" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPXP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMMRM" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPYJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMQWS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPZD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABIPZN" varchar(30) NOT NULL default ' ',
  "AAABIPZO" varchar(30) NOT NULL default ' ',
  "AAABIPZP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIPZZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMNVB" varchar(4) NOT NULL default '0000',
  "AAABIQGU" varchar(4) NOT NULL default ' ',
  "AAABIQGV" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIQAV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABIQGU" varchar(4) NOT NULL default ' ',
  "AAABIQGV" varchar(6) NOT NULL default '000000',
  "AAABKWRL" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIQBR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMNVB" varchar(4) NOT NULL default '0000',
  "AAABMOEX" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIQCM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMNVB" varchar(4) NOT NULL default '0000',
  "AAABMOEX" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIQDH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMNVB" varchar(4) NOT NULL default '0000',
  "AAABMOEX" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIQEC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMOFL" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIQEW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIQFQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIQGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABIQGU" varchar(4) NOT NULL default ' ',
  "AAABIQGV" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIQHF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMOMK" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIQHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMRFB" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIQIT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABIQJD" varchar(2) NOT NULL default ' ',
  "AAABIQJE" varchar(1) NOT NULL default ' ',
  "AAABKWRL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIQJP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMOQL" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIQKJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMPSI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIQLD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABLCXR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIQLX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMHBZ" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIQMT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABIQNL" varchar(9) NOT NULL default '000000000',
  "AAABIQNM" varchar(9) NOT NULL default '000000000',
  "AAABMOQS" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOGK","AAABMNJK","AAABMNVC","AAABMORC","AAABIQNL","AAABIQNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIQNN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMMTE" varchar(4) NOT NULL default ' ',
  "AAABMMTF" varchar(6) NOT NULL default '000000',
  "AAABMGUX" varchar(4) NOT NULL default '0000',
  "AAABMGSM" varchar(8) NOT NULL default '00000000',
  "AAABMPUD" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPPH" varchar(8) NOT NULL default '00000000',
  "AAABMPPI" varchar(8) NOT NULL default '00000000',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMMTE","AAABMMTF","AAABMGUX","AAABMGSM","AAABMPUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIQOF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABMRGE" varchar(4) NOT NULL default '0000',
  "AAABIQOJ" decimal(3,0) NOT NULL default '0',
  "AAABIQOK" decimal(3,0) NOT NULL default '0',
  "AAABIQOL" decimal(3,0) NOT NULL default '0',
  "AAABIQOM" decimal(3,0) NOT NULL default '0',
  "AAABIQON" decimal(3,0) NOT NULL default '0',
  "AAABIQOO" decimal(3,0) NOT NULL default '0',
  "AAABIQOP" varchar(1) NOT NULL default ' ',
  "AAABIQOQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIN","AAABMRGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIQOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUP" varchar(10) NOT NULL default ' ',
  "AAABMPUQ" varchar(6) NOT NULL default '000000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABIQPB" varchar(4) NOT NULL default ' ',
  "AAABIQPC" varchar(4) NOT NULL default ' ',
  "AAABIQPD" varchar(1) NOT NULL default ' ',
  "AAABMPTK" varchar(18) NOT NULL default ' ',
  "AAABIQPF" varchar(1) NOT NULL default ' ',
  "AAABIQPG" varchar(1) NOT NULL default ' ',
  "AAABIQPH" varchar(40) NOT NULL default ' ',
  "AAABIQPI" varchar(1) NOT NULL default ' ',
  "AAABMPRL" varchar(10) NOT NULL default ' ',
  "AAABMPRM" varchar(10) NOT NULL default ' ',
  "AAABMPRN" varchar(10) NOT NULL default ' ',
  "AAABMPRO" varchar(10) NOT NULL default ' ',
  "AAABIQPN" varchar(1) NOT NULL default ' ',
  "AAABIQPO" varchar(50) NOT NULL default ' ',
  "AAABIQPP" varchar(6) NOT NULL default '000000',
  "AAABIQPQ" varchar(4) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPUP","AAABMPUQ"),
  KEY "AAABIQPV" ("AAABMPUP"),
  KEY "AAABIQPX" ("AAABMRHL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIQPZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUP" varchar(10) NOT NULL default ' ',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMRDD" varchar(1) NOT NULL default ' ',
  "AAABMPPH" varchar(8) NOT NULL default '00000000',
  "AAABMPPI" varchar(8) NOT NULL default '00000000',
  "AAABMPPJ" varchar(4) NOT NULL default ' ',
  "AAABMPPK" varchar(4) NOT NULL default ' ',
  "AAABMPPL" varchar(2) NOT NULL default ' ',
  "AAABMPPM" varchar(2) NOT NULL default ' ',
  "AAABMPPN" varchar(4) NOT NULL default ' ',
  "AAABMPPO" varchar(8) NOT NULL default ' ',
  "AAABMPPP" varchar(8) NOT NULL default '00000000',
  "AAABMPPQ" varchar(12) NOT NULL default ' ',
  "AAABMPPR" varchar(8) NOT NULL default '00000000',
  "AAABMPPS" varchar(6) NOT NULL default '000000',
  "AAABMPUM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPUP"),
  KEY "AAABIQQZ" ("AAABMRIL","AAABMRHL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIQRC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUP" varchar(10) NOT NULL default ' ',
  "AAABMPUQ" varchar(6) NOT NULL default '000000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPQG" varchar(4) NOT NULL default ' ',
  "AAABMPQH" varchar(6) NOT NULL default '000000',
  "AAABMPQI" varchar(4) NOT NULL default '0000',
  "AAABMPQJ" varchar(8) NOT NULL default ' ',
  "AAABMPQK" varchar(4) NOT NULL default '0000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMRCJ" varchar(8) NOT NULL default '00000000',
  "AAABMPQQ" varchar(4) NOT NULL default '0000',
  "AAABMPQR" varchar(4) NOT NULL default ' ',
  "AAABMPQS" varchar(1) NOT NULL default ' ',
  "AAABMPQT" varchar(3) NOT NULL default ' ',
  "AAABMPQU" varchar(1) NOT NULL default ' ',
  "AAABMPQV" varchar(10) NOT NULL default ' ',
  "AAABMPQW" varchar(1) NOT NULL default ' ',
  "AAABMPQX" varchar(6) NOT NULL default '000000',
  "AAABMPQY" varchar(1) NOT NULL default ' ',
  "AAABMPQZ" varchar(8) NOT NULL default '00000000',
  "AAABMPRA" varchar(1) NOT NULL default ' ',
  "AAABMPRB" varchar(1) NOT NULL default ' ',
  "AAABMPRC" varchar(4) NOT NULL default '0000',
  "AAABMPRD" varchar(8) NOT NULL default ' ',
  "AAABMPRE" varchar(6) NOT NULL default '000000',
  "AAABMPRF" varchar(4) NOT NULL default ' ',
  "AAABMPRG" varchar(2) NOT NULL default ' ',
  "AAABMPRH" varchar(2) NOT NULL default ' ',
  "AAABMPRI" varchar(4) NOT NULL default ' ',
  "AAABMPRJ" varchar(4) NOT NULL default ' ',
  "AAABMPRK" varchar(13) NOT NULL default '0000000000000',
  "AAABMPRL" varchar(10) NOT NULL default ' ',
  "AAABMPRM" varchar(10) NOT NULL default ' ',
  "AAABMPRN" varchar(10) NOT NULL default ' ',
  "AAABMPRO" varchar(10) NOT NULL default ' ',
  "AAABMPRP" varchar(10) NOT NULL default ' ',
  "AAABMPRQ" varchar(10) NOT NULL default ' ',
  "AAABMPRR" varchar(10) NOT NULL default ' ',
  "AAABMPRS" varchar(10) NOT NULL default ' ',
  "AAABMPRT" varchar(10) NOT NULL default ' ',
  "AAABMPRU" varchar(10) NOT NULL default ' ',
  "AAABMPRV" varchar(10) NOT NULL default ' ',
  "AAABMPRW" varchar(8) NOT NULL default '00000000',
  "AAABMPRX" decimal(16,2) NOT NULL default '0.00',
  "AAABMPRY" varchar(5) NOT NULL default ' ',
  "AAABMPRZ" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMPSB" varchar(18) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABMPSD" varchar(4) NOT NULL default ' ',
  "AAABMPSE" varchar(1) NOT NULL default ' ',
  "AAABMPSF" varchar(1) NOT NULL default ' ',
  "AAABMPSG" varchar(1) NOT NULL default ' ',
  "AAABMPSH" varchar(1) NOT NULL default ' ',
  "AAABMPSI" varchar(4) NOT NULL default ' ',
  "AAABMPSJ" varchar(2) NOT NULL default ' ',
  "AAABMPSK" varchar(8) NOT NULL default '00000000',
  "AAABMPSL" varchar(8) NOT NULL default '00000000',
  "AAABMPSM" varchar(8) NOT NULL default '00000000',
  "AAABMPSN" varchar(8) NOT NULL default '00000000',
  "AAABMPSO" varchar(4) NOT NULL default ' ',
  "AAABMPSP" varchar(2) NOT NULL default ' ',
  "AAABMPSQ" varchar(2) NOT NULL default ' ',
  "AAABMPSR" varchar(4) NOT NULL default ' ',
  "AAABMPSS" varchar(4) NOT NULL default ' ',
  "AAABMPST" varchar(13) NOT NULL default '0000000000000',
  "AAABMPSU" decimal(16,2) NOT NULL default '0.00',
  "AAABMPSV" varchar(5) NOT NULL default ' ',
  "AAABMPSW" decimal(9,5) NOT NULL default '0.00000',
  "AAABMPSX" varchar(4) NOT NULL default ' ',
  "AAABMPSY" decimal(7,3) NOT NULL default '0.000',
  "AAABMPSZ" decimal(7,3) NOT NULL default '0.000',
  "AAABMPTA" decimal(7,3) NOT NULL default '0.000',
  "AAABMPTB" decimal(7,3) NOT NULL default '0.000',
  "AAABMPTC" decimal(16,2) NOT NULL default '0.00',
  "AAABMPTD" varchar(5) NOT NULL default ' ',
  "AAABMPTE" varchar(6) NOT NULL default ' ',
  "AAABMPTF" varchar(10) NOT NULL default ' ',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAABMPTH" varchar(1) NOT NULL default '0',
  "AAABMPTI" varchar(6) NOT NULL default '000000',
  "AAABMPTJ" int(11) NOT NULL default '0',
  "AAABMPTK" varchar(18) NOT NULL default ' ',
  "AAABMPTL" varchar(18) NOT NULL default ' ',
  "AAABMPTM" decimal(15,5) NOT NULL default '0.00000',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAABMPTO" decimal(15,5) NOT NULL default '0.00000',
  "AAABMPTP" varchar(3) NOT NULL default ' ',
  "AAABMPTQ" decimal(16,2) NOT NULL default '0.00',
  "AAABMPTR" varchar(5) NOT NULL default ' ',
  "AAABMPTS" varchar(10) NOT NULL default ' ',
  "AAABMPTT" varchar(10) NOT NULL default ' ',
  "AAABMPTU" varchar(12) NOT NULL default ' ',
  "AAABMPTV" varchar(10) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABMPTX" decimal(13,2) NOT NULL default '0.00',
  "AAABMPTY" decimal(13,2) NOT NULL default '0.00',
  "AAABMPTZ" varchar(5) NOT NULL default ' ',
  "AAABMPUA" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMPUC" varchar(50) NOT NULL default ' ',
  "AAABMPUD" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPUE" varchar(6) NOT NULL default ' ',
  "AAABMPUF" varchar(6) NOT NULL default ' ',
  "AAABMPUG" varchar(8) NOT NULL default '00000000',
  "AAABMPUH" varchar(3) NOT NULL default ' ',
  "AAABMPUI" varchar(5) NOT NULL default ' ',
  "AAABMPUJ" decimal(15,5) NOT NULL default '0.00000',
  "AAABMPUK" decimal(16,2) NOT NULL default '0.00',
  "AAABMPUL" varchar(4) NOT NULL default '0000',
  "AAABMPUM" varchar(1) NOT NULL default ' ',
  "AAABMPUN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPUP","AAABMPUQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIQVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIQWE" varchar(10) NOT NULL default '0000000000',
  "AAABISXO" varchar(10) NOT NULL default '0000000000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABIQWC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIQWE","AAABISXO","AAABMRHL","AAABMPPC","AAABMPPD","AAABMPPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIQWK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUP" varchar(10) NOT NULL default ' ',
  "AAABMPUQ" varchar(6) NOT NULL default '000000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMFOO" varchar(4) NOT NULL default '0000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMPQG" varchar(4) NOT NULL default ' ',
  "AAABMPQH" varchar(6) NOT NULL default '000000',
  "AAABMPQI" varchar(4) NOT NULL default '0000',
  "AAABMPQJ" varchar(8) NOT NULL default ' ',
  "AAABMPQK" varchar(4) NOT NULL default '0000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMRCJ" varchar(8) NOT NULL default '00000000',
  "AAABMPQQ" varchar(4) NOT NULL default '0000',
  "AAABMPQR" varchar(4) NOT NULL default ' ',
  "AAABMPQS" varchar(1) NOT NULL default ' ',
  "AAABMPQT" varchar(3) NOT NULL default ' ',
  "AAABMPQU" varchar(1) NOT NULL default ' ',
  "AAABMPQV" varchar(10) NOT NULL default ' ',
  "AAABMPQW" varchar(1) NOT NULL default ' ',
  "AAABMPQX" varchar(6) NOT NULL default '000000',
  "AAABMPQY" varchar(1) NOT NULL default ' ',
  "AAABMPQZ" varchar(8) NOT NULL default '00000000',
  "AAABMPRA" varchar(1) NOT NULL default ' ',
  "AAABMPRB" varchar(1) NOT NULL default ' ',
  "AAABMPRC" varchar(4) NOT NULL default '0000',
  "AAABMPRD" varchar(8) NOT NULL default ' ',
  "AAABMPRE" varchar(6) NOT NULL default '000000',
  "AAABMPRF" varchar(4) NOT NULL default ' ',
  "AAABMPRG" varchar(2) NOT NULL default ' ',
  "AAABMPRH" varchar(2) NOT NULL default ' ',
  "AAABMPRI" varchar(4) NOT NULL default ' ',
  "AAABMPRJ" varchar(4) NOT NULL default ' ',
  "AAABMPRK" varchar(13) NOT NULL default '0000000000000',
  "AAABMPRL" varchar(10) NOT NULL default ' ',
  "AAABMPRM" varchar(10) NOT NULL default ' ',
  "AAABMPRN" varchar(10) NOT NULL default ' ',
  "AAABMPRO" varchar(10) NOT NULL default ' ',
  "AAABMPRP" varchar(10) NOT NULL default ' ',
  "AAABMPRQ" varchar(10) NOT NULL default ' ',
  "AAABMPRR" varchar(10) NOT NULL default ' ',
  "AAABMPRS" varchar(10) NOT NULL default ' ',
  "AAABMPRT" varchar(10) NOT NULL default ' ',
  "AAABMPRU" varchar(10) NOT NULL default ' ',
  "AAABMPRV" varchar(10) NOT NULL default ' ',
  "AAABMPRW" varchar(8) NOT NULL default '00000000',
  "AAABMPRX" decimal(16,2) NOT NULL default '0.00',
  "AAABMPRY" varchar(5) NOT NULL default ' ',
  "AAABMPRZ" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMPSB" varchar(18) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABMPSD" varchar(4) NOT NULL default ' ',
  "AAABMPSE" varchar(1) NOT NULL default ' ',
  "AAABMPSF" varchar(1) NOT NULL default ' ',
  "AAABMPSG" varchar(1) NOT NULL default ' ',
  "AAABMPSH" varchar(1) NOT NULL default ' ',
  "AAABMPSI" varchar(4) NOT NULL default ' ',
  "AAABMPSJ" varchar(2) NOT NULL default ' ',
  "AAABMPSK" varchar(8) NOT NULL default '00000000',
  "AAABMPSL" varchar(8) NOT NULL default '00000000',
  "AAABMPSM" varchar(8) NOT NULL default '00000000',
  "AAABMPSN" varchar(8) NOT NULL default '00000000',
  "AAABMPSO" varchar(4) NOT NULL default ' ',
  "AAABMPSP" varchar(2) NOT NULL default ' ',
  "AAABMPSQ" varchar(2) NOT NULL default ' ',
  "AAABMPSR" varchar(4) NOT NULL default ' ',
  "AAABMPSS" varchar(4) NOT NULL default ' ',
  "AAABMPST" varchar(13) NOT NULL default '0000000000000',
  "AAABMPSU" decimal(16,2) NOT NULL default '0.00',
  "AAABMPSV" varchar(5) NOT NULL default ' ',
  "AAABMPSW" decimal(9,5) NOT NULL default '0.00000',
  "AAABMPSX" varchar(4) NOT NULL default ' ',
  "AAABMPSY" decimal(7,3) NOT NULL default '0.000',
  "AAABMPSZ" decimal(7,3) NOT NULL default '0.000',
  "AAABMPTA" decimal(7,3) NOT NULL default '0.000',
  "AAABMPTB" decimal(7,3) NOT NULL default '0.000',
  "AAABMPTC" decimal(16,2) NOT NULL default '0.00',
  "AAABMPTD" varchar(5) NOT NULL default ' ',
  "AAABMPTE" varchar(6) NOT NULL default ' ',
  "AAABMPTF" varchar(10) NOT NULL default ' ',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAABMPTH" varchar(1) NOT NULL default '0',
  "AAABMPTI" varchar(6) NOT NULL default '000000',
  "AAABMPTJ" int(11) NOT NULL default '0',
  "AAABMPTK" varchar(18) NOT NULL default ' ',
  "AAABMPTL" varchar(18) NOT NULL default ' ',
  "AAABMPTM" decimal(15,5) NOT NULL default '0.00000',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAABMPTO" decimal(15,5) NOT NULL default '0.00000',
  "AAABMPTP" varchar(3) NOT NULL default ' ',
  "AAABMPTQ" decimal(16,2) NOT NULL default '0.00',
  "AAABMPTR" varchar(5) NOT NULL default ' ',
  "AAABMPTS" varchar(10) NOT NULL default ' ',
  "AAABMPTT" varchar(10) NOT NULL default ' ',
  "AAABMPTU" varchar(12) NOT NULL default ' ',
  "AAABMPTV" varchar(10) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABMPTX" decimal(13,2) NOT NULL default '0.00',
  "AAABMPTY" decimal(13,2) NOT NULL default '0.00',
  "AAABMPTZ" varchar(5) NOT NULL default ' ',
  "AAABMPUA" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMPUC" varchar(50) NOT NULL default ' ',
  "AAABMPUD" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPUE" varchar(6) NOT NULL default ' ',
  "AAABMPUF" varchar(6) NOT NULL default ' ',
  "AAABMPUG" varchar(8) NOT NULL default '00000000',
  "AAABMPUH" varchar(3) NOT NULL default ' ',
  "AAABMPUI" varchar(5) NOT NULL default ' ',
  "AAABMPUJ" decimal(15,5) NOT NULL default '0.00000',
  "AAABMPUK" decimal(16,2) NOT NULL default '0.00',
  "AAABMPUL" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMPUP","AAABMPUQ","AAABMPQE","AAABMRHL","AAABMFOO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIRBE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABMPUP" varchar(10) NOT NULL default ' ',
  "AAABISXL" varchar(9) NOT NULL default '000000000',
  "AAABIRBM" varchar(10) NOT NULL default '0000000000',
  "AAABISXO" varchar(10) NOT NULL default '0000000000',
  "AAABIRBO" varchar(10) NOT NULL default '0000000000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMGUN" varchar(8) NOT NULL default '00000000',
  "AAABMGUO" varchar(8) NOT NULL default '00000000',
  "AAABISNL" varchar(4) NOT NULL default ' ',
  "AAABMGFC" varchar(8) NOT NULL default '00000000',
  "AAABMGFD" varchar(8) NOT NULL default '00000000',
  "AAABMGFE" varchar(8) NOT NULL default '00000000',
  "AAABISNP" varchar(8) NOT NULL default '00000000',
  "AAABISNQ" varchar(8) NOT NULL default '00000000',
  "AAABISNR" varchar(8) NOT NULL default '00000000',
  "AAABISNS" varchar(8) NOT NULL default '00000000',
  "AAABMPPQ" varchar(12) NOT NULL default ' ',
  "AAABMPPR" varchar(8) NOT NULL default '00000000',
  "AAABMPPS" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMPPC","AAABMPPD","AAABMPPE","AAABMPUP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIRCK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABISVK" varchar(8) NOT NULL default '00000000',
  "AAABISXL" varchar(9) NOT NULL default '000000000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMGTN" varchar(4) NOT NULL default ' ',
  "AAABMIMR" varchar(4) NOT NULL default '0000',
  "AAABMGTP" varchar(4) NOT NULL default ' ',
  "AAABMGTQ" varchar(6) NOT NULL default '000000',
  "AAABMGTR" varchar(4) NOT NULL default '0000',
  "AAABMGTS" varchar(9) NOT NULL default '000000000',
  "AAABMPPQ" varchar(12) NOT NULL default ' ',
  "AAABMPPR" varchar(8) NOT NULL default '00000000',
  "AAABMPPS" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMPPC","AAABMPPD","AAABMPPE","AAABMPQE"),
  KEY "AAABIRDK" ("AAABMPPC","AAABMPPD"),
  KEY "AAABIRDN" ("AAABMPPC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIRDP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABISXL" varchar(9) NOT NULL default '000000000',
  "AAABISXO" varchar(10) NOT NULL default '0000000000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMGUN" varchar(8) NOT NULL default '00000000',
  "AAABMGUO" varchar(8) NOT NULL default '00000000',
  "AAABISNL" varchar(4) NOT NULL default ' ',
  "AAABMGFC" varchar(8) NOT NULL default '00000000',
  "AAABMGFD" varchar(8) NOT NULL default '00000000',
  "AAABMGFE" varchar(8) NOT NULL default '00000000',
  "AAABISNP" varchar(8) NOT NULL default '00000000',
  "AAABISNQ" varchar(8) NOT NULL default '00000000',
  "AAABISNR" varchar(8) NOT NULL default '00000000',
  "AAABISNS" varchar(8) NOT NULL default '00000000',
  "AAABMPPQ" varchar(12) NOT NULL default ' ',
  "AAABMPPR" varchar(8) NOT NULL default '00000000',
  "AAABMPPS" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMPPC","AAABMPPD","AAABMPPE"),
  KEY "AAABIRER" ("AAABISXO"),
  KEY "AAABIRET" ("AAABMPPC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIREV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABMGUY" varchar(4) NOT NULL default '0000',
  "AAABISXL" varchar(9) NOT NULL default '000000000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMGUN" varchar(8) NOT NULL default '00000000',
  "AAABMGUO" varchar(8) NOT NULL default '00000000',
  "AAABMPUN" varchar(1) NOT NULL default ' ',
  "AAABMPPQ" varchar(12) NOT NULL default ' ',
  "AAABMPPR" varchar(8) NOT NULL default '00000000',
  "AAABMPPS" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMPPC","AAABMPPD","AAABMPPE","AAABMGUY"),
  KEY "AAABIRFS" ("AAABMPPC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIRFU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABISXL" varchar(9) NOT NULL default '000000000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABISXO" varchar(10) NOT NULL default '0000000000',
  "AAABMGUN" varchar(8) NOT NULL default '00000000',
  "AAABMGUO" varchar(8) NOT NULL default '00000000',
  "AAABISXR" varchar(4) NOT NULL default ' ',
  "AAABISXS" varchar(8) NOT NULL default '00000000',
  "AAABISXT" varchar(8) NOT NULL default '00000000',
  "AAABISXU" varchar(4) NOT NULL default '0000',
  "AAABMPPQ" varchar(12) NOT NULL default ' ',
  "AAABMPPR" varchar(8) NOT NULL default '00000000',
  "AAABMPPS" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMPPC","AAABMPPD","AAABMPPE"),
  KEY "AAABIRGS" ("AAABMPPC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIRGU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUP" varchar(10) NOT NULL default ' ',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMRDD" varchar(1) NOT NULL default ' ',
  "AAABMPPH" varchar(8) NOT NULL default '00000000',
  "AAABMPPI" varchar(8) NOT NULL default '00000000',
  "AAABMPPJ" varchar(4) NOT NULL default ' ',
  "AAABMPPK" varchar(4) NOT NULL default ' ',
  "AAABMPPL" varchar(2) NOT NULL default ' ',
  "AAABMPPM" varchar(2) NOT NULL default ' ',
  "AAABMPPN" varchar(4) NOT NULL default ' ',
  "AAABMPPO" varchar(8) NOT NULL default ' ',
  "AAABMPPP" varchar(8) NOT NULL default '00000000',
  "AAABMPPQ" varchar(12) NOT NULL default ' ',
  "AAABMPPR" varchar(8) NOT NULL default '00000000',
  "AAABMPPS" varchar(6) NOT NULL default '000000',
  "AAABMPUM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPUP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIRHU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUP" varchar(10) NOT NULL default ' ',
  "AAABMPUQ" varchar(6) NOT NULL default '000000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPQG" varchar(4) NOT NULL default ' ',
  "AAABMPQH" varchar(6) NOT NULL default '000000',
  "AAABMPQI" varchar(4) NOT NULL default '0000',
  "AAABMPQJ" varchar(8) NOT NULL default ' ',
  "AAABMPQK" varchar(4) NOT NULL default '0000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMRCJ" varchar(8) NOT NULL default '00000000',
  "AAABMPQQ" varchar(4) NOT NULL default '0000',
  "AAABMPQR" varchar(4) NOT NULL default ' ',
  "AAABMPQS" varchar(1) NOT NULL default ' ',
  "AAABMPQT" varchar(3) NOT NULL default ' ',
  "AAABMPQU" varchar(1) NOT NULL default ' ',
  "AAABMPQV" varchar(10) NOT NULL default ' ',
  "AAABMPQW" varchar(1) NOT NULL default ' ',
  "AAABMPQX" varchar(6) NOT NULL default '000000',
  "AAABMPQY" varchar(1) NOT NULL default ' ',
  "AAABMPQZ" varchar(8) NOT NULL default '00000000',
  "AAABMPRA" varchar(1) NOT NULL default ' ',
  "AAABMPRB" varchar(1) NOT NULL default ' ',
  "AAABMPRC" varchar(4) NOT NULL default '0000',
  "AAABMPRD" varchar(8) NOT NULL default ' ',
  "AAABMPRE" varchar(6) NOT NULL default '000000',
  "AAABMPRF" varchar(4) NOT NULL default ' ',
  "AAABMPRG" varchar(2) NOT NULL default ' ',
  "AAABMPRH" varchar(2) NOT NULL default ' ',
  "AAABMPRI" varchar(4) NOT NULL default ' ',
  "AAABMPRJ" varchar(4) NOT NULL default ' ',
  "AAABMPRK" varchar(13) NOT NULL default '0000000000000',
  "AAABMPRL" varchar(10) NOT NULL default ' ',
  "AAABMPRM" varchar(10) NOT NULL default ' ',
  "AAABMPRN" varchar(10) NOT NULL default ' ',
  "AAABMPRO" varchar(10) NOT NULL default ' ',
  "AAABMPRP" varchar(10) NOT NULL default ' ',
  "AAABMPRQ" varchar(10) NOT NULL default ' ',
  "AAABMPRR" varchar(10) NOT NULL default ' ',
  "AAABMPRS" varchar(10) NOT NULL default ' ',
  "AAABMPRT" varchar(10) NOT NULL default ' ',
  "AAABMPRU" varchar(10) NOT NULL default ' ',
  "AAABMPRV" varchar(10) NOT NULL default ' ',
  "AAABMPRW" varchar(8) NOT NULL default '00000000',
  "AAABMPRX" decimal(16,2) NOT NULL default '0.00',
  "AAABMPRY" varchar(5) NOT NULL default ' ',
  "AAABMPRZ" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMPSB" varchar(18) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABMPSD" varchar(4) NOT NULL default ' ',
  "AAABMPSE" varchar(1) NOT NULL default ' ',
  "AAABMPSF" varchar(1) NOT NULL default ' ',
  "AAABMPSG" varchar(1) NOT NULL default ' ',
  "AAABMPSH" varchar(1) NOT NULL default ' ',
  "AAABMPSI" varchar(4) NOT NULL default ' ',
  "AAABMPSJ" varchar(2) NOT NULL default ' ',
  "AAABMPSK" varchar(8) NOT NULL default '00000000',
  "AAABMPSL" varchar(8) NOT NULL default '00000000',
  "AAABMPSM" varchar(8) NOT NULL default '00000000',
  "AAABMPSN" varchar(8) NOT NULL default '00000000',
  "AAABMPSO" varchar(4) NOT NULL default ' ',
  "AAABMPSP" varchar(2) NOT NULL default ' ',
  "AAABMPSQ" varchar(2) NOT NULL default ' ',
  "AAABMPSR" varchar(4) NOT NULL default ' ',
  "AAABMPSS" varchar(4) NOT NULL default ' ',
  "AAABMPST" varchar(13) NOT NULL default '0000000000000',
  "AAABMPSU" decimal(16,2) NOT NULL default '0.00',
  "AAABMPSV" varchar(5) NOT NULL default ' ',
  "AAABMPSW" decimal(9,5) NOT NULL default '0.00000',
  "AAABMPSX" varchar(4) NOT NULL default ' ',
  "AAABMPSY" decimal(7,3) NOT NULL default '0.000',
  "AAABMPSZ" decimal(7,3) NOT NULL default '0.000',
  "AAABMPTA" decimal(7,3) NOT NULL default '0.000',
  "AAABMPTB" decimal(7,3) NOT NULL default '0.000',
  "AAABMPTC" decimal(16,2) NOT NULL default '0.00',
  "AAABMPTD" varchar(5) NOT NULL default ' ',
  "AAABMPTE" varchar(6) NOT NULL default ' ',
  "AAABMPTF" varchar(10) NOT NULL default ' ',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAABMPTH" varchar(1) NOT NULL default '0',
  "AAABMPTI" varchar(6) NOT NULL default '000000',
  "AAABMPTJ" int(11) NOT NULL default '0',
  "AAABMPTK" varchar(18) NOT NULL default ' ',
  "AAABMPTL" varchar(18) NOT NULL default ' ',
  "AAABMPTM" decimal(15,5) NOT NULL default '0.00000',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAABMPTO" decimal(15,5) NOT NULL default '0.00000',
  "AAABMPTP" varchar(3) NOT NULL default ' ',
  "AAABMPTQ" decimal(16,2) NOT NULL default '0.00',
  "AAABMPTR" varchar(5) NOT NULL default ' ',
  "AAABMPTS" varchar(10) NOT NULL default ' ',
  "AAABMPTT" varchar(10) NOT NULL default ' ',
  "AAABMPTU" varchar(12) NOT NULL default ' ',
  "AAABMPTV" varchar(10) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABMPTX" decimal(13,2) NOT NULL default '0.00',
  "AAABMPTY" decimal(13,2) NOT NULL default '0.00',
  "AAABMPTZ" varchar(5) NOT NULL default ' ',
  "AAABMPUA" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMPUC" varchar(50) NOT NULL default ' ',
  "AAABMPUD" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPUE" varchar(6) NOT NULL default ' ',
  "AAABMPUF" varchar(6) NOT NULL default ' ',
  "AAABMPUG" varchar(8) NOT NULL default '00000000',
  "AAABMPUH" varchar(3) NOT NULL default ' ',
  "AAABMPUI" varchar(5) NOT NULL default ' ',
  "AAABMPUJ" decimal(15,5) NOT NULL default '0.00000',
  "AAABMPUK" decimal(16,2) NOT NULL default '0.00',
  "AAABMPUL" varchar(4) NOT NULL default '0000',
  "AAABMPUM" varchar(1) NOT NULL default ' ',
  "AAABMPUN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPUP","AAABMPUQ"),
  KEY "AAABIRMM" ("AAABMPQR","AAABMPRI","AAABMPRK","AAABMPRL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIRMR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUP" varchar(10) NOT NULL default ' ',
  "AAABMPUQ" varchar(6) NOT NULL default '000000',
  "AAABMPEW" varchar(6) NOT NULL default '000000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPQG" varchar(4) NOT NULL default ' ',
  "AAABMPQH" varchar(6) NOT NULL default '000000',
  "AAABMPQI" varchar(4) NOT NULL default '0000',
  "AAABMPQJ" varchar(8) NOT NULL default ' ',
  "AAABMPQK" varchar(4) NOT NULL default '0000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMRCJ" varchar(8) NOT NULL default '00000000',
  "AAABMPQQ" varchar(4) NOT NULL default '0000',
  "AAABMPQR" varchar(4) NOT NULL default ' ',
  "AAABMPQS" varchar(1) NOT NULL default ' ',
  "AAABMPQT" varchar(3) NOT NULL default ' ',
  "AAABMPQU" varchar(1) NOT NULL default ' ',
  "AAABMPQV" varchar(10) NOT NULL default ' ',
  "AAABMPQW" varchar(1) NOT NULL default ' ',
  "AAABMPQX" varchar(6) NOT NULL default '000000',
  "AAABMPQY" varchar(1) NOT NULL default ' ',
  "AAABMPQZ" varchar(8) NOT NULL default '00000000',
  "AAABMPRA" varchar(1) NOT NULL default ' ',
  "AAABMPRB" varchar(1) NOT NULL default ' ',
  "AAABMPRC" varchar(4) NOT NULL default '0000',
  "AAABMPRD" varchar(8) NOT NULL default ' ',
  "AAABMPRE" varchar(6) NOT NULL default '000000',
  "AAABMPRF" varchar(4) NOT NULL default ' ',
  "AAABMPRG" varchar(2) NOT NULL default ' ',
  "AAABMPRH" varchar(2) NOT NULL default ' ',
  "AAABMPRI" varchar(4) NOT NULL default ' ',
  "AAABMPRJ" varchar(4) NOT NULL default ' ',
  "AAABMPRK" varchar(13) NOT NULL default '0000000000000',
  "AAABMPRL" varchar(10) NOT NULL default ' ',
  "AAABMPRM" varchar(10) NOT NULL default ' ',
  "AAABMPRN" varchar(10) NOT NULL default ' ',
  "AAABMPRO" varchar(10) NOT NULL default ' ',
  "AAABMPRP" varchar(10) NOT NULL default ' ',
  "AAABMPRQ" varchar(10) NOT NULL default ' ',
  "AAABMPRR" varchar(10) NOT NULL default ' ',
  "AAABMPRS" varchar(10) NOT NULL default ' ',
  "AAABMPRT" varchar(10) NOT NULL default ' ',
  "AAABMPRU" varchar(10) NOT NULL default ' ',
  "AAABMPRV" varchar(10) NOT NULL default ' ',
  "AAABMPRW" varchar(8) NOT NULL default '00000000',
  "AAABMPRX" decimal(16,2) NOT NULL default '0.00',
  "AAABMPRY" varchar(5) NOT NULL default ' ',
  "AAABMPRZ" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMPSB" varchar(18) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABMPSD" varchar(4) NOT NULL default ' ',
  "AAABMPSE" varchar(1) NOT NULL default ' ',
  "AAABMPSF" varchar(1) NOT NULL default ' ',
  "AAABMPSG" varchar(1) NOT NULL default ' ',
  "AAABMPSH" varchar(1) NOT NULL default ' ',
  "AAABMPSI" varchar(4) NOT NULL default ' ',
  "AAABMPSJ" varchar(2) NOT NULL default ' ',
  "AAABMPSK" varchar(8) NOT NULL default '00000000',
  "AAABMPSL" varchar(8) NOT NULL default '00000000',
  "AAABMPSM" varchar(8) NOT NULL default '00000000',
  "AAABMPSN" varchar(8) NOT NULL default '00000000',
  "AAABMPSO" varchar(4) NOT NULL default ' ',
  "AAABMPSP" varchar(2) NOT NULL default ' ',
  "AAABMPSQ" varchar(2) NOT NULL default ' ',
  "AAABMPSR" varchar(4) NOT NULL default ' ',
  "AAABMPSS" varchar(4) NOT NULL default ' ',
  "AAABMPST" varchar(13) NOT NULL default '0000000000000',
  "AAABMPSU" decimal(16,2) NOT NULL default '0.00',
  "AAABMPSV" varchar(5) NOT NULL default ' ',
  "AAABMPSW" decimal(9,5) NOT NULL default '0.00000',
  "AAABMPSX" varchar(4) NOT NULL default ' ',
  "AAABMPSY" decimal(7,3) NOT NULL default '0.000',
  "AAABMPSZ" decimal(7,3) NOT NULL default '0.000',
  "AAABMPTA" decimal(7,3) NOT NULL default '0.000',
  "AAABMPTB" decimal(7,3) NOT NULL default '0.000',
  "AAABMPTC" decimal(16,2) NOT NULL default '0.00',
  "AAABMPTD" varchar(5) NOT NULL default ' ',
  "AAABMPTE" varchar(6) NOT NULL default ' ',
  "AAABMPTF" varchar(10) NOT NULL default ' ',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAABMPTH" varchar(1) NOT NULL default '0',
  "AAABMPTI" varchar(6) NOT NULL default '000000',
  "AAABMPTJ" int(11) NOT NULL default '0',
  "AAABMPTK" varchar(18) NOT NULL default ' ',
  "AAABMPTL" varchar(18) NOT NULL default ' ',
  "AAABMPTM" decimal(15,5) NOT NULL default '0.00000',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAABMPTO" decimal(15,5) NOT NULL default '0.00000',
  "AAABMPTP" varchar(3) NOT NULL default ' ',
  "AAABMPTQ" decimal(16,2) NOT NULL default '0.00',
  "AAABMPTR" varchar(5) NOT NULL default ' ',
  "AAABMPTS" varchar(10) NOT NULL default ' ',
  "AAABMPTT" varchar(10) NOT NULL default ' ',
  "AAABMPTU" varchar(12) NOT NULL default ' ',
  "AAABMPTV" varchar(10) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABMPTX" decimal(13,2) NOT NULL default '0.00',
  "AAABMPTY" decimal(13,2) NOT NULL default '0.00',
  "AAABMPTZ" varchar(5) NOT NULL default ' ',
  "AAABMPUA" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMPUC" varchar(50) NOT NULL default ' ',
  "AAABMPUD" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPUE" varchar(6) NOT NULL default ' ',
  "AAABMPUF" varchar(6) NOT NULL default ' ',
  "AAABMPUG" varchar(8) NOT NULL default '00000000',
  "AAABMPUH" varchar(3) NOT NULL default ' ',
  "AAABMPUI" varchar(5) NOT NULL default ' ',
  "AAABMPUJ" decimal(15,5) NOT NULL default '0.00000',
  "AAABMPUK" decimal(16,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMPUP","AAABMPUQ","AAABMPEW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIRRI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPTL" varchar(18) NOT NULL default ' ',
  "AAABMQWS" varchar(4) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMAWF" varchar(8) NOT NULL default '00000000',
  "AAABIRSN" varchar(12) NOT NULL default ' ',
  "AAABIRSO" varchar(8) NOT NULL default '00000000',
  "AAABIRRQ" varchar(12) NOT NULL default ' ',
  "AAABIRRR" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMPTL"),
  KEY "AAABIRRV" ("AAABMQWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIRRX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMPTL" varchar(18) NOT NULL default ' ',
  "AAABMOHD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMPTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIRSF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPTL" varchar(18) NOT NULL default ' ',
  "AAABMLTD" varchar(4) NOT NULL default '0000',
  "AAABIRSJ" varchar(18) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMAWF" varchar(8) NOT NULL default '00000000',
  "AAABIRSN" varchar(12) NOT NULL default ' ',
  "AAABIRSO" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMPTL","AAABMLTD"),
  KEY "AAABIRSS" ("AAABMPTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIRSU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUP" varchar(10) NOT NULL default ' ',
  "AAABIRTE" varchar(10) NOT NULL default '0000000000',
  "AAABMPPH" varchar(8) NOT NULL default '00000000',
  "AAABMPPI" varchar(8) NOT NULL default '00000000',
  "AAABIRTA" varchar(1) NOT NULL default ' ',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABMPUP","AAABIRTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIRTF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIP" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIRTK" (
  "AAABIRUG" varchar(30) NOT NULL default ' ',
  "AAABIRUD" varchar(30) NOT NULL default ' ',
  "AAABIRUE" varchar(30) NOT NULL default ' ',
  "AAABIRTO" varchar(1) NOT NULL default ' ',
  "AAABIRUH" varchar(4) NOT NULL default '0000',
  "AAABIRTQ" varchar(30) NOT NULL default ' ',
  "AAABIRTR" varchar(30) NOT NULL default ' ',
  "AAABIRTS" varchar(30) NOT NULL default ' ',
  "AAABIRTT" varchar(8) NOT NULL default ' ',
  "AAABIRTU" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABIRUV" varchar(72) NOT NULL default ' ',
  "AAABIRTX" varchar(1) NOT NULL default ' ',
  "AAABIRTY" varchar(1) NOT NULL default ' ',
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABIRUB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABIRUG","AAABIRUD","AAABIRUE"),
  UNIQUE KEY "AAABIRUF" ("AAABIRUG","AAABIRUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIRUI" (
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAABIRUM" varchar(40) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABIRUO" varchar(30) NOT NULL default ' ',
  "AAABLNCO" varchar(40) NOT NULL default ' ',
  "AAABIRUQ" varchar(1) NOT NULL default ' ',
  "AAABIRUR" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABIRUV" varchar(72) NOT NULL default ' ',
  "AAABIRUW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLRST","AAABMKFH","AAABLFYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIRVA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMICS" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMICS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIRVF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRIO" varchar(14) NOT NULL default ' ',
  "AAABIRVJ" varchar(32) NOT NULL default ' ',
  "AAABIRVK" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIRVO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMOGK","AAABMNJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIRVV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIRWA" varchar(30) NOT NULL default ' ',
  "AAABIRWB" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOGK","AAABMNJK","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIRWG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMHDH" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMHDH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIRWL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABIRWS" varchar(60) NOT NULL default ' ',
  "AAABIRWT" varchar(60) NOT NULL default ' ',
  "AAABIRWU" varchar(60) NOT NULL default ' ',
  "AAABIRWV" varchar(60) NOT NULL default ' ',
  "AAABIRWW" varchar(60) NOT NULL default ' ',
  "AAABIRWX" varchar(60) NOT NULL default ' ',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIRXH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABIRXK" varchar(4) NOT NULL default ' ',
  "AAABIRXL" varchar(2) NOT NULL default ' ',
  "AAABIRXM" varchar(2) NOT NULL default ' ',
  "AAABIRXN" varchar(4) NOT NULL default ' ',
  "AAABIRXO" varchar(4) NOT NULL default ' ',
  "AAABIRXP" varchar(1) NOT NULL default ' ',
  "AAABIRXQ" varchar(1) NOT NULL default ' ',
  "AAABIRXR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIRXU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQPY" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABMQPY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIRXZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOUF" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMOUF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIRYE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQKL" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMQKL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIRYJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQKT" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMQKT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIRYO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQLR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMQLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIRYT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQLZ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMQLZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIRYY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRGB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIRZD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABMGSM" varchar(8) NOT NULL default '00000000',
  "AAABMPUA" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMPPC","AAABMPPD","AAABMPPE","AAABMGSM","AAABMPUA","AAABMPWO"),
  KEY "AAABIRZU" ("AAABMPUA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIRZW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMPPC","AAABMPPD","AAABMPPE","AAABMPQE"),
  KEY "AAABISAJ" ("AAABMPQE"),
  KEY "AAABISAL" ("AAABMPPC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABISAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABMGSM" varchar(8) NOT NULL default '00000000',
  "AAABMPUP" varchar(10) NOT NULL default ' ',
  "AAABISAV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMPPC","AAABMPPD","AAABMPPE","AAABMGSM","AAABMPUP"),
  KEY "AAABISBD" ("AAABMPUP"),
  KEY "AAABISBF" ("AAABMPPC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABISBH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABMGSM" varchar(8) NOT NULL default '00000000',
  "AAABMPUD" varchar(16) NOT NULL default '0000000000000000',
  "AAABMGRF" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMPPC","AAABMPPD","AAABMPPE","AAABMGSM","AAABMPUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABISBX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABMGSM" varchar(8) NOT NULL default '00000000',
  "AAABMPUP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMPPC","AAABMPPD","AAABMPPE","AAABMGSM","AAABMPUP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABISCM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUP" varchar(10) NOT NULL default ' ',
  "AAABMPUQ" varchar(6) NOT NULL default '000000',
  "AAABISCQ" varchar(10) NOT NULL default ' ',
  "AAABISCR" varchar(6) NOT NULL default '000000',
  "AAABISCS" varchar(10) NOT NULL default ' ',
  "AAABISCT" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMPUP","AAABMPUQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABISCX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABMGSM" varchar(8) NOT NULL default '00000000',
  "AAABMGOZ" varchar(6) NOT NULL default '000000',
  "AAABMGPA" varchar(6) NOT NULL default '000000',
  "AAABMPUN" varchar(1) NOT NULL default ' ',
  "AAABMGEU" varchar(1) NOT NULL default ' ',
  "AAABMGEV" varchar(8) NOT NULL default '00000000',
  "AAABISXL" varchar(9) NOT NULL default '000000000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMPUM" varchar(1) NOT NULL default ' ',
  "AAABMGUN" varchar(8) NOT NULL default '00000000',
  "AAABMGUO" varchar(8) NOT NULL default '00000000',
  "AAABISNL" varchar(4) NOT NULL default ' ',
  "AAABMGFC" varchar(8) NOT NULL default '00000000',
  "AAABMGFD" varchar(8) NOT NULL default '00000000',
  "AAABMGFE" varchar(8) NOT NULL default '00000000',
  "AAABISNP" varchar(8) NOT NULL default '00000000',
  "AAABISNQ" varchar(8) NOT NULL default '00000000',
  "AAABISNR" varchar(8) NOT NULL default '00000000',
  "AAABISNS" varchar(8) NOT NULL default '00000000',
  "AAABMPPQ" varchar(12) NOT NULL default ' ',
  "AAABMPPR" varchar(8) NOT NULL default '00000000',
  "AAABMPPS" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMPPC","AAABMPPD","AAABMPPE","AAABMGSM"),
  KEY "AAABISEF" ("AAABMPPC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABISEH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABISRB" varchar(13) NOT NULL default '0000000000000',
  "AAABISRC" varchar(4) NOT NULL default ' ',
  "AAABISRD" varchar(6) NOT NULL default '000000',
  "AAABISRE" varchar(4) NOT NULL default '0000',
  "AAABISQS" varchar(8) NOT NULL default '00000000',
  "AAABMPMZ" varchar(4) NOT NULL default '0000',
  "AAABMPUA" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMPLP" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABISFK" varchar(10) NOT NULL default '0000000000',
  "AAABISFL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABISRB","AAABISRC","AAABISRD","AAABISRE","AAABISQS","AAABMPMZ","AAABMPUA","AAABMPWO","AAABMPLP","AAABMRGQ","AAABMRGR","AAABMRGS","AAABISFK","AAABISFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABISFM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABISRB" varchar(13) NOT NULL default '0000000000000',
  "AAABISRC" varchar(4) NOT NULL default ' ',
  "AAABISRD" varchar(6) NOT NULL default '000000',
  "AAABISRE" varchar(4) NOT NULL default '0000',
  "AAABISQS" varchar(8) NOT NULL default '00000000',
  "AAABMQIZ" varchar(4) NOT NULL default '0000',
  "AAABMQJA" varchar(4) NOT NULL default ' ',
  "AAABMQJB" varchar(4) NOT NULL default '0000',
  "AAABMQJC" varchar(13) NOT NULL default '0000000000000',
  "AAABMQJD" varchar(4) NOT NULL default '0000',
  "AAABMRFP" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  "AAABISQM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABISRB","AAABISRC","AAABISRD","AAABISRE","AAABISQS","AAABMQIZ","AAABMQJA","AAABMQJB","AAABMQJC","AAABMQJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABISGN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUP" varchar(10) NOT NULL default ' ',
  "AAABMPUQ" varchar(6) NOT NULL default '000000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMFOO" varchar(4) NOT NULL default '0000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMPQG" varchar(4) NOT NULL default ' ',
  "AAABMPQH" varchar(6) NOT NULL default '000000',
  "AAABMPQI" varchar(4) NOT NULL default '0000',
  "AAABMPQJ" varchar(8) NOT NULL default ' ',
  "AAABMPQK" varchar(4) NOT NULL default '0000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMRCJ" varchar(8) NOT NULL default '00000000',
  "AAABMPQQ" varchar(4) NOT NULL default '0000',
  "AAABMPQR" varchar(4) NOT NULL default ' ',
  "AAABMPQS" varchar(1) NOT NULL default ' ',
  "AAABMPQT" varchar(3) NOT NULL default ' ',
  "AAABMPQU" varchar(1) NOT NULL default ' ',
  "AAABMPQV" varchar(10) NOT NULL default ' ',
  "AAABMPQW" varchar(1) NOT NULL default ' ',
  "AAABMPQX" varchar(6) NOT NULL default '000000',
  "AAABMPQY" varchar(1) NOT NULL default ' ',
  "AAABMPQZ" varchar(8) NOT NULL default '00000000',
  "AAABMPRA" varchar(1) NOT NULL default ' ',
  "AAABMPRB" varchar(1) NOT NULL default ' ',
  "AAABMPRC" varchar(4) NOT NULL default '0000',
  "AAABMPRD" varchar(8) NOT NULL default ' ',
  "AAABMPRE" varchar(6) NOT NULL default '000000',
  "AAABMPRF" varchar(4) NOT NULL default ' ',
  "AAABMPRG" varchar(2) NOT NULL default ' ',
  "AAABMPRH" varchar(2) NOT NULL default ' ',
  "AAABMPRI" varchar(4) NOT NULL default ' ',
  "AAABMPRJ" varchar(4) NOT NULL default ' ',
  "AAABMPRK" varchar(13) NOT NULL default '0000000000000',
  "AAABMPRL" varchar(10) NOT NULL default ' ',
  "AAABMPRM" varchar(10) NOT NULL default ' ',
  "AAABMPRN" varchar(10) NOT NULL default ' ',
  "AAABMPRO" varchar(10) NOT NULL default ' ',
  "AAABMPRP" varchar(10) NOT NULL default ' ',
  "AAABMPRQ" varchar(10) NOT NULL default ' ',
  "AAABMPRR" varchar(10) NOT NULL default ' ',
  "AAABMPRS" varchar(10) NOT NULL default ' ',
  "AAABMPRT" varchar(10) NOT NULL default ' ',
  "AAABMPRU" varchar(10) NOT NULL default ' ',
  "AAABMPRV" varchar(10) NOT NULL default ' ',
  "AAABMPRW" varchar(8) NOT NULL default '00000000',
  "AAABMPRX" decimal(16,2) NOT NULL default '0.00',
  "AAABMPRY" varchar(5) NOT NULL default ' ',
  "AAABMPRZ" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMPSB" varchar(18) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABMPSD" varchar(4) NOT NULL default ' ',
  "AAABMPSE" varchar(1) NOT NULL default ' ',
  "AAABMPSF" varchar(1) NOT NULL default ' ',
  "AAABMPSG" varchar(1) NOT NULL default ' ',
  "AAABMPSH" varchar(1) NOT NULL default ' ',
  "AAABMPSI" varchar(4) NOT NULL default ' ',
  "AAABMPSJ" varchar(2) NOT NULL default ' ',
  "AAABMPSK" varchar(8) NOT NULL default '00000000',
  "AAABMPSL" varchar(8) NOT NULL default '00000000',
  "AAABMPSM" varchar(8) NOT NULL default '00000000',
  "AAABMPSN" varchar(8) NOT NULL default '00000000',
  "AAABMPSO" varchar(4) NOT NULL default ' ',
  "AAABMPSP" varchar(2) NOT NULL default ' ',
  "AAABMPSQ" varchar(2) NOT NULL default ' ',
  "AAABMPSR" varchar(4) NOT NULL default ' ',
  "AAABMPSS" varchar(4) NOT NULL default ' ',
  "AAABMPST" varchar(13) NOT NULL default '0000000000000',
  "AAABMPSU" decimal(16,2) NOT NULL default '0.00',
  "AAABMPSV" varchar(5) NOT NULL default ' ',
  "AAABMPSW" decimal(9,5) NOT NULL default '0.00000',
  "AAABMPSX" varchar(4) NOT NULL default ' ',
  "AAABMPSY" decimal(7,3) NOT NULL default '0.000',
  "AAABMPSZ" decimal(7,3) NOT NULL default '0.000',
  "AAABMPTA" decimal(7,3) NOT NULL default '0.000',
  "AAABMPTB" decimal(7,3) NOT NULL default '0.000',
  "AAABMPTC" decimal(16,2) NOT NULL default '0.00',
  "AAABMPTD" varchar(5) NOT NULL default ' ',
  "AAABMPTE" varchar(6) NOT NULL default ' ',
  "AAABMPTF" varchar(10) NOT NULL default ' ',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAABMPTH" varchar(1) NOT NULL default '0',
  "AAABMPTI" varchar(6) NOT NULL default '000000',
  "AAABMPTJ" int(11) NOT NULL default '0',
  "AAABMPTK" varchar(18) NOT NULL default ' ',
  "AAABMPTL" varchar(18) NOT NULL default ' ',
  "AAABMPTM" decimal(15,5) NOT NULL default '0.00000',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAABMPTO" decimal(15,5) NOT NULL default '0.00000',
  "AAABMPTP" varchar(3) NOT NULL default ' ',
  "AAABMPTQ" decimal(16,2) NOT NULL default '0.00',
  "AAABMPTR" varchar(5) NOT NULL default ' ',
  "AAABMPTS" varchar(10) NOT NULL default ' ',
  "AAABMPTT" varchar(10) NOT NULL default ' ',
  "AAABMPTU" varchar(12) NOT NULL default ' ',
  "AAABMPTV" varchar(10) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABMPTX" decimal(13,2) NOT NULL default '0.00',
  "AAABMPTY" decimal(13,2) NOT NULL default '0.00',
  "AAABMPTZ" varchar(5) NOT NULL default ' ',
  "AAABMPUA" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMPUC" varchar(50) NOT NULL default ' ',
  "AAABMPUD" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPUE" varchar(6) NOT NULL default ' ',
  "AAABMPUF" varchar(6) NOT NULL default ' ',
  "AAABMPUG" varchar(8) NOT NULL default '00000000',
  "AAABMPUH" varchar(3) NOT NULL default ' ',
  "AAABMPUI" varchar(5) NOT NULL default ' ',
  "AAABMPUJ" decimal(15,5) NOT NULL default '0.00000',
  "AAABMPUK" decimal(16,2) NOT NULL default '0.00',
  "AAABMPUL" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMPUP","AAABMPUQ","AAABMPQE","AAABMRHL","AAABMFOO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABISLH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUD" varchar(16) NOT NULL default '0000000000000000',
  "AAABMGOG" varchar(10) NOT NULL default ' ',
  "AAABMGOH" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMGRF" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMPUD","AAABMGOG","AAABMGOH","AAABMQII","AAABMJXV","AAABMJXW"),
  KEY "AAABISLX" ("AAABMGRF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABISLZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABISVK" varchar(8) NOT NULL default '00000000',
  "AAABISXL" varchar(9) NOT NULL default '000000000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMGTN" varchar(4) NOT NULL default ' ',
  "AAABMIMR" varchar(4) NOT NULL default '0000',
  "AAABMGTP" varchar(4) NOT NULL default ' ',
  "AAABMGTQ" varchar(6) NOT NULL default '000000',
  "AAABMGTR" varchar(4) NOT NULL default '0000',
  "AAABMGTS" varchar(9) NOT NULL default '000000000',
  "AAABMPPQ" varchar(12) NOT NULL default ' ',
  "AAABMPPR" varchar(8) NOT NULL default '00000000',
  "AAABMPPS" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMPPC","AAABMPPD","AAABMPPE","AAABMPQE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABISMZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABISXL" varchar(9) NOT NULL default '000000000',
  "AAABISXO" varchar(10) NOT NULL default '0000000000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMGUN" varchar(8) NOT NULL default '00000000',
  "AAABMGUO" varchar(8) NOT NULL default '00000000',
  "AAABISNL" varchar(4) NOT NULL default ' ',
  "AAABMGFC" varchar(8) NOT NULL default '00000000',
  "AAABMGFD" varchar(8) NOT NULL default '00000000',
  "AAABMGFE" varchar(8) NOT NULL default '00000000',
  "AAABISNP" varchar(8) NOT NULL default '00000000',
  "AAABISNQ" varchar(8) NOT NULL default '00000000',
  "AAABISNR" varchar(8) NOT NULL default '00000000',
  "AAABISNS" varchar(8) NOT NULL default '00000000',
  "AAABMPPQ" varchar(12) NOT NULL default ' ',
  "AAABMPPR" varchar(8) NOT NULL default '00000000',
  "AAABMPPS" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMPPC","AAABMPPD","AAABMPPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABISOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABMGUY" varchar(4) NOT NULL default '0000',
  "AAABISXL" varchar(9) NOT NULL default '000000000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMGUN" varchar(8) NOT NULL default '00000000',
  "AAABMGUO" varchar(8) NOT NULL default '00000000',
  "AAABMPUN" varchar(1) NOT NULL default ' ',
  "AAABMPPQ" varchar(12) NOT NULL default ' ',
  "AAABMPPR" varchar(8) NOT NULL default '00000000',
  "AAABMPPS" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMPPC","AAABMPPD","AAABMPPE","AAABMGUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABISOY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABISXL" varchar(9) NOT NULL default '000000000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABISXO" varchar(10) NOT NULL default '0000000000',
  "AAABMGUN" varchar(8) NOT NULL default '00000000',
  "AAABMGUO" varchar(8) NOT NULL default '00000000',
  "AAABISXR" varchar(4) NOT NULL default ' ',
  "AAABISXS" varchar(8) NOT NULL default '00000000',
  "AAABISXT" varchar(8) NOT NULL default '00000000',
  "AAABISXU" varchar(4) NOT NULL default '0000',
  "AAABMPPQ" varchar(12) NOT NULL default ' ',
  "AAABMPPR" varchar(8) NOT NULL default '00000000',
  "AAABMPPS" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMPPC","AAABMPPD","AAABMPPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABISPW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABISRB" varchar(13) NOT NULL default '0000000000000',
  "AAABISRC" varchar(4) NOT NULL default ' ',
  "AAABISRD" varchar(6) NOT NULL default '000000',
  "AAABISRE" varchar(4) NOT NULL default '0000',
  "AAABISQS" varchar(8) NOT NULL default '00000000',
  "AAABMQIZ" varchar(4) NOT NULL default '0000',
  "AAABMOMJ" varchar(4) NOT NULL default '0000',
  "AAABMQJB" varchar(4) NOT NULL default '0000',
  "AAABMQJC" varchar(13) NOT NULL default '0000000000000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMOLS" varchar(4) NOT NULL default ' ',
  "AAABMRFP" varchar(12) NOT NULL default ' ',
  "AAABISQM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABISRB","AAABISRC","AAABISRD","AAABISRE","AAABISQS","AAABMQIZ","AAABMOMJ","AAABMQJB","AAABMQJC","AAABMRFQ","AAABMQIX","AAABMRCU"),
  KEY "AAABISRA" ("AAABISRB","AAABISRC","AAABISRD","AAABISRE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABISRF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABMGOZ" varchar(6) NOT NULL default '000000',
  "AAABMGPA" varchar(6) NOT NULL default '000000',
  "AAABMGSM" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMPPC","AAABMPPD","AAABMPPE","AAABMGOZ","AAABMGPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABISRV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABISSB" varchar(10) NOT NULL default ' ',
  "AAABISRY" varchar(2) NOT NULL default ' ',
  "AAABISRZ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABISSB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABISSC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHT" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABISSH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABISTY" varchar(4) NOT NULL default '0000',
  "AAABISSV" varchar(50) NOT NULL default ' ',
  "AAABISSW" varchar(15) NOT NULL default ' ',
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  "AAABMRIF" varchar(1) NOT NULL default ' ',
  "AAABMRIG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIN"),
  KEY "AAABISSS" ("AAABMRIN","AAABISTY","AAABISSV","AAABISSW"),
  KEY "AAABISSX" ("AAABMRIL","AAABLXWK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABISTA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABISTE" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABISTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABISTF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABISTM" varchar(2) NOT NULL default ' ',
  "AAABISTN" varchar(2) NOT NULL default ' ',
  "AAABISTJ" varchar(2) NOT NULL default ' ',
  "AAABISTK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABISTM","AAABISTN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABISTO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABISTS" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABISTS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABISTT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABISTY" varchar(4) NOT NULL default '0000',
  "AAABISTW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABISTY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABISTZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABISUD" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABISUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABISUE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABISUM" varchar(4) NOT NULL default '0000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABISUO" varchar(14) NOT NULL default '00000000000000',
  "AAABISUJ" varchar(25) NOT NULL default ' ',
  "AAABMOUK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABISUM","AAABMRIN","AAABISUO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABISUP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABISUU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABMNJQ" varchar(4) NOT NULL default ' ',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMORC","AAABMNJQ","AAABMOGK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABISVD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABISVK" varchar(8) NOT NULL default '00000000',
  "AAABISXL" varchar(9) NOT NULL default '000000000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMGTN" varchar(4) NOT NULL default ' ',
  "AAABMIMR" varchar(4) NOT NULL default '0000',
  "AAABMGTP" varchar(4) NOT NULL default ' ',
  "AAABMGTQ" varchar(6) NOT NULL default '000000',
  "AAABMGTR" varchar(4) NOT NULL default '0000',
  "AAABMGTS" varchar(9) NOT NULL default '000000000',
  "AAABMPPQ" varchar(12) NOT NULL default ' ',
  "AAABMPPR" varchar(8) NOT NULL default '00000000',
  "AAABMPPS" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMPPC","AAABMPPD","AAABMPPE","AAABMPQE"),
  KEY "AAABISWD" ("AAABMPPC","AAABMPPD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABISWG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABMGUY" varchar(4) NOT NULL default '0000',
  "AAABISXL" varchar(9) NOT NULL default '000000000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMGUN" varchar(8) NOT NULL default '00000000',
  "AAABMGUO" varchar(8) NOT NULL default '00000000',
  "AAABMPUN" varchar(1) NOT NULL default ' ',
  "AAABMPPQ" varchar(12) NOT NULL default ' ',
  "AAABMPPR" varchar(8) NOT NULL default '00000000',
  "AAABMPPS" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMPPC","AAABMPPD","AAABMPPE","AAABMGUY"),
  KEY "AAABISXD" ("AAABMPPC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABISXF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABISXL" varchar(9) NOT NULL default '000000000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABISXO" varchar(10) NOT NULL default '0000000000',
  "AAABMGUN" varchar(8) NOT NULL default '00000000',
  "AAABMGUO" varchar(8) NOT NULL default '00000000',
  "AAABISXR" varchar(4) NOT NULL default ' ',
  "AAABISXS" varchar(8) NOT NULL default '00000000',
  "AAABISXT" varchar(8) NOT NULL default '00000000',
  "AAABISXU" varchar(4) NOT NULL default '0000',
  "AAABMPPQ" varchar(12) NOT NULL default ' ',
  "AAABMPPR" varchar(8) NOT NULL default '00000000',
  "AAABMPPS" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMPPC","AAABMPPD","AAABMPPE"),
  KEY "AAABISYD" ("AAABMPPC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABISYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUP" varchar(10) NOT NULL default ' ',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMRDD" varchar(1) NOT NULL default ' ',
  "AAABMPPH" varchar(8) NOT NULL default '00000000',
  "AAABMPPI" varchar(8) NOT NULL default '00000000',
  "AAABMPPJ" varchar(4) NOT NULL default ' ',
  "AAABMPPK" varchar(4) NOT NULL default ' ',
  "AAABMPPL" varchar(2) NOT NULL default ' ',
  "AAABMPPM" varchar(2) NOT NULL default ' ',
  "AAABMPPN" varchar(4) NOT NULL default ' ',
  "AAABMPPO" varchar(8) NOT NULL default ' ',
  "AAABMPPP" varchar(8) NOT NULL default '00000000',
  "AAABMPPQ" varchar(12) NOT NULL default ' ',
  "AAABMPPR" varchar(8) NOT NULL default '00000000',
  "AAABMPPS" varchar(6) NOT NULL default '000000',
  "AAABMPUM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPUP"),
  KEY "AAABISZF" ("AAABMRHL","AAABMPPC","AAABMPPD","AAABMPPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABISZK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUP" varchar(10) NOT NULL default ' ',
  "AAABMPUQ" varchar(6) NOT NULL default '000000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPQG" varchar(4) NOT NULL default ' ',
  "AAABMPQH" varchar(6) NOT NULL default '000000',
  "AAABMPQI" varchar(4) NOT NULL default '0000',
  "AAABMPQJ" varchar(8) NOT NULL default ' ',
  "AAABMPQK" varchar(4) NOT NULL default '0000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMRCJ" varchar(8) NOT NULL default '00000000',
  "AAABMPQQ" varchar(4) NOT NULL default '0000',
  "AAABMPQR" varchar(4) NOT NULL default ' ',
  "AAABMPQS" varchar(1) NOT NULL default ' ',
  "AAABMPQT" varchar(3) NOT NULL default ' ',
  "AAABMPQU" varchar(1) NOT NULL default ' ',
  "AAABMPQV" varchar(10) NOT NULL default ' ',
  "AAABMPQW" varchar(1) NOT NULL default ' ',
  "AAABMPQX" varchar(6) NOT NULL default '000000',
  "AAABMPQY" varchar(1) NOT NULL default ' ',
  "AAABMPQZ" varchar(8) NOT NULL default '00000000',
  "AAABMPRA" varchar(1) NOT NULL default ' ',
  "AAABMPRB" varchar(1) NOT NULL default ' ',
  "AAABMPRC" varchar(4) NOT NULL default '0000',
  "AAABMPRD" varchar(8) NOT NULL default ' ',
  "AAABMPRE" varchar(6) NOT NULL default '000000',
  "AAABMPRF" varchar(4) NOT NULL default ' ',
  "AAABMPRG" varchar(2) NOT NULL default ' ',
  "AAABMPRH" varchar(2) NOT NULL default ' ',
  "AAABMPRI" varchar(4) NOT NULL default ' ',
  "AAABMPRJ" varchar(4) NOT NULL default ' ',
  "AAABMPRK" varchar(13) NOT NULL default '0000000000000',
  "AAABMPRL" varchar(10) NOT NULL default ' ',
  "AAABMPRM" varchar(10) NOT NULL default ' ',
  "AAABMPRN" varchar(10) NOT NULL default ' ',
  "AAABMPRO" varchar(10) NOT NULL default ' ',
  "AAABMPRP" varchar(10) NOT NULL default ' ',
  "AAABMPRQ" varchar(10) NOT NULL default ' ',
  "AAABMPRR" varchar(10) NOT NULL default ' ',
  "AAABMPRS" varchar(10) NOT NULL default ' ',
  "AAABMPRT" varchar(10) NOT NULL default ' ',
  "AAABMPRU" varchar(10) NOT NULL default ' ',
  "AAABMPRV" varchar(10) NOT NULL default ' ',
  "AAABMPRW" varchar(8) NOT NULL default '00000000',
  "AAABMPRX" decimal(16,2) NOT NULL default '0.00',
  "AAABMPRY" varchar(5) NOT NULL default ' ',
  "AAABMPRZ" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMPSB" varchar(18) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABMPSD" varchar(4) NOT NULL default ' ',
  "AAABMPSE" varchar(1) NOT NULL default ' ',
  "AAABMPSF" varchar(1) NOT NULL default ' ',
  "AAABMPSG" varchar(1) NOT NULL default ' ',
  "AAABMPSH" varchar(1) NOT NULL default ' ',
  "AAABMPSI" varchar(4) NOT NULL default ' ',
  "AAABMPSJ" varchar(2) NOT NULL default ' ',
  "AAABMPSK" varchar(8) NOT NULL default '00000000',
  "AAABMPSL" varchar(8) NOT NULL default '00000000',
  "AAABMPSM" varchar(8) NOT NULL default '00000000',
  "AAABMPSN" varchar(8) NOT NULL default '00000000',
  "AAABMPSO" varchar(4) NOT NULL default ' ',
  "AAABMPSP" varchar(2) NOT NULL default ' ',
  "AAABMPSQ" varchar(2) NOT NULL default ' ',
  "AAABMPSR" varchar(4) NOT NULL default ' ',
  "AAABMPSS" varchar(4) NOT NULL default ' ',
  "AAABMPST" varchar(13) NOT NULL default '0000000000000',
  "AAABMPSU" decimal(16,2) NOT NULL default '0.00',
  "AAABMPSV" varchar(5) NOT NULL default ' ',
  "AAABMPSW" decimal(9,5) NOT NULL default '0.00000',
  "AAABMPSX" varchar(4) NOT NULL default ' ',
  "AAABMPSY" decimal(7,3) NOT NULL default '0.000',
  "AAABMPSZ" decimal(7,3) NOT NULL default '0.000',
  "AAABMPTA" decimal(7,3) NOT NULL default '0.000',
  "AAABMPTB" decimal(7,3) NOT NULL default '0.000',
  "AAABMPTC" decimal(16,2) NOT NULL default '0.00',
  "AAABMPTD" varchar(5) NOT NULL default ' ',
  "AAABMPTE" varchar(6) NOT NULL default ' ',
  "AAABMPTF" varchar(10) NOT NULL default ' ',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAABMPTH" varchar(1) NOT NULL default '0',
  "AAABMPTI" varchar(6) NOT NULL default '000000',
  "AAABMPTJ" int(11) NOT NULL default '0',
  "AAABMPTK" varchar(18) NOT NULL default ' ',
  "AAABMPTL" varchar(18) NOT NULL default ' ',
  "AAABMPTM" decimal(15,5) NOT NULL default '0.00000',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAABMPTO" decimal(15,5) NOT NULL default '0.00000',
  "AAABMPTP" varchar(3) NOT NULL default ' ',
  "AAABMPTQ" decimal(16,2) NOT NULL default '0.00',
  "AAABMPTR" varchar(5) NOT NULL default ' ',
  "AAABMPTS" varchar(10) NOT NULL default ' ',
  "AAABMPTT" varchar(10) NOT NULL default ' ',
  "AAABMPTU" varchar(12) NOT NULL default ' ',
  "AAABMPTV" varchar(10) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABMPTX" decimal(13,2) NOT NULL default '0.00',
  "AAABMPTY" decimal(13,2) NOT NULL default '0.00',
  "AAABMPTZ" varchar(5) NOT NULL default ' ',
  "AAABMPUA" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMPUC" varchar(50) NOT NULL default ' ',
  "AAABMPUD" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPUE" varchar(6) NOT NULL default ' ',
  "AAABMPUF" varchar(6) NOT NULL default ' ',
  "AAABMPUG" varchar(8) NOT NULL default '00000000',
  "AAABMPUH" varchar(3) NOT NULL default ' ',
  "AAABMPUI" varchar(5) NOT NULL default ' ',
  "AAABMPUJ" decimal(15,5) NOT NULL default '0.00000',
  "AAABMPUK" decimal(16,2) NOT NULL default '0.00',
  "AAABMPUL" varchar(4) NOT NULL default '0000',
  "AAABMPUM" varchar(1) NOT NULL default ' ',
  "AAABMPUN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPUP","AAABMPUQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABITEC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUP" varchar(10) NOT NULL default ' ',
  "AAABMPUQ" varchar(6) NOT NULL default '000000',
  "AAABMPEW" varchar(6) NOT NULL default '000000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPQG" varchar(4) NOT NULL default ' ',
  "AAABMPQH" varchar(6) NOT NULL default '000000',
  "AAABMPQI" varchar(4) NOT NULL default '0000',
  "AAABMPQJ" varchar(8) NOT NULL default ' ',
  "AAABMPQK" varchar(4) NOT NULL default '0000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMRCJ" varchar(8) NOT NULL default '00000000',
  "AAABMPQQ" varchar(4) NOT NULL default '0000',
  "AAABMPQR" varchar(4) NOT NULL default ' ',
  "AAABMPQS" varchar(1) NOT NULL default ' ',
  "AAABMPQT" varchar(3) NOT NULL default ' ',
  "AAABMPQU" varchar(1) NOT NULL default ' ',
  "AAABMPQV" varchar(10) NOT NULL default ' ',
  "AAABMPQW" varchar(1) NOT NULL default ' ',
  "AAABMPQX" varchar(6) NOT NULL default '000000',
  "AAABMPQY" varchar(1) NOT NULL default ' ',
  "AAABMPQZ" varchar(8) NOT NULL default '00000000',
  "AAABMPRA" varchar(1) NOT NULL default ' ',
  "AAABMPRB" varchar(1) NOT NULL default ' ',
  "AAABMPRC" varchar(4) NOT NULL default '0000',
  "AAABMPRD" varchar(8) NOT NULL default ' ',
  "AAABMPRE" varchar(6) NOT NULL default '000000',
  "AAABMPRF" varchar(4) NOT NULL default ' ',
  "AAABMPRG" varchar(2) NOT NULL default ' ',
  "AAABMPRH" varchar(2) NOT NULL default ' ',
  "AAABMPRI" varchar(4) NOT NULL default ' ',
  "AAABMPRJ" varchar(4) NOT NULL default ' ',
  "AAABMPRK" varchar(13) NOT NULL default '0000000000000',
  "AAABMPRL" varchar(10) NOT NULL default ' ',
  "AAABMPRM" varchar(10) NOT NULL default ' ',
  "AAABMPRN" varchar(10) NOT NULL default ' ',
  "AAABMPRO" varchar(10) NOT NULL default ' ',
  "AAABMPRP" varchar(10) NOT NULL default ' ',
  "AAABMPRQ" varchar(10) NOT NULL default ' ',
  "AAABMPRR" varchar(10) NOT NULL default ' ',
  "AAABMPRS" varchar(10) NOT NULL default ' ',
  "AAABMPRT" varchar(10) NOT NULL default ' ',
  "AAABMPRU" varchar(10) NOT NULL default ' ',
  "AAABMPRV" varchar(10) NOT NULL default ' ',
  "AAABMPRW" varchar(8) NOT NULL default '00000000',
  "AAABMPRX" decimal(16,2) NOT NULL default '0.00',
  "AAABMPRY" varchar(5) NOT NULL default ' ',
  "AAABMPRZ" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMPSB" varchar(18) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABMPSD" varchar(4) NOT NULL default ' ',
  "AAABMPSE" varchar(1) NOT NULL default ' ',
  "AAABMPSF" varchar(1) NOT NULL default ' ',
  "AAABMPSG" varchar(1) NOT NULL default ' ',
  "AAABMPSH" varchar(1) NOT NULL default ' ',
  "AAABMPSI" varchar(4) NOT NULL default ' ',
  "AAABMPSJ" varchar(2) NOT NULL default ' ',
  "AAABMPSK" varchar(8) NOT NULL default '00000000',
  "AAABMPSL" varchar(8) NOT NULL default '00000000',
  "AAABMPSM" varchar(8) NOT NULL default '00000000',
  "AAABMPSN" varchar(8) NOT NULL default '00000000',
  "AAABMPSO" varchar(4) NOT NULL default ' ',
  "AAABMPSP" varchar(2) NOT NULL default ' ',
  "AAABMPSQ" varchar(2) NOT NULL default ' ',
  "AAABMPSR" varchar(4) NOT NULL default ' ',
  "AAABMPSS" varchar(4) NOT NULL default ' ',
  "AAABMPST" varchar(13) NOT NULL default '0000000000000',
  "AAABMPSU" decimal(16,2) NOT NULL default '0.00',
  "AAABMPSV" varchar(5) NOT NULL default ' ',
  "AAABMPSW" decimal(9,5) NOT NULL default '0.00000',
  "AAABMPSX" varchar(4) NOT NULL default ' ',
  "AAABMPSY" decimal(7,3) NOT NULL default '0.000',
  "AAABMPSZ" decimal(7,3) NOT NULL default '0.000',
  "AAABMPTA" decimal(7,3) NOT NULL default '0.000',
  "AAABMPTB" decimal(7,3) NOT NULL default '0.000',
  "AAABMPTC" decimal(16,2) NOT NULL default '0.00',
  "AAABMPTD" varchar(5) NOT NULL default ' ',
  "AAABMPTE" varchar(6) NOT NULL default ' ',
  "AAABMPTF" varchar(10) NOT NULL default ' ',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAABMPTH" varchar(1) NOT NULL default '0',
  "AAABMPTI" varchar(6) NOT NULL default '000000',
  "AAABMPTJ" int(11) NOT NULL default '0',
  "AAABMPTK" varchar(18) NOT NULL default ' ',
  "AAABMPTL" varchar(18) NOT NULL default ' ',
  "AAABMPTM" decimal(15,5) NOT NULL default '0.00000',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAABMPTO" decimal(15,5) NOT NULL default '0.00000',
  "AAABMPTP" varchar(3) NOT NULL default ' ',
  "AAABMPTQ" decimal(16,2) NOT NULL default '0.00',
  "AAABMPTR" varchar(5) NOT NULL default ' ',
  "AAABMPTS" varchar(10) NOT NULL default ' ',
  "AAABMPTT" varchar(10) NOT NULL default ' ',
  "AAABMPTU" varchar(12) NOT NULL default ' ',
  "AAABMPTV" varchar(10) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABMPTX" decimal(13,2) NOT NULL default '0.00',
  "AAABMPTY" decimal(13,2) NOT NULL default '0.00',
  "AAABMPTZ" varchar(5) NOT NULL default ' ',
  "AAABMPUA" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMPUC" varchar(50) NOT NULL default ' ',
  "AAABMPUD" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPUE" varchar(6) NOT NULL default ' ',
  "AAABMPUF" varchar(6) NOT NULL default ' ',
  "AAABMPUG" varchar(8) NOT NULL default '00000000',
  "AAABMPUH" varchar(3) NOT NULL default ' ',
  "AAABMPUI" varchar(5) NOT NULL default ' ',
  "AAABMPUJ" decimal(15,5) NOT NULL default '0.00000',
  "AAABMPUK" decimal(16,2) NOT NULL default '0.00',
  "AAABMPUM" varchar(1) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMPUP","AAABMPUQ","AAABMPEW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABITIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABITJY" varchar(20) NOT NULL default ' ',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMKGE" varchar(1) NOT NULL default ' ',
  "AAABITJC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABITJY","AAABMOGK","AAABMNJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABITJH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABITJY" varchar(20) NOT NULL default ' ',
  "AAABMLZS" varchar(3) NOT NULL default '000',
  "AAABMKGE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABITJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABITJO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMICK" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMICK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABITJT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOTH" varchar(4) NOT NULL default ' ',
  "AAABITJY" varchar(20) NOT NULL default ' ',
  "AAABMRDD" varchar(1) NOT NULL default ' ',
  "AAABITKA" varchar(1) NOT NULL default ' ',
  "AAABITKB" varchar(2) NOT NULL default ' ',
  "AAABITKC" varchar(2) NOT NULL default ' ',
  "AAABMKWZ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABITKH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABMNJQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMORC","AAABMNJQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABITKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIUNU" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABIUNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABITKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIUOC" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABIUOC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABITKY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABMRGE" varchar(4) NOT NULL default '0000',
  "AAABITLC" decimal(7,0) NOT NULL default '0',
  "AAABITLD" varchar(1) NOT NULL default ' ',
  "AAABITLE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIN","AAABMRGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABITLI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABITLO" varchar(72) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMMJT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABITLO","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABITLQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUP" varchar(10) NOT NULL default ' ',
  "AAABMPUQ" varchar(6) NOT NULL default '000000',
  "AAABMPEW" varchar(6) NOT NULL default '000000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPQG" varchar(4) NOT NULL default ' ',
  "AAABMPQH" varchar(6) NOT NULL default '000000',
  "AAABMPQI" varchar(4) NOT NULL default '0000',
  "AAABMPQJ" varchar(8) NOT NULL default ' ',
  "AAABMPQK" varchar(4) NOT NULL default '0000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMRCJ" varchar(8) NOT NULL default '00000000',
  "AAABMPQQ" varchar(4) NOT NULL default '0000',
  "AAABMPQR" varchar(4) NOT NULL default ' ',
  "AAABMPQS" varchar(1) NOT NULL default ' ',
  "AAABMPQT" varchar(3) NOT NULL default ' ',
  "AAABMPQU" varchar(1) NOT NULL default ' ',
  "AAABMPQV" varchar(10) NOT NULL default ' ',
  "AAABMPQW" varchar(1) NOT NULL default ' ',
  "AAABMPQX" varchar(6) NOT NULL default '000000',
  "AAABMPQY" varchar(1) NOT NULL default ' ',
  "AAABMPQZ" varchar(8) NOT NULL default '00000000',
  "AAABMPRA" varchar(1) NOT NULL default ' ',
  "AAABMPRB" varchar(1) NOT NULL default ' ',
  "AAABMPRC" varchar(4) NOT NULL default '0000',
  "AAABMPRD" varchar(8) NOT NULL default ' ',
  "AAABMPRE" varchar(6) NOT NULL default '000000',
  "AAABMPRF" varchar(4) NOT NULL default ' ',
  "AAABMPRG" varchar(2) NOT NULL default ' ',
  "AAABMPRH" varchar(2) NOT NULL default ' ',
  "AAABMPRI" varchar(4) NOT NULL default ' ',
  "AAABMPRJ" varchar(4) NOT NULL default ' ',
  "AAABMPRK" varchar(13) NOT NULL default '0000000000000',
  "AAABMPRL" varchar(10) NOT NULL default ' ',
  "AAABMPRM" varchar(10) NOT NULL default ' ',
  "AAABMPRN" varchar(10) NOT NULL default ' ',
  "AAABMPRO" varchar(10) NOT NULL default ' ',
  "AAABMPRP" varchar(10) NOT NULL default ' ',
  "AAABMPRQ" varchar(10) NOT NULL default ' ',
  "AAABMPRR" varchar(10) NOT NULL default ' ',
  "AAABMPRS" varchar(10) NOT NULL default ' ',
  "AAABMPRT" varchar(10) NOT NULL default ' ',
  "AAABMPRU" varchar(10) NOT NULL default ' ',
  "AAABMPRV" varchar(10) NOT NULL default ' ',
  "AAABMPRW" varchar(8) NOT NULL default '00000000',
  "AAABMPRX" decimal(16,2) NOT NULL default '0.00',
  "AAABMPRY" varchar(5) NOT NULL default ' ',
  "AAABMPRZ" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMPSB" varchar(18) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABMPSD" varchar(4) NOT NULL default ' ',
  "AAABMPSE" varchar(1) NOT NULL default ' ',
  "AAABMPSF" varchar(1) NOT NULL default ' ',
  "AAABMPSG" varchar(1) NOT NULL default ' ',
  "AAABMPSH" varchar(1) NOT NULL default ' ',
  "AAABMPSI" varchar(4) NOT NULL default ' ',
  "AAABMPSJ" varchar(2) NOT NULL default ' ',
  "AAABMPSK" varchar(8) NOT NULL default '00000000',
  "AAABMPSL" varchar(8) NOT NULL default '00000000',
  "AAABMPSM" varchar(8) NOT NULL default '00000000',
  "AAABMPSN" varchar(8) NOT NULL default '00000000',
  "AAABMPSO" varchar(4) NOT NULL default ' ',
  "AAABMPSP" varchar(2) NOT NULL default ' ',
  "AAABMPSQ" varchar(2) NOT NULL default ' ',
  "AAABMPSR" varchar(4) NOT NULL default ' ',
  "AAABMPSS" varchar(4) NOT NULL default ' ',
  "AAABMPST" varchar(13) NOT NULL default '0000000000000',
  "AAABMPSU" decimal(16,2) NOT NULL default '0.00',
  "AAABMPSV" varchar(5) NOT NULL default ' ',
  "AAABMPSW" decimal(9,5) NOT NULL default '0.00000',
  "AAABMPSX" varchar(4) NOT NULL default ' ',
  "AAABMPSY" decimal(7,3) NOT NULL default '0.000',
  "AAABMPSZ" decimal(7,3) NOT NULL default '0.000',
  "AAABMPTA" decimal(7,3) NOT NULL default '0.000',
  "AAABMPTB" decimal(7,3) NOT NULL default '0.000',
  "AAABMPTC" decimal(16,2) NOT NULL default '0.00',
  "AAABMPTD" varchar(5) NOT NULL default ' ',
  "AAABMPTE" varchar(6) NOT NULL default ' ',
  "AAABMPTF" varchar(10) NOT NULL default ' ',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAABMPTH" varchar(1) NOT NULL default '0',
  "AAABMPTI" varchar(6) NOT NULL default '000000',
  "AAABMPTJ" int(11) NOT NULL default '0',
  "AAABMPTK" varchar(18) NOT NULL default ' ',
  "AAABMPTL" varchar(18) NOT NULL default ' ',
  "AAABMPTM" decimal(15,5) NOT NULL default '0.00000',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAABMPTO" decimal(15,5) NOT NULL default '0.00000',
  "AAABMPTP" varchar(3) NOT NULL default ' ',
  "AAABMPTQ" decimal(16,2) NOT NULL default '0.00',
  "AAABMPTR" varchar(5) NOT NULL default ' ',
  "AAABMPTS" varchar(10) NOT NULL default ' ',
  "AAABMPTT" varchar(10) NOT NULL default ' ',
  "AAABMPTU" varchar(12) NOT NULL default ' ',
  "AAABMPTV" varchar(10) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABMPTX" decimal(13,2) NOT NULL default '0.00',
  "AAABMPTY" decimal(13,2) NOT NULL default '0.00',
  "AAABMPTZ" varchar(5) NOT NULL default ' ',
  "AAABMPUA" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMPUC" varchar(50) NOT NULL default ' ',
  "AAABMPUD" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPUE" varchar(6) NOT NULL default ' ',
  "AAABMPUF" varchar(6) NOT NULL default ' ',
  "AAABMPUG" varchar(8) NOT NULL default '00000000',
  "AAABMPUH" varchar(3) NOT NULL default ' ',
  "AAABMPUI" varchar(5) NOT NULL default ' ',
  "AAABMPUJ" decimal(15,5) NOT NULL default '0.00000',
  "AAABMPUK" decimal(16,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMPUP","AAABMPUQ","AAABMPEW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABITQH" (
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
  "AAABMQCO" varchar(6) NOT NULL default ' ',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABITZM" varchar(10) NOT NULL default ' ',
  "AAABITZN" varchar(13) NOT NULL default ' ',
  "AAABITZO" varchar(1) NOT NULL default ' ',
  "AAABITZP" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAABMPWJ" varchar(2) NOT NULL default ' ',
  "AAABMPWK" varchar(2) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMQDV" varchar(4) NOT NULL default ' ',
  "AAABMQDW" varchar(10) NOT NULL default ' ',
  "AAABMPVO" decimal(17,2) NOT NULL default '0.00',
  "AAABMPVP" decimal(17,2) NOT NULL default '0.00',
  "AAABMPVQ" decimal(17,2) NOT NULL default '0.00',
  "AAABMPVR" decimal(15,3) NOT NULL default '0.000',
  "AAABMPVS" decimal(15,3) NOT NULL default '0.000',
  "AAABMQHE" varchar(3) NOT NULL default ' ',
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
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMPWM" varchar(8) NOT NULL default '00000000',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMQCG"),
  UNIQUE KEY "AAABITSI" ("AAABMQHU","AAABMQCJ","AAABMQHY","AAABMQCL","AAABMQDO","AAABMQHV","AAABMQCO"),
  KEY "AAABITSQ" ("AAABMPWM","AAABMQHU","AAABMQDO","AAABITZM","AAABITZN","AAABITZO","AAABITZP"),
  KEY "AAABITSY" ("AAABMQHU","AAABMQCV","AAABMQCW","AAABMQCX","AAABMQDO","AAABMQHV","AAABMQDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABITTG" (
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
  "AAABMQCO" varchar(6) NOT NULL default ' ',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABITZM" varchar(10) NOT NULL default ' ',
  "AAABITZN" varchar(13) NOT NULL default ' ',
  "AAABITZO" varchar(1) NOT NULL default ' ',
  "AAABITZP" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAABMPWJ" varchar(2) NOT NULL default ' ',
  "AAABMPWK" varchar(2) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMQDV" varchar(4) NOT NULL default ' ',
  "AAABMQDW" varchar(10) NOT NULL default ' ',
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
  UNIQUE KEY "AAABITYE" ("AAABMQHU","AAABMQCJ","AAABMQHY","AAABMQCL","AAABMQDO","AAABMQHV","AAABMQCO","AAABMQHS"),
  KEY "AAABITYN" ("AAABMQHU","AAABMQHZ"),
  KEY "AAABITYQ" ("AAABMQHU","AAABMQCV","AAABMQCW","AAABMQCX","AAABMQDO","AAABMQHV","AAABMQDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABITYY" (
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
  "AAABITZM" varchar(10) NOT NULL default ' ',
  "AAABITZN" varchar(13) NOT NULL default ' ',
  "AAABITZO" varchar(1) NOT NULL default ' ',
  "AAABITZP" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAABMPWJ" varchar(2) NOT NULL default ' ',
  "AAABMPWK" varchar(2) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMQDV" varchar(4) NOT NULL default ' ',
  "AAABMQDW" varchar(10) NOT NULL default ' ',
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
  KEY "AAABIUDS" ("AAABMQHU","AAABMQHV","AAABMQHW","AAABMQHX","AAABMQHY","AAABMQHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUDZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABMRGE" varchar(4) NOT NULL default '0000',
  "AAABIUED" decimal(7,0) NOT NULL default '0',
  "AAABIUEE" decimal(7,0) NOT NULL default '0',
  "AAABIUEF" decimal(7,0) NOT NULL default '0',
  "AAABIUEG" decimal(7,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMRIN","AAABMRGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUEK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMOTH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIZ" varchar(4) NOT NULL default '0000',
  "AAABMOMJ" varchar(4) NOT NULL default '0000',
  "AAABMQJB" varchar(4) NOT NULL default '0000',
  "AAABMQJC" varchar(13) NOT NULL default '0000000000000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMOLS" varchar(4) NOT NULL default ' ',
  "AAABMRFP" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQIZ","AAABMOMJ","AAABMQJB","AAABMQJC","AAABMRFQ","AAABMQIX","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUFJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMQPU" varchar(1) NOT NULL default ' ',
  "AAABMGHG" varchar(10) NOT NULL default ' ',
  "AAABMRFP" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL"),
  KEY "AAABIUFV" ("AAABMQPU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUFX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMGGH" varchar(30) NOT NULL default ' ',
  "AAABIUGD" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMRIM","AAABMRDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUGI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRCU" varchar(9) NOT NULL default '000000000',
  "AAABIUGM" varchar(8) NOT NULL default '00000000',
  "AAABIUGN" decimal(16,2) NOT NULL default '0.00',
  "AAABIUGO" varchar(5) NOT NULL default ' ',
  "AAABIUGP" varchar(8) NOT NULL default '00000000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUGV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPLP" varchar(13) NOT NULL default '0000000000000',
  "AAABMRCU" varchar(9) NOT NULL default '000000000',
  "AAABIUGZ" varchar(8) NOT NULL default '00000000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  "AAABMJVD" varchar(80) NOT NULL default ' ',
  "AAABMJVE" varchar(80) NOT NULL default ' ',
  "AAABMJVF" varchar(80) NOT NULL default ' ',
  "AAABMJVG" varchar(80) NOT NULL default ' ',
  "AAABMJVH" varchar(80) NOT NULL default ' ',
  "AAABLTRV" varchar(80) NOT NULL default ' ',
  "AAABLTSB" varchar(80) NOT NULL default ' ',
  "AAABLTSH" varchar(80) NOT NULL default ' ',
  "AAABLTSN" varchar(80) NOT NULL default ' ',
  "AAABIUHL" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPLP","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUHP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOTH" varchar(4) NOT NULL default ' ',
  "AAABIUIJ" varchar(10) NOT NULL default ' ',
  "AAABMQPU" varchar(1) NOT NULL default ' ',
  "AAABMFSV" varchar(4) NOT NULL default '0000',
  "AAABMMCV" varchar(61) NOT NULL default ' ',
  "AAABMFSI" varchar(1) NOT NULL default '0',
  "AAABMFSJ" varchar(1) NOT NULL default '0',
  "AAABMFSK" varchar(1) NOT NULL default '0',
  "AAABMRIH" varchar(12) NOT NULL default ' ',
  "AAABMRII" varchar(8) NOT NULL default '00000000',
  "AAABMRIJ" varchar(12) NOT NULL default ' ',
  "AAABMRIK" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMOTH","AAABIUIJ","AAABMQPU","AAABMFSV","AAABMMCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUIN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMQPW" varchar(5) NOT NULL default '00000',
  "AAABMQPX" varchar(5) NOT NULL default '00000',
  "AAABMQPY" varchar(5) NOT NULL default '00000',
  "AAABMQPZ" varchar(5) NOT NULL default '00000',
  "AAABMQQA" varchar(5) NOT NULL default '00000',
  "AAABMRFP" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL"),
  KEY "AAABIUJA" ("AAABMRIL","AAABMQPW"),
  KEY "AAABIUJD" ("AAABMRIL","AAABMQPX"),
  KEY "AAABIUJG" ("AAABMRIL","AAABMQPY"),
  KEY "AAABIUJJ" ("AAABMRIL","AAABMQPZ"),
  KEY "AAABIUJM" ("AAABMRIL","AAABMQQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUJP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMELN" varchar(7) NOT NULL default ' ',
  "AAABMEGX" varchar(30) NOT NULL default ' ',
  "AAABIUJU" varchar(10) NOT NULL default ' ',
  "AAABIUJV" varchar(72) NOT NULL default ' ',
  "AAABKWRH" varchar(4) NOT NULL default ' ',
  "AAABIUJX" varchar(8) NOT NULL default '00000000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMELN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUKD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRCU" varchar(9) NOT NULL default '000000000',
  "AAABIUKH" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  "AAABIUKJ" varchar(8) NOT NULL default '00000000',
  "AAABIUKK" varchar(8) NOT NULL default '00000000',
  "AAABIUKL" varchar(4) NOT NULL default '0000',
  "AAABIUKM" varchar(8) NOT NULL default '00000000',
  "AAABIUKN" varchar(2) NOT NULL default '00',
  "AAABIUKO" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUKS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRCU" varchar(9) NOT NULL default '000000000',
  "AAABLXTG" varchar(14) NOT NULL default '00000000000000',
  "AAABIUKX" varchar(1) NOT NULL default ' ',
  "AAABIUKY" varchar(1) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIULE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRCU" varchar(9) NOT NULL default '000000000',
  "AAABMEGX" varchar(30) NOT NULL default ' ',
  "AAABJEGP" varchar(10) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIULP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRCU" varchar(9) NOT NULL default '000000000',
  "AAABIULT" varchar(8) NOT NULL default '00000000',
  "AAABIUMG" varchar(8) NOT NULL default '00000000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUMA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRCU" varchar(9) NOT NULL default '000000000',
  "AAABMEGX" varchar(30) NOT NULL default ' ',
  "AAABIUMF" varchar(10) NOT NULL default ' ',
  "AAABIUMG" varchar(8) NOT NULL default '00000000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUMM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRCU" varchar(9) NOT NULL default '000000000',
  "AAABIUMQ" varchar(13) NOT NULL default '0000000000000',
  "AAABMOIX" varchar(6) NOT NULL default '000000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUMX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIUNB" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABMRIL","AAABIUNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUNC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABMRGE" varchar(4) NOT NULL default '0000',
  "AAABIUNU" varchar(4) NOT NULL default '0000',
  "AAABIUOC" varchar(4) NOT NULL default '0000',
  "AAABIUNI" decimal(7,0) NOT NULL default '0',
  "AAABIUNJ" decimal(3,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMRIN","AAABMRGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUNN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIUNU" varchar(4) NOT NULL default '0000',
  "AAABIUNR" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABIUNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUNV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIUOC" varchar(4) NOT NULL default '0000',
  "AAABIUNZ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABIUOC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUOD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABMQIL"),
  KEY "AAABIUOJ" ("AAABMPQE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUOL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIUOQ" varchar(8) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIUOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUOR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIUQT" varchar(4) NOT NULL default ' ',
  "AAABMEJX" varchar(4) NOT NULL default '0000',
  "AAABMODF" varchar(30) NOT NULL default ' ',
  "AAABMEJR" varchar(30) NOT NULL default ' ',
  "AAABMEJS" varchar(8) NOT NULL default ' ',
  "AAABMEJT" varchar(6) NOT NULL default ' ',
  "AAABMEJU" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIUQT","AAABMEJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIUQM" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIUQM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUPJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIUQT" varchar(4) NOT NULL default ' ',
  "AAABMEJX" varchar(4) NOT NULL default '0000',
  "AAABMODF" varchar(30) NOT NULL default ' ',
  "AAABMEJR" varchar(30) NOT NULL default ' ',
  "AAABMEJS" varchar(8) NOT NULL default ' ',
  "AAABMEJT" varchar(6) NOT NULL default ' ',
  "AAABMEJU" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIUQT","AAABMEJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUPV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIUQT" varchar(4) NOT NULL default ' ',
  "AAABIUQM" varchar(10) NOT NULL default ' ',
  "AAABMEJX" varchar(4) NOT NULL default '0000',
  "AAABMODF" varchar(30) NOT NULL default ' ',
  "AAABMEJR" varchar(30) NOT NULL default ' ',
  "AAABMEJS" varchar(8) NOT NULL default ' ',
  "AAABMEJT" varchar(6) NOT NULL default ' ',
  "AAABMEJU" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIUQT","AAABIUQM","AAABMEJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUQJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLGN" varchar(4) NOT NULL default ' ',
  "AAABIUQM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUQP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIUQT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIUQT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUQU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKHPE" varchar(4) NOT NULL default ' ',
  "AAABMABE" varchar(3) NOT NULL default ' ',
  "AAABLXEK" varchar(6) NOT NULL default '000000',
  "AAABKHPH" varchar(8) NOT NULL default '00000000',
  "AAABKHPI" varchar(6) NOT NULL default '000000',
  "AAABKHPJ" varchar(6) NOT NULL default '000000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMGOG" varchar(10) NOT NULL default ' ',
  "AAABIURW" varchar(1) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABIURY" varchar(10) NOT NULL default ' ',
  "AAABMGOH" varchar(10) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMRDI" varchar(8) NOT NULL default '00000000',
  "AAABMRDJ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABKHPE","AAABMABE","AAABLXEK","AAABKHPH","AAABKHPI","AAABKHPJ","AAABMQII","AAABMGOG","AAABIURW","AAABMLCZ","AAABIURY","AAABMGOH","AAABMJXV","AAABMJXW","AAABMCVI"),
  KEY "AAABIUSD" ("AAABMRDI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUSF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNIF" varchar(4) NOT NULL default ' ',
  "AAABMNIO" varchar(4) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAABMLXR" varchar(2) NOT NULL default ' ',
  "AAABMLXS" varchar(8) NOT NULL default '00000000',
  "AAABMLXT" varchar(8) NOT NULL default '00000000',
  "AAABMLYK" varchar(8) NOT NULL default '00000000',
  "AAABMLXV" varchar(2) NOT NULL default '00',
  "AAABMLXW" varchar(3) NOT NULL default '000',
  "AAABMLXX" varchar(1) NOT NULL default ' ',
  "AAABMLXY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNIF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUSU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNIF" varchar(4) NOT NULL default ' ',
  "AAABMLYK" varchar(8) NOT NULL default '00000000',
  "AAABMRCJ" varchar(8) NOT NULL default '00000000',
  "AAABMRCQ" varchar(8) NOT NULL default '00000000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMNIF","AAABMLYK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUTE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMNIF" varchar(4) NOT NULL default ' ',
  "AAABMLYP" varchar(30) NOT NULL default ' ',
  "AAABMLYQ" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMNIF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUTN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMNIO" varchar(4) NOT NULL default ' ',
  "AAABMLZY" varchar(30) NOT NULL default ' ',
  "AAABMLZZ" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMNIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUTW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMNIU" varchar(5) NOT NULL default ' ',
  "AAABMMDA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMNIU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUUE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNIF" varchar(4) NOT NULL default ' ',
  "AAABMNIG" varchar(10) NOT NULL default ' ',
  "AAABMORS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNIF","AAABMNIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUUM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNIO" varchar(4) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMNYU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNIO","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUUV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNIU" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNIU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUVA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIVKK" varchar(15) NOT NULL default ' ',
  "AAABIVKH" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABIVKK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUVI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIVKK" varchar(15) NOT NULL default ' ',
  "AAABIUVL" varchar(15) NOT NULL default ' ',
  "AAABIVIW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIVKK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUVP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABKKHL" varchar(10) NOT NULL default ' ',
  "AAABIVPE" varchar(10) NOT NULL default ' ',
  "AAABIVPG" varchar(10) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABIVGV" varchar(10) NOT NULL default ' ',
  "AAABKGFD" varchar(8) NOT NULL default '00000000',
  "AAABIUXH" varchar(8) NOT NULL default '00000000',
  "AAABIVJW" varchar(2) NOT NULL default ' ',
  "AAABIVIG" varchar(2) NOT NULL default ' ',
  "AAABKQEQ" varchar(4) NOT NULL default ' ',
  "AAABIUWE" varchar(4) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKM" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABKKHL","AAABIVPE","AAABIVPG","AAABMNZR","AAABIVGV"),
  KEY "AAABIUWU" ("AAABMRIL","AAABIVGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUWX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABKKHL" varchar(10) NOT NULL default ' ',
  "AAABIVPE" varchar(10) NOT NULL default ' ',
  "AAABIVPG" varchar(10) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABKGFD" varchar(8) NOT NULL default '00000000',
  "AAABIUXH" varchar(8) NOT NULL default '00000000',
  "AAABIVJW" varchar(2) NOT NULL default ' ',
  "AAABIVIG" varchar(2) NOT NULL default ' ',
  "AAABKQEQ" varchar(4) NOT NULL default ' ',
  "AAABIUXL" varchar(1) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKM" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABKKHL","AAABIVPE","AAABIVPG","AAABMNZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUYA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABKKHL" varchar(10) NOT NULL default ' ',
  "AAABIVPE" varchar(10) NOT NULL default ' ',
  "AAABIVPG" varchar(10) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABIVJH" varchar(1) NOT NULL default ' ',
  "AAABIVDX" varchar(18) NOT NULL default ' ',
  "AAABIUYL" varchar(1) NOT NULL default ' ',
  "AAABIVDU" varchar(1) NOT NULL default ' ',
  "AAABKQEQ" varchar(4) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKM" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABKKHL","AAABIVPE","AAABIVPG","AAABMNZR","AAABIVJH","AAABIVDX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUZE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABIVIM" varchar(4) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABIUZL" varchar(4) NOT NULL default ' ',
  "AAABIUZM" varchar(4) NOT NULL default ' ',
  "AAABIUZN" varchar(4) NOT NULL default ' ',
  "AAABIUZO" varchar(4) NOT NULL default ' ',
  "AAABIUZP" varchar(4) NOT NULL default ' ',
  "AAABIUZQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABIVIM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIUZW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIVAY" varchar(15) NOT NULL default ' ',
  "AAABIUZZ" varchar(1) NOT NULL default ' ',
  "AAABIVAA" varchar(1) NOT NULL default ' ',
  "AAABIVAB" varchar(1) NOT NULL default ' ',
  "AAABIVAC" varchar(1) NOT NULL default ' ',
  "AAABIVAD" varchar(1) NOT NULL default ' ',
  "AAABIVAE" varchar(1) NOT NULL default ' ',
  "AAABIVAF" varchar(1) NOT NULL default ' ',
  "AAABIVAG" varchar(1) NOT NULL default ' ',
  "AAABIVAH" varchar(1) NOT NULL default ' ',
  "AAABIVAI" varchar(1) NOT NULL default ' ',
  "AAABIVAJ" varchar(1) NOT NULL default ' ',
  "AAABIVAK" varchar(1) NOT NULL default ' ',
  "AAABIVAL" varchar(1) NOT NULL default ' ',
  "AAABIVAM" varchar(1) NOT NULL default ' ',
  "AAABIVAN" varchar(1) NOT NULL default ' ',
  "AAABIVAO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIVAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVAR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIVAY" varchar(15) NOT NULL default ' ',
  "AAABIVID" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABIVAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVAZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABKKHL" varchar(10) NOT NULL default ' ',
  "AAABIVPE" varchar(10) NOT NULL default ' ',
  "AAABIVPG" varchar(10) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABIVPF" varchar(10) NOT NULL default ' ',
  "AAABIVPH" varchar(10) NOT NULL default ' ',
  "AAABIVPL" varchar(18) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABMRBY" varchar(4) NOT NULL default '0000',
  "AAABMCMR" varchar(2) NOT NULL default '00',
  "AAABLZMT" varchar(2) NOT NULL default '00',
  "AAABIVPI" varchar(8) NOT NULL default '00000000',
  "AAABMKET" varchar(8) NOT NULL default '00000000',
  "AAABIVJW" varchar(2) NOT NULL default ' ',
  "AAABIVGV" varchar(10) NOT NULL default ' ',
  "AAABIVPM" varchar(40) NOT NULL default ' ',
  "AAABIVPN" varchar(40) NOT NULL default ' ',
  "AAABMPTM" decimal(15,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABIVPR" decimal(19,2) NOT NULL default '0.00',
  "AAABIVPS" decimal(19,2) NOT NULL default '0.00',
  "AAABIVPQ" varchar(5) NOT NULL default ' ',
  "AAABIVCA" varchar(1) NOT NULL default ' ',
  "AAABLXSK" varchar(18) NOT NULL default ' ',
  "AAABIVCC" varchar(2) NOT NULL default ' ',
  "AAABMRFO" varchar(4) NOT NULL default ' ',
  "AAABIVDU" varchar(1) NOT NULL default ' ',
  "AAABIVDV" varchar(1) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKM" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABKKHL","AAABIVPE","AAABIVPG","AAABMLCZ","AAABIVPF","AAABIVPH","AAABIVPL","AAABMLTD","AAABMRBY","AAABMCMR","AAABLZMT"),
  KEY "AAABIVDB" ("AAABMRIL","AAABIVGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVDE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABKKHL" varchar(10) NOT NULL default ' ',
  "AAABIVPE" varchar(10) NOT NULL default ' ',
  "AAABIVPG" varchar(10) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABMRBY" varchar(4) NOT NULL default '0000',
  "AAABMCMR" varchar(2) NOT NULL default '00',
  "AAABLZMT" varchar(2) NOT NULL default '00',
  "AAABIVPI" varchar(8) NOT NULL default '00000000',
  "AAABMKET" varchar(8) NOT NULL default '00000000',
  "AAABMRFO" varchar(4) NOT NULL default ' ',
  "AAABIVDU" varchar(1) NOT NULL default ' ',
  "AAABIVDV" varchar(1) NOT NULL default ' ',
  "AAABIVJH" varchar(1) NOT NULL default ' ',
  "AAABIVDX" varchar(18) NOT NULL default ' ',
  "AAABIVGG" varchar(4) NOT NULL default ' ',
  "AAABIVGV" varchar(10) NOT NULL default ' ',
  "AAABMPTM" decimal(15,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABIVPR" decimal(19,2) NOT NULL default '0.00',
  "AAABIVPS" decimal(19,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABIVEF" varchar(1) NOT NULL default ' ',
  "AAABIVEG" varchar(10) NOT NULL default ' ',
  "AAABIVEH" varchar(10) NOT NULL default ' ',
  "AAABIVEI" varchar(10) NOT NULL default ' ',
  "AAABIVEJ" varchar(10) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKM" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABKKHL","AAABIVPE","AAABIVPG","AAABMLCZ","AAABMLTD","AAABMRBY","AAABMCMR","AAABLZMT"),
  KEY "AAABIVFC" ("AAABMRIL","AAABIVGV","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVFG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIVFL" varchar(1) NOT NULL default ' ',
  "AAABIVGV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIVFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVFM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIVFX" varchar(4) NOT NULL default ' ',
  "AAABKKHL" varchar(10) NOT NULL default ' ',
  "AAABIVPE" varchar(10) NOT NULL default ' ',
  "AAABIVPG" varchar(10) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIVFX","AAABKKHL","AAABIVPE","AAABIVPG","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVGC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIVKQ" varchar(15) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABIVGG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIVKQ","AAABMQII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABKKHL" varchar(10) NOT NULL default ' ',
  "AAABIVPE" varchar(10) NOT NULL default ' ',
  "AAABIVPG" varchar(10) NOT NULL default ' ',
  "AAABMRBY" varchar(4) NOT NULL default '0000',
  "AAABMCMR" varchar(2) NOT NULL default '00',
  "AAABLZMT" varchar(2) NOT NULL default '00',
  "AAABIVGV" varchar(10) NOT NULL default ' ',
  "AAABIVGW" varchar(1) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKM" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABKKHL","AAABIVPE","AAABIVPG","AAABMRBY","AAABMCMR","AAABLZMT"),
  KEY "AAABIVHN" ("AAABMRIL","AAABMRBY","AAABMCMR","AAABIVPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVHS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIVIG" varchar(2) NOT NULL default ' ',
  "AAABIVIW" varchar(30) NOT NULL default ' ',
  "AAABIVHW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIVIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIVIG" varchar(2) NOT NULL default ' ',
  "AAABIVID" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABIVIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVIH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIVIM" varchar(4) NOT NULL default ' ',
  "AAABIVIK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIVIM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVIN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIVIS" varchar(15) NOT NULL default ' ',
  "AAABIVIW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIVIS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVIT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIVJH" varchar(1) NOT NULL default ' ',
  "AAABIVIW" varchar(30) NOT NULL default ' ',
  "AAABIVIX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIVJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVJA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIVJH" varchar(1) NOT NULL default ' ',
  "AAABIVJE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABIVJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVJI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIVJW" varchar(2) NOT NULL default ' ',
  "AAABIVJL" decimal(2,0) NOT NULL default '0',
  "AAABMNIF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIVJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVJP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIVJW" varchar(2) NOT NULL default ' ',
  "AAABMOHD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABIVJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVJX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIVKK" varchar(15) NOT NULL default ' ',
  "AAABIVOE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIVKK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVKD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIVKK" varchar(15) NOT NULL default ' ',
  "AAABIVKH" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABIVKK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVKL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIVKQ" varchar(15) NOT NULL default ' ',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIVKQ","AAABMALG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVKS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKQEQ" varchar(4) NOT NULL default ' ',
  "AAABIVKV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKQEQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVKY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKQEQ" varchar(4) NOT NULL default ' ',
  "AAABMOHD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKQEQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVLG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABKKHL" varchar(10) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABIVPL" varchar(18) NOT NULL default ' ',
  "AAABLLXX" decimal(5,0) NOT NULL default '0',
  "AAABIVLP" varchar(3) NOT NULL default ' ',
  "AAABLLXY" decimal(5,0) NOT NULL default '0',
  "AAABIVLR" varchar(3) NOT NULL default ' ',
  "AAABIVPN" varchar(40) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKM" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABKKHL","AAABMLCZ","AAABIVPL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVMG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABKKHL" varchar(10) NOT NULL default ' ',
  "AAABIVNR" varchar(1) NOT NULL default ' ',
  "AAABIVNS" varchar(10) NOT NULL default ' ',
  "AAABIVNT" varchar(10) NOT NULL default ' ',
  "AAABLVHD" varchar(15) NOT NULL default ' ',
  "AAABLZCP" varchar(35) NOT NULL default ' ',
  "AAABLZCR" varchar(35) NOT NULL default ' ',
  "AAABLVHG" varchar(35) NOT NULL default ' ',
  "AAABLVHH" varchar(35) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABLVHI" varchar(35) NOT NULL default ' ',
  "AAABLVHJ" varchar(35) NOT NULL default ' ',
  "AAABLVHQ" varchar(35) NOT NULL default ' ',
  "AAABLXIZ" varchar(16) NOT NULL default ' ',
  "AAABLVHV" varchar(31) NOT NULL default ' ',
  "AAABIVNA" varchar(35) NOT NULL default ' ',
  "AAABIVNB" varchar(35) NOT NULL default ' ',
  "AAABIVNC" varchar(35) NOT NULL default ' ',
  "AAABIVND" varchar(35) NOT NULL default ' ',
  "AAABIVNE" varchar(35) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKM" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABKQEQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABKKHL","AAABIVNR","AAABIVNS","AAABIVNT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVNU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIVNZ" varchar(4) NOT NULL default ' ',
  "AAABIVNX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIVNZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVOA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIVPU" varchar(6) NOT NULL default '000000',
  "AAABIVOD" varchar(25) NOT NULL default ' ',
  "AAABIVOE" varchar(60) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABIVOI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIVPU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVOL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIVPU" varchar(6) NOT NULL default '000000',
  "AAABIVPV" varchar(7) NOT NULL default '0000000',
  "AAABIVOV" varchar(2) NOT NULL default '00',
  "AAABIVOQ" varchar(30) NOT NULL default ' ',
  "AAABIVOR" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIVPU","AAABIVPV","AAABIVOV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVOW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIVPU" varchar(6) NOT NULL default '000000',
  "AAABIVPV" varchar(7) NOT NULL default '0000000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABKKHL" varchar(10) NOT NULL default ' ',
  "AAABIVPE" varchar(10) NOT NULL default ' ',
  "AAABIVPF" varchar(10) NOT NULL default ' ',
  "AAABIVPG" varchar(10) NOT NULL default ' ',
  "AAABIVPH" varchar(10) NOT NULL default ' ',
  "AAABIVPI" varchar(8) NOT NULL default '00000000',
  "AAABMKET" varchar(8) NOT NULL default '00000000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABIVPL" varchar(18) NOT NULL default ' ',
  "AAABIVPM" varchar(40) NOT NULL default ' ',
  "AAABIVPN" varchar(40) NOT NULL default ' ',
  "AAABMPTM" decimal(15,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABIVPQ" varchar(5) NOT NULL default ' ',
  "AAABIVPR" decimal(19,2) NOT NULL default '0.00',
  "AAABIVPS" decimal(19,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABIVPU","AAABIVPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVPW" (
  "AAABIVVH" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIVVF" varchar(8) NOT NULL default '00000000',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABIWMM" varchar(32) NOT NULL default ' ',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  "AAABMRGO" varchar(1) NOT NULL default ' ',
  "AAABLECU" varchar(8) NOT NULL default '00000000',
  "AAABLECV" varchar(8) NOT NULL default '00000000',
  "AAABIVVC" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABIVVH","AAABIWFD","AAABIVVF"),
  KEY "AAABIVQK" ("AAABIVVH","AAABIWFD","AAABLECU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVQO" (
  "AAABIVVH" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIVVF" varchar(8) NOT NULL default '00000000',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABIWMM" varchar(32) NOT NULL default ' ',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  "AAABMRGO" varchar(1) NOT NULL default ' ',
  "AAABLECU" varchar(8) NOT NULL default '00000000',
  "AAABLECV" varchar(8) NOT NULL default '00000000',
  "AAABIVVC" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABIVVH","AAABIWFD","AAABIVVF"),
  KEY "AAABIVRC" ("AAABIVVH","AAABIWFD","AAABLECU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVRG" (
  "AAABIVVH" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIVVF" varchar(8) NOT NULL default '00000000',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABIWMM" varchar(32) NOT NULL default ' ',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  "AAABMRGO" varchar(1) NOT NULL default ' ',
  "AAABLECU" varchar(8) NOT NULL default '00000000',
  "AAABLECV" varchar(8) NOT NULL default '00000000',
  "AAABIVVC" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABIVVH","AAABIWFD","AAABIVVF"),
  KEY "AAABIVRU" ("AAABIVVH","AAABIWFD","AAABLECU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVRY" (
  "AAABIVVH" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIVVF" varchar(8) NOT NULL default '00000000',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABIWMM" varchar(32) NOT NULL default ' ',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  "AAABMRGO" varchar(1) NOT NULL default ' ',
  "AAABLECU" varchar(8) NOT NULL default '00000000',
  "AAABLECV" varchar(8) NOT NULL default '00000000',
  "AAABIVVC" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABIVVH","AAABIWFD","AAABIVVF"),
  KEY "AAABIVSM" ("AAABIVVH","AAABIWFD","AAABLECU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVSQ" (
  "AAABIVVH" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIVVF" varchar(8) NOT NULL default '00000000',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABIWMM" varchar(32) NOT NULL default ' ',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  "AAABMRGO" varchar(1) NOT NULL default ' ',
  "AAABLECU" varchar(8) NOT NULL default '00000000',
  "AAABLECV" varchar(8) NOT NULL default '00000000',
  "AAABIVVC" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABIVVH","AAABIWFD","AAABIVVF"),
  KEY "AAABIVTE" ("AAABIVVH","AAABIWFD","AAABLECU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVTI" (
  "AAABIVVH" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIVVF" varchar(8) NOT NULL default '00000000',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABIWMM" varchar(32) NOT NULL default ' ',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  "AAABMRGO" varchar(1) NOT NULL default ' ',
  "AAABLECU" varchar(8) NOT NULL default '00000000',
  "AAABLECV" varchar(8) NOT NULL default '00000000',
  "AAABIVVC" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABIVVH","AAABIWFD","AAABIVVF"),
  KEY "AAABIVTW" ("AAABIVVH","AAABIWFD","AAABLECU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVUA" (
  "AAABIVVH" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIVVF" varchar(8) NOT NULL default '00000000',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABIWMM" varchar(32) NOT NULL default ' ',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  "AAABMRGO" varchar(1) NOT NULL default ' ',
  "AAABLECU" varchar(8) NOT NULL default '00000000',
  "AAABLECV" varchar(8) NOT NULL default '00000000',
  "AAABIVVC" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABIVVH","AAABIWFD","AAABIVVF"),
  KEY "AAABIVUO" ("AAABIVVH","AAABIWFD","AAABLECU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVUS" (
  "AAABIVVH" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIVVF" varchar(8) NOT NULL default '00000000',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABIWMM" varchar(32) NOT NULL default ' ',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  "AAABMRGO" varchar(1) NOT NULL default ' ',
  "AAABLECU" varchar(8) NOT NULL default '00000000',
  "AAABLECV" varchar(8) NOT NULL default '00000000',
  "AAABIVVC" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABIVVH","AAABIWFD","AAABIVVF"),
  KEY "AAABIVVG" ("AAABIVVH","AAABIWFD","AAABLECU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVVK" (
  "AAABIWES" int(11) NOT NULL default '0',
  "AAABIWAW" varchar(1) NOT NULL default ' ',
  "AAABLKHX" int(11) NOT NULL default '0',
  "AAABLKHY" int(11) NOT NULL default '0',
  "AAABIWAT" int(11) NOT NULL default '0',
  "AAABIWAM" int(11) NOT NULL default '0',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  "AAABIWAO" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABIWES","AAABIWAW","AAABLKHX","AAABLKHY","AAABIWAT"),
  KEY "AAABIVVY" ("AAABIWES","AAABIWAW","AAABLKHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVWC" (
  "AAABIWES" int(11) NOT NULL default '0',
  "AAABIWAW" varchar(1) NOT NULL default ' ',
  "AAABLKHX" int(11) NOT NULL default '0',
  "AAABLKHY" int(11) NOT NULL default '0',
  "AAABIWAT" int(11) NOT NULL default '0',
  "AAABIWAM" int(11) NOT NULL default '0',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  "AAABIWAO" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABIWES","AAABIWAW","AAABLKHX","AAABLKHY","AAABIWAT"),
  KEY "AAABIVWQ" ("AAABIWES","AAABIWAW","AAABLKHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVWU" (
  "AAABIWES" int(11) NOT NULL default '0',
  "AAABIWAW" varchar(1) NOT NULL default ' ',
  "AAABLKHX" int(11) NOT NULL default '0',
  "AAABLKHY" int(11) NOT NULL default '0',
  "AAABIWAT" int(11) NOT NULL default '0',
  "AAABIWAM" int(11) NOT NULL default '0',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  "AAABIWAO" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABIWES","AAABIWAW","AAABLKHX","AAABLKHY","AAABIWAT"),
  KEY "AAABIVXI" ("AAABIWES","AAABIWAW","AAABLKHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVXM" (
  "AAABIWES" int(11) NOT NULL default '0',
  "AAABIWAW" varchar(1) NOT NULL default ' ',
  "AAABLKHX" int(11) NOT NULL default '0',
  "AAABLKHY" int(11) NOT NULL default '0',
  "AAABIWAT" int(11) NOT NULL default '0',
  "AAABIWAM" int(11) NOT NULL default '0',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  "AAABIWAO" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABIWES","AAABIWAW","AAABLKHX","AAABLKHY","AAABIWAT"),
  KEY "AAABIVYA" ("AAABIWES","AAABIWAW","AAABLKHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVYE" (
  "AAABIWES" int(11) NOT NULL default '0',
  "AAABIWAW" varchar(1) NOT NULL default ' ',
  "AAABLKHX" int(11) NOT NULL default '0',
  "AAABLKHY" int(11) NOT NULL default '0',
  "AAABIWAT" int(11) NOT NULL default '0',
  "AAABIWAM" int(11) NOT NULL default '0',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  "AAABIWAO" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABIWES","AAABIWAW","AAABLKHX","AAABLKHY","AAABIWAT"),
  KEY "AAABIVYS" ("AAABIWES","AAABIWAW","AAABLKHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVYW" (
  "AAABIWES" int(11) NOT NULL default '0',
  "AAABIWAW" varchar(1) NOT NULL default ' ',
  "AAABLKHX" int(11) NOT NULL default '0',
  "AAABLKHY" int(11) NOT NULL default '0',
  "AAABIWAT" int(11) NOT NULL default '0',
  "AAABIWAM" int(11) NOT NULL default '0',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  "AAABIWAO" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABIWES","AAABIWAW","AAABLKHX","AAABLKHY","AAABIWAT"),
  KEY "AAABIVZK" ("AAABIWES","AAABIWAW","AAABLKHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIVZO" (
  "AAABIWES" int(11) NOT NULL default '0',
  "AAABIWAW" varchar(1) NOT NULL default ' ',
  "AAABLKHX" int(11) NOT NULL default '0',
  "AAABLKHY" int(11) NOT NULL default '0',
  "AAABIWAT" int(11) NOT NULL default '0',
  "AAABIWAM" int(11) NOT NULL default '0',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  "AAABIWAO" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABIWES","AAABIWAW","AAABLKHX","AAABLKHY","AAABIWAT"),
  KEY "AAABIWAC" ("AAABIWES","AAABIWAW","AAABLKHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWAG" (
  "AAABIWES" int(11) NOT NULL default '0',
  "AAABIWAW" varchar(1) NOT NULL default ' ',
  "AAABLKHX" int(11) NOT NULL default '0',
  "AAABLKHY" int(11) NOT NULL default '0',
  "AAABIWAT" int(11) NOT NULL default '0',
  "AAABIWAM" int(11) NOT NULL default '0',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  "AAABIWAO" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABIWES","AAABIWAW","AAABLKHX","AAABLKHY","AAABIWAT"),
  KEY "AAABIWAU" ("AAABIWES","AAABIWAW","AAABLKHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWAY" (
  "AAABIWES" int(11) NOT NULL default '0',
  "AAABIWET" varchar(30) NOT NULL default ' ',
  "AAABIWMM" varchar(32) NOT NULL default ' ',
  "AAABIWEV" varchar(2) NOT NULL default '00',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWER" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABIWES","AAABIWET","AAABIWMM","AAABIWEV"),
  UNIQUE KEY "AAABIWBJ" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWBL" (
  "AAABIWES" int(11) NOT NULL default '0',
  "AAABIWET" varchar(30) NOT NULL default ' ',
  "AAABIWMM" varchar(32) NOT NULL default ' ',
  "AAABIWEV" varchar(2) NOT NULL default '00',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWER" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABIWES","AAABIWET","AAABIWMM","AAABIWEV"),
  UNIQUE KEY "AAABIWBW" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWBY" (
  "AAABIWES" int(11) NOT NULL default '0',
  "AAABIWET" varchar(30) NOT NULL default ' ',
  "AAABIWMM" varchar(32) NOT NULL default ' ',
  "AAABIWEV" varchar(2) NOT NULL default '00',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWER" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABIWES","AAABIWET","AAABIWMM","AAABIWEV"),
  UNIQUE KEY "AAABIWCJ" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWCL" (
  "AAABIWES" int(11) NOT NULL default '0',
  "AAABIWET" varchar(30) NOT NULL default ' ',
  "AAABIWMM" varchar(32) NOT NULL default ' ',
  "AAABIWEV" varchar(2) NOT NULL default '00',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWER" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABIWES","AAABIWET","AAABIWMM","AAABIWEV"),
  UNIQUE KEY "AAABIWCW" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWCY" (
  "AAABIWES" int(11) NOT NULL default '0',
  "AAABIWET" varchar(30) NOT NULL default ' ',
  "AAABIWMM" varchar(32) NOT NULL default ' ',
  "AAABIWEV" varchar(2) NOT NULL default '00',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWER" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABIWES","AAABIWET","AAABIWMM","AAABIWEV"),
  UNIQUE KEY "AAABIWDJ" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWDL" (
  "AAABIWES" int(11) NOT NULL default '0',
  "AAABIWET" varchar(30) NOT NULL default ' ',
  "AAABIWMM" varchar(32) NOT NULL default ' ',
  "AAABIWEV" varchar(2) NOT NULL default '00',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWER" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABIWES","AAABIWET","AAABIWMM","AAABIWEV"),
  UNIQUE KEY "AAABIWDW" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWDY" (
  "AAABIWES" int(11) NOT NULL default '0',
  "AAABIWET" varchar(30) NOT NULL default ' ',
  "AAABIWMM" varchar(32) NOT NULL default ' ',
  "AAABIWEV" varchar(2) NOT NULL default '00',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWER" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABIWES","AAABIWET","AAABIWMM","AAABIWEV"),
  UNIQUE KEY "AAABIWEJ" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWEL" (
  "AAABIWES" int(11) NOT NULL default '0',
  "AAABIWET" varchar(30) NOT NULL default ' ',
  "AAABIWMM" varchar(32) NOT NULL default ' ',
  "AAABIWEV" varchar(2) NOT NULL default '00',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWER" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABIWES","AAABIWET","AAABIWMM","AAABIWEV"),
  UNIQUE KEY "AAABIWEW" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWEY" (
  "AAABIWTI" varchar(3) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABJQWM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWTI","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWFE" (
  "AAABIWFK" varchar(1) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWFK"),
  UNIQUE KEY "AAABIWFL" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWFN" (
  "AAABIWFT" varchar(1) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWFT"),
  UNIQUE KEY "AAABIWFU" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWFW" (
  "AAABIWGC" varchar(10) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWGC"),
  UNIQUE KEY "AAABIWGD" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWGF" (
  "AAABIWGL" varchar(30) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWGL"),
  UNIQUE KEY "AAABIWGM" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWGO" (
  "AAABIWGU" varchar(6) NOT NULL default '000000',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWGU"),
  UNIQUE KEY "AAABIWGV" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWGX" (
  "AAABIWHD" varchar(2) NOT NULL default '00',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWHD"),
  UNIQUE KEY "AAABIWHE" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWHG" (
  "AAABIWHM" varchar(1) NOT NULL default '0',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWHM"),
  UNIQUE KEY "AAABIWHN" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWHP" (
  "AAABIWHV" varchar(5) NOT NULL default '00000',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWHV"),
  UNIQUE KEY "AAABIWHW" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWHY" (
  "AAABIWIE" varchar(6) NOT NULL default '000000',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWIE"),
  UNIQUE KEY "AAABIWIF" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWIH" (
  "AAABIWIN" varchar(4) NOT NULL default '0000',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWIN"),
  UNIQUE KEY "AAABIWIO" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWIQ" (
  "AAABIWIW" varchar(14) NOT NULL default '00000000000000',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWIW"),
  UNIQUE KEY "AAABIWIX" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWIZ" (
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPQY"),
  UNIQUE KEY "AAABIWJG" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWJI" (
  "AAABIWJO" varchar(8) NOT NULL default '00000000',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWJO"),
  UNIQUE KEY "AAABIWJP" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWJR" (
  "AAABIWLB" varchar(2) NOT NULL default ' ',
  "AAABIWJZ" varchar(7) NOT NULL default '0000000',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWLB","AAABIWJZ"),
  UNIQUE KEY "AAABIWKA" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWKC" (
  "AAABIWKI" varchar(3) NOT NULL default '000',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWKI"),
  UNIQUE KEY "AAABIWKJ" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWKL" (
  "AAABIWLB" varchar(2) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWLB"),
  UNIQUE KEY "AAABIWKS" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWKU" (
  "AAABIWLB" varchar(2) NOT NULL default ' ',
  "AAABIWLC" varchar(4) NOT NULL default '0000',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWLB","AAABIWLC"),
  UNIQUE KEY "AAABIWLD" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWLF" (
  "AAABIWLL" varchar(1) NOT NULL default '0',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWLL"),
  UNIQUE KEY "AAABIWLM" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWLO" (
  "AAABIWLU" varchar(30) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWLU"),
  UNIQUE KEY "AAABIWLV" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWLX" (
  "AAABIWMD" varchar(25) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWMD"),
  UNIQUE KEY "AAABIWME" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWMG" (
  "AAABIWMM" varchar(32) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWMM"),
  UNIQUE KEY "AAABIWMN" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWMP" (
  "AAABIWTI" varchar(3) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWTI"),
  UNIQUE KEY "AAABIWMW" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWMY" (
  "AAABIWNE" varchar(30) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWNE"),
  UNIQUE KEY "AAABIWNF" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWNH" (
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWNN"),
  UNIQUE KEY "AAABIWNO" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWNQ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA"),
  UNIQUE KEY "AAABIWNX" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWNZ" (
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQDU"),
  UNIQUE KEY "AAABIWOG" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWOI" (
  "AAABIWOO" varchar(1) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWOO"),
  UNIQUE KEY "AAABIWOP" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWOR" (
  "AAABIWOX" varchar(1) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWOX"),
  UNIQUE KEY "AAABIWOY" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWPA" (
  "AAABIWPG" varchar(4) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWPG"),
  UNIQUE KEY "AAABIWPH" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWPJ" (
  "AAABIWPP" varchar(1) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWPP"),
  UNIQUE KEY "AAABIWPQ" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWPS" (
  "AAABIWPY" varchar(1) NOT NULL default '0',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWPY"),
  UNIQUE KEY "AAABIWPZ" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWQB" (
  "AAABIWQH" varchar(30) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWQH"),
  UNIQUE KEY "AAABIWQI" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWQK" (
  "AAABIWQQ" varchar(1) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWQQ"),
  UNIQUE KEY "AAABIWQR" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWQT" (
  "AAABIWQZ" varchar(2) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWQZ"),
  UNIQUE KEY "AAABIWRA" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWRC" (
  "AAABLTIZ" varchar(6) NOT NULL default '000000',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTIZ"),
  UNIQUE KEY "AAABIWRJ" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWRL" (
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWTP"),
  UNIQUE KEY "AAABIWRS" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWRU" (
  "AAABIWTO" varchar(40) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWTO"),
  UNIQUE KEY "AAABIWSB" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWSD" (
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWTN"),
  UNIQUE KEY "AAABIWSK" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWSM" (
  "AAABMKAI" varchar(3) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKAI"),
  UNIQUE KEY "AAABIWST" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWSV" (
  "AAABIWTQ" varchar(1) NOT NULL default '0',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWTQ"),
  UNIQUE KEY "AAABIWTC" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWTE" (
  "AAABIWTI" varchar(3) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWTI","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWTK" (
  "AAABIWTQ" varchar(1) NOT NULL default '0',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTO" varchar(40) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWTQ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWTS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIWUJ" varchar(32) NOT NULL default ' ',
  "AAABIWUK" varchar(32) NOT NULL default ' ',
  "AAABIWUL" varchar(32) NOT NULL default ' ',
  "AAABIWUM" varchar(1) NOT NULL default ' ',
  "AAABIWUN" varchar(32) NOT NULL default ' ',
  "AAABIWTZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIWUJ","AAABIWUK","AAABIWUL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWUE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIWUP" varchar(32) NOT NULL default ' ',
  "AAABIWUH" varchar(32) NOT NULL default ' ',
  "AAABIWUI" varchar(32) NOT NULL default ' ',
  "AAABIWUJ" varchar(32) NOT NULL default ' ',
  "AAABIWUK" varchar(32) NOT NULL default ' ',
  "AAABIWUL" varchar(32) NOT NULL default ' ',
  "AAABIWUM" varchar(1) NOT NULL default ' ',
  "AAABIWUN" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIWUP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWUQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABIWUT" varchar(14) NOT NULL default ' ',
  "AAABIWUU" varchar(1) NOT NULL default ' ',
  "AAABIWUV" varchar(4) NOT NULL default '0000',
  "AAABIWUW" varchar(4) NOT NULL default '0000',
  "AAABIWUX" varchar(4) NOT NULL default '0000',
  "AAABIWUY" varchar(4) NOT NULL default '0000',
  "AAABIWUZ" varchar(4) NOT NULL default '0000',
  "AAABIWVA" varchar(4) NOT NULL default '0000',
  "AAABIWVB" varchar(4) NOT NULL default '0000',
  "AAABIWVC" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWVF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZMY" varchar(10) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABJEHS" varchar(30) NOT NULL default ' ',
  "AAABMAKT" varchar(10) NOT NULL default ' ',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIWVY" varchar(16) NOT NULL default '0000000000000000',
  "AAABJEHT" varchar(30) NOT NULL default ' ',
  "AAABIWVP" varchar(30) NOT NULL default ' ',
  "AAABIWVQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZMY","AAABMQIJ","AAABJEHS","AAABMAKT","AAABMALG","AAABMRHS","AAABIWVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWVZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZFC" varchar(4) NOT NULL default ' ',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABIWWJ" varchar(30) NOT NULL default ' ',
  "AAABLZNN" varchar(10) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZFC","AAABLZFD","AAABIWWJ","AAABLZNN","AAABLZNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWWM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJAJX" varchar(2) NOT NULL default ' ',
  "AAABJATS" varchar(4) NOT NULL default ' ',
  "AAABIXRJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJAJX","AAABJATS","AAABIXRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWWV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(32) NOT NULL default ' ',
  "AAABJACA" varchar(6) NOT NULL default '000000',
  "AAABLZCP" varchar(35) NOT NULL default ' ',
  "AAABLZCR" varchar(35) NOT NULL default ' ',
  "AAABJSTP" varchar(35) NOT NULL default ' ',
  "AAABIWXC" varchar(35) NOT NULL default ' ',
  "AAABIWXD" varchar(10) NOT NULL default ' ',
  "AAABIYJJ" varchar(3) NOT NULL default ' ',
  "AAABJXDX" varchar(35) NOT NULL default ' ',
  "AAABIYJL" varchar(16) NOT NULL default ' ',
  "AAABIWXH" varchar(31) NOT NULL default ' ',
  "AAABMECK" varchar(15) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABIWXK" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABIXQG" varchar(1) NOT NULL default ' ',
  "AAABIWXN" varchar(16) NOT NULL default ' ',
  "AAABIWXO" varchar(11) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJADN","AAABJACA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWXS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKNCK" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKNCK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWXX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKNCK" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKNCK","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWYF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(32) NOT NULL default ' ',
  "AAABJACA" varchar(6) NOT NULL default '000000',
  "AAABIXVR" varchar(6) NOT NULL default '000000',
  "AAABJGGL" varchar(35) NOT NULL default ' ',
  "AAABIWYL" varchar(2) NOT NULL default ' ',
  "AAABIXVF" varchar(1) NOT NULL default ' ',
  "AAABKGMO" decimal(13,2) NOT NULL default '0.00',
  "AAABJABF" varchar(5) NOT NULL default ' ',
  "AAABIWYP" varchar(10) NOT NULL default ' ',
  "AAABIWYQ" varchar(10) NOT NULL default ' ',
  "AAABKGOS" varchar(4) NOT NULL default '0000',
  "AAABJABE" varchar(4) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABIXQG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJADN","AAABJACA","AAABIXVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWYZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(32) NOT NULL default ' ',
  "AAABIXXI" varchar(6) NOT NULL default '000000',
  "AAABIXAW" varchar(6) NOT NULL default '000000',
  "AAABKGMO" decimal(13,2) NOT NULL default '0.00',
  "AAABJABF" varchar(5) NOT NULL default ' ',
  "AAABJABG" varchar(2) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABIXQG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJADN","AAABIXXI","AAABIXAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIWZN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(32) NOT NULL default ' ',
  "AAABIXXI" varchar(6) NOT NULL default '000000',
  "AAABLSCQ" varchar(1) NOT NULL default ' ',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAABIXVX" varchar(30) NOT NULL default ' ',
  "AAABIWZU" decimal(15,0) NOT NULL default '0',
  "AAABIWZV" varchar(6) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABIXQG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJADN","AAABIXXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXAB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(32) NOT NULL default ' ',
  "AAABIXXI" varchar(6) NOT NULL default '000000',
  "AAABIXAW" varchar(6) NOT NULL default '000000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAABIXAI" decimal(13,3) NOT NULL default '0.000',
  "AAABIXAJ" decimal(13,3) NOT NULL default '0.000',
  "AAABJAEX" decimal(13,3) NOT NULL default '0.000',
  "AAABLEAO" varchar(3) NOT NULL default ' ',
  "AAABKNCK" varchar(2) NOT NULL default ' ',
  "AAABIXWN" varchar(1) NOT NULL default ' ',
  "AAABJAFI" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABIXQG" varchar(1) NOT NULL default ' ',
  "AAABMCEY" varchar(10) NOT NULL default ' ',
  "AAABMCVW" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJADN","AAABIXXI","AAABIXAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXAX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMMRQ" varchar(4) NOT NULL default ' ',
  "AAABLCDI" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABIYLH" varchar(1) NOT NULL default ' ',
  "AAABIYLI" varchar(1) NOT NULL default ' ',
  "AAABIYLJ" varchar(1) NOT NULL default ' ',
  "AAABIXBI" varchar(1) NOT NULL default ' ',
  "AAABIXBJ" varchar(1) NOT NULL default ' ',
  "AAABIXBK" varchar(1) NOT NULL default ' ',
  "AAABIXBL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMMRQ","AAABLCDI","AAABMQII","AAABMJXV","AAABMJXW","AAABMPLT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXBT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(32) NOT NULL default ' ',
  "AAABIXCE" varchar(6) NOT NULL default '000000',
  "AAABIXRJ" varchar(2) NOT NULL default ' ',
  "AAABIXBY" varchar(1) NOT NULL default ' ',
  "AAABIXBZ" varchar(1) NOT NULL default ' ',
  "AAABIXCA" varchar(1) NOT NULL default ' ',
  "AAABIXQG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJADN","AAABIXCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXCF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(32) NOT NULL default ' ',
  "AAABJACA" varchar(6) NOT NULL default '000000',
  "AAABIXCJ" varchar(10) NOT NULL default ' ',
  "AAABJWRA" varchar(1) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABIXQW" varchar(2) NOT NULL default ' ',
  "AAABIXCP" varchar(2) NOT NULL default ' ',
  "AAABIZZP" varchar(2) NOT NULL default ' ',
  "AAABIXJT" varchar(8) NOT NULL default '00000000',
  "AAABIXJU" varchar(6) NOT NULL default '000000',
  "AAABIXJV" varchar(12) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABIXQG" varchar(1) NOT NULL default ' ',
  "AAABIXCW" varchar(1) NOT NULL default ' ',
  "AAABIXCX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJADN","AAABJACA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXDB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(32) NOT NULL default ' ',
  "AAABJACA" varchar(6) NOT NULL default '000000',
  "AAABIXLQ" varchar(35) NOT NULL default ' ',
  "AAABIXFK" varchar(6) NOT NULL default '000000',
  "AAABIXKR" varchar(4) NOT NULL default ' ',
  "AAABKGMO" decimal(13,2) NOT NULL default '0.00',
  "AAABJABF" varchar(5) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABIXQG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJADN","AAABJACA","AAABIXLQ","AAABIXFK","AAABIXKR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXDS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(32) NOT NULL default ' ',
  "AAABJACA" varchar(6) NOT NULL default '000000',
  "AAABIXLQ" varchar(35) NOT NULL default ' ',
  "AAABIXDX" varchar(10) NOT NULL default ' ',
  "AAABJADR" varchar(2) NOT NULL default ' ',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAABIXEA" decimal(15,0) NOT NULL default '0',
  "AAABLYTF" varchar(6) NOT NULL default ' ',
  "AAABIZZY" varchar(1) NOT NULL default '0',
  "AAABKNCK" varchar(2) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABIXQG" varchar(1) NOT NULL default ' ',
  "AAABLHXT" varchar(35) NOT NULL default ' ',
  "AAABLXIV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLUXQ","AAABJADN","AAABJACA","AAABIXLQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXEM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(32) NOT NULL default ' ',
  "AAABJACA" varchar(6) NOT NULL default '000000',
  "AAABIXLQ" varchar(35) NOT NULL default ' ',
  "AAABIXFK" varchar(6) NOT NULL default '000000',
  "AAABIXES" varchar(6) NOT NULL default '000000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAABIXEV" decimal(13,3) NOT NULL default '0.000',
  "AAABLEAO" varchar(3) NOT NULL default ' ',
  "AAABIXEX" varchar(1) NOT NULL default ' ',
  "AAABIZZY" varchar(1) NOT NULL default '0',
  "AAABKNCK" varchar(2) NOT NULL default ' ',
  "AAABJAFI" varchar(1) NOT NULL default ' ',
  "AAABIXFB" varchar(8) NOT NULL default '00000000',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABIXQG" varchar(1) NOT NULL default ' ',
  "AAABMCEY" varchar(10) NOT NULL default ' ',
  "AAABMCVW" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJADN","AAABJACA","AAABIXLQ","AAABIXFK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXFL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIXGF" varchar(2) NOT NULL default ' ',
  "AAABIXFO" varchar(1) NOT NULL default ' ',
  "AAABIXPK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIXGF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXFS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIXGF" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIXGF","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXGA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(32) NOT NULL default ' ',
  "AAABJACA" varchar(6) NOT NULL default '000000',
  "AAABIXGO" varchar(6) NOT NULL default '000000',
  "AAABIXGF" varchar(2) NOT NULL default ' ',
  "AAABIZIH" decimal(10,3) NOT NULL default '0.000',
  "AAABIZII" decimal(10,3) NOT NULL default '0.000',
  "AAABIZMW" varchar(3) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABIXQG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJADN","AAABJACA","AAABIXGO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXGP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(32) NOT NULL default ' ',
  "AAABJACA" varchar(6) NOT NULL default '000000',
  "AAABIZHA" varchar(6) NOT NULL default '000000',
  "AAABJABH" varchar(1) NOT NULL default ' ',
  "AAABKGMO" decimal(13,2) NOT NULL default '0.00',
  "AAABJABF" varchar(5) NOT NULL default ' ',
  "AAABJABG" varchar(2) NOT NULL default ' ',
  "AAABIZGV" varchar(40) NOT NULL default ' ',
  "AAABIXGZ" varchar(25) NOT NULL default ' ',
  "AAABIXHA" varchar(4) NOT NULL default ' ',
  "AAABIXHB" varchar(8) NOT NULL default '00000000',
  "AAABIXHC" varchar(13) NOT NULL default ' ',
  "AAABIXHD" varchar(15) NOT NULL default ' ',
  "AAABIXHE" varchar(16) NOT NULL default ' ',
  "AAABIXHF" varchar(10) NOT NULL default ' ',
  "AAABKGOS" varchar(4) NOT NULL default '0000',
  "AAABJABE" varchar(4) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABIXQG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJADN","AAABJACA","AAABIZHA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXHO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(32) NOT NULL default ' ',
  "AAABJACA" varchar(6) NOT NULL default '000000',
  "AAABIZHA" varchar(6) NOT NULL default '000000',
  "AAABIXVR" varchar(6) NOT NULL default '000000',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABIXQG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJADN","AAABJACA","AAABIZHA","AAABIXVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXIB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(32) NOT NULL default ' ',
  "AAABIXIP" varchar(2) NOT NULL default ' ',
  "AAABJACA" varchar(6) NOT NULL default '000000',
  "AAABIXIR" varchar(35) NOT NULL default ' ',
  "AAABIXIS" varchar(6) NOT NULL default '000000',
  "AAABIXIT" varchar(6) NOT NULL default '000000',
  "AAABIXKR" varchar(4) NOT NULL default ' ',
  "AAABIXIV" varchar(10) NOT NULL default ' ',
  "AAABIXIW" varchar(16) NOT NULL default ' ',
  "AAABIXIM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJADN","AAABIXIP","AAABJACA","AAABIXIR","AAABIXIS","AAABIXIT","AAABIXKR","AAABIXIV","AAABIXIW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXIX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(32) NOT NULL default ' ',
  "AAABJADR" varchar(2) NOT NULL default ' ',
  "AAABLTWR" varchar(10) NOT NULL default ' ',
  "AAABIXJC" varchar(18) NOT NULL default ' ',
  "AAABIXJD" varchar(18) NOT NULL default ' ',
  "AAABJACE" varchar(10) NOT NULL default ' ',
  "AAABIXJF" varchar(10) NOT NULL default ' ',
  "AAABJACF" varchar(10) NOT NULL default ' ',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAABLUMF" varchar(1) NOT NULL default ' ',
  "AAABIXJJ" varchar(1) NOT NULL default ' ',
  "AAABIXJK" varchar(2) NOT NULL default ' ',
  "AAABIXJL" varchar(1) NOT NULL default ' ',
  "AAABIXJM" varchar(6) NOT NULL default '000000',
  "AAABIXJN" varchar(6) NOT NULL default '000000',
  "AAABIXJO" varchar(1) NOT NULL default ' ',
  "AAABLSDL" varchar(1) NOT NULL default ' ',
  "AAABJAEL" varchar(8) NOT NULL default '00000000',
  "AAABLOCX" varchar(6) NOT NULL default '000000',
  "AAABJAEN" varchar(12) NOT NULL default ' ',
  "AAABIXJT" varchar(8) NOT NULL default '00000000',
  "AAABIXJU" varchar(6) NOT NULL default '000000',
  "AAABIXJV" varchar(12) NOT NULL default ' ',
  "AAABIXQG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJADN"),
  KEY "AAABIXJZ" ("AAABLUXQ","AAABJADR","AAABLTWR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXKD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(32) NOT NULL default ' ',
  "AAABJACA" varchar(6) NOT NULL default '000000',
  "AAABIXLW" varchar(35) NOT NULL default ' ',
  "AAABIXKR" varchar(4) NOT NULL default ' ',
  "AAABKGMO" decimal(13,2) NOT NULL default '0.00',
  "AAABJABF" varchar(5) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABIXQG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJADN","AAABJACA","AAABIXLW","AAABIXKR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXKS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(32) NOT NULL default ' ',
  "AAABJACA" varchar(6) NOT NULL default '000000',
  "AAABIXLW" varchar(35) NOT NULL default ' ',
  "AAABIXMN" varchar(10) NOT NULL default ' ',
  "AAABLTIZ" decimal(15,0) NOT NULL default '0',
  "AAABIXMF" varchar(6) NOT NULL default ' ',
  "AAABIXMG" decimal(15,0) NOT NULL default '0',
  "AAABIXMH" varchar(6) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABIXQG" varchar(1) NOT NULL default ' ',
  "AAABIXLE" varchar(10) NOT NULL default ' ',
  "AAABIXLF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJADN","AAABJACA","AAABIXLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXLK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(32) NOT NULL default ' ',
  "AAABJACA" varchar(6) NOT NULL default '000000',
  "AAABIXLW" varchar(35) NOT NULL default ' ',
  "AAABIXLX" varchar(6) NOT NULL default '000000',
  "AAABIXLQ" varchar(35) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABIXQG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJADN","AAABJACA","AAABIXLW","AAABIXLX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXLY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(32) NOT NULL default ' ',
  "AAABJACA" varchar(6) NOT NULL default '000000',
  "AAABIXMN" varchar(10) NOT NULL default ' ',
  "AAABIXMD" varchar(1) NOT NULL default ' ',
  "AAABLTIZ" decimal(15,0) NOT NULL default '0',
  "AAABIXMF" varchar(6) NOT NULL default ' ',
  "AAABIXMG" decimal(15,0) NOT NULL default '0',
  "AAABIXMH" varchar(6) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABIXQG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJADN","AAABJACA","AAABIXMN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXMO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(32) NOT NULL default ' ',
  "AAABIXMR" VARBINARY(255),
  PRIMARY KEY ("AAABLUXQ","AAABJADN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXMU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMMRQ" varchar(4) NOT NULL default ' ',
  "AAABLCDI" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMRCU" varchar(6) NOT NULL default '000000',
  "AAABIXND" varchar(15) NOT NULL default ' ',
  "AAABIXNE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMMRQ","AAABLCDI","AAABMQII","AAABMJXV","AAABMJXW","AAABMPLT","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXNN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJABG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJABG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXNS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJABG" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJABG","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXOA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMMRQ" varchar(4) NOT NULL default ' ',
  "AAABLCDI" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMRCU" varchar(6) NOT NULL default '000000',
  "AAABJABG" varchar(2) NOT NULL default ' ',
  "AAABIXOK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMMRQ","AAABLCDI","AAABMQII","AAABMJXV","AAABMJXW","AAABMPLT","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXOT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKNCK" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKNCK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXOY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKNCK" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKNCK","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXPG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIXQA" varchar(2) NOT NULL default ' ',
  "AAABIXPJ" varchar(1) NOT NULL default ' ',
  "AAABIXPK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIXQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXPN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIXQA" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIXQA","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXPV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(32) NOT NULL default ' ',
  "AAABJACA" varchar(6) NOT NULL default '000000',
  "AAABIXQK" varchar(6) NOT NULL default '000000',
  "AAABIXQA" varchar(2) NOT NULL default ' ',
  "AAABIZLV" decimal(15,0) NOT NULL default '0',
  "AAABIXQC" varchar(6) NOT NULL default ' ',
  "AAABLTLN" decimal(15,0) NOT NULL default '0',
  "AAABIXQE" varchar(6) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABIXQG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJADN","AAABJACA","AAABIXQK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXQL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIXQW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIXQW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXQQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIXQW" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIXQW","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXQY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIXRJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIXRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXRD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIXRJ" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIXRJ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXRL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMKFH" varchar(20) NOT NULL default ' ',
  "AAABLRFA" varchar(20) NOT NULL default ' ',
  "AAABIXRP" varchar(1) NOT NULL default ' ',
  "AAABIXRQ" varchar(1) NOT NULL default ' ',
  "AAABIXRR" varchar(1) NOT NULL default ' ',
  "AAABIXRS" varchar(1) NOT NULL default ' ',
  "AAABIXRT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMKFH","AAABLRFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXRX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMKFH" varchar(20) NOT NULL default ' ',
  "AAABIXSA" varchar(1) NOT NULL default ' ',
  "AAABIXSB" varchar(6) NOT NULL default '000000',
  "AAABIXSC" varchar(6) NOT NULL default '000000',
  "AAABIXSD" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXSG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZFC" varchar(4) NOT NULL default ' ',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABJEHS" varchar(30) NOT NULL default ' ',
  "AAABJEHT" varchar(30) NOT NULL default ' ',
  "AAABJAQI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZFC","AAABLZFD","AAABJEHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXSR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZMY" varchar(10) NOT NULL default ' ',
  "AAABIXSZ" varchar(3) NOT NULL default '000',
  "AAABMRCU" varchar(6) NOT NULL default '000000',
  "AAABMDBA" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZMY","AAABIXSZ","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXTB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABLZFC" varchar(4) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLZFD","AAABLZFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXTJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZFC" varchar(4) NOT NULL default ' ',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABKYJI" varchar(10) NOT NULL default ' ',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABIXTR" varchar(4) NOT NULL default ' ',
  "AAABIXTS" varchar(4) NOT NULL default ' ',
  "AAABIXTT" varchar(4) NOT NULL default ' ',
  "AAABIXTU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZFC","AAABLZFD","AAABKYJI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXTZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIXUE" varchar(2) NOT NULL default '00',
  "AAABIXUC" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIXUE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXUF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIZKA" varchar(2) NOT NULL default '00',
  "AAABIZJY" varchar(12) NOT NULL default ' ',
  "AAABIZJZ" varchar(10) NOT NULL default ' ',
  "AAABIXUK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIZKA","AAABIZJY","AAABIZJZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXUP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIYDN" varchar(32) NOT NULL default ' ',
  "AAABJADN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIYDN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXUV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(10) NOT NULL default ' ',
  "AAABJACA" varchar(6) NOT NULL default '000000',
  "AAABIZMG" varchar(6) NOT NULL default '000000',
  "AAABIZHA" varchar(6) NOT NULL default '000000',
  "AAABIXVR" varchar(6) NOT NULL default '000000',
  "AAABIZMN" varchar(10) NOT NULL default ' ',
  "AAABIZBU" varchar(10) NOT NULL default ' ',
  "AAABLPNX" varchar(2) NOT NULL default ' ',
  "AAABIXVF" varchar(1) NOT NULL default ' ',
  "AAABIXVG" decimal(23,4) NOT NULL default '0.0000',
  "AAABIZDZ" varchar(30) NOT NULL default ' ',
  "AAABIZEA" varchar(60) NOT NULL default ' ',
  "AAABIZEB" varchar(60) NOT NULL default ' ',
  "AAABIZEC" varchar(60) NOT NULL default ' ',
  "AAABIZED" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJADN","AAABJACA","AAABIZMG","AAABIZHA","AAABIXVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXVS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(10) NOT NULL default ' ',
  "AAABIXXI" varchar(2) NOT NULL default '00',
  "AAABIXVW" varchar(1) NOT NULL default ' ',
  "AAABIXVX" varchar(30) NOT NULL default ' ',
  "AAABIXVY" varchar(8) NOT NULL default '00000000',
  "AAABJTGK" varchar(6) NOT NULL default '000000',
  "AAABIXWA" varchar(1) NOT NULL default ' ',
  "AAABIZMZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJADN","AAABIXXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXWF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(10) NOT NULL default ' ',
  "AAABIXXI" varchar(2) NOT NULL default '00',
  "AAABIZEH" varchar(6) NOT NULL default '000000',
  "AAABIYRB" varchar(1) NOT NULL default ' ',
  "AAABIZDL" varchar(18) NOT NULL default ' ',
  "AAABLEAO" varchar(3) NOT NULL default ' ',
  "AAABIXWN" varchar(1) NOT NULL default ' ',
  "AAABIXWO" decimal(13,3) NOT NULL default '0.000',
  "AAABIXWP" decimal(13,3) NOT NULL default '0.000',
  "AAABIXXF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJADN","AAABIXXI","AAABIZEH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXWV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(10) NOT NULL default ' ',
  "AAABIXXI" varchar(2) NOT NULL default '00',
  "AAABIZEH" varchar(6) NOT NULL default '000000',
  "AAABIYRB" varchar(1) NOT NULL default ' ',
  "AAABIZGJ" varchar(2) NOT NULL default ' ',
  "AAABLPQY" varchar(3) NOT NULL default ' ',
  "AAABIXXD" decimal(23,4) NOT NULL default '0.0000',
  "AAABIXXE" decimal(23,4) NOT NULL default '0.0000',
  "AAABIXXF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJADN","AAABIXXI","AAABIZEH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXXK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAABMAKT" varchar(10) NOT NULL default ' ',
  "AAABMECK" varchar(4) NOT NULL default ' ',
  "AAABIYEN" varchar(35) NOT NULL default ' ',
  "AAABIYEO" varchar(35) NOT NULL default ' ',
  "AAABIXXS" varchar(10) NOT NULL default ' ',
  "AAABIXXT" varchar(10) NOT NULL default ' ',
  "AAABIXXU" varchar(40) NOT NULL default ' ',
  "AAABIXXV" varchar(40) NOT NULL default ' ',
  "AAABIXXW" varchar(40) NOT NULL default ' ',
  "AAABLKIV" varchar(40) NOT NULL default ' ',
  "AAABIXXY" varchar(20) NOT NULL default ' ',
  "AAABIXXZ" varchar(10) NOT NULL default ' ',
  "AAABIXYA" varchar(10) NOT NULL default ' ',
  "AAABJXDX" varchar(35) NOT NULL default ' ',
  "AAABJSTP" varchar(35) NOT NULL default ' ',
  "AAABIYJI" varchar(35) NOT NULL default ' ',
  "AAABIYJJ" varchar(3) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIXYH" varchar(30) NOT NULL default ' ',
  "AAABIXYI" varchar(10) NOT NULL default ' ',
  "AAABIXYJ" varchar(30) NOT NULL default ' ',
  "AAABIXYK" varchar(10) NOT NULL default ' ',
  "AAABIZDZ" varchar(30) NOT NULL default ' ',
  "AAABIZEA" varchar(60) NOT NULL default ' ',
  "AAABIZEB" varchar(60) NOT NULL default ' ',
  "AAABIZEC" varchar(60) NOT NULL default ' ',
  "AAABIZED" varchar(60) NOT NULL default ' ',
  "AAABIZEE" varchar(60) NOT NULL default ' ',
  "AAABIXYR" varchar(60) NOT NULL default ' ',
  "AAABIXYS" varchar(60) NOT NULL default ' ',
  "AAABIXYT" varchar(60) NOT NULL default ' ',
  "AAABIXYU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPLT","AAABMALG","AAABMAKT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXYZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIYBL" varchar(5) NOT NULL default '00000',
  "AAABLDNU" varchar(32) NOT NULL default ' ',
  "AAABLPSU" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIYBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXZG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIZJZ" varchar(10) NOT NULL default ' ',
  "AAABIZKA" varchar(2) NOT NULL default '00',
  "AAABIYDN" varchar(32) NOT NULL default ' ',
  "AAABIZJY" varchar(12) NOT NULL default ' ',
  "AAABLYKK" varchar(10) NOT NULL default ' ',
  "AAABLZCS" varchar(10) NOT NULL default ' ',
  "AAABKNQM" varchar(32) NOT NULL default ' ',
  "AAABIYDX" varchar(20) NOT NULL default ' ',
  "AAABIXZQ" varchar(30) NOT NULL default ' ',
  "AAABIXZR" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIZJZ","AAABIZKA","AAABIYDN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIXZW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIYAL" varchar(5) NOT NULL default '00000',
  "AAABIYBE" varchar(15) NOT NULL default ' ',
  "AAABIYBF" varchar(40) NOT NULL default ' ',
  "AAABIYBG" varchar(25) NOT NULL default ' ',
  "AAABIYBH" varchar(25) NOT NULL default ' ',
  "AAABIYBI" varchar(1) NOT NULL default ' ',
  "AAABLUZL" varchar(20) NOT NULL default ' ',
  "AAABIYBK" varchar(132) NOT NULL default ' ',
  "AAABIYBL" varchar(5) NOT NULL default '00000',
  "AAABIYDN" varchar(32) NOT NULL default ' ',
  "AAABLXFQ" varchar(1) NOT NULL default ' ',
  "AAABIYBO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIYAL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYAM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIYEJ" varchar(5) NOT NULL default '00000',
  "AAABIYBE" varchar(15) NOT NULL default ' ',
  "AAABIYBF" varchar(40) NOT NULL default ' ',
  "AAABIYBG" varchar(25) NOT NULL default ' ',
  "AAABIYBH" varchar(25) NOT NULL default ' ',
  "AAABIYBI" varchar(1) NOT NULL default ' ',
  "AAABLUZL" varchar(20) NOT NULL default ' ',
  "AAABIYBK" varchar(132) NOT NULL default ' ',
  "AAABIYBL" varchar(5) NOT NULL default '00000',
  "AAABLXFQ" varchar(1) NOT NULL default ' ',
  "AAABIYBO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIYEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYBB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIYBQ" varchar(5) NOT NULL default '00000',
  "AAABIYBE" varchar(15) NOT NULL default ' ',
  "AAABIYBF" varchar(40) NOT NULL default ' ',
  "AAABIYBG" varchar(25) NOT NULL default ' ',
  "AAABIYBH" varchar(25) NOT NULL default ' ',
  "AAABIYBI" varchar(1) NOT NULL default ' ',
  "AAABLUZL" varchar(20) NOT NULL default ' ',
  "AAABIYBK" varchar(132) NOT NULL default ' ',
  "AAABIYBL" varchar(5) NOT NULL default '00000',
  "AAABIYDX" varchar(20) NOT NULL default ' ',
  "AAABLXFQ" varchar(1) NOT NULL default ' ',
  "AAABIYBO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIYBQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYBR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIYDX" varchar(20) NOT NULL default ' ',
  "AAABLPSU" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIYDX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYBX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIZKA" varchar(2) NOT NULL default '00',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABIYCB" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIZKA","AAABMNJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYCF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIZKA" varchar(2) NOT NULL default '00',
  "AAABIYCI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIZKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYCL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIYDN" varchar(32) NOT NULL default ' ',
  "AAABIYCS" varchar(15) NOT NULL default ' ',
  "AAABKCAK" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABIYDN","AAABIYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYCT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIYEJ" varchar(5) NOT NULL default '00000',
  "AAABLTXF" varchar(2) NOT NULL default '00',
  "AAABJSDF" varchar(2) NOT NULL default '00',
  "AAABIYCY" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABJETX" varchar(20) NOT NULL default ' ',
  "AAABIYDC" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABIYEJ","AAABLTXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYDG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIYDN" varchar(32) NOT NULL default ' ',
  "AAABIYEJ" varchar(5) NOT NULL default '00000',
  "AAABMRFO" varchar(40) NOT NULL default ' ',
  "AAABIZMZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIYDN","AAABIYEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYDP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIYDW" varchar(20) NOT NULL default ' ',
  "AAABIYDX" varchar(20) NOT NULL default ' ',
  "AAABIYDT" varchar(132) NOT NULL default ' ',
  "AAABLEKT" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIYDW","AAABIYDX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYDY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYCI" varchar(2) NOT NULL default ' ',
  "AAABLBFZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYCI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYEE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIYEJ" varchar(5) NOT NULL default '00000',
  "AAABLYCI" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIYEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYEK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYKK" varchar(10) NOT NULL default ' ',
  "AAABIYEN" varchar(35) NOT NULL default ' ',
  "AAABIYEO" varchar(105) NOT NULL default ' ',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABLYMA" varchar(1) NOT NULL default ' ',
  "AAABLYNA" varchar(3) NOT NULL default ' ',
  "AAABLYJX" varchar(1) NOT NULL default ' ',
  "AAABLYJY" varchar(1) NOT NULL default ' ',
  "AAABLYMB" varchar(8) NOT NULL default ' ',
  "AAABLYMC" varchar(8) NOT NULL default ' ',
  "AAABLYMD" varchar(8) NOT NULL default ' ',
  "AAABLYME" varchar(8) NOT NULL default ' ',
  "AAABLYMF" varchar(8) NOT NULL default ' ',
  "AAABLYMG" varchar(8) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default '0',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKM" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABIYGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYKK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYFI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZCS" varchar(10) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(30) NOT NULL default ' ',
  "AAABLYNZ" varchar(1) NOT NULL default ' ',
  "AAABLYLY" varchar(5) NOT NULL default ' ',
  "AAABLYLZ" varchar(1) NOT NULL default ' ',
  "AAABLYMA" varchar(1) NOT NULL default ' ',
  "AAABLYMB" varchar(8) NOT NULL default ' ',
  "AAABLYMC" varchar(8) NOT NULL default ' ',
  "AAABLYMD" varchar(8) NOT NULL default ' ',
  "AAABLYME" varchar(8) NOT NULL default ' ',
  "AAABLYMF" varchar(8) NOT NULL default ' ',
  "AAABLYMG" varchar(8) NOT NULL default ' ',
  "AAABLYMH" varchar(15) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABLYMJ" varchar(30) NOT NULL default ' ',
  "AAABLYNA" varchar(3) NOT NULL default ' ',
  "AAABLYNB" decimal(4,0) NOT NULL default '0',
  "AAABLYNC" varchar(8) NOT NULL default '00000000',
  "AAABLYND" varchar(15) NOT NULL default ' ',
  "AAABLYNE" varchar(15) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default '0',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKM" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABIYGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYGQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIZMN" varchar(10) NOT NULL default ' ',
  "AAABIYKH" varchar(4) NOT NULL default ' ',
  "AAABIYKM" decimal(23,4) NOT NULL default '0.0000',
  "AAABIYGV" varchar(1) NOT NULL default ' ',
  "AAABIZKE" decimal(15,0) NOT NULL default '0',
  "AAABIYGX" decimal(6,0) NOT NULL default '0',
  "AAABIYKL" decimal(23,4) NOT NULL default '0.0000',
  "AAABIYGZ" decimal(23,4) NOT NULL default '0.0000',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABIZBU" varchar(10) NOT NULL default ' ',
  "AAABIZBA" varchar(8) NOT NULL default '00000000',
  "AAABIZBB" decimal(6,0) NOT NULL default '0',
  "AAABIYHE" decimal(23,4) NOT NULL default '0.0000',
  "AAABLIVH" varchar(1) NOT NULL default ' ',
  "AAABIZBE" varchar(2) NOT NULL default ' ',
  "AAABIZBF" varchar(1) NOT NULL default ' ',
  "AAABIZBG" varchar(3) NOT NULL default ' ',
  "AAABJFWW" varchar(8) NOT NULL default '00000000',
  "AAABIZBJ" varchar(40) NOT NULL default ' ',
  "AAABLOCD" varchar(40) NOT NULL default ' ',
  "AAABIZMP" varchar(1) NOT NULL default ' ',
  "AAABIZBM" varchar(1) NOT NULL default ' ',
  "AAABLKJE" varchar(1) NOT NULL default ' ',
  "AAABIZKF" decimal(21,7) NOT NULL default '0.0000000',
  "AAABIZMZ" varchar(1) NOT NULL default ' ',
  "AAABIZDZ" varchar(30) NOT NULL default ' ',
  "AAABIZEA" varchar(60) NOT NULL default ' ',
  "AAABIZEB" varchar(60) NOT NULL default ' ',
  "AAABIZEC" varchar(60) NOT NULL default ' ',
  "AAABIZED" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIZMN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYHY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(10) NOT NULL default ' ',
  "AAABJACA" varchar(6) NOT NULL default '000000',
  "AAABMLPV" varchar(35) NOT NULL default ' ',
  "AAABLZCR" varchar(35) NOT NULL default ' ',
  "AAABIYJF" varchar(40) NOT NULL default ' ',
  "AAABJXDX" varchar(35) NOT NULL default ' ',
  "AAABJSTP" varchar(35) NOT NULL default ' ',
  "AAABIYJI" varchar(35) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABIYJJ" varchar(3) NOT NULL default ' ',
  "AAABIYJK" varchar(10) NOT NULL default ' ',
  "AAABIYJL" varchar(16) NOT NULL default ' ',
  "AAABIYIM" varchar(31) NOT NULL default ' ',
  "AAABMECK" varchar(15) NOT NULL default ' ',
  "AAABLFZK" varchar(2) NOT NULL default ' ',
  "AAABIZDZ" varchar(30) NOT NULL default ' ',
  "AAABIZEA" varchar(60) NOT NULL default ' ',
  "AAABIZEB" varchar(60) NOT NULL default ' ',
  "AAABIZEC" varchar(60) NOT NULL default ' ',
  "AAABIZED" varchar(60) NOT NULL default ' ',
  "AAABIZMZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJADN","AAABJACA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYIY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIZMN" varchar(10) NOT NULL default ' ',
  "AAABIYJB" varchar(1) NOT NULL default ' ',
  "AAABIZMP" varchar(1) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABMLPV" varchar(35) NOT NULL default ' ',
  "AAABIYJF" varchar(40) NOT NULL default ' ',
  "AAABJXDX" varchar(35) NOT NULL default ' ',
  "AAABJSTP" varchar(35) NOT NULL default ' ',
  "AAABIYJI" varchar(35) NOT NULL default ' ',
  "AAABIYJJ" varchar(3) NOT NULL default ' ',
  "AAABIYJK" varchar(10) NOT NULL default ' ',
  "AAABIYJL" varchar(16) NOT NULL default ' ',
  "AAABIYXW" varchar(4) NOT NULL default ' ',
  "AAABIYXX" varchar(2) NOT NULL default ' ',
  "AAABIYVH" varchar(2) NOT NULL default ' ',
  "AAABIYJP" varchar(2) NOT NULL default ' ',
  "AAABLFZK" varchar(2) NOT NULL default ' ',
  "AAABIYJR" varchar(1) NOT NULL default ' ',
  "AAABIYJS" varchar(1) NOT NULL default ' ',
  "AAABIYJT" varchar(1) NOT NULL default ' ',
  "AAABIYJU" varchar(1) NOT NULL default ' ',
  "AAABIYJV" varchar(1) NOT NULL default ' ',
  "AAABIYJW" varchar(10) NOT NULL default ' ',
  "AAABIYJX" varchar(10) NOT NULL default ' ',
  "AAABIYJY" varchar(10) NOT NULL default ' ',
  "AAABIYJZ" varchar(10) NOT NULL default ' ',
  "AAABIYKA" varchar(1) NOT NULL default ' ',
  "AAABIYKB" varchar(1) NOT NULL default ' ',
  "AAABIYKC" varchar(1) NOT NULL default ' ',
  "AAABIYKD" varchar(1) NOT NULL default ' ',
  "AAABIYKE" varchar(1) NOT NULL default ' ',
  "AAABIYKF" varchar(4) NOT NULL default ' ',
  "AAABIYKG" varchar(40) NOT NULL default ' ',
  "AAABIYKH" varchar(4) NOT NULL default ' ',
  "AAABIYKI" varchar(8) NOT NULL default '00000000',
  "AAABIYKJ" varchar(6) NOT NULL default '000000',
  "AAABIZBB" decimal(6,0) NOT NULL default '0',
  "AAABIYKL" decimal(23,4) NOT NULL default '0.0000',
  "AAABIYKM" decimal(23,4) NOT NULL default '0.0000',
  "AAABIYKN" varchar(3) NOT NULL default ' ',
  "AAABIZGP" varchar(15) NOT NULL default ' ',
  "AAABIYKP" varchar(18) NOT NULL default ' ',
  "AAABIZGM" varchar(4) NOT NULL default ' ',
  "AAABIZGL" varchar(25) NOT NULL default '0000000000000000000000000',
  "AAABIZGN" varchar(8) NOT NULL default '00000000',
  "AAABIYKT" varchar(20) NOT NULL default ' ',
  "AAABIYKU" varchar(15) NOT NULL default ' ',
  "AAABIYKV" varchar(4) NOT NULL default ' ',
  "AAABIYKW" varchar(1) NOT NULL default ' ',
  "AAABIYKX" varchar(4) NOT NULL default ' ',
  "AAABIYKY" varchar(1) NOT NULL default ' ',
  "AAABIYKZ" varchar(4) NOT NULL default ' ',
  "AAABIYLA" varchar(1) NOT NULL default ' ',
  "AAABIYLB" varchar(2) NOT NULL default ' ',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAABIYLD" varchar(3) NOT NULL default ' ',
  "AAABIYLE" varchar(3) NOT NULL default ' ',
  "AAABIYLF" varchar(16) NOT NULL default ' ',
  "AAABIYLG" varchar(11) NOT NULL default ' ',
  "AAABIYLH" varchar(1) NOT NULL default ' ',
  "AAABIYLI" varchar(1) NOT NULL default ' ',
  "AAABIYLJ" varchar(1) NOT NULL default ' ',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAABIYLL" varchar(2) NOT NULL default ' ',
  "AAABIZDZ" varchar(30) NOT NULL default ' ',
  "AAABIZEA" varchar(60) NOT NULL default ' ',
  "AAABIZEB" varchar(60) NOT NULL default ' ',
  "AAABIZEC" varchar(60) NOT NULL default ' ',
  "AAABIZED" varchar(60) NOT NULL default ' ',
  "AAABIZEE" varchar(60) NOT NULL default ' ',
  "AAABLKJE" varchar(1) NOT NULL default ' ',
  "AAABIZKF" decimal(21,7) NOT NULL default '0.0000000',
  "AAABIZMZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIZMN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYLX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIYMT" VARBINARY(255) NOT NULL,
  "AAABIZKA" varchar(2) NOT NULL default '00',
  "AAABIZJY" varchar(12) NOT NULL default ' ',
  "AAABIZJZ" varchar(10) NOT NULL default ' ',
  "AAABIZKD" varchar(10) NOT NULL default ' ',
  "AAABIZKB" varchar(10) NOT NULL default ' ',
  "AAABIZKC" varchar(10) NOT NULL default ' ',
  "AAABIZHI" varchar(16) NOT NULL default '0000000000000000',
  "AAABIYMH" varchar(1) NOT NULL default ' ',
  "AAABIYMI" varchar(1) NOT NULL default ' ',
  "AAABIYMJ" varchar(1) NOT NULL default ' ',
  "AAABIYMK" varchar(1) NOT NULL default ' ',
  "AAABIYML" varchar(1) NOT NULL default ' ',
  "AAABIYMM" varchar(1) NOT NULL default ' ',
  "AAABIYMN" decimal(21,7) NOT NULL default '0.0000000',
  "AAABIYMO" decimal(15,0) NOT NULL default '0',
  "AAABIYMP" decimal(21,7) NOT NULL default '0.0000000',
  "AAABIYMQ" varchar(10) NOT NULL default ' ',
  "AAABIYMR" decimal(21,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAABIYMT"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYMU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIYSQ" varchar(10) NOT NULL default ' ',
  "AAABJADN" varchar(10) NOT NULL default ' ',
  "AAABJACA" varchar(6) NOT NULL default '000000',
  "AAABIZMG" varchar(6) NOT NULL default '000000',
  "AAABLQCN" varchar(4) NOT NULL default ' ',
  "AAABIZMN" varchar(10) NOT NULL default ' ',
  "AAABIZMP" varchar(1) NOT NULL default ' ',
  "AAABIYND" varchar(2) NOT NULL default ' ',
  "AAABIZCF" varchar(8) NOT NULL default '00000000',
  "AAABIZCG" varchar(35) NOT NULL default ' ',
  "AAABIZCH" varchar(8) NOT NULL default '00000000',
  "AAABLTIZ" varchar(6) NOT NULL default '000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABIZCK" varchar(1) NOT NULL default ' ',
  "AAABIZCL" varchar(2) NOT NULL default ' ',
  "AAABIZDJ" varchar(1) NOT NULL default ' ',
  "AAABIYNM" varchar(1) NOT NULL default ' ',
  "AAABIZCO" decimal(23,4) NOT NULL default '0.0000',
  "AAABIYSR" varchar(35) NOT NULL default ' ',
  "AAABIZDX" varchar(3) NOT NULL default ' ',
  "AAABKWFM" varchar(8) NOT NULL default '00000000',
  "AAABLTLM" varchar(6) NOT NULL default '000000',
  "AAABMKKB" varchar(8) NOT NULL default '00000000',
  "AAABLTLN" varchar(6) NOT NULL default '000000',
  "AAABIZDZ" varchar(30) NOT NULL default ' ',
  "AAABIZEA" varchar(60) NOT NULL default ' ',
  "AAABIZEB" varchar(60) NOT NULL default ' ',
  "AAABIZEC" varchar(60) NOT NULL default ' ',
  "AAABIZED" varchar(60) NOT NULL default ' ',
  "AAABIZEE" varchar(60) NOT NULL default ' ',
  "AAABIZMZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIYSQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYOD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIYSQ" varchar(10) NOT NULL default ' ',
  "AAABIZEH" varchar(6) NOT NULL default '000000',
  "AAABJADN" varchar(10) NOT NULL default ' ',
  "AAABMLGN" varchar(4) NOT NULL default ' ',
  "AAABLXOH" varchar(6) NOT NULL default '000000',
  "AAABIZDG" varchar(1) NOT NULL default ' ',
  "AAABIZDH" varchar(1) NOT NULL default ' ',
  "AAABIZMP" varchar(1) NOT NULL default ' ',
  "AAABIZDJ" varchar(1) NOT NULL default ' ',
  "AAABIZDK" varchar(2) NOT NULL default ' ',
  "AAABIZDL" varchar(18) NOT NULL default ' ',
  "AAABIYOQ" varchar(4) NOT NULL default ' ',
  "AAABIYOR" varchar(4) NOT NULL default ' ',
  "AAABIYOS" decimal(15,3) NOT NULL default '0.000',
  "AAABIYOT" decimal(4,3) NOT NULL default '0.000',
  "AAABIYOU" varchar(3) NOT NULL default ' ',
  "AAABIYSX" decimal(15,3) NOT NULL default '0.000',
  "AAABIYSY" decimal(4,3) NOT NULL default '0.000',
  "AAABIYSZ" varchar(3) NOT NULL default ' ',
  "AAABIZDP" decimal(15,3) NOT NULL default '0.000',
  "AAABIZDQ" decimal(4,3) NOT NULL default '0.000',
  "AAABIZDR" decimal(15,3) NOT NULL default '0.000',
  "AAABIZDS" decimal(4,3) NOT NULL default '0.000',
  "AAABIZDT" varchar(1) NOT NULL default ' ',
  "AAABIZDU" varchar(10) NOT NULL default ' ',
  "AAABIZDV" decimal(23,4) NOT NULL default '0.0000',
  "AAABIZDW" decimal(23,4) NOT NULL default '0.0000',
  "AAABIZDX" varchar(3) NOT NULL default ' ',
  "AAABKJFK" varchar(4) NOT NULL default ' ',
  "AAABIZDZ" varchar(30) NOT NULL default ' ',
  "AAABIZEA" varchar(60) NOT NULL default ' ',
  "AAABIZEB" varchar(60) NOT NULL default ' ',
  "AAABIZEC" varchar(60) NOT NULL default ' ',
  "AAABIZED" varchar(60) NOT NULL default ' ',
  "AAABIZEE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIYSQ","AAABIZEH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYPR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIZKA" varchar(2) NOT NULL default '00',
  "AAABIZJZ" varchar(10) NOT NULL default ' ',
  "AAABIYQF" varchar(1) NOT NULL default ' ',
  "AAABLSEB" varchar(8) NOT NULL default ' ',
  "AAABIZDZ" varchar(30) NOT NULL default ' ',
  "AAABIZEA" varchar(60) NOT NULL default ' ',
  "AAABIZEB" varchar(60) NOT NULL default ' ',
  "AAABIZEC" varchar(60) NOT NULL default ' ',
  "AAABIZED" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIZKA","AAABIZJZ","AAABIYQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYQG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(10) NOT NULL default ' ',
  "AAABIYQP" varchar(4) NOT NULL default '0000',
  "AAABIYQK" varchar(10) NOT NULL default ' ',
  "AAABIYQL" varchar(1) NOT NULL default ' ',
  "AAABKGMO" decimal(23,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABLUXQ","AAABJADN","AAABIYQP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYQQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(10) NOT NULL default ' ',
  "AAABIYRA" varchar(18) NOT NULL default ' ',
  "AAABIYRB" varchar(1) NOT NULL default '0',
  "AAABIYRC" varchar(3) NOT NULL default ' ',
  "AAABLEAN" varchar(13) NOT NULL default '0000000000000',
  "AAABIZMZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJADN","AAABIYRA","AAABIYRB","AAABIYRC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYRD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(10) NOT NULL default ' ',
  "AAABJACA" varchar(6) NOT NULL default '000000',
  "AAABIYTS" varchar(10) NOT NULL default ' ',
  "AAABIZMG" varchar(6) NOT NULL default '000000',
  "AAABIYRJ" varchar(6) NOT NULL default ' ',
  "AAABIYRK" varchar(35) NOT NULL default ' ',
  "AAABIZMP" varchar(1) NOT NULL default ' ',
  "AAABIZMN" varchar(10) NOT NULL default ' ',
  "AAABIYRN" varchar(8) NOT NULL default '00000000',
  "AAABIZCH" varchar(8) NOT NULL default '00000000',
  "AAABLTIZ" varchar(6) NOT NULL default '000000',
  "AAABIYRQ" varchar(8) NOT NULL default '00000000',
  "AAABIYRR" varchar(6) NOT NULL default '000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABIZCK" varchar(1) NOT NULL default ' ',
  "AAABIZCL" varchar(2) NOT NULL default ' ',
  "AAABIZDJ" varchar(1) NOT NULL default ' ',
  "AAABIZCO" decimal(23,4) NOT NULL default '0.0000',
  "AAABIZDX" varchar(3) NOT NULL default ' ',
  "AAABIZDZ" varchar(30) NOT NULL default ' ',
  "AAABIZEA" varchar(60) NOT NULL default ' ',
  "AAABIZEB" varchar(60) NOT NULL default ' ',
  "AAABIZEC" varchar(60) NOT NULL default ' ',
  "AAABIZED" varchar(60) NOT NULL default ' ',
  "AAABIZEE" varchar(60) NOT NULL default ' ',
  "AAABIZMZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJADN","AAABJACA","AAABIYTS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYSJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(10) NOT NULL default ' ',
  "AAABIYTS" varchar(10) NOT NULL default ' ',
  "AAABIYTT" varchar(6) NOT NULL default '000000',
  "AAABIZDG" varchar(1) NOT NULL default ' ',
  "AAABIYSP" varchar(6) NOT NULL default '000000',
  "AAABIYSQ" varchar(10) NOT NULL default ' ',
  "AAABIYSR" varchar(35) NOT NULL default ' ',
  "AAABIYSS" varchar(6) NOT NULL default '000000',
  "AAABIZDH" varchar(1) NOT NULL default ' ',
  "AAABIZDJ" varchar(1) NOT NULL default ' ',
  "AAABIYSV" varchar(2) NOT NULL default ' ',
  "AAABIZDL" varchar(18) NOT NULL default ' ',
  "AAABIYSX" decimal(15,3) NOT NULL default '0.000',
  "AAABIYSY" decimal(4,3) NOT NULL default '0.000',
  "AAABIYSZ" varchar(3) NOT NULL default ' ',
  "AAABIZDP" decimal(15,3) NOT NULL default '0.000',
  "AAABIZDQ" decimal(4,3) NOT NULL default '0.000',
  "AAABIZDR" decimal(15,3) NOT NULL default '0.000',
  "AAABIZDS" decimal(4,3) NOT NULL default '0.000',
  "AAABIZDT" varchar(1) NOT NULL default ' ',
  "AAABIZDU" varchar(10) NOT NULL default ' ',
  "AAABIZDV" decimal(23,4) NOT NULL default '0.0000',
  "AAABIZDW" decimal(23,4) NOT NULL default '0.0000',
  "AAABIZDX" varchar(3) NOT NULL default ' ',
  "AAABKJFK" varchar(4) NOT NULL default ' ',
  "AAABIZDZ" varchar(30) NOT NULL default ' ',
  "AAABIZEA" varchar(60) NOT NULL default ' ',
  "AAABIZEB" varchar(60) NOT NULL default ' ',
  "AAABIZEC" varchar(60) NOT NULL default ' ',
  "AAABIZED" varchar(60) NOT NULL default ' ',
  "AAABIZEE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJADN","AAABIYTS","AAABIYTT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYTU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMKFH" varchar(20) NOT NULL default ' ',
  "AAABLRFA" varchar(20) NOT NULL default ' ',
  "AAABLXDP" varchar(1) NOT NULL default ' ',
  "AAABLSKU" varchar(1) NOT NULL default ' ',
  "AAABIYUA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMKFH","AAABLRFA","AAABLXDP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYUF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDTB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLDTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYUK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIZHR" varchar(3) NOT NULL default '000',
  "AAABIYYX" varchar(6) NOT NULL default '000000',
  "AAABLNPY" varchar(30) NOT NULL default ' ',
  "AAABLRPK" varchar(27) NOT NULL default ' ',
  "AAABLKJW" varchar(30) NOT NULL default ' ',
  "AAABIYUR" varchar(30) NOT NULL default ' ',
  "AAABIYUS" varchar(10) NOT NULL default ' ',
  "AAABLTOK" varchar(16) NOT NULL default ' ',
  "AAABIYUU" varchar(30) NOT NULL default ' ',
  "AAABIYUV" varchar(1) NOT NULL default ' ',
  "AAABLDTB" varchar(30) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIZHR","AAABIYYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYVB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIZDL" varchar(18) NOT NULL default ' ',
  "AAABIYVE" varchar(40) NOT NULL default ' ',
  "AAABIYVF" varchar(40) NOT NULL default ' ',
  "AAABIYVG" varchar(4) NOT NULL default ' ',
  "AAABIYVH" varchar(2) NOT NULL default ' ',
  "AAABIYVI" varchar(9) NOT NULL default ' ',
  "AAABIYVJ" varchar(3) NOT NULL default ' ',
  "AAABLXSK" varchar(18) NOT NULL default ' ',
  "AAABIYYG" varchar(2) NOT NULL default ' ',
  "AAABIYYH" varchar(20) NOT NULL default ' ',
  "AAABIYVN" decimal(13,3) NOT NULL default '0.000',
  "AAABIYVO" decimal(13,3) NOT NULL default '0.000',
  "AAABIYVP" varchar(3) NOT NULL default ' ',
  "AAABIYVQ" decimal(13,3) NOT NULL default '0.000',
  "AAABIYVR" varchar(3) NOT NULL default ' ',
  "AAABIZGB" varchar(18) NOT NULL default ' ',
  "AAABIZGC" varchar(1) NOT NULL default ' ',
  "AAABIZFY" varchar(40) NOT NULL default ' ',
  "AAABIZFZ" varchar(40) NOT NULL default ' ',
  "AAABIYVW" varchar(4) NOT NULL default ' ',
  "AAABIYXL" varchar(1) NOT NULL default ' ',
  "AAABIZDZ" varchar(30) NOT NULL default ' ',
  "AAABIZEA" varchar(60) NOT NULL default ' ',
  "AAABIZEB" varchar(60) NOT NULL default ' ',
  "AAABIZEC" varchar(60) NOT NULL default ' ',
  "AAABIZED" varchar(60) NOT NULL default ' ',
  "AAABIZEE" varchar(60) NOT NULL default ' ',
  "AAABLKJE" varchar(1) NOT NULL default ' ',
  "AAABIZKF" decimal(21,7) NOT NULL default '0.0000000',
  "AAABIZMZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIZDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYWJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIZDL" varchar(18) NOT NULL default ' ',
  "AAABIYYQ" varchar(3) NOT NULL default ' ',
  "AAABIYWY" varchar(18) NOT NULL default ' ',
  "AAABIYWO" varchar(6) NOT NULL default '000000',
  "AAABIZDZ" varchar(30) NOT NULL default ' ',
  "AAABIZEA" varchar(60) NOT NULL default ' ',
  "AAABIZEB" varchar(60) NOT NULL default ' ',
  "AAABIZEC" varchar(60) NOT NULL default ' ',
  "AAABIZED" varchar(60) NOT NULL default ' ',
  "AAABIZMZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIZDL","AAABIYYQ","AAABIYWY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYWZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIZDL" varchar(18) NOT NULL default ' ',
  "AAABIYXW" varchar(4) NOT NULL default ' ',
  "AAABIYXX" varchar(2) NOT NULL default ' ',
  "AAABLXRM" varchar(1) NOT NULL default ' ',
  "AAABIYXF" varchar(4) NOT NULL default ' ',
  "AAABIYXG" varchar(1) NOT NULL default '0',
  "AAABIYXH" varchar(2) NOT NULL default ' ',
  "AAABIYXI" decimal(13,3) NOT NULL default '0.000',
  "AAABIZGB" varchar(18) NOT NULL default ' ',
  "AAABIYXK" varchar(2) NOT NULL default ' ',
  "AAABIYXL" varchar(1) NOT NULL default ' ',
  "AAABIYXM" varchar(3) NOT NULL default ' ',
  "AAABIYXN" varchar(18) NOT NULL default ' ',
  "AAABIZDZ" varchar(30) NOT NULL default ' ',
  "AAABIYYJ" varchar(60) NOT NULL default ' ',
  "AAABIYYK" varchar(60) NOT NULL default ' ',
  "AAABIYYL" varchar(60) NOT NULL default ' ',
  "AAABIYYM" varchar(60) NOT NULL default ' ',
  "AAABIZMZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIZDL","AAABIYXW","AAABIYXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYXY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIZDL" varchar(18) NOT NULL default ' ',
  "AAABIYYQ" varchar(3) NOT NULL default ' ',
  "AAABIYYC" decimal(5,0) NOT NULL default '0',
  "AAABIYYD" decimal(5,0) NOT NULL default '0',
  "AAABIYYE" varchar(1) NOT NULL default ' ',
  "AAABLXSK" varchar(18) NOT NULL default ' ',
  "AAABIYYG" varchar(2) NOT NULL default ' ',
  "AAABIYYH" varchar(20) NOT NULL default ' ',
  "AAABIZDZ" varchar(30) NOT NULL default ' ',
  "AAABIYYJ" varchar(60) NOT NULL default ' ',
  "AAABIYYK" varchar(60) NOT NULL default ' ',
  "AAABIYYL" varchar(60) NOT NULL default ' ',
  "AAABIYYM" varchar(60) NOT NULL default ' ',
  "AAABIZMZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIZDL","AAABIYYQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYYR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIYYX" varchar(6) NOT NULL default '000000',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABIZAI" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLUXQ","AAABIYYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYYY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABLZNN" varchar(10) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLMHB","AAABLZNN","AAABLZNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIYZH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLZNN" varchar(10) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAABIYZY" varchar(70) NOT NULL default ' ',
  "AAABIYZZ" varchar(1) NOT NULL default ' ',
  "AAABIZAA" varchar(6) NOT NULL default '000000',
  "AAABLNPY" varchar(30) NOT NULL default ' ',
  "AAABIYZP" varchar(6) NOT NULL default '000000',
  "AAABJADN" varchar(10) NOT NULL default ' ',
  "AAABJACA" varchar(6) NOT NULL default '000000',
  "AAABIZMG" varchar(6) NOT NULL default '000000',
  "AAABIYZT" varchar(132) NOT NULL default ' ',
  "AAABIZMZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLZNN","AAABLZNO","AAABIYZY","AAABIYZZ","AAABIZAA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZAB" (
  "AAABIZAI" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABIZAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZAE" (
  "AAABIZAI" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABIZAI","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZAK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMKFH" varchar(3) NOT NULL default '000',
  "AAABIZAS" varchar(6) NOT NULL default '000000',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMKFH","AAABIZAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZAT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIZBU" varchar(10) NOT NULL default ' ',
  "AAABIZMN" varchar(10) NOT NULL default ' ',
  "AAABLOAS" varchar(16) NOT NULL default ' ',
  "AAABLOBA" varchar(8) NOT NULL default '00000000',
  "AAABIZAZ" varchar(6) NOT NULL default '000000',
  "AAABIZBA" varchar(8) NOT NULL default '00000000',
  "AAABIZBB" decimal(6,0) NOT NULL default '0',
  "AAABJFXX" decimal(23,4) NOT NULL default '0.0000',
  "AAABLIVH" varchar(1) NOT NULL default ' ',
  "AAABIZBE" varchar(2) NOT NULL default ' ',
  "AAABIZBF" varchar(1) NOT NULL default ' ',
  "AAABIZBG" varchar(3) NOT NULL default ' ',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABJFWW" varchar(8) NOT NULL default '00000000',
  "AAABIZBJ" varchar(40) NOT NULL default ' ',
  "AAABLOCD" varchar(50) NOT NULL default ' ',
  "AAABIZMP" varchar(1) NOT NULL default ' ',
  "AAABIZBM" varchar(1) NOT NULL default ' ',
  "AAABIZMZ" varchar(1) NOT NULL default ' ',
  "AAABIZDZ" varchar(30) NOT NULL default ' ',
  "AAABIZEA" varchar(60) NOT NULL default ' ',
  "AAABIZEB" varchar(60) NOT NULL default ' ',
  "AAABIZEC" varchar(60) NOT NULL default ' ',
  "AAABIZED" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIZBU","AAABIZMN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZBW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIZEG" varchar(10) NOT NULL default ' ',
  "AAABJADN" varchar(10) NOT NULL default ' ',
  "AAABJACA" varchar(6) NOT NULL default '000000',
  "AAABIZCB" varchar(6) NOT NULL default '000000',
  "AAABIZMP" varchar(1) NOT NULL default ' ',
  "AAABLQCN" varchar(4) NOT NULL default ' ',
  "AAABIZMN" varchar(10) NOT NULL default ' ',
  "AAABIZCF" varchar(8) NOT NULL default '00000000',
  "AAABIZCG" varchar(35) NOT NULL default ' ',
  "AAABIZCH" varchar(8) NOT NULL default '00000000',
  "AAABLTIZ" varchar(6) NOT NULL default '000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABIZCK" varchar(1) NOT NULL default ' ',
  "AAABIZCL" varchar(2) NOT NULL default ' ',
  "AAABIZDJ" varchar(1) NOT NULL default ' ',
  "AAABIZCN" varchar(1) NOT NULL default ' ',
  "AAABIZCO" decimal(23,4) NOT NULL default '0.0000',
  "AAABIZCP" varchar(35) NOT NULL default ' ',
  "AAABIZDX" varchar(3) NOT NULL default ' ',
  "AAABIZDZ" varchar(30) NOT NULL default ' ',
  "AAABIZEA" varchar(60) NOT NULL default ' ',
  "AAABIZEB" varchar(60) NOT NULL default ' ',
  "AAABIZEC" varchar(60) NOT NULL default ' ',
  "AAABIZED" varchar(60) NOT NULL default ' ',
  "AAABIZEE" varchar(60) NOT NULL default ' ',
  "AAABIZMZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIZEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZDA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIZEG" varchar(10) NOT NULL default ' ',
  "AAABIZEH" varchar(6) NOT NULL default '000000',
  "AAABJADN" varchar(10) NOT NULL default ' ',
  "AAABLXOH" varchar(6) NOT NULL default '000000',
  "AAABIZDG" varchar(1) NOT NULL default ' ',
  "AAABIZDH" varchar(1) NOT NULL default ' ',
  "AAABIZMP" varchar(1) NOT NULL default ' ',
  "AAABIZDJ" varchar(1) NOT NULL default ' ',
  "AAABIZDK" varchar(2) NOT NULL default ' ',
  "AAABIZDL" varchar(18) NOT NULL default ' ',
  "AAABIZDM" decimal(15,3) NOT NULL default '0.000',
  "AAABIZDN" decimal(4,0) NOT NULL default '0',
  "AAABIZDO" varchar(3) NOT NULL default ' ',
  "AAABIZDP" decimal(15,3) NOT NULL default '0.000',
  "AAABIZDQ" decimal(4,3) NOT NULL default '0.000',
  "AAABIZDR" decimal(15,3) NOT NULL default '0.000',
  "AAABIZDS" decimal(4,3) NOT NULL default '0.000',
  "AAABIZDT" varchar(1) NOT NULL default ' ',
  "AAABIZDU" varchar(10) NOT NULL default ' ',
  "AAABIZDV" decimal(23,4) NOT NULL default '0.0000',
  "AAABIZDW" decimal(23,4) NOT NULL default '0.0000',
  "AAABIZDX" varchar(3) NOT NULL default ' ',
  "AAABLDUY" varchar(4) NOT NULL default ' ',
  "AAABIZDZ" varchar(30) NOT NULL default ' ',
  "AAABIZEA" varchar(60) NOT NULL default ' ',
  "AAABIZEB" varchar(60) NOT NULL default ' ',
  "AAABIZEC" varchar(60) NOT NULL default ' ',
  "AAABIZED" varchar(60) NOT NULL default ' ',
  "AAABIZEE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIZEG","AAABIZEH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZEI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(10) NOT NULL default ' ',
  "AAABJACA" varchar(6) NOT NULL default '000000',
  "AAABIZMG" varchar(6) NOT NULL default '000000',
  "AAABIZHA" varchar(6) NOT NULL default '000000',
  "AAABIZGJ" varchar(2) NOT NULL default ' ',
  "AAABKGMO" decimal(18,0) NOT NULL default '0',
  "AAABIZGL" varchar(25) NOT NULL default '0000000000000000000000000',
  "AAABIZGM" varchar(4) NOT NULL default ' ',
  "AAABIZGN" varchar(8) NOT NULL default '00000000',
  "AAABIZGO" varchar(13) NOT NULL default ' ',
  "AAABIZGP" varchar(15) NOT NULL default ' ',
  "AAABIZGQ" varchar(16) NOT NULL default ' ',
  "AAABIZGR" varchar(8) NOT NULL default '00000000',
  "AAABIZGS" varchar(6) NOT NULL default '000000',
  "AAABIZGT" varchar(1) NOT NULL default ' ',
  "AAABIZGU" varchar(2) NOT NULL default ' ',
  "AAABIZGV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJADN","AAABJACA","AAABIZMG","AAABIZHA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZFG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJACE" varchar(10) NOT NULL default ' ',
  "AAABIZJY" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJACE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZFM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIZFS" varchar(16) NOT NULL default '0000000000000000',
  "AAABIZFT" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIZFS","AAABIZFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZFU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIZGB" varchar(18) NOT NULL default ' ',
  "AAABIZGC" varchar(1) NOT NULL default ' ',
  "AAABIZFY" varchar(40) NOT NULL default ' ',
  "AAABIZFZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIZGB","AAABIZGC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZGD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(10) NOT NULL default ' ',
  "AAABJACA" varchar(6) NOT NULL default '000000',
  "AAABIZMG" varchar(6) NOT NULL default '000000',
  "AAABIZHA" varchar(6) NOT NULL default '000000',
  "AAABIZGJ" varchar(2) NOT NULL default ' ',
  "AAABKGMO" decimal(23,4) NOT NULL default '0.0000',
  "AAABIZGL" varchar(25) NOT NULL default '0000000000000000000000000',
  "AAABIZGM" varchar(4) NOT NULL default ' ',
  "AAABIZGN" varchar(8) NOT NULL default '00000000',
  "AAABIZGO" varchar(13) NOT NULL default ' ',
  "AAABIZGP" varchar(15) NOT NULL default ' ',
  "AAABIZGQ" varchar(16) NOT NULL default ' ',
  "AAABIZGR" varchar(8) NOT NULL default '00000000',
  "AAABIZGS" varchar(6) NOT NULL default '000000',
  "AAABIZGT" varchar(1) NOT NULL default ' ',
  "AAABIZGU" varchar(4) NOT NULL default ' ',
  "AAABIZGV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJADN","AAABJACA","AAABIZMG","AAABIZHA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZHB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIZHM" varchar(32) NOT NULL default ' ',
  "AAABIZHN" varchar(10) NOT NULL default ' ',
  "AAABIZHF" varchar(1) NOT NULL default ' ',
  "AAABIZKA" varchar(2) NOT NULL default '00',
  "AAABIZHH" varchar(1) NOT NULL default ' ',
  "AAABIZHI" varchar(16) NOT NULL default '0000000000000000',
  "AAABIZHJ" decimal(21,7) NOT NULL default '0.0000000',
  "AAABIZHK" decimal(21,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAABIZHM","AAABIZHN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZHO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIZHT" varchar(16) NOT NULL default ' ',
  "AAABIZHR" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLUXQ","AAABIZHT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZHU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(10) NOT NULL default ' ',
  "AAABIZHX" varchar(4) NOT NULL default ' ',
  "AAABLPNX" varchar(2) NOT NULL default ' ',
  "AAABIZHZ" varchar(10) NOT NULL default ' ',
  "AAABIZIA" varchar(10) NOT NULL default ' ',
  "AAABJACF" varchar(10) NOT NULL default ' ',
  "AAABIZKC" varchar(10) NOT NULL default ' ',
  "AAABIZID" varchar(10) NOT NULL default ' ',
  "AAABLYRW" varchar(6) NOT NULL default ' ',
  "AAABIZIF" varchar(40) NOT NULL default ' ',
  "AAABJTDQ" decimal(6,0) NOT NULL default '0',
  "AAABIZIH" decimal(8,0) NOT NULL default '0',
  "AAABIZII" decimal(8,0) NOT NULL default '0',
  "AAABIZMR" varchar(8) NOT NULL default '00000000',
  "AAABIZMS" varchar(6) NOT NULL default '000000',
  "AAABIZMT" varchar(8) NOT NULL default '00000000',
  "AAABIZMU" varchar(6) NOT NULL default '000000',
  "AAABIZIN" varchar(8) NOT NULL default '00000000',
  "AAABIZIO" varchar(6) NOT NULL default '000000',
  "AAABIZIP" varchar(8) NOT NULL default '00000000',
  "AAABIZIQ" varchar(6) NOT NULL default '000000',
  "AAABIZIR" varchar(1) NOT NULL default ' ',
  "AAABIZMZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJADN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZIV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(10) NOT NULL default ' ',
  "AAABLOAI" decimal(15,0) NOT NULL default '0',
  "AAABIZIZ" varchar(1) NOT NULL default ' ',
  "AAABIZJA" varchar(1) NOT NULL default ' ',
  "AAABIZJB" varchar(1) NOT NULL default ' ',
  "AAABIZJC" varchar(1) NOT NULL default ' ',
  "AAABIZJD" varchar(1) NOT NULL default ' ',
  "AAABIZJE" varchar(1) NOT NULL default ' ',
  "AAABIZJF" varchar(1) NOT NULL default ' ',
  "AAABIZJG" varchar(1) NOT NULL default ' ',
  "AAABIZJH" varchar(1) NOT NULL default ' ',
  "AAABIZJI" varchar(1) NOT NULL default ' ',
  "AAABIZJJ" varchar(1) NOT NULL default ' ',
  "AAABIZJK" varchar(1) NOT NULL default ' ',
  "AAABIZJL" varchar(1) NOT NULL default ' ',
  "AAABIZJM" varchar(1) NOT NULL default ' ',
  "AAABIZJN" varchar(1) NOT NULL default ' ',
  "AAABIZJO" varchar(1) NOT NULL default ' ',
  "AAABIZJP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJADN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZJS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIZKK" varchar(36) NOT NULL default ' ',
  "AAABIZKL" varchar(1) NOT NULL default ' ',
  "AAABLKZT" varchar(1) NOT NULL default ' ',
  "AAABIZJX" varchar(1) NOT NULL default ' ',
  "AAABIZJY" varchar(12) NOT NULL default ' ',
  "AAABIZJZ" varchar(10) NOT NULL default ' ',
  "AAABIZKA" varchar(2) NOT NULL default '00',
  "AAABIZKB" varchar(10) NOT NULL default ' ',
  "AAABIZKC" varchar(10) NOT NULL default ' ',
  "AAABIZKD" varchar(10) NOT NULL default ' ',
  "AAABIZKE" decimal(15,0) NOT NULL default '0',
  "AAABIZKF" decimal(15,0) NOT NULL default '0',
  "AAABIZKG" varchar(240) NOT NULL default ' ',
  "AAABIZKH" varchar(240) NOT NULL default ' ',
  "AAABIZKI" varchar(240) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIZKK","AAABIZKL","AAABLKZT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZKN" (
  "AAABIZKU" varchar(1) NOT NULL default '0',
  "AAABIZKP" varchar(1) NOT NULL default '0',
  "AAABIZKQ" varchar(1) NOT NULL default '0',
  "AAABIZKR" varchar(1) NOT NULL default '0',
  "AAABIZKS" varchar(1) NOT NULL default '0',
  "AAABIZKT" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABIZKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZKV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIZLL" varchar(18) NOT NULL default ' ',
  "AAABIZLM" varchar(3) NOT NULL default '000',
  "AAABIZKZ" varchar(18) NOT NULL default ' ',
  "AAABIZLA" varchar(3) NOT NULL default '000',
  "AAABIZLB" varchar(16) NOT NULL default '0000000000000000',
  "AAABIZLC" varchar(1) NOT NULL default '0',
  "AAABIZLD" varchar(16) NOT NULL default '0000000000000000',
  "AAABIZLE" varchar(1) NOT NULL default '0',
  "AAABIZLF" varchar(16) NOT NULL default '0000000000000000',
  "AAABIZLG" varchar(1) NOT NULL default '0',
  "AAABIZLH" varchar(16) NOT NULL default '0000000000000000',
  "AAABIZLI" varchar(1) NOT NULL default '0',
  "AAABLOAI" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABIZLL","AAABIZLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZLN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(10) NOT NULL default ' ',
  "AAABJACA" varchar(6) NOT NULL default '000000',
  "AAABIZMG" varchar(6) NOT NULL default '000000',
  "AAABIZMP" varchar(1) NOT NULL default ' ',
  "AAABIZLT" varchar(1) NOT NULL default ' ',
  "AAABIZLU" varchar(8) NOT NULL default '00000000',
  "AAABIZLV" varchar(6) NOT NULL default '000000',
  "AAABMKKB" varchar(8) NOT NULL default '00000000',
  "AAABLTLN" varchar(6) NOT NULL default '000000',
  "AAABLEKV" varchar(25) NOT NULL default ' ',
  "AAABIZLZ" varchar(12) NOT NULL default ' ',
  "AAABIZMQ" varchar(1) NOT NULL default ' ',
  "AAABIZMM" varchar(2) NOT NULL default ' ',
  "AAABIZMC" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLUXQ","AAABJADN","AAABJACA","AAABIZMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZMH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJADN" varchar(10) NOT NULL default ' ',
  "AAABJACA" varchar(6) NOT NULL default '000000',
  "AAABIZML" varchar(6) NOT NULL default '000000',
  "AAABIZMM" varchar(2) NOT NULL default ' ',
  "AAABIZMN" varchar(10) NOT NULL default ' ',
  "AAABJWRA" varchar(1) NOT NULL default ' ',
  "AAABIZMP" varchar(1) NOT NULL default ' ',
  "AAABIZMQ" varchar(1) NOT NULL default ' ',
  "AAABIZMR" varchar(8) NOT NULL default '00000000',
  "AAABIZMS" varchar(6) NOT NULL default '000000',
  "AAABIZMT" varchar(8) NOT NULL default '00000000',
  "AAABIZMU" varchar(6) NOT NULL default '000000',
  "AAABIZMV" varchar(6) NOT NULL default '000000',
  "AAABIZMW" varchar(3) NOT NULL default ' ',
  "AAABIZMX" varchar(2) NOT NULL default ' ',
  "AAABIZMY" varchar(2) NOT NULL default ' ',
  "AAABIZMZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJADN","AAABJACA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZND" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIZSP" varchar(40) NOT NULL default ' ',
  "AAABIZNV" varchar(30) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLUSR" varchar(100) NOT NULL default ' ',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  "AAABIZNS" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIZSP","AAABIZNV","AAABMEKQ","AAABMQIJ","AAABLUSR","AAABMCJZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZOA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  "AAABLXYV" varchar(2) NOT NULL default '00',
  "AAABLXZF" varchar(1) NOT NULL default ' ',
  "AAABLXYY" decimal(11,2) NOT NULL default '0.00',
  "AAABLXYZ" varchar(5) NOT NULL default ' ',
  "AAABLXZA" decimal(5,0) NOT NULL default '0',
  "AAABLXZB" varchar(3) NOT NULL default ' ',
  "AAABLXZI" varchar(1) NOT NULL default ' ',
  "AAABLXZJ" varchar(1) NOT NULL default ' ',
  "AAABLXZK" varchar(3) NOT NULL default ' ',
  "AAABLXZL" varchar(5) NOT NULL default ' ',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYAS","AAABLXYV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZOR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  "AAABLXYV" varchar(2) NOT NULL default '00',
  "AAABIZPM" varchar(4) NOT NULL default '0000',
  "AAABLYAA" decimal(15,3) NOT NULL default '0.000',
  "AAABLXYY" decimal(11,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLYAS","AAABLXYV","AAABIZPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZPC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  "AAABLXYV" varchar(2) NOT NULL default '00',
  "AAABIZPM" varchar(4) NOT NULL default '0000',
  "AAABLWXO" decimal(15,2) NOT NULL default '0.00',
  "AAABLXYY" decimal(11,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLYAS","AAABLXYV","AAABIZPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZPN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPN" varchar(30) NOT NULL default ' ',
  "AAABIZPQ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLPN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZPT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIZSP" varchar(40) NOT NULL default ' ',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABKYSP" varchar(5) NOT NULL default ' ',
  "AAABKYSQ" varchar(5) NOT NULL default ' ',
  "AAABIZQZ" varchar(8) NOT NULL default ' ',
  "AAABKWQY" decimal(9,0) NOT NULL default '0',
  "AAABKWQZ" decimal(9,0) NOT NULL default '0',
  "AAABIZQC" varchar(4) NOT NULL default ' ',
  "AAABIZQD" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABIZSP","AAABLXJP","AAABKYSP","AAABKYSQ","AAABIZQZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZQK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIZSP" varchar(40) NOT NULL default ' ',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABKYSP" varchar(5) NOT NULL default ' ',
  "AAABKYSQ" varchar(5) NOT NULL default ' ',
  "AAABIZQZ" varchar(8) NOT NULL default ' ',
  "AAABMPSW" decimal(9,5) NOT NULL default '0.00000',
  "AAABKWQY" decimal(9,0) NOT NULL default '0',
  "AAABKWQZ" decimal(9,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABIZSP","AAABLXJP","AAABKYSP","AAABKYSQ","AAABIZQZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZRA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIZSP" varchar(40) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJVUZ" varchar(3) NOT NULL default ' ',
  "AAABIZRF" varchar(3) NOT NULL default ' ',
  "AAABIZRG" varchar(6) NOT NULL default ' ',
  "AAABIZRH" varchar(10) NOT NULL default ' ',
  "AAABIZRI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIZSP","AAABMRHS","AAABJVUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZRN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMX","AAABMAMY","AAABMEKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZRW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIZSB" varchar(40) NOT NULL default ' ',
  "AAABIZSP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIZSB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZSC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIZSP" varchar(40) NOT NULL default ' ',
  "AAABJVUZ" varchar(3) NOT NULL default ' ',
  "AAABJVTD" varchar(6) NOT NULL default ' ',
  "AAABMLEL" int(11) NOT NULL default '0',
  "AAABIZSI" int(11) NOT NULL default '0',
  "AAABIZSJ" smallint(6) NOT NULL default '0',
  "AAABIZSK" decimal(9,6) NOT NULL default '0.000000',
  "AAABIZSL" smallint(6) NOT NULL default '0',
  "AAABIZSM" varchar(3) NOT NULL default ' ',
  "AAABIZSN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIZSP","AAABJVUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZSR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABJSTM" decimal(15,12) NOT NULL default '0.000000000000',
  "AAABJSTN" decimal(15,12) NOT NULL default '0.000000000000',
  "AAABIZSW" varchar(4) NOT NULL default '0000',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKM" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPLT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZTC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLMR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLMR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZTH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABIZVM" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPLT","AAABIZVM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZTO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABKLUK" varchar(5) NOT NULL default '00000',
  "AAABIZUP" varchar(8) NOT NULL default '00000000',
  "AAABIZUQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMPLT","AAABKLUK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZTX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPLT","AAABLZFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZUE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQVW" varchar(18) NOT NULL default ' ',
  "AAABIZVM" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQVW","AAABIZVM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZUL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQVW" varchar(18) NOT NULL default ' ',
  "AAABKLUK" varchar(5) NOT NULL default '00000',
  "AAABIZUP" varchar(8) NOT NULL default '00000000',
  "AAABIZUQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMQVW","AAABKLUK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZUU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQVW" varchar(18) NOT NULL default ' ',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQVW","AAABLZFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZVB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIZVM" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIZVM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZVG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIZVM" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIZVK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIZVM","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZVO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABLYOT" varchar(1) NOT NULL default ' ',
  "AAABLYOU" decimal(3,1) NOT NULL default '0.0',
  "AAABLYOV" varchar(1) NOT NULL default ' ',
  "AAABLYOW" decimal(3,1) NOT NULL default '0.0',
  "AAABLYOX" varchar(1) NOT NULL default ' ',
  "AAABLYOY" decimal(3,1) NOT NULL default '0.0',
  "AAABLYPB" varchar(1) NOT NULL default ' ',
  "AAABLYPC" decimal(3,1) NOT NULL default '0.0',
  "AAABLYPD" varchar(1) NOT NULL default ' ',
  "AAABLYPE" decimal(3,1) NOT NULL default '0.0',
  "AAABLYOZ" varchar(1) NOT NULL default ' ',
  "AAABLYPA" decimal(3,1) NOT NULL default '0.0',
  PRIMARY KEY ("AAABMRIL","AAABLZFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZWF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIZWL" varchar(1) NOT NULL default ' ',
  "AAABIZWM" varchar(10) NOT NULL default ' ',
  "AAABIZWJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIZWL","AAABIZWM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZWN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIZWP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZWR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIZWU" varchar(20) NOT NULL default ' ',
  "AAABIZWV" varchar(20) NOT NULL default ' ',
  "AAABIZWW" varchar(20) NOT NULL default ' ',
  "AAABIZWX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZXA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYRW" varchar(6) NOT NULL default ' ',
  "AAABJAUE" varchar(3) NOT NULL default ' ',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABIZXW" decimal(3,0) NOT NULL default '0',
  "AAABIZXX" decimal(11,0) NOT NULL default '0',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYRW","AAABJAUE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZXM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYRW" varchar(6) NOT NULL default ' ',
  "AAABJAUE" varchar(3) NOT NULL default ' ',
  "AAABMRCU" varchar(5) NOT NULL default '00000',
  "AAABIZXR" varchar(5) NOT NULL default '00000',
  "AAABJARR" varchar(10) NOT NULL default ' ',
  "AAABJARS" varchar(10) NOT NULL default ' ',
  "AAABJARL" varchar(18) NOT NULL default ' ',
  "AAABJARM" varchar(18) NOT NULL default ' ',
  "AAABIZXW" decimal(3,0) NOT NULL default '0',
  "AAABIZXX" decimal(11,0) NOT NULL default '0',
  "AAABKMMG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYRW","AAABJAUE","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZYD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJAAL" varchar(10) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJAAL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZYJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABIZYS" varchar(20) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABIZYS","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZYU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJABE" varchar(4) NOT NULL default ' ',
  "AAABIZYX" varchar(1) NOT NULL default ' ',
  "AAABIZYY" varchar(1) NOT NULL default ' ',
  "AAABIZYZ" varchar(8) NOT NULL default ' ',
  "AAABIZZA" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJABE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZZD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIZZP" varchar(2) NOT NULL default ' ',
  "AAABIZZG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIZZP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZZJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIZZP" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIZZP","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABIZZR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIZZY" varchar(1) NOT NULL default '0',
  "AAABKNCK" varchar(2) NOT NULL default ' ',
  "AAABJIOZ" varchar(3) NOT NULL default ' ',
  "AAABJIPA" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIZZY","AAABKNCK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAAA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABJAAL" varchar(10) NOT NULL default ' ',
  "AAABJAAM" varchar(10) NOT NULL default ' ',
  "AAABJAAF" int(11) NOT NULL default '0',
  "AAABJAAG" varchar(1) NOT NULL default ' ',
  "AAABJAAH" varchar(1) NOT NULL default ' ',
  "AAABJAAI" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABLZKF","AAABJAAL","AAABJAAM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAAN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJAAS" varchar(4) NOT NULL default '0000',
  "AAABLYCI" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJAAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAAT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJABE" varchar(4) NOT NULL default ' ',
  "AAABJABF" varchar(5) NOT NULL default ' ',
  "AAABJABG" varchar(2) NOT NULL default ' ',
  "AAABJABH" varchar(1) NOT NULL default ' ',
  "AAABJAAZ" varchar(2) NOT NULL default ' ',
  "AAABJABA" varchar(2) NOT NULL default ' ',
  "AAABJABB" varchar(2) NOT NULL default ' ',
  "AAABJABC" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJABE","AAABJABF","AAABJABG","AAABJABH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJABI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJABT" varchar(3) NOT NULL default '000',
  "AAABJABU" varchar(35) NOT NULL default ' ',
  "AAABJABV" varchar(6) NOT NULL default '000000',
  "AAABJABW" varchar(3) NOT NULL default '000',
  "AAABJABX" varchar(35) NOT NULL default ' ',
  "AAABJABY" varchar(6) NOT NULL default '000000',
  "AAABJADN" varchar(32) NOT NULL default ' ',
  "AAABJACA" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUXQ","AAABJABT","AAABJABU","AAABJABV","AAABJABW","AAABJABX","AAABJABY","AAABJADN","AAABJACA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJACB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJAFK" varchar(10) NOT NULL default ' ',
  "AAABJACE" varchar(10) NOT NULL default ' ',
  "AAABJACF" varchar(10) NOT NULL default ' ',
  "AAABJATS" varchar(4) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABJAJX" varchar(2) NOT NULL default ' ',
  "AAABJACL" varchar(3) NOT NULL default ' ',
  "AAABJACM" varchar(1) NOT NULL default ' ',
  "AAABJACN" varchar(1) NOT NULL default ' ',
  "AAABJACO" varchar(1) NOT NULL default ' ',
  "AAABJACP" varchar(1) NOT NULL default ' ',
  "AAABJACQ" varchar(1) NOT NULL default ' ',
  "AAABJACR" varchar(1) NOT NULL default ' ',
  "AAABJACS" varchar(1) NOT NULL default ' ',
  "AAABJACT" varchar(1) NOT NULL default ' ',
  "AAABJACU" varchar(1) NOT NULL default ' ',
  "AAABJACV" varchar(1) NOT NULL default ' ',
  "AAABJACW" varchar(1) NOT NULL default ' ',
  "AAABJAEL" varchar(8) NOT NULL default '00000000',
  "AAABLOCX" varchar(6) NOT NULL default '000000',
  "AAABJAEN" varchar(12) NOT NULL default ' ',
  "AAABLPNX" varchar(3) NOT NULL default '000',
  "AAABJAEQ" varchar(10) NOT NULL default ' ',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJAFK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJADF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJAFK" varchar(10) NOT NULL default ' ',
  "AAABJADN" varchar(32) NOT NULL default ' ',
  "AAABJADR" varchar(2) NOT NULL default ' ',
  "AAABLTWR" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJAFK","AAABJADN"),
  KEY "AAABJADO" ("AAABLUXQ","AAABJAFK","AAABJADR","AAABLTWR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJADT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJAFK" varchar(10) NOT NULL default ' ',
  "AAABJAEG" varchar(2) NOT NULL default ' ',
  "AAABJADX" decimal(13,2) NOT NULL default '0.00',
  "AAABJADY" decimal(13,2) NOT NULL default '0.00',
  "AAABJADZ" decimal(13,2) NOT NULL default '0.00',
  "AAABJAEA" decimal(13,2) NOT NULL default '0.00',
  "AAABJAEB" decimal(13,2) NOT NULL default '0.00',
  "AAABKNCK" varchar(2) NOT NULL default ' ',
  "AAABJAED" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJAFK","AAABJAEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAEH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJAFK" varchar(10) NOT NULL default ' ',
  "AAABJAEQ" varchar(10) NOT NULL default ' ',
  "AAABJAEL" varchar(8) NOT NULL default '00000000',
  "AAABLOCX" varchar(6) NOT NULL default '000000',
  "AAABJAEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJAFK","AAABJAEQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAER" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJAFK" varchar(10) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABJAEV" decimal(13,3) NOT NULL default '0.000',
  "AAABJAEW" decimal(13,3) NOT NULL default '0.000',
  "AAABJAEX" decimal(13,3) NOT NULL default '0.000',
  "AAABJAEY" decimal(13,3) NOT NULL default '0.000',
  "AAABJAEZ" decimal(13,3) NOT NULL default '0.000',
  "AAABJAFA" decimal(13,3) NOT NULL default '0.000',
  "AAABJAFB" decimal(13,3) NOT NULL default '0.000',
  "AAABJAFC" varchar(3) NOT NULL default ' ',
  "AAABJAFD" decimal(13,2) NOT NULL default '0.00',
  "AAABJAFE" varchar(5) NOT NULL default ' ',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAABMCEY" varchar(10) NOT NULL default ' ',
  "AAABMCVW" varchar(10) NOT NULL default ' ',
  "AAABJAFI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJAFK","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAFM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJATS" varchar(4) NOT NULL default ' ',
  "AAABJAFP" varchar(4) NOT NULL default ' ',
  "AAABJAFQ" varchar(2) NOT NULL default ' ',
  "AAABJAFR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJATS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAFU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABJATS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLZFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAGA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJATS" varchar(4) NOT NULL default ' ',
  "AAABJATT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJATS","AAABJATT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAGH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJATS" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJAGL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJATS","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAGP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJAJX" varchar(2) NOT NULL default ' ',
  "AAABJAHQ" varchar(2) NOT NULL default ' ',
  "AAABJAGT" varchar(1) NOT NULL default ' ',
  "AAABJAGU" varchar(1) NOT NULL default ' ',
  "AAABJAGV" varchar(1) NOT NULL default ' ',
  "AAABJAGW" varchar(1) NOT NULL default ' ',
  "AAABJAGX" varchar(2) NOT NULL default ' ',
  "AAABLZZR" varchar(4) NOT NULL default ' ',
  "AAABJAGZ" varchar(4) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABJAHB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJAJX","AAABJAHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAHF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJAHQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJAHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAHK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJAHQ" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJAHO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJAHQ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAHS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJAHW" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJAHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAHX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLUSV" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLUSV","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAIF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJAIJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJAIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAIK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLUSV" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJAIO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLUSV","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAIS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLZFC" varchar(4) NOT NULL default ' ',
  "AAABJATT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLZFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAIY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJAJX" varchar(2) NOT NULL default ' ',
  "AAABJAJB" varchar(1) NOT NULL default ' ',
  "AAABJAJC" varchar(1) NOT NULL default ' ',
  "AAABJAJD" varchar(1) NOT NULL default ' ',
  "AAABJAJE" varchar(1) NOT NULL default ' ',
  "AAABJAJF" varchar(1) NOT NULL default ' ',
  "AAABJAJG" varchar(1) NOT NULL default ' ',
  "AAABJAJH" varchar(1) NOT NULL default ' ',
  "AAABJAJI" varchar(1) NOT NULL default ' ',
  "AAABJAJJ" varchar(1) NOT NULL default ' ',
  "AAABJAJK" varchar(1) NOT NULL default ' ',
  "AAABJAJL" varchar(1) NOT NULL default ' ',
  "AAABJAJM" varchar(1) NOT NULL default ' ',
  "AAABJAJN" varchar(1) NOT NULL default ' ',
  "AAABJAJO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJAJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAJR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJAJX" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJAJV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJAJX","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAJZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJARY" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJARY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAKE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJARY" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJAKI" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJARY","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJATK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJATK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAKR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJATK" varchar(3) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJAKV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJATK","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAKZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABLZZR" varchar(4) NOT NULL default ' ',
  "AAABJALD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZFD","AAABLZZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJALH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLBG" varchar(6) NOT NULL default ' ',
  "AAABJATK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJALN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLBG" varchar(6) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJALR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLBG","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJALV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABKYDT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMX","AAABMAMY","AAABMQIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAMF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABJATK" varchar(3) NOT NULL default ' ',
  "AAABJAMJ" decimal(11,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMLHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAMM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABJATK" varchar(3) NOT NULL default ' ',
  "AAABJAMQ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPLT","AAABJATK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAMU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZFC" varchar(4) NOT NULL default ' ',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABJAQS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZFC","AAABLZFD","AAABMRCU","AAABMAMY","AAABMQIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJANI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJAPY" varchar(10) NOT NULL default ' ',
  "AAABJANL" varchar(8) NOT NULL default '00000000',
  "AAABJARB" varchar(30) NOT NULL default ' ',
  "AAABJASZ" varchar(10) NOT NULL default ' ',
  "AAABJAUE" varchar(3) NOT NULL default ' ',
  "AAABJARL" varchar(18) NOT NULL default ' ',
  "AAABJARM" varchar(18) NOT NULL default ' ',
  "AAABJTAJ" varchar(6) NOT NULL default '000000',
  "AAABJARO" varchar(6) NOT NULL default ' ',
  "AAABJARP" varchar(10) NOT NULL default ' ',
  "AAABJARQ" varchar(10) NOT NULL default ' ',
  "AAABJARR" varchar(10) NOT NULL default ' ',
  "AAABJARS" varchar(10) NOT NULL default ' ',
  "AAABLYRW" varchar(6) NOT NULL default ' ',
  "AAABJARY" varchar(10) NOT NULL default ' ',
  "AAABJANZ" varchar(8) NOT NULL default '00000000',
  "AAABJAOA" varchar(1) NOT NULL default ' ',
  "AAABJAOB" varchar(1) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKM" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJAPY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAOK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJAPY" varchar(10) NOT NULL default ' ',
  "AAABJLDK" varchar(1) NOT NULL default ' ',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABJAQC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJAPY","AAABJLDK","AAABMQIL","AAABJAQC"),
  KEY "AAABJAOV" ("AAABJLDK","AAABMQIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAOY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJAPY" varchar(10) NOT NULL default ' ',
  "AAABJAPZ" varchar(3) NOT NULL default '000',
  "AAABJASL" varchar(3) NOT NULL default '000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABJATL" decimal(11,0) NOT NULL default '0',
  "AAABJATM" decimal(11,0) NOT NULL default '0',
  "AAABJATN" decimal(11,0) NOT NULL default '0',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABJASU" varchar(2) NOT NULL default ' ',
  "AAABMAKT" varchar(10) NOT NULL default '0000000000',
  "AAABLXWL" varchar(25) NOT NULL default ' ',
  "AAABJASX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJAPY","AAABJAPZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAPQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJAPY" varchar(10) NOT NULL default ' ',
  "AAABJAPZ" varchar(3) NOT NULL default '000',
  "AAABJLDK" varchar(1) NOT NULL default ' ',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABJAQC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJAPY","AAABJAPZ","AAABJLDK","AAABMQIL","AAABJAQC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAQD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJAUE" varchar(3) NOT NULL default ' ',
  "AAABJEHS" varchar(30) NOT NULL default ' ',
  "AAABJEHT" varchar(30) NOT NULL default ' ',
  "AAABJAQI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJAUE","AAABJEHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAQM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJAUE" varchar(3) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABJAQS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJAUE","AAABMRCU","AAABMAMY","AAABMQIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJASZ" varchar(10) NOT NULL default ' ',
  "AAABJARB" varchar(30) NOT NULL default ' ',
  "AAABJAUE" varchar(3) NOT NULL default ' ',
  "AAABJARD" varchar(25) NOT NULL default ' ',
  "AAABJARE" varchar(1) NOT NULL default ' ',
  "AAABJARF" varchar(1) NOT NULL default ' ',
  "AAABJARG" varchar(1) NOT NULL default ' ',
  "AAABJARH" varchar(1) NOT NULL default ' ',
  "AAABJARI" varchar(1) NOT NULL default ' ',
  "AAABJARJ" varchar(1) NOT NULL default ' ',
  "AAABJARK" varchar(1) NOT NULL default ' ',
  "AAABJARL" varchar(18) NOT NULL default ' ',
  "AAABJARM" varchar(18) NOT NULL default ' ',
  "AAABJTAJ" varchar(6) NOT NULL default '000000',
  "AAABJARO" varchar(6) NOT NULL default ' ',
  "AAABJARP" varchar(10) NOT NULL default ' ',
  "AAABJARQ" varchar(10) NOT NULL default ' ',
  "AAABJARR" varchar(10) NOT NULL default ' ',
  "AAABJARS" varchar(10) NOT NULL default ' ',
  "AAABLYRW" varchar(6) NOT NULL default ' ',
  "AAABLOZR" varchar(8) NOT NULL default '00000000',
  "AAABLOZS" varchar(8) NOT NULL default '00000000',
  "AAABJARW" varchar(8) NOT NULL default '00000000',
  "AAABJARX" varchar(8) NOT NULL default '00000000',
  "AAABJARY" varchar(10) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKM" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJASZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJASH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJASZ" varchar(10) NOT NULL default ' ',
  "AAABJATA" varchar(3) NOT NULL default '000',
  "AAABJASL" varchar(3) NOT NULL default '000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  "AAABJASO" varchar(25) NOT NULL default ' ',
  "AAABJATL" decimal(11,0) NOT NULL default '0',
  "AAABJATM" decimal(11,0) NOT NULL default '0',
  "AAABJATN" decimal(11,0) NOT NULL default '0',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABJASU" varchar(2) NOT NULL default ' ',
  "AAABMAKT" varchar(10) NOT NULL default '0000000000',
  "AAABLXWL" varchar(25) NOT NULL default ' ',
  "AAABJASX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJASZ","AAABJATA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJATB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJAUE" varchar(3) NOT NULL default ' ',
  "AAABJATE" varchar(1) NOT NULL default ' ',
  "AAABJATF" varchar(1) NOT NULL default ' ',
  "AAABLZNE" varchar(2) NOT NULL default ' ',
  "AAABJATH" varchar(1) NOT NULL default ' ',
  "AAABJATI" varchar(1) NOT NULL default ' ',
  "AAABJATJ" varchar(1) NOT NULL default ' ',
  "AAABJATK" varchar(3) NOT NULL default ' ',
  "AAABJATL" decimal(11,0) NOT NULL default '0',
  "AAABJATM" decimal(11,0) NOT NULL default '0',
  "AAABJATN" decimal(11,0) NOT NULL default '0',
  "AAABJATO" varchar(1) NOT NULL default ' ',
  "AAABJATP" varchar(5) NOT NULL default '00000',
  "AAABJATQ" varchar(2) NOT NULL default ' ',
  "AAABJATR" varchar(2) NOT NULL default ' ',
  "AAABJATS" varchar(4) NOT NULL default ' ',
  "AAABJATT" varchar(2) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJAUE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJATY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJAUE" varchar(3) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJAUC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJAUE","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAUG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABLZNI" varchar(4) NOT NULL default ' ',
  "AAABJIOZ" varchar(3) NOT NULL default ' ',
  "AAABJIPA" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAUO" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMMVO" varchar(20) NOT NULL default ' ',
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABLRFA" varchar(6) NOT NULL default ' ',
  "AAABLYCI" varchar(2) NOT NULL default ' ',
  "AAABJAUV" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMONG","AAABMMVH","AAABMMVO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAUZ" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAABMNMP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABLTKC","AAABLXDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAVH" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABMNMP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABMNMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAVM" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMNMP" varchar(3) NOT NULL default ' ',
  "AAABMAHQ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABMRHS","AAABMNMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAVU" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBLA" varchar(12) NOT NULL default ' ',
  "AAABJBLK" varchar(20) NOT NULL default ' ',
  "AAABMIUX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBLA","AAABJBLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAWC" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBUJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBUJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAWH" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJBUJ" varchar(2) NOT NULL default ' ',
  "AAABJBGR" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABMRHS","AAABJBUJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAWP" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJAXB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJAXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAWU" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJAXB" varchar(4) NOT NULL default ' ',
  "AAABJBVB" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABMRHS","AAABJAXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAXC" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJQZV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJQZV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAXH" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBUS" varchar(8) NOT NULL default ' ',
  "AAABJBSC" varchar(10) NOT NULL default ' ',
  "AAABJBSE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBUS"),
  UNIQUE KEY "AAABJAXO" ("AAABMONG","AAABJBSC","AAABJBSE","AAABJBUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAXT" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJBUS" varchar(8) NOT NULL default ' ',
  "AAABJBGR" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABMRHS","AAABJBUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAYB" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBKU" varchar(10) NOT NULL default ' ',
  "AAABJBPT" varchar(2) NOT NULL default ' ',
  "AAABJBKW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAYI" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJBKU" varchar(10) NOT NULL default ' ',
  "AAABJBGR" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABMRHS","AAABJBKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAYQ" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBVZ" varchar(4) NOT NULL default ' ',
  "AAABJBUS" varchar(8) NOT NULL default ' ',
  "AAABMCFC" varchar(4) NOT NULL default ' ',
  "AAABJBUT" varchar(5) NOT NULL default ' ',
  "AAABJBUJ" varchar(2) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABKZNH" varchar(2) NOT NULL default ' ',
  "AAABKZND" varchar(10) NOT NULL default ' ',
  "AAABJNWN" varchar(2) NOT NULL default ' ',
  "AAABKZNG" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBVZ","AAABJBUS","AAABMCFC","AAABJBUT","AAABJBUJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAZI" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBSC" varchar(10) NOT NULL default ' ',
  "AAABJAZL" varchar(1) NOT NULL default ' ',
  "AAABJAZM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBSC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAZP" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJBSC" varchar(10) NOT NULL default ' ',
  "AAABJBGR" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABMRHS","AAABJBSC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJAZX" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBLK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBAC" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBEO" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBAH" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJBEO" varchar(6) NOT NULL default ' ',
  "AAABJBGR" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABMRHS","AAABJBEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBAP" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBAU" varchar(1) NOT NULL default '0',
  "AAABJBAS" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBAU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBAV" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBJD" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBBA" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJBJD" varchar(3) NOT NULL default ' ',
  "AAABJBGR" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABMRHS","AAABJBJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBBI" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBVZ" varchar(4) NOT NULL default ' ',
  "AAABJBJH" varchar(10) NOT NULL default ' ',
  "AAABJPMO" varchar(1) NOT NULL default '0',
  "AAABJBBU" varchar(8) NOT NULL default '00000000',
  "AAABJBRG" decimal(15,10) NOT NULL default '0.0000000000',
  "AAABLMFE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBVZ","AAABJBJH","AAABJPMO","AAABJBBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBBV" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBSC" varchar(10) NOT NULL default ' ',
  "AAABJBSE" varchar(4) NOT NULL default ' ',
  "AAABJBQH" varchar(4) NOT NULL default ' ',
  "AAABJBPT" varchar(2) NOT NULL default ' ',
  "AAABJBCB" varchar(1) NOT NULL default ' ',
  "AAABJBCC" varchar(4) NOT NULL default ' ',
  "AAABJBCD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBSC","AAABJBSE","AAABJBQH","AAABJBPT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBCJ" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABMCFC" varchar(4) NOT NULL default ' ',
  "AAABJBVE" varchar(1) NOT NULL default ' ',
  "AAABJBVF" varchar(1) NOT NULL default ' ',
  "AAABJBUQ" varchar(1) NOT NULL default ' ',
  "AAABJBUP" varchar(2) NOT NULL default ' ',
  "AAABJBUO" varchar(2) NOT NULL default ' ',
  "AAABJBPT" varchar(2) NOT NULL default ' ',
  "AAABJBCS" varchar(4) NOT NULL default ' ',
  "AAABJBCT" varchar(4) NOT NULL default ' ',
  "AAABJBCU" varchar(4) NOT NULL default ' ',
  "AAABJBCV" varchar(4) NOT NULL default ' ',
  "AAABJBCW" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMONG","AAABMCFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBCZ" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCFC" varchar(4) NOT NULL default ' ',
  "AAABJBGR" varchar(50) NOT NULL default ' ',
  "AAABMCNE" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABMRHS","AAABMCFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBDI" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBLA" varchar(12) NOT NULL default ' ',
  "AAABJBDL" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBLA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBDO" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBVZ" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJBDS" varchar(1) NOT NULL default ' ',
  "AAABJBKY" varchar(5) NOT NULL default ' ',
  "AAABJBLQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBDX" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJBVZ" varchar(4) NOT NULL default ' ',
  "AAABJBGR" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABMRHS","AAABJBVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBEF" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBGU" varchar(10) NOT NULL default ' ',
  "AAABJBEO" varchar(6) NOT NULL default ' ',
  "AAABLQTM" varchar(1) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABJBEL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBGU","AAABJBEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBEP" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBVZ" varchar(4) NOT NULL default ' ',
  "AAABJBSC" varchar(10) NOT NULL default ' ',
  "AAABJBFA" varchar(2) NOT NULL default ' ',
  "AAABJBUJ" varchar(2) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABJBEW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBVZ","AAABJBSC","AAABJBFA","AAABJBUJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBFC" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABMCFC" varchar(4) NOT NULL default ' ',
  "AAABJBJX" varchar(1) NOT NULL default ' ',
  "AAABJBFL" varchar(1) NOT NULL default ' ',
  "AAABJBFH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABMCFC","AAABJBJX","AAABJBFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBFM" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBPT" varchar(2) NOT NULL default ' ',
  "AAABJBLB" varchar(3) NOT NULL default '000',
  "AAABJBFQ" varchar(1) NOT NULL default ' ',
  "AAABJBFR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBPT","AAABJBLB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBFV" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBLB" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMONG","AAABJBLB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBGA" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJBLB" varchar(3) NOT NULL default '000',
  "AAABJBGR" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABMRHS","AAABJBLB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBGI" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBGU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBGU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBGN" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJBGU" varchar(10) NOT NULL default ' ',
  "AAABJBGR" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABMRHS","AAABJBGU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBGV" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBVZ" varchar(4) NOT NULL default ' ',
  "AAABJBLQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBVZ","AAABJBLQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBHC" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBSC" varchar(10) NOT NULL default ' ',
  "AAABJBSE" varchar(4) NOT NULL default ' ',
  "AAABJBQH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBSC","AAABJBSE","AAABJBQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBHL" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBVZ" varchar(4) NOT NULL default ' ',
  "AAABJBUJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBVZ","AAABJBUJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBHS" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBSC" varchar(10) NOT NULL default ' ',
  "AAABKVKH" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBSC","AAABKVKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBHZ" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBPT" varchar(2) NOT NULL default ' ',
  "AAABJBUS" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBPT","AAABJBUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBIG" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBSC" varchar(10) NOT NULL default ' ',
  "AAABJBSE" varchar(4) NOT NULL default ' ',
  "AAABJBJX" varchar(1) NOT NULL default ' ',
  "AAABMCFC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBSC","AAABJBSE","AAABJBJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBIQ" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBSC" varchar(10) NOT NULL default ' ',
  "AAABJBVZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBSC","AAABJBVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBIX" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBVZ" varchar(4) NOT NULL default ' ',
  "AAABJBSC" varchar(10) NOT NULL default ' ',
  "AAABJBJH" varchar(10) NOT NULL default ' ',
  "AAABJBKU" varchar(10) NOT NULL default ' ',
  "AAABJBJD" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBVZ","AAABJBSC","AAABJBJH","AAABJBKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBJJ" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBSC" varchar(10) NOT NULL default ' ',
  "AAABJBSE" varchar(4) NOT NULL default ' ',
  "AAABJBQH" varchar(4) NOT NULL default ' ',
  "AAABJBQP" varchar(1) NOT NULL default ' ',
  "AAABJBJX" varchar(1) NOT NULL default ' ',
  "AAABJBPT" varchar(2) NOT NULL default ' ',
  "AAABMCFC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBSC","AAABJBSE","AAABJBQH","AAABJBQP","AAABJBJX","AAABJBPT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBJZ" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBUS" varchar(8) NOT NULL default ' ',
  "AAABJBQR" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBUS","AAABJBQR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBKG" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBSC" varchar(10) NOT NULL default ' ',
  "AAABJBSE" varchar(4) NOT NULL default ' ',
  "AAABJBVZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBSC","AAABJBSE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBKO" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBVZ" varchar(4) NOT NULL default ' ',
  "AAABJQXQ" varchar(13) NOT NULL default ' ',
  "AAABJBKS" varchar(60) NOT NULL default ' ',
  "AAABJBKT" varchar(1) NOT NULL default ' ',
  "AAABJBKU" varchar(10) NOT NULL default ' ',
  "AAABJBPT" varchar(2) NOT NULL default ' ',
  "AAABJBKW" varchar(1) NOT NULL default ' ',
  "AAABJBLQ" varchar(2) NOT NULL default ' ',
  "AAABJBKY" varchar(5) NOT NULL default ' ',
  "AAABLQCR" varchar(35) NOT NULL default ' ',
  "AAABJBLA" varchar(12) NOT NULL default ' ',
  "AAABJBLB" varchar(3) NOT NULL default '000',
  "AAABJBLC" varchar(3) NOT NULL default '000',
  "AAABJBVE" varchar(1) NOT NULL default ' ',
  "AAABLUQG" varchar(1) NOT NULL default ' ',
  "AAABJBLF" varchar(8) NOT NULL default '00000000',
  "AAABJBSC" varchar(10) NOT NULL default ' ',
  "AAABJBVK" varchar(18) NOT NULL default ' ',
  "AAABJBVL" varchar(18) NOT NULL default ' ',
  "AAABJBVM" varchar(18) NOT NULL default ' ',
  "AAABJBLK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBVZ","AAABJQXQ"),
  KEY "AAABJBLO" ("AAABMONG","AAABJBLQ","AAABLQCR","AAABJBSC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBLT" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBVZ" varchar(4) NOT NULL default ' ',
  "AAABJQXQ" varchar(13) NOT NULL default ' ',
  "AAABJBSC" varchar(10) NOT NULL default ' ',
  "AAABJBSD" varchar(35) NOT NULL default ' ',
  "AAABJBQY" varchar(5) NOT NULL default ' ',
  "AAABJBQZ" decimal(23,2) NOT NULL default '0.00',
  "AAABJBQX" varchar(8) NOT NULL default '00000000',
  "AAABJBSE" varchar(4) NOT NULL default ' ',
  "AAABJBQR" varchar(8) NOT NULL default ' ',
  "AAABJBME" varchar(1) NOT NULL default ' ',
  "AAABJBMF" varchar(1) NOT NULL default ' ',
  "AAABJBMG" varchar(1) NOT NULL default ' ',
  "AAABJBVK" varchar(18) NOT NULL default ' ',
  "AAABJBVL" varchar(18) NOT NULL default ' ',
  "AAABJBVM" varchar(18) NOT NULL default ' ',
  "AAABLDAZ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMONG","AAABJBVZ","AAABJQXQ","AAABJBSC","AAABJBSD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBMQ" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBRU" varchar(10) NOT NULL default '0000000000',
  "AAABJBUJ" varchar(2) NOT NULL default ' ',
  "AAABJBVZ" varchar(4) NOT NULL default ' ',
  "AAABJQXQ" varchar(13) NOT NULL default ' ',
  "AAABJBPT" varchar(2) NOT NULL default ' ',
  "AAABJBSC" varchar(10) NOT NULL default ' ',
  "AAABJBSD" varchar(35) NOT NULL default ' ',
  "AAABJBPW" varchar(12) NOT NULL default ' ',
  "AAABJBPX" varchar(3) NOT NULL default ' ',
  "AAABJBPY" varchar(10) NOT NULL default ' ',
  "AAABJBPZ" varchar(4) NOT NULL default ' ',
  "AAABJBQA" varchar(1) NOT NULL default ' ',
  "AAABJBQB" varchar(10) NOT NULL default '0000000000',
  "AAABJBSL" varchar(10) NOT NULL default '0000000000',
  "AAABJBUM" varchar(10) NOT NULL default '0000000000',
  "AAABJBSN" varchar(1) NOT NULL default ' ',
  "AAABJBQF" varchar(1) NOT NULL default ' ',
  "AAABJBSE" varchar(4) NOT NULL default ' ',
  "AAABJBQH" varchar(4) NOT NULL default ' ',
  "AAABJBQI" varchar(4) NOT NULL default '0000',
  "AAABMCFC" varchar(4) NOT NULL default ' ',
  "AAABJBUO" varchar(2) NOT NULL default ' ',
  "AAABJBUP" varchar(2) NOT NULL default ' ',
  "AAABJBUQ" varchar(1) NOT NULL default ' ',
  "AAABJBUR" varchar(4) NOT NULL default '0000',
  "AAABJBUS" varchar(8) NOT NULL default ' ',
  "AAABJBQP" varchar(1) NOT NULL default ' ',
  "AAABJBQQ" varchar(10) NOT NULL default ' ',
  "AAABJBQR" varchar(8) NOT NULL default ' ',
  "AAABJBQS" varchar(8) NOT NULL default '00000000',
  "AAABJBQT" varchar(8) NOT NULL default '00000000',
  "AAABJBQU" varchar(1) NOT NULL default ' ',
  "AAABJBQV" varchar(5) NOT NULL default ' ',
  "AAABJBQW" decimal(23,2) NOT NULL default '0.00',
  "AAABJBQX" varchar(8) NOT NULL default '00000000',
  "AAABJBQY" varchar(5) NOT NULL default ' ',
  "AAABJBQZ" decimal(23,2) NOT NULL default '0.00',
  "AAABJBUW" varchar(5) NOT NULL default ' ',
  "AAABLBXX" decimal(23,2) NOT NULL default '0.00',
  "AAABMPSW" decimal(9,5) NOT NULL default '0.00000',
  "AAABJBUY" decimal(15,10) NOT NULL default '0.0000000000',
  "AAABJBUZ" decimal(23,2) NOT NULL default '0.00',
  "AAABJPMO" varchar(1) NOT NULL default '0',
  "AAABJBRG" decimal(15,10) NOT NULL default '0.0000000000',
  "AAABJBVE" varchar(1) NOT NULL default ' ',
  "AAABJBVK" varchar(18) NOT NULL default ' ',
  "AAABJBVL" varchar(18) NOT NULL default ' ',
  "AAABJBVM" varchar(18) NOT NULL default ' ',
  "AAABJBVN" varchar(12) NOT NULL default ' ',
  "AAABLVJK" varchar(8) NOT NULL default '00000000',
  "AAABJBVP" varchar(6) NOT NULL default '000000',
  "AAABJBVQ" varchar(20) NOT NULL default ' ',
  "AAABJBVR" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABJBVT" varchar(6) NOT NULL default '000000',
  "AAABJBVU" varchar(20) NOT NULL default ' ',
  "AAABJBOW" varchar(10) NOT NULL default ' ',
  "AAABJBOX" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMONG","AAABJBRU"),
  KEY "AAABJBPA" ("AAABMONG","AAABJBVZ","AAABJQXQ"),
  KEY "AAABJBPE" ("AAABMONG","AAABJBSC","AAABJBSD","AAABJBSE"),
  KEY "AAABJBPJ" ("AAABMONG","AAABJBUM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBPM" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBRU" varchar(10) NOT NULL default '0000000000',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABJBUJ" varchar(2) NOT NULL default ' ',
  "AAABJBVZ" varchar(4) NOT NULL default ' ',
  "AAABJQXQ" varchar(13) NOT NULL default ' ',
  "AAABJBPT" varchar(2) NOT NULL default ' ',
  "AAABJBSC" varchar(10) NOT NULL default ' ',
  "AAABJBSD" varchar(35) NOT NULL default ' ',
  "AAABJBPW" varchar(12) NOT NULL default ' ',
  "AAABJBPX" varchar(3) NOT NULL default ' ',
  "AAABJBPY" varchar(10) NOT NULL default ' ',
  "AAABJBPZ" varchar(4) NOT NULL default ' ',
  "AAABJBQA" varchar(1) NOT NULL default ' ',
  "AAABJBQB" varchar(10) NOT NULL default '0000000000',
  "AAABJBSL" varchar(10) NOT NULL default '0000000000',
  "AAABJBUM" varchar(10) NOT NULL default '0000000000',
  "AAABJBSN" varchar(1) NOT NULL default ' ',
  "AAABJBQF" varchar(1) NOT NULL default ' ',
  "AAABJBSE" varchar(4) NOT NULL default ' ',
  "AAABJBQH" varchar(4) NOT NULL default ' ',
  "AAABJBQI" varchar(4) NOT NULL default '0000',
  "AAABMCFC" varchar(4) NOT NULL default ' ',
  "AAABJBUO" varchar(2) NOT NULL default ' ',
  "AAABJBUP" varchar(2) NOT NULL default ' ',
  "AAABJBUQ" varchar(1) NOT NULL default ' ',
  "AAABJBUR" varchar(4) NOT NULL default '0000',
  "AAABJBUS" varchar(8) NOT NULL default ' ',
  "AAABJBQP" varchar(1) NOT NULL default ' ',
  "AAABJBQQ" varchar(10) NOT NULL default ' ',
  "AAABJBQR" varchar(8) NOT NULL default ' ',
  "AAABJBQS" varchar(8) NOT NULL default '00000000',
  "AAABJBQT" varchar(8) NOT NULL default '00000000',
  "AAABJBQU" varchar(1) NOT NULL default ' ',
  "AAABJBQV" varchar(5) NOT NULL default ' ',
  "AAABJBQW" decimal(23,2) NOT NULL default '0.00',
  "AAABJBQX" varchar(8) NOT NULL default '00000000',
  "AAABJBQY" varchar(5) NOT NULL default ' ',
  "AAABJBQZ" decimal(23,2) NOT NULL default '0.00',
  "AAABJBUW" varchar(5) NOT NULL default ' ',
  "AAABLBXX" decimal(23,2) NOT NULL default '0.00',
  "AAABMPSW" decimal(9,5) NOT NULL default '0.00000',
  "AAABJBUY" decimal(15,10) NOT NULL default '0.0000000000',
  "AAABJBUZ" decimal(23,2) NOT NULL default '0.00',
  "AAABJPMO" varchar(1) NOT NULL default '0',
  "AAABJBRG" decimal(15,10) NOT NULL default '0.0000000000',
  "AAABJBVE" varchar(1) NOT NULL default ' ',
  "AAABJBVK" varchar(18) NOT NULL default ' ',
  "AAABJBVL" varchar(18) NOT NULL default ' ',
  "AAABJBVM" varchar(18) NOT NULL default ' ',
  "AAABJBVN" varchar(12) NOT NULL default ' ',
  "AAABLVJK" varchar(8) NOT NULL default '00000000',
  "AAABJBVP" varchar(6) NOT NULL default '000000',
  "AAABJBVQ" varchar(20) NOT NULL default ' ',
  "AAABJBVR" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABJBVT" varchar(6) NOT NULL default '000000',
  "AAABJBVU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBRU","AAABMKFU"),
  KEY "AAABJBRW" ("AAABMONG","AAABJBVZ","AAABJQXQ"),
  KEY "AAABJBSA" ("AAABMONG","AAABJBSC","AAABJBSD","AAABJBSE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBSF" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBUB" varchar(10) NOT NULL default '0000000000',
  "AAABJBVZ" varchar(4) NOT NULL default ' ',
  "AAABJQXQ" varchar(13) NOT NULL default ' ',
  "AAABJBUJ" varchar(2) NOT NULL default ' ',
  "AAABJBSL" varchar(10) NOT NULL default '0000000000',
  "AAABJBUM" varchar(10) NOT NULL default '0000000000',
  "AAABJBSN" varchar(1) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCMR" varchar(2) NOT NULL default '00',
  "AAABMPVB" varchar(3) NOT NULL default '000',
  "AAABJBVB" varchar(50) NOT NULL default ' ',
  "AAABLADP" varchar(8) NOT NULL default '00000000',
  "AAABJBVC" varchar(50) NOT NULL default ' ',
  "AAABJBVE" varchar(1) NOT NULL default ' ',
  "AAABJBVF" varchar(1) NOT NULL default ' ',
  "AAABJBUS" varchar(8) NOT NULL default ' ',
  "AAABMCFC" varchar(4) NOT NULL default ' ',
  "AAABJBUO" varchar(2) NOT NULL default ' ',
  "AAABJBUP" varchar(2) NOT NULL default ' ',
  "AAABJBUQ" varchar(1) NOT NULL default ' ',
  "AAABJBTB" varchar(4) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABKZNH" varchar(2) NOT NULL default ' ',
  "AAABKZND" varchar(10) NOT NULL default ' ',
  "AAABJNWN" varchar(2) NOT NULL default ' ',
  "AAABKZNG" varchar(10) NOT NULL default ' ',
  "AAABJBUT" varchar(5) NOT NULL default ' ',
  "AAABJBUU" decimal(23,2) NOT NULL default '0.00',
  "AAABJBUW" varchar(5) NOT NULL default ' ',
  "AAABLBXX" decimal(23,2) NOT NULL default '0.00',
  "AAABJBUY" decimal(15,10) NOT NULL default '0.0000000000',
  "AAABJBUZ" decimal(23,2) NOT NULL default '0.00',
  "AAABMPSW" decimal(9,5) NOT NULL default '0.00000',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABJBVK" varchar(18) NOT NULL default ' ',
  "AAABJBVL" varchar(18) NOT NULL default ' ',
  "AAABJBVM" varchar(18) NOT NULL default ' ',
  "AAABJBVN" varchar(12) NOT NULL default ' ',
  "AAABLVJK" varchar(8) NOT NULL default '00000000',
  "AAABJBVP" varchar(6) NOT NULL default '000000',
  "AAABJBVQ" varchar(20) NOT NULL default ' ',
  "AAABJBVR" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABJBVT" varchar(6) NOT NULL default '000000',
  "AAABJBVU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBUB"),
  KEY "AAABJBUC" ("AAABMONG","AAABJBVZ","AAABJQXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBUG" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJBVW" varchar(10) NOT NULL default '0000000000',
  "AAABJBUJ" varchar(2) NOT NULL default ' ',
  "AAABJBVZ" varchar(4) NOT NULL default ' ',
  "AAABJQXQ" varchar(13) NOT NULL default ' ',
  "AAABJBUM" varchar(10) NOT NULL default '0000000000',
  "AAABMCFC" varchar(4) NOT NULL default ' ',
  "AAABJBUO" varchar(2) NOT NULL default ' ',
  "AAABJBUP" varchar(2) NOT NULL default ' ',
  "AAABJBUQ" varchar(1) NOT NULL default ' ',
  "AAABJBUR" varchar(4) NOT NULL default '0000',
  "AAABJBUS" varchar(8) NOT NULL default ' ',
  "AAABJBUT" varchar(5) NOT NULL default ' ',
  "AAABJBUU" decimal(23,2) NOT NULL default '0.00',
  "AAABJBUV" decimal(23,2) NOT NULL default '0.00',
  "AAABJBUW" varchar(5) NOT NULL default ' ',
  "AAABLBXX" decimal(23,2) NOT NULL default '0.00',
  "AAABJBUY" decimal(15,10) NOT NULL default '0.0000000000',
  "AAABJBUZ" decimal(23,2) NOT NULL default '0.00',
  "AAABMPSW" decimal(9,5) NOT NULL default '0.00000',
  "AAABJBVB" varchar(50) NOT NULL default ' ',
  "AAABJBVC" varchar(50) NOT NULL default ' ',
  "AAABLADP" varchar(8) NOT NULL default '00000000',
  "AAABJBVE" varchar(1) NOT NULL default ' ',
  "AAABJBVF" varchar(1) NOT NULL default ' ',
  "AAABJBVG" varchar(8) NOT NULL default '00000000',
  "AAABJBVH" varchar(4) NOT NULL default '0000',
  "AAABJBVI" varchar(8) NOT NULL default '00000000',
  "AAABJBVJ" varchar(4) NOT NULL default '0000',
  "AAABJBVK" varchar(18) NOT NULL default ' ',
  "AAABJBVL" varchar(18) NOT NULL default ' ',
  "AAABJBVM" varchar(18) NOT NULL default ' ',
  "AAABJBVN" varchar(12) NOT NULL default ' ',
  "AAABLVJK" varchar(8) NOT NULL default '00000000',
  "AAABJBVP" varchar(6) NOT NULL default '000000',
  "AAABJBVQ" varchar(20) NOT NULL default ' ',
  "AAABJBVR" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABJBVT" varchar(6) NOT NULL default '000000',
  "AAABJBVU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJBVW"),
  KEY "AAABJBVX" ("AAABMONG","AAABJBVZ","AAABJQXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBWB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDJD" varchar(3) NOT NULL default ' ',
  "AAABLRSJ" varchar(1) NOT NULL default ' ',
  "AAABJBWZ" varchar(1) NOT NULL default ' ',
  "AAABJBWG" varchar(1) NOT NULL default ' ',
  "AAABJCLY" varchar(1) NOT NULL default ' ',
  "AAABJCNN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDJD","AAABLRSJ","AAABJBWZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBWN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJDJD" varchar(3) NOT NULL default ' ',
  "AAABLRSJ" varchar(1) NOT NULL default ' ',
  "AAABJBWZ" varchar(1) NOT NULL default ' ',
  "AAABJBWT" varchar(8) NOT NULL default ' ',
  "AAABJBWU" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJDJD","AAABLRSJ","AAABJBWZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBXA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCTT" varchar(1) NOT NULL default ' ',
  "AAABJBXD" varchar(8) NOT NULL default ' ',
  "AAABJBXE" varchar(8) NOT NULL default ' ',
  "AAABJBXF" varchar(1) NOT NULL default ' ',
  "AAABJCNN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJCTT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBXJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCTT" varchar(1) NOT NULL default ' ',
  "AAABJDFU" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJCTT","AAABJDFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBXQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJCTT" varchar(1) NOT NULL default ' ',
  "AAABJBXU" varchar(8) NOT NULL default ' ',
  "AAABJBXV" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJCTT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBXZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABJCDT" varchar(8) NOT NULL default ' ',
  "AAABJCDZ" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  "AAABJCNN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP","AAABJCDT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBYJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJBYW" varchar(1) NOT NULL default ' ',
  "AAABJCNN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJBYW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBYP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJBYW" varchar(1) NOT NULL default ' ',
  "AAABJBYT" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJBYW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBYX" (
  "AAABJCEL" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABJCEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABJCEL" varchar(3) NOT NULL default ' ',
  "AAABMREL" varchar(20) NOT NULL default ' ',
  "AAABJCFQ" varchar(1) NOT NULL default ' ',
  "AAABJBZG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP","AAABJCEL","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBZL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMREL" varchar(20) NOT NULL default ' ',
  "AAABJDFT" varchar(1) NOT NULL default ' ',
  "AAABJDFU" varchar(3) NOT NULL default ' ',
  "AAABJDMQ" varchar(2) NOT NULL default ' ',
  "AAABJDMT" varchar(2) NOT NULL default ' ',
  "AAABJCFQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJBZX" (
  "AAABJCAB" varchar(20) NOT NULL default ' ',
  "AAABLILW" varchar(8) NOT NULL default ' ',
  "AAABJCAA" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABJCAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCAC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCBC" varchar(20) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJDFU" varchar(3) NOT NULL default ' ',
  "AAABJDFT" varchar(1) NOT NULL default ' ',
  "AAABJDGB" varchar(2) NOT NULL default ' ',
  "AAABMLPO" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABJCBC","AAABLHPM","AAABJDFU","AAABJDFT","AAABJDGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCAQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJCBC" varchar(20) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJDFU" varchar(3) NOT NULL default ' ',
  "AAABJDFT" varchar(1) NOT NULL default ' ',
  "AAABJDGB" varchar(2) NOT NULL default ' ',
  "AAABJCAY" varchar(8) NOT NULL default ' ',
  "AAABJCAZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJCBC","AAABLHPM","AAABJDFU","AAABJDFT","AAABJDGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCBH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCEL" varchar(3) NOT NULL default ' ',
  "AAABJCBQ" varchar(1) NOT NULL default '0',
  "AAABJCBR" varchar(26) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABJCBT" varchar(26) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJCEL","AAABJCBQ","AAABJCBR","AAABMGHE","AAABJCBT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCBU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABJCCC" varchar(3) NOT NULL default ' ',
  "AAABJCFQ" varchar(1) NOT NULL default ' ',
  "AAABJCDZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP","AAABJCCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCCD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCDL" varchar(3) NOT NULL default ' ',
  "AAABJCCG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJCDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCCJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABJCDL" varchar(3) NOT NULL default ' ',
  "AAABJCCN" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMDEP","AAABJCDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCCR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABJCDT" varchar(8) NOT NULL default ' ',
  "AAABJCDZ" varchar(1) NOT NULL default ' ',
  "AAABJCNN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP","AAABJCDT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCDA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCDT" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJCDT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCDL" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJCDJ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJCDL","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCDN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCDT" varchar(8) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJCDT","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCDV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABJCTT" varchar(1) NOT NULL default ' ',
  "AAABJCDZ" varchar(1) NOT NULL default ' ',
  "AAABJCFQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP","AAABJCTT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCEE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABJCEL" varchar(3) NOT NULL default ' ',
  "AAABJCFH" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMDEP","AAABJCEL","AAABJCFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCEN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCFH" varchar(4) NOT NULL default '0000',
  "AAABJCEQ" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJCFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCET" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCFA" varchar(4) NOT NULL default ' ',
  "AAABJCEW" varchar(3) NOT NULL default '000',
  "AAABJCEX" varchar(3) NOT NULL default '000',
  "AAABJCEY" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABJCFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCFB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCFH" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJCFH","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCFJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABJCFN" varchar(4) NOT NULL default ' ',
  "AAABLOJX" varchar(4) NOT NULL default ' ',
  "AAABJCFP" varchar(1) NOT NULL default ' ',
  "AAABJCFQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCFT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDJD" varchar(3) NOT NULL default ' ',
  "AAABLRSJ" varchar(1) NOT NULL default ' ',
  "AAABJCJJ" varchar(3) NOT NULL default ' ',
  "AAABJCGT" varchar(3) NOT NULL default ' ',
  "AAABJCNN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDJD","AAABLRSJ","AAABJCJJ","AAABJCGT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCGF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJDJD" varchar(3) NOT NULL default ' ',
  "AAABLRSJ" varchar(1) NOT NULL default ' ',
  "AAABJCJJ" varchar(3) NOT NULL default ' ',
  "AAABJCGT" varchar(3) NOT NULL default ' ',
  "AAABJCGM" varchar(8) NOT NULL default ' ',
  "AAABJCGN" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJDJD","AAABLRSJ","AAABJCJJ","AAABJCGT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCGU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCIE" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCEY" varchar(10) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABLVGA" varchar(4) NOT NULL default '0000',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABKROQ" varchar(10) NOT NULL default ' ',
  "AAABKROR" varchar(6) NOT NULL default '000000',
  "AAABMCIM" varchar(10) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABJCIQ" varchar(4) NOT NULL default '0000',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABJCHM" varchar(1) NOT NULL default ' ',
  "AAABMBFP" varchar(1) NOT NULL default ' ',
  "AAABLXSU" varchar(1) NOT NULL default ' ',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABJCHR" varchar(1) NOT NULL default ' ',
  "AAABJCHS" varchar(1) NOT NULL default ' ',
  "AAABJCHT" varchar(8) NOT NULL default '00000000',
  "AAABJSIR" varchar(8) NOT NULL default '00000000',
  "AAABMCGA" varchar(4) NOT NULL default ' ',
  "AAABJCHW" varchar(18) NOT NULL default ' ',
  "AAABJCHX" varchar(10) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABJCHZ" varchar(15) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABLUFX" varchar(1) NOT NULL default ' ',
  "AAABKQPE" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABJCIE","AAABMCVI","AAABMLCZ","AAABMCEY","AAABMCRM","AAABLVGA","AAABMCVP","AAABMCVQ","AAABKROQ","AAABKROR","AAABMCIM","AAABLTXI","AAABJCIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCIR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCNM" varchar(1) NOT NULL default ' ',
  "AAABJCJJ" varchar(3) NOT NULL default ' ',
  "AAABJCNN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJCNM","AAABJCJJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCIZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJCNM" varchar(1) NOT NULL default ' ',
  "AAABJCJJ" varchar(3) NOT NULL default ' ',
  "AAABJCJE" varchar(8) NOT NULL default ' ',
  "AAABJCJF" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJCNM","AAABJCJJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCJK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCNM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJCNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCJP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJCNM" varchar(1) NOT NULL default ' ',
  "AAABJCJT" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJCNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCJX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDJD" varchar(3) NOT NULL default ' ',
  "AAABLRSJ" varchar(1) NOT NULL default ' ',
  "AAABJCZW" varchar(5) NOT NULL default ' ',
  "AAABJCZD" varchar(32) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABJCKE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDJD","AAABLRSJ","AAABJCZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCKJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJDJD" varchar(3) NOT NULL default ' ',
  "AAABLRSJ" varchar(1) NOT NULL default ' ',
  "AAABJCZW" varchar(5) NOT NULL default ' ',
  "AAABJCKP" varchar(35) NOT NULL default ' ',
  "AAABJCKQ" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJDJD","AAABLRSJ","AAABJCZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCLY" varchar(3) NOT NULL default ' ',
  "AAABJDFU" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJCLY","AAABJDFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCLD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDDE" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDDE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCLI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJDDE" varchar(8) NOT NULL default ' ',
  "AAABJCLV" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJDDE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCLQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJCLY" varchar(3) NOT NULL default ' ',
  "AAABJDFU" varchar(3) NOT NULL default ' ',
  "AAABJCLV" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJCLY","AAABJDFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCMA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCYZ" varchar(1) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABLLIO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJCYZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCMH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMALF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMALF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCMM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMALF" varchar(1) NOT NULL default ' ',
  "AAABJCMQ" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMALF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCMU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCNH" varchar(1) NOT NULL default ' ',
  "AAABJCNN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJCNH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCNA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJCNH" varchar(1) NOT NULL default ' ',
  "AAABJCNE" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJCNH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCNI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDJD" varchar(3) NOT NULL default ' ',
  "AAABLRSJ" varchar(1) NOT NULL default ' ',
  "AAABJCNM" varchar(1) NOT NULL default ' ',
  "AAABJCNN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDJD","AAABLRSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCNR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJDJD" varchar(3) NOT NULL default ' ',
  "AAABLRSJ" varchar(1) NOT NULL default ' ',
  "AAABJCNW" varchar(8) NOT NULL default ' ',
  "AAABJCNX" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJDJD","AAABLRSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCOC" (
  "AAABJCRK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABJCRK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCOF" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJCRK" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABJCRK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCOL" (
  "AAABJCRJ" varchar(4) NOT NULL default ' ',
  "AAABJCRK" varchar(4) NOT NULL default ' ',
  "AAABJCRL" varchar(3) NOT NULL default '000',
  "AAABJCRA" varchar(1) NOT NULL default ' ',
  "AAABJCRB" varchar(1) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABJCRC" varchar(1) NOT NULL default ' ',
  "AAABJCRD" varchar(1) NOT NULL default ' ',
  "AAABJCRE" varchar(1) NOT NULL default ' ',
  "AAABJCRF" varchar(1) NOT NULL default ' ',
  "AAABJCPQ" varchar(30) NOT NULL default ' ',
  "AAABJCPR" varchar(1) NOT NULL default ' ',
  "AAABJCPS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJCRJ","AAABJCRK","AAABJCRL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCRJ" varchar(4) NOT NULL default ' ',
  "AAABJCRK" varchar(4) NOT NULL default ' ',
  "AAABJCRL" varchar(3) NOT NULL default '000',
  "AAABJCRA" varchar(1) NOT NULL default ' ',
  "AAABJCRB" varchar(1) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABJCRC" varchar(1) NOT NULL default ' ',
  "AAABJCRD" varchar(1) NOT NULL default ' ',
  "AAABJCPO" varchar(1) NOT NULL default ' ',
  "AAABJCRF" varchar(1) NOT NULL default ' ',
  "AAABJCPQ" varchar(30) NOT NULL default ' ',
  "AAABJCPR" varchar(1) NOT NULL default ' ',
  "AAABJCPS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJCRJ","AAABJCRK","AAABJCRL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCPX" (
  "AAABJCRK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABJCRK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCQA" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJCRK" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABJCRK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCQG" (
  "AAABJCRJ" varchar(4) NOT NULL default ' ',
  "AAABJCRK" varchar(4) NOT NULL default ' ',
  "AAABJCRL" varchar(3) NOT NULL default '000',
  "AAABJCRA" varchar(1) NOT NULL default ' ',
  "AAABJCRB" varchar(1) NOT NULL default ' ',
  "AAABJCRC" varchar(1) NOT NULL default ' ',
  "AAABJCRD" varchar(1) NOT NULL default ' ',
  "AAABJCRE" varchar(1) NOT NULL default ' ',
  "AAABJCRF" varchar(1) NOT NULL default ' ',
  "AAABJCRG" varchar(30) NOT NULL default ' ',
  "AAABJCRH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJCRJ","AAABJCRK","AAABJCRL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCQV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCRJ" varchar(4) NOT NULL default ' ',
  "AAABJCRK" varchar(4) NOT NULL default ' ',
  "AAABJCRL" varchar(3) NOT NULL default '000',
  "AAABJCRA" varchar(1) NOT NULL default ' ',
  "AAABJCRB" varchar(1) NOT NULL default ' ',
  "AAABJCRC" varchar(1) NOT NULL default ' ',
  "AAABJCRD" varchar(1) NOT NULL default ' ',
  "AAABJCRE" varchar(1) NOT NULL default ' ',
  "AAABJCRF" varchar(1) NOT NULL default ' ',
  "AAABJCRG" varchar(30) NOT NULL default ' ',
  "AAABJCRH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJCRJ","AAABJCRK","AAABJCRL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCRM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABJCZD" varchar(32) NOT NULL default ' ',
  "AAABJCYC" varchar(32) NOT NULL default ' ',
  "AAABLXTF" varchar(18) NOT NULL default ' ',
  "AAABJCXQ" varchar(40) NOT NULL default ' ',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  "AAABJDPL" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABJCZD","AAABJCYC","AAABLXTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCSA" (
  "AAABMNLW" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCSD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCZD" varchar(32) NOT NULL default ' ',
  "AAABJCTK" varchar(6) NOT NULL default '000000',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABKRJU" varchar(8) NOT NULL default '00000000',
  "AAABJCSL" varchar(4) NOT NULL default ' ',
  "AAABJCSM" varchar(10) NOT NULL default ' ',
  "AAABJCSN" varchar(1) NOT NULL default ' ',
  "AAABJCSO" varchar(4) NOT NULL default ' ',
  "AAABJCSP" varchar(10) NOT NULL default ' ',
  "AAABJCSQ" varchar(1) NOT NULL default ' ',
  "AAABJCSR" varchar(1) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAABJCSU" varchar(4) NOT NULL default ' ',
  "AAABJCSV" varchar(32) NOT NULL default ' ',
  "AAABJCSW" varchar(32) NOT NULL default ' ',
  "AAABJXFD" varchar(1) NOT NULL default ' ',
  "AAABJCSY" varchar(3) NOT NULL default '000',
  "AAABJCSZ" varchar(3) NOT NULL default '000',
  "AAABJCTA" varchar(3) NOT NULL default '000',
  "AAABMALF" varchar(1) NOT NULL default ' ',
  "AAABLTYS" decimal(15,0) NOT NULL default '0',
  "AAABLTZM" varchar(8) NOT NULL default '00000000',
  "AAABLTZN" varchar(12) NOT NULL default ' ',
  "AAABLTYV" varchar(8) NOT NULL default '00000000',
  "AAABLTYW" varchar(12) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJCZD","AAABJCTK"),
  KEY "AAABJCTL" ("AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCTN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCZD" varchar(32) NOT NULL default ' ',
  "AAABLTVY" VARBINARY(255) NOT NULL,
  "AAABJCVE" varchar(6) NOT NULL default '000000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABJCTT" varchar(1) NOT NULL default ' ',
  "AAABJCZW" varchar(5) NOT NULL default ' ',
  "AAABJDMI" varchar(8) NOT NULL default '00000000',
  "AAABJCUR" varchar(5) NOT NULL default ' ',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  "AAABJDPL" varchar(8) NOT NULL default '00000000',
  "AAABJDMJ" varchar(124) NOT NULL default ' ',
  "AAABLTYS" decimal(15,0) NOT NULL default '0',
  "AAABLTZM" varchar(8) NOT NULL default '00000000',
  "AAABLTZN" varchar(12) NOT NULL default ' ',
  "AAABLTYV" varchar(8) NOT NULL default '00000000',
  "AAABLTYW" varchar(12) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJCZD","AAABLTVY"(255),"AAABJCVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCUK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCZD" varchar(32) NOT NULL default ' ',
  "AAABLTVY" VARBINARY(255) NOT NULL,
  "AAABJCVE" varchar(6) NOT NULL default '000000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABLQZP" varchar(1) NOT NULL default ' ',
  "AAABJCUR" varchar(5) NOT NULL default ' ',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  "AAABJDPL" varchar(8) NOT NULL default '00000000',
  "AAABJDMJ" varchar(124) NOT NULL default ' ',
  "AAABLTYS" decimal(15,0) NOT NULL default '0',
  "AAABLTZM" varchar(8) NOT NULL default '00000000',
  "AAABLTZN" varchar(12) NOT NULL default ' ',
  "AAABLTYV" varchar(8) NOT NULL default '00000000',
  "AAABLTYW" varchar(12) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJCZD","AAABLTVY"(255),"AAABJCVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCVF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABLTVY" VARBINARY(255) NOT NULL,
  "AAABJDGK" varchar(32) NOT NULL default ' ',
  "AAABJDAS" varchar(4) NOT NULL default ' ',
  "AAABJCVN" varchar(3) NOT NULL default ' ',
  "AAABLUFA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRFQ","AAABMLES","AAABLUNJ","AAABLTVY"(255),"AAABJDGK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCVV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMRFQ","AAABMLES","AAABJFGL","AAABJFHB","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCWI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABJCZD" varchar(32) NOT NULL default ' ',
  "AAABJCYC" varchar(32) NOT NULL default ' ',
  "AAABJCWW" varchar(8) NOT NULL default '00000000',
  "AAABJCXN" decimal(13,3) NOT NULL default '0.000',
  "AAABJCXQ" varchar(40) NOT NULL default ' ',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  "AAABJDPL" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABJCZD","AAABJCYC","AAABJCWW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCWX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJCXW" varchar(32) NOT NULL default ' ',
  "AAABJCYJ" varchar(3) NOT NULL default ' ',
  "AAABJCXY" varchar(32) NOT NULL default ' ',
  "AAABJCXC" varchar(6) NOT NULL default '000000',
  "AAABJCXD" varchar(4) NOT NULL default ' ',
  "AAABJCXE" varchar(4) NOT NULL default ' ',
  "AAABJCYC" varchar(32) NOT NULL default ' ',
  "AAABJCXG" varchar(6) NOT NULL default '000000',
  "AAABJCXH" varchar(4) NOT NULL default ' ',
  "AAABJCXI" varchar(4) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLXTF" varchar(18) NOT NULL default ' ',
  "AAABMLFE" varchar(10) NOT NULL default ' ',
  "AAABKRJU" varchar(8) NOT NULL default '00000000',
  "AAABJCXN" decimal(13,3) NOT NULL default '0.000',
  "AAABJCXO" varchar(10) NOT NULL default ' ',
  "AAABKRZZ" varchar(10) NOT NULL default '0000000000',
  "AAABJCXQ" varchar(40) NOT NULL default ' ',
  "AAABJCYE" varchar(32) NOT NULL default ' ',
  "AAABJCXS" varchar(2) NOT NULL default ' ',
  "AAABJCXT" varchar(10) NOT NULL default ' ',
  "AAABJCXU" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABLUXQ","AAABJCXW"),
  KEY "AAABJCXX" ("AAABJCXY"),
  KEY "AAABJCXZ" ("AAABMLCZ"),
  KEY "AAABJCYB" ("AAABJCYC"),
  KEY "AAABJCYD" ("AAABJCYE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJCYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDAE" varchar(6) NOT NULL default '000000',
  "AAABKZYE" varchar(6) NOT NULL default '000000',
  "AAABJCYJ" varchar(1) NOT NULL default ' ',
  "AAABJCYK" varchar(40) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  "AAABJCYM" varchar(83) NOT NULL default ' ',
  "AAABJCYN" varchar(8) NOT NULL default ' ',
  "AAABMKFH" varchar(50) NOT NULL default ' ',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  "AAABKMJV" varchar(30) NOT NULL default ' ',
  "AAABJCYR" varchar(1) NOT NULL default ' ',
  "AAABMLTD" varchar(4) NOT NULL default '0000',
  "AAABJLKW" varchar(4) NOT NULL default '0000',
  "AAABJCYU" varchar(40) NOT NULL default ' ',
  "AAABJCYV" varchar(5) NOT NULL default '00000',
  "AAABJCYW" varchar(2) NOT NULL default '00',
  "AAABJCYX" varchar(5) NOT NULL default ' ',
  "AAABLUPE" varchar(30) NOT NULL default ' ',
  "AAABJCYZ" varchar(1) NOT NULL default ' ',
  "AAABJCZA" varchar(5) NOT NULL default '00000',
  "AAABLUNJ" varchar(12) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABJCZD" varchar(32) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABJDNJ" VARBINARY(255),
  "AAABJCZG" varchar(8) NOT NULL default '00000000',
  "AAABJCZH" varchar(2) NOT NULL default ' ',
  "AAABMRFO" varchar(3) NOT NULL default ' ',
  "AAABJCZJ" varchar(1) NOT NULL default ' ',
  "AAABJCZK" varchar(9) NOT NULL default ' ',
  "AAABJCZL" varchar(1) NOT NULL default ' ',
  "AAABJCZM" varchar(2) NOT NULL default ' ',
  "AAABJCZN" varchar(2) NOT NULL default ' ',
  "AAABJCZO" varchar(35) NOT NULL default ' ',
  "AAABLYTE" varchar(3) NOT NULL default ' ',
  "AAABJCZQ" varchar(8) NOT NULL default '00000000',
  "AAABJCZR" varchar(2) NOT NULL default ' ',
  "AAABJCZS" varchar(8) NOT NULL default '00000000',
  "AAABJCZT" varchar(1) NOT NULL default ' ',
  "AAABJCZU" varchar(2) NOT NULL default ' ',
  "AAABJCZV" varchar(2) NOT NULL default ' ',
  "AAABJCZW" varchar(5) NOT NULL default ' ',
  "AAABJDNG" VARBINARY(255),
  "AAABJCZY" varchar(1) NOT NULL default ' ',
  "AAABJCZZ" varchar(40) NOT NULL default ' ',
  "AAABJDAA" decimal(7,0) NOT NULL default '0',
  "AAABJDAB" decimal(7,0) NOT NULL default '0',
  "AAABJDAC" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABJDAE","AAABKZYE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDAG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCMG" varchar(2) NOT NULL default '00',
  "AAABJDAN" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCMG","AAABJDAN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDAO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVY" VARBINARY(255) NOT NULL,
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABJDAS" varchar(4) NOT NULL default ' ',
  "AAABLUFA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTVY"(255),"AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDAX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDGK" varchar(32) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABJDFT" varchar(1) NOT NULL default ' ',
  "AAABJDFU" varchar(3) NOT NULL default ' ',
  "AAABJFHC" varchar(1) NOT NULL default ' ',
  "AAABJDFW" varchar(2) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLARI" varchar(6) NOT NULL default '000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABJDPL" varchar(6) NOT NULL default '000000',
  "AAABJDGB" varchar(2) NOT NULL default ' ',
  "AAABJDGC" varchar(45) NOT NULL default ' ',
  "AAABJDGD" varchar(1) NOT NULL default ' ',
  "AAABLTZM" varchar(8) NOT NULL default '00000000',
  "AAABLTZN" varchar(12) NOT NULL default ' ',
  "AAABLTYV" varchar(8) NOT NULL default '00000000',
  "AAABLTYW" varchar(12) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDGK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDBV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDGK" varchar(32) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABJDFT" varchar(1) NOT NULL default ' ',
  "AAABJDFU" varchar(3) NOT NULL default ' ',
  "AAABJFHC" varchar(1) NOT NULL default ' ',
  "AAABJDFW" varchar(2) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLARI" varchar(6) NOT NULL default '000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABJDPL" varchar(6) NOT NULL default '000000',
  "AAABJDGB" varchar(2) NOT NULL default ' ',
  "AAABJDGC" varchar(45) NOT NULL default ' ',
  "AAABJDGD" varchar(1) NOT NULL default ' ',
  "AAABLTZM" varchar(8) NOT NULL default '00000000',
  "AAABLTZN" varchar(12) NOT NULL default ' ',
  "AAABLTYV" varchar(8) NOT NULL default '00000000',
  "AAABLTYW" varchar(12) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDGK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDCT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDGK" varchar(32) NOT NULL default ' ',
  "AAABJDDE" varchar(8) NOT NULL default ' ',
  "AAABLTZM" varchar(8) NOT NULL default '00000000',
  "AAABLTZN" varchar(12) NOT NULL default ' ',
  "AAABLTYV" varchar(8) NOT NULL default '00000000',
  "AAABLTYW" varchar(12) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDGK","AAABJDDE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDGK" varchar(32) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABJDFT" varchar(1) NOT NULL default ' ',
  "AAABJDFU" varchar(3) NOT NULL default ' ',
  "AAABJFHC" varchar(1) NOT NULL default ' ',
  "AAABJDFW" varchar(2) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLARI" varchar(6) NOT NULL default '000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABJDPL" varchar(6) NOT NULL default '000000',
  "AAABJDGB" varchar(2) NOT NULL default ' ',
  "AAABJDGC" varchar(45) NOT NULL default ' ',
  "AAABJDGD" varchar(1) NOT NULL default ' ',
  "AAABLTZM" varchar(8) NOT NULL default '00000000',
  "AAABLTZN" varchar(12) NOT NULL default ' ',
  "AAABLTYV" varchar(8) NOT NULL default '00000000',
  "AAABLTYW" varchar(12) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDGK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDED" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDGK" varchar(32) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABJDFT" varchar(1) NOT NULL default ' ',
  "AAABJDFU" varchar(3) NOT NULL default ' ',
  "AAABJFHC" varchar(1) NOT NULL default ' ',
  "AAABJDFW" varchar(2) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLARI" varchar(6) NOT NULL default '000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABJDPL" varchar(6) NOT NULL default '000000',
  "AAABJDGB" varchar(2) NOT NULL default ' ',
  "AAABJDGC" varchar(45) NOT NULL default ' ',
  "AAABJDGD" varchar(1) NOT NULL default ' ',
  "AAABLTZM" varchar(8) NOT NULL default '00000000',
  "AAABLTZN" varchar(12) NOT NULL default ' ',
  "AAABLTYV" varchar(8) NOT NULL default '00000000',
  "AAABLTYW" varchar(12) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDGK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDFB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDGK" varchar(32) NOT NULL default ' ',
  "AAABJDFM" varchar(18) NOT NULL default ' ',
  "AAABLTZM" varchar(8) NOT NULL default '00000000',
  "AAABLTZN" varchar(12) NOT NULL default ' ',
  "AAABLTYV" varchar(8) NOT NULL default '00000000',
  "AAABLTYW" varchar(12) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDGK","AAABJDFM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDFN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDGK" varchar(32) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABJDFT" varchar(1) NOT NULL default ' ',
  "AAABJDFU" varchar(3) NOT NULL default ' ',
  "AAABJFHC" varchar(1) NOT NULL default ' ',
  "AAABJDFW" varchar(2) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLARI" varchar(6) NOT NULL default '000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABJDPL" varchar(6) NOT NULL default '000000',
  "AAABJDGB" varchar(2) NOT NULL default ' ',
  "AAABJDGC" varchar(45) NOT NULL default ' ',
  "AAABJDGD" varchar(1) NOT NULL default ' ',
  "AAABLTZM" varchar(8) NOT NULL default '00000000',
  "AAABLTZN" varchar(12) NOT NULL default ' ',
  "AAABLTYV" varchar(8) NOT NULL default '00000000',
  "AAABLTYW" varchar(12) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDGK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDGL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDKE" VARBINARY(255) NOT NULL,
  "AAABJDGU" varchar(8) NOT NULL default '00000000',
  "AAABMLTD" varchar(4) NOT NULL default '0000',
  "AAABJDMH" varchar(4) NOT NULL default '0000',
  "AAABJDMI" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABJDKE"(255),"AAABJDGU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDGV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDNO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDHA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJDNO" varchar(2) NOT NULL default ' ',
  "AAABJDHE" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJDNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDHI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVY" VARBINARY(255) NOT NULL,
  "AAABLTVZ" varchar(8) NOT NULL default '00000000',
  "AAABJDIH" varchar(2) NOT NULL default ' ',
  "AAABJDII" varchar(2) NOT NULL default ' ',
  "AAABJDIJ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLTVY"(255),"AAABLTVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDHS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVY" VARBINARY(255) NOT NULL,
  "AAABLTVZ" varchar(8) NOT NULL default '00000000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABJDHX" decimal(13,3) NOT NULL default '0.000',
  "AAABLQYC" decimal(13,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABLTVY"(255),"AAABLTVZ","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDID" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVY" VARBINARY(255) NOT NULL,
  "AAABLTVZ" varchar(8) NOT NULL default '00000000',
  "AAABJDIH" varchar(2) NOT NULL default ' ',
  "AAABJDII" varchar(2) NOT NULL default ' ',
  "AAABJDIJ" varchar(8) NOT NULL default '00000000',
  "AAABJDIK" varchar(1) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTVY"(255),"AAABLTVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDIP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVY" VARBINARY(255) NOT NULL,
  "AAABLTVZ" varchar(8) NOT NULL default '00000000',
  "AAABJDJD" varchar(3) NOT NULL default ' ',
  "AAABLRSJ" varchar(1) NOT NULL default ' ',
  "AAABJDIV" varchar(8) NOT NULL default '00000000',
  "AAABJDIW" varchar(2) NOT NULL default ' ',
  "AAABJDIX" varchar(2) NOT NULL default ' ',
  "AAABJDIY" varchar(8) NOT NULL default '00000000',
  "AAABMQXN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTVY"(255),"AAABLTVZ","AAABJDJD","AAABLRSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDJF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDLR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDJK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJDLR" varchar(1) NOT NULL default ' ',
  "AAABJDJO" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJDLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDJS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDKO" VARBINARY(255) NOT NULL,
  "AAABLKWV" varchar(2) NOT NULL default '00',
  "AAABLTVY" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAABJDKO"(255),"AAABLKWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDKA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDKO" VARBINARY(255) NOT NULL,
  "AAABLKWV" varchar(2) NOT NULL default '00',
  "AAABJDKE" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAABJDKO"(255),"AAABLKWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDKI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDKO" VARBINARY(255) NOT NULL,
  "AAABLKWV" varchar(2) NOT NULL default '00',
  "AAABLTVY" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAABJDKO"(255),"AAABLKWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDKQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDOF" varchar(3) NOT NULL default '000',
  "AAABJDNO" varchar(2) NOT NULL default ' ',
  "AAABMALF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDOF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDKX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJDOF" varchar(3) NOT NULL default '000',
  "AAABJDLJ" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJDOF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDLF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJDOF" varchar(3) NOT NULL default '000',
  "AAABJDLJ" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJDOF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDLN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDOF" varchar(3) NOT NULL default '000',
  "AAABJDNO" varchar(2) NOT NULL default ' ',
  "AAABJDLR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDOF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDLU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABMALF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABMCEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDMC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDNG" VARBINARY(255) NOT NULL,
  "AAABKQPU" varchar(8) NOT NULL default '00000000',
  "AAABMLTD" varchar(4) NOT NULL default '0000',
  "AAABJDMH" varchar(4) NOT NULL default '0000',
  "AAABJDMI" varchar(8) NOT NULL default '00000000',
  "AAABJDMJ" varchar(124) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDNG"(255),"AAABKQPU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDMN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDNG" VARBINARY(255) NOT NULL,
  "AAABJDMQ" varchar(1) NOT NULL default ' ',
  "AAABJDNJ" VARBINARY(255),
  "AAABJDMS" varchar(4) NOT NULL default ' ',
  "AAABJDMT" varchar(1) NOT NULL default ' ',
  "AAABJDMU" VARBINARY(255),
  "AAABJDMV" varchar(4) NOT NULL default ' ',
  "AAABJDNK" varchar(8) NOT NULL default ' ',
  "AAABJDMX" varchar(4) NOT NULL default ' ',
  "AAABJDMY" varchar(1) NOT NULL default ' ',
  "AAABLTZM" varchar(8) NOT NULL default '00000000',
  "AAABLTZN" varchar(12) NOT NULL default ' ',
  "AAABLTYV" varchar(8) NOT NULL default '00000000',
  "AAABLTYW" varchar(12) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLTYS" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABJDNG"(255)),
  KEY "AAABJDNH" ("AAABMRIL","AAABJDNJ"(255),"AAABJDNK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDNL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDOO" varchar(2) NOT NULL default '00',
  "AAABJDNO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDOO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDNR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABLKLY" varchar(1) NOT NULL default ' ',
  "AAABJDOO" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMLHQ","AAABLKLY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDNZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDOF" varchar(3) NOT NULL default '000',
  "AAABJDOO" varchar(2) NOT NULL default '00',
  "AAABLXQJ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABJDOF","AAABJDOO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDOH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJDOO" varchar(2) NOT NULL default '00',
  "AAABJDOL" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJDOO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDOP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDOW" VARBINARY(255) NOT NULL,
  "AAABJDOX" varchar(8) NOT NULL default '00000000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMQXN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDOW"(255),"AAABJDOX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDOY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABLARI" varchar(6) NOT NULL default '000000',
  "AAABJDPL" varchar(6) NOT NULL default '000000',
  "AAABJDPM" varchar(7) NOT NULL default ' ',
  "AAABJDPN" int(11) NOT NULL default '0',
  "AAABJDPO" varchar(1) NOT NULL default '0',
  "AAABJDPP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXIY","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDPT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABJDQB" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXIY","AAABMDEP","AAABJDQB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDQC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABLFYC" varchar(2) NOT NULL default ' ',
  "AAABJDQL" varchar(4) NOT NULL default '0000',
  "AAABLTWR" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLXIY","AAABLFYC","AAABJDQL","AAABLTWR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDQN" (
  "AAABMDXP" varchar(12) NOT NULL default ' ',
  "AAABLMXP" varchar(1) NOT NULL default ' ',
  "AAABMDWW" varchar(12) NOT NULL default ' ',
  "AAABJDQR" int(11) NOT NULL default '0',
  "AAABMRCK" varchar(72) NOT NULL default ' ',
  "AAABJDQT" varchar(1) NOT NULL default ' ',
  "AAABJDQU" varchar(1) NOT NULL default ' ',
  "AAABJDQV" varchar(1) NOT NULL default ' ',
  "AAABJDQW" varchar(1) NOT NULL default ' ',
  "AAABMMBE" varchar(1) NOT NULL default ' ',
  "AAABJDQY" varchar(46) NOT NULL default ' ',
  "AAABJDQZ" varchar(46) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAABJDRC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDXP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDRE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDTI" varchar(16) NOT NULL default ' ',
  "AAABJDTJ" varchar(30) NOT NULL default ' ',
  "AAABJDRI" varchar(1) NOT NULL default ' ',
  "AAABJDRJ" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABJDTI","AAABJDTJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDRN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDRW" varchar(30) NOT NULL default ' ',
  "AAABJDRX" varchar(1) NOT NULL default ' ',
  "AAABJDRY" varchar(30) NOT NULL default ' ',
  "AAABJDTJ" varchar(30) NOT NULL default ' ',
  "AAABJDRT" varchar(30) NOT NULL default ' ',
  "AAABJDTI" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDRW","AAABJDRX","AAABJDRY","AAABJDTJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDSA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWIT" varchar(12) NOT NULL default ' ',
  "AAABLWIV" varchar(30) NOT NULL default ' ',
  "AAABLWIX" smallint(6) NOT NULL default '0',
  "AAABLWIZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLWIT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDSI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDTH" varchar(30) NOT NULL default ' ',
  "AAABJDTI" varchar(16) NOT NULL default ' ',
  "AAABJDTJ" varchar(30) NOT NULL default ' ',
  "AAABJDST" varchar(2) NOT NULL default ' ',
  "AAABLPSU" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDTH","AAABJDTI","AAABJDTJ","AAABJDST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDSU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDTH" varchar(30) NOT NULL default ' ',
  "AAABJDTI" varchar(16) NOT NULL default ' ',
  "AAABJDTJ" varchar(30) NOT NULL default ' ',
  "AAABJDSZ" varchar(1) NOT NULL default ' ',
  "AAABJDTA" smallint(6) NOT NULL default '0',
  "AAABJDTB" varchar(10) NOT NULL default ' ',
  "AAABJDTC" varchar(30) NOT NULL default ' ',
  "AAABJDTD" varchar(16) NOT NULL default ' ',
  "AAABJDTE" varchar(30) NOT NULL default ' ',
  "AAABJDTF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDTH","AAABJDTI","AAABJDTJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDTK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKSPJ" varchar(10) NOT NULL default ' ',
  "AAABKSQI" varchar(5) NOT NULL default '00000',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  "AAABLUHY" varchar(1) NOT NULL default ' ',
  "AAABKPNI" varchar(1) NOT NULL default ' ',
  "AAABLRSP" varchar(1) NOT NULL default ' ',
  "AAABLRBI" varchar(1) NOT NULL default ' ',
  "AAABKPNL" varchar(1) NOT NULL default ' ',
  "AAABKPNM" varchar(1) NOT NULL default ' ',
  "AAABKPNN" varchar(8) NOT NULL default ' ',
  "AAABKPNO" varchar(2) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLUFJ" varchar(12) NOT NULL default ' ',
  "AAABKPNT" varchar(40) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLUFQ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABLUHX" varchar(10) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABLUFN" varchar(4) NOT NULL default ' ',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABKPOD" decimal(13,3) NOT NULL default '0.000',
  "AAABLNUW" varchar(8) NOT NULL default '00000000',
  "AAABKPOF" varchar(1) NOT NULL default ' ',
  "AAABKXWF" varchar(8) NOT NULL default '00000000',
  "AAABKPOH" varchar(8) NOT NULL default '00000000',
  "AAABLLYR" decimal(3,0) NOT NULL default '0',
  "AAABKPOJ" decimal(11,2) NOT NULL default '0.00',
  "AAABMCVU" decimal(5,0) NOT NULL default '0',
  "AAABLUFX" varchar(1) NOT NULL default ' ',
  "AAABLXTD" varchar(1) NOT NULL default ' ',
  "AAABLXTE" varchar(1) NOT NULL default ' ',
  "AAABKPOO" varchar(1) NOT NULL default ' ',
  "AAABKPOP" varchar(1) NOT NULL default ' ',
  "AAABKPOQ" varchar(1) NOT NULL default ' ',
  "AAABKSPM" varchar(1) NOT NULL default ' ',
  "AAABKPOS" varchar(1) NOT NULL default ' ',
  "AAABKSPN" varchar(1) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABKRYH" varchar(10) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABKPOX" varchar(1) NOT NULL default ' ',
  "AAABLUFO" varchar(10) NOT NULL default ' ',
  "AAABLIPI" varchar(5) NOT NULL default '00000',
  "AAABLLYO" varchar(10) NOT NULL default ' ',
  "AAABKPPB" varchar(1) NOT NULL default ' ',
  "AAABKQVL" varchar(10) NOT NULL default ' ',
  "AAABKQVM" varchar(3) NOT NULL default '000',
  "AAABLURO" varchar(3) NOT NULL default ' ',
  "AAABLXTF" varchar(8) NOT NULL default ' ',
  "AAABKPPG" varchar(8) NOT NULL default '00000000',
  "AAABKPPH" decimal(3,0) NOT NULL default '0',
  "AAABKPPI" decimal(7,0) NOT NULL default '0',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABLUHU" varchar(8) NOT NULL default '00000000',
  "AAABKPPM" decimal(13,3) NOT NULL default '0.000',
  "AAABKPQQ" varchar(10) NOT NULL default ' ',
  "AAABMCVW" varchar(10) NOT NULL default ' ',
  "AAABKPPO" varchar(1) NOT NULL default ' ',
  "AAABKPPP" varchar(1) NOT NULL default ' ',
  "AAABKRZZ" varchar(10) NOT NULL default '0000000000',
  "AAABLXSU" varchar(1) NOT NULL default ' ',
  "AAABKQWC" varchar(10) NOT NULL default '0000000000',
  "AAABKQWD" varchar(4) NOT NULL default '0000',
  "AAABLGJB" varchar(1) NOT NULL default ' ',
  "AAABKPPV" varchar(3) NOT NULL default ' ',
  "AAABKRRX" varchar(2) NOT NULL default ' ',
  "AAABKRTB" varchar(1) NOT NULL default ' ',
  "AAABKPPY" varchar(10) NOT NULL default '0000000000',
  "AAABKPPZ" varchar(1) NOT NULL default ' ',
  "AAABKPQA" varchar(1) NOT NULL default ' ',
  "AAABLXTN" varchar(18) NOT NULL default '000000000000000000',
  "AAABKPQC" varchar(2) NOT NULL default ' ',
  "AAABKPQD" varchar(1) NOT NULL default ' ',
  "AAABLWZD" varchar(10) NOT NULL default ' ',
  "AAABMCEY" varchar(10) NOT NULL default ' ',
  "AAABKRUF" varchar(1) NOT NULL default ' ',
  "AAABKQWF" varchar(4) NOT NULL default ' ',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABKPQL" varchar(1) NOT NULL default ' ',
  "AAABLRZL" varchar(18) NOT NULL default ' ',
  "AAABKPQN" decimal(13,3) NOT NULL default '0.000',
  "AAABKPQO" varchar(8) NOT NULL default '00000000',
  "AAABLSJL" varchar(2) NOT NULL default ' ',
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  "AAABKRYA" varchar(10) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABKRXZ" varchar(10) NOT NULL default ' ',
  "AAABKQYN" varchar(1) NOT NULL default ' ',
  "AAABLXVG" varchar(1) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLUFU" varchar(35) NOT NULL default ' ',
  "AAABKPQZ" varchar(1) NOT NULL default ' ',
  "AAABKPRA" varchar(4) NOT NULL default ' ',
  "AAABLXVL" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXVN" varchar(12) NOT NULL default ' ',
  "AAABKRYX" varchar(40) NOT NULL default ' ',
  "AAABKRYY" varchar(10) NOT NULL default ' ',
  "AAABKPRG" varchar(10) NOT NULL default ' ',
  "AAABKPRH" varchar(10) NOT NULL default ' ',
  "AAABKPRI" varchar(5) NOT NULL default '00000',
  "AAABLURC" decimal(3,0) NOT NULL default '0',
  "AAABLXVP" varchar(10) NOT NULL default ' ',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABKSPJ","AAABKSQI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJDXX" (
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEXJ" varchar(15) NOT NULL default ' ',
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  "AAABJETC" varchar(60) NOT NULL default ' ',
  "AAABJEHO" varchar(4) NOT NULL default ' ',
  "AAABJDYD" varchar(20) NOT NULL default ' ',
  "AAABJEHP" varchar(4) NOT NULL default ' ',
  "AAABJDYF" varchar(15) NOT NULL default ' ',
  "AAABJDYG" varchar(40) NOT NULL default ' ',
  "AAABJDYH" varchar(1) NOT NULL default ' ',
  "AAABJDYI" varchar(60) NOT NULL default ' ',
  "AAABJEQT" varchar(15) NOT NULL default ' ',
  "AAABJEEF" varchar(60) NOT NULL default ' ',
  "AAABJDYL" varchar(30) NOT NULL default ' ',
  "AAABJDYM" varchar(4) NOT NULL default ' ',
  "AAABJEDO" varchar(20) NOT NULL default ' ',
  "AAABJEDP" varchar(12) NOT NULL default ' ',
  "AAABJEHQ" varchar(10) NOT NULL default ' ',
  "AAABJDYQ" varchar(10) NOT NULL default ' ',
  "AAABJDYR" varchar(80) NOT NULL default ' ',
  "AAABJDYS" varchar(80) NOT NULL default ' ',
  "AAABLUEO" varchar(32) NOT NULL default ' ',
  "AAABJDYU" varchar(80) NOT NULL default ' ',
  "AAABJEHS" varchar(30) NOT NULL default ' ',
  "AAABJDYW" varchar(60) NOT NULL default ' ',
  "AAABJEHT" varchar(30) NOT NULL default ' ',
  "AAABJDYY" varchar(60) NOT NULL default ' ',
  "AAABJEHU" varchar(30) NOT NULL default ' ',
  "AAABJDZA" varchar(30) NOT NULL default ' ',
  "AAABJDZB" varchar(60) NOT NULL default ' ',
  "AAABJDZC" varchar(30) NOT NULL default ' ',
  "AAABJDZD" varchar(60) NOT NULL default ' ',
  "AAABJDZE" varchar(30) NOT NULL default ' ',
  "AAABJEIC" varchar(1) NOT NULL default ' ',
  "AAABJDZG" varchar(1) NOT NULL default ' ',
  "AAABJDZH" varchar(1) NOT NULL default ' ',
  "AAABJDZI" varchar(1) NOT NULL default ' ',
  "AAABJDZJ" varchar(1) NOT NULL default ' ',
  "AAABJDZK" varchar(1) NOT NULL default ' ',
  "AAABJDZL" varchar(1) NOT NULL default ' ',
  "AAABJDZM" varchar(1) NOT NULL default ' ',
  "AAABJDZN" varchar(1) NOT NULL default ' ',
  "AAABJDZO" varchar(1) NOT NULL default ' ',
  "AAABJDZP" varchar(1) NOT NULL default ' ',
  "AAABJDZQ" varchar(1) NOT NULL default ' ',
  "AAABJDZR" varchar(1) NOT NULL default ' ',
  "AAABJDZS" varchar(1) NOT NULL default ' ',
  "AAABJDZT" varchar(1) NOT NULL default ' ',
  "AAABJDZU" varchar(40) NOT NULL default ' ',
  "AAABJDZV" varchar(40) NOT NULL default ' ',
  "AAABJDZW" varchar(40) NOT NULL default ' ',
  "AAABJDZX" varchar(40) NOT NULL default ' ',
  "AAABJDZY" varchar(40) NOT NULL default ' ',
  "AAABJDZZ" varchar(40) NOT NULL default ' ',
  "AAABJEAA" varchar(40) NOT NULL default ' ',
  "AAABJEAB" varchar(40) NOT NULL default ' ',
  "AAABJEAC" varchar(40) NOT NULL default ' ',
  "AAABJEAD" varchar(40) NOT NULL default ' ',
  "AAABJEAE" varchar(40) NOT NULL default ' ',
  "AAABJEAF" varchar(40) NOT NULL default ' ',
  "AAABJEAG" varchar(40) NOT NULL default ' ',
  "AAABJEAH" varchar(40) NOT NULL default ' ',
  "AAABJEAI" varchar(40) NOT NULL default ' ',
  "AAABJEAJ" varchar(40) NOT NULL default ' ',
  "AAABJEAK" varchar(40) NOT NULL default ' ',
  "AAABJEAL" varchar(40) NOT NULL default ' ',
  "AAABJEAM" varchar(40) NOT NULL default ' ',
  "AAABJEAN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEAP" (
  "AAABJEJS" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEAS" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJEJS" varchar(2) NOT NULL default ' ',
  "AAABJEAV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABJEJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEAY" (
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABJEBA" varchar(45) NOT NULL default ' ',
  "AAABJEBB" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABLESS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEBD" (
  "AAABJEVT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEVT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEBG" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJEVT" varchar(2) NOT NULL default ' ',
  "AAABJEBJ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABJEVT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEBM" (
  "AAABJEXF" varchar(3) NOT NULL default '000',
  "AAABJEBO" varchar(3) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABJEBQ" varchar(1) NOT NULL default ' ',
  "AAABJEBR" varchar(1) NOT NULL default ' ',
  "AAABJEBS" varchar(1) NOT NULL default ' ',
  "AAABJEGZ" varchar(1) NOT NULL default ' ',
  "AAABJEHA" varchar(1) NOT NULL default ' ',
  "AAABJEBV" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEBX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJEXF" varchar(3) NOT NULL default '000',
  "AAABJECA" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABJEXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJECD" (
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABLTXI" varchar(10) NOT NULL default ' ',
  "AAABJECG" varchar(20) NOT NULL default ' ',
  "AAABJECH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRFQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJECJ" (
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEQT" varchar(15) NOT NULL default ' ',
  "AAABLJKC" varchar(4) NOT NULL default '0000',
  "AAABMNDU" varchar(40) NOT NULL default ' ',
  "AAABMFSV" varchar(4) NOT NULL default '0000',
  "AAABJECP" varchar(1) NOT NULL default ' ',
  "AAABJECQ" varchar(132) NOT NULL default ' ',
  "AAABJECR" varchar(132) NOT NULL default ' ',
  "AAABJECS" varchar(30) NOT NULL default ' ',
  "AAABJECT" varchar(60) NOT NULL default ' ',
  "AAABJECU" varchar(30) NOT NULL default ' ',
  "AAABJECV" varchar(30) NOT NULL default ' ',
  "AAABJECW" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABJECZ" varchar(4) NOT NULL default '0000',
  "AAABJEDA" varchar(6) NOT NULL default '000000',
  "AAABJEDB" varchar(30) NOT NULL default ' ',
  "AAABLBEI" varchar(6) NOT NULL default '000000',
  "AAABJTEQ" varchar(6) NOT NULL default '000000',
  "AAABMLMY" varchar(6) NOT NULL default '000000',
  "AAABLDNU" varchar(4) NOT NULL default ' ',
  "AAABLUWW" varchar(1) NOT NULL default ' ',
  "AAABJEDH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEXI","AAABJEQT","AAABLJKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEDL" (
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEQT" varchar(15) NOT NULL default ' ',
  "AAABJEDO" varchar(20) NOT NULL default ' ',
  "AAABJEDP" varchar(12) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABJEXI","AAABJEQT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEDU" (
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEQT" varchar(15) NOT NULL default ' ',
  "AAABJESP" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABJEXI","AAABJEQT","AAABJESP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEEB" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEQT" varchar(15) NOT NULL default ' ',
  "AAABJEEF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABJEXI","AAABJEQT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEEJ" (
  "AAABJEEM" varchar(32) NOT NULL default ' ',
  "AAABJEEL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEEM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEEN" (
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABJEXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEES" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEEV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABJEXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEEY" (
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEXJ" varchar(15) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABJEXI","AAABJEXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEFF" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEXJ" varchar(15) NOT NULL default ' ',
  "AAABJEFJ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABJEXI","AAABJEXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEFN" (
  "AAABLEJW" varchar(74) NOT NULL default ' ',
  "AAABLUVJ" varchar(32) NOT NULL default ' ',
  "AAABMKFU" varchar(14) NOT NULL default '00000000000000',
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEXJ" varchar(15) NOT NULL default ' ',
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  "AAABLKIV" varchar(2) NOT NULL default ' ',
  "AAABJEKZ" varchar(1) NOT NULL default '0',
  "AAABJESW" varchar(30) NOT NULL default ' ',
  "AAABLUFA" varchar(2) NOT NULL default ' ',
  "AAABMLFE" varchar(1) NOT NULL default ' ',
  "AAABJEFZ" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJEGC" int(11) NOT NULL default '0',
  "AAABJEGD" int(11) NOT NULL default '0',
  "AAABJEGE" int(11) NOT NULL default '0',
  "AAABJEGF" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLEJW","AAABLUVJ","AAABMKFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEGJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEGM" varchar(10) NOT NULL default ' ',
  "AAABJEGN" varchar(10) NOT NULL default ' ',
  "AAABJEGO" varchar(2) NOT NULL default ' ',
  "AAABJEGP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJEXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEGS" (
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEXJ" varchar(15) NOT NULL default ' ',
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  "AAABMKFU" varchar(14) NOT NULL default '00000000000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJEXF" varchar(3) NOT NULL default '000',
  "AAABJEGZ" varchar(1) NOT NULL default ' ',
  "AAABJEHA" varchar(1) NOT NULL default ' ',
  "AAABJEHB" varchar(30) NOT NULL default ' ',
  "AAABJEHC" varchar(30) NOT NULL default ' ',
  "AAABJEHD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEXI","AAABJEXJ","AAABMKFH","AAABMKFU","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEHJ" (
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEXJ" varchar(15) NOT NULL default ' ',
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  "AAABMIRY" varchar(2) NOT NULL default ' ',
  "AAABJEHO" varchar(4) NOT NULL default ' ',
  "AAABJEHP" varchar(4) NOT NULL default ' ',
  "AAABJEHQ" varchar(10) NOT NULL default ' ',
  "AAABLUEO" varchar(32) NOT NULL default ' ',
  "AAABJEHS" varchar(30) NOT NULL default ' ',
  "AAABJEHT" varchar(30) NOT NULL default ' ',
  "AAABJEHU" varchar(30) NOT NULL default ' ',
  "AAABJEHV" varchar(8) NOT NULL default '00000000',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAABJEHX" varchar(1) NOT NULL default ' ',
  "AAABJEHY" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABJEIB" varchar(1) NOT NULL default ' ',
  "AAABJEIC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEXI","AAABJEXJ","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEIG" (
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEXJ" varchar(15) NOT NULL default ' ',
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  "AAABMLTD" varchar(4) NOT NULL default '0000',
  "AAABJESF" varchar(30) NOT NULL default ' ',
  "AAABJESG" varchar(30) NOT NULL default ' ',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAABMMRH" varchar(72) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABJEXI","AAABJEXJ","AAABMKFH","AAABMLTD","AAABJESF","AAABJESG","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEIY" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEJS" varchar(2) NOT NULL default ' ',
  "AAABJEXJ" varchar(15) NOT NULL default ' ',
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  "AAABMLTD" varchar(4) NOT NULL default '0000',
  "AAABJESF" varchar(30) NOT NULL default ' ',
  "AAABJESG" varchar(30) NOT NULL default ' ',
  "AAABJESW" varchar(30) NOT NULL default ' ',
  "AAABJEPD" varchar(30) NOT NULL default ' ',
  "AAABJEQT" varchar(15) NOT NULL default ' ',
  "AAABJEVU" varchar(30) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABJEXI","AAABJEJS","AAABJEXJ","AAABMKFH","AAABMLTD","AAABJESF","AAABJESG","AAABJESW","AAABJEPD","AAABJEQT","AAABJEVU","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEKD" (
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEXJ" varchar(15) NOT NULL default ' ',
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  "AAABJEKH" varchar(120) NOT NULL default ' ',
  "AAABJEKI" varchar(120) NOT NULL default ' ',
  "AAABJEKJ" varchar(60) NOT NULL default ' ',
  "AAABJEKK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEXI","AAABJEXJ","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEKO" (
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEXJ" varchar(15) NOT NULL default ' ',
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  "AAABJEKZ" varchar(1) NOT NULL default '0',
  "AAABJELA" varchar(4) NOT NULL default '0000',
  "AAABJELB" varchar(8) NOT NULL default ' ',
  "AAABLEJW" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEXI","AAABJEXJ","AAABMKFH","AAABJEKZ","AAABJELA","AAABJELB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJELC" (
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEXJ" varchar(15) NOT NULL default ' ',
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  "AAABJELL" varchar(30) NOT NULL default ' ',
  "AAABJELH" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEXI","AAABJEXJ","AAABMKFH","AAABJELL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJELM" (
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEXJ" varchar(15) NOT NULL default ' ',
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  "AAABJESW" varchar(30) NOT NULL default ' ',
  "AAABJEMU" varchar(30) NOT NULL default ' ',
  "AAABJELS" varchar(4) NOT NULL default '0000',
  "AAABJELT" varchar(4) NOT NULL default ' ',
  "AAABJELU" varchar(3) NOT NULL default '000',
  "AAABJELV" varchar(60) NOT NULL default ' ',
  "AAABJELW" varchar(30) NOT NULL default ' ',
  "AAABJELX" varchar(1) NOT NULL default ' ',
  "AAABJELY" varchar(30) NOT NULL default ' ',
  "AAABJELZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEXI","AAABJEXJ","AAABMKFH","AAABJESW","AAABJEMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEMF" (
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEXJ" varchar(15) NOT NULL default ' ',
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  "AAABJEMT" varchar(30) NOT NULL default ' ',
  "AAABJEMU" varchar(30) NOT NULL default ' ',
  "AAABJEML" varchar(4) NOT NULL default '0000',
  "AAABJEMM" varchar(4) NOT NULL default '0000',
  "AAABJEMN" varchar(20) NOT NULL default ' ',
  "AAABJEMO" varchar(3) NOT NULL default '000',
  "AAABJEMP" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABJEXI","AAABJEXJ","AAABMKFH","AAABJEMT","AAABJEMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEMV" (
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEXJ" varchar(15) NOT NULL default ' ',
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  "AAABJEOD" varchar(4) NOT NULL default '0000',
  "AAABLKIV" varchar(2) NOT NULL default ' ',
  "AAABLEJW" varchar(120) NOT NULL default ' ',
  "AAABJENC" varchar(30) NOT NULL default ' ',
  "AAABJEND" varchar(4) NOT NULL default '0000',
  "AAABJENE" varchar(1) NOT NULL default ' ',
  "AAABJENF" varchar(1) NOT NULL default ' ',
  "AAABJENG" varchar(6) NOT NULL default '000000',
  "AAABJENH" varchar(1) NOT NULL default ' ',
  "AAABJENI" varchar(32) NOT NULL default ' ',
  "AAABJENJ" varchar(1) NOT NULL default ' ',
  "AAABJENK" varchar(1) NOT NULL default ' ',
  "AAABJENL" varchar(1) NOT NULL default ' ',
  "AAABJENM" varchar(1) NOT NULL default ' ',
  "AAABJENN" varchar(1) NOT NULL default ' ',
  "AAABJENO" varchar(1) NOT NULL default ' ',
  "AAABJENP" varchar(1) NOT NULL default ' ',
  "AAABJENQ" varchar(1) NOT NULL default ' ',
  "AAABJENR" varchar(1) NOT NULL default ' ',
  "AAABJENS" varchar(1) NOT NULL default ' ',
  "AAABJENT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEXI","AAABJEXJ","AAABMKFH","AAABJEOD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJENY" (
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEXJ" varchar(15) NOT NULL default ' ',
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  "AAABJESW" varchar(30) NOT NULL default ' ',
  "AAABJEOD" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABJEXI","AAABJEXJ","AAABMKFH","AAABJESW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEOI" (
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEXJ" varchar(15) NOT NULL default ' ',
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  "AAABJEXF" varchar(3) NOT NULL default '000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABJEXI","AAABJEXJ","AAABMKFH","AAABJEXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEOU" (
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEXJ" varchar(15) NOT NULL default ' ',
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  "AAABMLTD" varchar(4) NOT NULL default '0000',
  "AAABJESF" varchar(30) NOT NULL default ' ',
  "AAABJESG" varchar(30) NOT NULL default ' ',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAABJESW" varchar(30) NOT NULL default ' ',
  "AAABJEPD" varchar(30) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABJEXI","AAABJEXJ","AAABMKFH","AAABMLTD","AAABJESF","AAABJESG","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEPN" (
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEXJ" varchar(15) NOT NULL default ' ',
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEXI","AAABJEXJ","AAABMKFH","AAABMDEN","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEPZ" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEXJ" varchar(15) NOT NULL default ' ',
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  "AAABMKFU" varchar(14) NOT NULL default '00000000000000',
  "AAABJEQN" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABJEXI","AAABJEXJ","AAABMKFH","AAABMKFU","AAABJEQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEQO" (
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEXJ" varchar(15) NOT NULL default ' ',
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  "AAABMLTD" varchar(4) NOT NULL default '0000',
  "AAABJEQT" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEXI","AAABJEXJ","AAABMKFH","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEQY" (
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEXJ" varchar(15) NOT NULL default ' ',
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  "AAABMLTD" varchar(4) NOT NULL default '0000',
  "AAABJESF" varchar(30) NOT NULL default ' ',
  "AAABJESW" varchar(30) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEXI","AAABJEXJ","AAABMKFH","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJERL" (
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEXJ" varchar(15) NOT NULL default ' ',
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  "AAABMLTD" varchar(4) NOT NULL default '0000',
  "AAABJESF" varchar(30) NOT NULL default ' ',
  "AAABJESG" varchar(30) NOT NULL default ' ',
  "AAABJEVT" varchar(2) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABJEVU" varchar(30) NOT NULL default ' ',
  "AAABJETU" varchar(30) NOT NULL default ' ',
  "AAABLRQQ" varchar(30) NOT NULL default ' ',
  "AAABJERY" varchar(30) NOT NULL default ' ',
  "AAABJERZ" varchar(4) NOT NULL default '0000',
  "AAABJESA" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABJEXI","AAABJEXJ","AAABMKFH","AAABMLTD","AAABJESF","AAABJESG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJESH" (
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEXJ" varchar(15) NOT NULL default ' ',
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  "AAABJESW" varchar(30) NOT NULL default ' ',
  "AAABJESM" varchar(60) NOT NULL default ' ',
  "AAABJESN" varchar(30) NOT NULL default ' ',
  "AAABMLTD" varchar(4) NOT NULL default '0000',
  "AAABJESP" varchar(2) NOT NULL default '00',
  "AAABJESQ" varchar(1) NOT NULL default ' ',
  "AAABJESR" varchar(10) NOT NULL default '0000000000',
  "AAABJESS" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABJEXI","AAABJEXJ","AAABMKFH","AAABJESW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJESX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEXJ" varchar(15) NOT NULL default ' ',
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  "AAABJETC" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABJEXI","AAABJEXJ","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJETH" (
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEVT" varchar(2) NOT NULL default ' ',
  "AAABJEVU" varchar(30) NOT NULL default ' ',
  "AAABJETL" varchar(2) NOT NULL default ' ',
  "AAABJETM" varchar(2) NOT NULL default ' ',
  "AAABJETN" varchar(2) NOT NULL default ' ',
  "AAABJETO" varchar(3) NOT NULL default '000',
  "AAABJETP" varchar(4) NOT NULL default ' ',
  "AAABJETQ" varchar(1) NOT NULL default ' ',
  "AAABJETR" varchar(3) NOT NULL default '000',
  "AAABJETS" varchar(4) NOT NULL default ' ',
  "AAABJETT" varchar(1) NOT NULL default ' ',
  "AAABJETU" varchar(30) NOT NULL default ' ',
  "AAABJETV" varchar(30) NOT NULL default ' ',
  "AAABJETW" varchar(1) NOT NULL default ' ',
  "AAABJETX" varchar(30) NOT NULL default ' ',
  "AAABJETY" varchar(2) NOT NULL default '00',
  "AAABJETZ" varchar(2) NOT NULL default '00',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABJEXI","AAABJEVT","AAABJEVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEUF" (
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEVT" varchar(2) NOT NULL default ' ',
  "AAABJEVU" varchar(30) NOT NULL default ' ',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAABMMRH" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEXI","AAABJEVT","AAABJEVU","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEUP" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEVT" varchar(2) NOT NULL default ' ',
  "AAABJEVU" varchar(30) NOT NULL default ' ',
  "AAABJEUU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABJEXI","AAABJEVT","AAABJEVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEUZ" (
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEVT" varchar(2) NOT NULL default ' ',
  "AAABJEVU" varchar(30) NOT NULL default ' ',
  "AAABJEVK" varchar(30) NOT NULL default ' ',
  "AAABJEVE" varchar(30) NOT NULL default ' ',
  "AAABJEVQ" varchar(30) NOT NULL default ' ',
  "AAABJEVR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEXI","AAABJEVT","AAABJEVU","AAABJEVK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEVL" (
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEVT" varchar(2) NOT NULL default ' ',
  "AAABJEVU" varchar(30) NOT NULL default ' ',
  "AAABJEVV" varchar(30) NOT NULL default ' ',
  "AAABJEVQ" varchar(30) NOT NULL default ' ',
  "AAABJEVR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEXI","AAABJEVT","AAABJEVU","AAABJEVV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEVW" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJEVY" varchar(1) NOT NULL default ' ',
  "AAABJEVZ" varchar(1) NOT NULL default ' ',
  "AAABJEWA" varchar(1) NOT NULL default ' ',
  "AAABJEWB" varchar(1) NOT NULL default ' ',
  "AAABJEWC" varchar(1) NOT NULL default ' ',
  "AAABJEWD" varchar(1) NOT NULL default ' ',
  "AAABJEWE" varchar(1) NOT NULL default ' ',
  "AAABJEWF" varchar(1) NOT NULL default ' ',
  "AAABJEWG" varchar(1) NOT NULL default ' ',
  "AAABJEWH" varchar(1) NOT NULL default ' ',
  "AAABJEWI" varchar(1) NOT NULL default ' ',
  "AAABJEWJ" varchar(1) NOT NULL default ' ',
  "AAABJEWK" varchar(1) NOT NULL default ' ',
  "AAABJEWL" varchar(1) NOT NULL default ' ',
  "AAABJEWM" varchar(1) NOT NULL default ' ',
  "AAABJEWN" varchar(1) NOT NULL default ' ',
  "AAABJEWO" varchar(3) NOT NULL default '000',
  "AAABJEWP" varchar(1) NOT NULL default ' ',
  "AAABJEWQ" varchar(1) NOT NULL default ' ',
  "AAABJEWR" varchar(1) NOT NULL default ' ',
  "AAABJEWS" varchar(1) NOT NULL default ' ',
  "AAABJEWT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEWV" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEXJ" varchar(15) NOT NULL default ' ',
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  "AAABJEXF" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMDEN","AAABJEXI","AAABJEXJ","AAABMKFH","AAABJEXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEXG" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  "AAABJEXJ" varchar(15) NOT NULL default ' ',
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEXN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABJEXQ" varchar(2) NOT NULL default ' ',
  "AAABJEXR" varchar(3) NOT NULL default '000',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAABJEXT" varchar(4) NOT NULL default ' ',
  "AAABJEXU" varchar(4) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABLXDP" varchar(30) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAABJEYB" varchar(4) NOT NULL default ' ',
  "AAABJEYC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEYE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABJYUQ" varchar(1) NOT NULL default ' ',
  "AAABLQZY" varchar(4) NOT NULL default '0000',
  "AAABJSBE" varchar(1) NOT NULL default ' ',
  "AAABJEYM" varchar(1) NOT NULL default ' ',
  "AAABJEYN" varchar(1) NOT NULL default ' ',
  "AAABLXQP" varchar(8) NOT NULL default '00000000',
  "AAABJEYP" varchar(12) NOT NULL default ' ',
  "AAABKPVY" varchar(8) NOT NULL default '00000000',
  "AAABJEYR" varchar(12) NOT NULL default ' ',
  "AAABJEYS" varchar(10) NOT NULL default ' ',
  "AAABJEYT" varchar(4) NOT NULL default '0000',
  "AAABJEYU" varchar(10) NOT NULL default ' ',
  "AAABJEYV" varchar(4) NOT NULL default '0000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABJEYX" varchar(10) NOT NULL default ' ',
  "AAABJEYY" varchar(4) NOT NULL default '0000',
  "AAABJEYZ" varchar(4) NOT NULL default ' ',
  "AAABJEZA" decimal(11,2) NOT NULL default '0.00',
  "AAABJEZB" decimal(11,2) NOT NULL default '0.00',
  "AAABJEZC" varchar(5) NOT NULL default ' ',
  "AAABJEZD" varchar(1) NOT NULL default ' ',
  "AAABJEZE" varchar(1) NOT NULL default ' ',
  "AAABJEZF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABMCVI","AAABMCSR","AAABJYUQ","AAABLQZY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEZM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFBD" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFBD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJEZR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFBD" varchar(10) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLRYR" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABLPZD" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABJEZZ" decimal(15,2) NOT NULL default '0.00',
  "AAABJFAA" varchar(5) NOT NULL default ' ',
  "AAABJFAB" decimal(13,3) NOT NULL default '0.000',
  "AAABJFAC" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABJFAG" varchar(1) NOT NULL default ' ',
  "AAABJFAH" varchar(1) NOT NULL default ' ',
  "AAABJFAI" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABJFBD","AAABMCSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFAM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFBD" varchar(10) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFBD","AAABMCSR","AAABMLCZ","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFAX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFBD" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJFBB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFBD","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFBF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABJFBK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCSR","AAABMREX","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFBP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVV" varchar(4) NOT NULL default ' ',
  "AAABJFBS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFBV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABLZMW" varchar(4) NOT NULL default '0000',
  "AAABJFBZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCFC","AAABLZMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFCD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFCJ" varchar(2) NOT NULL default ' ',
  "AAABJFCG" varchar(2) NOT NULL default '00',
  "AAABJFCH" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFCJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFCK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCIM" varchar(10) NOT NULL default ' ',
  "AAABMCME" varchar(4) NOT NULL default '0000',
  "AAABMCMG" varchar(4) NOT NULL default '0000',
  "AAABMRFO" varchar(2) NOT NULL default '00',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLXSU" varchar(1) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABJFCZ" varchar(5) NOT NULL default ' ',
  "AAABMCQA" decimal(13,2) NOT NULL default '0.00',
  "AAABLIPB" varchar(3) NOT NULL default ' ',
  "AAABJFDC" decimal(13,3) NOT NULL default '0.000',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMPWO" varchar(6) NOT NULL default '000000',
  "AAABLJGC" decimal(13,2) NOT NULL default '0.00',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABLHRD" varchar(4) NOT NULL default ' ',
  "AAABJFDR" varchar(10) NOT NULL default ' ',
  "AAABJFDS" varchar(10) NOT NULL default ' ',
  "AAABKPOJ" decimal(11,2) NOT NULL default '0.00',
  "AAABMCVU" decimal(5,0) NOT NULL default '0',
  "AAABJFDV" decimal(13,2) NOT NULL default '0.00',
  "AAABJFDW" varchar(2) NOT NULL default ' ',
  "AAABMDGB" varchar(16) NOT NULL default ' ',
  "AAABKRXZ" varchar(10) NOT NULL default ' ',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABJFEB" varchar(1) NOT NULL default ' ',
  "AAABJFEC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCIM","AAABMCME","AAABMCMG"),
  KEY "AAABJFEH" ("AAABMRIL","AAABMDBA","AAABMDBB","AAABMPWO"),
  KEY "AAABJFEM" ("AAABMRIL","AAABMRFO","AAABMREX","AAABMCSR","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFES" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABJFHC" varchar(1) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABJFHF" varchar(10) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHI" varchar(10) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJFGP" varchar(2) NOT NULL default ' ',
  "AAABJFGQ" varchar(1) NOT NULL default ' ',
  "AAABJFGR" varchar(8) NOT NULL default '00000000',
  "AAABJFJL" varchar(4) NOT NULL default ' ',
  "AAABJRKN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHJ","AAABLHPM","AAABLUNJ","AAABJFHB","AAABJFHC","AAABKFCV","AAABKFCZ","AAABJFHF","AAABLQVJ"),
  KEY "AAABJFFW" ("AAABJFHI","AAABJFHJ","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFGA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABJFHC" varchar(1) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABJFHF" varchar(10) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHI" varchar(10) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJFGP" varchar(2) NOT NULL default ' ',
  "AAABJFGQ" varchar(1) NOT NULL default ' ',
  "AAABJFGR" varchar(8) NOT NULL default '00000000',
  "AAABJFGS" varchar(1) NOT NULL default ' ',
  "AAABJFGT" varchar(1) NOT NULL default ' ',
  "AAABJFGU" varchar(1) NOT NULL default ' ',
  "AAABJGBU" varchar(2) NOT NULL default ' ',
  "AAABJGCH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHJ","AAABLHPM","AAABLUNJ","AAABJFHB","AAABJFHC","AAABKFCV","AAABKFCZ","AAABJFHF","AAABLQVJ"),
  KEY "AAABJFHH" ("AAABJFHI","AAABJFHJ","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFHL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABJFHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFHV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFIA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJFIE" varchar(31) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFII" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABJFIM" varchar(2) NOT NULL default ' ',
  "AAABJFIN" varchar(2) NOT NULL default ' ',
  "AAABJFIO" varchar(4) NOT NULL default ' ',
  "AAABJFIP" varchar(1) NOT NULL default ' ',
  "AAABMCXS" varchar(3) NOT NULL default ' ',
  "AAABJFIR" varchar(4) NOT NULL default ' ',
  "AAABJFIS" varchar(1) NOT NULL default '0',
  "AAABJFIT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFIX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKRBY" varchar(7) NOT NULL default ' ',
  "AAABKRFV" varchar(2) NOT NULL default ' ',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKRBY","AAABKRFV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFJH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJRKN" varchar(10) NOT NULL default ' ',
  "AAABJFJL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJRKN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFJP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCWX" varchar(3) NOT NULL default ' ',
  "AAABJRKN" varchar(10) NOT NULL default ' ',
  "AAABJFJV" decimal(9,6) NOT NULL default '0.000000',
  "AAABMJCM" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCWV","AAABMCWX","AAABJRKN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFKD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFKF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFKH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJGBD" varchar(10) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABJFNK" varchar(4) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABJFKS" varchar(40) NOT NULL default ' ',
  "AAABJFKT" varchar(35) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABJFKW" varchar(8) NOT NULL default '00000000',
  "AAABJFKX" varchar(8) NOT NULL default '00000000',
  "AAABJFKY" varchar(4) NOT NULL default ' ',
  "AAABJFKZ" decimal(3,0) NOT NULL default '0',
  "AAABJFLA" decimal(3,0) NOT NULL default '0',
  "AAABJFLB" decimal(3,0) NOT NULL default '0',
  "AAABJFLC" decimal(5,3) NOT NULL default '0.000',
  "AAABJFLD" decimal(5,3) NOT NULL default '0.000',
  "AAABKYBY" varchar(4) NOT NULL default ' ',
  "AAABJFLF" decimal(3,0) NOT NULL default '0',
  "AAABJFLG" decimal(3,0) NOT NULL default '0',
  "AAABJFLH" decimal(3,0) NOT NULL default '0',
  "AAABJFLI" decimal(5,3) NOT NULL default '0.000',
  "AAABJFLJ" decimal(5,3) NOT NULL default '0.000',
  "AAABLXRI" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABJFLM" varchar(2) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABJFLQ" varchar(8) NOT NULL default '00000000',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABJFLS" varchar(1) NOT NULL default ' ',
  "AAABJFLT" varchar(1) NOT NULL default ' ',
  "AAABJFLU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJGBD"),
  KEY "AAABJFLY" ("AAABMRIL","AAABMREX","AAABMPLT"),
  KEY "AAABJFMC" ("AAABMRIL","AAABMREX","AAABMCSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFMG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJFNK" varchar(4) NOT NULL default ' ',
  "AAABJFMK" varchar(2) NOT NULL default ' ',
  "AAABJFML" varchar(2) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABJFMN" varchar(1) NOT NULL default ' ',
  "AAABJFMO" varchar(1) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABLRQQ" varchar(3) NOT NULL default ' ',
  "AAABLKXQ" varchar(4) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABJFMT" varchar(1) NOT NULL default ' ',
  "AAABJFMU" decimal(4,2) NOT NULL default '0.00',
  "AAABJFMV" decimal(4,2) NOT NULL default '0.00',
  "AAABJFMW" decimal(5,3) NOT NULL default '0.000',
  "AAABJFMX" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJFNK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFNB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJFNK" varchar(4) NOT NULL default ' ',
  "AAABJFNG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMRHS","AAABJFNK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFNL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJFXE" varchar(4) NOT NULL default ' ',
  "AAABJFNP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJFXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFNT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJFXE" varchar(4) NOT NULL default ' ',
  "AAABJFNY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMRHS","AAABJFXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFOD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJGBD" varchar(10) NOT NULL default ' ',
  "AAABJFOP" varchar(3) NOT NULL default '000',
  "AAABJFXE" varchar(4) NOT NULL default ' ',
  "AAABJFST" decimal(4,2) NOT NULL default '0.00',
  "AAABJFOK" decimal(4,0) NOT NULL default '0',
  "AAABJFSE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJGBD","AAABJFOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFOQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJFPM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJFPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFOX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJFPM" varchar(4) NOT NULL default ' ',
  "AAABJFPC" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMRHS","AAABJFPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJGBD" varchar(10) NOT NULL default ' ',
  "AAABJFPX" varchar(3) NOT NULL default '000',
  "AAABJFPM" varchar(4) NOT NULL default ' ',
  "AAABJFPN" varchar(8) NOT NULL default '00000000',
  "AAABJFPO" varchar(8) NOT NULL default '00000000',
  "AAABJFSB" varchar(8) NOT NULL default '00000000',
  "AAABJFSC" varchar(8) NOT NULL default '00000000',
  "AAABJFPR" varchar(1) NOT NULL default ' ',
  "AAABJFPS" varchar(1) NOT NULL default ' ',
  "AAABJFPT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJGBD","AAABJFPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJFXF" varchar(4) NOT NULL default ' ',
  "AAABLPPS" varchar(10) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJFXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFQH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJFXF" varchar(4) NOT NULL default ' ',
  "AAABJFQM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMRHS","AAABJFXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFQR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJGBD" varchar(10) NOT NULL default ' ',
  "AAABJFRC" varchar(3) NOT NULL default '000',
  "AAABJFXF" varchar(4) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABJFST" decimal(5,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJGBD","AAABJFRC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFRD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJFRZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJFRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFRK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJFRZ" varchar(4) NOT NULL default ' ',
  "AAABJFRP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMRHS","AAABJFRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFRU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJGBD" varchar(10) NOT NULL default ' ',
  "AAABJFSL" varchar(3) NOT NULL default '000',
  "AAABJFRZ" varchar(4) NOT NULL default ' ',
  "AAABJFST" decimal(4,2) NOT NULL default '0.00',
  "AAABJFSB" varchar(8) NOT NULL default '00000000',
  "AAABJFSC" varchar(8) NOT NULL default '00000000',
  "AAABJFSD" decimal(4,0) NOT NULL default '0',
  "AAABJFSE" varchar(1) NOT NULL default ' ',
  "AAABJFSF" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABJFSH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJGBD","AAABJFSL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFSM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJGBD" varchar(10) NOT NULL default ' ',
  "AAABJFSX" varchar(3) NOT NULL default '000',
  "AAABLPPS" varchar(10) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABJFST" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJGBD","AAABJFSX"),
  KEY "AAABJFSY" ("AAABMRIL","AAABLXTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFTB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABJFTF" varchar(1) NOT NULL default ' ',
  "AAABJFTG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMPWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFTK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABJFTO" varchar(1) NOT NULL default ' ',
  "AAABJFTP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMPWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFTT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJGBD" varchar(10) NOT NULL default ' ',
  "AAABJGAV" varchar(3) NOT NULL default '000',
  "AAABJFVR" varchar(2) NOT NULL default ' ',
  "AAABJFTZ" varchar(40) NOT NULL default ' ',
  "AAABLKQL" varchar(12) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABLUMH" varchar(12) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJGBD","AAABJGAV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFUI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJGBD" varchar(10) NOT NULL default ' ',
  "AAABJGAV" varchar(3) NOT NULL default '000',
  "AAABJGAW" varchar(3) NOT NULL default '000',
  "AAABJFVR" varchar(2) NOT NULL default ' ',
  "AAABJFVS" varchar(2) NOT NULL default ' ',
  "AAABJFUQ" varchar(1) NOT NULL default ' ',
  "AAABLOBA" varchar(8) NOT NULL default '00000000',
  "AAABJFVT" varchar(8) NOT NULL default '00000000',
  "AAABJFUT" varchar(8) NOT NULL default '00000000',
  "AAABKGOS" varchar(4) NOT NULL default '0000',
  "AAABJFUV" varchar(2) NOT NULL default '00',
  "AAABLPMG" varchar(2) NOT NULL default ' ',
  "AAABLOAS" varchar(16) NOT NULL default ' ',
  "AAABMCOT" varchar(2) NOT NULL default ' ',
  "AAABJFUZ" varchar(3) NOT NULL default '000',
  "AAABJFVA" varchar(3) NOT NULL default '000',
  "AAABJFVB" varchar(3) NOT NULL default '000',
  "AAABJFVC" varchar(3) NOT NULL default '000',
  "AAABLFYC" varchar(5) NOT NULL default ' ',
  "AAABKGPK" varchar(20) NOT NULL default ' ',
  "AAABJFVF" varchar(10) NOT NULL default ' ',
  "AAABLKQL" varchar(12) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABLUMH" varchar(12) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJGBD","AAABJGAV","AAABJGAW"),
  KEY "AAABJFVP" ("AAABMRIL","AAABJFVR","AAABJFVS","AAABJFVT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJGBD" varchar(10) NOT NULL default ' ',
  "AAABJGAV" varchar(3) NOT NULL default '000',
  "AAABJGAW" varchar(3) NOT NULL default '000',
  "AAABJFYY" varchar(3) NOT NULL default '000',
  "AAABJFWB" varchar(1) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABLPPS" varchar(10) NOT NULL default ' ',
  "AAABLPMG" varchar(2) NOT NULL default ' ',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABJFXX" decimal(13,2) NOT NULL default '0.00',
  "AAABJFWI" decimal(13,2) NOT NULL default '0.00',
  "AAABJFWJ" decimal(13,2) NOT NULL default '0.00',
  "AAABJFWK" decimal(13,2) NOT NULL default '0.00',
  "AAABJFWL" decimal(13,2) NOT NULL default '0.00',
  "AAABJFWM" varchar(1) NOT NULL default ' ',
  "AAABJFWN" varchar(1) NOT NULL default ' ',
  "AAABLOCD" varchar(50) NOT NULL default ' ',
  "AAABJFWP" varchar(4) NOT NULL default ' ',
  "AAABJFWQ" varchar(3) NOT NULL default ' ',
  "AAABJFWR" decimal(7,3) NOT NULL default '0.000',
  "AAABJFWS" varchar(15) NOT NULL default ' ',
  "AAABJFWT" varchar(15) NOT NULL default ' ',
  "AAABJFWU" varchar(1) NOT NULL default ' ',
  "AAABJFWV" varchar(4) NOT NULL default ' ',
  "AAABJFWW" varchar(8) NOT NULL default '00000000',
  "AAABJFWX" decimal(3,0) NOT NULL default '0',
  "AAABJFWY" decimal(3,0) NOT NULL default '0',
  "AAABJFWZ" decimal(3,0) NOT NULL default '0',
  "AAABJFXA" decimal(5,3) NOT NULL default '0.000',
  "AAABJFXB" decimal(5,3) NOT NULL default '0.000',
  "AAABJFXC" varchar(2) NOT NULL default ' ',
  "AAABJFYN" varchar(6) NOT NULL default '000000',
  "AAABJFXE" varchar(4) NOT NULL default ' ',
  "AAABJFXF" varchar(4) NOT NULL default ' ',
  "AAABJFXG" varchar(3) NOT NULL default '000',
  "AAABJFXH" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJGBD","AAABJGAV","AAABJGAW","AAABJFYY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFXO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJGBD" varchar(10) NOT NULL default ' ',
  "AAABJGAV" varchar(3) NOT NULL default '000',
  "AAABJGAW" varchar(3) NOT NULL default '000',
  "AAABJFYY" varchar(3) NOT NULL default '000',
  "AAABJFYE" varchar(2) NOT NULL default ' ',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABJFXX" decimal(23,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJGBD","AAABJGAV","AAABJGAW","AAABJFYY","AAABJFYE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJGBD" varchar(10) NOT NULL default ' ',
  "AAABJGAV" varchar(3) NOT NULL default '000',
  "AAABJGAW" varchar(3) NOT NULL default '000',
  "AAABJFYY" varchar(3) NOT NULL default '000',
  "AAABJFYZ" varchar(3) NOT NULL default '000',
  "AAABJFYN" varchar(6) NOT NULL default '000000',
  "AAABLTNT" varchar(2) NOT NULL default ' ',
  "AAABLTNU" varchar(2) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABLOGQ" decimal(15,2) NOT NULL default '0.00',
  "AAABLOHZ" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJGBD","AAABJGAV","AAABJGAW","AAABJFYY","AAABJFYZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFZA" (
  "AAABMBXY" varchar(50) NOT NULL default ' ',
  "AAABLTWP" varchar(20) NOT NULL default ' ',
  "AAABLTWQ" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMBXY"),
  KEY "AAABJFZF" ("AAABLTWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJFZH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABJGBD" varchar(10) NOT NULL default ' ',
  "AAABJGAV" varchar(3) NOT NULL default '000',
  "AAABJGAW" varchar(3) NOT NULL default '000',
  "AAABJGAL" varchar(1) NOT NULL default ' ',
  "AAABMCPM" varchar(8) NOT NULL default '00000000',
  "AAABMCPO" varchar(10) NOT NULL default ' ',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBB","AAABMDBA","AAABMPWO","AAABJGBD","AAABJGAV","AAABJGAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGAC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABJGBD" varchar(10) NOT NULL default ' ',
  "AAABJGAV" varchar(3) NOT NULL default '000',
  "AAABJGAW" varchar(3) NOT NULL default '000',
  "AAABJGAL" varchar(1) NOT NULL default ' ',
  "AAABMCPM" varchar(8) NOT NULL default '00000000',
  "AAABMCPO" varchar(10) NOT NULL default ' ',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBB","AAABMDBA","AAABMPWO","AAABJGBD","AAABJGAV","AAABJGAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGAX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABJGBH" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJGBD" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXIY","AAABMPWP","AAABJGBH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGBI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGBU" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGBN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJGBU" varchar(2) NOT NULL default ' ',
  "AAABJGBR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJGBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGBV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGCH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGCH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGCA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJGCH" varchar(2) NOT NULL default ' ',
  "AAABJGCE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJGCH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGCI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGDC" varchar(10) NOT NULL default '0000000000',
  "AAABLXSX" varchar(8) NOT NULL default '00000000',
  "AAABJGCM" varchar(8) NOT NULL default '00000000',
  "AAABJGCN" varchar(8) NOT NULL default '00000000',
  "AAABKRFT" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKPXV" varchar(3) NOT NULL default '000',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABJGCS" varchar(50) NOT NULL default ' ',
  "AAABJGCT" decimal(16,0) NOT NULL default '0',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABJGCX" decimal(16,0) NOT NULL default '0',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABMAKM" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGDC"),
  KEY "AAABJGDD" ("AAABMRIL","AAABLXSX","AAABMDBB","AAABKPXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGDI" (
  "AAABJGDO" varchar(1) NOT NULL default ' ',
  "AAABJGDK" varchar(4) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAABKRVS" varchar(1) NOT NULL default '0',
  "AAABLRNO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJGDO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGDP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRBY" varchar(7) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABJGDU" varchar(1) NOT NULL default ' ',
  "AAABJGHU" varchar(1) NOT NULL default ' ',
  "AAABJGDW" varchar(1) NOT NULL default ' ',
  "AAABJGDX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRBY","AAABMCRJ","AAABKOTY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGEC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRBY" varchar(7) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRBY","AAABMCRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGEK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXSX" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLXSX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGEP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABJGIL" varchar(3) NOT NULL default ' ',
  "AAABJGET" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGEW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRBY" varchar(7) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default ' ',
  "AAABJSLF" varchar(30) NOT NULL default ' ',
  "AAABJGGK" varchar(2) NOT NULL default ' ',
  "AAABJGGL" varchar(42) NOT NULL default ' ',
  "AAABJGGM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRBY","AAABMCRJ","AAABMRHS","AAABMQBS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGFL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABJGIL" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGFR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRBY" varchar(7) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJSLF" varchar(30) NOT NULL default ' ',
  "AAABJGGK" varchar(2) NOT NULL default ' ',
  "AAABJGGL" varchar(42) NOT NULL default ' ',
  "AAABJGGM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRBY","AAABMCRJ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGGE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRBY" varchar(7) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJSLF" varchar(30) NOT NULL default ' ',
  "AAABJGGK" varchar(2) NOT NULL default ' ',
  "AAABJGGL" varchar(42) NOT NULL default ' ',
  "AAABJGGM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRBY","AAABMCRJ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGGR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABJGHG" varchar(3) NOT NULL default ' ',
  "AAABKRGH" varchar(6) NOT NULL default ' ',
  "AAABJGGW" varchar(3) NOT NULL default ' ',
  "AAABJGGX" decimal(15,2) NOT NULL default '0.00',
  "AAABJGGY" varchar(3) NOT NULL default ' ',
  "AAABJGGZ" varchar(3) NOT NULL default ' ',
  "AAABJGHA" varchar(3) NOT NULL default ' ',
  "AAABJGHB" varchar(3) NOT NULL default ' ',
  "AAABJGHC" varchar(3) NOT NULL default ' ',
  "AAABJGHD" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABJGHG","AAABKRGH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGHI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGHU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGHU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGHN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJGHU" varchar(1) NOT NULL default ' ',
  "AAABMOHD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJGHU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGHV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABJGHZ" varchar(10) NOT NULL default ' ',
  "AAABJGIA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABKOTY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGIE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGIG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGII" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABJGIL" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGIO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRBY" varchar(7) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default ' ',
  "AAABJGIT" varchar(30) NOT NULL default ' ',
  "AAABJGIU" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRBY","AAABMCRJ","AAABMQBS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGIZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGJI" varchar(4) NOT NULL default ' ',
  "AAABJGJJ" varchar(5) NOT NULL default ' ',
  "AAABLTWK" varchar(40) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABKWJY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGJI","AAABJGJJ","AAABLTWK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGJL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGKI" varchar(4) NOT NULL default ' ',
  "AAABJGJO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGKI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGJR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGKI" varchar(4) NOT NULL default ' ',
  "AAABJGKJ" varchar(5) NOT NULL default ' ',
  "AAABJGKG" varchar(1) NOT NULL default ' ',
  "AAABJGJW" varchar(1) NOT NULL default ' ',
  "AAABJGJX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGKI","AAABJGKJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGKB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGKI" varchar(4) NOT NULL default ' ',
  "AAABJGKJ" varchar(5) NOT NULL default ' ',
  "AAABJGKK" varchar(8) NOT NULL default ' ',
  "AAABJGKG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGKI","AAABJGKJ","AAABJGKK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGKL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGKT" varchar(4) NOT NULL default ' ',
  "AAABJGKU" varchar(4) NOT NULL default ' ',
  "AAABJGKV" varchar(5) NOT NULL default ' ',
  "AAABJGKQ" varchar(1) NOT NULL default ' ',
  "AAABJGKR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGKT","AAABJGKU","AAABJGKV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGLB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGLN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGLN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGLG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJGLN" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJGLN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGLO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGMA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGMA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGLT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJGMA" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJGMA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGMB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOSZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGMG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLOSZ" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLOSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGMO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGNH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGNH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGMT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJGNH" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJGNH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGNB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGNS" varchar(10) NOT NULL default ' ',
  "AAABLDCS" varchar(10) NOT NULL default ' ',
  "AAABLOSZ" varchar(10) NOT NULL default ' ',
  "AAABLDDF" varchar(10) NOT NULL default ' ',
  "AAABJGNH" varchar(10) NOT NULL default ' ',
  "AAABLDEC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJGNL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJGNS" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJGNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--enable_query_log
