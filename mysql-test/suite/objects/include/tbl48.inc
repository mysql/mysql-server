# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+
# |                             tbl48.inc                              |
# |                        Copyright 2006 by MySQL AB                  |
# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+

--disable_query_log
CREATE TABLE "AAABLVKY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABLVLB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVLE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMRQ" varchar(4) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMRQ","AAABMLHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVLL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMRQ" varchar(4) NOT NULL default ' ',
  "AAABLVLO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMRQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVLR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXFC" varchar(4) NOT NULL default ' ',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMNYU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVLY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMOAI" varchar(4) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABLZUS" varchar(1) NOT NULL default ' ',
  "AAABLVMP" varchar(1) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABMCTH" varchar(1) NOT NULL default ' ',
  "AAABLXAT" varchar(1) NOT NULL default ' ',
  "AAABLXAU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMOAI","AAABMPLS","AAABLZUS","AAABLVMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVMQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWLL" varchar(10) NOT NULL default ' ',
  "AAABLWIT" varchar(21) NOT NULL default ' ',
  "AAABLWIU" varchar(1) NOT NULL default ' ',
  "AAABLWIV" varchar(21) NOT NULL default ' ',
  "AAABLWIW" varchar(1) NOT NULL default ' ',
  "AAABLWIX" varchar(21) NOT NULL default ' ',
  "AAABLWIY" varchar(1) NOT NULL default ' ',
  "AAABLWIZ" varchar(21) NOT NULL default ' ',
  "AAABLWJA" varchar(1) NOT NULL default ' ',
  "AAABLWJB" varchar(21) NOT NULL default ' ',
  "AAABLWJC" varchar(1) NOT NULL default ' ',
  "AAABLWJD" varchar(21) NOT NULL default ' ',
  "AAABLWJE" varchar(1) NOT NULL default ' ',
  "AAABLWJF" varchar(21) NOT NULL default ' ',
  "AAABLWJG" varchar(1) NOT NULL default ' ',
  "AAABLWJH" varchar(21) NOT NULL default ' ',
  "AAABLWJI" varchar(1) NOT NULL default ' ',
  "AAABLWJJ" varchar(21) NOT NULL default ' ',
  "AAABLWJK" varchar(1) NOT NULL default ' ',
  "AAABLVNL" varchar(21) NOT NULL default ' ',
  "AAABLVNM" varchar(1) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLWVZ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLWLL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVNS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOIB" varchar(2) NOT NULL default ' ',
  "AAABLVNV" varchar(3) NOT NULL default '000',
  "AAABLVNW" varchar(3) NOT NULL default '000',
  "AAABLVNX" varchar(3) NOT NULL default '000',
  "AAABLVNY" varchar(3) NOT NULL default '000',
  "AAABLVNZ" varchar(3) NOT NULL default '000',
  "AAABLVOA" varchar(3) NOT NULL default '000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMOIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVOG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOIB" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLVOK" varchar(25) NOT NULL default ' ',
  "AAABLVOL" varchar(25) NOT NULL default ' ',
  "AAABLVOM" varchar(25) NOT NULL default ' ',
  "AAABLVON" varchar(25) NOT NULL default ' ',
  "AAABLVOO" varchar(25) NOT NULL default ' ',
  "AAABLVOP" varchar(25) NOT NULL default ' ',
  "AAABLVOQ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOIB","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLTD" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLWLL" varchar(25) NOT NULL default ' ',
  "AAABLVOZ" varchar(2) NOT NULL default '00',
  "AAABLYMX" varchar(2) NOT NULL default '00',
  "AAABMECK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLTD","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVPF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLTD" varchar(3) NOT NULL default '000',
  "AAABLYMX" varchar(2) NOT NULL default '00',
  "AAABLVPJ" varchar(2) NOT NULL default '00',
  "AAABLVPK" varchar(2) NOT NULL default '00',
  "AAABLVPL" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVPO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLGN" varchar(4) NOT NULL default ' ',
  "AAABLVPR" varchar(1) NOT NULL default ' ',
  "AAABLVPS" varchar(1) NOT NULL default ' ',
  "AAABLVPT" varchar(1) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMLGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVPZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVQE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNDR" varchar(25) NOT NULL default ' ',
  "AAABMOIB" varchar(2) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMNDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVQN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMNDR" varchar(25) NOT NULL default ' ',
  "AAABMOIB" varchar(2) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLWVZ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMNDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABMNDR" varchar(25) NOT NULL default ' ',
  "AAABMOIB" varchar(2) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLWVZ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMLEZ","AAABMNDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVRL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABMNDR" varchar(25) NOT NULL default ' ',
  "AAABMOIB" varchar(2) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLWVZ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMLEZ","AAABMNDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVRY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMNDR" varchar(25) NOT NULL default ' ',
  "AAABMOIB" varchar(2) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLWVZ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABMNDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVSN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWBR" varchar(4) NOT NULL default ' ',
  "AAABLVSQ" varchar(4) NOT NULL default '0000',
  "AAABLVSR" varchar(1) NOT NULL default '0',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLWBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVSV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWBR" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAGQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLWBR","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVTD" (
  "AAABLVTI" decimal(13,3) NOT NULL default '0.000',
  "AAABLVTJ" decimal(13,3) NOT NULL default '0.000',
  "AAABLVTK" decimal(13,3) NOT NULL default '0.000',
  "AAABLVTL" decimal(13,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABLVTI","AAABLVTJ","AAABLVTK","AAABLVTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVTM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWIG" varchar(10) NOT NULL default ' ',
  "AAABLVVL" varchar(6) NOT NULL default '000000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABLWHG" varchar(10) NOT NULL default ' ',
  "AAABLYAT" varchar(10) NOT NULL default ' ',
  "AAABLWHH" varchar(10) NOT NULL default ' ',
  "AAABLWHI" varchar(10) NOT NULL default ' ',
  "AAABLWBR" varchar(4) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLVTZ" decimal(11,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLWIE" varchar(1) NOT NULL default ' ',
  "AAABLVUC" decimal(15,3) NOT NULL default '0.000',
  "AAABLVUD" decimal(19,2) NOT NULL default '0.00',
  "AAABLVUE" decimal(15,3) NOT NULL default '0.000',
  "AAABLVUF" decimal(19,2) NOT NULL default '0.00',
  "AAABLVUG" decimal(15,3) NOT NULL default '0.000',
  "AAABLVUH" decimal(19,2) NOT NULL default '0.00',
  "AAABLVUI" decimal(15,3) NOT NULL default '0.000',
  "AAABLVUJ" decimal(19,2) NOT NULL default '0.00',
  "AAABLVUK" decimal(15,3) NOT NULL default '0.000',
  "AAABLVUL" decimal(19,2) NOT NULL default '0.00',
  "AAABLVUM" decimal(15,3) NOT NULL default '0.000',
  "AAABLVUN" decimal(19,2) NOT NULL default '0.00',
  "AAABLVUO" decimal(15,3) NOT NULL default '0.000',
  "AAABLVUP" decimal(19,2) NOT NULL default '0.00',
  "AAABLVUQ" decimal(15,3) NOT NULL default '0.000',
  "AAABLVUR" decimal(19,2) NOT NULL default '0.00',
  "AAABLVUS" decimal(15,3) NOT NULL default '0.000',
  "AAABLVUT" decimal(19,2) NOT NULL default '0.00',
  "AAABLVUU" decimal(15,3) NOT NULL default '0.000',
  "AAABLVUV" decimal(19,2) NOT NULL default '0.00',
  "AAABLVUW" decimal(15,3) NOT NULL default '0.000',
  "AAABLVUX" decimal(19,2) NOT NULL default '0.00',
  "AAABLVUY" decimal(15,3) NOT NULL default '0.000',
  "AAABLVUZ" decimal(19,2) NOT NULL default '0.00',
  "AAABLVVA" decimal(15,3) NOT NULL default '0.000',
  "AAABLVVB" decimal(19,2) NOT NULL default '0.00',
  "AAABLVVC" decimal(15,3) NOT NULL default '0.000',
  "AAABLVVD" decimal(19,2) NOT NULL default '0.00',
  "AAABLVVE" decimal(15,3) NOT NULL default '0.000',
  "AAABLVVF" decimal(19,2) NOT NULL default '0.00',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKM" varchar(6) NOT NULL default '000000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLWIG","AAABLVVL","AAABMQII","AAABMJXV","AAABMJXW","AAABLWHG","AAABLYAT","AAABLWHH","AAABLWHI","AAABLWBR","AAABMLCZ"),
  KEY "AAABLVVV" ("AAABMRIL","AAABLWIG","AAABMQII","AAABMJXV","AAABMJXW","AAABLWHG","AAABLYAT","AAABLWHH","AAABLWHI","AAABLWBR","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVWH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWIG" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLWIG","AAABMQII","AAABMJXV","AAABMJXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVWS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWIG" varchar(10) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMRCU" varchar(15) NOT NULL default '000000000000000',
  "AAABLWBR" varchar(4) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLWHG" varchar(10) NOT NULL default ' ',
  "AAABLYAT" varchar(10) NOT NULL default ' ',
  "AAABLWHH" varchar(10) NOT NULL default ' ',
  "AAABLWHI" varchar(10) NOT NULL default ' ',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAABLWBH" varchar(10) NOT NULL default ' ',
  "AAABLWIP" varchar(6) NOT NULL default '000000',
  "AAABMQIK" varchar(8) NOT NULL default '00000000',
  "AAABLWBE" varchar(1) NOT NULL default ' ',
  "AAABLWBD" varchar(1) NOT NULL default ' ',
  "AAABMRFR" varchar(6) NOT NULL default '000000',
  "AAABLZJP" varchar(3) NOT NULL default ' ',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABLXPV" decimal(5,0) NOT NULL default '0',
  "AAABLXPW" decimal(5,0) NOT NULL default '0',
  "AAABLVXR" decimal(13,3) NOT NULL default '0.000',
  "AAABLXQC" varchar(10) NOT NULL default ' ',
  "AAABLXQD" varchar(6) NOT NULL default '000000',
  "AAABLYWS" varchar(10) NOT NULL default ' ',
  "AAABLYFT" varchar(6) NOT NULL default '000000',
  "AAABLYAB" varchar(10) NOT NULL default ' ',
  "AAABLVXX" varchar(6) NOT NULL default '000000',
  "AAABLVXY" decimal(13,3) NOT NULL default '0.000',
  "AAABLVXZ" decimal(11,2) NOT NULL default '0.00',
  "AAABLVYA" varchar(5) NOT NULL default ' ',
  "AAABLVYB" varchar(5) NOT NULL default ' ',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABLVYD" decimal(15,2) NOT NULL default '0.00',
  "AAABLVYE" decimal(15,2) NOT NULL default '0.00',
  "AAABLVYF" decimal(9,5) NOT NULL default '0.00000',
  "AAABLVYG" decimal(15,2) NOT NULL default '0.00',
  "AAABLVYH" decimal(9,5) NOT NULL default '0.00000',
  "AAABLVYI" varchar(8) NOT NULL default '00000000',
  "AAABLWIE" varchar(1) NOT NULL default ' ',
  "AAABLWFM" decimal(15,3) NOT NULL default '0.000',
  "AAABLWFN" decimal(19,2) NOT NULL default '0.00',
  "AAABLWFO" decimal(15,3) NOT NULL default '0.000',
  "AAABLWFP" decimal(19,2) NOT NULL default '0.00',
  "AAABLWFQ" decimal(15,3) NOT NULL default '0.000',
  "AAABLWFR" decimal(19,2) NOT NULL default '0.00',
  "AAABLWFS" decimal(15,3) NOT NULL default '0.000',
  "AAABLWFT" decimal(19,2) NOT NULL default '0.00',
  "AAABLWFU" decimal(15,3) NOT NULL default '0.000',
  "AAABLWFV" decimal(19,2) NOT NULL default '0.00',
  "AAABLWFW" decimal(15,3) NOT NULL default '0.000',
  "AAABLWFX" decimal(19,2) NOT NULL default '0.00',
  "AAABLWFY" decimal(15,3) NOT NULL default '0.000',
  "AAABLWFZ" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGA" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGB" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGC" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGD" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGE" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGF" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGG" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGH" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGI" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGJ" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGK" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGL" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGM" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGN" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGO" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGP" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGQ" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGR" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGS" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGT" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGU" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGV" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGW" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGX" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGY" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGZ" decimal(19,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLWIG","AAABMLER","AAABMQII","AAABMJXV","AAABMJXW","AAABMRCU"),
  KEY "AAABLWAF" ("AAABMRIL","AAABLWIG","AAABMQII","AAABMJXV","AAABMJXW","AAABLWBR","AAABMLCZ","AAABLWBE","AAABLWBD"),
  KEY "AAABLWAP" ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABLWBH","AAABLWBD","AAABMLER"),
  KEY "AAABLWAX" ("AAABMRIL","AAABLWIG","AAABMQII","AAABMJXV","AAABMJXW","AAABLWBD","AAABLWBE"),
  KEY "AAABLWBF" ("AAABMRIL","AAABLWBH","AAABLWIP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWBJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWIG" varchar(10) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMRCU" varchar(15) NOT NULL default '000000000000000',
  "AAABLWBR" varchar(4) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMQIK" varchar(8) NOT NULL default '00000000',
  "AAABLWEA" varchar(10) NOT NULL default ' ',
  "AAABLWEB" varchar(8) NOT NULL default '00000000',
  "AAABLWIE" varchar(1) NOT NULL default ' ',
  "AAABLWFM" decimal(15,3) NOT NULL default '0.000',
  "AAABLWFN" decimal(19,2) NOT NULL default '0.00',
  "AAABLWFO" decimal(15,3) NOT NULL default '0.000',
  "AAABLWFP" decimal(19,2) NOT NULL default '0.00',
  "AAABLWFQ" decimal(15,3) NOT NULL default '0.000',
  "AAABLWFR" decimal(19,2) NOT NULL default '0.00',
  "AAABLWFS" decimal(15,3) NOT NULL default '0.000',
  "AAABLWFT" decimal(19,2) NOT NULL default '0.00',
  "AAABLWFU" decimal(15,3) NOT NULL default '0.000',
  "AAABLWFV" decimal(19,2) NOT NULL default '0.00',
  "AAABLWFW" decimal(15,3) NOT NULL default '0.000',
  "AAABLWFX" decimal(19,2) NOT NULL default '0.00',
  "AAABLWFY" decimal(15,3) NOT NULL default '0.000',
  "AAABLWFZ" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGA" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGB" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGC" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGD" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGE" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGF" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGG" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGH" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGI" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGJ" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGK" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGL" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGM" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGN" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGO" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGP" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGQ" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGR" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGS" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGT" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGU" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGV" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGW" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGX" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGY" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGZ" decimal(19,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLWIG","AAABMLER","AAABMQII","AAABMJXV","AAABMJXW","AAABMRCU"),
  KEY "AAABLWDV" ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABLWEA","AAABLWEB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWEC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWLL" varchar(3) NOT NULL default ' ',
  "AAABLWKZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLWLL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWEI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWLL" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAGQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLWLL","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMRCU" varchar(15) NOT NULL default '000000000000000',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABLWEW" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQIL","AAABMRCU","AAABMLER","AAABLZVN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWFC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLWHG" varchar(10) NOT NULL default ' ',
  "AAABLWHH" varchar(10) NOT NULL default ' ',
  "AAABLWHI" varchar(10) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMRCU" varchar(15) NOT NULL default '000000000000000',
  "AAABLYAG" varchar(1) NOT NULL default ' ',
  "AAABLWFM" decimal(15,3) NOT NULL default '0.000',
  "AAABLWFN" decimal(19,2) NOT NULL default '0.00',
  "AAABLWFO" decimal(15,3) NOT NULL default '0.000',
  "AAABLWFP" decimal(19,2) NOT NULL default '0.00',
  "AAABLWFQ" decimal(15,3) NOT NULL default '0.000',
  "AAABLWFR" decimal(19,2) NOT NULL default '0.00',
  "AAABLWFS" decimal(15,3) NOT NULL default '0.000',
  "AAABLWFT" decimal(19,2) NOT NULL default '0.00',
  "AAABLWFU" decimal(15,3) NOT NULL default '0.000',
  "AAABLWFV" decimal(19,2) NOT NULL default '0.00',
  "AAABLWFW" decimal(15,3) NOT NULL default '0.000',
  "AAABLWFX" decimal(19,2) NOT NULL default '0.00',
  "AAABLWFY" decimal(15,3) NOT NULL default '0.000',
  "AAABLWFZ" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGA" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGB" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGC" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGD" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGE" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGF" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGG" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGH" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGI" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGJ" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGK" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGL" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGM" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGN" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGO" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGP" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGQ" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGR" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGS" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGT" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGU" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGV" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGW" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGX" decimal(19,2) NOT NULL default '0.00',
  "AAABLWGY" decimal(15,3) NOT NULL default '0.000',
  "AAABLWGZ" decimal(19,2) NOT NULL default '0.00',
  "AAABLXYY" decimal(11,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQIL","AAABMLCZ","AAABLWHG","AAABLWHH","AAABLWHI","AAABMRFQ","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWHL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWHQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWIG" varchar(10) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMRCU" varchar(15) NOT NULL default '000000000000000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABLWIP" varchar(6) NOT NULL default '000000',
  "AAABLWIA" varchar(1) NOT NULL default ' ',
  "AAABMAMV" varchar(4) NOT NULL default ' ',
  "AAABLZDH" varchar(6) NOT NULL default '000000',
  "AAABLZDI" varchar(12) NOT NULL default ' ',
  "AAABLWIE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLWIG","AAABMLER","AAABMQII","AAABMJXV","AAABMJXW","AAABMRCU"),
  KEY "AAABLWIM" ("AAABMRIL","AAABMQIL","AAABLWIP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWIQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWLL" varchar(3) NOT NULL default ' ',
  "AAABLWIT" varchar(3) NOT NULL default ' ',
  "AAABLWIU" varchar(1) NOT NULL default ' ',
  "AAABLWIV" varchar(3) NOT NULL default ' ',
  "AAABLWIW" varchar(1) NOT NULL default ' ',
  "AAABLWIX" varchar(3) NOT NULL default ' ',
  "AAABLWIY" varchar(1) NOT NULL default ' ',
  "AAABLWIZ" varchar(3) NOT NULL default ' ',
  "AAABLWJA" varchar(1) NOT NULL default ' ',
  "AAABLWJB" varchar(3) NOT NULL default ' ',
  "AAABLWJC" varchar(1) NOT NULL default ' ',
  "AAABLWJD" varchar(3) NOT NULL default ' ',
  "AAABLWJE" varchar(1) NOT NULL default ' ',
  "AAABLWJF" varchar(3) NOT NULL default ' ',
  "AAABLWJG" varchar(1) NOT NULL default ' ',
  "AAABLWJH" varchar(3) NOT NULL default ' ',
  "AAABLWJI" varchar(1) NOT NULL default ' ',
  "AAABLWJJ" varchar(3) NOT NULL default ' ',
  "AAABLWJK" varchar(1) NOT NULL default ' ',
  "AAABLWJL" varchar(3) NOT NULL default ' ',
  "AAABLWJM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLWLL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWJP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWLL" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLWJT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLWLL","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWJX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWKC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWKE" varchar(2) NOT NULL default ' ',
  "AAABLWKF" varchar(2) NOT NULL default ' ',
  "AAABLWKG" varchar(2) NOT NULL default ' ',
  "AAABLWKH" varchar(2) NOT NULL default ' ',
  "AAABLWKI" varchar(1) NOT NULL default ' ',
  "AAABLWKJ" varchar(1) NOT NULL default ' ',
  "AAABLWKK" varchar(1) NOT NULL default ' ',
  "AAABLWKL" varchar(1) NOT NULL default ' ',
  "AAABLWKM" varchar(1) NOT NULL default ' ',
  "AAABMNYU" varchar(16) NOT NULL default ' ',
  "AAABLWKO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWKQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWKS" varchar(3) NOT NULL default ' ',
  "AAABLWKT" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWKV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWLL" varchar(3) NOT NULL default ' ',
  "AAABMLGN" varchar(4) NOT NULL default ' ',
  "AAABLWKZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLWLL","AAABMLGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWLD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWLL" varchar(3) NOT NULL default ' ',
  "AAABLWLG" varchar(21) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLWVZ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLWLL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWLM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABLWLS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABMLHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWLY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABLWQI" varchar(2) NOT NULL default ' ',
  "AAABLWQJ" varchar(2) NOT NULL default ' ',
  "AAABLWOE" varchar(1) NOT NULL default ' ',
  "AAABLWPT" varchar(5) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABLWPB" varchar(5) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLZMW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLEZ","AAABLWQI","AAABLWQJ","AAABLWOE","AAABLWPT","AAABMNZR"),
  KEY "AAABLWMR" ("AAABMNZR","AAABMCJZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWMU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABLWQI" varchar(2) NOT NULL default ' ',
  "AAABLWQJ" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLEZ","AAABLWQI","AAABLWQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWNG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLWNJ" varchar(4) NOT NULL default ' ',
  "AAABLWNK" varchar(1) NOT NULL default ' ',
  "AAABLWNL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWNO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABLWQI" varchar(2) NOT NULL default ' ',
  "AAABLWQJ" varchar(2) NOT NULL default ' ',
  "AAABLWOE" varchar(1) NOT NULL default ' ',
  "AAABLWPT" varchar(5) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABLWPB" varchar(5) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLEZ","AAABLWQI","AAABLWQJ","AAABLWOE","AAABLWPT","AAABMNZR"),
  KEY "AAABLWOH" ("AAABMNZR","AAABMCJZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWOK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABLWQI" varchar(2) NOT NULL default ' ',
  "AAABLWQJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLEZ","AAABLWQI","AAABLWQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWOT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABLWQI" varchar(2) NOT NULL default ' ',
  "AAABLWQJ" varchar(2) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABLWPT" varchar(5) NOT NULL default ' ',
  "AAABLWPU" varchar(4) NOT NULL default '0000',
  "AAABLWPB" varchar(5) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLWPD" varchar(1) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMCRM" varchar(10) NOT NULL default ' ',
  "AAABLWQC" varchar(10) NOT NULL default ' ',
  "AAABLWPH" varchar(10) NOT NULL default ' ',
  "AAABLWPI" varchar(5) NOT NULL default ' ',
  "AAABLWPJ" decimal(13,3) NOT NULL default '0.000',
  "AAABLYKR" varchar(3) NOT NULL default ' ',
  "AAABLXGN" varchar(30) NOT NULL default ' ',
  "AAABLWPM" varchar(1) NOT NULL default ' ',
  "AAABLWPW" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLEZ","AAABLWQI","AAABLWQJ","AAABMRFQ","AAABLWPT","AAABLWPU"),
  KEY "AAABLWPV" ("AAABLWPW"),
  KEY "AAABLWPX" ("AAABMRFQ"),
  KEY "AAABLWPZ" ("AAABMCRM"),
  KEY "AAABLWQB" ("AAABLWQC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWQD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABLWQI" varchar(2) NOT NULL default ' ',
  "AAABLWQJ" varchar(2) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWQN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABLYNC" varchar(3) NOT NULL default ' ',
  "AAABLYKS" decimal(3,1) NOT NULL default '0.0',
  PRIMARY KEY ("AAABMRIL","AAABMQII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWQU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAABLWQY" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMRFE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWRC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXNA" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWRH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXNA" varchar(3) NOT NULL default ' ',
  "AAABMAGQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWRP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZFC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWRU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABLWSY" varchar(1) NOT NULL default ' ',
  "AAABLWSZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABMLHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWSH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABLZZR" varchar(4) NOT NULL default ' ',
  "AAABLWSY" varchar(1) NOT NULL default ' ',
  "AAABLWSZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMLEZ","AAABLZZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWSS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABLZFC" varchar(4) NOT NULL default ' ',
  "AAABLWSY" varchar(1) NOT NULL default ' ',
  "AAABLWSZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABLZFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWTF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXYW" varchar(4) NOT NULL default ' ',
  "AAABLWTI" varchar(1) NOT NULL default ' ',
  "AAABLWTS" varchar(1) NOT NULL default ' ',
  "AAABLWTT" varchar(1) NOT NULL default ' ',
  "AAABLWTY" varchar(1) NOT NULL default ' ',
  "AAABLWUC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXYW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWTP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWUF" varchar(1) NOT NULL default ' ',
  "AAABLWTS" varchar(1) NOT NULL default ' ',
  "AAABLWTT" varchar(1) NOT NULL default ' ',
  "AAABLWTU" varchar(1) NOT NULL default ' ',
  "AAABLWTV" varchar(1) NOT NULL default ' ',
  "AAABLWTW" varchar(1) NOT NULL default ' ',
  "AAABLWZY" varchar(1) NOT NULL default ' ',
  "AAABLWTY" varchar(1) NOT NULL default ' ',
  "AAABLWTZ" varchar(1) NOT NULL default ' ',
  "AAABLWUA" varchar(1) NOT NULL default ' ',
  "AAABLWUB" varchar(1) NOT NULL default ' ',
  "AAABLWUC" varchar(1) NOT NULL default ' ',
  "AAABLWUD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLWUF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWUG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABLZCP" varchar(35) NOT NULL default ' ',
  "AAABLWUN" varchar(1) NOT NULL default ' ',
  "AAABLWZY" varchar(1) NOT NULL default ' ',
  "AAABLWUP" varchar(8) NOT NULL default '00000000',
  "AAABLWUQ" decimal(19,2) NOT NULL default '0.00',
  "AAABLWUR" decimal(19,2) NOT NULL default '0.00',
  "AAABLWUS" decimal(19,2) NOT NULL default '0.00',
  "AAABLXJF" varchar(5) NOT NULL default ' ',
  "AAABLXSA" varchar(6) NOT NULL default ' ',
  "AAABLWVO" varchar(8) NOT NULL default ' ',
  "AAABLWVP" varchar(3) NOT NULL default ' ',
  "AAABLWVQ" varchar(3) NOT NULL default ' ',
  "AAABMAMV" varchar(4) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABLWVC" varchar(6) NOT NULL default '000000',
  "AAABLWVD" varchar(6) NOT NULL default '000000',
  "AAABLXAB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPLT","AAABMQII","AAABMJXV","AAABMJXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWVK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABLWWC" varchar(6) NOT NULL default '000000',
  "AAABLWVO" varchar(8) NOT NULL default ' ',
  "AAABLWVP" varchar(3) NOT NULL default ' ',
  "AAABLWVQ" varchar(3) NOT NULL default ' ',
  "AAABMAMV" varchar(4) NOT NULL default ' ',
  "AAABLWVS" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(5) NOT NULL default ' ',
  "AAABLWVU" varchar(12) NOT NULL default ' ',
  "AAABLWVV" varchar(8) NOT NULL default '00000000',
  "AAABLWVW" varchar(6) NOT NULL default '000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLWVZ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMCJZ","AAABLWWC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWWD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWWI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABLWWN" varchar(8) NOT NULL default '00000000',
  "AAABLWWO" varchar(6) NOT NULL default '000000',
  "AAABLWWP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABMQIJ","AAABMNYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWWU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWXC" varchar(1) NOT NULL default ' ',
  "AAABMNDU" varchar(40) NOT NULL default ' ',
  "AAABMNYU" varchar(30) NOT NULL default ' ',
  "AAABLXGN" varchar(30) NOT NULL default ' ',
  "AAABMNDR" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLWXC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWXD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  "AAABLXYV" varchar(2) NOT NULL default '00',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLWXJ" varchar(10) NOT NULL default ' ',
  "AAABLXZI" varchar(1) NOT NULL default ' ',
  "AAABLXZJ" varchar(1) NOT NULL default ' ',
  "AAABLYAA" decimal(15,3) NOT NULL default '0.000',
  "AAABLXZK" varchar(3) NOT NULL default ' ',
  "AAABLWXO" decimal(15,2) NOT NULL default '0.00',
  "AAABLXZL" varchar(5) NOT NULL default ' ',
  "AAABLXZF" varchar(1) NOT NULL default ' ',
  "AAABLXYY" decimal(11,2) NOT NULL default '0.00',
  "AAABLXYZ" varchar(5) NOT NULL default ' ',
  "AAABLXZA" decimal(5,0) NOT NULL default '0',
  "AAABLXZB" varchar(3) NOT NULL default ' ',
  "AAABLWXV" varchar(4) NOT NULL default ' ',
  "AAABLXZC" decimal(5,0) NOT NULL default '0',
  "AAABLXZD" decimal(5,0) NOT NULL default '0',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABLWXZ" decimal(11,2) NOT NULL default '0.00',
  "AAABLWYA" decimal(11,2) NOT NULL default '0.00',
  "AAABLWYB" decimal(15,2) NOT NULL default '0.00',
  "AAABLWYC" decimal(15,2) NOT NULL default '0.00',
  "AAABLWYD" decimal(15,2) NOT NULL default '0.00',
  "AAABLXZG" varchar(5) NOT NULL default ' ',
  "AAABLWYF" decimal(15,2) NOT NULL default '0.00',
  "AAABLWYG" varchar(1) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMCTM" varchar(2) NOT NULL default ' ',
  "AAABLWYK" varchar(1) NOT NULL default ' ',
  "AAABLWYL" varchar(2) NOT NULL default '00',
  "AAABLXYX" varchar(18) NOT NULL default ' ',
  "AAABLXZM" decimal(11,2) NOT NULL default '0.00',
  "AAABLWYO" varchar(1) NOT NULL default ' ',
  "AAABLWZZ" varchar(1) NOT NULL default ' ',
  "AAABLXVJ" varchar(10) NOT NULL default ' ',
  "AAABLXVK" varchar(10) NOT NULL default ' ',
  "AAABLWYS" varchar(10) NOT NULL default ' ',
  "AAABLWYT" varchar(2) NOT NULL default '00',
  "AAABLWYU" varchar(8) NOT NULL default '00000000',
  "AAABMMRQ" varchar(4) NOT NULL default ' ',
  "AAABLWYW" varchar(2) NOT NULL default '00',
  "AAABLWYX" decimal(15,3) NOT NULL default '0.000',
  "AAABLWYY" decimal(15,3) NOT NULL default '0.000',
  "AAABLWYZ" decimal(13,2) NOT NULL default '0.00',
  "AAABLWZA" varchar(4) NOT NULL default '0000',
  "AAABLWZB" varchar(4) NOT NULL default '0000',
  "AAABLWZC" varchar(1) NOT NULL default ' ',
  "AAABLWZD" varchar(10) NOT NULL default ' ',
  "AAABLXYS" varchar(10) NOT NULL default ' ',
  "AAABMCTP" varchar(2) NOT NULL default ' ',
  "AAABMKSP" varchar(1) NOT NULL default ' ',
  "AAABMDGQ" varchar(13) NOT NULL default ' ',
  "AAABMOAI" varchar(4) NOT NULL default ' ',
  "AAABLWZJ" varchar(1) NOT NULL default ' ',
  "AAABLWZK" varchar(13) NOT NULL default ' ',
  "AAABLWZL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYAS","AAABLXYV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLWZP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXYO" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABLXJM" varchar(10) NOT NULL default ' ',
  "AAABLXYW" varchar(4) NOT NULL default ' ',
  "AAABLWZX" varchar(1) NOT NULL default ' ',
  "AAABLWZY" varchar(1) NOT NULL default ' ',
  "AAABLWZZ" varchar(1) NOT NULL default ' ',
  "AAABLXAA" varchar(8) NOT NULL default '00000000',
  "AAABLXAB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXYO","AAABMQII","AAABMJXV","AAABMJXW","AAABLXJM"),
  KEY "AAABLXAI" ("AAABLXJM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLXAK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMOAI" varchar(4) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABLZUS" varchar(1) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABMCTH" varchar(1) NOT NULL default ' ',
  "AAABLXAT" varchar(1) NOT NULL default ' ',
  "AAABLXAU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMOAI","AAABMPLS","AAABLZUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLXBA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABLXBE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMOAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLXBI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXGQ" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABLXCM" varchar(18) NOT NULL default ' ',
  "AAABLXBP" varchar(18) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLXGN" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXGQ","AAABMQII","AAABMJXV","AAABMJXW","AAABLXCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLXBZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXGQ" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABLXCM" varchar(18) NOT NULL default ' ',
  "AAABLXGN" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXGQ","AAABMQII","AAABMJXV","AAABMJXW","AAABLXCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLXCN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  "AAABLXEK" varchar(6) NOT NULL default '000000',
  "AAABLXDX" varchar(1) NOT NULL default ' ',
  "AAABLXDY" varchar(10) NOT NULL default ' ',
  "AAABLXDZ" varchar(4) NOT NULL default ' ',
  "AAABLXEA" varchar(2) NOT NULL default ' ',
  "AAABLXEB" varchar(2) NOT NULL default ' ',
  "AAABLXEC" varchar(73) NOT NULL default ' ',
  "AAABLXED" varchar(2) NOT NULL default '00',
  "AAABLXEL" varchar(1) NOT NULL default ' ',
  "AAABLXCZ" varchar(180) NOT NULL default ' ',
  "AAABLXJF" varchar(5) NOT NULL default ' ',
  "AAABLXDB" decimal(19,2) NOT NULL default '0.00',
  "AAABLXDC" decimal(19,2) NOT NULL default '0.00',
  "AAABLXZB" varchar(3) NOT NULL default ' ',
  "AAABLXDE" decimal(15,3) NOT NULL default '0.000',
  "AAABLXDF" decimal(19,2) NOT NULL default '0.00',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABLXYO" varchar(10) NOT NULL default ' ',
  "AAABMABE" varchar(3) NOT NULL default ' ',
  "AAABLXES" varchar(8) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABLXDP" varchar(1) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAABLXDS" varchar(100) NOT NULL default ' ',
  "AAABLXDT" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABMRIL","AAABLYAS","AAABLXEK","AAABLXDX","AAABLXDY","AAABLXDZ","AAABLXEA","AAABLXEB","AAABLXEC","AAABLXED","AAABLXEL"),
  KEY "AAABLXEF" ("AAABMQII","AAABMJXV","AAABMJXW","AAABLXYO","AAABLXEK","AAABLXEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLXEM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXSA" varchar(6) NOT NULL default ' ',
  "AAABLXES" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXSA","AAABLXES")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLXET" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXFC" varchar(2) NOT NULL default ' ',
  "AAABLXFD" varchar(1) NOT NULL default ' ',
  "AAABLXSA" varchar(6) NOT NULL default ' ',
  "AAABLXFF" varchar(1) NOT NULL default ' ',
  "AAABLXFG" varchar(40) NOT NULL default ' ',
  "AAABLXFH" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXFC","AAABLXFD","AAABLXSA","AAABLXFF","AAABLXFG","AAABLXFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLXFI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXGQ" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABLXGU" varchar(10) NOT NULL default ' ',
  "AAABLXGV" varchar(10) NOT NULL default ' ',
  "AAABLXFQ" varchar(10) NOT NULL default ' ',
  "AAABLXFR" varchar(10) NOT NULL default ' ',
  "AAABMMJW" varchar(10) NOT NULL default ' ',
  "AAABLXYO" varchar(10) NOT NULL default ' ',
  "AAABLXGN" varchar(50) NOT NULL default ' ',
  "AAABLXGO" varchar(50) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXGQ","AAABMQII","AAABMJXV","AAABMJXW","AAABLXGU","AAABLXGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLXGF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXGQ" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABLXGU" varchar(10) NOT NULL default ' ',
  "AAABLXGV" varchar(10) NOT NULL default ' ',
  "AAABLXGN" varchar(50) NOT NULL default ' ',
  "AAABLXGO" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXGQ","AAABMQII","AAABMJXV","AAABMJXW","AAABLXGU","AAABLXGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLXGW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXHD" varchar(3) NOT NULL default ' ',
  "AAABLXYX" varchar(18) NOT NULL default ' ',
  "AAABLXHF" varchar(1) NOT NULL default ' ',
  "AAABLXHB" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXHD","AAABLXYX","AAABLXHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLXHG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABLXHM" varchar(8) NOT NULL default '00000000',
  "AAABLXHN" varchar(8) NOT NULL default '00000000',
  "AAABLXHO" varchar(8) NOT NULL default '00000000',
  "AAABLZUS" varchar(1) NOT NULL default ' ',
  "AAABMAGA" varchar(1) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABMCFY" varchar(3) NOT NULL default ' ',
  "AAABLXHT" varchar(8) NOT NULL default '00000000',
  "AAABLXHU" varchar(10) NOT NULL default ' ',
  "AAABLXHV" varchar(2) NOT NULL default ' ',
  "AAABLXHW" varchar(2) NOT NULL default ' ',
  "AAABLXPL" decimal(15,2) NOT NULL default '0.00',
  "AAABMRFB" varchar(5) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABLXIC" varchar(3) NOT NULL default ' ',
  "AAABLXID" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABLXIF" varchar(4) NOT NULL default ' ',
  "AAABLXIG" varchar(8) NOT NULL default '00000000',
  "AAABLXIH" varchar(8) NOT NULL default '00000000',
  "AAABLXII" varchar(10) NOT NULL default ' ',
  "AAABLYIE" varchar(8) NOT NULL default '00000000',
  "AAABLXIK" varchar(1) NOT NULL default ' ',
  "AAABLXIL" varchar(1) NOT NULL default ' ',
  "AAABLXIM" varchar(9) NOT NULL default ' ',
  "AAABLXIN" varchar(1) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABLXIP" varchar(2) NOT NULL default ' ',
  "AAABMLHO" varchar(4) NOT NULL default ' ',
  "AAABLXQS" varchar(3) NOT NULL default '000',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  "AAABLXIT" varchar(20) NOT NULL default ' ',
  "AAABLXIU" varchar(4) NOT NULL default ' ',
  "AAABLXIV" varchar(8) NOT NULL default '00000000',
  "AAABLXIW" varchar(4) NOT NULL default ' ',
  "AAABLXIX" varchar(12) NOT NULL default ' ',
  "AAABLXIY" varchar(35) NOT NULL default ' ',
  "AAABLXIZ" varchar(16) NOT NULL default ' ',
  "AAABLXJA" decimal(3,0) NOT NULL default '0',
  "AAABLXJB" varchar(8) NOT NULL default '00000000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABLXSA" varchar(6) NOT NULL default ' ',
  "AAABLXJF" varchar(5) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABLXJH" varchar(3) NOT NULL default ' ',
  "AAABLXJI" varchar(3) NOT NULL default ' ',
  "AAABLXJJ" varchar(3) NOT NULL default ' ',
  "AAABLXJK" varchar(3) NOT NULL default ' ',
  "AAABLXJL" varchar(3) NOT NULL default ' ',
  "AAABLXJM" varchar(10) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABLXSX" varchar(8) NOT NULL default '00000000',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABMCZV" varchar(4) NOT NULL default ' ',
  "AAABLXJR" varchar(10) NOT NULL default ' ',
  "AAABLXJS" varchar(4) NOT NULL default ' ',
  "AAABLXJT" varchar(3) NOT NULL default ' ',
  "AAABLXJU" varchar(3) NOT NULL default ' ',
  "AAABLXJV" varchar(5) NOT NULL default ' ',
  "AAABLXJW" varchar(8) NOT NULL default '00000000',
  "AAABLXJX" varchar(8) NOT NULL default '00000000',
  "AAABLXJY" varchar(8) NOT NULL default '00000000',
  "AAABLXJZ" decimal(15,2) NOT NULL default '0.00',
  "AAABLXKA" varchar(1) NOT NULL default ' ',
  "AAABLXKB" varchar(3) NOT NULL default ' ',
  "AAABLXKC" varchar(1) NOT NULL default ' ',
  "AAABLYWS" varchar(10) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABLXKF" varchar(4) NOT NULL default ' ',
  "AAABLXKG" varchar(1) NOT NULL default ' ',
  "AAABLXKH" varchar(1) NOT NULL default ' ',
  "AAABLXKI" varchar(1) NOT NULL default ' ',
  "AAABLXKJ" varchar(1) NOT NULL default ' ',
  "AAABLXKK" varchar(1) NOT NULL default ' ',
  "AAABLXKL" varchar(1) NOT NULL default ' ',
  "AAABLXKM" varchar(1) NOT NULL default ' ',
  "AAABLXKN" varchar(1) NOT NULL default ' ',
  "AAABLXKO" varchar(1) NOT NULL default ' ',
  "AAABMDGB" varchar(16) NOT NULL default ' ',
  "AAABMDGC" varchar(18) NOT NULL default ' ',
  "AAABLXTW" varchar(1) NOT NULL default ' ',
  "AAABLXKS" varchar(6) NOT NULL default ' ',
  "AAABLXKT" varchar(2) NOT NULL default '00',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABLXKV" varchar(12) NOT NULL default ' ',
  "AAABLXKW" varchar(10) NOT NULL default ' ',
  "AAABLXKX" varchar(4) NOT NULL default ' ',
  "AAABLXKY" varchar(1) NOT NULL default ' ',
  "AAABLXKZ" varchar(8) NOT NULL default '00000000',
  "AAABLXLA" varchar(6) NOT NULL default '000000',
  "AAABLXLB" varchar(6) NOT NULL default '000000',
  "AAABLXLC" varchar(10) NOT NULL default ' ',
  "AAABLXLD" varchar(6) NOT NULL default '000000',
  "AAABLXLE" varchar(3) NOT NULL default ' ',
  "AAABLXLF" varchar(3) NOT NULL default ' ',
  "AAABMCXT" varchar(1) NOT NULL default ' ',
  "AAABLXLH" varchar(1) NOT NULL default ' ',
  "AAABLXLI" varchar(8) NOT NULL default '00000000',
  "AAABLXLJ" varchar(8) NOT NULL default '00000000',
  "AAABLXLK" varchar(12) NOT NULL default ' ',
  "AAABLXLL" varchar(20) NOT NULL default ' ',
  "AAABLXLM" varchar(4) NOT NULL default '0000',
  "AAABLXLN" varchar(18) NOT NULL default ' ',
  "AAABLXLO" varchar(3) NOT NULL default '000',
  "AAABLXLP" varchar(20) NOT NULL default ' ',
  "AAABLXLQ" varchar(8) NOT NULL default ' ',
  "AAABLXLR" varchar(4) NOT NULL default ' ',
  "AAABLXLS" varchar(20) NOT NULL default ' ',
  "AAABLXLT" varchar(4) NOT NULL default ' ',
  "AAABLXLU" varchar(20) NOT NULL default ' ',
  "AAABLXLV" varchar(8) NOT NULL default '00000000',
  "AAABLXLW" tinyint(3) unsigned NOT NULL default '0',
  "AAABLXLX" varchar(20) NOT NULL default ' ',
  "AAABLXLY" varchar(1) NOT NULL default ' ',
  "AAABLXLZ" varchar(3) NOT NULL default ' ',
  "AAABLXMA" varchar(6) NOT NULL default ' ',
  "AAABLXMB" varchar(8) NOT NULL default '00000000',
  "AAABLXMC" varchar(8) NOT NULL default '00000000',
  "AAABLXMD" decimal(12,3) NOT NULL default '0.000',
  "AAABLXME" varchar(20) NOT NULL default ' ',
  "AAABLXMF" varchar(10) NOT NULL default ' ',
  "AAABLXMG" varchar(1) NOT NULL default ' ',
  "AAABLXMH" varchar(4) NOT NULL default ' ',
  "AAABLXMI" varchar(40) NOT NULL default ' ',
  "AAABLXMJ" decimal(13,2) NOT NULL default '0.00',
  "AAABLXMK" varchar(4) NOT NULL default ' ',
  "AAABLXML" varchar(8) NOT NULL default '00000000',
  "AAABLXMM" varchar(4) NOT NULL default ' ',
  "AAABLXMN" varchar(1) NOT NULL default ' ',
  "AAABLXMO" varchar(1) NOT NULL default ' ',
  "AAABLXMP" varchar(10) NOT NULL default ' ',
  "AAABLXMQ" varchar(10) NOT NULL default ' ',
  "AAABLXMR" varchar(10) NOT NULL default ' ',
  "AAABLXMS" varchar(2) NOT NULL default ' ',
  "AAABLXMT" varchar(4) NOT NULL default '0000',
  "AAABLXMU" varchar(1) NOT NULL default ' ',
  "AAABLXMV" varchar(50) NOT NULL default ' ',
  "AAABLXMW" varchar(4) NOT NULL default '0000',
  "AAABLXMX" varchar(4) NOT NULL default '0000',
  "AAABLXMY" varchar(3) NOT NULL default ' ',
  "AAABLXMZ" varchar(8) NOT NULL default '00000000',
  "AAABLXNA" varchar(3) NOT NULL default ' ',
  "AAABLXNB" varchar(1) NOT NULL default ' ',
  "AAABLXVR" varchar(8) NOT NULL default '00000000',
  "AAABLXND" varchar(1) NOT NULL default ' ',
  "AAABLXNE" varchar(1) NOT NULL default ' ',
  "AAABLXNF" varchar(1) NOT NULL default ' ',
  "AAABLXNG" varchar(1) NOT NULL default ' ',
  "AAABLXNH" varchar(1) NOT NULL default ' ',
  "AAABLXNI" varchar(1) NOT NULL default ' ',
  "AAABLXNJ" varchar(1) NOT NULL default ' ',
  "AAABLXVW" varchar(10) NOT NULL default ' ',
  "AAABLXNL" varchar(1) NOT NULL default ' ',
  "AAABLXNM" varchar(10) NOT NULL default ' ',
  "AAABLXNN" varchar(7) NOT NULL default ' ',
  "AAABLXNO" varchar(7) NOT NULL default ' ',
  "AAABLXNP" varchar(10) NOT NULL default '0000000000',
  "AAABLXNQ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLXNT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLXNY" varchar(18) NOT NULL default ' ',
  "AAABLXNZ" varchar(18) NOT NULL default ' ',
  "AAABMCEY" varchar(10) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABLXOC" varchar(40) NOT NULL default ' ',
  "AAABMLGN" varchar(4) NOT NULL default ' ',
  "AAABLXOE" varchar(1) NOT NULL default ' ',
  "AAABLXOF" varchar(1) NOT NULL default ' ',
  "AAABLXOG" varchar(1) NOT NULL default ' ',
  "AAABLXOH" varchar(6) NOT NULL default '000000',
  "AAABLXOI" varchar(6) NOT NULL default '000000',
  "AAABLYWK" varchar(2) NOT NULL default ' ',
  "AAABLXOK" varchar(18) NOT NULL default ' ',
  "AAABLXOL" decimal(13,2) NOT NULL default '0.00',
  "AAABLXZS" decimal(13,3) NOT NULL default '0.000',
  "AAABLXON" varchar(3) NOT NULL default ' ',
  "AAABLXOO" decimal(5,0) NOT NULL default '0',
  "AAABLXOP" decimal(5,0) NOT NULL default '0',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABLXOR" decimal(13,3) NOT NULL default '0.000',
  "AAABLXOS" decimal(13,3) NOT NULL default '0.000',
  "AAABLXOT" varchar(8) NOT NULL default '00000000',
  "AAABLXOU" decimal(13,3) NOT NULL default '0.000',
  "AAABLXOV" varchar(6) NOT NULL default ' ',
  "AAABLXOW" varchar(35) NOT NULL default ' ',
  "AAABLXOX" decimal(3,0) NOT NULL default '0',
  "AAABLXOY" decimal(3,0) NOT NULL default '0',
  "AAABLXOZ" varchar(3) NOT NULL default ' ',
  "AAABLXPA" varchar(3) NOT NULL default ' ',
  "AAABLXPB" varchar(3) NOT NULL default '000',
  "AAABLXPC" varchar(1) NOT NULL default ' ',
  "AAABLXPD" varchar(1) NOT NULL default ' ',
  "AAABLXPE" decimal(3,1) NOT NULL default '0.0',
  "AAABLXPF" decimal(3,1) NOT NULL default '0.0',
  "AAABLZAP" varchar(2) NOT NULL default ' ',
  "AAABLXPH" varchar(1) NOT NULL default ' ',
  "AAABLXPI" varchar(1) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABLXPL" decimal(15,2) NOT NULL default '0.00',
  "AAABMRFB" varchar(5) NOT NULL default ' ',
  "AAABLXPN" decimal(1,0) NOT NULL default '0',
  "AAABLXPO" varchar(1) NOT NULL default ' ',
  "AAABLXPP" varchar(1) NOT NULL default ' ',
  "AAABLYYI" decimal(15,3) NOT NULL default '0.000',
  "AAABLXPR" decimal(15,3) NOT NULL default '0.000',
  "AAABLXPS" decimal(15,3) NOT NULL default '0.000',
  "AAABLYFP" decimal(15,3) NOT NULL default '0.000',
  "AAABLZJP" varchar(3) NOT NULL default ' ',
  "AAABLXPV" decimal(5,0) NOT NULL default '0',
  "AAABLXPW" decimal(5,0) NOT NULL default '0',
  "AAABMLEV" decimal(15,3) NOT NULL default '0.000',
  "AAABLXPY" decimal(15,3) NOT NULL default '0.000',
  "AAABMLEW" varchar(3) NOT NULL default ' ',
  "AAABMLEX" decimal(15,3) NOT NULL default '0.000',
  "AAABMLEY" varchar(3) NOT NULL default ' ',
  "AAABLXQC" varchar(10) NOT NULL default ' ',
  "AAABLXQD" varchar(6) NOT NULL default '000000',
  "AAABLYWS" varchar(10) NOT NULL default ' ',
  "AAABLYFT" varchar(6) NOT NULL default '000000',
  "AAABLXQG" varchar(1) NOT NULL default ' ',
  "AAABLXQH" varchar(1) NOT NULL default ' ',
  "AAABLXQI" varchar(1) NOT NULL default ' ',
  "AAABLXQJ" varchar(2) NOT NULL default '00',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABLYRW" varchar(6) NOT NULL default ' ',
  "AAABLXQO" varchar(1) NOT NULL default ' ',
  "AAABLXQP" varchar(8) NOT NULL default '00000000',
  "AAABLXQQ" varchar(8) NOT NULL default ' ',
  "AAABLXQR" decimal(5,0) NOT NULL default '0',
  "AAABLXQS" varchar(3) NOT NULL default '000',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABLXQW" varchar(1) NOT NULL default ' ',
  "AAABLXQX" varchar(1) NOT NULL default ' ',
  "AAABLXQY" varchar(1) NOT NULL default ' ',
  "AAABLXQZ" varchar(1) NOT NULL default ' ',
  "AAABLXRA" varchar(1) NOT NULL default ' ',
  "AAABLXRB" varchar(1) NOT NULL default ' ',
  "AAABLXRC" varchar(1) NOT NULL default ' ',
  "AAABLXRD" varchar(1) NOT NULL default ' ',
  "AAABLXRE" varchar(1) NOT NULL default ' ',
  "AAABLXRF" decimal(5,2) NOT NULL default '0.00',
  "AAABLXRG" decimal(5,2) NOT NULL default '0.00',
  "AAABLXRH" varchar(1) NOT NULL default ' ',
  "AAABLXRI" decimal(11,2) NOT NULL default '0.00',
  "AAABLXZA" decimal(5,0) NOT NULL default '0',
  "AAABLXZB" varchar(3) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABLXRM" varchar(1) NOT NULL default ' ',
  "AAABLXRN" varchar(2) NOT NULL default ' ',
  "AAABLXRO" varchar(1) NOT NULL default ' ',
  "AAABLXRP" varchar(2) NOT NULL default ' ',
  "AAABLXRQ" varchar(2) NOT NULL default ' ',
  "AAABLXRR" varchar(2) NOT NULL default ' ',
  "AAABLXRS" varchar(2) NOT NULL default ' ',
  "AAABLXRT" varchar(13) NOT NULL default ' ',
  "AAABLXRU" varchar(1) NOT NULL default ' ',
  "AAABMCVW" varchar(10) NOT NULL default ' ',
  "AAABLXRW" varchar(1) NOT NULL default ' ',
  "AAABLXRX" varchar(1) NOT NULL default ' ',
  "AAABLXRY" varchar(1) NOT NULL default ' ',
  "AAABLXRZ" decimal(13,3) NOT NULL default '0.000',
  "AAABLXSA" varchar(6) NOT NULL default ' ',
  "AAABLXSB" decimal(13,2) NOT NULL default '0.00',
  "AAABLXSC" decimal(13,2) NOT NULL default '0.00',
  "AAABLXSD" decimal(13,2) NOT NULL default '0.00',
  "AAABLXSE" decimal(13,2) NOT NULL default '0.00',
  "AAABLXSF" decimal(13,2) NOT NULL default '0.00',
  "AAABLXSG" decimal(13,2) NOT NULL default '0.00',
  "AAABLXSH" decimal(13,2) NOT NULL default '0.00',
  "AAABLXSI" decimal(9,5) NOT NULL default '0.00000',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABLXSK" varchar(18) NOT NULL default ' ',
  "AAABLXSL" varchar(1) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAABLXSN" varchar(3) NOT NULL default ' ',
  "AAABLXSO" varchar(3) NOT NULL default ' ',
  "AAABLXSP" varchar(3) NOT NULL default ' ',
  "AAABLXSQ" varchar(3) NOT NULL default ' ',
  "AAABLXSR" varchar(3) NOT NULL default ' ',
  "AAABLXSS" decimal(13,3) NOT NULL default '0.000',
  "AAABLXST" varchar(4) NOT NULL default ' ',
  "AAABLXSU" varchar(1) NOT NULL default ' ',
  "AAABLXSV" varchar(1) NOT NULL default ' ',
  "AAABMCXN" varchar(10) NOT NULL default '0000000000',
  "AAABLXSX" varchar(8) NOT NULL default '00000000',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABLXSZ" varchar(18) NOT NULL default ' ',
  "AAABLXTA" varchar(4) NOT NULL default ' ',
  "AAABLXTB" varchar(3) NOT NULL default ' ',
  "AAABLXTC" double NOT NULL default '0',
  "AAABLXTD" varchar(1) NOT NULL default ' ',
  "AAABLXTE" varchar(1) NOT NULL default ' ',
  "AAABLXTF" varchar(8) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABLXTH" varchar(6) NOT NULL default ' ',
  "AAABLXTI" varchar(4) NOT NULL default ' ',
  "AAABLXTJ" decimal(11,2) NOT NULL default '0.00',
  "AAABLXTK" varchar(1) NOT NULL default ' ',
  "AAABLXTL" varchar(1) NOT NULL default ' ',
  "AAABLXTM" decimal(9,5) NOT NULL default '0.00000',
  "AAABLXTN" varchar(18) NOT NULL default '000000000000000000',
  "AAABLXTO" varchar(18) NOT NULL default '000000000000000000',
  "AAABLXTP" varchar(1) NOT NULL default ' ',
  "AAABLXTQ" varchar(1) NOT NULL default ' ',
  "AAABLXTR" varchar(4) NOT NULL default ' ',
  "AAABLXTS" int(11) NOT NULL default '0',
  "AAABLXTT" varchar(1) NOT NULL default ' ',
  "AAABLXTU" varchar(4) NOT NULL default ' ',
  "AAABLXTV" varchar(1) NOT NULL default ' ',
  "AAABLXTW" varchar(1) NOT NULL default ' ',
  "AAABLXTX" varchar(1) NOT NULL default ' ',
  "AAABLXTY" varchar(1) NOT NULL default ' ',
  "AAABLXTZ" varchar(12) NOT NULL default '000000000000',
  "AAABLXUA" varchar(4) NOT NULL default ' ',
  "AAABLXUB" varchar(4) NOT NULL default ' ',
  "AAABLXUC" varchar(1) NOT NULL default ' ',
  "AAABLXUD" varchar(8) NOT NULL default '00000000',
  "AAABLXUE" varchar(2) NOT NULL default ' ',
  "AAABLXUF" varchar(1) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  "AAABLXUH" varchar(10) NOT NULL default '0000000000',
  "AAABLXUI" varchar(3) NOT NULL default '000',
  "AAABLXUJ" varchar(1) NOT NULL default ' ',
  "AAABLXUK" varchar(8) NOT NULL default '00000000',
  "AAABLXUL" varchar(8) NOT NULL default '00000000',
  "AAABLXUM" varchar(1) NOT NULL default ' ',
  "AAABLXUN" varchar(6) NOT NULL default ' ',
  "AAABLXUO" varchar(6) NOT NULL default ' ',
  "AAABLXUP" varchar(4) NOT NULL default ' ',
  "AAABLXUQ" varchar(18) NOT NULL default ' ',
  "AAABLXUR" varchar(18) NOT NULL default ' ',
  "AAABLXUS" varchar(2) NOT NULL default ' ',
  "AAABLXUT" varchar(8) NOT NULL default ' ',
  "AAABLXUU" varchar(4) NOT NULL default ' ',
  "AAABLXUV" varchar(35) NOT NULL default ' ',
  "AAABLXUW" double NOT NULL default '0',
  "AAABLXUX" varchar(2) NOT NULL default ' ',
  "AAABLXUY" double NOT NULL default '0',
  "AAABLXUZ" varchar(10) NOT NULL default ' ',
  "AAABLXVA" varchar(3) NOT NULL default ' ',
  "AAABLXVB" varchar(3) NOT NULL default ' ',
  "AAABLXVC" varchar(2) NOT NULL default ' ',
  "AAABLXVD" varchar(10) NOT NULL default ' ',
  "AAABLXVE" varchar(6) NOT NULL default '000000',
  "AAABLXVF" varchar(18) NOT NULL default ' ',
  "AAABLXVG" varchar(1) NOT NULL default ' ',
  "AAABLXVH" varchar(18) NOT NULL default ' ',
  "AAABLXVI" varchar(18) NOT NULL default ' ',
  "AAABLXVJ" varchar(10) NOT NULL default ' ',
  "AAABLXVK" varchar(10) NOT NULL default ' ',
  "AAABLXVL" varchar(1) NOT NULL default ' ',
  "AAABLXVM" varchar(1) NOT NULL default ' ',
  "AAABLXVN" varchar(12) NOT NULL default ' ',
  "AAABLXVO" decimal(13,2) NOT NULL default '0.00',
  "AAABLXVP" varchar(10) NOT NULL default ' ',
  "AAABLXVQ" varchar(1) NOT NULL default ' ',
  "AAABLXVR" varchar(8) NOT NULL default '00000000',
  "AAABLXVS" varchar(3) NOT NULL default '000',
  "AAABLXVT" decimal(9,3) NOT NULL default '0.000',
  "AAABLXVU" decimal(9,3) NOT NULL default '0.000',
  "AAABLXVV" varchar(1) NOT NULL default ' ',
  "AAABLXVW" varchar(10) NOT NULL default ' ',
  "AAABLXVX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQIL","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLXWB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABMAKT" varchar(10) NOT NULL default '0000000000',
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  "AAABLXWL" varchar(25) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLXWN" varchar(1) NOT NULL default ' ',
  "AAABLXWO" varchar(1) NOT NULL default ' ',
  "AAABMJGC" varchar(1) NOT NULL default ' ',
  "AAABLXWQ" varchar(1) NOT NULL default ' ',
  "AAABLXWR" varchar(1) NOT NULL default ' ',
  "AAABLXWS" varchar(2) NOT NULL default '00',
  "AAABLXWT" varchar(30) NOT NULL default ' ',
  "AAABLXWU" varchar(10) NOT NULL default ' ',
  "AAABLXWV" varchar(2) NOT NULL default '00',
  "AAABMCWL" varchar(20) NOT NULL default ' ',
  "AAABLXWX" varchar(1) NOT NULL default ' ',
  "AAABLXWY" varchar(10) NOT NULL default ' ',
  "AAABLXWZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQIL","AAABMLTD","AAABMALG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLXXE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXXL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMBIU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXXL","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLXXO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXXV" varchar(30) NOT NULL default ' ',
  "AAABLXXW" varchar(30) NOT NULL default ' ',
  "AAABLXXS" varchar(30) NOT NULL default ' ',
  "AAABLXXT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXXV","AAABLXXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLXXX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXYW" varchar(4) NOT NULL default ' ',
  "AAABLXYE" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXYW","AAABLXYE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLXYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  "AAABLYAT" varchar(10) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABLYAX" varchar(1) NOT NULL default ' ',
  "AAABLYAY" varchar(10) NOT NULL default ' ',
  "AAABLXYO" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABLXYS" varchar(10) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLXYU" varchar(100) NOT NULL default ' ',
  "AAABLXYV" varchar(2) NOT NULL default '00',
  "AAABLXYW" varchar(4) NOT NULL default ' ',
  "AAABLXYX" varchar(18) NOT NULL default ' ',
  "AAABLXYY" decimal(11,2) NOT NULL default '0.00',
  "AAABLXYZ" varchar(5) NOT NULL default ' ',
  "AAABLXZA" decimal(5,0) NOT NULL default '0',
  "AAABLXZB" varchar(3) NOT NULL default ' ',
  "AAABLXZC" decimal(5,0) NOT NULL default '0',
  "AAABLXZD" decimal(5,0) NOT NULL default '0',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABLXZF" varchar(1) NOT NULL default ' ',
  "AAABLXZG" varchar(5) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLXZI" varchar(1) NOT NULL default ' ',
  "AAABLXZJ" varchar(1) NOT NULL default ' ',
  "AAABLXZK" varchar(3) NOT NULL default ' ',
  "AAABLXZL" varchar(5) NOT NULL default ' ',
  "AAABLXZM" decimal(11,2) NOT NULL default '0.00',
  "AAABLXZN" varchar(8) NOT NULL default '00000000',
  "AAABLXZO" varchar(8) NOT NULL default '00000000',
  "AAABMQIK" varchar(8) NOT NULL default '00000000',
  "AAABLXZQ" varchar(19) NOT NULL default ' ',
  "AAABLXZR" decimal(19,2) NOT NULL default '0.00',
  "AAABLXZS" decimal(13,3) NOT NULL default '0.000',
  "AAABLXZT" decimal(15,2) NOT NULL default '0.00',
  "AAABLXZU" decimal(15,3) NOT NULL default '0.000',
  "AAABLXZV" decimal(15,2) NOT NULL default '0.00',
  "AAABLXZW" decimal(19,2) NOT NULL default '0.00',
  "AAABLXZX" decimal(15,2) NOT NULL default '0.00',
  "AAABLXZY" decimal(11,2) NOT NULL default '0.00',
  "AAABLXZZ" decimal(19,2) NOT NULL default '0.00',
  "AAABLYAA" decimal(15,3) NOT NULL default '0.000',
  "AAABLYAB" varchar(10) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABLYAE" varchar(3) NOT NULL default '000',
  "AAABMAMV" varchar(4) NOT NULL default ' ',
  "AAABLYAG" varchar(4) NOT NULL default ' ',
  "AAABLYAH" varchar(1) NOT NULL default ' ',
  "AAABMEGE" varchar(1) NOT NULL default ' ',
  "AAABLYAJ" varchar(1) NOT NULL default ' ',
  "AAABLYAK" varchar(1) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMAKM" varchar(6) NOT NULL default '000000',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLYAS","AAABLYAT","AAABMLCZ","AAABMQIL","AAABMLTD","AAABLYAX","AAABLYAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYAZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYBL" varchar(3) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAABLYBH" varchar(1) NOT NULL default ' ',
  "AAABLYBI" varchar(1) NOT NULL default ' ',
  "AAABLYBJ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYBL","AAABMQII","AAABMJXV","AAABMJXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYBP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYWK" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYWK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYBU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYBW" varchar(3) NOT NULL default '000',
  "AAABLYBX" varchar(3) NOT NULL default '000',
  "AAABLYBY" tinyint(3) unsigned NOT NULL default '0',
  "AAABLYBZ" int(11) NOT NULL default '0',
  "AAABLYDM" varchar(1) NOT NULL default ' ',
  "AAABLYCB" varchar(1) NOT NULL default ' ',
  "AAABLYCC" varchar(1) NOT NULL default ' ',
  "AAABLYCD" varchar(1) NOT NULL default ' ',
  "AAABLYCE" varchar(1) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAABLYCG" varchar(2) NOT NULL default ' ',
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABLYCI" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYCK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABLYDL" varchar(1) NOT NULL default ' ',
  "AAABLYDN" varchar(1) NOT NULL default ' ',
  "AAABLYDB" varchar(1) NOT NULL default ' ',
  "AAABLYDM" varchar(1) NOT NULL default ' ',
  "AAABLYCR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYCU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABLYDN" varchar(1) NOT NULL default ' ',
  "AAABLYDM" varchar(1) NOT NULL default ' ',
  "AAABLYDB" varchar(1) NOT NULL default ' ',
  "AAABLYDC" varchar(3) NOT NULL default '000',
  "AAABLYDD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYDI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABLYDL" varchar(1) NOT NULL default ' ',
  "AAABLYDM" varchar(1) NOT NULL default ' ',
  "AAABLYDN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYDQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYDU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYDV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABLYEC" varchar(1) NOT NULL default ' ',
  "AAABLYED" varchar(1) NOT NULL default ' ',
  "AAABLYEE" varchar(1) NOT NULL default ' ',
  "AAABLYEF" varchar(3) NOT NULL default '000',
  "AAABLYEG" varchar(1) NOT NULL default ' ',
  "AAABLYEH" varchar(3) NOT NULL default '000',
  "AAABLYEI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABMPLT","AAABMLHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYEP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYEV" varchar(4) NOT NULL default ' ',
  "AAABLYES" varchar(1) NOT NULL default ' ',
  "AAABLYET" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYEV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYEW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLYEZ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYFC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLYIF" varchar(8) NOT NULL default '00000000',
  "AAABLYIG" varchar(8) NOT NULL default '00000000',
  "AAABLYFP" decimal(15,3) NOT NULL default '0.000',
  "AAABLZUS" varchar(1) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABLYWS" varchar(10) NOT NULL default ' ',
  "AAABLYFT" varchar(6) NOT NULL default '000000',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABLYFV" varchar(1) NOT NULL default ' ',
  "AAABLYFW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABMQIL","AAABMLTD"),
  KEY "AAABLYGD" ("AAABMPLT","AAABMCVI","AAABMLCZ","AAABLZUS"),
  KEY "AAABLYGI" ("AAABLYIF","AAABLYIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYGL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLGN" varchar(4) NOT NULL default ' ',
  "AAABLYGO" varchar(1) NOT NULL default ' ',
  "AAABLYGP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYGS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYGX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABLYHA" varchar(1) NOT NULL default ' ',
  "AAABLYHB" varchar(1) NOT NULL default ' ',
  "AAABLYHC" varchar(1) NOT NULL default ' ',
  "AAABLYHD" varchar(1) NOT NULL default ' ',
  "AAABLYHE" varchar(1) NOT NULL default ' ',
  "AAABLYHF" varchar(1) NOT NULL default ' ',
  "AAABLYHG" varchar(1) NOT NULL default ' ',
  "AAABLYHH" varchar(1) NOT NULL default ' ',
  "AAABLYHI" varchar(1) NOT NULL default ' ',
  "AAABLYHJ" varchar(1) NOT NULL default ' ',
  "AAABLYHK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYHN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYHT" varchar(4) NOT NULL default ' ',
  "AAABLYHQ" varchar(1) NOT NULL default ' ',
  "AAABLYHR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYHT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYHU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLYIE" varchar(8) NOT NULL default '00000000',
  "AAABLYIF" varchar(8) NOT NULL default '00000000',
  "AAABLYIG" varchar(8) NOT NULL default '00000000',
  "AAABLYYI" decimal(15,3) NOT NULL default '0.000',
  "AAABLYII" decimal(15,3) NOT NULL default '0.000',
  "AAABLZJP" varchar(3) NOT NULL default ' ',
  "AAABLYIK" varchar(40) NOT NULL default ' ',
  "AAABLYWK" varchar(2) NOT NULL default ' ',
  "AAABLYIM" varchar(1) NOT NULL default ' ',
  "AAABLYIN" varchar(1) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLYIP" varchar(1) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABMQIL","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYIZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABLZZR" varchar(4) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABLYJE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZFD","AAABLZZR","AAABMLHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYJJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYJL" varchar(1) NOT NULL default ' ',
  "AAABLYJM" varchar(1) NOT NULL default ' ',
  "AAABMLEP" varchar(1) NOT NULL default ' ',
  "AAABLYJO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYJQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYKK" varchar(10) NOT NULL default ' ',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABLYMA" varchar(1) NOT NULL default ' ',
  "AAABLYNA" varchar(3) NOT NULL default ' ',
  "AAABLYJX" varchar(1) NOT NULL default ' ',
  "AAABLYJY" varchar(1) NOT NULL default ' ',
  "AAABLYMB" varchar(8) NOT NULL default '00000000',
  "AAABLYMC" varchar(8) NOT NULL default '00000000',
  "AAABLYMD" varchar(8) NOT NULL default '00000000',
  "AAABLYME" varchar(8) NOT NULL default '00000000',
  "AAABLYMF" varchar(8) NOT NULL default '00000000',
  "AAABLYMG" varchar(8) NOT NULL default '00000000',
  "AAABMRFO" varchar(1) NOT NULL default '0',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKM" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYKK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYKL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYLY" varchar(5) NOT NULL default ' ',
  "AAABLYMK" decimal(13,3) NOT NULL default '0.000',
  "AAABLYML" decimal(13,3) NOT NULL default '0.000',
  "AAABLYKQ" decimal(13,3) NOT NULL default '0.000',
  "AAABLYKR" varchar(3) NOT NULL default ' ',
  "AAABLYKS" decimal(3,1) NOT NULL default '0.0',
  "AAABLYMO" decimal(13,3) NOT NULL default '0.000',
  "AAABLYMP" decimal(13,3) NOT NULL default '0.000',
  "AAABLYMQ" decimal(13,3) NOT NULL default '0.000',
  "AAABLYMR" varchar(3) NOT NULL default ' ',
  "AAABLYNN" varchar(3) NOT NULL default ' ',
  "AAABLYNO" decimal(13,3) NOT NULL default '0.000',
  "AAABLYNP" decimal(13,3) NOT NULL default '0.000',
  "AAABLYNQ" varchar(3) NOT NULL default ' ',
  "AAABLYNR" decimal(13,3) NOT NULL default '0.000',
  "AAABLYNS" decimal(13,3) NOT NULL default '0.000',
  "AAABLYNT" varchar(2) NOT NULL default '00',
  "AAABLYNU" varchar(1) NOT NULL default '0',
  "AAABLYNV" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYLY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYLI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYLY" varchar(5) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAHQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYLY","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYLQ" (
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
  "AAABLYMB" varchar(8) NOT NULL default '00000000',
  "AAABLYMC" varchar(8) NOT NULL default '00000000',
  "AAABLYMD" varchar(8) NOT NULL default '00000000',
  "AAABLYME" varchar(8) NOT NULL default '00000000',
  "AAABLYMF" varchar(8) NOT NULL default '00000000',
  "AAABLYMG" varchar(8) NOT NULL default '00000000',
  "AAABLYMH" varchar(15) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABLYMJ" varchar(30) NOT NULL default ' ',
  "AAABLYMK" decimal(13,3) NOT NULL default '0.000',
  "AAABLYML" decimal(13,3) NOT NULL default '0.000',
  "AAABLYMM" decimal(13,3) NOT NULL default '0.000',
  "AAABLYMN" varchar(3) NOT NULL default ' ',
  "AAABLYMO" decimal(13,3) NOT NULL default '0.000',
  "AAABLYMP" decimal(13,3) NOT NULL default '0.000',
  "AAABLYMQ" decimal(13,3) NOT NULL default '0.000',
  "AAABLYMR" varchar(3) NOT NULL default ' ',
  "AAABLYMS" decimal(13,3) NOT NULL default '0.000',
  "AAABLYMT" varchar(3) NOT NULL default ' ',
  "AAABLYMU" decimal(13,3) NOT NULL default '0.000',
  "AAABLYMV" varchar(3) NOT NULL default ' ',
  "AAABMLZG" decimal(13,3) NOT NULL default '0.000',
  "AAABLYMX" decimal(13,3) NOT NULL default '0.000',
  "AAABLYMY" decimal(13,3) NOT NULL default '0.000',
  "AAABLYMZ" varchar(3) NOT NULL default ' ',
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
  "AAABLYNL" decimal(7,2) NOT NULL default '0.00',
  "AAABLYNM" varchar(3) NOT NULL default ' ',
  "AAABLYNN" varchar(3) NOT NULL default ' ',
  "AAABLYNO" decimal(13,3) NOT NULL default '0.000',
  "AAABLYNP" decimal(13,3) NOT NULL default '0.000',
  "AAABLYNQ" varchar(3) NOT NULL default ' ',
  "AAABLYNR" decimal(13,3) NOT NULL default '0.000',
  "AAABLYNS" decimal(13,3) NOT NULL default '0.000',
  "AAABLYNT" varchar(2) NOT NULL default '00',
  "AAABLYNU" varchar(1) NOT NULL default '0',
  "AAABLYNV" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZCS"),
  KEY "AAABLYNY" ("AAABLYNZ","AAABLZFD","AAABMLEZ","AAABLZCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYOD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYOO" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYOO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYOI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYOO" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMEDK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYOO","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYOQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABLYOT" varchar(1) NOT NULL default ' ',
  "AAABLYOU" decimal(3,1) NOT NULL default '0.0',
  "AAABLYOV" varchar(1) NOT NULL default ' ',
  "AAABLYOW" decimal(3,1) NOT NULL default '0.0',
  "AAABLYOX" varchar(1) NOT NULL default ' ',
  "AAABLYOY" decimal(3,1) NOT NULL default '0.0',
  "AAABLYOZ" varchar(1) NOT NULL default ' ',
  "AAABLYPA" decimal(3,1) NOT NULL default '0.0',
  "AAABLYPB" varchar(1) NOT NULL default ' ',
  "AAABLYPC" decimal(3,1) NOT NULL default '0.0',
  "AAABLYPD" varchar(1) NOT NULL default ' ',
  "AAABLYPE" decimal(3,1) NOT NULL default '0.0',
  PRIMARY KEY ("AAABMRIL","AAABLZFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMALO" varchar(10) NOT NULL default ' ',
  "AAABLZWY" varchar(30) NOT NULL default ' ',
  "AAABLZWZ" varchar(30) NOT NULL default ' ',
  "AAABLZXA" varchar(30) NOT NULL default ' ',
  "AAABLZXB" varchar(30) NOT NULL default ' ',
  "AAABLZXC" varchar(30) NOT NULL default ' ',
  "AAABLZXD" varchar(30) NOT NULL default ' ',
  "AAABLZXE" varchar(30) NOT NULL default ' ',
  "AAABLZXF" varchar(30) NOT NULL default ' ',
  "AAABLZXG" varchar(30) NOT NULL default ' ',
  "AAABLZXH" varchar(30) NOT NULL default ' ',
  "AAABLZXI" varchar(4) NOT NULL default '0000',
  "AAABLZXJ" varchar(4) NOT NULL default '0000',
  "AAABLZXK" varchar(4) NOT NULL default '0000',
  "AAABLZXL" varchar(4) NOT NULL default '0000',
  "AAABLZXM" varchar(4) NOT NULL default '0000',
  "AAABLZXN" varchar(4) NOT NULL default '0000',
  "AAABLZXO" varchar(4) NOT NULL default '0000',
  "AAABLZXP" varchar(4) NOT NULL default '0000',
  "AAABLZXQ" varchar(4) NOT NULL default '0000',
  "AAABLZXR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMALO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYQG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMEKY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMX","AAABMAMY","AAABMQIJ","AAABMEKY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYQR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYRP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYRP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYQW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYRP" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAGQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYRP","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYRE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYRW" varchar(6) NOT NULL default ' ',
  "AAABLYRX" varchar(5) NOT NULL default '00000',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABLZCO" varchar(10) NOT NULL default ' ',
  "AAABLZCQ" varchar(10) NOT NULL default ' ',
  "AAABLYRM" varchar(10) NOT NULL default ' ',
  "AAABLYRN" varchar(10) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default '0',
  "AAABLYRP" varchar(2) NOT NULL default ' ',
  "AAABLYRQ" decimal(3,0) NOT NULL default '0',
  "AAABLYRR" decimal(11,0) NOT NULL default '0',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKM" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYRW","AAABLYRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYRY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYTE" varchar(3) NOT NULL default ' ',
  "AAABLYTF" varchar(10) NOT NULL default ' ',
  "AAABLYSL" varchar(5) NOT NULL default '00000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABLYSE" decimal(11,0) NOT NULL default '0',
  "AAABLYSF" decimal(11,0) NOT NULL default '0',
  "AAABLYSG" decimal(11,0) NOT NULL default '0',
  "AAABLYSH" decimal(11,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLYTE","AAABLYTF","AAABLYSL"),
  UNIQUE KEY "AAABLYSM" ("AAABMRIL","AAABMPLT","AAABLYTE","AAABLYTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYSR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYTE" varchar(3) NOT NULL default ' ',
  "AAABLYTF" varchar(10) NOT NULL default ' ',
  "AAABLYSV" decimal(11,0) NOT NULL default '0',
  "AAABLYSW" decimal(11,0) NOT NULL default '0',
  "AAABLYSX" decimal(11,0) NOT NULL default '0',
  "AAABLYSY" decimal(11,0) NOT NULL default '0',
  "AAABLYSZ" decimal(11,0) NOT NULL default '0',
  "AAABLYTA" decimal(11,0) NOT NULL default '0',
  "AAABLYTB" varchar(12) NOT NULL default ' ',
  "AAABLYTC" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLYTE","AAABLYTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYTG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZFE" varchar(2) NOT NULL default ' ',
  "AAABLYTJ" varchar(1) NOT NULL default ' ',
  "AAABLYVK" varchar(4) NOT NULL default ' ',
  "AAABLYVL" varchar(4) NOT NULL default ' ',
  "AAABLYVM" varchar(4) NOT NULL default ' ',
  "AAABLYVN" varchar(4) NOT NULL default ' ',
  "AAABLYTO" varchar(1) NOT NULL default ' ',
  "AAABLYTP" varchar(4) NOT NULL default ' ',
  "AAABLYTQ" varchar(4) NOT NULL default ' ',
  "AAABLYTR" varchar(18) NOT NULL default ' ',
  "AAABLYTS" decimal(15,2) NOT NULL default '0.00',
  "AAABLYTT" varchar(5) NOT NULL default ' ',
  "AAABLYTU" decimal(4,1) NOT NULL default '0.0',
  "AAABLYTV" varchar(1) NOT NULL default ' ',
  "AAABLYTW" varchar(1) NOT NULL default ' ',
  "AAABLYTX" varchar(4) NOT NULL default ' ',
  "AAABLYTY" varchar(4) NOT NULL default ' ',
  "AAABLYTZ" varchar(1) NOT NULL default ' ',
  "AAABLYUA" varchar(1) NOT NULL default ' ',
  "AAABLYUB" varchar(1) NOT NULL default ' ',
  "AAABLYUC" varchar(1) NOT NULL default ' ',
  "AAABLYUD" varchar(1) NOT NULL default ' ',
  "AAABLYUE" varchar(1) NOT NULL default ' ',
  "AAABLYUF" varchar(1) NOT NULL default ' ',
  "AAABLYUG" varchar(1) NOT NULL default ' ',
  "AAABLYUH" varchar(1) NOT NULL default ' ',
  "AAABLYUI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZFE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYUL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZFE" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZFE","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYUT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABLZCA" varchar(1) NOT NULL default ' ',
  "AAABLYUX" varchar(1) NOT NULL default ' ',
  "AAABLYUY" varchar(1) NOT NULL default ' ',
  "AAABLYUZ" varchar(1) NOT NULL default ' ',
  "AAABLYVA" varchar(1) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMLHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYVG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZFC" varchar(4) NOT NULL default ' ',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABLYVK" varchar(4) NOT NULL default ' ',
  "AAABLYVL" varchar(4) NOT NULL default ' ',
  "AAABLYVM" varchar(4) NOT NULL default ' ',
  "AAABLYVN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZFC","AAABLZFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYVR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZMY" varchar(10) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLYVV" decimal(13,3) NOT NULL default '0.000',
  "AAABLYVW" varchar(3) NOT NULL default ' ',
  "AAABLYVX" decimal(13,3) NOT NULL default '0.000',
  "AAABLYVY" decimal(13,3) NOT NULL default '0.000',
  "AAABLZJP" varchar(3) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLZMY","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYWF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZFE" varchar(2) NOT NULL default ' ',
  "AAABLZDT" varchar(4) NOT NULL default ' ',
  "AAABLZDQ" varchar(4) NOT NULL default ' ',
  "AAABLYWK" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZFE","AAABLZDT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYWO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYXC" varchar(30) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLYWS" varchar(10) NOT NULL default ' ',
  "AAABLYWT" varchar(1) NOT NULL default ' ',
  "AAABLYWU" decimal(15,3) NOT NULL default '0.000',
  "AAABLYWV" decimal(15,3) NOT NULL default '0.000',
  "AAABLYWW" decimal(15,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLYXC","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYXE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLZMY" varchar(10) NOT NULL default ' ',
  "AAABLZAI" varchar(10) NOT NULL default ' ',
  "AAABLZAJ" varchar(10) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLYYA" varchar(18) NOT NULL default ' ',
  "AAABLZBN" varchar(16) NOT NULL default ' ',
  "AAABLYXN" varchar(8) NOT NULL default '00000000',
  "AAABMDGB" varchar(16) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABLZAE" varchar(5) NOT NULL default ' ',
  "AAABLYZL" decimal(13,2) NOT NULL default '0.00',
  "AAABLYXT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLZMY","AAABLZAI","AAABLZAJ","AAABMOHM","AAABLYYA","AAABLZBN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYYC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLZMY" varchar(10) NOT NULL default ' ',
  "AAABLZAI" varchar(10) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLYYI" decimal(15,3) NOT NULL default '0.000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLZJP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLZMY","AAABLZAI","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYYQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZMY" varchar(10) NOT NULL default ' ',
  "AAABLZAI" varchar(10) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLYYV" varchar(1) NOT NULL default ' ',
  "AAABLYYW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZMY","AAABLZAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLZMY" varchar(10) NOT NULL default ' ',
  "AAABLZAI" varchar(10) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDGB" varchar(16) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLYZI" varchar(8) NOT NULL default '00000000',
  "AAABMCQA" decimal(13,2) NOT NULL default '0.00',
  "AAABLYZK" decimal(13,2) NOT NULL default '0.00',
  "AAABLYZL" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLZAE" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLZMY","AAABLZAI","AAABMDBA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLYZT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLZMY" varchar(10) NOT NULL default ' ',
  "AAABLZAI" varchar(10) NOT NULL default ' ',
  "AAABLZAJ" varchar(10) NOT NULL default ' ',
  "AAABLZAK" varchar(10) NOT NULL default ' ',
  "AAABMDGB" varchar(16) NOT NULL default ' ',
  "AAABLZAB" decimal(15,2) NOT NULL default '0.00',
  "AAABLZAC" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLZAE" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLZMY","AAABLZAI","AAABLZAJ","AAABLZAK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZAL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZAP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZAP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZAQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZAY" varchar(10) NOT NULL default ' ',
  "AAABLZAZ" varchar(10) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLZAY","AAABLZAZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZBA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABLZBE" varchar(5) NOT NULL default ' ',
  "AAABLZBF" varchar(2) NOT NULL default ' ',
  "AAABLZBG" varchar(2) NOT NULL default ' ',
  "AAABLZBH" varchar(2) NOT NULL default ' ',
  "AAABLZFL" varchar(2) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABLZBN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZBQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZFE" varchar(2) NOT NULL default ' ',
  "AAABLZZR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZFE","AAABLZZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZBX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLGN" varchar(4) NOT NULL default ' ',
  "AAABLZCA" varchar(1) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMLGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZCG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZCI" varchar(30) NOT NULL default ' ',
  "AAABLZCV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZCL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZMY" varchar(10) NOT NULL default ' ',
  "AAABLZCO" varchar(10) NOT NULL default ' ',
  "AAABLZCP" varchar(30) NOT NULL default ' ',
  "AAABLZCQ" varchar(10) NOT NULL default ' ',
  "AAABLZCR" varchar(30) NOT NULL default ' ',
  "AAABLZCS" varchar(10) NOT NULL default ' ',
  "AAABMAHQ" varchar(30) NOT NULL default ' ',
  "AAABLZCU" decimal(15,2) NOT NULL default '0.00',
  "AAABLZCV" decimal(15,2) NOT NULL default '0.00',
  "AAABLZCW" decimal(15,2) NOT NULL default '0.00',
  "AAABLZCX" decimal(15,2) NOT NULL default '0.00',
  "AAABLZCY" decimal(15,2) NOT NULL default '0.00',
  "AAABLZCZ" decimal(15,2) NOT NULL default '0.00',
  "AAABLZDA" decimal(15,2) NOT NULL default '0.00',
  "AAABLZDB" decimal(15,2) NOT NULL default '0.00',
  "AAABLZDC" decimal(15,2) NOT NULL default '0.00',
  "AAABLZDD" decimal(15,2) NOT NULL default '0.00',
  "AAABLZDE" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABLZDH" varchar(6) NOT NULL default '000000',
  "AAABLZDI" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZMY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZDL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZFE" varchar(2) NOT NULL default ' ',
  "AAABLZDT" varchar(4) NOT NULL default ' ',
  "AAABLZDP" varchar(1) NOT NULL default ' ',
  "AAABLZDQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZFE","AAABLZDT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZDU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZDW" varchar(1) NOT NULL default ' ',
  "AAABLZDX" varchar(1) NOT NULL default ' ',
  "AAABLZDY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZEA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZMY" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABLZEF" varchar(3) NOT NULL default ' ',
  "AAABLZEG" varchar(1) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLZMY","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZEN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZFL" varchar(2) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLZFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZEV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZFC" varchar(4) NOT NULL default ' ',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABLZFE" varchar(2) NOT NULL default ' ',
  "AAABLZFA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZFC","AAABLZFD","AAABLZFE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZFF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZFL" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZFL","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZFN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZJU" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZJU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZFS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLZJU" varchar(3) NOT NULL default ' ',
  "AAABLZFW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLZJU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZGA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZJQ" varchar(3) NOT NULL default ' ',
  "AAABLZIB" varchar(4) NOT NULL default ' ',
  "AAABLZGE" varchar(3) NOT NULL default ' ',
  "AAABLZGF" varchar(3) NOT NULL default ' ',
  "AAABLZGG" varchar(4) NOT NULL default ' ',
  "AAABLZGH" varchar(4) NOT NULL default ' ',
  "AAABLZGI" varchar(3) NOT NULL default ' ',
  "AAABLZKC" varchar(4) NOT NULL default ' ',
  "AAABLZKD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZJQ","AAABLZIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZGO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLZJQ" varchar(3) NOT NULL default ' ',
  "AAABLZGS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLZJQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZGW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZJX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZHB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZJX" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAGQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZJX","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZHJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZHS" varchar(4) NOT NULL default ' ',
  "AAABLZHM" varchar(4) NOT NULL default ' ',
  "AAABLZHN" varchar(4) NOT NULL default ' ',
  "AAABLZHO" varchar(4) NOT NULL default ' ',
  "AAABLZHP" varchar(1) NOT NULL default ' ',
  "AAABLZHQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZHT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZIA" varchar(3) NOT NULL default ' ',
  "AAABLZIB" varchar(4) NOT NULL default ' ',
  "AAABLZKZ" varchar(1) NOT NULL default ' ',
  "AAABMLGN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZIA","AAABLZIB","AAABLZKZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZID" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZJV" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZII" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZJV" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAGQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZJV","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZIQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZJW" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZIV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZJW" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAGQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZJW","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZJD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABLZKM" varchar(6) NOT NULL default '000000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLZJJ" decimal(15,3) NOT NULL default '0.000',
  "AAABLZJK" varchar(3) NOT NULL default ' ',
  "AAABLZJL" decimal(15,3) NOT NULL default '0.000',
  "AAABLZJM" varchar(3) NOT NULL default ' ',
  "AAABLZJN" decimal(15,3) NOT NULL default '0.000',
  "AAABLZJO" decimal(15,3) NOT NULL default '0.000',
  "AAABLZJP" varchar(3) NOT NULL default ' ',
  "AAABLZJQ" varchar(3) NOT NULL default ' ',
  "AAABLZJR" varchar(10) NOT NULL default ' ',
  "AAABLZJS" varchar(8) NOT NULL default '00000000',
  "AAABLZJT" varchar(6) NOT NULL default '000000',
  "AAABLZJU" varchar(3) NOT NULL default ' ',
  "AAABLZJV" varchar(3) NOT NULL default ' ',
  "AAABLZJW" varchar(3) NOT NULL default ' ',
  "AAABLZJX" varchar(3) NOT NULL default ' ',
  "AAABLZKZ" varchar(1) NOT NULL default ' ',
  "AAABLZJZ" varchar(1) NOT NULL default ' ',
  "AAABLZKA" varchar(4) NOT NULL default ' ',
  "AAABLZKB" varchar(4) NOT NULL default ' ',
  "AAABLZKC" varchar(4) NOT NULL default ' ',
  "AAABLZKD" varchar(4) NOT NULL default ' ',
  "AAABLZKE" varchar(8) NOT NULL default '00000000',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABLZKG" varchar(8) NOT NULL default '00000000',
  "AAABLZKH" varchar(12) NOT NULL default ' ',
  "AAABLZKI" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMQIL","AAABMLTD","AAABLZKM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZKN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZKZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZKZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZKS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLZKZ" varchar(1) NOT NULL default ' ',
  "AAABLZKW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLZKZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZLA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZLF" varchar(3) NOT NULL default '000',
  "AAABLZLD" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZLF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZLG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZLN" varchar(30) NOT NULL default ' ',
  "AAABLZLO" varchar(30) NOT NULL default ' ',
  "AAABLZLK" varchar(30) NOT NULL default ' ',
  "AAABLZLL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZLN","AAABLZLO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZLP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZLR" varchar(1) NOT NULL default ' ',
  "AAABLZLS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZLU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZMY" varchar(10) NOT NULL default ' ',
  "AAABLZME" varchar(4) NOT NULL default '0000',
  "AAABLZMT" varchar(1) NOT NULL default '0',
  "AAABLZMU" varchar(6) NOT NULL default '000000',
  "AAABLZMV" varchar(6) NOT NULL default '000000',
  "AAABLZMB" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLZMY","AAABLZME")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZMF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZMY" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(4) NOT NULL default '0000',
  "AAABLZMT" varchar(1) NOT NULL default '0',
  "AAABLZMU" varchar(6) NOT NULL default '000000',
  "AAABLZMV" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLZMY","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZMP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZMY" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(4) NOT NULL default '0000',
  "AAABLZMT" varchar(1) NOT NULL default '0',
  "AAABLZMU" varchar(6) NOT NULL default '000000',
  "AAABLZMV" varchar(6) NOT NULL default '000000',
  "AAABLZMW" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZMY","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZNA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMALO" varchar(10) NOT NULL default ' ',
  "AAABLZND" varchar(1) NOT NULL default ' ',
  "AAABLZNE" varchar(2) NOT NULL default ' ',
  "AAABLZNF" varchar(2) NOT NULL default ' ',
  "AAABLZNG" varchar(2) NOT NULL default ' ',
  "AAABLZNH" varchar(2) NOT NULL default ' ',
  "AAABLZNI" varchar(4) NOT NULL default ' ',
  "AAABLZNJ" varchar(1) NOT NULL default ' ',
  "AAABLZNK" varchar(1) NOT NULL default ' ',
  "AAABLZNL" varchar(1) NOT NULL default ' ',
  "AAABLZNM" varchar(1) NOT NULL default ' ',
  "AAABLZNN" varchar(10) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAABLZNP" varchar(10) NOT NULL default ' ',
  "AAABLZNQ" varchar(4) NOT NULL default ' ',
  "AAABLZNR" varchar(10) NOT NULL default ' ',
  "AAABLZNS" varchar(4) NOT NULL default ' ',
  "AAABLZYJ" varchar(1) NOT NULL default ' ',
  "AAABLZNU" varchar(1) NOT NULL default ' ',
  "AAABLZNV" varchar(1) NOT NULL default ' ',
  "AAABLZNW" varchar(1) NOT NULL default ' ',
  "AAABLZNX" varchar(3) NOT NULL default '000',
  "AAABLZNY" varchar(1) NOT NULL default ' ',
  "AAABLZNZ" varchar(1) NOT NULL default ' ',
  "AAABLZOA" varchar(1) NOT NULL default '0',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKM" varchar(6) NOT NULL default '000000',
  "AAABLZOE" varchar(3) NOT NULL default '000',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABMADG" varchar(10) NOT NULL default ' ',
  "AAABLZOI" varchar(1) NOT NULL default ' ',
  "AAABLZOJ" varchar(1) NOT NULL default ' ',
  "AAABLZOK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMALO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZON" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMALO" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAGQ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMALO","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZOV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMALO" varchar(10) NOT NULL default ' ',
  "AAABMAKI" varchar(7) NOT NULL default ' ',
  "AAABLZOZ" varchar(3) NOT NULL default '000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKM" varchar(6) NOT NULL default '000000',
  "AAABMADG" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMALO","AAABMAKI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMALO" varchar(10) NOT NULL default ' ',
  "AAABMAKI" varchar(7) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAGQ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMALO","AAABMAKI","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZPR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMALO" varchar(10) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMALO","AAABMLHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMALO" varchar(10) NOT NULL default ' ',
  "AAABLZRF" varchar(1) NOT NULL default '0',
  "AAABMAKI" varchar(7) NOT NULL default ' ',
  "AAABMGOZ" varchar(8) NOT NULL default '00000000',
  "AAABLZQI" varchar(6) NOT NULL default '000000',
  "AAABLZQJ" varchar(6) NOT NULL default '000000',
  "AAABLZQK" varchar(6) NOT NULL default '000000',
  "AAABLZQL" varchar(6) NOT NULL default '000000',
  "AAABLZQM" varchar(2) NOT NULL default '00',
  "AAABLZQN" varchar(8) NOT NULL default '00000000',
  "AAABLZQO" varchar(5) NOT NULL default '00000',
  "AAABLZQP" varchar(1) NOT NULL default ' ',
  "AAABLZQQ" varchar(10) NOT NULL default ' ',
  "AAABMAJC" varchar(25) NOT NULL default ' ',
  "AAABMAJD" varchar(25) NOT NULL default ' ',
  "AAABMAJF" varchar(5) NOT NULL default ' ',
  "AAABMAJG" varchar(5) NOT NULL default ' ',
  "AAABMAJH" varchar(5) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKM" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMPLT","AAABMQII","AAABMJXV","AAABMJXW","AAABMALO","AAABLZRF","AAABMAKI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZRH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAKU" varchar(5) NOT NULL default ' ',
  "AAABLZRK" varchar(2) NOT NULL default '00',
  "AAABLZRL" varchar(2) NOT NULL default '00',
  "AAABLZRM" varchar(4) NOT NULL default '0000',
  "AAABLZRN" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMAKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZRQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAKU" varchar(5) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAKU","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZRY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAKU" varchar(5) NOT NULL default ' ',
  "AAABLZSB" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZSE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMALO" varchar(10) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMABL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEY" varchar(10) NOT NULL default ' ',
  "AAABLZSP" varchar(3) NOT NULL default ' ',
  "AAABLZSQ" decimal(17,3) NOT NULL default '0.000',
  "AAABLZSR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABMALO","AAABMPLT","AAABMABL","AAABMLCZ","AAABMCVI","AAABMCEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZTC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMALO" varchar(10) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMABL" varchar(3) NOT NULL default '000',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABLZVP" varchar(2) NOT NULL default ' ',
  "AAABMIZS" varchar(30) NOT NULL default ' ',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABLZUS" varchar(1) NOT NULL default ' ',
  "AAABMAAS" varchar(1) NOT NULL default ' ',
  "AAABMAAV" varchar(2) NOT NULL default '00',
  "AAABLZTT" varchar(55) NOT NULL default ' ',
  "AAABMAAZ" varchar(3) NOT NULL default '000',
  "AAABMABA" varchar(3) NOT NULL default '000',
  "AAABMAAW" varchar(1) NOT NULL default ' ',
  "AAABMAAX" varchar(1) NOT NULL default ' ',
  "AAABMAAY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABMALO","AAABMPLT","AAABMABL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZUG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMALO" varchar(10) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMALP" varchar(1) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEY" varchar(10) NOT NULL default ' ',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABLZUS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABMALO","AAABMPLT","AAABMALP","AAABMLCZ","AAABMCVI","AAABMCEY","AAABMQIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZVE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMALO" varchar(10) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMALP" varchar(1) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABLZVP" varchar(2) NOT NULL default ' ',
  "AAABMALF" varchar(3) NOT NULL default '000',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAABMALH" varchar(3) NOT NULL default '000',
  "AAABMALI" varchar(3) NOT NULL default '000',
  "AAABMJGC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABMALO","AAABMPLT","AAABMALP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZWC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMALO" varchar(10) NOT NULL default ' ',
  "AAABMAKT" varchar(10) NOT NULL default '0000000000',
  "AAABMAKU" varchar(5) NOT NULL default ' ',
  "AAABLZWV" varchar(1) NOT NULL default ' ',
  "AAABLZWM" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABMPLT","AAABMQII","AAABMJXV","AAABMJXW","AAABMALO","AAABMAKT","AAABMAKU","AAABLZWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZWW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZWY" varchar(30) NOT NULL default ' ',
  "AAABLZWZ" varchar(30) NOT NULL default ' ',
  "AAABLZXA" varchar(30) NOT NULL default ' ',
  "AAABLZXB" varchar(30) NOT NULL default ' ',
  "AAABLZXC" varchar(30) NOT NULL default ' ',
  "AAABLZXD" varchar(30) NOT NULL default ' ',
  "AAABLZXE" varchar(30) NOT NULL default ' ',
  "AAABLZXF" varchar(30) NOT NULL default ' ',
  "AAABLZXG" varchar(30) NOT NULL default ' ',
  "AAABLZXH" varchar(30) NOT NULL default ' ',
  "AAABLZXI" varchar(6) NOT NULL default '000000',
  "AAABLZXJ" varchar(6) NOT NULL default '000000',
  "AAABLZXK" varchar(6) NOT NULL default '000000',
  "AAABLZXL" varchar(6) NOT NULL default '000000',
  "AAABLZXM" varchar(6) NOT NULL default '000000',
  "AAABLZXN" varchar(6) NOT NULL default '000000',
  "AAABLZXO" varchar(6) NOT NULL default '000000',
  "AAABLZXP" varchar(6) NOT NULL default '000000',
  "AAABLZXQ" varchar(6) NOT NULL default '000000',
  "AAABLZXR" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZXT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMALO" varchar(10) NOT NULL default ' ',
  "AAABLZXZ" varchar(30) NOT NULL default ' ',
  "AAABLZYA" varchar(1) NOT NULL default ' ',
  "AAABLZYB" varchar(30) NOT NULL default ' ',
  "AAABLZYC" varchar(1) NOT NULL default ' ',
  "AAABLZYD" varchar(30) NOT NULL default ' ',
  "AAABLZYE" varchar(1) NOT NULL default ' ',
  "AAABLZYF" varchar(1) NOT NULL default ' ',
  "AAABLZYG" varchar(2) NOT NULL default '00',
  "AAABLZYH" varchar(1) NOT NULL default ' ',
  "AAABLZYI" varchar(1) NOT NULL default ' ',
  "AAABLZYJ" varchar(1) NOT NULL default ' ',
  "AAABLZYK" varchar(4) NOT NULL default '0000',
  "AAABLZYL" varchar(1) NOT NULL default ' ',
  "AAABLZYM" varchar(1) NOT NULL default ' ',
  "AAABLZYN" varchar(4) NOT NULL default '0000',
  "AAABLZYO" varchar(1) NOT NULL default ' ',
  "AAABLZYP" varchar(1) NOT NULL default ' ',
  "AAABLZYQ" varchar(1) NOT NULL default ' ',
  "AAABLZYR" varchar(1) NOT NULL default ' ',
  "AAABLZYS" varchar(1) NOT NULL default ' ',
  "AAABLZYT" varchar(1) NOT NULL default ' ',
  "AAABMABL" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABMALO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMALO" varchar(10) NOT NULL default ' ',
  "AAABMAGA" varchar(1) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABLZZR" varchar(4) NOT NULL default ' ',
  "AAABMAMV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABMALO","AAABMAGA","AAABMLHQ","AAABLZZR","AAABMAMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLZZT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMALO" varchar(10) NOT NULL default ' ',
  "AAABMAGA" varchar(1) NOT NULL default ' ',
  "AAABMAAA" varchar(1) NOT NULL default ' ',
  "AAABMAAB" varchar(1) NOT NULL default ' ',
  "AAABMAAC" varchar(1) NOT NULL default ' ',
  "AAABMAAD" varchar(1) NOT NULL default ' ',
  "AAABMAAE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABMALO","AAABMAGA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMAAL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMALO" varchar(10) NOT NULL default ' ',
  "AAABMABL" varchar(3) NOT NULL default '000',
  "AAABMAAS" varchar(1) NOT NULL default ' ',
  "AAABMAGA" varchar(1) NOT NULL default ' ',
  "AAABMAAU" varchar(1) NOT NULL default ' ',
  "AAABMAAV" varchar(2) NOT NULL default '00',
  "AAABMAAW" varchar(1) NOT NULL default ' ',
  "AAABMAAX" varchar(1) NOT NULL default ' ',
  "AAABMAAY" varchar(1) NOT NULL default ' ',
  "AAABMAAZ" varchar(3) NOT NULL default '000',
  "AAABMABA" varchar(3) NOT NULL default '000',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMIZS" varchar(30) NOT NULL default ' ',
  "AAABMABE" varchar(3) NOT NULL default ' ',
  "AAABMABF" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABMALO","AAABMABL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMABM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMALO" varchar(10) NOT NULL default ' ',
  "AAABMALP" varchar(1) NOT NULL default ' ',
  "AAABMACA" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABMALO","AAABMALP","AAABMACA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMACB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMADG" varchar(10) NOT NULL default ' ',
  "AAABMAFP" varchar(30) NOT NULL default ' ',
  "AAABMACF" varchar(2) NOT NULL default '00',
  "AAABMAFQ" varchar(30) NOT NULL default ' ',
  "AAABMACH" varchar(2) NOT NULL default '00',
  "AAABMAFR" varchar(30) NOT NULL default ' ',
  "AAABMACJ" varchar(2) NOT NULL default '00',
  "AAABMAFS" varchar(30) NOT NULL default ' ',
  "AAABMACL" varchar(2) NOT NULL default '00',
  "AAABMAFT" varchar(30) NOT NULL default ' ',
  "AAABMACN" varchar(2) NOT NULL default '00',
  "AAABMAFU" varchar(30) NOT NULL default ' ',
  "AAABMACP" varchar(2) NOT NULL default '00',
  "AAABMAFV" varchar(30) NOT NULL default ' ',
  "AAABMACR" varchar(2) NOT NULL default '00',
  "AAABMAFW" varchar(30) NOT NULL default ' ',
  "AAABMACT" varchar(2) NOT NULL default '00',
  "AAABMAFX" varchar(30) NOT NULL default ' ',
  "AAABMACV" varchar(2) NOT NULL default '00',
  "AAABMAFY" varchar(30) NOT NULL default ' ',
  "AAABMACX" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMADG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMADA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMADG" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAGQ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMADG","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMADI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMALO" varchar(10) NOT NULL default ' ',
  "AAABMAGA" varchar(1) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABMAFA" varchar(2) NOT NULL default ' ',
  "AAABMAFB" varchar(1) NOT NULL default ' ',
  "AAABMAEM" varchar(1) NOT NULL default ' ',
  "AAABMAEN" varchar(1) NOT NULL default ' ',
  "AAABMAEO" varchar(1) NOT NULL default ' ',
  "AAABMAEP" varchar(1) NOT NULL default ' ',
  "AAABMAEQ" int(11) NOT NULL default '0',
  "AAABMAER" varchar(1) NOT NULL default ' ',
  "AAABMAES" varchar(1) NOT NULL default ' ',
  "AAABMAET" varchar(1) NOT NULL default ' ',
  "AAABMAEU" varchar(1) NOT NULL default ' ',
  "AAABMAEV" varchar(1) NOT NULL default ' ',
  "AAABMAEW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMALO","AAABMAGA","AAABMLHQ","AAABMAFA","AAABMAFB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMAEG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAGA" varchar(1) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABMAFA" varchar(2) NOT NULL default ' ',
  "AAABMAFB" varchar(1) NOT NULL default ' ',
  "AAABMAEM" varchar(1) NOT NULL default ' ',
  "AAABMAEN" varchar(1) NOT NULL default ' ',
  "AAABMAEO" varchar(1) NOT NULL default ' ',
  "AAABMAEP" varchar(1) NOT NULL default ' ',
  "AAABMAEQ" int(11) NOT NULL default '0',
  "AAABMAER" varchar(1) NOT NULL default ' ',
  "AAABMAES" varchar(1) NOT NULL default ' ',
  "AAABMAET" varchar(1) NOT NULL default ' ',
  "AAABMAEU" varchar(1) NOT NULL default ' ',
  "AAABMAEV" varchar(1) NOT NULL default ' ',
  "AAABMAEW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAGA","AAABMLHQ","AAABMAFA","AAABMAFB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMAFC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAGA" varchar(1) NOT NULL default ' ',
  "AAABMAFF" varchar(30) NOT NULL default ' ',
  "AAABMAFG" varchar(30) NOT NULL default ' ',
  "AAABMAFH" varchar(30) NOT NULL default ' ',
  "AAABMAFI" varchar(30) NOT NULL default ' ',
  "AAABMAFJ" varchar(30) NOT NULL default ' ',
  "AAABMAFK" varchar(30) NOT NULL default ' ',
  "AAABMAFL" varchar(30) NOT NULL default ' ',
  "AAABMAFM" varchar(30) NOT NULL default ' ',
  "AAABMAFN" varchar(30) NOT NULL default ' ',
  "AAABMAFO" varchar(30) NOT NULL default ' ',
  "AAABMAFP" varchar(30) NOT NULL default ' ',
  "AAABMAFQ" varchar(30) NOT NULL default ' ',
  "AAABMAFR" varchar(30) NOT NULL default ' ',
  "AAABMAFS" varchar(30) NOT NULL default ' ',
  "AAABMAFT" varchar(30) NOT NULL default ' ',
  "AAABMAFU" varchar(30) NOT NULL default ' ',
  "AAABMAFV" varchar(30) NOT NULL default ' ',
  "AAABMAFW" varchar(30) NOT NULL default ' ',
  "AAABMAFX" varchar(30) NOT NULL default ' ',
  "AAABMAFY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAGA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMAGB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABMAGE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMAGH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMALP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMALP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMAGM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMALP" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAGQ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMALP","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMAGU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMJGC" varchar(1) NOT NULL default ' ',
  "AAABMAHA" varchar(1) NOT NULL default ' ',
  "AAABMAHB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMAHG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAIK" varchar(3) NOT NULL default ' ',
  "AAABMAHJ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMAIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMAHM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAIK" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAHQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAIK","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMAHU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMALO" varchar(10) NOT NULL default ' ',
  "AAABMAKT" varchar(10) NOT NULL default '0000000000',
  "AAABMAKU" varchar(5) NOT NULL default ' ',
  "AAABMAKI" varchar(7) NOT NULL default ' ',
  "AAABMAIE" varchar(8) NOT NULL default '00000000',
  "AAABMAIF" varchar(8) NOT NULL default '00000000',
  "AAABMAIG" varchar(8) NOT NULL default '00000000',
  "AAABMAIH" varchar(8) NOT NULL default '00000000',
  "AAABMAII" varchar(1) NOT NULL default ' ',
  "AAABMAIJ" varchar(4) NOT NULL default ' ',
  "AAABMAIK" varchar(3) NOT NULL default ' ',
  "AAABMAIL" varchar(1) NOT NULL default ' ',
  "AAABMAIM" varchar(1) NOT NULL default ' ',
  "AAABMGOZ" varchar(8) NOT NULL default '00000000',
  "AAABMAIO" varchar(2) NOT NULL default ' ',
  "AAABMAIP" varchar(2) NOT NULL default ' ',
  "AAABMAIQ" varchar(1) NOT NULL default ' ',
  "AAABMAIR" varchar(1) NOT NULL default ' ',
  "AAABMAIS" varchar(1) NOT NULL default ' ',
  "AAABMAIT" varchar(1) NOT NULL default ' ',
  "AAABMAIU" varchar(1) NOT NULL default ' ',
  "AAABMAIV" varchar(1) NOT NULL default ' ',
  "AAABMAIW" varchar(1) NOT NULL default ' ',
  "AAABMAIX" varchar(8) NOT NULL default '00000000',
  "AAABMAIY" varchar(1) NOT NULL default ' ',
  "AAABMAIZ" varchar(8) NOT NULL default '00000000',
  "AAABMAJA" varchar(1) NOT NULL default ' ',
  "AAABMAJB" varchar(1) NOT NULL default ' ',
  "AAABMAJC" varchar(40) NOT NULL default ' ',
  "AAABMAJD" varchar(40) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABMAJF" varchar(4) NOT NULL default ' ',
  "AAABMAJG" varchar(4) NOT NULL default ' ',
  "AAABMAJH" varchar(4) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKM" varchar(6) NOT NULL default '000000',
  "AAABMAJL" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMPLT","AAABMQII","AAABMJXV","AAABMJXW","AAABMALO","AAABMAKT","AAABMAKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMAJU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMALO" varchar(10) NOT NULL default ' ',
  "AAABMAKT" varchar(10) NOT NULL default '0000000000',
  "AAABMAKU" varchar(5) NOT NULL default ' ',
  "AAABMAKV" varchar(8) NOT NULL default '00000000',
  "AAABMAKW" varchar(6) NOT NULL default '000000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMAKG" varchar(8) NOT NULL default '00000000',
  "AAABMAKH" varchar(5) NOT NULL default ' ',
  "AAABMAKI" varchar(7) NOT NULL default ' ',
  "AAABMAKJ" varchar(7) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKM" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMPLT","AAABMQII","AAABMJXV","AAABMJXW","AAABMALO","AAABMAKT","AAABMAKU","AAABMAKV","AAABMAKW","AAABMQIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMAKY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMALO" varchar(10) NOT NULL default ' ',
  "AAABMALP" varchar(1) NOT NULL default ' ',
  "AAABMALF" varchar(3) NOT NULL default '000',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAABMALH" varchar(3) NOT NULL default '000',
  "AAABMALI" varchar(3) NOT NULL default '000',
  "AAABMJGC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABMALO","AAABMALP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMALQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMALV" varchar(4) NOT NULL default ' ',
  "AAABMALT" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMALV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMALW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMAMD" varchar(10) NOT NULL default ' ',
  "AAABMAME" varchar(10) NOT NULL default ' ',
  "AAABMAMF" varchar(10) NOT NULL default ' ',
  "AAABMAMG" varchar(10) NOT NULL default ' ',
  "AAABMAMH" varchar(10) NOT NULL default ' ',
  "AAABMAMI" varchar(10) NOT NULL default ' ',
  "AAABMAMJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPLT","AAABMQII","AAABMJXV","AAABMJXW","AAABMAMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMAMQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMAMV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMX","AAABMAMY","AAABMQIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMANA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMAND" varchar(1) NOT NULL default ' ',
  "AAABMANE" decimal(5,2) NOT NULL default '0.00',
  "AAABMANF" decimal(5,2) NOT NULL default '0.00',
  "AAABMBNF" varchar(10) NOT NULL default ' ',
  "AAABMANH" varchar(3) NOT NULL default ' ',
  "AAABMANI" varchar(1) NOT NULL default ' ',
  "AAABMANJ" varchar(1) NOT NULL default ' ',
  "AAABMANK" varchar(1) NOT NULL default ' ',
  "AAABMANL" varchar(1) NOT NULL default ' ',
  "AAABMANM" varchar(1) NOT NULL default ' ',
  "AAABMANN" varchar(3) NOT NULL default ' ',
  "AAABMANO" varchar(1) NOT NULL default '0',
  "AAABMANP" varchar(1) NOT NULL default ' ',
  "AAABMANQ" varchar(1) NOT NULL default ' ',
  "AAABMANR" varchar(1) NOT NULL default ' ',
  "AAABMANS" varchar(1) NOT NULL default ' ',
  "AAABMANT" varchar(1) NOT NULL default ' ',
  "AAABMANU" varchar(1) NOT NULL default ' ',
  "AAABMANV" varchar(1) NOT NULL default ' ',
  "AAABMANW" varchar(1) NOT NULL default ' ',
  "AAABMANX" varchar(1) NOT NULL default ' ',
  "AAABMANY" varchar(1) NOT NULL default ' ',
  "AAABMANZ" varchar(1) NOT NULL default ' ',
  "AAABMAOA" varchar(1) NOT NULL default ' ',
  "AAABMAOB" varchar(1) NOT NULL default ' ',
  "AAABMAOC" varchar(1) NOT NULL default ' ',
  "AAABMAOD" varchar(1) NOT NULL default ' ',
  "AAABMAOE" varchar(1) NOT NULL default ' ',
  "AAABMAOF" varchar(1) NOT NULL default ' ',
  "AAABMAOG" varchar(1) NOT NULL default ' ',
  "AAABMAOH" varchar(1) NOT NULL default ' ',
  "AAABMAOI" varchar(1) NOT NULL default ' ',
  "AAABMAOJ" varchar(1) NOT NULL default ' ',
  "AAABMAOK" varchar(1) NOT NULL default ' ',
  "AAABMAOL" varchar(1) NOT NULL default ' ',
  "AAABMAOM" varchar(1) NOT NULL default ' ',
  "AAABMAON" varchar(1) NOT NULL default ' ',
  "AAABMAOO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMAOR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMBVN" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCIO","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMAOZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMCHR" varchar(15) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMAPG" varchar(1) NOT NULL default ' ',
  "AAABMAPH" varchar(1) NOT NULL default ' ',
  "AAABMAPI" varchar(1) NOT NULL default ' ',
  "AAABMAPJ" varchar(1) NOT NULL default ' ',
  "AAABMAPK" varchar(1) NOT NULL default ' ',
  "AAABMAPL" varchar(1) NOT NULL default ' ',
  "AAABMAPM" varchar(15) NOT NULL default ' ',
  "AAABMAPN" varchar(15) NOT NULL default ' ',
  "AAABMAPO" varchar(1) NOT NULL default ' ',
  "AAABMAPP" varchar(1) NOT NULL default ' ',
  "AAABMBAJ" varchar(3) NOT NULL default ' ',
  "AAABMAPR" varchar(1) NOT NULL default ' ',
  "AAABMAPS" varchar(1) NOT NULL default ' ',
  "AAABMAPT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCIO","AAABMCHR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMAPX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAQE" varchar(3) NOT NULL default ' ',
  "AAABMCKH" varchar(15) NOT NULL default ' ',
  "AAABMAQG" varchar(1) NOT NULL default ' ',
  "AAABMAQC" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAQE","AAABMCKH","AAABMAQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMAQH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMCHR" varchar(15) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMBVN" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCIO","AAABMCHR","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMAQR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABMCHR" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABMCEO","AAABMCIO","AAABMNZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMARE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMCFD" varchar(3) NOT NULL default ' ',
  "AAABMCFE" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCIO","AAABMCFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMARM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABMBFP" varchar(1) NOT NULL default ' ',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMCHR" varchar(15) NOT NULL default ' ',
  "AAABMCFD" varchar(3) NOT NULL default ' ',
  "AAABMART" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCFC","AAABMBFP","AAABMCIO","AAABMCHR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMARZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMCFE" varchar(5) NOT NULL default ' ',
  "AAABMCHR" varchar(15) NOT NULL default ' ',
  "AAABMCFG" varchar(2) NOT NULL default ' ',
  "AAABMCFH" varchar(2) NOT NULL default ' ',
  "AAABMASG" varchar(1) NOT NULL default ' ',
  "AAABMCFO" varchar(1) NOT NULL default ' ',
  "AAABMBJL" varchar(10) NOT NULL default ' ',
  "AAABMASJ" varchar(5) NOT NULL default ' ',
  "AAABMBNE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCIO","AAABMCFE","AAABMCHR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMASP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCFE" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCFE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMASU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCFE" varchar(5) NOT NULL default ' ',
  "AAABMBVN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCFE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMATC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMCFE" varchar(5) NOT NULL default ' ',
  "AAABMCHR" varchar(15) NOT NULL default ' ',
  "AAABMBVN" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCIO","AAABMCFE","AAABMCHR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMATO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBJL" varchar(10) NOT NULL default ' ',
  "AAABMATR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBJL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMATU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMBJL" varchar(10) NOT NULL default ' ',
  "AAABMBVN" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMBJL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMAUC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMCHR" varchar(15) NOT NULL default ' ',
  "AAABMCEV" varchar(7) NOT NULL default '0000000',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABMBGA" varchar(8) NOT NULL default '00000000',
  "AAABMBGB" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMCIO","AAABMCHR","AAABMCEV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMAUQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCFY" varchar(3) NOT NULL default ' ',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMCHR" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCFY","AAABMCIO","AAABMCHR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMAUZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBPX" varchar(2) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMAVD" varchar(1) NOT NULL default ' ',
  "AAABMAVE" varchar(1) NOT NULL default ' ',
  "AAABMAVF" varchar(1) NOT NULL default ' ',
  "AAABMAVG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBPX","AAABMNZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMAVK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMAWF" varchar(8) NOT NULL default '00000000',
  "AAABMCHR" varchar(15) NOT NULL default ' ',
  "AAABMCFU" varchar(15) NOT NULL default ' ',
  "AAABMAVT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMPLT","AAABMCIO","AAABMNZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMAVZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMAWF" varchar(8) NOT NULL default '00000000',
  "AAABMCHR" varchar(15) NOT NULL default ' ',
  "AAABMCFU" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCSR","AAABMCIO","AAABMNZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMAWN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCFD" varchar(3) NOT NULL default ' ',
  "AAABMAWQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMAWT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCFD" varchar(3) NOT NULL default ' ',
  "AAABMBVN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMAXB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMCHF" varchar(3) NOT NULL default ' ',
  "AAABMAXG" varchar(3) NOT NULL default ' ',
  "AAABMAXH" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMAXL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABMBFP" varchar(1) NOT NULL default ' ',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMAXZ" varchar(10) NOT NULL default ' ',
  "AAABMCHR" varchar(15) NOT NULL default ' ',
  "AAABMCFD" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCFC","AAABMBFP","AAABMCIO","AAABMCRJ","AAABMAXZ","AAABMCHR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMAYB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMCHR" varchar(15) NOT NULL default ' ',
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
  "AAABMAZD" varchar(16) NOT NULL default ' ',
  "AAABMAZE" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMCIO","AAABMDEN","AAABMMVH","AAABMCHR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMAZK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMCHR" varchar(15) NOT NULL default ' ',
  "AAABMBXM" varchar(1) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABMBXE" varchar(10) NOT NULL default ' ',
  "AAABMBXF" varchar(10) NOT NULL default ' ',
  "AAABMAZT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCIO","AAABMMVH","AAABMCHR","AAABMBXM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMAZZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBAJ" varchar(3) NOT NULL default ' ',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMBAL" varchar(3) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCFI" decimal(5,2) NOT NULL default '0.00',
  "AAABMBAG" decimal(11,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBAJ","AAABMCIO","AAABMBAL","AAABMNZR","AAABMCFI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBAO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMCHR" varchar(15) NOT NULL default ' ',
  "AAABMCFG" varchar(2) NOT NULL default ' ',
  "AAABMCFH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCIO","AAABMCHR","AAABMCFG","AAABMCFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBAZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLGN" varchar(4) NOT NULL default ' ',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMCHR" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLGN","AAABMCIO","AAABMCHR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBBI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBBK" varchar(1) NOT NULL default ' ',
  "AAABMBBL" varchar(1) NOT NULL default ' ',
  "AAABMBBM" varchar(1) NOT NULL default ' ',
  "AAABMBBN" varchar(1) NOT NULL default ' ',
  "AAABMBBO" varchar(1) NOT NULL default ' ',
  "AAABMBBP" varchar(30) NOT NULL default ' ',
  "AAABMBBQ" varchar(30) NOT NULL default ' ',
  "AAABMBBR" varchar(30) NOT NULL default ' ',
  "AAABMBBS" varchar(30) NOT NULL default ' ',
  "AAABMBBT" varchar(30) NOT NULL default ' ',
  "AAABMBBU" int(11) NOT NULL default '0',
  "AAABMBBV" varchar(1) NOT NULL default ' ',
  "AAABMBBW" varchar(1) NOT NULL default ' ',
  "AAABMBBX" varchar(1) NOT NULL default ' ',
  "AAABMBBY" int(11) NOT NULL default '0',
  "AAABMBBZ" varchar(30) NOT NULL default ' ',
  "AAABMBCA" varchar(30) NOT NULL default ' ',
  "AAABMBCB" varchar(30) NOT NULL default ' ',
  "AAABMBCC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBCE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMCHR" varchar(15) NOT NULL default ' ',
  "AAABMCMG" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMCIO","AAABMCHR","AAABMCMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBCN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMCHR" varchar(15) NOT NULL default ' ',
  "AAABMCMG" varchar(3) NOT NULL default '000',
  "AAABMCIJ" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCIO","AAABMCHR","AAABMCMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBCZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBEF" varchar(2) NOT NULL default ' ',
  "AAABMBDC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMBEF" varchar(2) NOT NULL default ' ',
  "AAABMBVN" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMBEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBDN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMBEF" varchar(2) NOT NULL default ' ',
  "AAABMCHR" varchar(15) NOT NULL default ' ',
  "AAABMBFR" varchar(8) NOT NULL default '00000000',
  "AAABMBEG" decimal(6,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABMCIO","AAABMBEF","AAABMCHR","AAABMBFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBDZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABMBFR" varchar(8) NOT NULL default '00000000',
  "AAABMBEF" varchar(2) NOT NULL default ' ',
  "AAABMBEG" decimal(6,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABMCIO","AAABMCVI","AAABMCEO","AAABMBFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBEM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABMBFP" varchar(1) NOT NULL default ' ',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMBFR" varchar(8) NOT NULL default '00000000',
  "AAABMBFJ" varchar(8) NOT NULL default '00000000',
  "AAABMBFK" varchar(1) NOT NULL default ' ',
  "AAABMBFL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCFC","AAABMBFP","AAABMCIO","AAABMCVI","AAABMBFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBFC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABMBFP" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMBFR" varchar(8) NOT NULL default '00000000',
  "AAABMBFJ" varchar(8) NOT NULL default '00000000',
  "AAABMBFK" varchar(1) NOT NULL default ' ',
  "AAABMBFL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCIO","AAABMCFC","AAABMBFP","AAABMCVI","AAABMBFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBFS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMCHR" varchar(15) NOT NULL default ' ',
  "AAABMBGF" varchar(4) NOT NULL default '0000',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABMBGA" varchar(8) NOT NULL default '00000000',
  "AAABMBGB" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMCIO","AAABMCHR","AAABMBGF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBGG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPLT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBGL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBGQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMCKF" varchar(3) NOT NULL default ' ',
  "AAABMCKH" varchar(15) NOT NULL default ' ',
  "AAABMBHX" varchar(10) NOT NULL default ' ',
  "AAABMBHW" varchar(5) NOT NULL default ' ',
  "AAABMBHY" varchar(2) NOT NULL default ' ',
  "AAABMBHZ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREF","AAABMCKF","AAABMCKH","AAABMBHX","AAABMBHW","AAABMBHY","AAABMBHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBHH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMCKF" varchar(3) NOT NULL default ' ',
  "AAABMCKH" varchar(15) NOT NULL default ' ',
  "AAABMBHW" varchar(5) NOT NULL default ' ',
  "AAABMBHX" varchar(10) NOT NULL default ' ',
  "AAABMBHY" varchar(2) NOT NULL default ' ',
  "AAABMBHZ" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREF","AAABMCKF","AAABMCKH","AAABMBHW","AAABMBHX","AAABMBHY","AAABMBHZ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBIB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMBIE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBIH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBIP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBJL" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(3) NOT NULL default '000',
  "AAABMBIT" varchar(4) NOT NULL default ' ',
  "AAABMBIU" varchar(30) NOT NULL default ' ',
  "AAABMBIV" varchar(3) NOT NULL default '000',
  "AAABMBIW" varchar(3) NOT NULL default '000',
  "AAABMBIX" varchar(4) NOT NULL default ' ',
  "AAABMBIY" varchar(30) NOT NULL default ' ',
  "AAABMBIZ" varchar(3) NOT NULL default '000',
  "AAABMBJA" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMBJL","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBJE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBJL" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMBJJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBJL","AAABMLTD","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBJO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBJZ" varchar(3) NOT NULL default ' ',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABMBJU" varchar(40) NOT NULL default ' ',
  "AAABMBJV" varchar(1) NOT NULL default ' ',
  "AAABMBJW" varchar(1) NOT NULL default ' ',
  "AAABMBJX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBJZ","AAABMCIO","AAABMNZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBKC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMCHR" varchar(15) NOT NULL default ' ',
  "AAABMCFE" varchar(5) NOT NULL default ' ',
  "AAABMCJI" varchar(10) NOT NULL default ' ',
  "AAABMCHJ" varchar(3) NOT NULL default ' ',
  "AAABMCFG" varchar(2) NOT NULL default ' ',
  "AAABMCFH" varchar(2) NOT NULL default ' ',
  "AAABMCER" varchar(1) NOT NULL default ' ',
  "AAABMBLE" varchar(1) NOT NULL default ' ',
  "AAABMBLF" varchar(30) NOT NULL default ' ',
  "AAABMBKO" varchar(5) NOT NULL default ' ',
  "AAABMBKP" varchar(10) NOT NULL default ' ',
  "AAABMBKQ" varchar(3) NOT NULL default ' ',
  "AAABMBKR" varchar(2) NOT NULL default ' ',
  "AAABMBKS" varchar(2) NOT NULL default ' ',
  "AAABMBKT" varchar(1) NOT NULL default ' ',
  "AAABMBVN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCIO","AAABMCHR","AAABMCFE","AAABMCJI","AAABMCHJ","AAABMCFG","AAABMCFH","AAABMCER","AAABMBLE","AAABMBLF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBLG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBNF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBLL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMBNF" varchar(10) NOT NULL default ' ',
  "AAABMCIJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMBNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBLT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBNF" varchar(10) NOT NULL default ' ',
  "AAABMCFJ" varchar(1) NOT NULL default ' ',
  "AAABMBLX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBNF","AAABMCFJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBMB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBNF" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCFJ" varchar(1) NOT NULL default ' ',
  "AAABMCIJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBNF","AAABMRHS","AAABMCFJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBML" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBNE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBNE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBMQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMBNE" varchar(10) NOT NULL default ' ',
  "AAABMCIJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMBNE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBMY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBNE" varchar(10) NOT NULL default ' ',
  "AAABMBNF" varchar(10) NOT NULL default ' ',
  "AAABMCFJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBNE","AAABMBNF","AAABMCFJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBNH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCHC" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBNM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCHC" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCIJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCHC","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBNU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMCHD" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCIO","AAABMCHD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMCHD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCIJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCIO","AAABMCHD","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBOL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCJI" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCJI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBOQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCJI" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMBOU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCJI","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBOY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCJI" varchar(10) NOT NULL default ' ',
  "AAABMCKF" varchar(3) NOT NULL default ' ',
  "AAABMBPL" varchar(1) NOT NULL default ' ',
  "AAABMBPD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCJI","AAABMCKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCJI" varchar(10) NOT NULL default ' ',
  "AAABMCKF" varchar(3) NOT NULL default ' ',
  "AAABMBPL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCJI","AAABMCKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBPP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBPX" varchar(2) NOT NULL default ' ',
  "AAABMCKF" varchar(3) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABMCJI" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBPX","AAABMCKF","AAABMNZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBQA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBQJ" varchar(3) NOT NULL default ' ',
  "AAABMBQK" varchar(3) NOT NULL default ' ',
  "AAABMCKF" varchar(3) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABMBQH" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBQJ","AAABMBQK","AAABMCKF","AAABMNZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBQN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBQS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABMOGI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMBXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBRA" (
  "AAABMBRV" varchar(35) NOT NULL default ' ',
  "AAABMBRC" varchar(1) NOT NULL default ' ',
  "AAABMBRD" varchar(1) NOT NULL default ' ',
  "AAABMBRE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBRV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBRG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBRV" varchar(35) NOT NULL default ' ',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMCHR" varchar(15) NOT NULL default ' ',
  "AAABMBRL" varchar(30) NOT NULL default ' ',
  "AAABMBRM" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBRV","AAABMCIO","AAABMCHR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBRR" (
  "AAABMBRV" varchar(35) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMBRU" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBRV","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBRX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBTQ" varchar(16) NOT NULL default ' ',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMCHR" varchar(15) NOT NULL default ' ',
  "AAABMBSC" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBTQ","AAABMCIO","AAABMCHR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBSH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBTD" varchar(30) NOT NULL default ' ',
  "AAABMCKF" varchar(3) NOT NULL default ' ',
  "AAABMCKH" varchar(15) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABMBSO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBTD","AAABMCKF","AAABMCKH","AAABMNZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBSU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBTD" varchar(30) NOT NULL default ' ',
  "AAABMCKF" varchar(3) NOT NULL default ' ',
  "AAABMCKH" varchar(15) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMBTB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBTD","AAABMCKF","AAABMCKH","AAABMNZR","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBTI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBTQ" varchar(16) NOT NULL default ' ',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMCHR" varchar(15) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMBVN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBTQ","AAABMCIO","AAABMCHR","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBTU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBUT" varchar(10) NOT NULL default ' ',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMCHR" varchar(15) NOT NULL default ' ',
  "AAABMBTZ" varchar(4) NOT NULL default '0000',
  "AAABMBUA" varchar(12) NOT NULL default ' ',
  "AAABMBUB" varchar(12) NOT NULL default ' ',
  "AAABMBUC" varchar(8) NOT NULL default '00000000',
  "AAABMBUD" varchar(8) NOT NULL default '00000000',
  "AAABMBUE" varchar(6) NOT NULL default '000000',
  "AAABMBUF" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMBUT","AAABMCIO","AAABMCHR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBUK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBUT" varchar(10) NOT NULL default ' ',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMCHR" varchar(15) NOT NULL default ' ',
  "AAABMBUW" varchar(4) NOT NULL default '0000',
  "AAABMCII" smallint(6) NOT NULL default '0',
  "AAABMCIJ" text,
  PRIMARY KEY ("AAABMRIL","AAABMBUT","AAABMCIO","AAABMCHR","AAABMBUW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBUX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBVP" varchar(40) NOT NULL default ' ',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMCHR" varchar(15) NOT NULL default ' ',
  "AAABMBVC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBVP","AAABMCIO","AAABMCHR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBVH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBVP" varchar(40) NOT NULL default ' ',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMCHR" varchar(15) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMBVN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBVP","AAABMCIO","AAABMCHR","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBVT" (
  "AAABMBWH" varchar(1) NOT NULL default ' ',
  "AAABMBVV" varchar(1) NOT NULL default ' ',
  "AAABMBVW" varchar(1) NOT NULL default ' ',
  "AAABMBVX" varchar(1) NOT NULL default ' ',
  "AAABMBVY" varchar(1) NOT NULL default ' ',
  "AAABMBVZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBWH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBWB" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMBWH" varchar(1) NOT NULL default ' ',
  "AAABMBWP" varchar(25) NOT NULL default ' ',
  "AAABMBWQ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMBWH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBWI" (
  "AAABMCET" varchar(4) NOT NULL default '0000',
  "AAABMBWK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMCET")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBWM" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCET" varchar(4) NOT NULL default '0000',
  "AAABMBWP" varchar(25) NOT NULL default ' ',
  "AAABMBWQ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMCET")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBWT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBYL" varchar(3) NOT NULL default ' ',
  "AAABMBYM" varchar(15) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABMBXM" varchar(1) NOT NULL default ' ',
  "AAABMRBY" varchar(4) NOT NULL default '0000',
  "AAABMBXO" int(11) NOT NULL default '0',
  "AAABMBXD" varchar(8) NOT NULL default '00000000',
  "AAABMBXE" varchar(10) NOT NULL default ' ',
  "AAABMBXF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBYL","AAABMBYM","AAABMBXQ","AAABMNZR","AAABMCJZ","AAABMBXM","AAABMRBY","AAABMBXO"),
  KEY "AAABMBXP" ("AAABMBXQ"),
  KEY "AAABMBXR" ("AAABMRBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBXT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBYL" varchar(3) NOT NULL default ' ',
  "AAABMBYM" varchar(15) NOT NULL default ' ',
  "AAABMRBY" varchar(4) NOT NULL default '0000',
  "AAABMBXY" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMBYL","AAABMBYM","AAABMRBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBYD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBYL" varchar(3) NOT NULL default ' ',
  "AAABMBYM" varchar(15) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABMBYP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBYL","AAABMBYM","AAABMNZR","AAABMCJZ","AAABMBYP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBYQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCKF" varchar(3) NOT NULL default ' ',
  "AAABMCKH" varchar(15) NOT NULL default ' ',
  "AAABMCAA" varchar(8) NOT NULL default '00000000',
  "AAABMCAB" varchar(8) NOT NULL default '00000000',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMBYX" varchar(6) NOT NULL default '000000',
  "AAABMBYY" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCKF","AAABMCKH","AAABMCAA","AAABMCAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMBZE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCKF" varchar(3) NOT NULL default ' ',
  "AAABMCKH" varchar(15) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCEY" varchar(10) NOT NULL default ' ',
  "AAABMCAA" varchar(8) NOT NULL default '00000000',
  "AAABMCAB" varchar(8) NOT NULL default '00000000',
  "AAABMPTM" decimal(15,3) NOT NULL default '0.000',
  "AAABMBZP" varchar(3) NOT NULL default ' ',
  "AAABMCHE" decimal(15,5) NOT NULL default '0.00000',
  "AAABMCHF" varchar(3) NOT NULL default ' ',
  "AAABMCFI" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMCKF","AAABMCKH","AAABMCVI","AAABMCEO","AAABMLCZ","AAABMCEY","AAABMCAA","AAABMCAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMCAC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMCKF" varchar(3) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCKH" varchar(15) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABMCJG" varchar(15) NOT NULL default ' ',
  "AAABMCJH" varchar(15) NOT NULL default ' ',
  "AAABMCJI" varchar(10) NOT NULL default ' ',
  "AAABMCAN" varchar(1) NOT NULL default ' ',
  "AAABMCAO" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCSR","AAABMCKF","AAABMREX","AAABMCKH","AAABMNZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMCAV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMCKF" varchar(3) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCKH" varchar(15) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABMCKA" varchar(1) NOT NULL default ' ',
  "AAABMCKB" varchar(3) NOT NULL default ' ',
  "AAABMCKC" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCSR","AAABMCKF","AAABMREX","AAABMCKH","AAABMNZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMCBM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMCBQ" varchar(1) NOT NULL default ' ',
  "AAABMCFI" decimal(5,2) NOT NULL default '0.00',
  "AAABMCBS" decimal(5,2) NOT NULL default '0.00',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCBU" varchar(1) NOT NULL default ' ',
  "AAABMCGF" decimal(15,5) NOT NULL default '0.00000',
  "AAABMCGG" varchar(3) NOT NULL default ' ',
  "AAABMCBX" varchar(1) NOT NULL default ' ',
  "AAABMCDU" decimal(13,3) NOT NULL default '0.000',
  "AAABMCDV" varchar(3) NOT NULL default ' ',
  "AAABMCDW" decimal(13,3) NOT NULL default '0.000',
  "AAABMCDX" varchar(3) NOT NULL default ' ',
  "AAABMCDY" varchar(25) NOT NULL default ' ',
  "AAABMCKW" varchar(2) NOT NULL default ' ',
  "AAABMCDZ" varchar(25) NOT NULL default ' ',
  "AAABMCCF" varchar(1) NOT NULL default ' ',
  "AAABMCHC" varchar(6) NOT NULL default ' ',
  "AAABMCHD" varchar(6) NOT NULL default ' ',
  "AAABMCCI" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABMCIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMCCM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCFL" varchar(15) NOT NULL default ' ',
  "AAABMCFM" varchar(15) NOT NULL default ' ',
  "AAABMCFN" varchar(20) NOT NULL default ' ',
  "AAABMCFJ" varchar(1) NOT NULL default ' ',
  "AAABMCFK" varchar(1) NOT NULL default ' ',
  "AAABMCDU" decimal(13,3) NOT NULL default '0.000',
  "AAABMCDV" varchar(3) NOT NULL default ' ',
  "AAABMCDW" decimal(13,3) NOT NULL default '0.000',
  "AAABMCDX" varchar(3) NOT NULL default ' ',
  "AAABMCDY" varchar(25) NOT NULL default ' ',
  "AAABMCDZ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMCDE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCDI" decimal(5,2) NOT NULL default '0.00',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABMCDL" decimal(7,5) NOT NULL default '0.00000',
  "AAABMCDM" decimal(7,5) NOT NULL default '0.00000',
  "AAABMCDN" varchar(8) NOT NULL default '00000000',
  "AAABMCDO" decimal(7,5) NOT NULL default '0.00000',
  "AAABMCDP" decimal(7,5) NOT NULL default '0.00000',
  "AAABMCDQ" varchar(8) NOT NULL default '00000000',
  "AAABMCFP" decimal(7,5) NOT NULL default '0.00000',
  "AAABMCDS" decimal(7,5) NOT NULL default '0.00000',
  "AAABMCDT" varchar(8) NOT NULL default '00000000',
  "AAABMCDU" decimal(13,3) NOT NULL default '0.000',
  "AAABMCDV" varchar(3) NOT NULL default ' ',
  "AAABMCDW" decimal(13,3) NOT NULL default '0.000',
  "AAABMCDX" varchar(3) NOT NULL default ' ',
  "AAABMCDY" varchar(25) NOT NULL default ' ',
  "AAABMCDZ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMCED" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCME" varchar(4) NOT NULL default '0000',
  "AAABMCIM" varchar(10) NOT NULL default ' ',
  "AAABMCMG" varchar(4) NOT NULL default '0000',
  "AAABMCHO" varchar(3) NOT NULL default '000',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMCEK" varchar(2) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCHR" varchar(15) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMCEQ" varchar(1) NOT NULL default ' ',
  "AAABMCER" varchar(1) NOT NULL default ' ',
  "AAABMCES" varchar(1) NOT NULL default ' ',
  "AAABMCET" varchar(4) NOT NULL default '0000',
  "AAABMCEU" varchar(8) NOT NULL default '00000000',
  "AAABMCEV" varchar(7) NOT NULL default '0000000',
  "AAABMCEW" varchar(6) NOT NULL default '000000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCEY" varchar(10) NOT NULL default ' ',
  "AAABMCEZ" varchar(1) NOT NULL default ' ',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABMCFD" varchar(3) NOT NULL default ' ',
  "AAABMCFE" varchar(5) NOT NULL default ' ',
  "AAABMCFF" varchar(5) NOT NULL default ' ',
  "AAABMCFG" varchar(2) NOT NULL default ' ',
  "AAABMCFH" varchar(2) NOT NULL default ' ',
  "AAABMCFI" decimal(5,2) NOT NULL default '0.00',
  "AAABMCFJ" varchar(1) NOT NULL default ' ',
  "AAABMCFK" varchar(1) NOT NULL default ' ',
  "AAABMCFL" varchar(15) NOT NULL default ' ',
  "AAABMCFM" varchar(15) NOT NULL default ' ',
  "AAABMCFN" varchar(20) NOT NULL default ' ',
  "AAABMCFO" varchar(1) NOT NULL default ' ',
  "AAABMCFP" decimal(7,5) NOT NULL default '0.00000',
  "AAABMCFQ" decimal(15,5) NOT NULL default '0.00000',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMCKE" varchar(10) NOT NULL default ' ',
  "AAABMCFT" varchar(15) NOT NULL default ' ',
  "AAABMCFU" varchar(15) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABMCFX" varchar(16) NOT NULL default ' ',
  "AAABMCFY" varchar(3) NOT NULL default ' ',
  "AAABMLGN" varchar(4) NOT NULL default ' ',
  "AAABMCGA" varchar(4) NOT NULL default ' ',
  "AAABMCGB" varchar(4) NOT NULL default ' ',
  "AAABMCGC" decimal(15,5) NOT NULL default '0.00000',
  "AAABMCGD" decimal(13,2) NOT NULL default '0.00',
  "AAABMCGE" decimal(13,2) NOT NULL default '0.00',
  "AAABMCGF" decimal(15,5) NOT NULL default '0.00000',
  "AAABMCGG" varchar(3) NOT NULL default ' ',
  "AAABMCGH" decimal(15,5) NOT NULL default '0.00000',
  "AAABMCGI" varchar(3) NOT NULL default ' ',
  "AAABMCGJ" decimal(6,3) NOT NULL default '0.000',
  "AAABMCGK" varchar(3) NOT NULL default ' ',
  "AAABMCGL" decimal(17,5) NOT NULL default '0.00000',
  "AAABMCGM" varchar(5) NOT NULL default ' ',
  "AAABMCKW" varchar(2) NOT NULL default ' ',
  "AAABMCGO" decimal(17,5) NOT NULL default '0.00000',
  "AAABMCGP" varchar(5) NOT NULL default ' ',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABMCGT" varchar(1) NOT NULL default ' ',
  "AAABMCGU" varchar(1) NOT NULL default ' ',
  "AAABMCGV" varchar(1) NOT NULL default ' ',
  "AAABMCGW" varchar(1) NOT NULL default ' ',
  "AAABMCGX" varchar(1) NOT NULL default ' ',
  "AAABMCGY" varchar(1) NOT NULL default ' ',
  "AAABMCGZ" varchar(1) NOT NULL default ' ',
  "AAABMCHA" varchar(1) NOT NULL default ' ',
  "AAABMCHB" varchar(1) NOT NULL default ' ',
  "AAABMCHC" varchar(6) NOT NULL default ' ',
  "AAABMCHD" varchar(6) NOT NULL default ' ',
  "AAABMCHE" decimal(15,5) NOT NULL default '0.00000',
  "AAABMCHF" varchar(3) NOT NULL default ' ',
  "AAABMCHG" varchar(1) NOT NULL default ' ',
  "AAABMCHH" varchar(1) NOT NULL default ' ',
  "AAABMCJI" varchar(10) NOT NULL default ' ',
  "AAABMCHJ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCME","AAABMCIM","AAABMCMG","AAABMCHO","AAABMCIO"),
  KEY "AAABMCHQ" ("AAABMCHR","AAABMCIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMCHT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCME" varchar(4) NOT NULL default '0000',
  "AAABMCIM" varchar(10) NOT NULL default ' ',
  "AAABMCMG" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMCME","AAABMCIM","AAABMCMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMCIC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCME" varchar(4) NOT NULL default '0000',
  "AAABMCIM" varchar(10) NOT NULL default ' ',
  "AAABMCMG" varchar(4) NOT NULL default '0000',
  "AAABMCIO" varchar(3) NOT NULL default ' ',
  "AAABMCII" smallint(6) NOT NULL default '0',
  "AAABMCIJ" text,
  PRIMARY KEY ("AAABMRIL","AAABMCME","AAABMCIM","AAABMCMG","AAABMCIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMCIP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKFU" decimal(21,7) NOT NULL default '0.0000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMCIY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCKE" varchar(10) NOT NULL default ' ',
  "AAABMCKF" varchar(3) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCKH" varchar(15) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABMCJG" varchar(15) NOT NULL default ' ',
  "AAABMCJH" varchar(15) NOT NULL default ' ',
  "AAABMCJI" varchar(10) NOT NULL default ' ',
  "AAABMCJJ" varchar(1) NOT NULL default ' ',
  "AAABMCJK" varchar(3) NOT NULL default ' ',
  "AAABMCJL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCKE","AAABMCKF","AAABMREX","AAABMCKH","AAABMNZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMCJS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCKE" varchar(10) NOT NULL default ' ',
  "AAABMCKF" varchar(3) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCKH" varchar(15) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABMCKA" varchar(1) NOT NULL default ' ',
  "AAABMCKB" varchar(3) NOT NULL default ' ',
  "AAABMCKC" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCKE","AAABMCKF","AAABMREX","AAABMCKH","AAABMNZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMCKJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCKW" varchar(2) NOT NULL default ' ',
  "AAABMCKM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCKW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMCKP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCKW" varchar(2) NOT NULL default ' ',
  "AAABMCKT" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCKW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMCKX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCME" varchar(4) NOT NULL default '0000',
  "AAABMCMF" varchar(10) NOT NULL default ' ',
  "AAABMDGB" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCME","AAABMCMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMCLF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCME" varchar(4) NOT NULL default '0000',
  "AAABMCLX" varchar(10) NOT NULL default ' ',
  "AAABMCLJ" varchar(8) NOT NULL default '00000000',
  "AAABMCLK" varchar(6) NOT NULL default '000000',
  "AAABMCLL" varchar(12) NOT NULL default ' ',
  "AAABMCLM" varchar(8) NOT NULL default '00000000',
  "AAABMCLN" varchar(6) NOT NULL default '000000',
  "AAABMCLO" varchar(12) NOT NULL default ' ',
  "AAABMCLP" varchar(8) NOT NULL default '00000000',
  "AAABMCLQ" varchar(6) NOT NULL default '000000',
  "AAABMCLR" varchar(12) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABMCLT" varchar(15) NOT NULL default ' ',
  "AAABMCLU" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCME","AAABMCLX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMCLY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCME" varchar(4) NOT NULL default '0000',
  "AAABMCMF" varchar(10) NOT NULL default ' ',
  "AAABMCMG" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMCME","AAABMCMF","AAABMCMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMCMH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMPUA" varchar(16) NOT NULL default '0000000000000000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMCMR" varchar(2) NOT NULL default '00',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMCMV" varchar(8) NOT NULL default '00000000',
  "AAABMCMW" varchar(8) NOT NULL default '00000000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABMDEP" varchar(4) NOT NULL default ' ',
  "AAABMCMZ" varchar(16) NOT NULL default ' ',
  "AAABMDGB" varchar(16) NOT NULL default ' ',
  "AAABMCNB" varchar(10) NOT NULL default ' ',
  "AAABMCNC" varchar(10) NOT NULL default ' ',
  "AAABMCND" varchar(4) NOT NULL default '0000',
  "AAABMCNE" varchar(25) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMCNG" decimal(9,5) NOT NULL default '0.00000',
  "AAABMCNH" varchar(5) NOT NULL default ' ',
  "AAABMCNI" decimal(9,5) NOT NULL default '0.00000',
  "AAABMCNJ" varchar(1) NOT NULL default ' ',
  "AAABMCNK" varchar(1) NOT NULL default ' ',
  "AAABMCNL" decimal(13,2) NOT NULL default '0.00',
  "AAABMCNM" varchar(1) NOT NULL default ' ',
  "AAABMCNN" varchar(4) NOT NULL default ' ',
  "AAABMCNO" varchar(12) NOT NULL default ' ',
  "AAABMCNP" varchar(40) NOT NULL default ' ',
  "AAABMCNQ" varchar(10) NOT NULL default ' ',
  "AAABMCNR" varchar(2) NOT NULL default ' ',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABMDGA" varchar(20) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMCNV" varchar(5) NOT NULL default ' ',
  "AAABMCNW" varchar(5) NOT NULL default ' ',
  "AAABMCNX" varchar(5) NOT NULL default ' ',
  "AAABMCNY" decimal(9,5) NOT NULL default '0.00000',
  "AAABMCNZ" decimal(9,5) NOT NULL default '0.00000',
  "AAABMCOA" varchar(1) NOT NULL default ' ',
  "AAABMCOB" varchar(1) NOT NULL default ' ',
  "AAABMCOC" varchar(1) NOT NULL default ' ',
  "AAABMCOD" varchar(1) NOT NULL default ' ',
  "AAABMCOE" varchar(1) NOT NULL default ' ',
  "AAABMCOF" varchar(8) NOT NULL default '00000000',
  "AAABMCOG" varchar(1) NOT NULL default ' ',
  "AAABMCOH" varchar(2) NOT NULL default ' ',
  "AAABMCOI" varchar(2) NOT NULL default ' ',
  "AAABMCOJ" varchar(4) NOT NULL default ' ',
  "AAABMCOK" varchar(4) NOT NULL default ' ',
  "AAABMCOL" varchar(1) NOT NULL default ' ',
  "AAABMCOM" varchar(4) NOT NULL default ' ',
  "AAABMCON" varchar(1) NOT NULL default ' ',
  "AAABMCOO" varchar(1) NOT NULL default ' ',
  "AAABMCOP" varchar(10) NOT NULL default ' ',
  "AAABMCOQ" decimal(9,5) NOT NULL default '0.00000',
  "AAABMCOR" varchar(10) NOT NULL default ' ',
  "AAABMCOS" varchar(1) NOT NULL default ' ',
  "AAABMCOT" varchar(2) NOT NULL default ' ',
  "AAABMCOU" varchar(12) NOT NULL default ' ',
  "AAABMCOV" varchar(4) NOT NULL default ' ',
  "AAABMCOW" varchar(3) NOT NULL default '000',
  "AAABMCOX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPMH","AAABMPUA"),
  KEY "AAABMCPB" ("AAABMPUA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMCPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMPUA" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCPL" varchar(1) NOT NULL default ' ',
  "AAABMCPM" varchar(8) NOT NULL default '00000000',
  "AAABMCPN" varchar(8) NOT NULL default '00000000',
  "AAABMCPO" varchar(10) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABMCPR" varchar(1) NOT NULL default ' ',
  "AAABMCPS" varchar(1) NOT NULL default ' ',
  "AAABMCPT" varchar(1) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMCPW" varchar(4) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABMCPY" varchar(2) NOT NULL default ' ',
  "AAABMCPZ" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQA" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQB" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQC" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQD" varchar(5) NOT NULL default ' ',
  "AAABMCQE" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQF" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQG" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQH" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQI" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQJ" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQK" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQL" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQM" varchar(1) NOT NULL default ' ',
  "AAABMCQN" varchar(2) NOT NULL default ' ',
  "AAABMCQO" varchar(1) NOT NULL default ' ',
  "AAABMCQP" varchar(3) NOT NULL default '000',
  "AAABMCQQ" varchar(3) NOT NULL default ' ',
  "AAABMCQR" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQS" decimal(9,5) NOT NULL default '0.00000',
  "AAABMCQT" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQU" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQV" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQW" varchar(8) NOT NULL default '00000000',
  "AAABMDGC" varchar(18) NOT NULL default ' ',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAABMCQZ" varchar(2) NOT NULL default ' ',
  "AAABMCRA" varchar(6) NOT NULL default ' ',
  "AAABMODA" varchar(3) NOT NULL default ' ',
  "AAABMCRC" varchar(10) NOT NULL default ' ',
  "AAABMCRD" varchar(4) NOT NULL default ' ',
  "AAABMCRE" varchar(2) NOT NULL default ' ',
  "AAABMCRF" varchar(10) NOT NULL default ' ',
  "AAABMCRG" decimal(13,2) NOT NULL default '0.00',
  "AAABMCRH" varchar(8) NOT NULL default '00000000',
  "AAABMCRI" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMCRL" varchar(16) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMCRO" varchar(10) NOT NULL default ' ',
  "AAABMCRP" varchar(6) NOT NULL default '000000',
  "AAABMCRQ" varchar(4) NOT NULL default '0000',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABMCRT" varchar(3) NOT NULL default ' ',
  "AAABMCRU" varchar(8) NOT NULL default '00000000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABMCRW" varchar(1) NOT NULL default ' ',
  "AAABMCRX" varchar(1) NOT NULL default ' ',
  "AAABMCRY" varchar(1) NOT NULL default ' ',
  "AAABMCRZ" varchar(1) NOT NULL default ' ',
  "AAABMCSA" varchar(1) NOT NULL default ' ',
  "AAABMCSB" varchar(1) NOT NULL default ' ',
  "AAABMCSC" varchar(1) NOT NULL default ' ',
  "AAABMCSD" varchar(1) NOT NULL default ' ',
  "AAABMCSE" varchar(1) NOT NULL default ' ',
  "AAABMCSF" varchar(1) NOT NULL default ' ',
  "AAABMCSG" varchar(1) NOT NULL default ' ',
  "AAABMCSH" varchar(1) NOT NULL default ' ',
  "AAABMCSI" varchar(1) NOT NULL default ' ',
  "AAABMCSJ" varchar(1) NOT NULL default ' ',
  "AAABMCSK" varchar(1) NOT NULL default ' ',
  "AAABMCSL" varchar(1) NOT NULL default ' ',
  "AAABMCSM" varchar(1) NOT NULL default ' ',
  "AAABMCSN" varchar(1) NOT NULL default ' ',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMCSS" varchar(10) NOT NULL default ' ',
  "AAABMCST" varchar(1) NOT NULL default ' ',
  "AAABMCSU" varchar(2) NOT NULL default ' ',
  "AAABMCSV" varchar(18) NOT NULL default ' ',
  "AAABMCSW" varchar(8) NOT NULL default '00000000',
  "AAABMMRQ" varchar(4) NOT NULL default ' ',
  "AAABMCSY" decimal(3,0) NOT NULL default '0',
  "AAABMCSZ" decimal(3,0) NOT NULL default '0',
  "AAABMCTA" decimal(3,0) NOT NULL default '0',
  "AAABMCTB" decimal(5,3) NOT NULL default '0.000',
  "AAABMCTC" decimal(5,3) NOT NULL default '0.000',
  "AAABMCTD" decimal(13,2) NOT NULL default '0.00',
  "AAABMCTE" decimal(13,2) NOT NULL default '0.00',
  "AAABMCTF" decimal(13,2) NOT NULL default '0.00',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAABMCTH" varchar(1) NOT NULL default ' ',
  "AAABMCTI" varchar(1) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABMCTK" varchar(4) NOT NULL default ' ',
  "AAABMCTL" decimal(13,2) NOT NULL default '0.00',
  "AAABMCTM" varchar(2) NOT NULL default ' ',
  "AAABMCTN" decimal(13,2) NOT NULL default '0.00',
  "AAABMCTO" decimal(13,2) NOT NULL default '0.00',
  "AAABMCTP" varchar(2) NOT NULL default ' ',
  "AAABMCTQ" decimal(13,2) NOT NULL default '0.00',
  "AAABMCTR" decimal(13,2) NOT NULL default '0.00',
  "AAABMCTS" varchar(2) NOT NULL default ' ',
  "AAABMCTT" decimal(13,2) NOT NULL default '0.00',
  "AAABMCTU" decimal(13,2) NOT NULL default '0.00',
  "AAABMCTV" varchar(10) NOT NULL default ' ',
  "AAABMCTW" varchar(4) NOT NULL default '0000',
  "AAABMCTX" varchar(3) NOT NULL default '000',
  "AAABMCTY" varchar(1) NOT NULL default ' ',
  "AAABMCTZ" varchar(8) NOT NULL default ' ',
  "AAABMCUA" varchar(8) NOT NULL default '00000000',
  "AAABMCUB" varchar(3) NOT NULL default ' ',
  "AAABMCUC" varchar(3) NOT NULL default ' ',
  "AAABMCUD" varchar(1) NOT NULL default ' ',
  "AAABMCUE" varchar(8) NOT NULL default '00000000',
  "AAABMCUF" varchar(6) NOT NULL default ' ',
  "AAABMCUG" varchar(1) NOT NULL default ' ',
  "AAABMCUH" varchar(8) NOT NULL default '00000000',
  "AAABMCUI" varchar(10) NOT NULL default ' ',
  "AAABMCUJ" varchar(4) NOT NULL default '0000',
  "AAABMCUK" varchar(3) NOT NULL default '000',
  "AAABMCUL" varchar(1) NOT NULL default ' ',
  "AAABMCUM" varchar(10) NOT NULL default ' ',
  "AAABMCUN" varchar(4) NOT NULL default '0000',
  "AAABMCUO" varchar(4) NOT NULL default ' ',
  "AAABMCUP" varchar(8) NOT NULL default '00000000',
  "AAABMCUQ" decimal(13,2) NOT NULL default '0.00',
  "AAABMCUR" varchar(2) NOT NULL default ' ',
  "AAABMCUS" decimal(7,2) NOT NULL default '0.00',
  "AAABMPSH" varchar(1) NOT NULL default ' ',
  "AAABMCUU" varchar(1) NOT NULL default ' ',
  "AAABMCUV" varchar(8) NOT NULL default '00000000',
  "AAABMCUW" varchar(1) NOT NULL default '0',
  "AAABMCUX" varchar(2) NOT NULL default ' ',
  "AAABMCUY" varchar(11) NOT NULL default ' ',
  "AAABMCUZ" varchar(27) NOT NULL default ' ',
  "AAABMCVA" varchar(2) NOT NULL default ' ',
  "AAABMCVB" decimal(13,2) NOT NULL default '0.00',
  "AAABMCVC" varchar(10) NOT NULL default ' ',
  "AAABMCVD" decimal(13,2) NOT NULL default '0.00',
  "AAABMCVE" decimal(13,2) NOT NULL default '0.00',
  "AAABMCVF" decimal(13,2) NOT NULL default '0.00',
  "AAABMCVG" decimal(13,2) NOT NULL default '0.00',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABMCVL" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVM" varchar(3) NOT NULL default ' ',
  "AAABMCVN" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVO" varchar(3) NOT NULL default ' ',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABMCVR" varchar(2) NOT NULL default '00',
  "AAABMCVS" varchar(1) NOT NULL default ' ',
  "AAABMCVT" varchar(1) NOT NULL default ' ',
  "AAABMCVU" decimal(5,0) NOT NULL default '0',
  "AAABMCVV" varchar(4) NOT NULL default ' ',
  "AAABMCVW" varchar(10) NOT NULL default ' ',
  "AAABMCVX" varchar(4) NOT NULL default ' ',
  "AAABMCVY" decimal(13,2) NOT NULL default '0.00',
  "AAABMCVZ" decimal(13,2) NOT NULL default '0.00',
  "AAABMCWA" decimal(13,2) NOT NULL default '0.00',
  "AAABMCWB" decimal(13,2) NOT NULL default '0.00',
  "AAABMCWC" varchar(1) NOT NULL default ' ',
  "AAABMCWD" decimal(11,2) NOT NULL default '0.00',
  "AAABMCWE" varchar(1) NOT NULL default ' ',
  "AAABMCWF" varchar(1) NOT NULL default ' ',
  "AAABMCWG" varchar(1) NOT NULL default ' ',
  "AAABMCWH" varchar(1) NOT NULL default ' ',
  "AAABMCWI" varchar(1) NOT NULL default ' ',
  "AAABMCWJ" varchar(1) NOT NULL default ' ',
  "AAABMCWK" varchar(1) NOT NULL default ' ',
  "AAABMCWL" varchar(20) NOT NULL default ' ',
  "AAABMCWM" varchar(3) NOT NULL default ' ',
  "AAABMCWN" varchar(3) NOT NULL default ' ',
  "AAABMCWO" varchar(3) NOT NULL default ' ',
  "AAABMCWP" varchar(4) NOT NULL default ' ',
  "AAABMCWQ" varchar(3) NOT NULL default '000',
  "AAABMCWR" decimal(13,2) NOT NULL default '0.00',
  "AAABMCWS" decimal(13,2) NOT NULL default '0.00',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAABMCWU" varchar(1) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCWW" varchar(2) NOT NULL default ' ',
  "AAABMCWX" varchar(3) NOT NULL default ' ',
  "AAABMCWY" varchar(10) NOT NULL default ' ',
  "AAABMKSP" varchar(1) NOT NULL default ' ',
  "AAABMDGQ" varchar(13) NOT NULL default ' ',
  "AAABMOAI" varchar(4) NOT NULL default ' ',
  "AAABMCXC" varchar(10) NOT NULL default ' ',
  "AAABMCXD" varchar(15) NOT NULL default ' ',
  "AAABMCXE" varchar(8) NOT NULL default ' ',
  "AAABMCXF" varchar(8) NOT NULL default '00000000',
  "AAABMCXG" decimal(9,6) NOT NULL default '0.000000',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAABMCXI" varchar(12) NOT NULL default ' ',
  "AAABMCXJ" varchar(12) NOT NULL default ' ',
  "AAABMCXK" varchar(10) NOT NULL default '0000000000',
  "AAABMCXL" varchar(8) NOT NULL default '00000000',
  "AAABMCXM" varchar(8) NOT NULL default '00000000',
  "AAABMCXN" varchar(10) NOT NULL default '0000000000',
  "AAABMCXO" varchar(4) NOT NULL default '0000',
  "AAABMCXP" varchar(1) NOT NULL default ' ',
  "AAABMCXQ" varchar(1) NOT NULL default ' ',
  "AAABMCXR" varchar(2) NOT NULL default ' ',
  "AAABMCXS" varchar(3) NOT NULL default ' ',
  "AAABMCXT" varchar(1) NOT NULL default ' ',
  "AAABMCXU" varchar(5) NOT NULL default '00000',
  "AAABMCXV" varchar(4) NOT NULL default ' ',
  "AAABMCXW" decimal(13,2) NOT NULL default '0.00',
  "AAABMCXX" decimal(13,2) NOT NULL default '0.00',
  "AAABMCXY" decimal(13,2) NOT NULL default '0.00',
  "AAABMCXZ" decimal(13,2) NOT NULL default '0.00',
  "AAABMCYA" decimal(13,2) NOT NULL default '0.00',
  "AAABMCYB" decimal(13,2) NOT NULL default '0.00',
  "AAABMCYC" decimal(13,2) NOT NULL default '0.00',
  "AAABMCYD" decimal(13,2) NOT NULL default '0.00',
  "AAABMCYE" decimal(13,2) NOT NULL default '0.00',
  "AAABMCYF" decimal(13,2) NOT NULL default '0.00',
  "AAABMCYG" decimal(13,2) NOT NULL default '0.00',
  "AAABMCYH" decimal(13,2) NOT NULL default '0.00',
  "AAABMCYI" decimal(13,2) NOT NULL default '0.00',
  "AAABMCYJ" decimal(13,2) NOT NULL default '0.00',
  "AAABMCYK" decimal(13,2) NOT NULL default '0.00',
  "AAABMCYL" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMPMH","AAABMPUA","AAABMPWO"),
  KEY "AAABMCYQ" ("AAABMPUA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMCYS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMPUA" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCZA" varchar(1) NOT NULL default ' ',
  "AAABMCZB" varchar(1) NOT NULL default ' ',
  "AAABMCZC" varchar(1) NOT NULL default ' ',
  "AAABMCZD" varchar(2) NOT NULL default ' ',
  "AAABMCZE" varchar(10) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABMCZH" varchar(4) NOT NULL default ' ',
  "AAABMCZI" decimal(13,2) NOT NULL default '0.00',
  "AAABMCZJ" decimal(13,2) NOT NULL default '0.00',
  "AAABMCZK" varchar(10) NOT NULL default ' ',
  "AAABMCZL" varchar(12) NOT NULL default ' ',
  "AAABMCZM" varchar(12) NOT NULL default ' ',
  "AAABMCZN" varchar(10) NOT NULL default ' ',
  "AAABMCZO" varchar(3) NOT NULL default '000',
  "AAABMCZP" decimal(13,2) NOT NULL default '0.00',
  "AAABMCZQ" varchar(16) NOT NULL default ' ',
  "AAABMCZR" varchar(3) NOT NULL default '000',
  "AAABMCZS" varchar(1) NOT NULL default ' ',
  "AAABMCZT" varchar(3) NOT NULL default '000',
  "AAABMCZU" varchar(10) NOT NULL default ' ',
  "AAABMCZV" varchar(4) NOT NULL default ' ',
  "AAABMCZW" varchar(10) NOT NULL default ' ',
  "AAABMCZX" varchar(20) NOT NULL default ' ',
  "AAABMCZY" varchar(2) NOT NULL default '00',
  "AAABMCZZ" varchar(2) NOT NULL default '00',
  "AAABMDAA" varchar(2) NOT NULL default '00',
  "AAABMDAB" varchar(2) NOT NULL default '00',
  "AAABMDAC" varchar(2) NOT NULL default ' ',
  "AAABMDAD" varchar(3) NOT NULL default ' ',
  "AAABMDAE" varchar(2) NOT NULL default ' ',
  "AAABMDAF" varchar(1) NOT NULL default ' ',
  "AAABMDAG" varchar(30) NOT NULL default ' ',
  "AAABMDAH" decimal(13,2) NOT NULL default '0.00',
  "AAABMDAI" varchar(5) NOT NULL default ' ',
  "AAABMDAJ" varchar(8) NOT NULL default '00000000',
  "AAABMDAK" varchar(2) NOT NULL default ' ',
  "AAABMDAL" varchar(50) NOT NULL default ' ',
  "AAABMDAM" varchar(8) NOT NULL default '00000000',
  "AAABMPWM" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMPMH","AAABMPUA","AAABMPWO"),
  KEY "AAABMDAS" ("AAABMPUA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDAU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMPUA" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMPMH","AAABMPUA","AAABMPWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDBG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABMDER" varchar(1) NOT NULL default ' ',
  "AAABMDBL" varchar(1) NOT NULL default ' ',
  "AAABMNLX" varchar(2) NOT NULL default ' ',
  "AAABMDBN" varchar(4) NOT NULL default ' ',
  "AAABMDBO" varchar(4) NOT NULL default ' ',
  "AAABMNLY" varchar(4) NOT NULL default ' ',
  "AAABMDCO" varchar(20) NOT NULL default ' ',
  "AAABMDBR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP","AAABMNLV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDBV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default '0000',
  "AAABMDER" varchar(1) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABMDCB" varchar(4) NOT NULL default ' ',
  "AAABMDCC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNLV","AAABMGHE","AAABMDER","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDCI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABMDCR" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABMDCO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNLV","AAABMDCR","AAABMLPV","AAABMGHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDCU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNDU" varchar(40) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMFSV" varchar(4) NOT NULL default ' ',
  "AAABMDCZ" varchar(3) NOT NULL default ' ',
  "AAABMDDA" varchar(1) NOT NULL default ' ',
  "AAABMDDB" varchar(1) NOT NULL default ' ',
  "AAABMDDC" varchar(1) NOT NULL default ' ',
  "AAABMDDD" varchar(1) NOT NULL default ' ',
  "AAABMDDE" varchar(1) NOT NULL default ' ',
  "AAABMDDF" varchar(3) NOT NULL default ' ',
  "AAABMDDG" varchar(1) NOT NULL default ' ',
  "AAABMDDH" varchar(1) NOT NULL default ' ',
  "AAABMDDI" varchar(1) NOT NULL default ' ',
  "AAABMDDJ" varchar(1) NOT NULL default ' ',
  "AAABMDDK" varchar(1) NOT NULL default ' ',
  "AAABMDDL" varchar(3) NOT NULL default ' ',
  "AAABMDDM" varchar(1) NOT NULL default ' ',
  "AAABMDDN" varchar(1) NOT NULL default ' ',
  "AAABMDDO" varchar(1) NOT NULL default ' ',
  "AAABMDDP" varchar(1) NOT NULL default ' ',
  "AAABMDDQ" varchar(1) NOT NULL default ' ',
  "AAABMDDR" varchar(3) NOT NULL default ' ',
  "AAABMDDS" varchar(1) NOT NULL default ' ',
  "AAABMDDT" varchar(1) NOT NULL default ' ',
  "AAABMDDU" varchar(1) NOT NULL default ' ',
  "AAABMDDV" varchar(1) NOT NULL default ' ',
  "AAABMDDW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNDU","AAABMDEP","AAABMFSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDEB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABMDER" varchar(1) NOT NULL default ' ',
  "AAABMDEI" varchar(20) NOT NULL default ' ',
  "AAABMDEJ" varchar(40) NOT NULL default ' ',
  "AAABMDEK" varchar(30) NOT NULL default ' ',
  "AAABMDEL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABMNLV","AAABMDEP","AAABMNLW","AAABMDER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDES" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMDFC" varchar(4) NOT NULL default '0000',
  "AAABMFSV" varchar(4) NOT NULL default '0000',
  "AAABMDEY" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNLV","AAABMREL","AAABMDFC","AAABMFSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDFE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRIP" varchar(4) NOT NULL default '0000',
  "AAABMDFI" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMRIP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDFM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMELL" varchar(10) NOT NULL default '0000000000',
  "AAABMDGM" varchar(10) NOT NULL default '0000000000',
  "AAABMDFQ" varchar(10) NOT NULL default ' ',
  "AAABMDFR" varchar(10) NOT NULL default ' ',
  "AAABMGXU" varchar(4) NOT NULL default ' ',
  "AAABMDFT" varchar(10) NOT NULL default ' ',
  "AAABMDFU" varchar(4) NOT NULL default '0000',
  "AAABMDFV" varchar(1) NOT NULL default ' ',
  "AAABMDFW" varchar(13) NOT NULL default ' ',
  "AAABMDFX" varchar(4) NOT NULL default ' ',
  "AAABMDFY" varchar(2) NOT NULL default ' ',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABMDGA" varchar(20) NOT NULL default ' ',
  "AAABMDGB" varchar(16) NOT NULL default ' ',
  "AAABMDGC" varchar(18) NOT NULL default ' ',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMPWM" varchar(8) NOT NULL default '00000000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMQCG" varchar(18) NOT NULL default ' ',
  "AAABMDGI" varchar(8) NOT NULL default '00000000',
  "AAABMDGJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMELL","AAABMDGM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDGN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDHG" varchar(10) NOT NULL default ' ',
  "AAABMDGQ" varchar(13) NOT NULL default '0000000000000',
  "AAABMEKI" varchar(4) NOT NULL default ' ',
  "AAABMEEK" varchar(4) NOT NULL default ' ',
  "AAABMEEL" varchar(3) NOT NULL default '000',
  "AAABMEEM" varchar(6) NOT NULL default ' ',
  "AAABMEEN" varchar(4) NOT NULL default ' ',
  "AAABMEEO" varchar(12) NOT NULL default ' ',
  "AAABMEEP" varchar(1) NOT NULL default ' ',
  "AAABMEEQ" varchar(12) NOT NULL default ' ',
  "AAABMEER" varchar(12) NOT NULL default ' ',
  "AAABMEES" varchar(68) NOT NULL default ' ',
  "AAABMEET" varchar(12) NOT NULL default ' ',
  "AAABMEEU" varchar(1) NOT NULL default ' ',
  "AAABMEEV" varchar(1) NOT NULL default ' ',
  "AAABMEEW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDHH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABMEAW" varchar(4) NOT NULL default '0000',
  "AAABMDHM" varchar(4) NOT NULL default '0000',
  "AAABMEBC" varchar(4) NOT NULL default ' ',
  "AAABMDHO" varchar(1) NOT NULL default '0',
  "AAABMDHP" varchar(1) NOT NULL default ' ',
  "AAABMDHQ" varchar(1) NOT NULL default ' ',
  "AAABMDHR" decimal(23,2) NOT NULL default '0.00',
  "AAABMDHS" decimal(23,2) NOT NULL default '0.00',
  "AAABMDHT" decimal(23,2) NOT NULL default '0.00',
  "AAABMDZN" decimal(23,2) NOT NULL default '0.00',
  "AAABMEFM" varchar(8) NOT NULL default '00000000',
  "AAABMDHW" varchar(1) NOT NULL default '0',
  "AAABMDHX" varchar(1) NOT NULL default ' ',
  "AAABMEFN" varchar(8) NOT NULL default '00000000',
  "AAABMDHZ" varchar(1) NOT NULL default '0',
  "AAABMDIA" varchar(1) NOT NULL default ' ',
  "AAABMDZT" varchar(8) NOT NULL default '00000000',
  "AAABMEFR" varchar(1) NOT NULL default '0',
  "AAABMDID" varchar(1) NOT NULL default '0',
  "AAABMDZV" varchar(1) NOT NULL default ' ',
  "AAABMEAA" varchar(8) NOT NULL default '00000000',
  "AAABMEAB" varchar(1) NOT NULL default '0',
  "AAABMEAC" varchar(1) NOT NULL default ' ',
  "AAABMEAP" varchar(1) NOT NULL default '0',
  "AAABMDIJ" varchar(8) NOT NULL default '00000000',
  "AAABMDIK" varchar(8) NOT NULL default '00000000',
  "AAABMDIL" varchar(1) NOT NULL default '0',
  "AAABMDIM" varchar(1) NOT NULL default ' ',
  "AAABMPTJ" int(11) NOT NULL default '0',
  "AAABMPTI" varchar(6) NOT NULL default '000000',
  "AAABMDIP" varchar(6) NOT NULL default '000000',
  "AAABMDZP" varchar(1) NOT NULL default '0',
  "AAABMPTH" varchar(1) NOT NULL default '0',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAABMDZE" decimal(10,7) NOT NULL default '0.0000000',
  "AAABMDZB" varchar(8) NOT NULL default '00000000',
  "AAABMEAI" varchar(8) NOT NULL default '00000000',
  "AAABMDIW" varchar(8) NOT NULL default '00000000',
  "AAABMDIX" varchar(1) NOT NULL default '0',
  "AAABMDIY" varchar(1) NOT NULL default ' ',
  "AAABMDIZ" varchar(8) NOT NULL default '00000000',
  "AAABMDJA" varchar(1) NOT NULL default '0',
  "AAABMDJB" varchar(1) NOT NULL default ' ',
  "AAABMEFI" varchar(5) NOT NULL default ' ',
  "AAABMDZH" decimal(17,2) NOT NULL default '0.00',
  "AAABMDZI" decimal(15,6) NOT NULL default '0.000000',
  "AAABMDZJ" decimal(23,2) NOT NULL default '0.00',
  "AAABMDZK" decimal(23,2) NOT NULL default '0.00',
  "AAABMDZL" varchar(1) NOT NULL default '0',
  "AAABMDZM" varchar(4) NOT NULL default ' ',
  "AAABMEAL" varchar(1) NOT NULL default ' ',
  "AAABMDJK" decimal(23,2) NOT NULL default '0.00',
  "AAABMDJL" decimal(23,2) NOT NULL default '0.00',
  "AAABMDZR" varchar(3) NOT NULL default '000',
  "AAABMDZS" varchar(3) NOT NULL default '000',
  "AAABMDYV" varchar(1) NOT NULL default ' ',
  "AAABMDJP" varchar(2) NOT NULL default '00',
  "AAABMEAQ" varchar(1) NOT NULL default ' ',
  "AAABMEAR" varchar(1) NOT NULL default '0',
  "AAABMEAM" varchar(1) NOT NULL default '0',
  "AAABMEAN" varchar(1) NOT NULL default '0',
  "AAABMDJU" varchar(1) NOT NULL default ' ',
  "AAABMOBZ" varchar(4) NOT NULL default '0000',
  "AAABMEAS" int(11) NOT NULL default '0',
  "AAABMDJX" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMEFT","AAABMEAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDKC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMPRB" varchar(1) NOT NULL default ' ',
  "AAABMPRD" varchar(8) NOT NULL default ' ',
  "AAABMPRE" varchar(6) NOT NULL default '000000',
  "AAABMPRC" varchar(4) NOT NULL default '0000',
  "AAABMPRY" varchar(5) NOT NULL default ' ',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMPRX" decimal(16,2) NOT NULL default '0.00',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMPPC","AAABMPPD","AAABMPPE","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMPRB","AAABMPRD","AAABMPRE","AAABMPRC","AAABMPRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDLI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMDRY" varchar(30) NOT NULL default ' ',
  "AAABMPUM" varchar(1) NOT NULL default ' ',
  "AAABMDLN" varchar(1) NOT NULL default ' ',
  "AAABMDLO" varchar(8) NOT NULL default '00000000',
  "AAABMDLP" varchar(8) NOT NULL default '00000000',
  "AAABMDLQ" varchar(8) NOT NULL default '00000000',
  "AAABMDLR" varchar(8) NOT NULL default '00000000',
  "AAABMDLS" varchar(8) NOT NULL default '00000000',
  "AAABMIMM" varchar(12) NOT NULL default ' ',
  "AAABMDLU" varchar(8) NOT NULL default '00000000',
  "AAABMDLV" varchar(6) NOT NULL default '000000',
  "AAABMDLW" varchar(8) NOT NULL default '00000000',
  "AAABMDLX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMPQE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDMA" (
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
CREATE TABLE "AAABMDNA" (
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
  KEY "AAABMDRS" ("AAABMPQE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDRU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMPUM" varchar(1) NOT NULL default ' ',
  "AAABMDRY" varchar(30) NOT NULL default ' ',
  "AAABMDRZ" varchar(1) NOT NULL default ' ',
  "AAABMDSA" varchar(12) NOT NULL default ' ',
  "AAABMDSB" varchar(1) NOT NULL default ' ',
  "AAABMDSC" varchar(1) NOT NULL default ' ',
  "AAABMDSD" varchar(14) NOT NULL default ' ',
  "AAABMDSE" varchar(1) NOT NULL default ' ',
  "AAABMDSF" varchar(8) NOT NULL default '00000000',
  "AAABMDSG" varchar(8) NOT NULL default '00000000',
  "AAABMDSH" varchar(1) NOT NULL default ' ',
  "AAABMIMM" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDSL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  "AAABMGSI" varchar(8) NOT NULL default '00000000',
  "AAABMPUD" varchar(16) NOT NULL default '0000000000000000',
  PRIMARY KEY ("AAABMRIL","AAABMRCU","AAABMRHL","AAABMPPC","AAABMPPD","AAABMPPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDTC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMEKC" varchar(4) NOT NULL default ' ',
  "AAABMELV" varchar(10) NOT NULL default ' ',
  "AAABMEJX" varchar(4) NOT NULL default '0000',
  "AAABMODF" varchar(30) NOT NULL default ' ',
  "AAABMEJR" varchar(30) NOT NULL default ' ',
  "AAABMEJS" varchar(8) NOT NULL default ' ',
  "AAABMEJT" varchar(6) NOT NULL default ' ',
  "AAABMEJU" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMEKC","AAABMELV","AAABMEJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDTQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMEBB" varchar(4) NOT NULL default ' ',
  "AAABMEBC" varchar(4) NOT NULL default ' ',
  "AAABMDTU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMEBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDTX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMODF" varchar(30) NOT NULL default ' ',
  "AAABMEDK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMODF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDUF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMODF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMODF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDUK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMODF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMODF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDUP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMODF" varchar(30) NOT NULL default ' ',
  "AAABMEDK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMODF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDUX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMODF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMODF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDVC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMODF" varchar(30) NOT NULL default ' ',
  "AAABMEDK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMODF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDVK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMODF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMODF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDVP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMODF" varchar(30) NOT NULL default ' ',
  "AAABMEDK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMODF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDVX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMEFZ" varchar(16) NOT NULL default ' ',
  "AAABMEIW" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMEFZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDWF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMEFZ" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMEFZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDWK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMEKC" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMEKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDWS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMDXP" varchar(6) NOT NULL default '000000',
  "AAABMDWW" varchar(6) NOT NULL default '000000',
  "AAABMLPV" varchar(4) NOT NULL default '0000',
  "AAABMDXM" varchar(4) NOT NULL default '0000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMDXC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMDXP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDXG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMDXP" varchar(6) NOT NULL default '000000',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABMDXM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMDXP","AAABMNJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDXR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMORS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMORS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDXW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMEKI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMEKI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDYB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDYH" varchar(1) NOT NULL default ' ',
  "AAABMORS" varchar(30) NOT NULL default ' ',
  "AAABMEIW" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDYH","AAABMORS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMDYJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABMEAW" varchar(4) NOT NULL default '0000',
  "AAABMOBZ" varchar(4) NOT NULL default '0000',
  "AAABMDYP" varchar(8) NOT NULL default '00000000',
  "AAABMDYQ" varchar(1) NOT NULL default '0',
  "AAABMDYR" varchar(1) NOT NULL default ' ',
  "AAABMDYS" varchar(2) NOT NULL default '00',
  "AAABMEBC" varchar(4) NOT NULL default ' ',
  "AAABMDYU" varchar(1) NOT NULL default ' ',
  "AAABMDYV" varchar(1) NOT NULL default ' ',
  "AAABMDYW" varchar(4) NOT NULL default '0000',
  "AAABMEBA" varchar(4) NOT NULL default '0000',
  "AAABMDYY" varchar(10) NOT NULL default ' ',
  "AAABMOCB" varchar(4) NOT NULL default ' ',
  "AAABMOCC" varchar(4) NOT NULL default ' ',
  "AAABMDZB" varchar(8) NOT NULL default '00000000',
  "AAABMDZC" decimal(10,7) NOT NULL default '0.0000000',
  "AAABMDZD" decimal(10,7) NOT NULL default '0.0000000',
  "AAABMDZE" decimal(10,7) NOT NULL default '0.0000000',
  "AAABMDZF" varchar(6) NOT NULL default '000000',
  "AAABMEFI" varchar(5) NOT NULL default ' ',
  "AAABMDZH" decimal(17,2) NOT NULL default '0.00',
  "AAABMDZI" decimal(15,6) NOT NULL default '0.000000',
  "AAABMDZJ" decimal(23,2) NOT NULL default '0.00',
  "AAABMDZK" decimal(23,2) NOT NULL default '0.00',
  "AAABMDZL" varchar(1) NOT NULL default '0',
  "AAABMDZM" varchar(4) NOT NULL default ' ',
  "AAABMDZN" decimal(23,2) NOT NULL default '0.00',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAABMDZP" varchar(1) NOT NULL default '0',
  "AAABMPTH" varchar(1) NOT NULL default '0',
  "AAABMDZR" varchar(3) NOT NULL default '000',
  "AAABMDZS" varchar(3) NOT NULL default '000',
  "AAABMDZT" varchar(8) NOT NULL default '00000000',
  "AAABMEFR" varchar(1) NOT NULL default '0',
  "AAABMDZV" varchar(1) NOT NULL default ' ',
  "AAABMDZW" varchar(1) NOT NULL default '0',
  "AAABMDZX" int(11) NOT NULL default '0',
  "AAABMDZY" varchar(1) NOT NULL default '0',
  "AAABMDZZ" varchar(1) NOT NULL default ' ',
  "AAABMEAA" varchar(8) NOT NULL default '00000000',
  "AAABMEAB" varchar(1) NOT NULL default '0',
  "AAABMEAC" varchar(1) NOT NULL default ' ',
  "AAABMEAD" varchar(1) NOT NULL default '0',
  "AAABMEAE" int(11) NOT NULL default '0',
  "AAABMEAF" varchar(1) NOT NULL default '0',
  "AAABMEAG" varchar(1) NOT NULL default '0',
  "AAABMEAH" varchar(1) NOT NULL default '0',
  "AAABMEAI" varchar(8) NOT NULL default '00000000',
  "AAABMEFL" varchar(2) NOT NULL default ' ',
  "AAABMEAK" varchar(2) NOT NULL default ' ',
  "AAABMEAL" varchar(1) NOT NULL default ' ',
  "AAABMEAM" varchar(1) NOT NULL default '0',
  "AAABMEAN" varchar(1) NOT NULL default '0',
  "AAABMEAO" varchar(1) NOT NULL default ' ',
  "AAABMEAP" varchar(1) NOT NULL default '0',
  "AAABMEAQ" varchar(1) NOT NULL default ' ',
  "AAABMEAR" varchar(1) NOT NULL default '0',
  "AAABMEAS" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMEFT","AAABMEAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEAX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOBZ" varchar(4) NOT NULL default '0000',
  "AAABMEBA" varchar(4) NOT NULL default '0000',
  "AAABMEBB" varchar(4) NOT NULL default ' ',
  "AAABMEBC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOBZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEBF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOBZ" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMEBJ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOBZ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEBN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMEKI" varchar(4) NOT NULL default ' ',
  "AAABMECK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMEKI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEBV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMELN" varchar(7) NOT NULL default ' ',
  "AAABMEKY" varchar(1) NOT NULL default ' ',
  "AAABMEFX" varchar(40) NOT NULL default ' ',
  "AAABMEFY" varchar(30) NOT NULL default ' ',
  "AAABMEFZ" varchar(16) NOT NULL default ' ',
  "AAABMORS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMELN","AAABMEKY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMECG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMELN" varchar(7) NOT NULL default ' ',
  "AAABMECK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMELN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMECO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMELN" varchar(7) NOT NULL default ' ',
  "AAABMEGF" varchar(2) NOT NULL default ' ',
  "AAABMEGG" varchar(2) NOT NULL default ' ',
  "AAABMEGH" varchar(1) NOT NULL default ' ',
  "AAABMEGI" varchar(1) NOT NULL default ' ',
  "AAABMEGJ" varchar(1) NOT NULL default ' ',
  "AAABMEKI" varchar(4) NOT NULL default ' ',
  "AAABMEKC" varchar(4) NOT NULL default ' ',
  "AAABMLFE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMELN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEDB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMEFX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMEFX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEDG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMEFX" varchar(40) NOT NULL default ' ',
  "AAABMEDK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMEFX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEDO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMEKI" varchar(4) NOT NULL default ' ',
  "AAABMEEK" varchar(4) NOT NULL default ' ',
  "AAABMEEL" varchar(3) NOT NULL default '000',
  "AAABMEEM" varchar(6) NOT NULL default ' ',
  "AAABMEEN" varchar(4) NOT NULL default ' ',
  "AAABMEEO" varchar(12) NOT NULL default ' ',
  "AAABMEEP" varchar(1) NOT NULL default ' ',
  "AAABMEEQ" varchar(12) NOT NULL default ' ',
  "AAABMEER" varchar(12) NOT NULL default ' ',
  "AAABMEES" varchar(68) NOT NULL default ' ',
  "AAABMEET" varchar(12) NOT NULL default ' ',
  "AAABMEEU" varchar(1) NOT NULL default ' ',
  "AAABMEEV" varchar(1) NOT NULL default ' ',
  "AAABMEEW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMEKI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEEG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMEKI" varchar(4) NOT NULL default ' ',
  "AAABMEEZ" varchar(13) NOT NULL default '0000000000000',
  "AAABMEEK" varchar(4) NOT NULL default ' ',
  "AAABMEEL" varchar(3) NOT NULL default '000',
  "AAABMEEM" varchar(6) NOT NULL default ' ',
  "AAABMEEN" varchar(4) NOT NULL default ' ',
  "AAABMEEO" varchar(12) NOT NULL default ' ',
  "AAABMEEP" varchar(1) NOT NULL default ' ',
  "AAABMEEQ" varchar(12) NOT NULL default ' ',
  "AAABMEER" varchar(12) NOT NULL default ' ',
  "AAABMEES" varchar(68) NOT NULL default ' ',
  "AAABMEET" varchar(12) NOT NULL default ' ',
  "AAABMEEU" varchar(1) NOT NULL default ' ',
  "AAABMEEV" varchar(1) NOT NULL default ' ',
  "AAABMEEW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMEKI","AAABMEEZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEFA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEFF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABMEFI" varchar(5) NOT NULL default ' ',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAABMEFK" varchar(1) NOT NULL default '0',
  "AAABMEFL" varchar(2) NOT NULL default ' ',
  "AAABMEFM" varchar(8) NOT NULL default '00000000',
  "AAABMEFN" varchar(8) NOT NULL default '00000000',
  "AAABMEFO" varchar(8) NOT NULL default '00000000',
  "AAABMEFP" varchar(8) NOT NULL default '00000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABMEFR" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMEFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEFU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMELL" varchar(10) NOT NULL default '0000000000',
  "AAABMEFX" varchar(40) NOT NULL default ' ',
  "AAABMEFY" varchar(30) NOT NULL default ' ',
  "AAABMEFZ" varchar(16) NOT NULL default ' ',
  "AAABMORS" varchar(30) NOT NULL default ' ',
  "AAABMEGB" varchar(8) NOT NULL default '00000000',
  "AAABMEGC" varchar(6) NOT NULL default '000000',
  "AAABMEGD" varchar(12) NOT NULL default ' ',
  "AAABMEGE" varchar(1) NOT NULL default ' ',
  "AAABMEGF" varchar(2) NOT NULL default ' ',
  "AAABMEGG" varchar(2) NOT NULL default ' ',
  "AAABMEGH" varchar(1) NOT NULL default ' ',
  "AAABMEGI" varchar(1) NOT NULL default ' ',
  "AAABMEGJ" varchar(1) NOT NULL default ' ',
  "AAABMEKI" varchar(4) NOT NULL default ' ',
  "AAABMEKC" varchar(4) NOT NULL default ' ',
  "AAABMLFE" varchar(1) NOT NULL default ' ',
  "AAABMPLP" varchar(13) NOT NULL default '0000000000000',
  "AAABMEKW" varchar(72) NOT NULL default ' ',
  "AAABMELN" varchar(7) NOT NULL default ' ',
  "AAABMEKY" varchar(1) NOT NULL default ' ',
  "AAABMEKZ" varchar(1) NOT NULL default ' ',
  "AAABMELA" varchar(1) NOT NULL default ' ',
  "AAABMELB" varchar(2) NOT NULL default ' ',
  "AAABMELC" varchar(10) NOT NULL default ' ',
  "AAABMELD" varchar(4) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMEGX" varchar(30) NOT NULL default ' ',
  "AAABMEGY" varchar(8) NOT NULL default '00000000',
  "AAABMEGZ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMELL"),
  KEY "AAABMEHC" ("AAABMPLP","AAABMELN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEHF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMEIW" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEHN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMGHG" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMGHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEHS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMGHG" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOHD" varchar(30) NOT NULL default ' ',
  "AAABMIUM" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMGHG","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEIB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMELV" varchar(10) NOT NULL default ' ',
  "AAABMEIE" varchar(1) NOT NULL default ' ',
  "AAABMEIF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMELV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEII" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMEKC" varchar(4) NOT NULL default ' ',
  "AAABMOAI" varchar(4) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABMELV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMEKC","AAABMOAI","AAABMPSC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEIS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMELV" varchar(10) NOT NULL default ' ',
  "AAABMEIW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMELV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEJA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMEKC" varchar(4) NOT NULL default ' ',
  "AAABMEJX" varchar(4) NOT NULL default '0000',
  "AAABMODF" varchar(30) NOT NULL default ' ',
  "AAABMEJR" varchar(30) NOT NULL default ' ',
  "AAABMEJS" varchar(8) NOT NULL default ' ',
  "AAABMEJT" varchar(6) NOT NULL default ' ',
  "AAABMEJU" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMEKC","AAABMEJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEJM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMEKC" varchar(4) NOT NULL default ' ',
  "AAABMEJX" varchar(4) NOT NULL default '0000',
  "AAABMODF" varchar(30) NOT NULL default ' ',
  "AAABMEJR" varchar(30) NOT NULL default ' ',
  "AAABMEJS" varchar(8) NOT NULL default ' ',
  "AAABMEJT" varchar(6) NOT NULL default ' ',
  "AAABMEJU" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMEKC","AAABMEJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEJY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMEKC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMEKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEKD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMEKI" varchar(4) NOT NULL default ' ',
  "AAABMORS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMEKI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEKJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMELV" varchar(10) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMELV","AAABMEKQ","AAABMQIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEKS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMELL" varchar(10) NOT NULL default '0000000000',
  "AAABMPLP" varchar(13) NOT NULL default '0000000000000',
  "AAABMEKW" varchar(72) NOT NULL default ' ',
  "AAABMELN" varchar(7) NOT NULL default ' ',
  "AAABMEKY" varchar(1) NOT NULL default ' ',
  "AAABMEKZ" varchar(1) NOT NULL default ' ',
  "AAABMELA" varchar(1) NOT NULL default ' ',
  "AAABMELB" varchar(2) NOT NULL default ' ',
  "AAABMELC" varchar(10) NOT NULL default ' ',
  "AAABMELD" varchar(4) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMRFO" varchar(4) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMELL"),
  KEY "AAABMELK" ("AAABMELL","AAABMPLP","AAABMELN","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMELP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMELV" varchar(10) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMELV","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMELY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAABMEMG" decimal(15,5) NOT NULL default '0.00000',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEMQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAABMQWT" varchar(18) NOT NULL default ' ',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMENI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMHYX" varchar(6) NOT NULL default ' ',
  "AAABMHYY" varchar(1) NOT NULL default ' ',
  "AAABMHYZ" varchar(4) NOT NULL default ' ',
  "AAABMHZA" varchar(4) NOT NULL default '0000',
  "AAABMHZB" varchar(6) NOT NULL default '000000',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEOD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMEOK" varchar(4) NOT NULL default ' ',
  "AAABMEOL" varchar(6) NOT NULL default '000000',
  "AAABMEOM" varchar(4) NOT NULL default '0000',
  "AAABMHYZ" varchar(4) NOT NULL default ' ',
  "AAABMHZB" varchar(6) NOT NULL default '000000',
  "AAABMHZA" varchar(4) NOT NULL default '0000',
  "AAABMEPN" varchar(4) NOT NULL default ' ',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  "AAABMEOU" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEPB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMHYZ" varchar(4) NOT NULL default ' ',
  "AAABMHZB" varchar(6) NOT NULL default '000000',
  "AAABMHZA" varchar(4) NOT NULL default '0000',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAABMEPM" decimal(15,5) NOT NULL default '0.00000',
  "AAABMEPN" varchar(4) NOT NULL default ' ',
  "AAABMMPH" decimal(16,2) NOT NULL default '0.00',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  "AAABMEPS" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEPZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMRCJ" varchar(8) NOT NULL default '00000000',
  "AAABMRCQ" varchar(8) NOT NULL default '00000000',
  "AAABMGGO" varchar(1) NOT NULL default ' ',
  "AAABMGGP" varchar(4) NOT NULL default '0000',
  "AAABMGGV" varchar(1) NOT NULL default ' ',
  "AAABMPOL" varchar(12) NOT NULL default ' ',
  "AAABMPOM" varchar(8) NOT NULL default '00000000',
  "AAABMPON" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD"),
  KEY "AAABMEQU" ("AAABMPOM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEQW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMRCJ" varchar(8) NOT NULL default '00000000',
  "AAABMRCQ" varchar(8) NOT NULL default '00000000',
  "AAABMERF" decimal(4,0) NOT NULL default '0',
  "AAABMERG" decimal(4,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD","AAABMRCJ"),
  KEY "AAABMERO" ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMRCJ","AAABMRCQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMERW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMIEF" varchar(4) NOT NULL default ' ',
  "AAABMIEG" varchar(6) NOT NULL default '000000',
  "AAABMIEH" varchar(4) NOT NULL default '0000',
  "AAABMESG" varchar(4) NOT NULL default ' ',
  "AAABMESH" varchar(6) NOT NULL default '000000',
  "AAABMESI" varchar(4) NOT NULL default '0000',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  "AAABMESM" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEST" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMMRH" varchar(1) NOT NULL default '0',
  "AAABMMRE" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD","AAABMMRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMETJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMITI" varchar(60) NOT NULL default ' ',
  "AAABMITJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEUA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMHOC" varchar(4) NOT NULL default '0000',
  "AAABMEUI" varchar(4) NOT NULL default ' ',
  "AAABMEUJ" varchar(6) NOT NULL default '000000',
  "AAABMEUK" varchar(4) NOT NULL default '0000',
  "AAABMMOQ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEUS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMHPX" varchar(4) NOT NULL default '0000',
  "AAABMHPW" varchar(4) NOT NULL default '0000',
  "AAABMHOZ" varchar(5) NOT NULL default ' ',
  "AAABMHPA" varchar(5) NOT NULL default ' ',
  "AAABMHPB" varchar(4) NOT NULL default ' ',
  "AAABMHPC" varchar(6) NOT NULL default '000000',
  "AAABMHPD" varchar(4) NOT NULL default '0000',
  "AAABMHPE" varchar(4) NOT NULL default ' ',
  "AAABMHPF" varchar(6) NOT NULL default '000000',
  "AAABMHPG" varchar(4) NOT NULL default '0000',
  "AAABMHPH" varchar(4) NOT NULL default '0000',
  "AAABMHPI" varchar(4) NOT NULL default ' ',
  "AAABMHPJ" varchar(6) NOT NULL default '000000',
  "AAABMHPK" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD","AAABMHPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMQPJ" varchar(4) NOT NULL default '0000',
  "AAABMHRB" varchar(4) NOT NULL default '0000',
  "AAABMHQH" varchar(5) NOT NULL default ' ',
  "AAABMHQI" varchar(5) NOT NULL default ' ',
  "AAABMHQJ" varchar(4) NOT NULL default ' ',
  "AAABMHQK" varchar(6) NOT NULL default '000000',
  "AAABMHQL" varchar(4) NOT NULL default '0000',
  "AAABMHQM" varchar(4) NOT NULL default ' ',
  "AAABMHQN" varchar(6) NOT NULL default '000000',
  "AAABMHQO" varchar(4) NOT NULL default '0000',
  "AAABMHQP" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD","AAABMQPJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEWT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMHSL" varchar(4) NOT NULL default '0000',
  "AAABMHSK" varchar(4) NOT NULL default '0000',
  "AAABMHRM" varchar(5) NOT NULL default ' ',
  "AAABMHRN" varchar(5) NOT NULL default ' ',
  "AAABMHRO" varchar(4) NOT NULL default '0000',
  "AAABMHRP" varchar(4) NOT NULL default ' ',
  "AAABMHRQ" varchar(6) NOT NULL default '000000',
  "AAABMHRR" varchar(4) NOT NULL default '0000',
  "AAABMHRS" varchar(4) NOT NULL default ' ',
  "AAABMHRT" varchar(6) NOT NULL default '000000',
  "AAABMHRU" varchar(4) NOT NULL default '0000',
  "AAABMHRV" varchar(4) NOT NULL default '0000',
  "AAABMHRW" varchar(4) NOT NULL default ' ',
  "AAABMHRX" varchar(6) NOT NULL default '000000',
  "AAABMHRY" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD","AAABMHSL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEXW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMHSV" varchar(4) NOT NULL default ' ',
  "AAABMHSW" varchar(6) NOT NULL default '000000',
  "AAABMHSX" varchar(4) NOT NULL default '0000',
  "AAABMHSY" varchar(4) NOT NULL default '0000',
  "AAABMHSZ" varchar(4) NOT NULL default ' ',
  "AAABMHTA" varchar(6) NOT NULL default '000000',
  "AAABMHTB" varchar(4) NOT NULL default '0000',
  "AAABMHTC" varchar(4) NOT NULL default '0000',
  "AAABMHTD" varchar(4) NOT NULL default ' ',
  "AAABMHTE" varchar(6) NOT NULL default '000000',
  "AAABMHTF" varchar(4) NOT NULL default '0000',
  "AAABMHTG" varchar(4) NOT NULL default '0000',
  "AAABMHTH" varchar(4) NOT NULL default '0000',
  "AAABMHTI" varchar(4) NOT NULL default '0000',
  "AAABMHTJ" varchar(4) NOT NULL default '0000',
  "AAABMHTK" varchar(4) NOT NULL default '0000',
  "AAABMHTL" varchar(4) NOT NULL default '0000',
  "AAABMHTM" varchar(4) NOT NULL default '0000',
  "AAABMHTN" varchar(4) NOT NULL default '0000',
  "AAABMHTU" varchar(4) NOT NULL default '0000',
  "AAABMHTO" varchar(4) NOT NULL default ' ',
  "AAABMHTP" varchar(6) NOT NULL default '000000',
  "AAABMHTQ" varchar(4) NOT NULL default '0000',
  "AAABMHTR" varchar(4) NOT NULL default ' ',
  "AAABMHTS" varchar(6) NOT NULL default '000000',
  "AAABMHTT" varchar(4) NOT NULL default '0000',
  "AAABMHTV" varchar(4) NOT NULL default '0000',
  "AAABMHTW" varchar(4) NOT NULL default '0000',
  "AAABMHTX" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMEZM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMEZT" varchar(4) NOT NULL default ' ',
  "AAABMEZU" varchar(6) NOT NULL default '000000',
  "AAABMEZV" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMFAC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMOMK" varchar(4) NOT NULL default '0000',
  "AAABMHUQ" varchar(4) NOT NULL default ' ',
  "AAABMHUR" varchar(6) NOT NULL default '000000',
  "AAABMHUS" varchar(4) NOT NULL default '0000',
  "AAABMHUT" varchar(4) NOT NULL default '0000',
  "AAABMHUU" varchar(4) NOT NULL default ' ',
  "AAABMHUV" varchar(6) NOT NULL default '000000',
  "AAABMHUW" varchar(4) NOT NULL default '0000',
  "AAABMHUX" varchar(4) NOT NULL default '0000',
  "AAABMHUY" varchar(4) NOT NULL default ' ',
  "AAABMHUZ" varchar(6) NOT NULL default '000000',
  "AAABMHVA" varchar(4) NOT NULL default '0000',
  "AAABMHVB" varchar(4) NOT NULL default '0000',
  "AAABMHVC" varchar(4) NOT NULL default '0000',
  "AAABMHVD" varchar(4) NOT NULL default '0000',
  "AAABMHVE" varchar(4) NOT NULL default '0000',
  "AAABMHVF" varchar(4) NOT NULL default '0000',
  "AAABMHVG" varchar(4) NOT NULL default '0000',
  "AAABMHVH" varchar(4) NOT NULL default '0000',
  "AAABMHVI" varchar(4) NOT NULL default '0000',
  "AAABMHVM" varchar(4) NOT NULL default '0000',
  "AAABMHVJ" varchar(4) NOT NULL default ' ',
  "AAABMHVK" varchar(6) NOT NULL default '000000',
  "AAABMHVL" varchar(4) NOT NULL default '0000',
  "AAABMHVN" varchar(4) NOT NULL default '0000',
  "AAABMHVO" varchar(4) NOT NULL default '0000',
  "AAABMHVP" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMFBQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMQYF" varchar(8) NOT NULL default '00000000',
  "AAABMFBY" varchar(1) NOT NULL default ' ',
  "AAABMQYH" varchar(4) NOT NULL default ' ',
  "AAABMQYI" varchar(4) NOT NULL default '0000',
  "AAABMFCB" varchar(1) NOT NULL default ' ',
  "AAABMFCC" varchar(1) NOT NULL default ' ',
  "AAABMFCD" varchar(1) NOT NULL default ' ',
  "AAABMFCE" varchar(1) NOT NULL default ' ',
  "AAABMQYO" varchar(1) NOT NULL default ' ',
  "AAABMFCG" varchar(1) NOT NULL default ' ',
  "AAABMFCH" varchar(1) NOT NULL default ' ',
  "AAABMFCI" varchar(1) NOT NULL default ' ',
  "AAABMFCJ" varchar(1) NOT NULL default ' ',
  "AAABMFCK" varchar(1) NOT NULL default ' ',
  "AAABMFCL" varchar(1) NOT NULL default ' ',
  "AAABMFCM" varchar(1) NOT NULL default ' ',
  "AAABMFCN" varchar(1) NOT NULL default ' ',
  "AAABMFCO" varchar(1) NOT NULL default ' ',
  "AAABMKJV" varchar(5) NOT NULL default ' ',
  "AAABMKJW" varchar(5) NOT NULL default ' ',
  "AAABMHKE" varchar(4) NOT NULL default '0000',
  "AAABMHKF" varchar(4) NOT NULL default '0000',
  "AAABMHKG" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMFDA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMHXV" varchar(4) NOT NULL default '0000',
  "AAABMHXU" varchar(4) NOT NULL default '0000',
  "AAABMHWH" varchar(4) NOT NULL default ' ',
  "AAABMHWI" varchar(6) NOT NULL default '000000',
  "AAABMHWJ" varchar(4) NOT NULL default '0000',
  "AAABMGSO" varchar(1) NOT NULL default ' ',
  "AAABMFDN" varchar(4) NOT NULL default ' ',
  "AAABMFDO" varchar(6) NOT NULL default '000000',
  "AAABMFDP" varchar(4) NOT NULL default '0000',
  "AAABMHWL" varchar(4) NOT NULL default ' ',
  "AAABMHWM" varchar(6) NOT NULL default '000000',
  "AAABMHWN" varchar(4) NOT NULL default '0000',
  "AAABMHWO" varchar(4) NOT NULL default '0000',
  "AAABMHWP" varchar(4) NOT NULL default ' ',
  "AAABMHWQ" varchar(6) NOT NULL default '000000',
  "AAABMHWR" varchar(4) NOT NULL default '0000',
  "AAABMHWS" varchar(4) NOT NULL default '0000',
  "AAABMHWT" varchar(4) NOT NULL default ' ',
  "AAABMHWU" varchar(6) NOT NULL default '000000',
  "AAABMHWV" varchar(4) NOT NULL default '0000',
  "AAABMHWW" varchar(4) NOT NULL default '0000',
  "AAABMHWX" varchar(4) NOT NULL default '0000',
  "AAABMHWY" varchar(4) NOT NULL default '0000',
  "AAABMHWZ" varchar(4) NOT NULL default '0000',
  "AAABMHXA" varchar(4) NOT NULL default '0000',
  "AAABMHXB" varchar(4) NOT NULL default '0000',
  "AAABMHXC" varchar(4) NOT NULL default '0000',
  "AAABMHXD" varchar(4) NOT NULL default '0000',
  "AAABMFEJ" varchar(4) NOT NULL default ' ',
  "AAABMFEK" varchar(6) NOT NULL default '000000',
  "AAABMFEL" varchar(4) NOT NULL default '0000',
  "AAABMFEM" varchar(4) NOT NULL default ' ',
  "AAABMFEN" varchar(6) NOT NULL default '000000',
  "AAABMFEO" varchar(4) NOT NULL default '0000',
  "AAABMHXK" varchar(4) NOT NULL default '0000',
  "AAABMHXL" varchar(4) NOT NULL default '0000',
  "AAABMHXM" varchar(4) NOT NULL default '0000',
  "AAABMHXN" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD","AAABMHXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMFFA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMKXP" varchar(4) NOT NULL default '0000',
  "AAABMMJT" varchar(32) NOT NULL default ' ',
  "AAABMKXF" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD","AAABMKXP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMFFR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMMRH" varchar(1) NOT NULL default '0',
  "AAABMMRE" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD","AAABMMRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMFGH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMFGO" varchar(4) NOT NULL default ' ',
  "AAABMFGP" varchar(6) NOT NULL default '000000',
  "AAABMFGQ" varchar(4) NOT NULL default '0000',
  "AAABMFGR" varchar(4) NOT NULL default ' ',
  "AAABMFGS" varchar(6) NOT NULL default '000000',
  "AAABMFGT" varchar(4) NOT NULL default '0000',
  "AAABMFGU" varchar(4) NOT NULL default ' ',
  "AAABMFGV" varchar(6) NOT NULL default '000000',
  "AAABMFGW" varchar(4) NOT NULL default '0000',
  "AAABMFGX" varchar(4) NOT NULL default ' ',
  "AAABMFGY" varchar(6) NOT NULL default '000000',
  "AAABMFGZ" varchar(4) NOT NULL default '0000',
  "AAABMFHA" varchar(4) NOT NULL default ' ',
  "AAABMFHB" varchar(6) NOT NULL default '000000',
  "AAABMFHC" varchar(4) NOT NULL default '0000',
  "AAABMFHD" varchar(4) NOT NULL default ' ',
  "AAABMFHE" varchar(6) NOT NULL default '000000',
  "AAABMFHF" varchar(4) NOT NULL default '0000',
  "AAABMKYU" varchar(60) NOT NULL default ' ',
  "AAABMKYV" varchar(30) NOT NULL default ' ',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  "AAABMFHL" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMFHS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMKYU" varchar(60) NOT NULL default ' ',
  "AAABMKYV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMFIJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMKYH" varchar(4) NOT NULL default '0000',
  "AAABMKXT" varchar(4) NOT NULL default ' ',
  "AAABMKXU" varchar(6) NOT NULL default '000000',
  "AAABMLRB" varchar(4) NOT NULL default '0000',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABMKXX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD","AAABMKYH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMFJD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMFJK" varchar(8) NOT NULL default '00000000',
  "AAABMFJL" varchar(8) NOT NULL default '00000000',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMFJV" (
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
  PRIMARY KEY ("AAABMRIL","AAABMPUP","AAABMPUQ","AAABMPQE","AAABMRHL","AAABMFOO"),
  KEY "AAABMFOP" ("AAABMRHL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMFOR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMFOY" varchar(4) NOT NULL default ' ',
  "AAABMFOZ" varchar(6) NOT NULL default '000000',
  "AAABMFPA" varchar(4) NOT NULL default '0000',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  "AAABMFPE" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMFPL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMHYZ" varchar(4) NOT NULL default ' ',
  "AAABMHZB" varchar(6) NOT NULL default '000000',
  "AAABMHZA" varchar(4) NOT NULL default '0000',
  "AAABMQTO" varchar(13) NOT NULL default ' ',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMFQF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMQRG" varchar(10) NOT NULL default ' ',
  "AAABMFYN" varchar(4) NOT NULL default ' ',
  "AAABMFYO" varchar(6) NOT NULL default '000000',
  "AAABMFYP" varchar(4) NOT NULL default '0000',
  "AAABMFYQ" varchar(4) NOT NULL default ' ',
  "AAABMFYR" varchar(6) NOT NULL default '000000',
  "AAABMFYS" varchar(4) NOT NULL default '0000',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  "AAABMFQW" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMFRD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMHYZ" varchar(4) NOT NULL default ' ',
  "AAABMHZB" varchar(6) NOT NULL default '000000',
  "AAABMHZA" varchar(4) NOT NULL default '0000',
  "AAABMFRN" decimal(16,2) NOT NULL default '0.00',
  "AAABMHHJ" decimal(16,2) NOT NULL default '0.00',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  "AAABMFRS" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMFRZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMGGS" varchar(10) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
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
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMOGK","AAABMGGS","AAABMNJK","AAABMQPU","AAABMFSV","AAABMMCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMFSX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMFTE" decimal(7,3) NOT NULL default '0.000',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  "AAABMFTI" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMFTP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAABMQWT" varchar(18) NOT NULL default ' ',
  "AAABMQOZ" decimal(7,3) NOT NULL default '0.000',
  "AAABMQPA" varchar(1) NOT NULL default ' ',
  "AAABMQPB" varchar(1) NOT NULL default ' ',
  "AAABMQPC" varchar(1) NOT NULL default ' ',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMFUL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMFUS" varchar(4) NOT NULL default ' ',
  "AAABMFUT" varchar(6) NOT NULL default '000000',
  "AAABMFUU" varchar(4) NOT NULL default '0000',
  "AAABMFUV" varchar(4) NOT NULL default ' ',
  "AAABMFUW" varchar(6) NOT NULL default '000000',
  "AAABMFUX" varchar(4) NOT NULL default '0000',
  "AAABMFUY" varchar(4) NOT NULL default ' ',
  "AAABMFUZ" varchar(6) NOT NULL default '000000',
  "AAABMFVA" varchar(4) NOT NULL default '0000',
  "AAABMFVB" varchar(4) NOT NULL default ' ',
  "AAABMFVC" varchar(6) NOT NULL default '000000',
  "AAABMFVD" varchar(4) NOT NULL default '0000',
  "AAABMFVE" varchar(4) NOT NULL default ' ',
  "AAABMFVF" varchar(6) NOT NULL default '000000',
  "AAABMFVG" varchar(4) NOT NULL default '0000',
  "AAABMFVH" varchar(4) NOT NULL default ' ',
  "AAABMFVI" varchar(6) NOT NULL default '000000',
  "AAABMFVJ" varchar(4) NOT NULL default '0000',
  "AAABMFVK" varchar(4) NOT NULL default ' ',
  "AAABMFVL" varchar(6) NOT NULL default '000000',
  "AAABMFVM" varchar(4) NOT NULL default '0000',
  "AAABMFVN" varchar(4) NOT NULL default ' ',
  "AAABMFVO" varchar(6) NOT NULL default '000000',
  "AAABMFVP" varchar(4) NOT NULL default '0000',
  "AAABMFVQ" varchar(4) NOT NULL default ' ',
  "AAABMFVR" varchar(6) NOT NULL default '000000',
  "AAABMFVS" varchar(4) NOT NULL default '0000',
  "AAABMFVT" varchar(4) NOT NULL default ' ',
  "AAABMFVU" varchar(6) NOT NULL default '000000',
  "AAABMFVV" varchar(4) NOT NULL default '0000',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMFWF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMHYZ" varchar(4) NOT NULL default ' ',
  "AAABMHZB" varchar(6) NOT NULL default '000000',
  "AAABMHZA" varchar(4) NOT NULL default '0000',
  "AAABMQWT" varchar(18) NOT NULL default ' ',
  "AAABMQOZ" decimal(7,3) NOT NULL default '0.000',
  "AAABMQPA" varchar(1) NOT NULL default ' ',
  "AAABMQPB" varchar(1) NOT NULL default ' ',
  "AAABMQPC" varchar(1) NOT NULL default ' ',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMFXD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMPOL" varchar(12) NOT NULL default ' ',
  "AAABMPOM" varchar(8) NOT NULL default '00000000',
  "AAABMPON" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMFXR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMQVQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMFYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMOQS" varchar(6) NOT NULL default ' ',
  "AAABMFYN" varchar(4) NOT NULL default ' ',
  "AAABMFYO" varchar(6) NOT NULL default '000000',
  "AAABMFYP" varchar(4) NOT NULL default '0000',
  "AAABMFYQ" varchar(4) NOT NULL default ' ',
  "AAABMFYR" varchar(6) NOT NULL default '000000',
  "AAABMFYS" varchar(4) NOT NULL default '0000',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  "AAABMFYW" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMFZD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMMTE" varchar(4) NOT NULL default ' ',
  "AAABMMTF" varchar(6) NOT NULL default '000000',
  "AAABMGUX" varchar(4) NOT NULL default '0000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMMTE","AAABMMTF","AAABMGUX","AAABMPQE"),
  KEY "AAABMFZQ" ("AAABMPQE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMFZS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMMTE" varchar(4) NOT NULL default ' ',
  "AAABMMTF" varchar(6) NOT NULL default '000000',
  "AAABMGUX" varchar(4) NOT NULL default '0000',
  "AAABMGSM" varchar(8) NOT NULL default '00000000',
  "AAABMPUD" varchar(16) NOT NULL default '0000000000000000',
  "AAABMGBK" varchar(16) NOT NULL default '0000000000000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMMTE","AAABMMTF","AAABMGUX","AAABMGSM","AAABMPUD"),
  KEY "AAABMGAI" ("AAABMRHL","AAABMMTE","AAABMMTF","AAABMGUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMGAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUD" varchar(16) NOT NULL default '0000000000000000',
  "AAABMGSE" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABMGSI" varchar(8) NOT NULL default '00000000',
  "AAABMPPI" varchar(8) NOT NULL default '00000000',
  "AAABMPPH" varchar(8) NOT NULL default '00000000',
  "AAABMGSO" varchar(1) NOT NULL default ' ',
  "AAABMGSL" varchar(1) NOT NULL default ' ',
  "AAABMGSM" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMPUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMGBC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMMTE" varchar(4) NOT NULL default ' ',
  "AAABMMTF" varchar(6) NOT NULL default '000000',
  "AAABMGUX" varchar(4) NOT NULL default '0000',
  "AAABMGSM" varchar(8) NOT NULL default '00000000',
  "AAABMPUD" varchar(16) NOT NULL default '0000000000000000',
  "AAABMGBK" varchar(16) NOT NULL default '0000000000000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMMTE","AAABMMTF","AAABMGUX","AAABMGSM","AAABMPUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMGBS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUD" varchar(16) NOT NULL default '0000000000000000',
  "AAABMGBV" varchar(16) NOT NULL default '0000000000000000',
  "AAABMGBW" varchar(16) NOT NULL default '0000000000000000',
  PRIMARY KEY ("AAABMRIL","AAABMPUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMGBZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUD" varchar(16) NOT NULL default '0000000000000000',
  "AAABMGRF" varchar(8) NOT NULL default '00000000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMJJG" varchar(8) NOT NULL default '00000000',
  "AAABMJKK" varchar(8) NOT NULL default '00000000',
  "AAABMGRO" decimal(15,5) NOT NULL default '0.00000',
  "AAABMGRP" decimal(15,5) NOT NULL default '0.00000',
  "AAABMGRQ" decimal(15,5) NOT NULL default '0.00000',
  "AAABMGRR" decimal(15,5) NOT NULL default '0.00000',
  "AAABMGRS" decimal(15,5) NOT NULL default '0.00000',
  "AAABMGRT" varchar(3) NOT NULL default ' ',
  "AAABMGRU" decimal(16,2) NOT NULL default '0.00',
  "AAABMGRV" decimal(16,2) NOT NULL default '0.00',
  "AAABMGRW" decimal(16,2) NOT NULL default '0.00',
  "AAABMGRX" decimal(16,2) NOT NULL default '0.00',
  "AAABMGRY" decimal(16,2) NOT NULL default '0.00',
  "AAABMGRZ" decimal(16,2) NOT NULL default '0.00',
  "AAABMGSA" decimal(16,2) NOT NULL default '0.00',
  "AAABMRFB" varchar(5) NOT NULL default ' ',
  "AAABMQPA" varchar(1) NOT NULL default ' ',
  "AAABMQPB" varchar(1) NOT NULL default ' ',
  "AAABMGSE" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABMGSI" varchar(8) NOT NULL default '00000000',
  "AAABMPPI" varchar(8) NOT NULL default '00000000',
  "AAABMPPH" varchar(8) NOT NULL default '00000000',
  "AAABMGSL" varchar(1) NOT NULL default ' ',
  "AAABMGSM" varchar(8) NOT NULL default '00000000',
  "AAABMGSN" varchar(1) NOT NULL default ' ',
  "AAABMGSO" varchar(1) NOT NULL default ' ',
  "AAABMGSP" varchar(12) NOT NULL default ' ',
  "AAABMGSQ" varchar(8) NOT NULL default '00000000',
  "AAABMGSR" varchar(6) NOT NULL default '000000',
  "AAABMGSS" varchar(12) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  "AAABMGSU" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMPUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMGDU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUD" varchar(16) NOT NULL default '0000000000000000',
  "AAABMGOG" varchar(10) NOT NULL default ' ',
  "AAABMGOH" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMGRF" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMPUD","AAABMGOG","AAABMGOH","AAABMQII","AAABMJXV","AAABMJXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMGEK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMMTE" varchar(4) NOT NULL default ' ',
  "AAABMMTF" varchar(6) NOT NULL default '000000',
  "AAABMGUX" varchar(4) NOT NULL default '0000',
  "AAABMGSM" varchar(8) NOT NULL default '00000000',
  "AAABMGOZ" varchar(6) NOT NULL default '000000',
  "AAABMGPA" varchar(6) NOT NULL default '000000',
  "AAABMPUN" varchar(1) NOT NULL default ' ',
  "AAABMGEU" varchar(1) NOT NULL default ' ',
  "AAABMGEV" varchar(8) NOT NULL default '00000000',
  "AAABMGUJ" varchar(9) NOT NULL default '000000000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMPUM" varchar(1) NOT NULL default ' ',
  "AAABMGUN" varchar(8) NOT NULL default '00000000',
  "AAABMGUO" varchar(8) NOT NULL default '00000000',
  "AAABMGFB" varchar(4) NOT NULL default ' ',
  "AAABMGFC" varchar(8) NOT NULL default '00000000',
  "AAABMGFD" varchar(8) NOT NULL default '00000000',
  "AAABMGFE" varchar(8) NOT NULL default '00000000',
  "AAABMGUQ" varchar(12) NOT NULL default ' ',
  "AAABMGUR" varchar(8) NOT NULL default '00000000',
  "AAABMGUS" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMMTE","AAABMMTF","AAABMGUX","AAABMGSM"),
  KEY "AAABMGFO" ("AAABMGSM"),
  KEY "AAABMGFQ" ("AAABMRHL","AAABMMTE","AAABMMTF","AAABMGUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMGFV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRHH" varchar(30) NOT NULL default ' ',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMRHC" varchar(30) NOT NULL default ' ',
  "AAABMGGD" varchar(20) NOT NULL default ' ',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMGGF" varchar(30) NOT NULL default ' ',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMGGH" varchar(30) NOT NULL default ' ',
  "AAABMQPU" varchar(1) NOT NULL default ' ',
  "AAABMOCK" varchar(30) NOT NULL default ' ',
  "AAABMMER" varchar(30) NOT NULL default ' ',
  "AAABMQVQ" varchar(60) NOT NULL default ' ',
  "AAABMRCJ" varchar(8) NOT NULL default '00000000',
  "AAABMRCQ" varchar(8) NOT NULL default '00000000',
  "AAABMGGO" varchar(1) NOT NULL default ' ',
  "AAABMGGP" varchar(4) NOT NULL default '0000',
  "AAABMIZS" varchar(60) NOT NULL default ' ',
  "AAABMMBE" varchar(4) NOT NULL default '0000',
  "AAABMGGS" varchar(10) NOT NULL default ' ',
  "AAABMGGT" varchar(9) NOT NULL default '000000000',
  "AAABMGGU" varchar(8) NOT NULL default '00000000',
  "AAABMGGV" varchar(1) NOT NULL default ' ',
  "AAABMGGW" varchar(4) NOT NULL default '0000',
  "AAABMGGX" varchar(4) NOT NULL default '0000',
  "AAABMGGY" varchar(13) NOT NULL default '0000000000000',
  "AAABMGGZ" varchar(9) NOT NULL default '000000000',
  "AAABMGHA" varchar(4) NOT NULL default '0000',
  "AAABMPOL" varchar(12) NOT NULL default ' ',
  "AAABMPOM" varchar(8) NOT NULL default '00000000',
  "AAABMPON" varchar(6) NOT NULL default '000000',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMGHG" varchar(10) NOT NULL default ' ',
  "AAABMGHH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRHH","AAABMRGQ","AAABMRGR","AAABMRGS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMGHO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMRDS" varchar(4) NOT NULL default '0000',
  "AAABMRDL" varchar(4) NOT NULL default '0000',
  "AAABMRDM" varchar(4) NOT NULL default '0000',
  "AAABMRDN" varchar(4) NOT NULL default '0000',
  "AAABMHYZ" varchar(4) NOT NULL default ' ',
  "AAABMHZA" varchar(4) NOT NULL default '0000',
  "AAABMHZB" varchar(6) NOT NULL default '000000',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMGIL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMNDG" varchar(4) NOT NULL default '0000',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMGJC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUD" varchar(16) NOT NULL default '0000000000000000',
  "AAABMGRF" varchar(8) NOT NULL default '00000000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMJJG" varchar(8) NOT NULL default '00000000',
  "AAABMJKK" varchar(8) NOT NULL default '00000000',
  "AAABMGRO" decimal(15,5) NOT NULL default '0.00000',
  "AAABMGRP" decimal(15,5) NOT NULL default '0.00000',
  "AAABMGRQ" decimal(15,5) NOT NULL default '0.00000',
  "AAABMGRR" decimal(15,5) NOT NULL default '0.00000',
  "AAABMGRS" decimal(15,5) NOT NULL default '0.00000',
  "AAABMGRT" varchar(3) NOT NULL default ' ',
  "AAABMGRU" decimal(16,2) NOT NULL default '0.00',
  "AAABMGRV" decimal(16,2) NOT NULL default '0.00',
  "AAABMGRW" decimal(16,2) NOT NULL default '0.00',
  "AAABMGRX" decimal(16,2) NOT NULL default '0.00',
  "AAABMGRY" decimal(16,2) NOT NULL default '0.00',
  "AAABMGRZ" decimal(16,2) NOT NULL default '0.00',
  "AAABMGSA" decimal(16,2) NOT NULL default '0.00',
  "AAABMRFB" varchar(5) NOT NULL default ' ',
  "AAABMQPA" varchar(1) NOT NULL default ' ',
  "AAABMQPB" varchar(1) NOT NULL default ' ',
  "AAABMGSE" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABMGSI" varchar(8) NOT NULL default '00000000',
  "AAABMPPI" varchar(8) NOT NULL default '00000000',
  "AAABMPPH" varchar(8) NOT NULL default '00000000',
  "AAABMGSL" varchar(1) NOT NULL default ' ',
  "AAABMGSM" varchar(8) NOT NULL default '00000000',
  "AAABMGSN" varchar(1) NOT NULL default ' ',
  "AAABMGSO" varchar(1) NOT NULL default ' ',
  "AAABMGSP" varchar(12) NOT NULL default ' ',
  "AAABMGSQ" varchar(8) NOT NULL default '00000000',
  "AAABMGSR" varchar(6) NOT NULL default '000000',
  "AAABMGSS" varchar(12) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  "AAABMGSU" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMPUD"),
  KEY "AAABMGKX" ("AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMGLC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUD" varchar(16) NOT NULL default '0000000000000000',
  "AAABMGRF" varchar(8) NOT NULL default '00000000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMJJG" varchar(8) NOT NULL default '00000000',
  "AAABMJKK" varchar(8) NOT NULL default '00000000',
  "AAABMGRO" decimal(15,5) NOT NULL default '0.00000',
  "AAABMGRP" decimal(15,5) NOT NULL default '0.00000',
  "AAABMGRQ" decimal(15,5) NOT NULL default '0.00000',
  "AAABMGRR" decimal(15,5) NOT NULL default '0.00000',
  "AAABMGRS" decimal(15,5) NOT NULL default '0.00000',
  "AAABMGRT" varchar(3) NOT NULL default ' ',
  "AAABMGRU" decimal(16,2) NOT NULL default '0.00',
  "AAABMGRV" decimal(16,2) NOT NULL default '0.00',
  "AAABMGRW" decimal(16,2) NOT NULL default '0.00',
  "AAABMGRX" decimal(16,2) NOT NULL default '0.00',
  "AAABMGRY" decimal(16,2) NOT NULL default '0.00',
  "AAABMGRZ" decimal(16,2) NOT NULL default '0.00',
  "AAABMGSA" decimal(16,2) NOT NULL default '0.00',
  "AAABMRFB" varchar(5) NOT NULL default ' ',
  "AAABMQPA" varchar(1) NOT NULL default ' ',
  "AAABMQPB" varchar(1) NOT NULL default ' ',
  "AAABMGSE" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABMGSI" varchar(8) NOT NULL default '00000000',
  "AAABMPPI" varchar(8) NOT NULL default '00000000',
  "AAABMPPH" varchar(8) NOT NULL default '00000000',
  "AAABMGSL" varchar(1) NOT NULL default ' ',
  "AAABMGSM" varchar(8) NOT NULL default '00000000',
  "AAABMGSN" varchar(1) NOT NULL default ' ',
  "AAABMGSO" varchar(1) NOT NULL default ' ',
  "AAABMGSP" varchar(12) NOT NULL default ' ',
  "AAABMGSQ" varchar(8) NOT NULL default '00000000',
  "AAABMGSR" varchar(6) NOT NULL default '000000',
  "AAABMGSS" varchar(12) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  "AAABMGSU" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMPUD"),
  KEY "AAABMGMX" ("AAABMPUD","AAABMGRF"),
  KEY "AAABMGNA" ("AAABMGSL"),
  KEY "AAABMGNC" ("AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMGNH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUD" varchar(16) NOT NULL default '0000000000000000',
  "AAABMGOG" varchar(10) NOT NULL default ' ',
  "AAABMGOH" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPUD","AAABMGOG","AAABMGOH","AAABMQII","AAABMJXV","AAABMJXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMGNW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUD" varchar(16) NOT NULL default '0000000000000000',
  "AAABMGOG" varchar(10) NOT NULL default ' ',
  "AAABMGOH" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPUD","AAABMGOG","AAABMGOH","AAABMQII","AAABMJXV","AAABMJXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMGOL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMMTE" varchar(4) NOT NULL default ' ',
  "AAABMMTF" varchar(6) NOT NULL default '000000',
  "AAABMGUX" varchar(4) NOT NULL default '0000',
  "AAABMGOZ" varchar(6) NOT NULL default '000000',
  "AAABMGPA" varchar(6) NOT NULL default '000000',
  "AAABMGSM" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMMTE","AAABMMTF","AAABMGUX","AAABMGOZ","AAABMGPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMGPB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMMTE" varchar(4) NOT NULL default ' ',
  "AAABMMTF" varchar(6) NOT NULL default '000000',
  "AAABMGUX" varchar(4) NOT NULL default '0000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMGTJ" varchar(8) NOT NULL default '00000000',
  "AAABMGUJ" varchar(9) NOT NULL default '000000000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMGTN" varchar(4) NOT NULL default ' ',
  "AAABMIMR" varchar(4) NOT NULL default '0000',
  "AAABMGTP" varchar(4) NOT NULL default ' ',
  "AAABMGTQ" varchar(6) NOT NULL default '000000',
  "AAABMGTR" varchar(4) NOT NULL default '0000',
  "AAABMGTS" varchar(9) NOT NULL default '000000000',
  "AAABMGUQ" varchar(12) NOT NULL default ' ',
  "AAABMGUR" varchar(8) NOT NULL default '00000000',
  "AAABMGUS" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMMTE","AAABMMTF","AAABMGUX","AAABMPQE"),
  KEY "AAABMGQB" ("AAABMMTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMGQD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMMTE" varchar(4) NOT NULL default ' ',
  "AAABMMTF" varchar(6) NOT NULL default '000000',
  "AAABMGUX" varchar(4) NOT NULL default '0000',
  "AAABMGUY" varchar(4) NOT NULL default '0000',
  "AAABMGUJ" varchar(9) NOT NULL default '000000000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMGUN" varchar(8) NOT NULL default '00000000',
  "AAABMGUO" varchar(8) NOT NULL default '00000000',
  "AAABMPUN" varchar(1) NOT NULL default ' ',
  "AAABMGUQ" varchar(12) NOT NULL default ' ',
  "AAABMGUR" varchar(8) NOT NULL default '00000000',
  "AAABMGUS" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMMTE","AAABMMTF","AAABMGUX","AAABMGUY"),
  KEY "AAABMGRA" ("AAABMMTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMGRC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUD" varchar(16) NOT NULL default '0000000000000000',
  "AAABMGRF" varchar(8) NOT NULL default '00000000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMJJG" varchar(8) NOT NULL default '00000000',
  "AAABMJKK" varchar(8) NOT NULL default '00000000',
  "AAABMGRO" decimal(15,5) NOT NULL default '0.00000',
  "AAABMGRP" decimal(15,5) NOT NULL default '0.00000',
  "AAABMGRQ" decimal(15,5) NOT NULL default '0.00000',
  "AAABMGRR" decimal(15,5) NOT NULL default '0.00000',
  "AAABMGRS" decimal(15,5) NOT NULL default '0.00000',
  "AAABMGRT" varchar(3) NOT NULL default ' ',
  "AAABMGRU" decimal(16,2) NOT NULL default '0.00',
  "AAABMGRV" decimal(16,2) NOT NULL default '0.00',
  "AAABMGRW" decimal(16,2) NOT NULL default '0.00',
  "AAABMGRX" decimal(16,2) NOT NULL default '0.00',
  "AAABMGRY" decimal(16,2) NOT NULL default '0.00',
  "AAABMGRZ" decimal(16,2) NOT NULL default '0.00',
  "AAABMGSA" decimal(16,2) NOT NULL default '0.00',
  "AAABMRFB" varchar(5) NOT NULL default ' ',
  "AAABMQPA" varchar(1) NOT NULL default ' ',
  "AAABMQPB" varchar(1) NOT NULL default ' ',
  "AAABMGSE" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABMGSI" varchar(8) NOT NULL default '00000000',
  "AAABMPPI" varchar(8) NOT NULL default '00000000',
  "AAABMPPH" varchar(8) NOT NULL default '00000000',
  "AAABMGSL" varchar(1) NOT NULL default ' ',
  "AAABMGSM" varchar(8) NOT NULL default '00000000',
  "AAABMGSN" varchar(1) NOT NULL default ' ',
  "AAABMGSO" varchar(1) NOT NULL default ' ',
  "AAABMGSP" varchar(12) NOT NULL default ' ',
  "AAABMGSQ" varchar(8) NOT NULL default '00000000',
  "AAABMGSR" varchar(6) NOT NULL default '000000',
  "AAABMGSS" varchar(12) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  "AAABMGSU" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMPUD"),
  KEY "AAABMGSX" ("AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMGTC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMMTE" varchar(4) NOT NULL default ' ',
  "AAABMMTF" varchar(6) NOT NULL default '000000',
  "AAABMGUX" varchar(4) NOT NULL default '0000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMGTJ" varchar(8) NOT NULL default '00000000',
  "AAABMGUJ" varchar(9) NOT NULL default '000000000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMGTN" varchar(4) NOT NULL default ' ',
  "AAABMIMR" varchar(4) NOT NULL default '0000',
  "AAABMGTP" varchar(4) NOT NULL default ' ',
  "AAABMGTQ" varchar(6) NOT NULL default '000000',
  "AAABMGTR" varchar(4) NOT NULL default '0000',
  "AAABMGTS" varchar(9) NOT NULL default '000000000',
  "AAABMGUQ" varchar(12) NOT NULL default ' ',
  "AAABMGUR" varchar(8) NOT NULL default '00000000',
  "AAABMGUS" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMMTE","AAABMMTF","AAABMGUX","AAABMPQE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMGUC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMMTE" varchar(4) NOT NULL default ' ',
  "AAABMMTF" varchar(6) NOT NULL default '000000',
  "AAABMGUX" varchar(4) NOT NULL default '0000',
  "AAABMGUY" varchar(4) NOT NULL default '0000',
  "AAABMGUJ" varchar(9) NOT NULL default '000000000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMGUN" varchar(8) NOT NULL default '00000000',
  "AAABMGUO" varchar(8) NOT NULL default '00000000',
  "AAABMPUN" varchar(1) NOT NULL default ' ',
  "AAABMGUQ" varchar(12) NOT NULL default ' ',
  "AAABMGUR" varchar(8) NOT NULL default '00000000',
  "AAABMGUS" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMMTE","AAABMMTF","AAABMGUX","AAABMGUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMGUZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMHYZ" varchar(4) NOT NULL default ' ',
  "AAABMHZB" varchar(6) NOT NULL default '000000',
  "AAABMHZA" varchar(4) NOT NULL default '0000',
  "AAABMGVJ" decimal(16,2) NOT NULL default '0.00',
  "AAABMGVK" varchar(60) NOT NULL default ' ',
  "AAABMGVL" varchar(15) NOT NULL default ' ',
  "AAABMGVM" varchar(18) NOT NULL default ' ',
  "AAABMGVN" varchar(16) NOT NULL default ' ',
  "AAABMGVO" varchar(8) NOT NULL default '00000000',
  "AAABMGVP" varchar(8) NOT NULL default '00000000',
  "AAABMGVQ" decimal(16,2) NOT NULL default '0.00',
  "AAABMGVR" decimal(7,3) NOT NULL default '0.000',
  "AAABMLXL" varchar(4) NOT NULL default '0000',
  "AAABMKLV" varchar(4) NOT NULL default '0000',
  "AAABMGVU" varchar(8) NOT NULL default '00000000',
  "AAABMGVV" decimal(16,2) NOT NULL default '0.00',
  "AAABMGVW" varchar(8) NOT NULL default '00000000',
  "AAABMLPE" varchar(4) NOT NULL default '0000',
  "AAABMGVY" varchar(50) NOT NULL default ' ',
  "AAABMGVZ" varchar(50) NOT NULL default ' ',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  "AAABMGWD" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMGWK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMHYZ" varchar(4) NOT NULL default ' ',
  "AAABMHZB" varchar(6) NOT NULL default '000000',
  "AAABMHZA" varchar(4) NOT NULL default '0000',
  "AAABMGWU" decimal(16,2) NOT NULL default '0.00',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMGXE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMQMN" varchar(10) NOT NULL default ' ',
  "AAABMQQF" varchar(4) NOT NULL default ' ',
  "AAABMQQG" varchar(2) NOT NULL default ' ',
  "AAABMQQH" varchar(2) NOT NULL default ' ',
  "AAABMGYF" varchar(10) NOT NULL default ' ',
  "AAABMQOS" varchar(4) NOT NULL default ' ',
  "AAABMQOT" varchar(2) NOT NULL default ' ',
  "AAABMQOU" varchar(2) NOT NULL default ' ',
  "AAABMGXT" varchar(10) NOT NULL default ' ',
  "AAABMGXU" varchar(4) NOT NULL default ' ',
  "AAABMQMR" varchar(10) NOT NULL default ' ',
  "AAABMGXW" varchar(10) NOT NULL default ' ',
  "AAABMGXX" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD"),
  KEY "AAABMGYE" ("AAABMGYF"),
  KEY "AAABMGYG" ("AAABMQMN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMGYI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMGYP" varchar(50) NOT NULL default ' ',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMGYZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMGZO" varchar(8) NOT NULL default '00000000',
  "AAABMGZP" varchar(6) NOT NULL default '000000',
  "AAABMGZQ" varchar(4) NOT NULL default '0000',
  "AAABMNNR" varchar(12) NOT NULL default ' ',
  "AAABMGZF" varchar(4) NOT NULL default '0000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMGZH" varchar(4) NOT NULL default '0000',
  "AAABMOBT" varchar(4) NOT NULL default ' ',
  "AAABMOBI" varchar(6) NOT NULL default '000000',
  "AAABMGZK" varchar(30) NOT NULL default ' ',
  "AAABMGZL" varchar(1) NOT NULL default ' ',
  "AAABMGZM" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMGZO","AAABMGZP","AAABMGZQ","AAABMNNR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMGZS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMGZZ" decimal(7,3) NOT NULL default '0.000',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMHAJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMHAQ" decimal(7,3) NOT NULL default '0.000',
  "AAABMHAR" decimal(7,3) NOT NULL default '0.000',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMHBB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABMRGE" varchar(4) NOT NULL default '0000',
  "AAABMHDH" varchar(4) NOT NULL default '0000',
  "AAABMHBG" decimal(7,0) NOT NULL default '0',
  "AAABMHBH" varchar(1) NOT NULL default ' ',
  "AAABMHBI" decimal(7,0) NOT NULL default '0',
  "AAABMHBJ" decimal(7,0) NOT NULL default '0',
  "AAABMHBK" decimal(13,3) NOT NULL default '0.000',
  "AAABMHBL" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIN","AAABMRGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMHBP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMHBW" varchar(4) NOT NULL default ' ',
  "AAABMHBX" varchar(6) NOT NULL default '000000',
  "AAABMHBY" varchar(4) NOT NULL default '0000',
  "AAABMHBZ" varchar(1) NOT NULL default ' ',
  "AAABMPSD" varchar(4) NOT NULL default ' ',
  "AAABMPSI" varchar(4) NOT NULL default ' ',
  "AAABMPSJ" varchar(2) NOT NULL default ' ',
  "AAABMPSF" varchar(1) NOT NULL default ' ',
  "AAABMPSG" varchar(1) NOT NULL default ' ',
  "AAABMHCF" varchar(1) NOT NULL default ' ',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMHCP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABMNJQ" varchar(4) NOT NULL default ' ',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMORC","AAABMNJQ","AAABMNVC","AAABMOGK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMHDA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMHDH" varchar(4) NOT NULL default '0000',
  "AAABMHDE" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMHDH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMHDI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMHDP" varchar(4) NOT NULL default ' ',
  "AAABMHDQ" varchar(6) NOT NULL default '000000',
  "AAABMHDR" varchar(4) NOT NULL default '0000',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  "AAABMHDV" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMHEC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMQVR" varchar(4) NOT NULL default '0000',
  "AAABMQVS" varchar(4) NOT NULL default '0000',
  "AAABMHEL" varchar(25) NOT NULL default ' ',
  "AAABMPTM" decimal(15,5) NOT NULL default '0.00000',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAABMHYZ" varchar(4) NOT NULL default ' ',
  "AAABMHZB" varchar(6) NOT NULL default '000000',
  "AAABMHZA" varchar(4) NOT NULL default '0000',
  "AAABMHER" decimal(16,2) NOT NULL default '0.00',
  "AAABMHES" decimal(16,2) NOT NULL default '0.00',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMHEW" varchar(50) NOT NULL default ' ',
  "AAABMHEX" varchar(50) NOT NULL default ' ',
  "AAABMQVW" varchar(18) NOT NULL default ' ',
  "AAABMKAN" varchar(8) NOT NULL default '00000000',
  "AAABMKAO" varchar(8) NOT NULL default '00000000',
  "AAABMOFL" varchar(4) NOT NULL default '0000',
  "AAABMLBI" varchar(4) NOT NULL default '0000',
  "AAABMHFD" varchar(4) NOT NULL default '0000',
  "AAABMHFE" varchar(4) NOT NULL default '0000',
  "AAABMHFF" varchar(4) NOT NULL default '0000',
  "AAABMHFG" varchar(4) NOT NULL default '0000',
  "AAABMHFH" varchar(4) NOT NULL default '0000',
  "AAABMHFI" varchar(4) NOT NULL default '0000',
  "AAABMHFJ" varchar(4) NOT NULL default '0000',
  "AAABMHFK" varchar(4) NOT NULL default '0000',
  "AAABMHFL" varchar(4) NOT NULL default '0000',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  "AAABMHFP" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMHFW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMHYZ" varchar(4) NOT NULL default ' ',
  "AAABMHZB" varchar(6) NOT NULL default '000000',
  "AAABMHZA" varchar(4) NOT NULL default '0000',
  "AAABMHGG" varchar(8) NOT NULL default '00000000',
  "AAABMMHY" varchar(4) NOT NULL default '0000',
  "AAABMNOB" varchar(4) NOT NULL default '0000',
  "AAABMHHG" decimal(16,2) NOT NULL default '0.00',
  "AAABMHHH" varchar(30) NOT NULL default ' ',
  "AAABMHHI" varchar(30) NOT NULL default ' ',
  "AAABMHHJ" decimal(16,2) NOT NULL default '0.00',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMHGW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMHYZ" varchar(4) NOT NULL default ' ',
  "AAABMHZB" varchar(6) NOT NULL default '000000',
  "AAABMHZA" varchar(4) NOT NULL default '0000',
  "AAABMHHG" decimal(16,2) NOT NULL default '0.00',
  "AAABMHHH" varchar(30) NOT NULL default ' ',
  "AAABMHHI" varchar(30) NOT NULL default ' ',
  "AAABMHHJ" decimal(16,2) NOT NULL default '0.00',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMHHT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMHIA" varchar(18) NOT NULL default ' ',
  "AAABMHIB" varchar(18) NOT NULL default ' ',
  "AAABMHIC" varchar(18) NOT NULL default ' ',
  "AAABMHID" varchar(18) NOT NULL default ' ',
  "AAABMHIE" varchar(18) NOT NULL default ' ',
  "AAABMHIF" varchar(18) NOT NULL default ' ',
  "AAABMHIG" varchar(18) NOT NULL default ' ',
  "AAABMHIH" varchar(18) NOT NULL default ' ',
  "AAABMHII" varchar(18) NOT NULL default ' ',
  "AAABMHIJ" varchar(18) NOT NULL default ' ',
  "AAABMHIK" varchar(18) NOT NULL default ' ',
  "AAABMHIL" varchar(18) NOT NULL default ' ',
  "AAABMHIM" varchar(18) NOT NULL default ' ',
  "AAABMHIN" varchar(18) NOT NULL default ' ',
  "AAABMHIO" varchar(18) NOT NULL default ' ',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMHIY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMQYF" varchar(8) NOT NULL default '00000000',
  "AAABMQYG" varchar(1) NOT NULL default ' ',
  "AAABMQYH" varchar(4) NOT NULL default ' ',
  "AAABMQYI" varchar(4) NOT NULL default '0000',
  "AAABMQYJ" varchar(1) NOT NULL default ' ',
  "AAABMQYK" varchar(1) NOT NULL default ' ',
  "AAABMQYL" varchar(1) NOT NULL default ' ',
  "AAABMQYM" varchar(1) NOT NULL default ' ',
  "AAABMQYN" varchar(1) NOT NULL default ' ',
  "AAABMQYO" varchar(1) NOT NULL default ' ',
  "AAABMKJH" varchar(4) NOT NULL default '0000',
  "AAABMQYP" varchar(1) NOT NULL default ' ',
  "AAABMQYQ" varchar(1) NOT NULL default ' ',
  "AAABMQYR" varchar(1) NOT NULL default ' ',
  "AAABMQYS" varchar(1) NOT NULL default ' ',
  "AAABMQYT" varchar(1) NOT NULL default ' ',
  "AAABMQYU" varchar(1) NOT NULL default ' ',
  "AAABMQYV" varchar(1) NOT NULL default ' ',
  "AAABMQYW" varchar(1) NOT NULL default ' ',
  "AAABMQYX" varchar(1) NOT NULL default ' ',
  "AAABMQYY" varchar(1) NOT NULL default ' ',
  "AAABMQYZ" varchar(1) NOT NULL default ' ',
  "AAABMHKB" varchar(1) NOT NULL default ' ',
  "AAABMKJV" varchar(5) NOT NULL default ' ',
  "AAABMKJW" varchar(5) NOT NULL default ' ',
  "AAABMHKE" varchar(4) NOT NULL default '0000',
  "AAABMHKF" varchar(4) NOT NULL default '0000',
  "AAABMHKG" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMHKN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMHMC" varchar(4) NOT NULL default '0000',
  "AAABMHMD" varchar(4) NOT NULL default '0000',
  "AAABMHKW" varchar(4) NOT NULL default '0000',
  "AAABMHKX" varchar(4) NOT NULL default ' ',
  "AAABMHKY" varchar(6) NOT NULL default '000000',
  "AAABMHKZ" varchar(4) NOT NULL default '0000',
  "AAABMHLA" varchar(4) NOT NULL default ' ',
  "AAABMHLB" varchar(6) NOT NULL default '000000',
  "AAABMHLC" varchar(4) NOT NULL default '0000',
  "AAABMHLD" varchar(4) NOT NULL default ' ',
  "AAABMHLE" varchar(6) NOT NULL default '000000',
  "AAABMHLF" varchar(4) NOT NULL default '0000',
  "AAABMHLG" varchar(4) NOT NULL default ' ',
  "AAABMHLH" varchar(6) NOT NULL default '000000',
  "AAABMHLI" varchar(4) NOT NULL default '0000',
  "AAABMHLJ" varchar(4) NOT NULL default ' ',
  "AAABMHLK" varchar(6) NOT NULL default '000000',
  "AAABMHLL" varchar(4) NOT NULL default '0000',
  "AAABMHLM" varchar(4) NOT NULL default ' ',
  "AAABMHLN" varchar(6) NOT NULL default '000000',
  "AAABMHLO" varchar(4) NOT NULL default '0000',
  "AAABMHLP" varchar(4) NOT NULL default ' ',
  "AAABMHLQ" varchar(6) NOT NULL default '000000',
  "AAABMHLR" varchar(4) NOT NULL default '0000',
  "AAABMHLS" varchar(4) NOT NULL default '0000',
  "AAABMHLT" varchar(4) NOT NULL default '0000',
  "AAABMHLU" varchar(4) NOT NULL default '0000',
  "AAABMHLV" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD","AAABMHMC","AAABMHMD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMHME" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMHNT" varchar(4) NOT NULL default '0000',
  "AAABMHNU" varchar(4) NOT NULL default '0000',
  "AAABMHMN" varchar(4) NOT NULL default '0000',
  "AAABMHMO" varchar(4) NOT NULL default ' ',
  "AAABMHMP" varchar(6) NOT NULL default '000000',
  "AAABMHMQ" varchar(4) NOT NULL default '0000',
  "AAABMHMR" varchar(4) NOT NULL default ' ',
  "AAABMHMS" varchar(6) NOT NULL default '000000',
  "AAABMHMT" varchar(4) NOT NULL default '0000',
  "AAABMHMU" varchar(4) NOT NULL default ' ',
  "AAABMHMV" varchar(6) NOT NULL default '000000',
  "AAABMHMW" varchar(4) NOT NULL default '0000',
  "AAABMHMX" varchar(4) NOT NULL default ' ',
  "AAABMHMY" varchar(6) NOT NULL default '000000',
  "AAABMHMZ" varchar(4) NOT NULL default '0000',
  "AAABMHNA" varchar(4) NOT NULL default ' ',
  "AAABMHNB" varchar(6) NOT NULL default '000000',
  "AAABMHNC" varchar(4) NOT NULL default '0000',
  "AAABMHND" varchar(4) NOT NULL default ' ',
  "AAABMHNE" varchar(6) NOT NULL default '000000',
  "AAABMHNF" varchar(4) NOT NULL default '0000',
  "AAABMHNG" varchar(4) NOT NULL default ' ',
  "AAABMHNH" varchar(6) NOT NULL default '000000',
  "AAABMHNI" varchar(4) NOT NULL default '0000',
  "AAABMHNJ" varchar(4) NOT NULL default '0000',
  "AAABMHNK" varchar(4) NOT NULL default '0000',
  "AAABMHNL" varchar(4) NOT NULL default '0000',
  "AAABMHNM" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD","AAABMHNT","AAABMHNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMHNV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMHOC" varchar(4) NOT NULL default '0000',
  "AAABMHOD" varchar(4) NOT NULL default ' ',
  "AAABMHOE" varchar(6) NOT NULL default '000000',
  "AAABMHOF" varchar(4) NOT NULL default '0000',
  "AAABMHOG" varchar(4) NOT NULL default ' ',
  "AAABMHOH" varchar(6) NOT NULL default '000000',
  "AAABMHOI" varchar(4) NOT NULL default '0000',
  "AAABMMOQ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMHOQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMHPW" varchar(4) NOT NULL default '0000',
  "AAABMHPX" varchar(4) NOT NULL default '0000',
  "AAABMHOZ" varchar(5) NOT NULL default ' ',
  "AAABMHPA" varchar(5) NOT NULL default ' ',
  "AAABMHPB" varchar(4) NOT NULL default ' ',
  "AAABMHPC" varchar(6) NOT NULL default '000000',
  "AAABMHPD" varchar(4) NOT NULL default '0000',
  "AAABMHPE" varchar(4) NOT NULL default ' ',
  "AAABMHPF" varchar(6) NOT NULL default '000000',
  "AAABMHPG" varchar(4) NOT NULL default '0000',
  "AAABMHPH" varchar(4) NOT NULL default '0000',
  "AAABMHPI" varchar(4) NOT NULL default ' ',
  "AAABMHPJ" varchar(6) NOT NULL default '000000',
  "AAABMHPK" varchar(4) NOT NULL default '0000',
  "AAABMHPL" varchar(4) NOT NULL default '0000',
  "AAABMHPM" varchar(4) NOT NULL default '0000',
  "AAABMHPN" varchar(4) NOT NULL default '0000',
  "AAABMHPO" varchar(4) NOT NULL default '0000',
  "AAABMHPP" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD","AAABMHPW","AAABMHPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMHPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMHRB" varchar(4) NOT NULL default '0000',
  "AAABMQPJ" varchar(4) NOT NULL default '0000',
  "AAABMHQH" varchar(5) NOT NULL default ' ',
  "AAABMHQI" varchar(5) NOT NULL default ' ',
  "AAABMHQJ" varchar(4) NOT NULL default ' ',
  "AAABMHQK" varchar(6) NOT NULL default '000000',
  "AAABMHQL" varchar(4) NOT NULL default '0000',
  "AAABMHQM" varchar(4) NOT NULL default ' ',
  "AAABMHQN" varchar(6) NOT NULL default '000000',
  "AAABMHQO" varchar(4) NOT NULL default '0000',
  "AAABMHQP" varchar(4) NOT NULL default '0000',
  "AAABMHQQ" varchar(4) NOT NULL default '0000',
  "AAABMHQR" varchar(4) NOT NULL default '0000',
  "AAABMHQS" varchar(4) NOT NULL default '0000',
  "AAABMHQT" varchar(4) NOT NULL default '0000',
  "AAABMHQU" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD","AAABMHRB","AAABMQPJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMHRD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMHSK" varchar(4) NOT NULL default '0000',
  "AAABMHSL" varchar(4) NOT NULL default '0000',
  "AAABMHRM" varchar(5) NOT NULL default ' ',
  "AAABMHRN" varchar(5) NOT NULL default ' ',
  "AAABMHRO" varchar(4) NOT NULL default '0000',
  "AAABMHRP" varchar(4) NOT NULL default ' ',
  "AAABMHRQ" varchar(6) NOT NULL default '000000',
  "AAABMHRR" varchar(4) NOT NULL default '0000',
  "AAABMHRS" varchar(4) NOT NULL default ' ',
  "AAABMHRT" varchar(6) NOT NULL default '000000',
  "AAABMHRU" varchar(4) NOT NULL default '0000',
  "AAABMHRV" varchar(4) NOT NULL default '0000',
  "AAABMHRW" varchar(4) NOT NULL default ' ',
  "AAABMHRX" varchar(6) NOT NULL default '000000',
  "AAABMHRY" varchar(4) NOT NULL default '0000',
  "AAABMHRZ" varchar(4) NOT NULL default '0000',
  "AAABMHSA" varchar(4) NOT NULL default '0000',
  "AAABMHSB" varchar(4) NOT NULL default '0000',
  "AAABMHSC" varchar(4) NOT NULL default '0000',
  "AAABMHSD" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD","AAABMHSK","AAABMHSL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMHSM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMHUE" varchar(4) NOT NULL default '0000',
  "AAABMHUF" varchar(4) NOT NULL default '0000',
  "AAABMHSV" varchar(4) NOT NULL default ' ',
  "AAABMHSW" varchar(6) NOT NULL default '000000',
  "AAABMHSX" varchar(4) NOT NULL default '0000',
  "AAABMHSY" varchar(4) NOT NULL default '0000',
  "AAABMHSZ" varchar(4) NOT NULL default ' ',
  "AAABMHTA" varchar(6) NOT NULL default '000000',
  "AAABMHTB" varchar(4) NOT NULL default '0000',
  "AAABMHTC" varchar(4) NOT NULL default '0000',
  "AAABMHTD" varchar(4) NOT NULL default ' ',
  "AAABMHTE" varchar(6) NOT NULL default '000000',
  "AAABMHTF" varchar(4) NOT NULL default '0000',
  "AAABMHTG" varchar(4) NOT NULL default '0000',
  "AAABMHTH" varchar(4) NOT NULL default '0000',
  "AAABMHTI" varchar(4) NOT NULL default '0000',
  "AAABMHTJ" varchar(4) NOT NULL default '0000',
  "AAABMHTK" varchar(4) NOT NULL default '0000',
  "AAABMHTL" varchar(4) NOT NULL default '0000',
  "AAABMHTM" varchar(4) NOT NULL default '0000',
  "AAABMHTN" varchar(4) NOT NULL default '0000',
  "AAABMHTO" varchar(4) NOT NULL default ' ',
  "AAABMHTP" varchar(6) NOT NULL default '000000',
  "AAABMHTQ" varchar(4) NOT NULL default '0000',
  "AAABMHTR" varchar(4) NOT NULL default ' ',
  "AAABMHTS" varchar(6) NOT NULL default '000000',
  "AAABMHTT" varchar(4) NOT NULL default '0000',
  "AAABMHTU" varchar(4) NOT NULL default '0000',
  "AAABMHTV" varchar(4) NOT NULL default '0000',
  "AAABMHTW" varchar(4) NOT NULL default '0000',
  "AAABMHTX" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD","AAABMHUE","AAABMHUF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMHUG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMHVW" varchar(4) NOT NULL default '0000',
  "AAABMHVX" varchar(4) NOT NULL default '0000',
  "AAABMOMK" varchar(4) NOT NULL default '0000',
  "AAABMHUQ" varchar(4) NOT NULL default ' ',
  "AAABMHUR" varchar(6) NOT NULL default '000000',
  "AAABMHUS" varchar(4) NOT NULL default '0000',
  "AAABMHUT" varchar(4) NOT NULL default '0000',
  "AAABMHUU" varchar(4) NOT NULL default ' ',
  "AAABMHUV" varchar(6) NOT NULL default '000000',
  "AAABMHUW" varchar(4) NOT NULL default '0000',
  "AAABMHUX" varchar(4) NOT NULL default '0000',
  "AAABMHUY" varchar(4) NOT NULL default ' ',
  "AAABMHUZ" varchar(6) NOT NULL default '000000',
  "AAABMHVA" varchar(4) NOT NULL default '0000',
  "AAABMHVB" varchar(4) NOT NULL default '0000',
  "AAABMHVC" varchar(4) NOT NULL default '0000',
  "AAABMHVD" varchar(4) NOT NULL default '0000',
  "AAABMHVE" varchar(4) NOT NULL default '0000',
  "AAABMHVF" varchar(4) NOT NULL default '0000',
  "AAABMHVG" varchar(4) NOT NULL default '0000',
  "AAABMHVH" varchar(4) NOT NULL default '0000',
  "AAABMHVI" varchar(4) NOT NULL default '0000',
  "AAABMHVJ" varchar(4) NOT NULL default ' ',
  "AAABMHVK" varchar(6) NOT NULL default '000000',
  "AAABMHVL" varchar(4) NOT NULL default '0000',
  "AAABMHVM" varchar(4) NOT NULL default '0000',
  "AAABMHVN" varchar(4) NOT NULL default '0000',
  "AAABMHVO" varchar(4) NOT NULL default '0000',
  "AAABMHVP" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD","AAABMHVW","AAABMHVX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMHVY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMHXU" varchar(4) NOT NULL default '0000',
  "AAABMHXV" varchar(4) NOT NULL default '0000',
  "AAABMHWH" varchar(4) NOT NULL default ' ',
  "AAABMHWI" varchar(6) NOT NULL default '000000',
  "AAABMHWJ" varchar(4) NOT NULL default '0000',
  "AAABMHWK" varchar(1) NOT NULL default ' ',
  "AAABMHWL" varchar(4) NOT NULL default ' ',
  "AAABMHWM" varchar(6) NOT NULL default '000000',
  "AAABMHWN" varchar(4) NOT NULL default '0000',
  "AAABMHWO" varchar(4) NOT NULL default '0000',
  "AAABMHWP" varchar(4) NOT NULL default ' ',
  "AAABMHWQ" varchar(6) NOT NULL default '000000',
  "AAABMHWR" varchar(4) NOT NULL default '0000',
  "AAABMHWS" varchar(4) NOT NULL default '0000',
  "AAABMHWT" varchar(4) NOT NULL default ' ',
  "AAABMHWU" varchar(6) NOT NULL default '000000',
  "AAABMHWV" varchar(4) NOT NULL default '0000',
  "AAABMHWW" varchar(4) NOT NULL default '0000',
  "AAABMHWX" varchar(4) NOT NULL default '0000',
  "AAABMHWY" varchar(4) NOT NULL default '0000',
  "AAABMHWZ" varchar(4) NOT NULL default '0000',
  "AAABMHXA" varchar(4) NOT NULL default '0000',
  "AAABMHXB" varchar(4) NOT NULL default '0000',
  "AAABMHXC" varchar(4) NOT NULL default '0000',
  "AAABMHXD" varchar(4) NOT NULL default '0000',
  "AAABMHXE" varchar(4) NOT NULL default ' ',
  "AAABMHXF" varchar(6) NOT NULL default '000000',
  "AAABMHXG" varchar(4) NOT NULL default '0000',
  "AAABMHXH" varchar(4) NOT NULL default ' ',
  "AAABMHXI" varchar(6) NOT NULL default '000000',
  "AAABMHXJ" varchar(4) NOT NULL default '0000',
  "AAABMHXK" varchar(4) NOT NULL default '0000',
  "AAABMHXL" varchar(4) NOT NULL default '0000',
  "AAABMHXM" varchar(4) NOT NULL default '0000',
  "AAABMHXN" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD","AAABMHXU","AAABMHXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMHXW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMHYZ" varchar(4) NOT NULL default ' ',
  "AAABMHZA" varchar(4) NOT NULL default '0000',
  "AAABMHZB" varchar(6) NOT NULL default '000000',
  "AAABMQWT" varchar(18) NOT NULL default ' ',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMHYQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMHYX" varchar(6) NOT NULL default ' ',
  "AAABMHYY" varchar(1) NOT NULL default ' ',
  "AAABMHYZ" varchar(4) NOT NULL default ' ',
  "AAABMHZA" varchar(4) NOT NULL default '0000',
  "AAABMHZB" varchar(6) NOT NULL default '000000',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMHZL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMHZS" varchar(4) NOT NULL default ' ',
  "AAABMHZT" varchar(6) NOT NULL default '000000',
  "AAABMHZU" varchar(4) NOT NULL default '0000',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  "AAABMHZY" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIAS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMRDF" varchar(13) NOT NULL default '0000000000000',
  "AAABMMTR" varchar(4) NOT NULL default ' ',
  "AAABMMTS" varchar(6) NOT NULL default '000000',
  "AAABMIBL" varchar(4) NOT NULL default '0000',
  "AAABMIBD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD"),
  KEY "AAABMIBK" ("AAABMIBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIBM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMRFB" varchar(5) NOT NULL default ' ',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMICD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMICK" varchar(4) NOT NULL default '0000',
  "AAABMICH" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMICK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMICL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMICS" varchar(4) NOT NULL default '0000',
  "AAABMICP" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMICS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMICT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMIDQ" varchar(13) NOT NULL default '0000000000000',
  "AAABMIDR" varchar(4) NOT NULL default '0000',
  "AAABMIDS" varchar(4) NOT NULL default '0000',
  "AAABMIDT" varchar(4) NOT NULL default ' ',
  "AAABMIDU" varchar(6) NOT NULL default '000000',
  "AAABMIDV" varchar(4) NOT NULL default '0000',
  "AAABMIDW" varchar(9) NOT NULL default '000000000',
  "AAABMIDX" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRIM","AAABMRDC","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMIDQ","AAABMIDR","AAABMIDS","AAABMIDT","AAABMIDU","AAABMIDV","AAABMIDW","AAABMIDX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIDY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMIEF" varchar(4) NOT NULL default ' ',
  "AAABMIEG" varchar(6) NOT NULL default '000000',
  "AAABMIEH" varchar(4) NOT NULL default '0000',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMIEL" varchar(4) NOT NULL default ' ',
  "AAABMIEM" varchar(6) NOT NULL default '000000',
  "AAABMIEN" varchar(4) NOT NULL default '0000',
  "AAABMIEO" varchar(50) NOT NULL default ' ',
  "AAABMIEP" varchar(50) NOT NULL default ' ',
  "AAABMIEQ" varchar(4) NOT NULL default '0000',
  "AAABMIER" varchar(4) NOT NULL default '0000',
  "AAABMIES" varchar(4) NOT NULL default '0000',
  "AAABMIET" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIFA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMIKD" varchar(4) NOT NULL default ' ',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMIFG" varchar(30) NOT NULL default ' ',
  "AAABMIFH" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMIKD","AAABMRGQ","AAABMRGR","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIFN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMOV" varchar(4) NOT NULL default '0000',
  "AAABMMOW" varchar(4) NOT NULL default '0000',
  "AAABMOHD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMMOV","AAABMMOW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIFX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMOU" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMMOU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIGC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMOU" varchar(4) NOT NULL default '0000',
  "AAABMOHD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMMOU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMOV" varchar(4) NOT NULL default '0000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMOV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIGQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMOV" varchar(4) NOT NULL default '0000',
  "AAABMOHD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMMOV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIGY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMIOS" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMIHC" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMIOS","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIHG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMOQ" varchar(4) NOT NULL default '0000',
  "AAABMIHK" varchar(60) NOT NULL default ' ',
  "AAABMIHL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMMOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIHP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMTL" varchar(4) NOT NULL default '0000',
  "AAABMIHT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMMTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIHX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMPU" varchar(4) NOT NULL default '0000',
  "AAABMIIB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMMPU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIIF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMJER" varchar(4) NOT NULL default '0000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMJER","AAABMRGQ","AAABMRGR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIIO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMIOS" varchar(4) NOT NULL default ' ',
  "AAABMINB" varchar(4) NOT NULL default ' ',
  "AAABMINC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMIOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIIV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMPMZ" varchar(4) NOT NULL default '0000',
  "AAABMQPU" varchar(1) NOT NULL default ' ',
  "AAABMIJA" varchar(1) NOT NULL default ' ',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMOGK","AAABMPMZ","AAABMQPU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIJG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMIKD" varchar(4) NOT NULL default ' ',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMNYU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMIKD","AAABMRGQ","AAABMRGR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIJR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMINB" varchar(4) NOT NULL default ' ',
  "AAABMIJY" varchar(4) NOT NULL default ' ',
  "AAABMIJZ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMINB","AAABMIJY","AAABMIJZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIKA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMIOS" varchar(4) NOT NULL default ' ',
  "AAABMIKD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMIOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMINB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMINB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIKL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMINB" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMIKP" varchar(30) NOT NULL default ' ',
  "AAABMIKQ" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMINB","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIKU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMIKZ" varchar(4) NOT NULL default ' ',
  "AAABMIKX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMIKZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMILA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMNYU" varchar(30) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMILF" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNYT","AAABMNYU","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMILK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMNYU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNYT","AAABMNYU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMILR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNXA" varchar(4) NOT NULL default '0000',
  "AAABMINM" varchar(13) NOT NULL default '0000000000000',
  "AAABMIMR" varchar(4) NOT NULL default '0000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMILX" varchar(4) NOT NULL default ' ',
  "AAABMILY" varchar(1) NOT NULL default ' ',
  "AAABMIOS" varchar(4) NOT NULL default ' ',
  "AAABMINN" varchar(4) NOT NULL default ' ',
  "AAABMINO" varchar(6) NOT NULL default '000000',
  "AAABMINP" varchar(4) NOT NULL default '0000',
  "AAABMIMD" varchar(9) NOT NULL default '000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMIMI" varchar(8) NOT NULL default '00000000',
  "AAABMIMJ" varchar(6) NOT NULL default '000000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMRFR" varchar(6) NOT NULL default '000000',
  "AAABMIMM" varchar(12) NOT NULL default ' ',
  "AAABMINB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNXA","AAABMINM","AAABMIMR"),
  KEY "AAABMIMS" ("AAABMRIL","AAABMRFQ","AAABMRFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIMW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMIOS" varchar(4) NOT NULL default ' ',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMINB" varchar(4) NOT NULL default ' ',
  "AAABMINC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMIOS","AAABMRGQ","AAABMRGR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMINH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMINV" varchar(8) NOT NULL default '00000000',
  "AAABMINW" varchar(1) NOT NULL default ' ',
  "AAABMIOS" varchar(4) NOT NULL default ' ',
  "AAABMINM" varchar(13) NOT NULL default '0000000000000',
  "AAABMINN" varchar(4) NOT NULL default ' ',
  "AAABMINO" varchar(6) NOT NULL default '000000',
  "AAABMINP" varchar(4) NOT NULL default '0000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMIOB" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMINV","AAABMINW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMINX" (
  "AAABMMKX" varchar(3) NOT NULL default '000',
  "AAABMIOB" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMMKX","AAABMIOB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIOC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMIOJ" varchar(1) NOT NULL default '0',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMIOG" varchar(30) NOT NULL default ' ',
  "AAABMIOH" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMIOJ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIOL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMIOS" varchar(4) NOT NULL default ' ',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMNSG" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMIOS","AAABMRGQ","AAABMRGR","AAABMNSG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIOW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMIOZ" varchar(5) NOT NULL default '00000',
  "AAABMIPA" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMRY" varchar(4) NOT NULL default '0000',
  "AAABMIPG" varchar(1) NOT NULL default ' ',
  "AAABMNPL" varchar(4) NOT NULL default '0000',
  "AAABMNPM" varchar(4) NOT NULL default '0000',
  "AAABMMQR" varchar(4) NOT NULL default '0000',
  "AAABMIQM" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMMRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIPN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMQR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMMQR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIPS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMQR" varchar(4) NOT NULL default '0000',
  "AAABMOHD" varchar(60) NOT NULL default ' ',
  "AAABMKVE" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMMQR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIQB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMIQM" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMIQM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIQG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMIQM" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOGI" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMIQM","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIQO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNPL" varchar(4) NOT NULL default '0000',
  "AAABMIQR" varchar(30) NOT NULL default ' ',
  "AAABMIQS" varchar(40) NOT NULL default ' ',
  "AAABMIQT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNPL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIQW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMNPL" varchar(4) NOT NULL default '0000',
  "AAABMOHD" varchar(60) NOT NULL default ' ',
  "AAABMKVE" varchar(15) NOT NULL default ' ',
  "AAABMIRC" varchar(30) NOT NULL default ' ',
  "AAABMIRD" varchar(30) NOT NULL default ' ',
  "AAABMIRE" varchar(30) NOT NULL default ' ',
  "AAABMIRF" varchar(30) NOT NULL default ' ',
  "AAABMIRG" varchar(30) NOT NULL default ' ',
  "AAABMIRH" varchar(30) NOT NULL default ' ',
  "AAABMIRI" varchar(30) NOT NULL default ' ',
  "AAABMIRJ" varchar(30) NOT NULL default ' ',
  "AAABMIRK" varchar(30) NOT NULL default ' ',
  "AAABMIRL" varchar(30) NOT NULL default ' ',
  "AAABMIRM" varchar(30) NOT NULL default ' ',
  "AAABMIRN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMNPL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIRR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMIUX" varchar(4) NOT NULL default '0000',
  "AAABMIRU" varchar(30) NOT NULL default ' ',
  "AAABMIRV" varchar(40) NOT NULL default ' ',
  "AAABMIRW" varchar(30) NOT NULL default ' ',
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAABMIRY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMIUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMISB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMQK" varchar(4) NOT NULL default '0000',
  "AAABMMQH" varchar(4) NOT NULL default '0000',
  "AAABMIUX" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMMQK","AAABMMQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMISJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMIWE" varchar(4) NOT NULL default '0000',
  "AAABMISM" varchar(30) NOT NULL default ' ',
  "AAABMISN" varchar(40) NOT NULL default ' ',
  "AAABMISO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMIWE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMISR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMQK" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMMQK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMISW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNPM" varchar(4) NOT NULL default '0000',
  "AAABMISZ" varchar(30) NOT NULL default ' ',
  "AAABMITA" varchar(40) NOT NULL default ' ',
  "AAABMITB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMITE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMRM" varchar(4) NOT NULL default '0000',
  "AAABMITI" varchar(60) NOT NULL default ' ',
  "AAABMITJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMMRM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMITN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMRY" varchar(4) NOT NULL default '0000',
  "AAABMITR" varchar(30) NOT NULL default ' ',
  "AAABMITS" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMMRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMITW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMRY" varchar(4) NOT NULL default '0000',
  "AAABMITZ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMMRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIUC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMTX" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMMTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIUH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMTX" varchar(4) NOT NULL default '0000',
  "AAABMOHD" varchar(30) NOT NULL default ' ',
  "AAABMIUM" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMMTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIUQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMIUX" varchar(4) NOT NULL default '0000',
  "AAABMOHD" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMIUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIUY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMQK" varchar(4) NOT NULL default '0000',
  "AAABMMQH" varchar(4) NOT NULL default '0000',
  "AAABMOHD" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMMQK","AAABMMQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIVI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMIWE" varchar(4) NOT NULL default '0000',
  "AAABMOGI" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMIWE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIVQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMQS" varchar(4) NOT NULL default '0000',
  "AAABMMQP" varchar(4) NOT NULL default '0000',
  "AAABMOGI" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMMQS","AAABMMQP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIWA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMQS" varchar(4) NOT NULL default '0000',
  "AAABMMQP" varchar(4) NOT NULL default '0000',
  "AAABMIWE" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMMQS","AAABMMQP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIWI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMQK" varchar(4) NOT NULL default '0000',
  "AAABMIWU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMMQK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIWQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMQS" varchar(4) NOT NULL default '0000',
  "AAABMIWU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMMQS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIWY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMQS" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMMQS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIXD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMNPM" varchar(4) NOT NULL default '0000',
  "AAABMOHD" varchar(60) NOT NULL default ' ',
  "AAABMKVE" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMNPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIXM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMODA" varchar(4) NOT NULL default ' ',
  "AAABMIXQ" varchar(30) NOT NULL default ' ',
  "AAABMIXR" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMODA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIXV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNUX" varchar(4) NOT NULL default '0000',
  "AAABMMRY" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMNUX","AAABMMRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIYC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNUX" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMNUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIYH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMNEB" varchar(4) NOT NULL default '0000',
  "AAABMIYL" varchar(30) NOT NULL default ' ',
  "AAABMIYM" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMNEB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIYQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMNUX" varchar(4) NOT NULL default '0000',
  "AAABMIYU" varchar(30) NOT NULL default ' ',
  "AAABMIYV" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMNUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIYZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMOCV" varchar(8) NOT NULL default ' ',
  "AAABMRDD" varchar(1) NOT NULL default ' ',
  "AAABMODA" varchar(4) NOT NULL default ' ',
  "AAABMPQS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMRGR","AAABMOCV","AAABMRDD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIZM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMTX" varchar(4) NOT NULL default '0000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMIZS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMIZV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMTX" varchar(4) NOT NULL default '0000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMMSH" varchar(30) NOT NULL default ' ',
  "AAABMMSD" varchar(30) NOT NULL default ' ',
  "AAABMMTR" varchar(4) NOT NULL default ' ',
  "AAABMMTS" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMMTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMJAG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMTX" varchar(4) NOT NULL default '0000',
  "AAABMJCV" varchar(30) NOT NULL default ' ',
  "AAABMMTR" varchar(4) NOT NULL default ' ',
  "AAABMMTS" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMMTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMJAO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMTX" varchar(4) NOT NULL default '0000',
  "AAABMJBA" varchar(8) NOT NULL default ' ',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMPRD" varchar(8) NOT NULL default ' ',
  "AAABMPRC" varchar(4) NOT NULL default '0000',
  "AAABMPRB" varchar(1) NOT NULL default ' ',
  "AAABMPRE" varchar(6) NOT NULL default '000000',
  "AAABMJAX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMTX","AAABMJBA","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMJBC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMTX" varchar(4) NOT NULL default '0000',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMJCR" varchar(1) NOT NULL default ' ',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMMTX","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMJBM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMTX" varchar(4) NOT NULL default '0000',
  "AAABMJCJ" varchar(30) NOT NULL default ' ',
  "AAABMJCK" varchar(1) NOT NULL default ' ',
  "AAABMJCL" varchar(3) NOT NULL default '000',
  "AAABMJCM" varchar(1) NOT NULL default ' ',
  "AAABMRDD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMJBW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMTX" varchar(4) NOT NULL default '0000',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMMTX","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMJCF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMTX" varchar(4) NOT NULL default '0000',
  "AAABMJCR" varchar(1) NOT NULL default ' ',
  "AAABMJCJ" varchar(30) NOT NULL default ' ',
  "AAABMJCK" varchar(1) NOT NULL default ' ',
  "AAABMJCL" varchar(3) NOT NULL default '000',
  "AAABMJCM" varchar(1) NOT NULL default ' ',
  "AAABMRDD" varchar(1) NOT NULL default ' ',
  "AAABMNJF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMTX","AAABMJCR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMJCS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMTX" varchar(4) NOT NULL default '0000',
  "AAABMJCV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMJCY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMCV" varchar(30) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMJDC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMCV","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMJDG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMCV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMJDL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIZ" varchar(4) NOT NULL default '0000',
  "AAABMOMJ" varchar(4) NOT NULL default '0000',
  "AAABMQJB" varchar(4) NOT NULL default '0000',
  "AAABMOMK" varchar(4) NOT NULL default '0000',
  "AAABMQJD" varchar(4) NOT NULL default '0000',
  "AAABMMZX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQIZ","AAABMOMJ","AAABMQJB","AAABMOMK","AAABMQJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMJDZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMJER" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMJER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMJEE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKWZ" varchar(4) NOT NULL default '0000',
  "AAABMJER" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMKWZ","AAABMJER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMJEL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMJER" varchar(4) NOT NULL default '0000',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMJER","AAABMNJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMJET" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMKLH" varchar(8) NOT NULL default '00000000',
  "AAABMKKY" varchar(8) NOT NULL default '00000000',
  "AAABMJMB" varchar(5) NOT NULL default ' ',
  "AAABMJLM" varchar(4) NOT NULL default ' ',
  "AAABMJWW" varchar(10) NOT NULL default ' ',
  "AAABMQQF" varchar(4) NOT NULL default ' ',
  "AAABMQQG" varchar(2) NOT NULL default ' ',
  "AAABMQQH" varchar(2) NOT NULL default ' ',
  "AAABMJWX" varchar(10) NOT NULL default ' ',
  "AAABMQOS" varchar(4) NOT NULL default ' ',
  "AAABMQOT" varchar(2) NOT NULL default ' ',
  "AAABMQOU" varchar(2) NOT NULL default ' ',
  "AAABMQMR" varchar(10) NOT NULL default ' ',
  "AAABMRDE" varchar(10) NOT NULL default ' ',
  "AAABMKDW" varchar(4) NOT NULL default ' ',
  "AAABMQMY" varchar(10) NOT NULL default ' ',
  "AAABMQMZ" varchar(20) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  "AAABMQNF" varchar(2) NOT NULL default ' ',
  "AAABMQNG" varchar(2) NOT NULL default ' ',
  "AAABMQNH" varchar(30) NOT NULL default ' ',
  "AAABMQNI" varchar(10) NOT NULL default ' ',
  "AAABMQNJ" varchar(30) NOT NULL default ' ',
  "AAABMQNL" varchar(10) NOT NULL default ' ',
  "AAABMQNM" varchar(30) NOT NULL default ' ',
  "AAABMQNO" varchar(10) NOT NULL default ' ',
  "AAABMQNP" varchar(30) NOT NULL default ' ',
  "AAABMQNR" varchar(10) NOT NULL default ' ',
  "AAABMQNS" varchar(30) NOT NULL default ' ',
  "AAABMQNU" varchar(10) NOT NULL default ' ',
  "AAABMQNV" varchar(30) NOT NULL default ' ',
  "AAABMJGC" varchar(1) NOT NULL default ' ',
  "AAABMJGD" decimal(15,2) NOT NULL default '0.00',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABMJGF" varchar(5) NOT NULL default ' ',
  "AAABMJGG" varchar(1) NOT NULL default '0',
  "AAABMKLA" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMKLH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMJGL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOTC" varchar(4) NOT NULL default ' ',
  "AAABMOTD" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOTC","AAABMOTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMJGS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOTC" varchar(4) NOT NULL default ' ',
  "AAABMOTD" varchar(20) NOT NULL default ' ',
  "AAABMJGX" varchar(30) NOT NULL default ' ',
  "AAABMJGY" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMOTC","AAABMOTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMJHD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOTC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOTC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMJHI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOTC" varchar(4) NOT NULL default ' ',
  "AAABMJHM" varchar(30) NOT NULL default ' ',
  "AAABMJHN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMOTC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMJHR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMKJP" varchar(1) NOT NULL default ' ',
  "AAABMJIV" varchar(4) NOT NULL default '0000',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAABMKLH" varchar(8) NOT NULL default '00000000',
  "AAABMKKY" varchar(8) NOT NULL default '00000000',
  "AAABMQOS" varchar(4) NOT NULL default ' ',
  "AAABMQOT" varchar(2) NOT NULL default ' ',
  "AAABMQOU" varchar(2) NOT NULL default ' ',
  "AAABMQMR" varchar(10) NOT NULL default ' ',
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABMKFI" varchar(10) NOT NULL default ' ',
  "AAABMJII" varchar(4) NOT NULL default ' ',
  "AAABMPTL" varchar(18) NOT NULL default ' ',
  "AAABMKFA" varchar(3) NOT NULL default ' ',
  "AAABMQPA" varchar(1) NOT NULL default ' ',
  "AAABMQPB" varchar(1) NOT NULL default ' ',
  "AAABMQPC" varchar(1) NOT NULL default ' ',
  "AAABMKLA" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMKJP","AAABMJIV","AAABMLPO","AAABMKLH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMJIY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMKFC" varchar(1) NOT NULL default ' ',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMJKK" varchar(8) NOT NULL default '00000000',
  "AAABMJJG" varchar(8) NOT NULL default '00000000',
  "AAABMPUD" varchar(16) NOT NULL default '0000000000000000',
  "AAABMKFG" varchar(10) NOT NULL default ' ',
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABMKFI" varchar(10) NOT NULL default ' ',
  "AAABMPTL" varchar(18) NOT NULL default ' ',
  "AAABMKEN" varchar(1) NOT NULL default ' ',
  "AAABMJJN" decimal(15,5) NOT NULL default '0.00000',
  "AAABMJJO" decimal(15,5) NOT NULL default '0.00000',
  "AAABMJJP" decimal(15,5) NOT NULL default '0.00000',
  "AAABMJJQ" decimal(15,5) NOT NULL default '0.00000',
  "AAABMKFA" varchar(3) NOT NULL default ' ',
  "AAABMJJS" decimal(16,2) NOT NULL default '0.00',
  "AAABMJJT" decimal(16,2) NOT NULL default '0.00',
  "AAABMJJU" decimal(16,2) NOT NULL default '0.00',
  "AAABMJJV" decimal(16,2) NOT NULL default '0.00',
  "AAABMKFB" varchar(5) NOT NULL default ' ',
  "AAABMJJX" decimal(16,2) NOT NULL default '0.00',
  "AAABMJJY" decimal(16,2) NOT NULL default '0.00',
  "AAABMJJZ" decimal(16,2) NOT NULL default '0.00',
  "AAABMJKA" decimal(16,2) NOT NULL default '0.00',
  "AAABMJKB" decimal(16,2) NOT NULL default '0.00',
  "AAABMJKC" decimal(15,5) NOT NULL default '0.00000',
  "AAABMKLA" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMKFC","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMJKK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMJKL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOTC" varchar(4) NOT NULL default ' ',
  "AAABMOTD" varchar(20) NOT NULL default ' ',
  "AAABMKGH" varchar(2) NOT NULL default '00',
  "AAABMKGI" varchar(4) NOT NULL default '0000',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABMKFU" varchar(14) NOT NULL default ' ',
  "AAABMQXM" decimal(13,0) NOT NULL default '0',
  "AAABMQXL" decimal(13,0) NOT NULL default '0',
  "AAABMKFX" decimal(13,0) NOT NULL default '0',
  "AAABMKFY" varchar(6) NOT NULL default ' ',
  "AAABMRDI" varchar(8) NOT NULL default '00000000',
  "AAABMRDJ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMOTC","AAABMOTD","AAABMKGH","AAABMKGI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMJLE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMKLH" varchar(8) NOT NULL default '00000000',
  "AAABMKKY" varchar(8) NOT NULL default '00000000',
  "AAABMKDE" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMQPU" varchar(1) NOT NULL default ' ',
  "AAABMJLM" varchar(4) NOT NULL default ' ',
  "AAABMKDG" varchar(1) NOT NULL default ' ',
  "AAABMJLO" varchar(5) NOT NULL default '00000',
  "AAABMJLP" varchar(5) NOT NULL default '00000',
  "AAABMJLQ" varchar(5) NOT NULL default '00000',
  "AAABMJLR" varchar(5) NOT NULL default '00000',
  "AAABMJLS" varchar(5) NOT NULL default '00000',
  "AAABMJLT" varchar(1) NOT NULL default ' ',
  "AAABMJLU" varchar(18) NOT NULL default ' ',
  "AAABMJLV" varchar(8) NOT NULL default '00000000',
  "AAABMJLW" varchar(8) NOT NULL default '00000000',
  "AAABMJLX" varchar(8) NOT NULL default '00000000',
  "AAABMJLY" varchar(8) NOT NULL default '00000000',
  "AAABMJLZ" varchar(3) NOT NULL default '000',
  "AAABMJMA" varchar(1) NOT NULL default ' ',
  "AAABMJMB" varchar(5) NOT NULL default ' ',
  "AAABMJWW" varchar(10) NOT NULL default ' ',
  "AAABMQQF" varchar(4) NOT NULL default ' ',
  "AAABMQQG" varchar(2) NOT NULL default ' ',
  "AAABMQQH" varchar(2) NOT NULL default ' ',
  "AAABMJWX" varchar(10) NOT NULL default ' ',
  "AAABMQOS" varchar(4) NOT NULL default ' ',
  "AAABMQOT" varchar(2) NOT NULL default ' ',
  "AAABMQOU" varchar(2) NOT NULL default ' ',
  "AAABMRDE" varchar(10) NOT NULL default ' ',
  "AAABMKDW" varchar(4) NOT NULL default ' ',
  "AAABMJMM" varchar(50) NOT NULL default ' ',
  "AAABMJMN" varchar(50) NOT NULL default ' ',
  "AAABMJMO" varchar(10) NOT NULL default ' ',
  "AAABMJMP" varchar(10) NOT NULL default ' ',
  "AAABMJMQ" varchar(10) NOT NULL default ' ',
  "AAABMJMR" varchar(50) NOT NULL default ' ',
  "AAABMJMS" varchar(50) NOT NULL default ' ',
  "AAABMJMT" varchar(50) NOT NULL default ' ',
  "AAABMJMU" varchar(13) NOT NULL default '0000000000000',
  "AAABMJMV" varchar(13) NOT NULL default '0000000000000',
  "AAABMJMW" varchar(13) NOT NULL default '0000000000000',
  "AAABMJMX" varchar(13) NOT NULL default '0000000000000',
  "AAABMJMY" varchar(8) NOT NULL default '00000000',
  "AAABMJMZ" varchar(8) NOT NULL default '00000000',
  "AAABMJNA" decimal(15,5) NOT NULL default '0.00000',
  "AAABMJNB" decimal(15,5) NOT NULL default '0.00000',
  "AAABMJNC" decimal(15,5) NOT NULL default '0.00000',
  "AAABMJND" varchar(3) NOT NULL default ' ',
  "AAABMJNE" varchar(18) NOT NULL default ' ',
  "AAABMJNF" varchar(6) NOT NULL default ' ',
  "AAABMJNG" varchar(6) NOT NULL default ' ',
  "AAABMJNH" decimal(17,5) NOT NULL default '0.00000',
  "AAABMJNI" decimal(17,5) NOT NULL default '0.00000',
  "AAABMJNJ" decimal(17,5) NOT NULL default '0.00000',
  "AAABMJNK" varchar(5) NOT NULL default ' ',
  "AAABMJNL" decimal(7,3) NOT NULL default '0.000',
  "AAABMJNM" decimal(7,3) NOT NULL default '0.000',
  "AAABMJNN" decimal(7,3) NOT NULL default '0.000',
  "AAABMJNO" decimal(16,2) NOT NULL default '0.00',
  "AAABMJNP" decimal(16,2) NOT NULL default '0.00',
  "AAABMJNQ" decimal(16,2) NOT NULL default '0.00',
  "AAABMJNR" varchar(5) NOT NULL default ' ',
  "AAABMJNS" varchar(18) NOT NULL default ' ',
  "AAABMJNT" varchar(6) NOT NULL default ' ',
  "AAABMJNU" varchar(6) NOT NULL default ' ',
  "AAABMJNV" decimal(7,3) NOT NULL default '0.000',
  "AAABMJNW" decimal(7,3) NOT NULL default '0.000',
  "AAABMJNX" decimal(7,3) NOT NULL default '0.000',
  "AAABMJNY" decimal(7,3) NOT NULL default '0.000',
  "AAABMJNZ" decimal(7,3) NOT NULL default '0.000',
  "AAABMJOA" decimal(7,3) NOT NULL default '0.000',
  "AAABMJOB" varchar(8) NOT NULL default '00000000',
  "AAABMJOC" varchar(8) NOT NULL default '00000000',
  "AAABMJOD" varchar(3) NOT NULL default '000',
  "AAABMJOE" varchar(3) NOT NULL default '000',
  "AAABMJOF" varchar(1) NOT NULL default ' ',
  "AAABMJOG" varchar(8) NOT NULL default '00000000',
  "AAABMJOH" varchar(3) NOT NULL default '000',
  "AAABMJOI" varchar(3) NOT NULL default '000',
  "AAABMJOJ" varchar(3) NOT NULL default '000',
  "AAABMJOK" varchar(8) NOT NULL default '00000000',
  "AAABMJOL" varchar(8) NOT NULL default '00000000',
  "AAABMJOM" varchar(3) NOT NULL default '000',
  "AAABMJON" varchar(3) NOT NULL default '000',
  "AAABMJOO" decimal(15,5) NOT NULL default '0.00000',
  "AAABMJOP" varchar(3) NOT NULL default ' ',
  "AAABMJOQ" varchar(3) NOT NULL default '000',
  "AAABMJOR" decimal(16,2) NOT NULL default '0.00',
  "AAABMJOS" varchar(1) NOT NULL default ' ',
  "AAABMJOT" varchar(1) NOT NULL default ' ',
  "AAABMKLA" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMKLH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMJOY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMKLH" varchar(8) NOT NULL default '00000000',
  "AAABMKKY" varchar(8) NOT NULL default '00000000',
  "AAABMQQY" decimal(16,2) NOT NULL default '0.00',
  "AAABMJPE" varchar(5) NOT NULL default ' ',
  "AAABMQQZ" decimal(16,2) NOT NULL default '0.00',
  "AAABMJPG" varchar(5) NOT NULL default ' ',
  "AAABMJPH" varchar(8) NOT NULL default '00000000',
  "AAABMQRA" decimal(16,2) NOT NULL default '0.00',
  "AAABMJPJ" varchar(5) NOT NULL default ' ',
  "AAABMJPK" varchar(4) NOT NULL default ' ',
  "AAABMQRB" decimal(16,2) NOT NULL default '0.00',
  "AAABMJPM" varchar(5) NOT NULL default ' ',
  "AAABMJPN" varchar(4) NOT NULL default ' ',
  "AAABMQRC" decimal(16,2) NOT NULL default '0.00',
  "AAABMJPP" varchar(5) NOT NULL default ' ',
  "AAABMJPQ" decimal(7,3) NOT NULL default '0.000',
  "AAABMJPR" varchar(10) NOT NULL default ' ',
  "AAABMJPS" varchar(6) NOT NULL default ' ',
  "AAABMJPT" decimal(7,3) NOT NULL default '0.000',
  "AAABMJPU" decimal(7,3) NOT NULL default '0.000',
  "AAABMJPV" decimal(7,3) NOT NULL default '0.000',
  "AAABMJPW" varchar(4) NOT NULL default ' ',
  "AAABMJPX" decimal(16,2) NOT NULL default '0.00',
  "AAABMJPY" varchar(5) NOT NULL default ' ',
  "AAABMJPZ" varchar(4) NOT NULL default ' ',
  "AAABMJQA" decimal(17,5) NOT NULL default '0.00000',
  "AAABMJQB" varchar(5) NOT NULL default ' ',
  "AAABMJQC" varchar(3) NOT NULL default ' ',
  "AAABMJQD" varchar(4) NOT NULL default ' ',
  "AAABMJQE" decimal(17,5) NOT NULL default '0.00000',
  "AAABMJQF" varchar(5) NOT NULL default ' ',
  "AAABMJQG" varchar(3) NOT NULL default ' ',
  "AAABMJQH" varchar(4) NOT NULL default ' ',
  "AAABMJQI" decimal(17,5) NOT NULL default '0.00000',
  "AAABMJQJ" varchar(5) NOT NULL default ' ',
  "AAABMJQK" varchar(3) NOT NULL default ' ',
  "AAABMJQL" varchar(4) NOT NULL default ' ',
  "AAABMJQM" decimal(17,5) NOT NULL default '0.00000',
  "AAABMJQN" varchar(5) NOT NULL default ' ',
  "AAABMJQO" varchar(3) NOT NULL default ' ',
  "AAABMJQP" varchar(4) NOT NULL default ' ',
  "AAABMJQQ" decimal(17,5) NOT NULL default '0.00000',
  "AAABMJQR" varchar(5) NOT NULL default ' ',
  "AAABMJQS" varchar(3) NOT NULL default ' ',
  "AAABMJQT" varchar(4) NOT NULL default ' ',
  "AAABMJQU" decimal(17,5) NOT NULL default '0.00000',
  "AAABMJQV" varchar(5) NOT NULL default ' ',
  "AAABMJQW" varchar(3) NOT NULL default ' ',
  "AAABMJQX" varchar(4) NOT NULL default ' ',
  "AAABMJQY" decimal(17,5) NOT NULL default '0.00000',
  "AAABMJQZ" varchar(5) NOT NULL default ' ',
  "AAABMJRA" varchar(3) NOT NULL default ' ',
  "AAABMJRB" varchar(4) NOT NULL default ' ',
  "AAABMJRC" decimal(17,5) NOT NULL default '0.00000',
  "AAABMJRD" varchar(5) NOT NULL default ' ',
  "AAABMJRE" varchar(3) NOT NULL default ' ',
  "AAABMJRF" varchar(4) NOT NULL default ' ',
  "AAABMJRG" decimal(7,3) NOT NULL default '0.000',
  "AAABMJRH" varchar(4) NOT NULL default ' ',
  "AAABMJRI" decimal(7,3) NOT NULL default '0.000',
  "AAABMJRJ" varchar(4) NOT NULL default ' ',
  "AAABMJRK" decimal(7,3) NOT NULL default '0.000',
  "AAABMJRL" varchar(4) NOT NULL default ' ',
  "AAABMJRM" decimal(7,3) NOT NULL default '0.000',
  "AAABMJRN" varchar(4) NOT NULL default ' ',
  "AAABMJRO" decimal(7,3) NOT NULL default '0.000',
  "AAABMJRP" varchar(4) NOT NULL default ' ',
  "AAABMQSU" decimal(16,2) NOT NULL default '0.00',
  "AAABMJRR" varchar(5) NOT NULL default ' ',
  "AAABMJRS" varchar(4) NOT NULL default ' ',
  "AAABMJRT" varchar(4) NOT NULL default ' ',
  "AAABMJRU" varchar(5) NOT NULL default ' ',
  "AAABMJRV" decimal(16,2) NOT NULL default '0.00',
  "AAABMJRW" decimal(16,2) NOT NULL default '0.00',
  "AAABMJRX" decimal(16,2) NOT NULL default '0.00',
  "AAABMJRY" decimal(16,2) NOT NULL default '0.00',
  "AAABMJRZ" decimal(16,2) NOT NULL default '0.00',
  "AAABMJSA" decimal(16,2) NOT NULL default '0.00',
  "AAABMJSB" decimal(16,2) NOT NULL default '0.00',
  "AAABMJSC" decimal(16,2) NOT NULL default '0.00',
  "AAABMJSD" decimal(16,2) NOT NULL default '0.00',
  "AAABMJSE" decimal(16,2) NOT NULL default '0.00',
  "AAABMJSF" decimal(16,2) NOT NULL default '0.00',
  "AAABMJSG" decimal(16,2) NOT NULL default '0.00',
  "AAABMJSH" decimal(16,2) NOT NULL default '0.00',
  "AAABMJSI" decimal(16,2) NOT NULL default '0.00',
  "AAABMJSJ" decimal(16,2) NOT NULL default '0.00',
  "AAABMJSK" varchar(13) NOT NULL default ' ',
  "AAABMJSL" varchar(4) NOT NULL default ' ',
  "AAABMQSS" decimal(16,2) NOT NULL default '0.00',
  "AAABMJSN" varchar(5) NOT NULL default ' ',
  "AAABMJSO" varchar(4) NOT NULL default ' ',
  "AAABMJSP" decimal(16,2) NOT NULL default '0.00',
  "AAABMJSQ" varchar(5) NOT NULL default ' ',
  "AAABMJSR" varchar(4) NOT NULL default ' ',
  "AAABMJSS" decimal(16,2) NOT NULL default '0.00',
  "AAABMJST" varchar(5) NOT NULL default ' ',
  "AAABMJSU" varchar(4) NOT NULL default ' ',
  "AAABMJSV" decimal(16,2) NOT NULL default '0.00',
  "AAABMJSW" varchar(5) NOT NULL default ' ',
  "AAABMJSX" varchar(4) NOT NULL default ' ',
  "AAABMJSY" decimal(16,2) NOT NULL default '0.00',
  "AAABMJSZ" decimal(16,2) NOT NULL default '0.00',
  "AAABMJTA" varchar(5) NOT NULL default ' ',
  "AAABMJTB" varchar(4) NOT NULL default ' ',
  "AAABMJTC" decimal(17,5) NOT NULL default '0.00000',
  "AAABMJTD" varchar(5) NOT NULL default ' ',
  "AAABMJTE" varchar(3) NOT NULL default ' ',
  "AAABMJTF" varchar(4) NOT NULL default ' ',
  "AAABMJTG" decimal(7,3) NOT NULL default '0.000',
  "AAABMJTH" varchar(4) NOT NULL default ' ',
  "AAABMKAP" decimal(16,2) NOT NULL default '0.00',
  "AAABMKAR" decimal(16,2) NOT NULL default '0.00',
  "AAABMKAQ" varchar(5) NOT NULL default ' ',
  "AAABMKAD" varchar(4) NOT NULL default '0000',
  "AAABMKAE" varchar(4) NOT NULL default '0000',
  "AAABMKAF" decimal(16,2) NOT NULL default '0.00',
  "AAABMKAG" varchar(5) NOT NULL default ' ',
  "AAABMJTP" varchar(4) NOT NULL default '0000',
  "AAABMJTQ" varchar(4) NOT NULL default '0000',
  "AAABMJTR" decimal(16,2) NOT NULL default '0.00',
  "AAABMJTS" varchar(5) NOT NULL default ' ',
  "AAABMJTT" varchar(4) NOT NULL default '0000',
  "AAABMJTU" varchar(4) NOT NULL default '0000',
  "AAABMJTV" decimal(16,2) NOT NULL default '0.00',
  "AAABMJTW" varchar(5) NOT NULL default ' ',
  "AAABMJTX" varchar(4) NOT NULL default '0000',
  "AAABMJTY" varchar(4) NOT NULL default '0000',
  "AAABMJTZ" decimal(16,2) NOT NULL default '0.00',
  "AAABMJUA" varchar(5) NOT NULL default ' ',
  "AAABMJUB" varchar(4) NOT NULL default '0000',
  "AAABMJUC" varchar(4) NOT NULL default '0000',
  "AAABMJUD" decimal(16,2) NOT NULL default '0.00',
  "AAABMJUE" varchar(5) NOT NULL default ' ',
  "AAABMKLA" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMKLH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMJUJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMKLH" varchar(8) NOT NULL default '00000000',
  "AAABMKKY" varchar(8) NOT NULL default '00000000',
  "AAABMJUO" decimal(16,2) NOT NULL default '0.00',
  "AAABMJUP" varchar(5) NOT NULL default ' ',
  "AAABMJUQ" decimal(16,2) NOT NULL default '0.00',
  "AAABMJUR" varchar(5) NOT NULL default ' ',
  "AAABMJUS" decimal(16,2) NOT NULL default '0.00',
  "AAABMJUT" varchar(5) NOT NULL default ' ',
  "AAABMJUU" decimal(16,2) NOT NULL default '0.00',
  "AAABMJUV" varchar(5) NOT NULL default ' ',
  "AAABMJUW" decimal(16,2) NOT NULL default '0.00',
  "AAABMJUX" varchar(5) NOT NULL default ' ',
  "AAABMJUY" varchar(8) NOT NULL default '00000000',
  "AAABMJUZ" varchar(8) NOT NULL default '00000000',
  "AAABMJVA" varchar(8) NOT NULL default '00000000',
  "AAABMJVB" varchar(8) NOT NULL default '00000000',
  "AAABMJVC" varchar(8) NOT NULL default '00000000',
  "AAABMJVD" varchar(30) NOT NULL default ' ',
  "AAABMJVE" varchar(30) NOT NULL default ' ',
  "AAABMJVF" varchar(30) NOT NULL default ' ',
  "AAABMJVG" varchar(30) NOT NULL default ' ',
  "AAABMJVH" varchar(30) NOT NULL default ' ',
  "AAABMKLA" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMKLH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMJVM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOTC" varchar(4) NOT NULL default ' ',
  "AAABMOTD" varchar(20) NOT NULL default ' ',
  "AAABMKGH" varchar(2) NOT NULL default '00',
  "AAABMKGI" varchar(4) NOT NULL default '0000',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABMKFT" varchar(4) NOT NULL default ' ',
  "AAABMKGB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOTC","AAABMOTD","AAABMKGH","AAABMKGI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMJWA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOTC" varchar(4) NOT NULL default ' ',
  "AAABMOTD" varchar(20) NOT NULL default ' ',
  "AAABMKGH" varchar(2) NOT NULL default '00',
  "AAABMJWN" varchar(30) NOT NULL default ' ',
  "AAABMJWG" varchar(1) NOT NULL default ' ',
  "AAABMJWH" varchar(132) NOT NULL default ' ',
  "AAABMJWI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOTC","AAABMOTD","AAABMKGH","AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMJWO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMJXH" varchar(13) NOT NULL default '0000000000000',
  "AAABMJXI" varchar(6) NOT NULL default '000000',
  "AAABMJXJ" varchar(1) NOT NULL default ' ',
  "AAABMKLH" varchar(8) NOT NULL default '00000000',
  "AAABMKKY" varchar(8) NOT NULL default '00000000',
  "AAABMJWW" varchar(10) NOT NULL default ' ',
  "AAABMJWX" varchar(10) NOT NULL default ' ',
  "AAABMKFI" varchar(10) NOT NULL default ' ',
  "AAABMJWZ" varchar(10) NOT NULL default ' ',
  "AAABMJXA" varchar(10) NOT NULL default ' ',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMKLA" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMJXH","AAABMJXI","AAABMJXJ","AAABMKLH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMJXL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABMJYC" varchar(2) NOT NULL default ' ',
  "AAABMJYD" varchar(10) NOT NULL default ' ',
  "AAABMJYE" varchar(8) NOT NULL default '00000000',
  "AAABMJXR" varchar(8) NOT NULL default '00000000',
  "AAABMJXS" varchar(2) NOT NULL default ' ',
  "AAABMJXT" varchar(2) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMJXX" varchar(35) NOT NULL default ' ',
  "AAABMJXY" varchar(50) NOT NULL default ' ',
  "AAABMKLA" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIN","AAABMJYC","AAABMJYD","AAABMJYE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMJYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAABMJYT" varchar(8) NOT NULL default '00000000',
  "AAABMJYK" varchar(25) NOT NULL default ' ',
  "AAABMJYL" decimal(13,3) NOT NULL default '0.000',
  "AAABMJYM" varchar(3) NOT NULL default ' ',
  "AAABMJYN" decimal(12,2) NOT NULL default '0.00',
  "AAABMJYO" varchar(5) NOT NULL default ' ',
  "AAABMKLA" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIN","AAABMLPO","AAABMJYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMJYU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABMJYX" varchar(50) NOT NULL default ' ',
  "AAABMJYY" varchar(15) NOT NULL default ' ',
  "AAABMJYZ" varchar(10) NOT NULL default ' ',
  "AAABMJZA" varchar(4) NOT NULL default ' ',
  "AAABMJZB" varchar(12) NOT NULL default ' ',
  "AAABMJZC" varchar(4) NOT NULL default ' ',
  "AAABMJZD" varchar(4) NOT NULL default '0000',
  "AAABMJZE" varchar(4) NOT NULL default '0000',
  "AAABMJZF" varchar(4) NOT NULL default '0000',
  "AAABMJZG" varchar(4) NOT NULL default '0000',
  "AAABMJZH" varchar(4) NOT NULL default '0000',
  "AAABMJZI" varchar(4) NOT NULL default '0000',
  "AAABMJZJ" varchar(4) NOT NULL default '0000',
  "AAABMJZK" varchar(4) NOT NULL default '0000',
  "AAABMJZL" varchar(4) NOT NULL default '0000',
  "AAABMJZM" varchar(4) NOT NULL default '0000',
  "AAABMJZN" varchar(4) NOT NULL default '0000',
  "AAABMJZO" varchar(4) NOT NULL default '0000',
  "AAABMJZP" varchar(4) NOT NULL default '0000',
  "AAABMJZQ" varchar(4) NOT NULL default '0000',
  "AAABMJZR" varchar(4) NOT NULL default '0000',
  "AAABMKLA" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMJZV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMKLH" varchar(8) NOT NULL default '00000000',
  "AAABMKKY" varchar(8) NOT NULL default '00000000',
  "AAABMKAD" varchar(4) NOT NULL default '0000',
  "AAABMKAE" varchar(4) NOT NULL default '0000',
  "AAABMKAF" decimal(16,2) NOT NULL default '0.00',
  "AAABMKAG" varchar(5) NOT NULL default ' ',
  "AAABMKAH" decimal(15,5) NOT NULL default '0.00000',
  "AAABMKAI" varchar(3) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABMQVW" varchar(18) NOT NULL default ' ',
  "AAABMKAN" varchar(8) NOT NULL default '00000000',
  "AAABMKAO" varchar(8) NOT NULL default '00000000',
  "AAABMKAP" decimal(16,2) NOT NULL default '0.00',
  "AAABMKAQ" varchar(5) NOT NULL default ' ',
  "AAABMKAR" decimal(16,2) NOT NULL default '0.00',
  "AAABMKAS" varchar(8) NOT NULL default '00000000',
  "AAABMKAT" varchar(8) NOT NULL default '00000000',
  "AAABMKAU" varchar(8) NOT NULL default '00000000',
  "AAABMKLV" varchar(4) NOT NULL default '0000',
  "AAABMKAW" decimal(13,2) NOT NULL default '0.00',
  "AAABMKAX" varchar(5) NOT NULL default ' ',
  "AAABMKAY" decimal(13,2) NOT NULL default '0.00',
  "AAABMKAZ" varchar(5) NOT NULL default ' ',
  "AAABMKBA" decimal(13,2) NOT NULL default '0.00',
  "AAABMKBB" varchar(5) NOT NULL default ' ',
  "AAABMKBC" varchar(8) NOT NULL default '00000000',
  "AAABMKLA" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMKLH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKBK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOTC" varchar(4) NOT NULL default ' ',
  "AAABMOTD" varchar(20) NOT NULL default ' ',
  "AAABMKCK" varchar(15) NOT NULL default ' ',
  "AAABMKDE" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMKCN" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMKCP" varchar(4) NOT NULL default '0000',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMORN" varchar(15) NOT NULL default ' ',
  "AAABMORO" varchar(15) NOT NULL default ' ',
  "AAABMORP" varchar(15) NOT NULL default ' ',
  "AAABMORQ" varchar(15) NOT NULL default ' ',
  "AAABMORR" varchar(15) NOT NULL default ' ',
  "AAABMORS" varchar(30) NOT NULL default ' ',
  "AAABMKCB" varchar(1) NOT NULL default ' ',
  "AAABMKCC" varchar(1) NOT NULL default ' ',
  "AAABMKCD" varchar(1) NOT NULL default ' ',
  "AAABMKCE" decimal(1,0) NOT NULL default '0',
  "AAABMKCF" varchar(1) NOT NULL default ' ',
  "AAABMKCG" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOTC","AAABMOTD","AAABMKCK","AAABMKDE","AAABMRDC","AAABMKCN","AAABMRGR","AAABMKCP","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKCR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOTC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKCV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMKDT" varchar(4) NOT NULL default '0000',
  "AAABMKFB" varchar(5) NOT NULL default ' ',
  "AAABMRDE" varchar(10) NOT NULL default ' ',
  "AAABMKDW" varchar(4) NOT NULL default ' ',
  "AAABMKLH" varchar(8) NOT NULL default '00000000',
  "AAABMKKY" varchar(8) NOT NULL default '00000000',
  "AAABMKDE" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMKDG" varchar(1) NOT NULL default ' ',
  "AAABMRDL" varchar(4) NOT NULL default '0000',
  "AAABMRDM" varchar(4) NOT NULL default '0000',
  "AAABMRDN" varchar(4) NOT NULL default '0000',
  "AAABMRDO" varchar(4) NOT NULL default '0000',
  "AAABMRDG" varchar(8) NOT NULL default '00000000',
  "AAABMRDH" varchar(8) NOT NULL default '00000000',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAABMRDK" varchar(30) NOT NULL default ' ',
  "AAABMKDP" decimal(13,2) NOT NULL default '0.00',
  "AAABMKLA" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMKDT","AAABMKFB","AAABMRDE","AAABMKDW","AAABMKLH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKDY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMKEY" varchar(7) NOT NULL default ' ',
  "AAABMKEZ" varchar(7) NOT NULL default ' ',
  "AAABMKFA" varchar(3) NOT NULL default ' ',
  "AAABMKFB" varchar(5) NOT NULL default ' ',
  "AAABMKFC" varchar(1) NOT NULL default ' ',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMKFG" varchar(10) NOT NULL default ' ',
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABMKFI" varchar(10) NOT NULL default ' ',
  "AAABMPTL" varchar(18) NOT NULL default ' ',
  "AAABMKEN" varchar(1) NOT NULL default ' ',
  "AAABMKEO" decimal(15,5) NOT NULL default '0.00000',
  "AAABMKEP" decimal(16,2) NOT NULL default '0.00',
  "AAABMRDG" varchar(8) NOT NULL default '00000000',
  "AAABMRDH" varchar(8) NOT NULL default '00000000',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAABMKET" varchar(8) NOT NULL default '00000000',
  "AAABMKEU" varchar(2) NOT NULL default '00',
  "AAABMKLA" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMKEY","AAABMKEZ","AAABMKFA","AAABMKFB","AAABMKFC","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMKFG","AAABMKFH","AAABMKFI","AAABMPTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKFK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKGP" varchar(30) NOT NULL default ' ',
  "AAABMKGE" varchar(1) NOT NULL default ' ',
  "AAABMOTC" varchar(4) NOT NULL default ' ',
  "AAABMOTD" varchar(20) NOT NULL default ' ',
  "AAABMKGH" varchar(2) NOT NULL default '00',
  "AAABMKGI" varchar(4) NOT NULL default '0000',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABMKFT" varchar(4) NOT NULL default ' ',
  "AAABMKFU" varchar(14) NOT NULL default ' ',
  "AAABMQXM" decimal(13,0) NOT NULL default '0',
  "AAABMQXL" decimal(13,0) NOT NULL default '0',
  "AAABMKFX" decimal(13,0) NOT NULL default '0',
  "AAABMKFY" varchar(6) NOT NULL default ' ',
  "AAABMRDI" varchar(8) NOT NULL default '00000000',
  "AAABMRDJ" varchar(6) NOT NULL default '000000',
  "AAABMKGB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKGP","AAABMKGE","AAABMOTC","AAABMOTD","AAABMKGH","AAABMKGI","AAABMKGJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKGP" varchar(30) NOT NULL default ' ',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  PRIMARY KEY ("AAABMRIL","AAABMKGP","AAABMRHL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKGR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAABMKLH" varchar(8) NOT NULL default '00000000',
  "AAABMKKY" varchar(8) NOT NULL default '00000000',
  "AAABMQWS" varchar(4) NOT NULL default ' ',
  "AAABMPTL" varchar(18) NOT NULL default ' ',
  "AAABMKHC" varchar(8) NOT NULL default '00000000',
  "AAABMKHD" varchar(1) NOT NULL default ' ',
  "AAABMKHE" varchar(1) NOT NULL default ' ',
  "AAABMKHF" varchar(1) NOT NULL default ' ',
  "AAABMKHG" varchar(1) NOT NULL default ' ',
  "AAABMKHH" varchar(1) NOT NULL default ' ',
  "AAABMKHI" varchar(1) NOT NULL default ' ',
  "AAABMKLA" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMLPO","AAABMKLH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKHR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMKIH" varchar(4) NOT NULL default '0000',
  "AAABMRFO" varchar(4) NOT NULL default ' ',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAABMKLH" varchar(8) NOT NULL default '00000000',
  "AAABMKKY" varchar(8) NOT NULL default '00000000',
  "AAABMKHZ" varchar(4) NOT NULL default '0000',
  "AAABMKIA" varchar(4) NOT NULL default '0000',
  "AAABMKIB" varchar(4) NOT NULL default '0000',
  "AAABMKIC" varchar(4) NOT NULL default '0000',
  "AAABMKID" varchar(4) NOT NULL default '0000',
  "AAABMKLA" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMKIH","AAABMRFO","AAABMLPO","AAABMKLH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKIL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOTC" varchar(4) NOT NULL default ' ',
  "AAABMOTD" varchar(20) NOT NULL default ' ',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOTC","AAABMOTD","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKIV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMKLH" varchar(8) NOT NULL default '00000000',
  "AAABMKKY" varchar(8) NOT NULL default '00000000',
  "AAABMLOL" varchar(1) NOT NULL default ' ',
  "AAABMQYF" varchar(8) NOT NULL default '00000000',
  "AAABMQYH" varchar(4) NOT NULL default ' ',
  "AAABMKJG" varchar(4) NOT NULL default '0000',
  "AAABMKJH" varchar(4) NOT NULL default '0000',
  "AAABMKJI" varchar(1) NOT NULL default ' ',
  "AAABMKJJ" varchar(1) NOT NULL default ' ',
  "AAABMKJK" varchar(1) NOT NULL default ' ',
  "AAABMKJL" varchar(1) NOT NULL default ' ',
  "AAABMKJM" varchar(1) NOT NULL default ' ',
  "AAABMKJN" varchar(1) NOT NULL default ' ',
  "AAABMKJO" varchar(1) NOT NULL default ' ',
  "AAABMKJP" varchar(1) NOT NULL default ' ',
  "AAABMKJQ" varchar(1) NOT NULL default ' ',
  "AAABMKJR" varchar(1) NOT NULL default ' ',
  "AAABMKJS" varchar(1) NOT NULL default ' ',
  "AAABMKJT" varchar(1) NOT NULL default ' ',
  "AAABMKJU" varchar(1) NOT NULL default ' ',
  "AAABMKJV" varchar(5) NOT NULL default ' ',
  "AAABMKJW" varchar(5) NOT NULL default ' ',
  "AAABMKJX" varchar(8) NOT NULL default '00000000',
  "AAABMKJY" varchar(8) NOT NULL default '00000000',
  "AAABMKJZ" varchar(10) NOT NULL default '0000000000',
  "AAABMKKA" varchar(6) NOT NULL default '000000',
  "AAABMKKB" varchar(8) NOT NULL default '00000000',
  "AAABMKKC" varchar(10) NOT NULL default '0000000000',
  "AAABMQZE" varchar(4) NOT NULL default ' ',
  "AAABMQZF" varchar(6) NOT NULL default '000000',
  "AAABMQZG" varchar(4) NOT NULL default '0000',
  "AAABMQZK" varchar(4) NOT NULL default ' ',
  "AAABMQZL" varchar(6) NOT NULL default '000000',
  "AAABMQZM" varchar(4) NOT NULL default '0000',
  "AAABMKLA" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMKLH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKKQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMKLG" varchar(2) NOT NULL default '00',
  "AAABMKLH" varchar(8) NOT NULL default '00000000',
  "AAABMKKY" varchar(8) NOT NULL default '00000000',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  "AAABMKLA" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMKLG","AAABMKLH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKLI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKLV" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMKLV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKLN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMKLV" varchar(4) NOT NULL default '0000',
  "AAABMKLR" varchar(30) NOT NULL default ' ',
  "AAABMKLS" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMKLV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKLW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMQYH" varchar(4) NOT NULL default ' ',
  "AAABMKMB" varchar(5) NOT NULL default ' ',
  "AAABMKMC" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRGQ","AAABMRGR","AAABMQYH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKMH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMKSP" varchar(1) NOT NULL default ' ',
  "AAABMPQR" varchar(4) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKMS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMKSP" varchar(1) NOT NULL default ' ',
  "AAABMPQR" varchar(4) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKNE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKUS" varchar(4) NOT NULL default '0000',
  "AAABMKNH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKNK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMPQR" varchar(4) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABMKNQ" varchar(4) NOT NULL default '0000',
  "AAABMKNR" varchar(4) NOT NULL default '0000',
  "AAABMKNS" varchar(4) NOT NULL default '0000',
  "AAABMKNT" varchar(30) NOT NULL default ' ',
  "AAABMKNU" varchar(30) NOT NULL default ' ',
  "AAABMKNV" varchar(30) NOT NULL default ' ',
  "AAABMKNW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPMH","AAABMREX","AAABMPQR","AAABMPSC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKOC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMPQR" varchar(4) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABMNRG" varchar(4) NOT NULL default '0000',
  "AAABMKOJ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMPMH","AAABMREX","AAABMPQR","AAABMPSC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKOP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKUS" varchar(4) NOT NULL default '0000',
  "AAABMPFB" varchar(4) NOT NULL default '0000',
  "AAABMKOT" varchar(8) NOT NULL default '00000000',
  "AAABMKOU" varchar(6) NOT NULL default '000000',
  "AAABMKOV" varchar(6) NOT NULL default '000000',
  "AAABMKOW" varchar(3) NOT NULL default '000',
  "AAABMKOX" varchar(4) NOT NULL default '0000',
  "AAABMKOY" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMKUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKPB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPFB" varchar(4) NOT NULL default '0000',
  "AAABMKPE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPFB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMPFB" varchar(4) NOT NULL default '0000',
  "AAABMKPL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMPFB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKPP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMXO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMMXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKPU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMXO" varchar(4) NOT NULL default '0000',
  "AAABMOHD" varchar(30) NOT NULL default ' ',
  "AAABMKVE" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMMXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKQD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMKQG" varchar(4) NOT NULL default '0000',
  "AAABMKQH" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKQK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKUS" varchar(4) NOT NULL default '0000',
  "AAABMKQN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKUS" varchar(4) NOT NULL default '0000',
  "AAABMKQT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKQW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMOHD" varchar(30) NOT NULL default ' ',
  "AAABMKVE" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKRF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMPMZ" varchar(4) NOT NULL default '0000',
  "AAABMMXQ" varchar(30) NOT NULL default ' ',
  "AAABMKRK" varchar(30) NOT NULL default ' ',
  "AAABMMXP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPMH","AAABMPMZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKRP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMPQR" varchar(4) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMPMH","AAABMPQR","AAABMPSC","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKSC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMOAI" varchar(4) NOT NULL default ' ',
  "AAABMKSP" varchar(1) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMPMH","AAABMREX","AAABMOAI","AAABMKSP","AAABMPSC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKSR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMOAI" varchar(4) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPMH","AAABMREX","AAABMOAI","AAABMPSC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKTC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMPMZ" varchar(4) NOT NULL default '0000',
  "AAABMOHD" varchar(30) NOT NULL default ' ',
  "AAABMKVE" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMPMZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKTL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMYK" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMMYK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKTQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMYK" varchar(4) NOT NULL default '0000',
  "AAABMOHD" varchar(30) NOT NULL default ' ',
  "AAABMKVE" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMMYK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKTZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKUM" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMKUM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKUE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMKUM" varchar(4) NOT NULL default '0000',
  "AAABMOHD" varchar(30) NOT NULL default ' ',
  "AAABMKVE" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMKUM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKUN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKUS" varchar(4) NOT NULL default '0000',
  "AAABMKUQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKUT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMZH" varchar(4) NOT NULL default '0000',
  "AAABMMXQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMZH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKUZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMZH" varchar(4) NOT NULL default '0000',
  "AAABMOHD" varchar(30) NOT NULL default ' ',
  "AAABMKVE" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMMZH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKVI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKVP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMZL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMZL" varchar(1) NOT NULL default ' ',
  "AAABMOHE" varchar(15) NOT NULL default ' ',
  "AAABMOHD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMMZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKWD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMLRB" varchar(4) NOT NULL default '0000',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABMKXX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRGQ","AAABMRGR","AAABMLRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKWO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKWZ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMKWZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKWT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKWZ" varchar(4) NOT NULL default '0000',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKWZ","AAABMNJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKXB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKXP" varchar(4) NOT NULL default '0000',
  "AAABMMJT" varchar(32) NOT NULL default ' ',
  "AAABMKXF" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKXP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKXI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMKXP" varchar(4) NOT NULL default '0000',
  "AAABMKXM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMKXP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKXQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKYH" varchar(4) NOT NULL default '0000',
  "AAABMKXT" varchar(4) NOT NULL default ' ',
  "AAABMKXU" varchar(6) NOT NULL default '000000',
  "AAABMLRB" varchar(4) NOT NULL default '0000',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABMKXX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKYH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKYA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMKYH" varchar(4) NOT NULL default '0000',
  "AAABMKYE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMKYH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKYI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMZS" varchar(4) NOT NULL default '0000',
  "AAABMMRH" varchar(1) NOT NULL default '0',
  "AAABMMRE" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMZS","AAABMMRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKYQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMZS" varchar(4) NOT NULL default '0000',
  "AAABMKYU" varchar(60) NOT NULL default ' ',
  "AAABMKYV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMMZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKYZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQJD" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMQJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKZE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIZ" varchar(4) NOT NULL default '0000',
  "AAABMOMJ" varchar(4) NOT NULL default '0000',
  "AAABMQJB" varchar(4) NOT NULL default '0000',
  "AAABMOMK" varchar(4) NOT NULL default '0000',
  "AAABMOLK" varchar(1) NOT NULL default ' ',
  "AAABMQJD" varchar(4) NOT NULL default '0000',
  "AAABMNAC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQIZ","AAABMOMJ","AAABMQJB","AAABMOMK","AAABMOLK","AAABMQJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKZU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQJD" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMKZY" varchar(30) NOT NULL default ' ',
  "AAABMKZZ" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQJD","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLAD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMZX" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOHD" varchar(30) NOT NULL default ' ',
  "AAABMOHE" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMZX","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLAM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNAC" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOHD" varchar(30) NOT NULL default ' ',
  "AAABMOHE" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNAC","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLAV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLBI" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMLBI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLBA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLBI" varchar(4) NOT NULL default '0000',
  "AAABMLBE" varchar(30) NOT NULL default ' ',
  "AAABMLBF" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLBI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLBJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMLBO" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRGQ","AAABMRGR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLBS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCO" varchar(40) NOT NULL default ' ',
  "AAABMLCP" varchar(4) NOT NULL default '0000',
  "AAABMLBW" varchar(10) NOT NULL default ' ',
  "AAABMODG" varchar(40) NOT NULL default ' ',
  "AAABMODH" varchar(30) NOT NULL default ' ',
  "AAABMODF" varchar(30) NOT NULL default ' ',
  "AAABMLCA" varchar(30) NOT NULL default ' ',
  "AAABMLCB" varchar(1) NOT NULL default ' ',
  "AAABMLCC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCO","AAABMLCP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLCG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLCO" varchar(40) NOT NULL default ' ',
  "AAABMLCP" varchar(4) NOT NULL default '0000',
  "AAABMOGI" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLCO","AAABMLCP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLCQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMLCT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLCW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLDD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLDM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLDH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLDM" varchar(4) NOT NULL default ' ',
  "AAABMLDK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLDM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLDN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLGG" varchar(10) NOT NULL default ' ',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMPUP" varchar(10) NOT NULL default ' ',
  "AAABMPUQ" varchar(6) NOT NULL default '000000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMLEK" varchar(4) NOT NULL default '0000',
  "AAABMLEL" varchar(2) NOT NULL default '00',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  "AAABMLEP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLGG","AAABMPQE","AAABMPUP","AAABMPUQ","AAABMQIL","AAABMLEK","AAABMLEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLEM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLGG" varchar(10) NOT NULL default ' ',
  "AAABMLEP" varchar(1) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABMLET" varchar(4) NOT NULL default '0000',
  "AAABMLEU" varchar(4) NOT NULL default '0000',
  "AAABMLEV" decimal(13,3) NOT NULL default '0.000',
  "AAABMLEW" varchar(3) NOT NULL default ' ',
  "AAABMLEX" decimal(13,3) NOT NULL default '0.000',
  "AAABMLEY" varchar(3) NOT NULL default ' ',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(30) NOT NULL default ' ',
  "AAABMLFB" decimal(5,2) NOT NULL default '0.00',
  "AAABMNDU" varchar(40) NOT NULL default ' ',
  "AAABMLFD" varchar(14) NOT NULL default ' ',
  "AAABMLFE" varchar(1) NOT NULL default ' ',
  "AAABMLFF" int(11) NOT NULL default '0',
  "AAABMLFG" int(11) NOT NULL default '0',
  "AAABMLFH" int(11) NOT NULL default '0',
  "AAABMLFI" varchar(6) NOT NULL default ' ',
  "AAABMLFJ" varchar(4) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMPUP" varchar(10) NOT NULL default ' ',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLGG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLFR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLGG" varchar(10) NOT NULL default ' ',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMPUP" varchar(10) NOT NULL default ' ',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMLFX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLGG","AAABMPQE","AAABMPUP","AAABMQIL"),
  KEY "AAABMLGD" ("AAABMRIL","AAABMQIL","AAABMLGG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLGH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRDD" varchar(1) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMLKT" varchar(8) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABMLGN" varchar(4) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRDD","AAABMNJK","AAABMLKT","AAABMPSC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLGU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLGZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLHO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLHO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLHP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLHJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMRDD" varchar(1) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABMLHO" varchar(4) NOT NULL default ' ',
  "AAABMLHP" varchar(1) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMRDD","AAABMPSC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLHV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLIB" varchar(30) NOT NULL default ' ',
  "AAABMQXR" varchar(4) NOT NULL default '0000',
  "AAABMLHZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLIB","AAABMQXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLID" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQXS" varchar(30) NOT NULL default ' ',
  "AAABMLPY" varchar(1) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABMLMY" varchar(6) NOT NULL default '000000',
  "AAABMLMZ" varchar(2) NOT NULL default '00',
  "AAABMLNA" varchar(2) NOT NULL default '00',
  "AAABMLNB" varchar(1) NOT NULL default ' ',
  "AAABMLNC" varchar(1) NOT NULL default ' ',
  "AAABMLND" varchar(1) NOT NULL default ' ',
  "AAABMLNE" varchar(1) NOT NULL default ' ',
  "AAABMLNF" varchar(1) NOT NULL default ' ',
  "AAABMLNG" varchar(1) NOT NULL default ' ',
  "AAABMLNH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQXS","AAABMLPY","AAABMREF","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLIZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQXS" varchar(30) NOT NULL default ' ',
  "AAABMORY" varchar(15) NOT NULL default ' ',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOSB" varchar(4) NOT NULL default '0000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMORN" varchar(15) NOT NULL default ' ',
  "AAABMORO" varchar(15) NOT NULL default ' ',
  "AAABMORP" varchar(15) NOT NULL default ' ',
  "AAABMORQ" varchar(15) NOT NULL default ' ',
  "AAABMORR" varchar(15) NOT NULL default ' ',
  "AAABMORS" varchar(30) NOT NULL default ' ',
  "AAABMORT" varchar(30) NOT NULL default ' ',
  "AAABMOSR" varchar(30) NOT NULL default ' ',
  "AAABMORV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQXS","AAABMORY","AAABMRCU","AAABMRDC","AAABMOSB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLJY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRDD" varchar(1) NOT NULL default ' ',
  "AAABMLKT" varchar(8) NOT NULL default ' ',
  "AAABMOCX" varchar(4) NOT NULL default '0000',
  "AAABMLKE" varchar(6) NOT NULL default '000000',
  "AAABMLKV" varchar(8) NOT NULL default ' ',
  "AAABMLKW" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRGR","AAABMRDD","AAABMLKT","AAABMOCX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLKT" varchar(8) NOT NULL default ' ',
  "AAABMOCX" varchar(4) NOT NULL default '0000',
  "AAABMLKV" varchar(8) NOT NULL default ' ',
  "AAABMLKW" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMLKT","AAABMOCX","AAABMLKV","AAABMLKW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLKX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMNEE" varchar(4) NOT NULL default '0000',
  "AAABMLLB" varchar(30) NOT NULL default ' ',
  "AAABMLLC" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMNEE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLLG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNDG" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMNDG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLLL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMNDG" varchar(4) NOT NULL default '0000',
  "AAABMLLP" varchar(30) NOT NULL default ' ',
  "AAABMLLQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMNDG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLLU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMNTS" varchar(4) NOT NULL default '0000',
  "AAABMNVB" varchar(4) NOT NULL default '0000',
  "AAABMLLZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMNTS","AAABMNVB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLME" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMNTS" varchar(4) NOT NULL default '0000',
  "AAABMLMI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMNTS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLMM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLMR" varchar(4) NOT NULL default ' ',
  "AAABMLMP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLMR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLMS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPX" varchar(40) NOT NULL default ' ',
  "AAABMLPY" varchar(1) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABMLMY" varchar(6) NOT NULL default '000000',
  "AAABMLMZ" varchar(2) NOT NULL default '00',
  "AAABMLNA" varchar(2) NOT NULL default '00',
  "AAABMLNB" varchar(1) NOT NULL default ' ',
  "AAABMLNC" varchar(1) NOT NULL default ' ',
  "AAABMLND" varchar(1) NOT NULL default ' ',
  "AAABMLNE" varchar(1) NOT NULL default ' ',
  "AAABMLNF" varchar(1) NOT NULL default ' ',
  "AAABMLNG" varchar(1) NOT NULL default ' ',
  "AAABMLNH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLPX","AAABMLPY","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLNM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREZ" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLNQ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREZ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLNU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLOG" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMLOG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLNZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLOG" varchar(4) NOT NULL default '0000',
  "AAABMLOD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLOG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLOH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPX" varchar(40) NOT NULL default ' ',
  "AAABMRFP" varchar(12) NOT NULL default ' ',
  "AAABMLOL" varchar(1) NOT NULL default ' ',
  "AAABMLOM" varchar(8) NOT NULL default '00000000',
  "AAABMLON" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMLPX","AAABMRFP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLOR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPE" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMLPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLOW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLPE" varchar(4) NOT NULL default '0000',
  "AAABMLPA" varchar(30) NOT NULL default ' ',
  "AAABMLPB" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLPF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPX" varchar(40) NOT NULL default ' ',
  "AAABMLPN" varchar(20) NOT NULL default ' ',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAABMLPK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLPX","AAABMLPN","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLPP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPX" varchar(40) NOT NULL default ' ',
  "AAABMLPY" varchar(1) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLPX","AAABMLPY","AAABMREF","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLQB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMNGJ" varchar(4) NOT NULL default '0000',
  "AAABMNGK" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRGQ","AAABMRGR","AAABMNGJ","AAABMNGK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLQM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLRB" varchar(4) NOT NULL default '0000',
  "AAABMLQP" varchar(30) NOT NULL default ' ',
  "AAABMLQQ" varchar(40) NOT NULL default ' ',
  "AAABMLQR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLQU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLRB" varchar(4) NOT NULL default '0000',
  "AAABMLQY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLRC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLTA" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMLTA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLRH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLTA" varchar(1) NOT NULL default '0',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLRL" varchar(10) NOT NULL default ' ',
  "AAABMLRM" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLTA","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLRQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMCV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLRV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREY" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMREY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLSA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREY" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLSE" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREY","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLSI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREZ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMREZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLSN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQTO" varchar(13) NOT NULL default ' ',
  "AAABMLSQ" varchar(12) NOT NULL default ' ',
  "AAABMLSR" varchar(6) NOT NULL default '000000',
  "AAABMLSS" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMQTO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLSV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQTO" varchar(13) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABMLSZ" varchar(4) NOT NULL default '0000',
  "AAABMLTA" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMQTO","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLTE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMQTO" varchar(13) NOT NULL default ' ',
  "AAABMLTI" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMQTO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLTM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMCV" varchar(30) NOT NULL default ' ',
  "AAABMLTT" varchar(4) NOT NULL default '0000',
  "AAABMMZN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMCV","AAABMLTT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLTU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRFO" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLTZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRFO" varchar(2) NOT NULL default '00',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLUD" varchar(30) NOT NULL default ' ',
  "AAABMLUE" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRFO","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLUI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQJA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQJA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLUN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOMJ" varchar(4) NOT NULL default '0000',
  "AAABMQJA" varchar(4) NOT NULL default ' ',
  "AAABMOKR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMOMJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLUU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOKR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMOKR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLUZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOMJ" varchar(4) NOT NULL default '0000',
  "AAABMOMK" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMOMJ","AAABMOMK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLVG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLWE" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMLWE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLVL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLWE" varchar(4) NOT NULL default '0000',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLWE","AAABMNJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLVT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLWE" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMLWE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLVY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLWE" varchar(4) NOT NULL default '0000',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLWE","AAABMNJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLWG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLWL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLWX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLWQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLWX" varchar(4) NOT NULL default ' ',
  "AAABMLWU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLWY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLXL" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMLXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLXD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLXL" varchar(4) NOT NULL default '0000',
  "AAABMLXH" varchar(30) NOT NULL default ' ',
  "AAABMLXI" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLXM" (
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
CREATE TABLE "AAABMLYB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNIF" varchar(4) NOT NULL default ' ',
  "AAABMLYK" varchar(8) NOT NULL default '00000000',
  "AAABMRCJ" varchar(8) NOT NULL default '00000000',
  "AAABMRCQ" varchar(8) NOT NULL default '00000000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMNIF","AAABMLYK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLYL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMNIF" varchar(4) NOT NULL default ' ',
  "AAABMLYP" varchar(30) NOT NULL default ' ',
  "AAABMLYQ" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMNIF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLYU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMODB" varchar(2) NOT NULL default ' ',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMOCV" varchar(8) NOT NULL default ' ',
  "AAABMMCT" varchar(1) NOT NULL default ' ',
  "AAABMLZS" varchar(4) NOT NULL default '0000',
  "AAABMLZT" varchar(1) NOT NULL default ' ',
  "AAABMMCU" varchar(30) NOT NULL default ' ',
  "AAABMMCV" varchar(30) NOT NULL default ' ',
  "AAABMLZG" varchar(2) NOT NULL default '00',
  "AAABMLZH" varchar(1) NOT NULL default ' ',
  "AAABMLZI" varchar(1) NOT NULL default ' ',
  "AAABMLZJ" varchar(4) NOT NULL default ' ',
  "AAABMLZK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMODB","AAABMRGQ","AAABMRGR","AAABMOCV","AAABMMCT","AAABMLZS","AAABMLZT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMLZU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMNIO" varchar(4) NOT NULL default ' ',
  "AAABMLZY" varchar(30) NOT NULL default ' ',
  "AAABMLZZ" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMNIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMAD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMAR" varchar(4) NOT NULL default '0000',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMNYU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMAR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMAK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMAR" varchar(4) NOT NULL default '0000',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMMAR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMAS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMCU" varchar(30) NOT NULL default ' ',
  "AAABMMCV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMCU","AAABMMCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMAZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMCU" varchar(30) NOT NULL default ' ',
  "AAABMMCV" varchar(30) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMBE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMCU","AAABMMCV","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMBJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMBU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMBO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMBU" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMBS" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMBU","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMBW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMCV" varchar(30) NOT NULL default ' ',
  "AAABMMBZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMCC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMODB" varchar(2) NOT NULL default ' ',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMOCV" varchar(8) NOT NULL default ' ',
  "AAABMMCT" varchar(1) NOT NULL default ' ',
  "AAABMMCU" varchar(30) NOT NULL default ' ',
  "AAABMMCV" varchar(30) NOT NULL default ' ',
  "AAABMMCM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMODB","AAABMRGQ","AAABMRGR","AAABMOCV","AAABMMCT","AAABMMCU","AAABMMCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMCW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMNIU" varchar(5) NOT NULL default ' ',
  "AAABMMDA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMNIU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMDE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRFP" varchar(12) NOT NULL default ' ',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMMDJ" varchar(1) NOT NULL default ' ',
  "AAABMMDK" varchar(1) NOT NULL default ' ',
  "AAABMMDL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRFP","AAABMRIM","AAABMRDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMDQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMNJQ" varchar(4) NOT NULL default ' ',
  "AAABMNJF" varchar(10) NOT NULL default ' ',
  "AAABMMDV" varchar(30) NOT NULL default ' ',
  "AAABMMDW" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMNJQ","AAABMNJF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMEB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPUN" varchar(1) NOT NULL default ' ',
  "AAABMOMK" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMPPC","AAABMPPD","AAABMPUN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMEN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  "AAABMMER" varchar(30) NOT NULL default ' ',
  "AAABMMES" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMNJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMEW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMMFA" varchar(1) NOT NULL default ' ',
  "AAABMMFB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRGQ","AAABMRGR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMFF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMFR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMFK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMFR" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMFO" varchar(30) NOT NULL default ' ',
  "AAABMMFP" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMFR","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMFT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQVS" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMQVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMFY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMQVS" varchar(4) NOT NULL default '0000',
  "AAABMMGC" varchar(30) NOT NULL default ' ',
  "AAABMMGD" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMQVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMGH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMNNW" varchar(4) NOT NULL default '0000',
  "AAABMMGL" varchar(30) NOT NULL default ' ',
  "AAABMMGM" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMNNW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMGQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQVR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMQVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMGV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMQVR" varchar(4) NOT NULL default '0000',
  "AAABMMGZ" varchar(30) NOT NULL default ' ',
  "AAABMMHA" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMQVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMNOB" varchar(4) NOT NULL default '0000',
  "AAABMMHI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMNOB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMHM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMHY" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMMHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMHR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMHY" varchar(4) NOT NULL default '0000',
  "AAABMMHV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMMHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMNEE" varchar(4) NOT NULL default '0000',
  "AAABMNEB" varchar(4) NOT NULL default '0000',
  "AAABMMIH" varchar(4) NOT NULL default '0000',
  "AAABMMII" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMRGQ","AAABMRGR","AAABMNEE","AAABMNEB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMIQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRDM" varchar(4) NOT NULL default '0000',
  "AAABMMIX" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMRGQ","AAABMRGR","AAABMRDM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMJE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQWS" varchar(4) NOT NULL default ' ',
  "AAABMMJW" varchar(4) NOT NULL default '0000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQWS","AAABMMJW","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMJO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQWS" varchar(4) NOT NULL default ' ',
  "AAABMMJW" varchar(4) NOT NULL default '0000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQWS","AAABMMJW","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMJY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQWS" varchar(4) NOT NULL default ' ',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMMKC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQWS","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNQJ" varchar(4) NOT NULL default ' ',
  "AAABMMKJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMNQJ" varchar(4) NOT NULL default ' ',
  "AAABMMKQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMNQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMKU" (
  "AAABMMKX" varchar(3) NOT NULL default '000',
  "AAABMQWS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMKX","AAABMQWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMKZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMQWS" varchar(4) NOT NULL default ' ',
  "AAABMOHD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMQWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMLH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMNOG" varchar(6) NOT NULL default ' ',
  "AAABMMLL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMNOG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMLP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPRI" varchar(4) NOT NULL default ' ',
  "AAABMMMD" varchar(1) NOT NULL default ' ',
  "AAABMPRC" varchar(4) NOT NULL default '0000',
  "AAABMPRD" varchar(8) NOT NULL default ' ',
  "AAABMPRE" varchar(6) NOT NULL default '000000',
  "AAABMPUN" varchar(1) NOT NULL default ' ',
  "AAABMMLX" varchar(1) NOT NULL default ' ',
  "AAABMMLY" varchar(1) NOT NULL default ' ',
  "AAABMMLZ" varchar(1) NOT NULL default ' ',
  "AAABMMMA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPRI","AAABMMMD","AAABMPRC","AAABMPRD","AAABMPRE","AAABMPUN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMMI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNPM" varchar(4) NOT NULL default '0000',
  "AAABMMQR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMNPM","AAABMMQR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMMP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMMMV" varchar(4) NOT NULL default '0000',
  "AAABMMMW" varchar(4) NOT NULL default '0000',
  "AAABMMMX" varchar(4) NOT NULL default '0000',
  "AAABMMMY" varchar(1) NOT NULL default ' ',
  "AAABMMMZ" varchar(4) NOT NULL default '0000',
  "AAABMMNA" varchar(1) NOT NULL default ' ',
  "AAABMMNB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMPPC","AAABMPPD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMNH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMOU" varchar(4) NOT NULL default '0000',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPRD" varchar(8) NOT NULL default ' ',
  "AAABMPRE" varchar(6) NOT NULL default '000000',
  "AAABMPRB" varchar(1) NOT NULL default ' ',
  "AAABMPRC" varchar(4) NOT NULL default '0000',
  "AAABMMNR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMOU","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMNV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMOU" varchar(4) NOT NULL default '0000',
  "AAABMMTR" varchar(4) NOT NULL default ' ',
  "AAABMMTS" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMMOU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMOC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMOV" varchar(4) NOT NULL default '0000',
  "AAABMMOW" varchar(4) NOT NULL default '0000',
  "AAABMORS" varchar(30) NOT NULL default ' ',
  "AAABMMOH" varchar(40) NOT NULL default ' ',
  "AAABMNYU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMOV","AAABMMOW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMOM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMOQ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMMOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMOR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMTL" varchar(4) NOT NULL default '0000',
  "AAABMMOU" varchar(4) NOT NULL default '0000',
  "AAABMMOV" varchar(4) NOT NULL default '0000',
  "AAABMMOW" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMMTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMOZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUF" varchar(6) NOT NULL default ' ',
  "AAABMMPK" varchar(3) NOT NULL default ' ',
  "AAABMNSG" varchar(5) NOT NULL default ' ',
  "AAABMNSH" varchar(8) NOT NULL default '00000000',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMMPG" decimal(15,5) NOT NULL default '0.00000',
  "AAABMMPH" decimal(16,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMPUF","AAABMMPK","AAABMNSG","AAABMNSH","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMPO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMPU" varchar(4) NOT NULL default '0000',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMNYU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMPU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMPV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMRY" varchar(4) NOT NULL default '0000',
  "AAABMMQK" varchar(4) NOT NULL default '0000',
  "AAABMMTX" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMMRY","AAABMMQK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMQD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMQR" varchar(4) NOT NULL default '0000',
  "AAABMMQK" varchar(4) NOT NULL default '0000',
  "AAABMMQH" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMMQR","AAABMMQK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMQL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMQR" varchar(4) NOT NULL default '0000',
  "AAABMMQS" varchar(4) NOT NULL default '0000',
  "AAABMMQP" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMMQR","AAABMMQS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMQT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMQY" varchar(4) NOT NULL default '0000',
  "AAABMMRY" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMMQY","AAABMMRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMRA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMRM" varchar(4) NOT NULL default '0000',
  "AAABMMRH" varchar(1) NOT NULL default '0',
  "AAABMMRE" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMRM","AAABMMRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMRI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMRM" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMMRM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMRN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMRS" varchar(4) NOT NULL default '0000',
  "AAABMMRQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMRS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMRT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMRY" varchar(4) NOT NULL default '0000',
  "AAABMNRG" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMMRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMRZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMMSH" varchar(30) NOT NULL default ' ',
  "AAABMMSD" varchar(30) NOT NULL default ' ',
  "AAABMMSE" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRGQ","AAABMMSH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMSI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMODA" varchar(4) NOT NULL default ' ',
  "AAABMMSL" varchar(1) NOT NULL default '0',
  "AAABMMSM" varchar(1) NOT NULL default '0',
  "AAABMRDD" varchar(1) NOT NULL default ' ',
  "AAABMOCV" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMODA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMSR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNEB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMNEB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMSW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMTE" varchar(4) NOT NULL default ' ',
  "AAABMMTF" varchar(6) NOT NULL default '000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMMTC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMTE","AAABMMTF","AAABMPPC","AAABMPPD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMTI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMTX" varchar(4) NOT NULL default '0000',
  "AAABMMTL" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMMTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMTO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMTX" varchar(4) NOT NULL default '0000',
  "AAABMMTR" varchar(4) NOT NULL default ' ',
  "AAABMMTS" varchar(6) NOT NULL default '000000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMQJD" varchar(4) NOT NULL default '0000',
  "AAABMMTV" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMMTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMTY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRDD" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMOCV" varchar(8) NOT NULL default ' ',
  "AAABMOCX" varchar(4) NOT NULL default '0000',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRDD","AAABMREX","AAABMOCV","AAABMOCX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMUK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMMUO" varchar(4) NOT NULL default ' ',
  "AAABMMUP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPMH","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMUT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMMUX" varchar(2) NOT NULL default ' ',
  "AAABMMUY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPMH","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMVC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMMVO" varchar(5) NOT NULL default ' ',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMMVI" varchar(30) NOT NULL default ' ',
  "AAABMMVJ" varchar(2) NOT NULL default ' ',
  "AAABMMVK" varchar(2) NOT NULL default ' ',
  "AAABMMVL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPMH","AAABMMVO","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMVQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMMVY" varchar(50) NOT NULL default ' ',
  "AAABMMVU" varchar(1) NOT NULL default ' ',
  "AAABMMVV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPMH","AAABMMVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMVZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMMWD" varchar(1) NOT NULL default ' ',
  "AAABMMWE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPMH","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMWI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMMWM" varchar(13) NOT NULL default '0000000000000',
  "AAABMMWN" varchar(13) NOT NULL default '0000000000000',
  PRIMARY KEY ("AAABMRIL","AAABMPMH","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMWR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMODB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMODB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMWW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMODA" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMMXA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMODA","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMXE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMXO" varchar(4) NOT NULL default '0000',
  "AAABMMYK" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMMXO","AAABMMYK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMXL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMMXO" varchar(4) NOT NULL default '0000',
  "AAABMMXP" varchar(30) NOT NULL default ' ',
  "AAABMMXQ" varchar(30) NOT NULL default ' ',
  "AAABMMXR" varchar(30) NOT NULL default ' ',
  "AAABMMXS" varchar(30) NOT NULL default ' ',
  "AAABMMXT" varchar(30) NOT NULL default ' ',
  "AAABMMXU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMXX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMPMZ" varchar(4) NOT NULL default '0000',
  "AAABMMYB" varchar(1) NOT NULL default ' ',
  "AAABMMYC" varchar(1) NOT NULL default ' ',
  "AAABMMYD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPMH","AAABMPMZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMYH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPMZ" varchar(4) NOT NULL default '0000',
  "AAABMMYK" varchar(4) NOT NULL default '0000',
  "AAABMMZH" varchar(4) NOT NULL default '0000',
  "AAABMMYM" varchar(30) NOT NULL default ' ',
  "AAABMMYN" varchar(30) NOT NULL default ' ',
  "AAABMMYO" varchar(30) NOT NULL default ' ',
  "AAABMMYP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPMZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMYS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMMYV" varchar(30) NOT NULL default ' ',
  "AAABMMYW" varchar(2) NOT NULL default ' ',
  "AAABMMYX" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMMZH" varchar(4) NOT NULL default '0000',
  "AAABMMZE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPMH","AAABMMZH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMZI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMZN" varchar(30) NOT NULL default ' ',
  "AAABMMZL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMZN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMZO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMZS" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMMZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMZT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMZX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMZX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMMZY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNAC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNAC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNAD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQPW" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABMQPW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNAI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMQPW" varchar(5) NOT NULL default '00000',
  "AAABMNAM" varchar(30) NOT NULL default ' ',
  "AAABMNAN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMQPW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNAR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQPX" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABMQPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNAW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMQPX" varchar(5) NOT NULL default '00000',
  "AAABMNBA" varchar(30) NOT NULL default ' ',
  "AAABMNBB" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMQPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNBF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQPY" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABMQPY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNBK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMQPY" varchar(5) NOT NULL default '00000',
  "AAABMNBO" varchar(30) NOT NULL default ' ',
  "AAABMNBP" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMQPY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNBT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQPZ" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABMQPZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNBY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMQPZ" varchar(5) NOT NULL default '00000',
  "AAABMNCC" varchar(30) NOT NULL default ' ',
  "AAABMNCD" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMQPZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNCH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQQA" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABMQQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNCM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMQQA" varchar(5) NOT NULL default '00000',
  "AAABMNCQ" varchar(30) NOT NULL default ' ',
  "AAABMNCR" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMQQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNCV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNDG" varchar(4) NOT NULL default '0000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMPQR" varchar(4) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABMRDD" varchar(1) NOT NULL default ' ',
  "AAABMOCV" varchar(8) NOT NULL default ' ',
  "AAABMOCX" varchar(4) NOT NULL default '0000',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMNDG","AAABMREX","AAABMPQR","AAABMPSC","AAABMRDD","AAABMOCV","AAABMOCX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNDN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMNDU" varchar(40) NOT NULL default ' ',
  "AAABMNDR" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMNDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNDV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNEE" varchar(4) NOT NULL default '0000',
  "AAABMNEF" varchar(4) NOT NULL default '0000',
  "AAABMODA" varchar(4) NOT NULL default ' ',
  "AAABMNEA" varchar(1) NOT NULL default ' ',
  "AAABMNEB" varchar(4) NOT NULL default '0000',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNEE","AAABMNEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNEG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNTS" varchar(4) NOT NULL default '0000',
  "AAABMNVB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMNTS","AAABMNVB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNEN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNTS" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMNTS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNES" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNTA" varchar(4) NOT NULL default ' ',
  "AAABMNGJ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMNTA","AAABMNGJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNEZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMORS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRGQ","AAABMNVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNFH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMNVS" varchar(4) NOT NULL default '0000',
  "AAABMNGJ" varchar(4) NOT NULL default '0000',
  "AAABMNGK" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRGQ","AAABMRGR","AAABMNVS","AAABMNGJ","AAABMNGK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNFU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNUM" varchar(4) NOT NULL default ' ',
  "AAABMNUN" varchar(6) NOT NULL default '000000',
  "AAABMNUO" varchar(4) NOT NULL default ' ',
  "AAABMNUP" varchar(6) NOT NULL default '000000',
  "AAABMNVS" varchar(4) NOT NULL default '0000',
  "AAABMNGJ" varchar(4) NOT NULL default '0000',
  "AAABMNGK" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMNUM","AAABMNUN","AAABMNUO","AAABMNUP","AAABMNVS","AAABMNGJ","AAABMNGK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNGL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNTA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNTA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNGQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUM" varchar(1) NOT NULL default ' ',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMORS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPUM","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNGY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMNVI" varchar(4) NOT NULL default '0000',
  "AAABMNHC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMNVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNHG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNHT" varchar(4) NOT NULL default '0000',
  "AAABMNHJ" varchar(30) NOT NULL default ' ',
  "AAABMOMK" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMNHT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNHN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIZ" varchar(4) NOT NULL default '0000',
  "AAABMOMJ" varchar(4) NOT NULL default '0000',
  "AAABMOMK" varchar(4) NOT NULL default '0000',
  "AAABMQJD" varchar(4) NOT NULL default '0000',
  "AAABMNHT" varchar(4) NOT NULL default '0000',
  "AAABMNHU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQIZ","AAABMOMJ","AAABMOMK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNIF" varchar(4) NOT NULL default ' ',
  "AAABMNIG" varchar(10) NOT NULL default ' ',
  "AAABMORS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNIF","AAABMNIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNIH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNIO" varchar(4) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMNYU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNIO","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNIQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNIU" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNIU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNIV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNJQ" varchar(4) NOT NULL default ' ',
  "AAABMNJF" varchar(10) NOT NULL default ' ',
  "AAABMORS" varchar(30) NOT NULL default ' ',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMNYU" varchar(30) NOT NULL default ' ',
  "AAABMNJC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNJQ","AAABMNJF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNJG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNJK" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMNJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNJL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNJQ" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNJQ","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNJS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNJX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNLV" varchar(25) NOT NULL default ' ',
  "AAABMNLW" varchar(4) NOT NULL default ' ',
  "AAABMNLX" varchar(2) NOT NULL default ' ',
  "AAABMNLY" varchar(4) NOT NULL default ' ',
  "AAABMNNR" varchar(12) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMNMP" varchar(4) NOT NULL default ' ',
  "AAABMNKH" varchar(1) NOT NULL default ' ',
  "AAABMNKI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNLV","AAABMNLW","AAABMNLX","AAABMNLY","AAABMNNR","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNKR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABMNLX" varchar(2) NOT NULL default ' ',
  "AAABMNLY" varchar(4) NOT NULL default ' ',
  "AAABMNMP" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMNLA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNLV","AAABMNLW","AAABMNLX","AAABMNLY","AAABMNMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNLH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABMNLX" varchar(2) NOT NULL default ' ',
  "AAABMNLY" varchar(4) NOT NULL default ' ',
  "AAABMNNR" varchar(12) NOT NULL default ' ',
  "AAABMNNC" varchar(25) NOT NULL default ' ',
  "AAABMNLP" varchar(70) NOT NULL default ' ',
  "AAABMNLQ" varchar(1) NOT NULL default ' ',
  "AAABMNLR" varchar(6) NOT NULL default '000000',
  "AAABMNLS" varchar(2) NOT NULL default '00',
  "AAABMNLT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNLV","AAABMNLW","AAABMNLX","AAABMNLY","AAABMNNR","AAABMNNC"),
  KEY "AAABMNMB" ("AAABMNNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNMD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNNC" varchar(25) NOT NULL default ' ',
  "AAABMNMP" varchar(4) NOT NULL default ' ',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABMNMK" int(11) NOT NULL default '0',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABMNMM" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMNNC","AAABMNMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNMQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNNC" varchar(25) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMNNE" varchar(9) NOT NULL default '000000000',
  "AAABMNMV" varchar(3) NOT NULL default ' ',
  "AAABMNMW" int(11) NOT NULL default '0',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMNMY" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(45) NOT NULL default ' ',
  "AAABMNNA" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNNC","AAABMREL","AAABMNNE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNNF" (
  "AAABMNNM" varchar(25) NOT NULL default ' ',
  "AAABMNNN" varchar(4) NOT NULL default ' ',
  "AAABMNNO" varchar(2) NOT NULL default ' ',
  "AAABMNNP" varchar(4) NOT NULL default ' ',
  "AAABMNNQ" varchar(70) NOT NULL default ' ',
  "AAABMNNR" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNNM","AAABMNNN","AAABMNNO","AAABMNNP","AAABMNNQ","AAABMNNR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNNS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNNW" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMNNW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNNX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNOB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMNOB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNOC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNOG" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNOG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNOH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMRDD" varchar(1) NOT NULL default ' ',
  "AAABMOCV" varchar(8) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABMNOO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMRDD","AAABMOCV","AAABMRGR","AAABMPSC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNOV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMOTQ" varchar(3) NOT NULL default ' ',
  "AAABMNPE" varchar(4) NOT NULL default ' ',
  "AAABMNPF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMOTQ","AAABMNPE","AAABMNPF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNPG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNPL" varchar(4) NOT NULL default '0000',
  "AAABMNPM" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMNPL","AAABMNPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNPN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMOCV" varchar(8) NOT NULL default ' ',
  "AAABMRDD" varchar(1) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABMNQC" varchar(3) NOT NULL default ' ',
  "AAABMNPV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMRGR","AAABMOCV","AAABMRDD","AAABMPSC","AAABMNQC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNQD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQWS" varchar(4) NOT NULL default ' ',
  "AAABMNQJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQWS","AAABMNQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNQK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNVI" varchar(4) NOT NULL default '0000',
  "AAABMNQN" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMNVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMNRG" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRDC","AAABMRGQ","AAABMRGR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNRA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMNRG" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRDC","AAABMRGQ","AAABMRGR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNRM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMNRV" varchar(4) NOT NULL default '0000',
  "AAABMNUA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRGQ","AAABMRGR","AAABMNRV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNRW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUE" varchar(6) NOT NULL default ' ',
  "AAABMNSG" varchar(5) NOT NULL default ' ',
  "AAABMNSH" varchar(8) NOT NULL default '00000000',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMNSC" decimal(16,2) NOT NULL default '0.00',
  "AAABMNSD" decimal(7,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABMPUE","AAABMNSG","AAABMNSH","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNSJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOGK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNSO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRGQ","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNSV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMNVS" varchar(4) NOT NULL default '0000',
  "AAABMNVO" varchar(4) NOT NULL default ' ',
  "AAABMNTA" varchar(4) NOT NULL default ' ',
  "AAABMNTB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRGQ","AAABMNVS","AAABMNVO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNTG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMNTK" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRGQ","AAABMRGR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNTO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMNTS" varchar(4) NOT NULL default '0000',
  "AAABMNTT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRGQ","AAABMNVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNTX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMNUA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRGQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNUD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNUM" varchar(4) NOT NULL default ' ',
  "AAABMNUN" varchar(6) NOT NULL default '000000',
  "AAABMNUO" varchar(4) NOT NULL default ' ',
  "AAABMNUP" varchar(6) NOT NULL default '000000',
  "AAABMNVS" varchar(4) NOT NULL default '0000',
  "AAABMNVO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNUM","AAABMNUN","AAABMNUO","AAABMNUP","AAABMNVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNUR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMNVB" varchar(4) NOT NULL default '0000',
  "AAABMNVC" varchar(30) NOT NULL default ' ',
  "AAABMNUX" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRGQ","AAABMRGR","AAABMNVB","AAABMNVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNVD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNVI" varchar(4) NOT NULL default '0000',
  "AAABMNVG" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMNVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNVJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMNVS" varchar(4) NOT NULL default '0000',
  "AAABMNVO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRGQ","AAABMRGR","AAABMNVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNVT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOTH" varchar(4) NOT NULL default ' ',
  "AAABMNVW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOTH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNVZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOIX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMOIX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNWE" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABMONH" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMONG","AAABMONH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNWJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMONQ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMONQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNWO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMONZ" varchar(4) NOT NULL default '0000',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMNYU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMONZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNWV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNXA" varchar(4) NOT NULL default '0000',
  "AAABMNWY" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMNXA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNXB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOOI" varchar(4) NOT NULL default '0000',
  "AAABMNXJ" varchar(4) NOT NULL default ' ',
  "AAABMNXK" varchar(1) NOT NULL default ' ',
  "AAABMOPS" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMOOI","AAABMNXJ","AAABMNXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNXL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOOR" varchar(4) NOT NULL default '0000',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMNYU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOOR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNXS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOPA" varchar(4) NOT NULL default '0000',
  "AAABMOQB" varchar(4) NOT NULL default '0000',
  "AAABMNXW" varchar(4) NOT NULL default ' ',
  "AAABMNXX" varchar(4) NOT NULL default '0000',
  "AAABMNXY" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNYB" (
  "AAABMNZC" varchar(3) NOT NULL default '000',
  "AAABMOPJ" varchar(4) NOT NULL default '0000',
  "AAABMNYE" varchar(4) NOT NULL default '0000',
  "AAABMOOR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMNZC","AAABMOPJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNYI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOPS" varchar(4) NOT NULL default '0000',
  "AAABMONQ" varchar(4) NOT NULL default '0000',
  "AAABMONZ" varchar(4) NOT NULL default '0000',
  "AAABMOPJ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMOPS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNYQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOQB" varchar(4) NOT NULL default '0000',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMNYU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOQB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNYX" (
  "AAABMNZC" varchar(3) NOT NULL default '000',
  "AAABMOPJ" varchar(4) NOT NULL default '0000',
  "AAABMNZE" varchar(2) NOT NULL default '00',
  "AAABMOPA" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMNZC","AAABMOPJ","AAABMNZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNZF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOQL" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMOQL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNZK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOQS" varchar(6) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMNZO" decimal(7,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABMOQS","AAABMNZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNZS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMORC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMNZX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABMOAI" varchar(4) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMRDC","AAABMOAH","AAABMOAI","AAABMPSC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOAK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOAP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOBU" varchar(10) NOT NULL default ' ',
  "AAABMODF" varchar(30) NOT NULL default ' ',
  "AAABMODG" varchar(40) NOT NULL default ' ',
  "AAABMODH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOAX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOBT" varchar(4) NOT NULL default ' ',
  "AAABMOBI" varchar(6) NOT NULL default '000000',
  "AAABMOBU" varchar(10) NOT NULL default ' ',
  "AAABMODF" varchar(30) NOT NULL default ' ',
  "AAABMODG" varchar(40) NOT NULL default ' ',
  "AAABMODH" varchar(30) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOBT","AAABMOBI","AAABMOBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOBK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOBT" varchar(4) NOT NULL default ' ',
  "AAABMOBU" varchar(10) NOT NULL default ' ',
  "AAABMODF" varchar(30) NOT NULL default ' ',
  "AAABMODG" varchar(40) NOT NULL default ' ',
  "AAABMODH" varchar(30) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOBT","AAABMOBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOBV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMOBZ" varchar(4) NOT NULL default '0000',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAABMOCB" varchar(4) NOT NULL default ' ',
  "AAABMOCC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRGQ","AAABMRGR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOCG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMOCK" varchar(30) NOT NULL default ' ',
  "AAABMOCL" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMOGK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOCP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMODA" varchar(4) NOT NULL default ' ',
  "AAABMODB" varchar(2) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMOCV" varchar(8) NOT NULL default ' ',
  "AAABMRDD" varchar(1) NOT NULL default ' ',
  "AAABMOCX" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMODA","AAABMODB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMODC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMODK" varchar(10) NOT NULL default ' ',
  "AAABMODF" varchar(30) NOT NULL default ' ',
  "AAABMODG" varchar(40) NOT NULL default ' ',
  "AAABMODH" varchar(30) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMODK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMODL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOEX" varchar(4) NOT NULL default '0000',
  "AAABMOEU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMOEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMODT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOEX" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMOEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMODY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOEX" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMOEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOED" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOEX" varchar(4) NOT NULL default '0000',
  "AAABMOEU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMOEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOEL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOEX" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMOEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOEX" varchar(4) NOT NULL default '0000',
  "AAABMOEU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMOEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOEY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOFL" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMOFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOFD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOFL" varchar(4) NOT NULL default '0000',
  "AAABMOFH" varchar(30) NOT NULL default ' ',
  "AAABMOFI" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMOFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOFM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMOFP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRGQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOFS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMPMZ" varchar(4) NOT NULL default '0000',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMOFX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOGK","AAABMPMZ","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOGC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOGK" varchar(4) NOT NULL default ' ',
  "AAABMPMZ" varchar(4) NOT NULL default '0000',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOGI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOGK","AAABMPMZ","AAABMRCU","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOGO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRGQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOGU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOLK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOGZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOLK" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOHD" varchar(30) NOT NULL default ' ',
  "AAABMOHE" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOLK","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOHI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABMOIB" varchar(14) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default '0',
  "AAABMOHN" varchar(4) NOT NULL default '0000',
  "AAABMOHO" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMOIA","AAABMOIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOHS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABMOIB" varchar(14) NOT NULL default ' ',
  "AAABMOHX" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMOIA","AAABMOIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOIC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOTH" varchar(4) NOT NULL default ' ',
  "AAABMOIG" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMOTH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOIK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOIP" varchar(4) NOT NULL default ' ',
  "AAABMOIN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOIP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOIQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOIX" varchar(6) NOT NULL default '000000',
  "AAABMOIU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMOIX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOIY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQJA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQJA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOJD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQJA" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOJH" varchar(30) NOT NULL default ' ',
  "AAABMOJI" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQJA","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOJM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOMJ" varchar(4) NOT NULL default '0000',
  "AAABMQJA" varchar(4) NOT NULL default ' ',
  "AAABMOKR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMOMJ","AAABMQJA","AAABMOKR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOJV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOMJ" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOJZ" varchar(30) NOT NULL default ' ',
  "AAABMOKA" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOMJ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOKE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOKR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMOKR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOKJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOKR" varchar(4) NOT NULL default '0000',
  "AAABMOKN" varchar(30) NOT NULL default ' ',
  "AAABMOKO" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMOKR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOKS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOLS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOLS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOKX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIZ" varchar(4) NOT NULL default '0000',
  "AAABMOMJ" varchar(4) NOT NULL default '0000',
  "AAABMOMK" varchar(4) NOT NULL default '0000',
  "AAABMQJB" varchar(4) NOT NULL default '0000',
  "AAABMOLK" varchar(1) NOT NULL default ' ',
  "AAABMOLS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQIZ","AAABMOMJ","AAABMOMK","AAABMQJB","AAABMOLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOLL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOLS" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOLP" varchar(30) NOT NULL default ' ',
  "AAABMOLQ" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOLS","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOLU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOMJ" varchar(4) NOT NULL default '0000',
  "AAABMOMK" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMOMJ","AAABMOMK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOMB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOMJ" varchar(4) NOT NULL default '0000',
  "AAABMOMK" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOMG" varchar(30) NOT NULL default ' ',
  "AAABMOMH" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOMJ","AAABMOMK","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOMM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMOOI" varchar(4) NOT NULL default '0000',
  "AAABMOMQ" varchar(8) NOT NULL default '00000000',
  "AAABMOMR" varchar(8) NOT NULL default '00000000',
  "AAABMOMS" varchar(8) NOT NULL default '00000000',
  "AAABMOMT" varchar(8) NOT NULL default '00000000',
  "AAABMOMU" varchar(8) NOT NULL default '00000000',
  "AAABMOMV" varchar(8) NOT NULL default '00000000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMONA" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABMONH" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMONE" varchar(30) NOT NULL default ' ',
  "AAABMONF" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABMONH","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMONJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMONQ" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMONN" varchar(30) NOT NULL default ' ',
  "AAABMONO" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMONQ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMONS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMONZ" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMONW" varchar(30) NOT NULL default ' ',
  "AAABMONX" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMONZ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOOI" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOOF" varchar(30) NOT NULL default ' ',
  "AAABMOOG" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOOI","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOOK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOOR" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOOO" varchar(30) NOT NULL default ' ',
  "AAABMOOP" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOOR","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOOT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOPA" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOOX" varchar(30) NOT NULL default ' ',
  "AAABMOOY" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOPA","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOPC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOPJ" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOPG" varchar(30) NOT NULL default ' ',
  "AAABMOPH" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOPJ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOPL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOPS" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOPP" varchar(30) NOT NULL default ' ',
  "AAABMOPQ" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOPS","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOPU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOQB" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOPY" varchar(30) NOT NULL default ' ',
  "AAABMOPZ" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOQB","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOQD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOQL" varchar(4) NOT NULL default '0000',
  "AAABMOQH" varchar(30) NOT NULL default ' ',
  "AAABMOQI" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMOQL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOQM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOQS" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOQS","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOQU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMORC" varchar(4) NOT NULL default '0000',
  "AAABMOQY" varchar(30) NOT NULL default ' ',
  "AAABMOQZ" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMORC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMORD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQXS" varchar(30) NOT NULL default ' ',
  "AAABMORY" varchar(15) NOT NULL default ' ',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMOSB" varchar(4) NOT NULL default '0000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMORN" varchar(15) NOT NULL default ' ',
  "AAABMORO" varchar(15) NOT NULL default ' ',
  "AAABMORP" varchar(15) NOT NULL default ' ',
  "AAABMORQ" varchar(15) NOT NULL default ' ',
  "AAABMORR" varchar(15) NOT NULL default ' ',
  "AAABMORS" varchar(30) NOT NULL default ' ',
  "AAABMORT" varchar(30) NOT NULL default ' ',
  "AAABMOSR" varchar(30) NOT NULL default ' ',
  "AAABMORV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQXS","AAABMORY","AAABMRCU","AAABMRDC","AAABMOSB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOSC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQXQ" varchar(4) NOT NULL default ' ',
  "AAABMQXR" varchar(4) NOT NULL default '0000',
  "AAABMQXS" varchar(30) NOT NULL default ' ',
  "AAABMQXH" varchar(30) NOT NULL default ' ',
  "AAABMQYH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQXQ","AAABMQXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOSM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQXQ" varchar(4) NOT NULL default ' ',
  "AAABMQXS" varchar(30) NOT NULL default ' ',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMOSR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQXQ","AAABMQXS","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOSW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOTC" varchar(4) NOT NULL default ' ',
  "AAABMOTD" varchar(20) NOT NULL default ' ',
  "AAABMOTA" decimal(3,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMOTC","AAABMOTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOTE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMOTH" varchar(4) NOT NULL default ' ',
  "AAABMOTI" varchar(18) NOT NULL default ' ',
  "AAABMOTJ" varchar(1) NOT NULL default ' ',
  "AAABMOTK" varchar(1) NOT NULL default ' ',
  "AAABMOTL" decimal(16,2) NOT NULL default '0.00',
  "AAABMOTM" varchar(5) NOT NULL default ' ',
  "AAABMOTN" varchar(30) NOT NULL default ' ',
  "AAABMOTO" varchar(30) NOT NULL default ' ',
  "AAABMOTP" varchar(1) NOT NULL default ' ',
  "AAABMOTQ" varchar(3) NOT NULL default ' ',
  "AAABMOTR" varchar(1) NOT NULL default ' ',
  "AAABMOTS" decimal(16,2) NOT NULL default '0.00',
  "AAABMOTT" varchar(5) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOTY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOUF" varchar(4) NOT NULL default '0000',
  "AAABMOUC" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMOUF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOUG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABMRHT" varchar(4) NOT NULL default '0000',
  "AAABMOUK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIN","AAABMRHT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMOUO" (
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
CREATE TABLE "AAABMOVO" (
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
CREATE TABLE "AAABMPAG" (
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
CREATE TABLE "AAABMPEX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPFB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMPFB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMPFC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUA" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPMZ" varchar(4) NOT NULL default '0000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMPMU" varchar(12) NOT NULL default ' ',
  "AAABMPMV" varchar(8) NOT NULL default '00000000',
  "AAABMPMW" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPUA","AAABMPMZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMPFN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUA" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMPMU" varchar(12) NOT NULL default ' ',
  "AAABMPMV" varchar(8) NOT NULL default '00000000',
  "AAABMPMW" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPUA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMPFW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUA" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMPMJ" varchar(12) NOT NULL default ' ',
  "AAABMPMK" varchar(8) NOT NULL default '00000000',
  "AAABMPML" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPUA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMPGF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUA" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMPMJ" varchar(12) NOT NULL default ' ',
  "AAABMPMK" varchar(8) NOT NULL default '00000000',
  "AAABMPML" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPUA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMPGO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUA" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPMZ" varchar(4) NOT NULL default '0000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMPHD" varchar(12) NOT NULL default ' ',
  "AAABMPHE" varchar(8) NOT NULL default '00000000',
  "AAABMPHF" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPUA","AAABMPMZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMPGZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUA" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMPHD" varchar(12) NOT NULL default ' ',
  "AAABMPHE" varchar(8) NOT NULL default '00000000',
  "AAABMPHF" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPUA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMPHI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUA" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMPMM" varchar(12) NOT NULL default ' ',
  "AAABMPMN" varchar(8) NOT NULL default ' ',
  "AAABMPMO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMPUA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMPHR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABMPUA" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMPMM" varchar(12) NOT NULL default ' ',
  "AAABMPMN" varchar(8) NOT NULL default ' ',
  "AAABMPMO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMPPC","AAABMPPD","AAABMPPE","AAABMPUA","AAABMPWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMPIK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABMPUA" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMPRK" varchar(13) NOT NULL default '0000000000000',
  "AAABMPMM" varchar(12) NOT NULL default ' ',
  "AAABMPMN" varchar(8) NOT NULL default ' ',
  "AAABMPMO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMPPC","AAABMPPD","AAABMPPE","AAABMPUA","AAABMPWO"),
  KEY "AAABMPJE" ("AAABMPUA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMPJG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUA" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMPQE" varchar(10) NOT NULL default '0000000000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABMPMZ" varchar(4) NOT NULL default '0000',
  "AAABMPMM" varchar(12) NOT NULL default ' ',
  "AAABMPMN" varchar(8) NOT NULL default '00000000',
  "AAABMPMO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMPUA","AAABMPWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMPJW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMPPC" varchar(4) NOT NULL default ' ',
  "AAABMPPD" varchar(6) NOT NULL default '000000',
  "AAABMPPE" varchar(4) NOT NULL default '0000',
  "AAABMPUA" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMPMZ" varchar(4) NOT NULL default '0000',
  "AAABMPMM" varchar(12) NOT NULL default ' ',
  "AAABMPMN" varchar(8) NOT NULL default ' ',
  "AAABMPMO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMPPC","AAABMPPD","AAABMPPE","AAABMPUA","AAABMPWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMPKP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUA" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMPMM" varchar(12) NOT NULL default ' ',
  "AAABMPMN" varchar(8) NOT NULL default ' ',
  "AAABMPMO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMPUA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMPKY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUA" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMPMM" varchar(12) NOT NULL default ' ',
  "AAABMPMN" varchar(8) NOT NULL default ' ',
  "AAABMPMO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMPUA","AAABMPWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMPLI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUA" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMPMZ" varchar(4) NOT NULL default '0000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMPLP" varchar(13) NOT NULL default '0000000000000',
  "AAABMPQR" varchar(4) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMPWM" varchar(8) NOT NULL default '00000000',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMPMM" varchar(12) NOT NULL default ' ',
  "AAABMPMN" varchar(8) NOT NULL default ' ',
  "AAABMPMO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMPUA","AAABMPWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMPMD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPUA" varchar(16) NOT NULL default '0000000000000000',
  "AAABMPMZ" varchar(4) NOT NULL default '0000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMPMI" varchar(4) NOT NULL default '0000',
  "AAABMPMJ" varchar(12) NOT NULL default ' ',
  "AAABMPMK" varchar(8) NOT NULL default '00000000',
  "AAABMPML" varchar(6) NOT NULL default '000000',
  "AAABMPMM" varchar(12) NOT NULL default ' ',
  "AAABMPMN" varchar(8) NOT NULL default '00000000',
  "AAABMPMO" varchar(6) NOT NULL default '000000',
  "AAABMPMP" varchar(1) NOT NULL default ' ',
  "AAABMPMQ" varchar(12) NOT NULL default ' ',
  "AAABMPMR" varchar(8) NOT NULL default '00000000',
  "AAABMPMS" varchar(6) NOT NULL default '000000',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAABMPMU" varchar(12) NOT NULL default ' ',
  "AAABMPMV" varchar(8) NOT NULL default '00000000',
  "AAABMPMW" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMPUA","AAABMPMZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMPNA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRCJ" varchar(8) NOT NULL default '00000000',
  "AAABMRCQ" varchar(8) NOT NULL default '00000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMPPO" varchar(8) NOT NULL default ' ',
  "AAABMPNY" varchar(4) NOT NULL default '0000',
  "AAABMPNZ" varchar(4) NOT NULL default ' ',
  "AAABMPOA" varchar(6) NOT NULL default '000000',
  "AAABMPOB" varchar(4) NOT NULL default '0000',
  "AAABMPOL" varchar(12) NOT NULL default ' ',
  "AAABMPOM" varchar(8) NOT NULL default '00000000',
  "AAABMPON" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRCJ","AAABMRCQ","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMPPO","AAABMPNY","AAABMPNZ","AAABMPOA","AAABMPOB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMPOC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRDF" varchar(13) NOT NULL default '0000000000000',
  "AAABMRCJ" varchar(8) NOT NULL default '00000000',
  "AAABMRCQ" varchar(8) NOT NULL default '00000000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMPOL" varchar(12) NOT NULL default ' ',
  "AAABMPOM" varchar(8) NOT NULL default '00000000',
  "AAABMPON" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRDF","AAABMRCJ","AAABMRCQ","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMPOW" (
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
  KEY "AAABMPPW" ("AAABMRIL","AAABMRHL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMPPZ" (
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
CREATE TABLE "AAABMPUR" (
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
  "AAABMQDP" varchar(10) NOT NULL default ' ',
  "AAABMQDQ" varchar(13) NOT NULL default ' ',
  "AAABMQDR" varchar(1) NOT NULL default ' ',
  "AAABMQDS" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
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
  "AAABMPWJ" varchar(2) NOT NULL default ' ',
  "AAABMPWK" varchar(2) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMPWM" varchar(8) NOT NULL default '00000000',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMQCG"),
  UNIQUE KEY "AAABMPWS" ("AAABMQHU","AAABMQCJ","AAABMQHY","AAABMQCL","AAABMQDO","AAABMQHV","AAABMQCO"),
  KEY "AAABMPXA" ("AAABMQHU","AAABMQHZ"),
  KEY "AAABMPXD" ("AAABMQHU","AAABMQCV","AAABMQCW","AAABMQCX","AAABMQDO","AAABMQHV","AAABMQDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMPXL" (
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
  "AAABMQDP" varchar(10) NOT NULL default ' ',
  "AAABMQDQ" varchar(13) NOT NULL default ' ',
  "AAABMQDR" varchar(1) NOT NULL default ' ',
  "AAABMQDS" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
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
  UNIQUE KEY "AAABMQCH" ("AAABMQHU","AAABMQCJ","AAABMQHY","AAABMQCL","AAABMQDO","AAABMQHV","AAABMQCO","AAABMQHS"),
  KEY "AAABMQCQ" ("AAABMQHU","AAABMQHZ"),
  KEY "AAABMQCT" ("AAABMQHU","AAABMQCV","AAABMQCW","AAABMQCX","AAABMQDO","AAABMQHV","AAABMQDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMQDB" (
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
  "AAABMQDP" varchar(10) NOT NULL default ' ',
  "AAABMQDQ" varchar(13) NOT NULL default ' ',
  "AAABMQDR" varchar(1) NOT NULL default ' ',
  "AAABMQDS" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
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
  KEY "AAABMQHT" ("AAABMQHU","AAABMQHV","AAABMQHW","AAABMQHX","AAABMQHY","AAABMQHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMQIA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMQIK" varchar(8) NOT NULL default '00000000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMQIN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMQIJ","AAABMQIK","AAABMQIL"),
  KEY "AAABMQIM" ("AAABMQIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMQIO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIZ" varchar(4) NOT NULL default '0000',
  "AAABMQJA" varchar(4) NOT NULL default ' ',
  "AAABMQJB" varchar(4) NOT NULL default '0000',
  "AAABMQJC" varchar(13) NOT NULL default '0000000000000',
  "AAABMQJD" varchar(4) NOT NULL default '0000',
  "AAABMRFP" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMQIZ","AAABMQJA","AAABMQJB","AAABMQJC","AAABMQJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMQJE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABMRIL","AAABMRIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMQJJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABMRGE" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIN","AAABMRGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMQJQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABMQJT" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMQJW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABMRGE" varchar(4) NOT NULL default '0000',
  "AAABMQKT" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIN","AAABMRGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMQKE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMQKL" varchar(4) NOT NULL default '0000',
  "AAABMQKI" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMQKL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMQKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMQKT" varchar(4) NOT NULL default '0000',
  "AAABMQKQ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMQKT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMQKU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABMQLR" varchar(4) NOT NULL default '0000',
  "AAABMQLI" varchar(8) NOT NULL default '00000000',
  "AAABMQLJ" varchar(8) NOT NULL default '00000000',
  "AAABMQLA" varchar(6) NOT NULL default '000000',
  "AAABMQLB" varchar(6) NOT NULL default '000000',
  "AAABMQLZ" varchar(4) NOT NULL default '0000',
  "AAABMRIF" varchar(1) NOT NULL default ' ',
  "AAABMRIG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRIN","AAABMQLR","AAABMQLI","AAABMQLJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMQLK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMQLR" varchar(4) NOT NULL default '0000',
  "AAABMQLO" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMQLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMQLS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMQLZ" varchar(4) NOT NULL default '0000',
  "AAABMQLW" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMQLZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMQMA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMRCQ" varchar(8) NOT NULL default '00000000',
  "AAABMRCJ" varchar(8) NOT NULL default '00000000',
  "AAABMRDI" varchar(8) NOT NULL default '00000000',
  "AAABMRDJ" varchar(6) NOT NULL default '000000',
  "AAABMQQF" varchar(4) NOT NULL default ' ',
  "AAABMQQG" varchar(2) NOT NULL default ' ',
  "AAABMQQH" varchar(2) NOT NULL default ' ',
  "AAABMQMN" varchar(10) NOT NULL default ' ',
  "AAABMQMO" varchar(4) NOT NULL default ' ',
  "AAABMQMP" varchar(2) NOT NULL default ' ',
  "AAABMQMQ" varchar(2) NOT NULL default ' ',
  "AAABMQMR" varchar(10) NOT NULL default ' ',
  "AAABMQMS" varchar(4) NOT NULL default ' ',
  "AAABMQMT" varchar(2) NOT NULL default ' ',
  "AAABMQMU" varchar(2) NOT NULL default ' ',
  "AAABMQMV" varchar(10) NOT NULL default ' ',
  "AAABMRDE" varchar(10) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQMY" varchar(15) NOT NULL default ' ',
  "AAABMQMZ" varchar(30) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  "AAABMQNC" varchar(3) NOT NULL default ' ',
  "AAABMQND" varchar(3) NOT NULL default ' ',
  "AAABMQNE" varchar(3) NOT NULL default ' ',
  "AAABMQNF" varchar(2) NOT NULL default ' ',
  "AAABMQNG" varchar(2) NOT NULL default ' ',
  "AAABMQNH" varchar(30) NOT NULL default ' ',
  "AAABMQNI" varchar(8) NOT NULL default '00000000',
  "AAABMQNJ" varchar(30) NOT NULL default ' ',
  "AAABMQNK" varchar(1) NOT NULL default ' ',
  "AAABMQNL" varchar(8) NOT NULL default '00000000',
  "AAABMQNM" varchar(30) NOT NULL default ' ',
  "AAABMQNN" varchar(1) NOT NULL default ' ',
  "AAABMQNO" varchar(8) NOT NULL default '00000000',
  "AAABMQNP" varchar(30) NOT NULL default ' ',
  "AAABMQNQ" varchar(1) NOT NULL default ' ',
  "AAABMQNR" varchar(8) NOT NULL default '00000000',
  "AAABMQNS" varchar(30) NOT NULL default ' ',
  "AAABMQNT" varchar(1) NOT NULL default ' ',
  "AAABMQNU" varchar(8) NOT NULL default '00000000',
  "AAABMQNV" varchar(30) NOT NULL default ' ',
  "AAABMQNW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMRCQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMQOD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRCQ" varchar(8) NOT NULL default '00000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMQPJ" varchar(4) NOT NULL default '0000',
  "AAABMRCU" varchar(5) NOT NULL default '00000',
  "AAABMRCJ" varchar(8) NOT NULL default '00000000',
  "AAABMRDI" varchar(8) NOT NULL default '00000000',
  "AAABMRDJ" varchar(6) NOT NULL default '000000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMQOQ" varchar(10) NOT NULL default ' ',
  "AAABMQQK" varchar(10) NOT NULL default ' ',
  "AAABMQOS" varchar(4) NOT NULL default ' ',
  "AAABMQOT" varchar(2) NOT NULL default ' ',
  "AAABMQOU" varchar(2) NOT NULL default ' ',
  "AAABMQOV" varchar(10) NOT NULL default ' ',
  "AAABMQOW" varchar(4) NOT NULL default ' ',
  "AAABMQWT" varchar(18) NOT NULL default ' ',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAABMQOZ" decimal(7,3) NOT NULL default '0.000',
  "AAABMQPA" varchar(1) NOT NULL default ' ',
  "AAABMQPB" varchar(1) NOT NULL default ' ',
  "AAABMQPC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRCQ","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMQPJ","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMQPL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRCQ" varchar(8) NOT NULL default '00000000',
  "AAABMRCJ" varchar(8) NOT NULL default '00000000',
  "AAABMRDI" varchar(8) NOT NULL default '00000000',
  "AAABMRDJ" varchar(6) NOT NULL default '000000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMQPU" varchar(1) NOT NULL default ' ',
  "AAABMRDD" varchar(1) NOT NULL default ' ',
  "AAABMQPW" varchar(5) NOT NULL default '00000',
  "AAABMQPX" varchar(5) NOT NULL default '00000',
  "AAABMQPY" varchar(5) NOT NULL default '00000',
  "AAABMQPZ" varchar(5) NOT NULL default '00000',
  "AAABMQQA" varchar(5) NOT NULL default '00000',
  "AAABMRHH" varchar(30) NOT NULL default ' ',
  "AAABMRHC" varchar(30) NOT NULL default ' ',
  "AAABMQQD" varchar(8) NOT NULL default '00000000',
  "AAABMQQE" varchar(8) NOT NULL default '00000000',
  "AAABMQQF" varchar(4) NOT NULL default ' ',
  "AAABMQQG" varchar(2) NOT NULL default ' ',
  "AAABMQQH" varchar(2) NOT NULL default ' ',
  "AAABMQQI" varchar(10) NOT NULL default ' ',
  "AAABMQQJ" varchar(50) NOT NULL default ' ',
  "AAABMQQK" varchar(10) NOT NULL default ' ',
  "AAABMQQL" varchar(50) NOT NULL default ' ',
  "AAABMQQM" varchar(10) NOT NULL default ' ',
  "AAABMQQN" varchar(50) NOT NULL default ' ',
  "AAABMQQO" varchar(10) NOT NULL default ' ',
  "AAABMQQP" varchar(50) NOT NULL default ' ',
  "AAABMQQQ" varchar(10) NOT NULL default ' ',
  "AAABMQQR" varchar(50) NOT NULL default ' ',
  "AAABMRFB" varchar(5) NOT NULL default ' ',
  "AAABMQQT" varchar(8) NOT NULL default '00000000',
  "AAABMQQU" varchar(13) NOT NULL default '0000000000000',
  "AAABMQQV" varchar(13) NOT NULL default '0000000000000',
  "AAABMQQW" varchar(13) NOT NULL default '0000000000000',
  "AAABMQQX" varchar(13) NOT NULL default '0000000000000',
  "AAABMQQY" decimal(16,2) NOT NULL default '0.00',
  "AAABMQQZ" decimal(16,2) NOT NULL default '0.00',
  "AAABMQRA" decimal(16,2) NOT NULL default '0.00',
  "AAABMQRB" decimal(16,2) NOT NULL default '0.00',
  "AAABMQRC" decimal(16,2) NOT NULL default '0.00',
  "AAABMQRD" decimal(7,3) NOT NULL default '0.000',
  "AAABMQRE" varchar(6) NOT NULL default ' ',
  "AAABMQRF" varchar(20) NOT NULL default ' ',
  "AAABMQRG" varchar(10) NOT NULL default ' ',
  "AAABMQRH" varchar(20) NOT NULL default ' ',
  "AAABMQRI" decimal(16,2) NOT NULL default '0.00',
  "AAABMQRJ" decimal(16,2) NOT NULL default '0.00',
  "AAABMQRK" varchar(8) NOT NULL default '00000000',
  "AAABMQRL" varchar(8) NOT NULL default '00000000',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAABMQRN" decimal(15,5) NOT NULL default '0.00000',
  "AAABMQRO" decimal(15,5) NOT NULL default '0.00000',
  "AAABMQRP" decimal(15,5) NOT NULL default '0.00000',
  "AAABMQRQ" decimal(15,5) NOT NULL default '0.00000',
  "AAABMQRR" decimal(16,2) NOT NULL default '0.00',
  "AAABMQRS" decimal(16,2) NOT NULL default '0.00',
  "AAABMQRT" decimal(16,2) NOT NULL default '0.00',
  "AAABMQRU" varchar(6) NOT NULL default ' ',
  "AAABMQRV" varchar(6) NOT NULL default ' ',
  "AAABMQRW" varchar(18) NOT NULL default ' ',
  "AAABMQRX" varchar(18) NOT NULL default ' ',
  "AAABMQRY" decimal(7,3) NOT NULL default '0.000',
  "AAABMQRZ" decimal(7,3) NOT NULL default '0.000',
  "AAABMQSA" decimal(7,3) NOT NULL default '0.000',
  "AAABMQSB" decimal(16,2) NOT NULL default '0.00',
  "AAABMQSC" decimal(15,5) NOT NULL default '0.00000',
  "AAABMQSD" varchar(3) NOT NULL default ' ',
  "AAABMQSE" decimal(17,5) NOT NULL default '0.00000',
  "AAABMQSF" decimal(16,2) NOT NULL default '0.00',
  "AAABMQSG" decimal(15,5) NOT NULL default '0.00000',
  "AAABMQSH" varchar(3) NOT NULL default ' ',
  "AAABMQSI" decimal(17,5) NOT NULL default '0.00000',
  "AAABMQSJ" decimal(16,2) NOT NULL default '0.00',
  "AAABMQSK" decimal(15,5) NOT NULL default '0.00000',
  "AAABMQSL" varchar(3) NOT NULL default ' ',
  "AAABMQSM" decimal(17,5) NOT NULL default '0.00000',
  "AAABMQSN" decimal(7,3) NOT NULL default '0.000',
  "AAABMQSO" decimal(7,3) NOT NULL default '0.000',
  "AAABMQSP" decimal(7,3) NOT NULL default '0.000',
  "AAABMQSQ" decimal(16,2) NOT NULL default '0.00',
  "AAABMQSR" decimal(16,2) NOT NULL default '0.00',
  "AAABMQSS" decimal(16,2) NOT NULL default '0.00',
  "AAABMQST" decimal(16,2) NOT NULL default '0.00',
  "AAABMQSU" decimal(16,2) NOT NULL default '0.00',
  "AAABMQSV" decimal(16,2) NOT NULL default '0.00',
  "AAABMQSW" varchar(4) NOT NULL default ' ',
  "AAABMQSX" varchar(12) NOT NULL default ' ',
  "AAABMQSY" varchar(4) NOT NULL default ' ',
  "AAABMQSZ" decimal(16,2) NOT NULL default '0.00',
  "AAABMQTA" varchar(1) NOT NULL default ' ',
  "AAABMQTB" varchar(1) NOT NULL default ' ',
  "AAABMQTC" decimal(16,2) NOT NULL default '0.00',
  "AAABMQTD" decimal(16,2) NOT NULL default '0.00',
  "AAABMQTE" decimal(16,2) NOT NULL default '0.00',
  "AAABMQTF" decimal(16,2) NOT NULL default '0.00',
  "AAABMQTG" decimal(16,2) NOT NULL default '0.00',
  "AAABMQTH" decimal(16,2) NOT NULL default '0.00',
  "AAABMQTI" decimal(16,2) NOT NULL default '0.00',
  "AAABMQTJ" decimal(16,2) NOT NULL default '0.00',
  "AAABMQTK" decimal(16,2) NOT NULL default '0.00',
  "AAABMQTL" decimal(16,2) NOT NULL default '0.00',
  "AAABMQTM" decimal(16,2) NOT NULL default '0.00',
  "AAABMQTN" decimal(16,2) NOT NULL default '0.00',
  "AAABMQTO" varchar(13) NOT NULL default ' ',
  "AAABMQTP" varchar(8) NOT NULL default '00000000',
  "AAABMQTQ" varchar(8) NOT NULL default '00000000',
  "AAABMQTR" decimal(16,2) NOT NULL default '0.00',
  "AAABMQTS" decimal(16,2) NOT NULL default '0.00',
  "AAABMQTT" decimal(7,3) NOT NULL default '0.000',
  "AAABMQTU" decimal(7,3) NOT NULL default '0.000',
  "AAABMQTV" decimal(7,3) NOT NULL default '0.000',
  "AAABMQTW" decimal(7,3) NOT NULL default '0.000',
  "AAABMQTX" decimal(16,2) NOT NULL default '0.00',
  "AAABMQTY" decimal(15,5) NOT NULL default '0.00000',
  "AAABMQTZ" varchar(3) NOT NULL default ' ',
  "AAABMQUA" decimal(17,5) NOT NULL default '0.00000',
  "AAABMQUB" decimal(16,2) NOT NULL default '0.00',
  "AAABMQUC" decimal(15,5) NOT NULL default '0.00000',
  "AAABMQUD" varchar(3) NOT NULL default ' ',
  "AAABMQUE" decimal(17,5) NOT NULL default '0.00000',
  "AAABMQUF" decimal(16,2) NOT NULL default '0.00',
  "AAABMQUG" decimal(15,5) NOT NULL default '0.00000',
  "AAABMQUH" varchar(3) NOT NULL default ' ',
  "AAABMQUI" decimal(17,5) NOT NULL default '0.00000',
  "AAABMQUJ" decimal(16,2) NOT NULL default '0.00',
  "AAABMQUK" decimal(15,5) NOT NULL default '0.00000',
  "AAABMQUL" varchar(3) NOT NULL default ' ',
  "AAABMQUM" decimal(17,5) NOT NULL default '0.00000',
  "AAABMQUN" decimal(16,2) NOT NULL default '0.00',
  "AAABMQUO" decimal(15,5) NOT NULL default '0.00000',
  "AAABMQUP" varchar(3) NOT NULL default ' ',
  "AAABMQUQ" decimal(17,5) NOT NULL default '0.00000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRCQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMQUU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRCQ" varchar(8) NOT NULL default '00000000',
  "AAABMRCJ" varchar(8) NOT NULL default '00000000',
  "AAABMRDI" varchar(8) NOT NULL default '00000000',
  "AAABMRDJ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRCQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMQVE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRCQ" varchar(8) NOT NULL default '00000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMRCU" varchar(5) NOT NULL default '00000',
  "AAABMRCJ" varchar(8) NOT NULL default '00000000',
  "AAABMRDI" varchar(8) NOT NULL default '00000000',
  "AAABMRDJ" varchar(6) NOT NULL default '000000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMQVQ" varchar(60) NOT NULL default ' ',
  "AAABMQVR" varchar(4) NOT NULL default '0000',
  "AAABMQVS" varchar(4) NOT NULL default '0000',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQVW" varchar(18) NOT NULL default ' ',
  "AAABMQVX" decimal(16,2) NOT NULL default '0.00',
  "AAABMRFB" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRCQ","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMQWG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRCQ" varchar(8) NOT NULL default '00000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMRCU" varchar(5) NOT NULL default '00000',
  "AAABMRCJ" varchar(8) NOT NULL default '00000000',
  "AAABMRDI" varchar(8) NOT NULL default '00000000',
  "AAABMRDJ" varchar(6) NOT NULL default '000000',
  "AAABMQWR" varchar(30) NOT NULL default ' ',
  "AAABMQWS" varchar(4) NOT NULL default ' ',
  "AAABMQWT" varchar(18) NOT NULL default ' ',
  "AAABMQWU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRCQ","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMQXC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQXQ" varchar(4) NOT NULL default ' ',
  "AAABMQXR" varchar(4) NOT NULL default '0000',
  "AAABMQXS" varchar(30) NOT NULL default ' ',
  "AAABMQXH" varchar(30) NOT NULL default ' ',
  "AAABMRDI" varchar(8) NOT NULL default '00000000',
  "AAABMRDJ" varchar(6) NOT NULL default '000000',
  "AAABMQXK" varchar(8) NOT NULL default '00000000',
  "AAABMQXL" decimal(13,0) NOT NULL default '0',
  "AAABMQXM" decimal(13,0) NOT NULL default '0',
  "AAABMQXN" varchar(1) NOT NULL default ' ',
  "AAABMQXO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQXQ","AAABMQXR","AAABMQXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMQXT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRCQ" varchar(8) NOT NULL default '00000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMRCJ" varchar(8) NOT NULL default '00000000',
  "AAABMRDI" varchar(8) NOT NULL default '00000000',
  "AAABMRDJ" varchar(6) NOT NULL default '000000',
  "AAABMQYD" varchar(9) NOT NULL default '000000000',
  "AAABMQYE" varchar(30) NOT NULL default ' ',
  "AAABMQYF" varchar(8) NOT NULL default '00000000',
  "AAABMQYG" varchar(1) NOT NULL default ' ',
  "AAABMQYH" varchar(4) NOT NULL default ' ',
  "AAABMQYI" varchar(4) NOT NULL default '0000',
  "AAABMQYJ" varchar(1) NOT NULL default ' ',
  "AAABMQYK" varchar(1) NOT NULL default ' ',
  "AAABMQYL" varchar(1) NOT NULL default ' ',
  "AAABMQYM" varchar(1) NOT NULL default ' ',
  "AAABMQYN" varchar(1) NOT NULL default ' ',
  "AAABMQYO" varchar(1) NOT NULL default ' ',
  "AAABMQYP" varchar(1) NOT NULL default ' ',
  "AAABMQYQ" varchar(1) NOT NULL default ' ',
  "AAABMQYR" varchar(1) NOT NULL default ' ',
  "AAABMQYS" varchar(1) NOT NULL default ' ',
  "AAABMQYT" varchar(1) NOT NULL default ' ',
  "AAABMQYU" varchar(1) NOT NULL default ' ',
  "AAABMQYV" varchar(1) NOT NULL default ' ',
  "AAABMQYW" varchar(1) NOT NULL default ' ',
  "AAABMQYX" varchar(1) NOT NULL default ' ',
  "AAABMQYY" varchar(1) NOT NULL default ' ',
  "AAABMQYZ" varchar(1) NOT NULL default ' ',
  "AAABMQZA" varchar(8) NOT NULL default '00000000',
  "AAABMQZB" varchar(10) NOT NULL default '0000000000',
  "AAABMQZC" varchar(8) NOT NULL default '00000000',
  "AAABMQZD" varchar(8) NOT NULL default '00000000',
  "AAABMQZE" varchar(4) NOT NULL default ' ',
  "AAABMQZF" varchar(6) NOT NULL default '000000',
  "AAABMQZG" varchar(4) NOT NULL default '0000',
  "AAABMQZH" varchar(30) NOT NULL default ' ',
  "AAABMQZI" decimal(17,5) NOT NULL default '0.00000',
  "AAABMQZJ" varchar(1) NOT NULL default ' ',
  "AAABMQZK" varchar(4) NOT NULL default ' ',
  "AAABMQZL" varchar(6) NOT NULL default '000000',
  "AAABMQZM" varchar(4) NOT NULL default '0000',
  "AAABMQZN" varchar(30) NOT NULL default ' ',
  "AAABMQZO" decimal(17,5) NOT NULL default '0.00000',
  "AAABMQZP" varchar(1) NOT NULL default ' ',
  "AAABMQZQ" decimal(15,5) NOT NULL default '0.00000',
  "AAABMRAQ" varchar(3) NOT NULL default ' ',
  "AAABMQZS" decimal(17,5) NOT NULL default '0.00000',
  "AAABMQZT" decimal(17,5) NOT NULL default '0.00000',
  "AAABMQZU" decimal(16,2) NOT NULL default '0.00',
  "AAABMRFB" varchar(5) NOT NULL default ' ',
  "AAABMQZW" decimal(17,5) NOT NULL default '0.00000',
  "AAABMQZX" decimal(17,5) NOT NULL default '0.00000',
  "AAABMRBS" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRCQ","AAABMRGQ","AAABMRGR","AAABMRGS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMRAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMRBY" varchar(4) NOT NULL default '0000',
  "AAABMRCQ" varchar(8) NOT NULL default '00000000',
  "AAABMRCJ" varchar(8) NOT NULL default '00000000',
  "AAABMRDI" varchar(8) NOT NULL default '00000000',
  "AAABMRDJ" varchar(6) NOT NULL default '000000',
  "AAABMRAQ" varchar(3) NOT NULL default ' ',
  "AAABMRAR" decimal(15,5) NOT NULL default '0.00000',
  "AAABMRAS" decimal(15,5) NOT NULL default '0.00000',
  "AAABMRAT" decimal(15,5) NOT NULL default '0.00000',
  "AAABMRAU" decimal(15,5) NOT NULL default '0.00000',
  "AAABMRAV" decimal(15,5) NOT NULL default '0.00000',
  "AAABMRAW" decimal(15,5) NOT NULL default '0.00000',
  "AAABMRAX" decimal(15,5) NOT NULL default '0.00000',
  "AAABMRAY" decimal(15,5) NOT NULL default '0.00000',
  "AAABMRAZ" decimal(15,5) NOT NULL default '0.00000',
  "AAABMRBA" decimal(15,5) NOT NULL default '0.00000',
  "AAABMRBB" decimal(15,5) NOT NULL default '0.00000',
  "AAABMRBC" decimal(15,5) NOT NULL default '0.00000',
  "AAABMRBD" decimal(15,5) NOT NULL default '0.00000',
  "AAABMRFB" varchar(5) NOT NULL default ' ',
  "AAABMRBF" decimal(16,2) NOT NULL default '0.00',
  "AAABMRBG" decimal(16,2) NOT NULL default '0.00',
  "AAABMRBH" decimal(16,2) NOT NULL default '0.00',
  "AAABMRBI" decimal(16,2) NOT NULL default '0.00',
  "AAABMRBJ" decimal(16,2) NOT NULL default '0.00',
  "AAABMRBK" decimal(16,2) NOT NULL default '0.00',
  "AAABMRBL" decimal(16,2) NOT NULL default '0.00',
  "AAABMRBM" decimal(16,2) NOT NULL default '0.00',
  "AAABMRBN" decimal(16,2) NOT NULL default '0.00',
  "AAABMRBO" decimal(16,2) NOT NULL default '0.00',
  "AAABMRBP" decimal(16,2) NOT NULL default '0.00',
  "AAABMRBQ" decimal(16,2) NOT NULL default '0.00',
  "AAABMRBR" decimal(16,2) NOT NULL default '0.00',
  "AAABMRBS" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMRBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMRBZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRCP" varchar(1) NOT NULL default ' ',
  "AAABMRCQ" varchar(8) NOT NULL default '00000000',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  "AAABMRCU" varchar(5) NOT NULL default '00000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCJ" varchar(8) NOT NULL default '00000000',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  "AAABMRDI" varchar(8) NOT NULL default '00000000',
  "AAABMRDJ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRCP","AAABMRCQ","AAABMRGQ","AAABMRGR","AAABMRGS","AAABMRCU","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMRCW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRDS" varchar(4) NOT NULL default '0000',
  "AAABMRFB" varchar(5) NOT NULL default ' ',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRDC" varchar(4) NOT NULL default '0000',
  "AAABMRDD" varchar(1) NOT NULL default ' ',
  "AAABMRDE" varchar(10) NOT NULL default ' ',
  "AAABMRDF" varchar(13) NOT NULL default '0000000000000',
  "AAABMRDG" varchar(8) NOT NULL default '00000000',
  "AAABMRDH" varchar(8) NOT NULL default '00000000',
  "AAABMRDI" varchar(8) NOT NULL default '00000000',
  "AAABMRDJ" varchar(6) NOT NULL default '000000',
  "AAABMRDK" varchar(30) NOT NULL default ' ',
  "AAABMRDL" varchar(4) NOT NULL default '0000',
  "AAABMRDM" varchar(4) NOT NULL default '0000',
  "AAABMRDN" varchar(4) NOT NULL default '0000',
  "AAABMRDO" varchar(4) NOT NULL default '0000',
  "AAABMRDP" decimal(16,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRDS","AAABMRFB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMRDU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRGB" varchar(4) NOT NULL default '0000',
  "AAABMRDY" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMRGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMREC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREH" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMREI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREQ" varchar(15) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREM" varchar(2) NOT NULL default ' ',
  "AAABMREN" varchar(40) NOT NULL default ' ',
  "AAABMREO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMRER" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABMRFV" varchar(13) NOT NULL default '0000000000000',
  "AAABMRFW" varchar(4) NOT NULL default '0000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMREY" varchar(4) NOT NULL default '0000',
  "AAABMREZ" varchar(4) NOT NULL default '0000',
  "AAABMRFA" varchar(50) NOT NULL default ' ',
  "AAABMRFB" varchar(5) NOT NULL default ' ',
  "AAABMRFC" decimal(16,2) NOT NULL default '0.00',
  "AAABMRFD" decimal(16,2) NOT NULL default '0.00',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAABMRFF" decimal(16,2) NOT NULL default '0.00',
  "AAABMRFG" decimal(15,5) NOT NULL default '0.00000',
  "AAABMRFH" decimal(15,5) NOT NULL default '0.00000',
  "AAABMRFI" decimal(7,3) NOT NULL default '0.000',
  "AAABMRFJ" decimal(7,3) NOT NULL default '0.000',
  "AAABMRFK" varchar(8) NOT NULL default '00000000',
  "AAABMRFL" varchar(8) NOT NULL default '00000000',
  "AAABMRFM" varchar(2) NOT NULL default ' ',
  "AAABMRFN" varchar(50) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default '00',
  "AAABMRFP" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMRFR" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRIN","AAABMRFV","AAABMRFW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMRFX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABMRGE" varchar(4) NOT NULL default '0000',
  "AAABMRGB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRIN","AAABMRGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMRGF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRHK" varchar(6) NOT NULL default '000000',
  "AAABMRHH" varchar(30) NOT NULL default ' ',
  "AAABMRGK" varchar(6) NOT NULL default '000000',
  "AAABMRGL" varchar(6) NOT NULL default '000000',
  "AAABMRGM" varchar(6) NOT NULL default '000000',
  "AAABMRGN" varchar(2) NOT NULL default '00',
  "AAABMRGO" varchar(1) NOT NULL default ' ',
  "AAABMRGP" varchar(4) NOT NULL default ' ',
  "AAABMRGQ" varchar(4) NOT NULL default ' ',
  "AAABMRGR" varchar(6) NOT NULL default '000000',
  "AAABMRGS" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMRGW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHL" varchar(13) NOT NULL default '0000000000000',
  "AAABMRHK" varchar(6) NOT NULL default '000000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRHH" varchar(30) NOT NULL default ' ',
  "AAABMRHC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHL","AAABMRHK","AAABMRHS","AAABMRHH"),
  KEY "AAABMRHI" ("AAABMRIL","AAABMRHK","AAABMRHL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMRHM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRHT" varchar(4) NOT NULL default '0000',
  "AAABMRHQ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMRHT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMRHU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRIM" varchar(4) NOT NULL default '0000',
  "AAABMRIN" varchar(14) NOT NULL default '00000000000000',
  "AAABMRIO" varchar(14) NOT NULL default ' ',
  "AAABMRIP" varchar(4) NOT NULL default '0000',
  "AAABMRIQ" varchar(4) NOT NULL default '0000',
  "AAABMRIB" decimal(13,3) NOT NULL default '0.000',
  "AAABMRIC" varchar(3) NOT NULL default ' ',
  "AAABMRID" decimal(12,2) NOT NULL default '0.00',
  "AAABMRIE" varchar(5) NOT NULL default ' ',
  "AAABMRIF" varchar(1) NOT NULL default ' ',
  "AAABMRIG" varchar(1) NOT NULL default ' ',
  "AAABMRIH" varchar(12) NOT NULL default ' ',
  "AAABMRII" varchar(8) NOT NULL default '00000000',
  "AAABMRIJ" varchar(12) NOT NULL default ' ',
  "AAABMRIK" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMRIM","AAABMRIN","AAABMRIO","AAABMRIP","AAABMRIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--enable_query_log
