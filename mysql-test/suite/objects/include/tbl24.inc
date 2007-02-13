# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+
# |                             tbl24.inc                              |
# |                        Copyright 2006 by MySQL AB                  |
# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+

--disable_query_log
CREATE TABLE "AAAAWDNP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABMCQQ" varchar(3) NOT NULL default ' ',
  "AAAAWDNU" varchar(2) NOT NULL default ' ',
  "AAAAWDNV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMEKQ","AAABMPTW","AAABMCQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWDOA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRXK" varchar(4) NOT NULL default ' ',
  "AAAAWDOD" varchar(1) NOT NULL default ' ',
  "AAAAWDOE" varchar(10) NOT NULL default ' ',
  "AAAAWDOF" varchar(1) NOT NULL default ' ',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAAAWDOH" varchar(10) NOT NULL default ' ',
  "AAAAWDOI" varchar(10) NOT NULL default ' ',
  "AAAAWDOJ" varchar(10) NOT NULL default ' ',
  "AAAAWDOK" varchar(10) NOT NULL default ' ',
  "AAAAWDOL" varchar(10) NOT NULL default ' ',
  "AAABKRGB" varchar(10) NOT NULL default ' ',
  "AAABMCSL" varchar(1) NOT NULL default ' ',
  "AAAAWDOO" varchar(1) NOT NULL default ' ',
  "AAAAWDOP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWDOS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIGYU" varchar(14) NOT NULL default ' ',
  "AAABLUAV" varchar(14) NOT NULL default ' ',
  "AAABMJWN" varchar(14) NOT NULL default ' ',
  "AAABBKIK" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABIGYU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWDPA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIGYU" varchar(14) NOT NULL default ' ',
  "AAABEPBY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABIGYU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWDPI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWDQM" varchar(14) NOT NULL default ' ',
  "AAABBGEH" varchar(14) NOT NULL default ' ',
  "AAAAWDPM" varchar(1) NOT NULL default ' ',
  "AAABELUP" varchar(2) NOT NULL default ' ',
  "AAABCLSH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWDQM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWDPR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWDQM" varchar(14) NOT NULL default ' ',
  "AAABEPBY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWDQM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWDPZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKRXK" varchar(4) NOT NULL default ' ',
  "AAAAWDQD" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKRXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWDQH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWDQM" varchar(14) NOT NULL default ' ',
  "AAABIGYU" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWDQM","AAABIGYU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWDQO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABJWRR" varchar(3) NOT NULL default ' ',
  "AAABMGVL" varchar(15) NOT NULL default ' ',
  "AAABLXIZ" varchar(16) NOT NULL default ' ',
  "AAABLCJJ" varchar(16) NOT NULL default ' ',
  "AAABLZCP" varchar(30) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJKXJ" varchar(4) NOT NULL default ' ',
  "AAAAWDQZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCTJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWDRD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAABMCRD" varchar(2) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  "AAAAWDRT" varchar(2) NOT NULL default ' ',
  "AAAAWDRU" varchar(1) NOT NULL default ' ',
  "AAABLOLZ" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCTG","AAABMCRD","AAABMCTJ","AAABKWCV","AAAAWDRT","AAAAWDRU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWDRV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  "AAABLOLZ" varchar(8) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWDSC" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCTJ","AAABKWCV","AAABLOLZ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWDSJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  "AAABLOLZ" varchar(8) NOT NULL default ' ',
  "AAABMCRU" varchar(1) NOT NULL default ' ',
  "AAAAWDSQ" varchar(2) NOT NULL default ' ',
  "AAAAYDLR" varchar(1) NOT NULL default ' ',
  "AAAAWQXF" varchar(3) NOT NULL default '000',
  "AAAAWDST" varchar(2) NOT NULL default ' ',
  "AAABKPVY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCTJ","AAABKWCV","AAABLOLZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWDTA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAAAWDTE" varchar(1) NOT NULL default ' ',
  "AAAAWDTF" varchar(1) NOT NULL default ' ',
  "AAABJWRW" varchar(2) NOT NULL default ' ',
  "AAAAWDTH" varchar(2) NOT NULL default '00',
  "AAAAWDTI" varchar(2) NOT NULL default '00',
  "AAAAWDTJ" varchar(1) NOT NULL default ' ',
  "AAAAWDTK" varchar(3) NOT NULL default ' ',
  "AAAAWDTL" varchar(18) NOT NULL default ' ',
  "AAAAWDTM" varchar(2) NOT NULL default ' ',
  "AAAAWDTN" varchar(15) NOT NULL default ' ',
  "AAAAWDTO" varchar(15) NOT NULL default ' ',
  "AAABKCHP" varchar(20) NOT NULL default ' ',
  "AAAAWDTQ" varchar(10) NOT NULL default ' ',
  "AAAAWDTR" varchar(1) NOT NULL default ' ',
  "AAAAWDTS" varchar(5) NOT NULL default ' ',
  "AAAAWDTT" varchar(18) NOT NULL default ' ',
  "AAAAWDTU" varchar(1) NOT NULL default ' ',
  "AAAAWDTV" varchar(1) NOT NULL default ' ',
  "AAAAWDTW" varchar(1) NOT NULL default ' ',
  "AAAAWDTX" varchar(1) NOT NULL default ' ',
  "AAAAWDTY" varchar(8) NOT NULL default '00000000',
  "AAAAWDTZ" varchar(5) NOT NULL default '00000',
  "AAAAWDUA" varchar(3) NOT NULL default '000',
  "AAAAWDUB" varchar(8) NOT NULL default '00000000',
  "AAAAWDUC" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCTJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWDUG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  "AAABMGVM" varchar(18) NOT NULL default ' ',
  "AAABJWRQ" varchar(2) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAWDUO" varchar(27) NOT NULL default ' ',
  "AAAAWDUP" varchar(5) NOT NULL default ' ',
  "AAAAWDUQ" varchar(24) NOT NULL default ' ',
  "AAABMCRF" varchar(10) NOT NULL default ' ',
  "AAAAWDUS" varchar(5) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABEMHV" varchar(10) NOT NULL default ' ',
  "AAAAWDUV" varchar(3) NOT NULL default '000',
  "AAAAWDUW" varchar(5) NOT NULL default ' ',
  "AAAAWDUX" varchar(5) NOT NULL default ' ',
  "AAAAWDUY" varchar(5) NOT NULL default ' ',
  "AAAAWDUZ" varchar(5) NOT NULL default ' ',
  "AAAAWGFJ" varchar(10) NOT NULL default ' ',
  "AAAAWDVB" varchar(10) NOT NULL default ' ',
  "AAABCPCU" varchar(2) NOT NULL default ' ',
  "AAAAWDVD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCTJ","AAABKWCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWDVI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAAAWDVZ" varchar(5) NOT NULL default ' ',
  "AAAAWDVN" varchar(5) NOT NULL default ' ',
  "AAABJWRR" varchar(3) NOT NULL default ' ',
  "AAABMGVL" varchar(15) NOT NULL default ' ',
  "AAABMGVM" varchar(18) NOT NULL default ' ',
  "AAAAWDVR" varchar(60) NOT NULL default ' ',
  "AAABJWRQ" varchar(2) NOT NULL default ' ',
  "AAABJRLW" varchar(20) NOT NULL default ' ',
  "AAABLCFI" varchar(10) NOT NULL default ' ',
  "AAAAWDVV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCTJ","AAAAWDVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWDWA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  "AAABMJVD" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABMCTJ","AAABKWCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWDWM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCZV" varchar(4) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLXSA" varchar(6) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAABLXJU" varchar(3) NOT NULL default ' ',
  "AAAAWDWT" decimal(15,2) NOT NULL default '0.00',
  "AAABLXJT" varchar(3) NOT NULL default ' ',
  "AAAAWDWV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCZV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWDWY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCZV" varchar(4) NOT NULL default ' ',
  "AAAAWDXC" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCZV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWDXG" (
  "AAAAWDYL" varchar(5) NOT NULL default ' ',
  "AAABIDWD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAWDYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWDXK" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWDYL" varchar(5) NOT NULL default ' ',
  "AAABMRCK" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAAWDYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWDXQ" (
  "AAAAWDYL" varchar(5) NOT NULL default ' ',
  "AAAAWDYM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAWDYL","AAAAWDYM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWDXV" (
  "AAAAWDYL" varchar(5) NOT NULL default ' ',
  "AAAAWDYM" varchar(1) NOT NULL default ' ',
  "AAAAWDYN" varchar(2) NOT NULL default '00',
  "AAABLVJB" varchar(12) NOT NULL default ' ',
  "AAAAWDYA" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAAAWDYL","AAAAWDYM","AAAAWDYN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWDYE" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWDYL" varchar(5) NOT NULL default ' ',
  "AAAAWDYM" varchar(1) NOT NULL default ' ',
  "AAAAWDYN" varchar(2) NOT NULL default '00',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAAWDYL","AAAAWDYM","AAAAWDYN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWDYO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJWRR" varchar(3) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAABJWRW" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJWRR","AAABMCTG","AAABJWRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWDZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWDZM" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWDZM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWDZF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWDZM" varchar(3) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWDZM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWDZN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  "AAAAWEAC" varchar(3) NOT NULL default ' ',
  "AAAAWDZV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCTJ","AAABMCTG","AAABMPWL","AAABKWCV","AAAAWEAC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEAD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABENPM" varchar(4) NOT NULL default ' ',
  "AAAAWEAQ" varchar(5) NOT NULL default ' ',
  "AAAAWEAR" varchar(5) NOT NULL default ' ',
  "AAAAWEAS" varchar(4) NOT NULL default ' ',
  "AAAAWEAT" varchar(5) NOT NULL default ' ',
  "AAAAWEAU" varchar(5) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABEQMD" varchar(1) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABENPM","AAAAWEAQ","AAAAWEAR","AAAAWEAS","AAAAWEAT","AAAAWEAU","AAABMPWL","AAABEQMD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEAX" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMREL" varchar(4) NOT NULL default ' ',
  "AAAAWEBA" varchar(99) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEBD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  "AAAAWEBI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP","AAABMMVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEBM" (
  "AAABEQVP" varchar(8) NOT NULL default ' ',
  "AAABFFDQ" varchar(5) NOT NULL default ' ',
  "AAABLSMW" varchar(5) NOT NULL default ' ',
  "AAAAWMZK" varchar(1) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABEQVP","AAABFFDQ","AAABLSMW","AAAAWMZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEBW" (
  "AAABEQVP" varchar(8) NOT NULL default ' ',
  "AAABFFDQ" varchar(5) NOT NULL default ' ',
  "AAABLSMW" varchar(5) NOT NULL default ' ',
  "AAABLSTU" varchar(1) NOT NULL default ' ',
  "AAAAZWRR" varchar(2) NOT NULL default '00',
  "AAABELZI" varchar(2) NOT NULL default '00',
  "AAABKAFO" varchar(61) NOT NULL default ' ',
  "AAABELZJ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABEQVP","AAABFFDQ","AAABLSMW","AAABLSTU","AAAAZWRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWECK" (
  "AAAAWEEC" varchar(1) NOT NULL default ' ',
  "AAAAXMWU" varchar(1) NOT NULL default ' ',
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAWEEC","AAAAXMWU","AAABLUAV","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWECT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEQVP" varchar(8) NOT NULL default ' ',
  "AAABFFDQ" varchar(5) NOT NULL default ' ',
  "AAAAWEDW" varchar(3) NOT NULL default ' ',
  "AAABLWIT" varchar(30) NOT NULL default ' ',
  "AAABLWIV" varchar(30) NOT NULL default ' ',
  "AAABLWIX" varchar(30) NOT NULL default ' ',
  "AAABLWIZ" varchar(30) NOT NULL default ' ',
  "AAABLWJB" varchar(30) NOT NULL default ' ',
  "AAAAWFEL" varchar(30) NOT NULL default ' ',
  "AAAAWFEM" varchar(30) NOT NULL default ' ',
  "AAAAWFEN" varchar(30) NOT NULL default ' ',
  "AAAAWFEO" varchar(30) NOT NULL default ' ',
  "AAAAWFEV" varchar(30) NOT NULL default ' ',
  "AAAAWEDI" varchar(2) NOT NULL default '00',
  "AAAAWEDJ" varchar(2) NOT NULL default '00',
  "AAAAWEDK" varchar(2) NOT NULL default '00',
  "AAAAWEDL" varchar(2) NOT NULL default '00',
  "AAAAWEDM" varchar(2) NOT NULL default '00',
  "AAAAWEDN" varchar(2) NOT NULL default '00',
  "AAAAYOOK" varchar(2) NOT NULL default '00',
  "AAAAWEDP" varchar(2) NOT NULL default '00',
  "AAAAWEDQ" varchar(2) NOT NULL default '00',
  "AAAAWEDR" varchar(2) NOT NULL default '00',
  "AAABGACX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEQVP","AAABFFDQ","AAAAWEDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEDX" (
  "AAAAWEEC" varchar(1) NOT NULL default ' ',
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAWEEC","AAABLUAV","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEEF" (
  "AAABEQVP" varchar(8) NOT NULL default ' ',
  "AAABFFDQ" varchar(5) NOT NULL default ' ',
  "AAABLSMW" varchar(5) NOT NULL default ' ',
  "AAABLSTU" varchar(1) NOT NULL default ' ',
  "AAAAZWRR" varchar(2) NOT NULL default '00',
  "AAABELZI" VARBINARY(255),
  "AAABKAFO" varchar(61) NOT NULL default ' ',
  "AAABELZJ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABEQVP","AAABFFDQ","AAABLSMW","AAABLSTU","AAAAZWRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEET" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEQVP" varchar(8) NOT NULL default ' ',
  "AAABFFDQ" varchar(5) NOT NULL default ' ',
  "AAABLSMW" varchar(5) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABEQVP","AAABFFDQ","AAABLSMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEFD" (
  "AAABEQVP" varchar(8) NOT NULL default ' ',
  "AAABFFDQ" varchar(5) NOT NULL default ' ',
  "AAABLSMW" varchar(5) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAAAWEFI" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABEQVP","AAABFFDQ","AAABLSMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEFM" (
  "AAABIYCS" varchar(4) NOT NULL default ' ',
  "AAAAWEFO" varchar(30) NOT NULL default ' ',
  "AAAAWEFP" varchar(30) NOT NULL default ' ',
  "AAAAWEFQ" varchar(30) NOT NULL default ' ',
  "AAAAWEFR" varchar(30) NOT NULL default ' ',
  "AAABJNZY" varchar(1) NOT NULL default ' ',
  "AAABIFMX" varchar(1) NOT NULL default ' ',
  "AAAAWEFU" varchar(1) NOT NULL default ' ',
  "AAAAWEFV" varchar(1) NOT NULL default ' ',
  "AAAAWEFW" varchar(1) NOT NULL default ' ',
  "AAABFUKI" varchar(1) NOT NULL default ' ',
  "AAAAWEFY" varchar(1) NOT NULL default ' ',
  "AAAAWEFZ" varchar(1) NOT NULL default ' ',
  "AAAAWEGA" varchar(1) NOT NULL default ' ',
  "AAABBPEL" varchar(4) NOT NULL default ' ',
  "AAABLECD" varchar(2) NOT NULL default ' ',
  "AAABIASH" varchar(3) NOT NULL default ' ',
  "AAABLTQC" varchar(3) NOT NULL default ' ',
  "AAAAWEGF" varchar(1) NOT NULL default ' ',
  "AAAAWEGG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEGI" (
  "AAAAWEGN" varchar(6) NOT NULL default ' ',
  "AAAAWEGO" varchar(4) NOT NULL default ' ',
  "AAAAWEGP" varchar(12) NOT NULL default ' ',
  "AAAAWEGM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAAWEGN","AAAAWEGO","AAAAWEGP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEGQ" (
  "AAAAWEGU" varchar(4) NOT NULL default ' ',
  "AAABMMVU" varchar(1) NOT NULL default ' ',
  "AAAAWEGT" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAWEGU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEGV" (
  "AAABMMVU" varchar(8) NOT NULL default ' ',
  "AAAAWEHF" varchar(5) NOT NULL default ' ',
  "AAAAWEGY" varchar(1) NOT NULL default ' ',
  "AAABIWUT" varchar(1) NOT NULL default ' ',
  "AAAAWEHA" varchar(1) NOT NULL default ' ',
  "AAAAYVAE" varchar(1) NOT NULL default ' ',
  "AAAAWEHC" varchar(4) NOT NULL default ' ',
  "AAABIZCH" varchar(58) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMVU","AAAAWEHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEHG" (
  "AAABBPEL" varchar(4) NOT NULL default ' ',
  "AAABEQYH" varchar(30) NOT NULL default ' ',
  "AAABGYKT" varchar(30) NOT NULL default ' ',
  "AAAAWEHK" varchar(30) NOT NULL default ' ',
  "AAAAWEHL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABBPEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEHN" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABBPEL" varchar(4) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABBPEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEHT" (
  "AAABEZYX" varchar(1) NOT NULL default ' ',
  "AAABMAHQ" varchar(8) NOT NULL default ' ',
  "AAABIANG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABEZYX","AAABMAHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEHZ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIYCS" varchar(4) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEIF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABDHAG" varchar(3) NOT NULL default ' ',
  "AAAAWEIK" varchar(18) NOT NULL default ' ',
  "AAAAWENG" varchar(18) NOT NULL default ' ',
  "AAABLQPX" varchar(4) NOT NULL default ' ',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAABMLEW" varchar(3) NOT NULL default ' ',
  "AAAAWEIP" varchar(16) NOT NULL default ' ',
  "AAABLHRD" varchar(4) NOT NULL default ' ',
  "AAAAWTLU" varchar(4) NOT NULL default ' ',
  "AAABKGBX" varchar(8) NOT NULL default ' ',
  "AAAAWEIT" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXOB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAWEIZ" decimal(13,2) NOT NULL default '0.00',
  "AAAAWEKG" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEJE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAAAWEJH" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCMO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEJK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAAAWEJP" varchar(1) NOT NULL default ' ',
  "AAAAWEKG" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCSR","AAABLXIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEJW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAWEJZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEKC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABAFKT" varchar(1) NOT NULL default ' ',
  "AAAAWEKG" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABAFKT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEKL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAAAWEKO" tinyint(3) unsigned NOT NULL default '0',
  "AAABMMRQ" varchar(4) NOT NULL default ' ',
  "AAAAWEKQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXOB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWEKY" varchar(1) NOT NULL default ' ',
  "AAABMMRQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWEKY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEKZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  "AAAAWELG" varchar(1) NOT NULL default ' ',
  "AAAAWELD" decimal(10,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAABLARI","AAAAWELG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWELH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAAAWENG" varchar(18) NOT NULL default ' ',
  "AAABLXUH" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABLXOB","AAAAWENG"),
  KEY "AAAAWELP" ("AAABMRIL","AAABLXUH"),
  KEY "AAAAWELS" ("AAABMRIL","AAAAWENG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWELV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAAAWELZ" varchar(20) NOT NULL default ' ',
  "AAAAWEMA" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXOB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEME" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJKSA" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJKSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEMJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJKSA" varchar(2) NOT NULL default ' ',
  "AAAAWEMN" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJKSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEMR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAWEMW" varchar(1) NOT NULL default ' ',
  "AAAAWEMX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBA","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWENC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRYG" varchar(10) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAAAWENG" varchar(18) NOT NULL default ' ',
  "AAAAWENH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRYG","AAABLXOB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWENL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRYG" varchar(10) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAAAWENP" varchar(18) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRYG","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWENU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXOB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWENZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAAAWEOC" varchar(18) NOT NULL default ' ',
  "AAAAWEOD" varchar(12) NOT NULL default ' ',
  "AAABGCTJ" varchar(4) NOT NULL default ' ',
  "AAABLVHV" varchar(31) NOT NULL default ' ',
  "AAABIXYH" varchar(30) NOT NULL default ' ',
  "AAABIXYI" varchar(10) NOT NULL default ' ',
  "AAAAWEOI" varchar(241) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLURA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEOL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABKNSN" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABKNSN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEOT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXJT" varchar(3) NOT NULL default ' ',
  "AAABMCZV" varchar(4) NOT NULL default ' ',
  "AAABLPWW" varchar(30) NOT NULL default ' ',
  "AAAAWEOY" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXJT","AAABMCZV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEPC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKMHD" varchar(3) NOT NULL default ' ',
  "AAAAWEPF" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKMHD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEPI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLURO" varchar(3) NOT NULL default ' ',
  "AAABGCTK" varchar(18) NOT NULL default ' ',
  "AAAAWEPN" varchar(12) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAAAWEPP" varchar(12) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAAAWEPS" varchar(2) NOT NULL default ' ',
  "AAAAWEPT" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABLURO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEPX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAAAWEQA" varchar(20) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABARWB" varchar(70) NOT NULL default ' ',
  "AAAAWEQD" varchar(70) NOT NULL default ' ',
  "AAAAWEQE" varchar(70) NOT NULL default ' ',
  "AAAAWEQF" varchar(70) NOT NULL default ' ',
  "AAABGAIJ" varchar(4) NOT NULL default ' ',
  "AAAAWEQH" varchar(6) NOT NULL default ' ',
  "AAAAWEQI" varchar(1) NOT NULL default ' ',
  "AAAAWEQJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLURS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEQM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABKNSN" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  "AAABKNVV" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABKNSN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEQV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLKLM" varchar(3) NOT NULL default ' ',
  "AAAAWEQZ" varchar(18) NOT NULL default ' ',
  "AAAAWERA" varchar(12) NOT NULL default ' ',
  "AAAAWERB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLKLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWERF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWUMF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWUMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWERK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXJT" varchar(3) NOT NULL default ' ',
  "AAABMCZV" varchar(4) NOT NULL default ' ',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAABLUPZ" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABLUPQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXJT","AAABMCZV","AAABMALG","AAABLUPZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWERX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABLURS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWESE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLUFN" varchar(4) NOT NULL default ' ',
  "AAABMLCT" varchar(4) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAAAWESL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABLURS","AAABLUFN","AAABMLCT","AAABLXOB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWESS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWUMF" varchar(3) NOT NULL default ' ',
  "AAABLHIK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWUMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWETA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAAAWETL" varchar(4) NOT NULL default ' ',
  "AAAAWETE" varchar(1) NOT NULL default ' ',
  "AAABLGPK" varchar(1) NOT NULL default ' ',
  "AAAAWETG" varchar(1) NOT NULL default ' ',
  "AAAAWETH" varchar(1) NOT NULL default ' ',
  "AAAAWETI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLURS","AAAAWETL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWETM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHRD" varchar(4) NOT NULL default ' ',
  "AAAAWSEN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWETS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWSEN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWSEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWETX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWSEN" varchar(4) NOT NULL default ' ',
  "AAAAWEUB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWSEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEUF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLHRD" varchar(4) NOT NULL default ' ',
  "AAAAWEUJ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLHRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEUN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWEVE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWEVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEUS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWEVE" varchar(4) NOT NULL default ' ',
  "AAAAWEUW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWEVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEVA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAAAWEVE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLURS","AAABLURA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEVI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXMWI" varchar(2) NOT NULL default ' ',
  "AAAAWEVL" varchar(1) NOT NULL default ' ',
  "AAAAXMXX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXMWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEVP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAXMWI" varchar(2) NOT NULL default ' ',
  "AAABBENX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAXMWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEVX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTW" varchar(8) NOT NULL default ' ',
  "AAAAWFCG" varchar(27) NOT NULL default ' ',
  "AAAAWHKE" varchar(1) NOT NULL default ' ',
  "AAABJRHX" varchar(4) NOT NULL default ' ',
  "AAAAWEWS" varchar(4) NOT NULL default ' ',
  "AAAAWEWE" varchar(2) NOT NULL default '00',
  "AAAAWHJP" varchar(3) NOT NULL default '000',
  "AAAAWEXT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTW","AAAAWFCG","AAAAWHKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEWL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWEXI" varchar(1) NOT NULL default ' ',
  "AAAAWEXJ" varchar(4) NOT NULL default ' ',
  "AAAAWHKE" varchar(1) NOT NULL default ' ',
  "AAABJRHX" varchar(4) NOT NULL default ' ',
  "AAAAWHJO" varchar(10) NOT NULL default ' ',
  "AAAAWEWS" varchar(4) NOT NULL default ' ',
  "AAABLOLZ" varchar(8) NOT NULL default ' ',
  "AAAAWHJP" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAAAWEXI","AAAAWEXJ","AAAAWHKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEWZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWEXI" varchar(1) NOT NULL default ' ',
  "AAAAWEXJ" varchar(4) NOT NULL default ' ',
  "AAAAWHKE" varchar(1) NOT NULL default ' ',
  "AAABGRZV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWEXI","AAAAWEXJ","AAAAWHKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEXL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMGVL" varchar(15) NOT NULL default ' ',
  "AAABJRHR" varchar(18) NOT NULL default ' ',
  "AAAAWEXP" varchar(2) NOT NULL default ' ',
  "AAAAWEXQ" varchar(2) NOT NULL default ' ',
  "AAAAWEXR" varchar(2) NOT NULL default ' ',
  "AAAAWEXS" varchar(2) NOT NULL default ' ',
  "AAAAWEXT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMGVL","AAABJRHR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEXX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJXED" varchar(8) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABDUGN" varchar(3) NOT NULL default ' ',
  "AAABDUGO" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJXED","AAABMPWL"),
  UNIQUE KEY "AAAAWEYG" ("AAABMRIL","AAABJXED","AAABDUGO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJXED" varchar(8) NOT NULL default ' ',
  "AAAAWEYO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJXED")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEYS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJXED" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJXED")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEYX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  "AAAAWHJP" varchar(3) NOT NULL default '000',
  "AAABGHHV" varchar(20) NOT NULL default ' ',
  "AAAAWEZP" varchar(10) NOT NULL default ' ',
  "AAABGFKN" varchar(1) NOT NULL default ' ',
  "AAABJRKN" varchar(55) NOT NULL default ' ',
  "AAABLRQQ" varchar(32) NOT NULL default ' ',
  "AAAAWEZI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCTJ","AAABKWCV","AAAAWHJP","AAABGHHV","AAAAWEZP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEZQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWFBF" varchar(8) NOT NULL default ' ',
  "AAAAWFAD" varchar(40) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWFBF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWEZZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWFBF" varchar(8) NOT NULL default ' ',
  "AAAAWFAD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWFBF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFAH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWFBF" varchar(8) NOT NULL default ' ',
  "AAAAWFBG" varchar(20) NOT NULL default ' ',
  "AAAAWFAL" varchar(4) NOT NULL default ' ',
  "AAAAWFAM" varchar(5) NOT NULL default ' ',
  "AAAAWFAN" varchar(5) NOT NULL default ' ',
  "AAAAWFAO" varchar(10) NOT NULL default ' ',
  "AAAAWFAP" varchar(4) NOT NULL default ' ',
  "AAAAWFAQ" varchar(10) NOT NULL default ' ',
  "AAAAWFAR" varchar(4) NOT NULL default ' ',
  "AAAAWFAS" varchar(10) NOT NULL default ' ',
  "AAABJRIC" varchar(1) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABIEXL" varchar(2) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABKPOO" varchar(1) NOT NULL default ' ',
  "AAAAWFAY" varchar(4) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABLDBD" varchar(10) NOT NULL default ' ',
  "AAABJWRR" varchar(3) NOT NULL default ' ',
  "AAABMCTK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWFBF","AAAAWFBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFBH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTW" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFBM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXTW" varchar(8) NOT NULL default ' ',
  "AAAAWFBQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFBU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTW" varchar(8) NOT NULL default ' ',
  "AAABMGVL" varchar(15) NOT NULL default ' ',
  "AAABJRHR" varchar(18) NOT NULL default ' ',
  "AAAAWFCG" varchar(27) NOT NULL default ' ',
  "AAAAWFCH" varchar(27) NOT NULL default ' ',
  "AAAAWFCZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTW","AAABMGVL","AAABJRHR","AAAAWFCG","AAAAWFCH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFCI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWFCZ" varchar(3) NOT NULL default ' ',
  "AAAAWFCL" varchar(4) NOT NULL default ' ',
  "AAAAWFCM" varchar(4) NOT NULL default ' ',
  "AAAAWFCN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFCQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWFCZ" varchar(3) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCUU" varchar(1) NOT NULL default ' ',
  "AAABLCXR" varchar(1) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAAAWFCX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWFCZ","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFDB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABCBUI" varchar(2) NOT NULL default ' ',
  "AAABMCQQ" varchar(3) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAAAWFDH" varchar(10) NOT NULL default ' ',
  "AAAAWFDI" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZBM","AAABCBUI","AAABMCQQ","AAABMOAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFDO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCBUI" varchar(2) NOT NULL default ' ',
  "AAABGAJY" varchar(2) NOT NULL default ' ',
  "AAAAWFDS" varchar(2) NOT NULL default ' ',
  "AAAAWFDT" varchar(2) NOT NULL default ' ',
  "AAAAWFDU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCBUI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFDX" (
  "AAABFFDQ" varchar(4) NOT NULL default ' ',
  "AAAAWFDZ" varchar(1) NOT NULL default ' ',
  "AAAAWFEA" varchar(1) NOT NULL default ' ',
  "AAAAWFEB" varchar(1) NOT NULL default ' ',
  "AAAAWFEC" varchar(1) NOT NULL default ' ',
  "AAAAWFED" varchar(1) NOT NULL default ' ',
  "AAAAWFEE" varchar(1) NOT NULL default ' ',
  "AAAAWFEF" varchar(1) NOT NULL default ' ',
  "AAAAWFEG" varchar(1) NOT NULL default ' ',
  "AAAAWFEH" varchar(2) NOT NULL default '00',
  "AAAAWFEI" varchar(2) NOT NULL default '00',
  "AAAAWFEJ" varchar(2) NOT NULL default '00',
  "AAAAWFEK" varchar(2) NOT NULL default '00',
  "AAAAWFEL" varchar(30) NOT NULL default ' ',
  "AAAAWFEM" varchar(30) NOT NULL default ' ',
  "AAAAWFEN" varchar(30) NOT NULL default ' ',
  "AAAAWFEO" varchar(30) NOT NULL default ' ',
  "AAABFRUM" varchar(30) NOT NULL default ' ',
  "AAABFRUN" varchar(30) NOT NULL default ' ',
  "AAABFRUO" varchar(30) NOT NULL default ' ',
  "AAABFRUP" varchar(30) NOT NULL default ' ',
  "AAAAWFET" varchar(1) NOT NULL default ' ',
  "AAAAWFEU" varchar(1) NOT NULL default ' ',
  "AAAAWFEV" varchar(30) NOT NULL default ' ',
  "AAAAWFEW" varchar(30) NOT NULL default ' ',
  "AAABFRUQ" varchar(30) NOT NULL default ' ',
  "AAAAWFEY" varchar(30) NOT NULL default ' ',
  "AAAAWFEZ" varchar(30) NOT NULL default ' ',
  "AAAAWFFA" varchar(30) NOT NULL default ' ',
  "AAAAWFFB" varchar(2) NOT NULL default '00',
  "AAAAWFFC" varchar(30) NOT NULL default ' ',
  "AAAAWFFD" varchar(30) NOT NULL default ' ',
  "AAAAWFFE" varchar(2) NOT NULL default '00',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFFDQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFFH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCBUI" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCBUI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFFP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXJS" varchar(10) NOT NULL default ' ',
  "AAABJRCJ" varchar(2) NOT NULL default ' ',
  "AAABGYAB" varchar(2) NOT NULL default ' ',
  "AAABEMIV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFFX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAWFGY" varchar(10) NOT NULL default ' ',
  "AAAAWFGB" varchar(10) NOT NULL default ' ',
  "AAAAWFGC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAAWFGY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFGG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXJS" varchar(10) NOT NULL default ' ',
  "AAABGCUS" varchar(30) NOT NULL default ' ',
  "AAABMBJJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFGP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAWFGY" varchar(10) NOT NULL default ' ',
  "AAABGCUS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAAAWFGY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFGZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXJS" varchar(10) NOT NULL default ' ',
  "AAABGYAB" varchar(2) NOT NULL default ' ',
  "AAABHIJR" varchar(1) NOT NULL default ' ',
  "AAAAWFHE" varchar(3) NOT NULL default '000',
  "AAAAWFHF" varchar(2) NOT NULL default '00',
  "AAABBHGJ" varchar(2) NOT NULL default '00',
  "AAABLXOT" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLXJS","AAABGYAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFHL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGYAB" varchar(2) NOT NULL default ' ',
  "AAAAZZFR" varchar(1) NOT NULL default ' ',
  "AAAAWFHP" varchar(1) NOT NULL default ' ',
  "AAAAWFHQ" varchar(1) NOT NULL default ' ',
  "AAAAWFHR" varchar(1) NOT NULL default ' ',
  "AAAAWGJM" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGYAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFHV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLODQ" varchar(3) NOT NULL default '000',
  "AAABGYAB" varchar(2) NOT NULL default ' ',
  "AAAAWFHZ" varchar(10) NOT NULL default ' ',
  "AAAAWFIA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLODQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFID" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGYAB" varchar(2) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAAAWFII" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGYAB","AAABMAMP","AAABMCTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFIN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGYAB" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  "AAABLXIS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGYAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABLCVA" varchar(2) NOT NULL default '00',
  "AAABKXKU" varchar(2) NOT NULL default '00',
  "AAAAWFJC" varchar(2) NOT NULL default ' ',
  "AAAAWFJD" varchar(2) NOT NULL default ' ',
  "AAAAWFJE" varchar(2) NOT NULL default ' ',
  "AAAAWFJF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLCAK","AAABLCVA","AAABKXKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFJL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXT" varchar(10) NOT NULL default ' ',
  "AAAAWFJY" varchar(1) NOT NULL default ' ',
  "AAAAWUYM" varchar(10) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABHIJR" varchar(1) NOT NULL default ' ',
  "AAABDIBD" varchar(10) NOT NULL default ' ',
  "AAABJFDR" varchar(10) NOT NULL default ' ',
  "AAAAWFJV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYXT","AAAAWFJY","AAAAWUYM","AAABMREX","AAABLZBM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFKC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFKH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKYXT" varchar(10) NOT NULL default ' ',
  "AAAAWFKL" varchar(50) NOT NULL default ' ',
  "AAAAWFKM" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKYXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFKQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWGJM" varchar(3) NOT NULL default ' ',
  "AAAAWFKT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWGJM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWGJM" varchar(3) NOT NULL default ' ',
  "AAABGCUS" varchar(30) NOT NULL default ' ',
  "AAABMBJJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWGJM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFLF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAWFLI" varchar(1) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFLM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBVLM" varchar(8) NOT NULL default ' ',
  "AAABKUHR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBVLM","AAABKUHR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFLV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCOT" varchar(2) NOT NULL default ' ',
  "AAABMCZS" varchar(1) NOT NULL default ' ',
  "AAAAWFLZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCOT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFMC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCOT" varchar(2) NOT NULL default ' ',
  "AAABHIHE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCOT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFMK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABENPM" varchar(4) NOT NULL default ' ',
  "AAAAWFMN" decimal(7,2) NOT NULL default '0.00',
  "AAAAWFMO" decimal(7,2) NOT NULL default '0.00',
  "AAAAWFMP" varchar(16) NOT NULL default ' ',
  "AAABENSY" varchar(1) NOT NULL default ' ',
  "AAAAWFMR" varchar(16) NOT NULL default ' ',
  "AAAAWFMS" varchar(1) NOT NULL default ' ',
  "AAAAWFMT" varchar(1) NOT NULL default ' ',
  "AAAAWFMU" varchar(1) NOT NULL default ' ',
  "AAAAWFMV" varchar(3) NOT NULL default '000',
  "AAAAWFMW" varchar(3) NOT NULL default '000',
  "AAAAWFMX" varchar(3) NOT NULL default '000',
  "AAAAWFMY" varchar(2) NOT NULL default '00',
  "AAAAWFMZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABENPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFNC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWFNE" varchar(1) NOT NULL default ' ',
  "AAAAWFNF" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFNH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABENPM" varchar(4) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAAAWFNL" varchar(1) NOT NULL default ' ',
  "AAABHSMG" varchar(2) NOT NULL default '00',
  "AAAAWFNN" decimal(13,2) NOT NULL default '0.00',
  "AAAAWFNO" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAWFNQ" varchar(1) NOT NULL default ' ',
  "AAAAWFNR" varchar(1) NOT NULL default ' ',
  "AAAAWFNS" varchar(1) NOT NULL default ' ',
  "AAAAWGOA" varchar(1) NOT NULL default ' ',
  "AAAAWFNU" varchar(1) NOT NULL default ' ',
  "AAAAWFNV" varchar(1) NOT NULL default ' ',
  "AAABKPWT" varchar(35) NOT NULL default ' ',
  "AAABKPWU" varchar(35) NOT NULL default ' ',
  "AAAAWFNY" varchar(35) NOT NULL default ' ',
  "AAAAWFNZ" varchar(16) NOT NULL default ' ',
  "AAAAWFOA" varchar(16) NOT NULL default ' ',
  "AAAAWFOB" varchar(1) NOT NULL default ' ',
  "AAABKPWV" varchar(35) NOT NULL default ' ',
  "AAAAWFOD" varchar(1) NOT NULL default ' ',
  "AAAAWFOE" varchar(1) NOT NULL default ' ',
  "AAAAWFOF" varchar(1) NOT NULL default ' ',
  "AAAAWFOG" varchar(2) NOT NULL default '00',
  "AAAAWFOH" varchar(1) NOT NULL default ' ',
  "AAAAWFOI" varchar(3) NOT NULL default ' ',
  "AAAAWFOJ" varchar(3) NOT NULL default ' ',
  "AAAAWFOK" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABENPM","AAABMCTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFOO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABENPM" varchar(4) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABENPM","AAABMCTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFOV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGRZB" varchar(61) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABGRZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFPB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCZD" varchar(2) NOT NULL default ' ',
  "AAAAWFPK" varchar(8) NOT NULL default ' ',
  "AAAAWFPL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCZD"),
  UNIQUE KEY "AAAAWFPI" ("AAABMRIL","AAAAWFPK","AAAAWFPL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFPM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABENPM" varchar(4) NOT NULL default ' ',
  "AAABLQPP" varchar(4) NOT NULL default ' ',
  "AAABLKKE" varchar(2) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAWFTB" varchar(10) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABENPM","AAABLQPP","AAABLKKE","AAABMCTG","AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFQB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABENPM" varchar(4) NOT NULL default ' ',
  "AAABJRGH" varchar(4) NOT NULL default ' ',
  "AAAAWFQF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABENPM","AAABJRGH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFQJ" (
  "AAABAGBI" varchar(30) NOT NULL default ' ',
  "AAABLYTE" varchar(3) NOT NULL default ' ',
  "AAABLLNS" varchar(6) NOT NULL default ' ',
  "AAAAWFQN" varchar(1) NOT NULL default ' ',
  "AAAAWFQO" varchar(1) NOT NULL default ' ',
  "AAAAWFQP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABAGBI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFQR" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAGBI" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABAGBI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFQX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJWRR" varchar(3) NOT NULL default ' ',
  "AAABMGVL" varchar(15) NOT NULL default ' ',
  "AAABMGVM" varchar(18) NOT NULL default ' ',
  "AAAAWFRC" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJWRR","AAABMGVL","AAABMGVM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFRH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTHM" varchar(8) NOT NULL default '00000000',
  "AAABLTHN" varchar(6) NOT NULL default ' ',
  "AAABENPM" varchar(4) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABJRHR" varchar(10) NOT NULL default ' ',
  "AAABMCTH" varchar(1) NOT NULL default ' ',
  "AAABIRRQ" varchar(12) NOT NULL default ' ',
  "AAABIRRR" varchar(8) NOT NULL default '00000000',
  "AAAAWFRR" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLTHM","AAABLTHN","AAABENPM","AAABMPWP","AAABJRHR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFRY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAABMCTG","AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFSH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABLTHM" varchar(8) NOT NULL default '00000000',
  "AAABLTHN" varchar(6) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMPWP","AAABLTHM","AAABLTHN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFSU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABENPM" varchar(4) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  "AAAAWFTB" varchar(10) NOT NULL default ' ',
  "AAABKSUB" varchar(10) NOT NULL default ' ',
  "AAAAWFTD" varchar(2) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABENPM","AAABMCTJ","AAABMCTG","AAABMPWL","AAABKWCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFTL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAABMJVD" varchar(30) NOT NULL default ' ',
  "AAAAWFTQ" varchar(1) NOT NULL default ' ',
  "AAAAWFTR" varchar(1) NOT NULL default ' ',
  "AAABEQMD" varchar(1) NOT NULL default ' ',
  "AAAAWFTT" varchar(1) NOT NULL default ' ',
  "AAAAWFTU" varchar(1) NOT NULL default ' ',
  "AAAAWFTV" varchar(1) NOT NULL default ' ',
  "AAAAWFTW" varchar(1) NOT NULL default ' ',
  "AAABEQVP" varchar(40) NOT NULL default ' ',
  "AAAAWFTY" varchar(1) NOT NULL default ' ',
  "AAABDULE" varchar(1) NOT NULL default ' ',
  "AAABEQOP" varchar(30) NOT NULL default ' ',
  "AAABEQOQ" varchar(6) NOT NULL default ' ',
  "AAAAWFUC" varchar(1) NOT NULL default ' ',
  "AAAAWFUD" varchar(1) NOT NULL default ' ',
  "AAAAWFUE" varchar(1) NOT NULL default ' ',
  "AAAAWFUF" varchar(1) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAAAWFUH" varchar(2) NOT NULL default ' ',
  "AAABMCPR" varchar(1) NOT NULL default ' ',
  "AAAAWFUJ" varchar(1) NOT NULL default ' ',
  "AAAAWFUK" varchar(2) NOT NULL default ' ',
  "AAAAWFUL" varchar(6) NOT NULL default ' ',
  "AAAAWFUM" varchar(6) NOT NULL default ' ',
  "AAABJKRF" varchar(1) NOT NULL default ' ',
  "AAAAWFUO" varchar(1) NOT NULL default ' ',
  "AAAAWFUP" varchar(2) NOT NULL default ' ',
  "AAAAWFUQ" varchar(1) NOT NULL default ' ',
  "AAAAWFUR" varchar(1) NOT NULL default ' ',
  "AAAAWFUS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAABMCTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFUW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAABLSDL" varchar(8) NOT NULL default ' ',
  "AAABEQQC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAABMCTG","AAABLSDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFVG" (
  "AAAAWFWN" varchar(1) NOT NULL default ' ',
  "AAABLCFW" varchar(1) NOT NULL default ' ',
  "AAABEQMD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAWFWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFVL" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWFWN" varchar(1) NOT NULL default ' ',
  "AAAAWFVO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAAWFWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFVR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABENPM" varchar(4) NOT NULL default ' ',
  "AAAAWFVY" varchar(1) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABENPM","AAAAWFVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFVZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWFWF" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCOV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWFWF","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFWH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWFWN" varchar(1) NOT NULL default ' ',
  "AAAAWFWK" varchar(3) NOT NULL default '000',
  "AAAAWFWL" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAAAWFWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFWO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLCGB" varchar(4) NOT NULL default ' ',
  "AAABKHGP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABLCGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFWY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWFXF" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAWFXC" decimal(15,2) NOT NULL default '0.00',
  "AAAAWFXD" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAWFXF","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFXH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLCGB" varchar(4) NOT NULL default ' ',
  "AAABKHGP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABLCGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFXR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABENPM" varchar(4) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAAAXKIU" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABENPM","AAABMCTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFXZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAWFYI" varchar(10) NOT NULL default ' ',
  "AAAAWFYE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPWP","AAABMREX","AAAAWFYI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFYJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWFYU" varchar(2) NOT NULL default ' ',
  "AAABCBUI" varchar(2) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAWFYX" varchar(3) NOT NULL default '000',
  "AAABDCEO" varchar(1) NOT NULL default ' ',
  "AAAAWGQJ" decimal(7,3) NOT NULL default '0.000',
  "AAAAZYEB" varchar(1) NOT NULL default ' ',
  "AAAAWFYS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWFYU","AAABCBUI","AAABMAMP","AAAAWFYX","AAABDCEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFYZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABGAJY" varchar(2) NOT NULL default ' ',
  "AAABCBUI" varchar(2) NOT NULL default ' ',
  "AAABBACP" varchar(3) NOT NULL default ' ',
  "AAABMOQQ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABMDBA","AAABMDBB","AAABMPWO","AAABGAJY","AAABCBUI","AAABBACP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFZT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECQ" varchar(3) NOT NULL default ' ',
  "AAABKIUC" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWFZZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABGAJY" varchar(2) NOT NULL default ' ',
  "AAABCBUI" varchar(2) NOT NULL default ' ',
  "AAAAWGAF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUAV","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGAJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABBACP" varchar(4) NOT NULL default ' ',
  "AAAAWGBK" varchar(1) NOT NULL default ' ',
  "AAAAWGBL" varchar(3) NOT NULL default '000',
  "AAABBGOP" varchar(1) NOT NULL default ' ',
  "AAABKHGP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABBACP","AAAAWGBK","AAAAWGBL","AAABBGOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGAX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBACP" varchar(4) NOT NULL default ' ',
  "AAAAWGBK" varchar(1) NOT NULL default ' ',
  "AAAAWGBL" varchar(3) NOT NULL default '000',
  "AAABBGOP" varchar(1) NOT NULL default ' ',
  "AAAAWGBD" varchar(1) NOT NULL default ' ',
  "AAABMCQQ" varchar(3) NOT NULL default ' ',
  "AAAAWGBF" varchar(1) NOT NULL default ' ',
  "AAAAWGBG" varchar(1) NOT NULL default ' ',
  "AAAAWGBH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBACP","AAAAWGBK","AAAAWGBL","AAABBGOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGBN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBACP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBACP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGBS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABBACP" varchar(4) NOT NULL default ' ',
  "AAABKHGP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABBACP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGCA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMCQA" decimal(13,2) NOT NULL default '0.00',
  "AAABMCSW" varchar(8) NOT NULL default '00000000',
  "AAABLUGB" varchar(8) NOT NULL default '00000000',
  "AAAAWGCJ" varchar(13) NOT NULL default ' ',
  "AAABKUHB" varchar(16) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBB","AAABMDBA","AAABMPWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGCR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAWGDC" varchar(2) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAWGDE" varchar(3) NOT NULL default '000',
  "AAABLRDW" varchar(2) NOT NULL default '00',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAAWGDC","AAABMPWL","AAAAWGDE","AAABLRDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGDG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAWGEA" varchar(10) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMREX","AAAAWGEA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGDQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAWGEA" varchar(10) NOT NULL default ' ',
  "AAABEMHV" varchar(10) NOT NULL default ' ',
  "AAAAWGDV" varchar(1) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABKSUB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAAWGEA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGEB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAABLOLZ" varchar(2) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  "AAAAXNBB" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCTG","AAABLOLZ","AAABMCTJ","AAABKWCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGEP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABJWRQ" varchar(10) NOT NULL default ' ',
  "AAABMCUL" varchar(1) NOT NULL default ' ',
  "AAABMCPR" varchar(1) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABKSUB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZBM","AAABJWRQ","AAABMCUL","AAABMCPR","AAABMOAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGFD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAAAWGFJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCTJ","AAABKWCV","AAABMCTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGFP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABEUWE" varchar(2) NOT NULL default ' ',
  "AAAAZWTG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABMCTJ","AAABEUWE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGGB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABEUWE" varchar(2) NOT NULL default ' ',
  "AAABLMLH" varchar(2) NOT NULL default ' ',
  "AAABLPGG" varchar(1) NOT NULL default ' ',
  "AAAAWGGI" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCTJ","AAABEUWE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGGN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGRZB" varchar(61) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABGRZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGGT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAWGGW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGGZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWGHM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWGHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWGHM" varchar(4) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWGHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGHN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAAAWGHR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAABMCTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGHV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAABDXWF" varchar(16) NOT NULL default ' ',
  "AAAAWGIA" varchar(1) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  "AAAAWGID" varchar(30) NOT NULL default ' ',
  "AAAAWGIE" varchar(30) NOT NULL default ' ',
  "AAAAWGIF" varchar(30) NOT NULL default ' ',
  "AAAAWGIG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGIK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAABMJVD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMP","AAABMCTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGIU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFWHB" varchar(10) NOT NULL default ' ',
  "AAAAWGJM" varchar(10) NOT NULL default ' ',
  "AAAAWGIY" varchar(2) NOT NULL default ' ',
  "AAAAWGIZ" varchar(2) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  "AAAAXBXD" varchar(16) NOT NULL default ' ',
  "AAAAWGJE" varchar(16) NOT NULL default ' ',
  "AAAAWGJF" varchar(2) NOT NULL default ' ',
  "AAAAWGJG" varchar(2) NOT NULL default ' ',
  "AAAAWGJH" varchar(2) NOT NULL default ' ',
  "AAAAWGJI" varchar(1) NOT NULL default ' ',
  "AAAAWGJJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFWHB","AAAAWGJM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHYAO" varchar(10) NOT NULL default ' ',
  "AAABAGBI" varchar(8) NOT NULL default ' ',
  "AAAAWGJR" varchar(2) NOT NULL default '00',
  "AAAAWGJS" varchar(2) NOT NULL default '00',
  "AAAAWGJT" varchar(2) NOT NULL default '00',
  "AAAAWGJU" varchar(1) NOT NULL default ' ',
  "AAAAWGJV" varchar(12) NOT NULL default ' ',
  "AAAAWGJW" varchar(1) NOT NULL default ' ',
  "AAAAWGJX" varchar(1) NOT NULL default ' ',
  "AAAAWGJY" varchar(12) NOT NULL default ' ',
  "AAAAWGJZ" varchar(1) NOT NULL default ' ',
  "AAAAWGKA" varchar(1) NOT NULL default ' ',
  "AAAAWGKB" varchar(12) NOT NULL default ' ',
  "AAABEIKL" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHYAO","AAABAGBI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAAAWGKS" varchar(4) NOT NULL default '0000',
  "AAAAWGKT" varchar(3) NOT NULL default '000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAWGKN" decimal(15,2) NOT NULL default '0.00',
  "AAAAWGKO" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMOAH","AAAAWGKS","AAAAWGKT","AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGKV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMRQ" varchar(4) NOT NULL default ' ',
  "AAAAWGMI" varchar(2) NOT NULL default '00',
  "AAABBSQX" varchar(1) NOT NULL default ' ',
  "AAAAWGLA" varchar(2) NOT NULL default '00',
  "AAAAWGLB" varchar(2) NOT NULL default '00',
  "AAAAWGLC" varchar(3) NOT NULL default '000',
  "AAAAWGLD" decimal(5,3) NOT NULL default '0.000',
  "AAAAWGLE" varchar(3) NOT NULL default '000',
  "AAAAWGLF" decimal(5,3) NOT NULL default '0.000',
  "AAAAWGLG" varchar(3) NOT NULL default '000',
  "AAAAWGLH" varchar(2) NOT NULL default '00',
  "AAAAWGLI" varchar(2) NOT NULL default '00',
  "AAAAWGLJ" varchar(2) NOT NULL default '00',
  "AAAAWGLK" varchar(2) NOT NULL default '00',
  "AAAAWGLL" varchar(2) NOT NULL default '00',
  "AAAAWGLM" varchar(2) NOT NULL default '00',
  "AAAAWGLN" varchar(1) NOT NULL default ' ',
  "AAAAWGLO" varchar(1) NOT NULL default ' ',
  "AAAAWGLP" varchar(1) NOT NULL default ' ',
  "AAABHOSP" varchar(8) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAAAWGLS" varchar(1) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAAAWGLU" varchar(1) NOT NULL default ' ',
  "AAAAWGLV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMRQ","AAAAWGMI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGLZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMRQ" varchar(4) NOT NULL default ' ',
  "AAAAWGMI" varchar(2) NOT NULL default '00',
  "AAABMJVD" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMMRQ","AAAAWGMI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGMJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWGNF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWGNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGMO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWGNF" varchar(3) NOT NULL default ' ',
  "AAABGRZV" varchar(25) NOT NULL default ' ',
  "AAABHIHE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWGNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGMX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWGNF" varchar(3) NOT NULL default ' ',
  "AAAAWGNG" varchar(2) NOT NULL default '00',
  "AAAAWGNB" varchar(30) NOT NULL default ' ',
  "AAABCVPG" varchar(30) NOT NULL default ' ',
  "AAABFLWU" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAWGNF","AAAAWGNG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGNH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAWGNO" varchar(1) NOT NULL default ' ',
  "AAABMCWO" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAAWGNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGNP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWHKA" varchar(2) NOT NULL default ' ',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAAAWUXR" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAWHKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGNW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCWO" varchar(3) NOT NULL default ' ',
  "AAAAWGOA" varchar(1) NOT NULL default ' ',
  "AAAAWGOB" varchar(5) NOT NULL default ' ',
  "AAABCSRC" varchar(1) NOT NULL default ' ',
  "AAAAWGOD" varchar(1) NOT NULL default ' ',
  "AAAAWGOE" varchar(1) NOT NULL default ' ',
  "AAAAWGOF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGOJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCWO" varchar(3) NOT NULL default ' ',
  "AAABGRZV" varchar(20) NOT NULL default ' ',
  "AAABHIHE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABMCWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABLXJS" varchar(2) NOT NULL default '00',
  "AAABGRZB" varchar(61) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPWP","AAABLXJS","AAABGRZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABGRZB" varchar(61) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPWP","AAABGRZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGPK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKVPA" varchar(10) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default ' ',
  "AAABKVPB" decimal(10,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAABKVPA","AAABMNZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGPS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKVPA" varchar(10) NOT NULL default ' ',
  "AAAAWGPV" varchar(60) NOT NULL default ' ',
  "AAAAWGPW" varchar(15) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAWGPZ" varchar(20) NOT NULL default ' ',
  "AAABLTPB" decimal(3,0) NOT NULL default '0',
  "AAAAWGQB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKVPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGQE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCFN" varchar(2) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default ' ',
  "AAAAWGQJ" decimal(7,3) NOT NULL default '0.000',
  "AAAAWGQK" decimal(7,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABLCFN","AAABMPWL","AAABMNZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGQP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWGQY" varchar(3) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAWGRB" varchar(2) NOT NULL default '00',
  "AAAAWGQV" decimal(13,2) NOT NULL default '0.00',
  "AAAAWGQW" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAWGQY","AAABMAMP","AAABMPWL","AAAAWGRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGRC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAWGRU" varchar(4) NOT NULL default ' ',
  "AAAAWGRG" varchar(2) NOT NULL default ' ',
  "AAAAWGRH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMREX","AAAAWGRU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGRL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAWGRU" varchar(4) NOT NULL default ' ',
  "AAABHACY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABMREX","AAAAWGRU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGRV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLTNT" varchar(2) NOT NULL default ' ',
  "AAABLOHP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAABLTNT","AAABLOHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGSE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLTNT" varchar(2) NOT NULL default ' ',
  "AAABLOHP" varchar(2) NOT NULL default ' ',
  "AAAAZJII" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMP","AAABLTNT","AAABLOHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAWHDA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAAAWHDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGSX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLTNT" varchar(2) NOT NULL default ' ',
  "AAABLTNU" varchar(2) NOT NULL default ' ',
  "AAABFDMZ" varchar(3) NOT NULL default ' ',
  "AAAAWGUG" varchar(8) NOT NULL default '00000000',
  "AAAAWGTR" decimal(15,2) NOT NULL default '0.00',
  "AAABLOIO" decimal(4,2) NOT NULL default '0.00',
  "AAAAWHCW" decimal(4,2) NOT NULL default '0.00',
  "AAAAWGTI" decimal(13,2) NOT NULL default '0.00',
  "AAAAWGTJ" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAABMPWL","AAABLTNT","AAABLTNU","AAABFDMZ","AAAAWGUG","AAAAWGTR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGTS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLTNT" varchar(2) NOT NULL default ' ',
  "AAABLTNU" varchar(2) NOT NULL default ' ',
  "AAABFDMZ" varchar(3) NOT NULL default ' ',
  "AAAAWGUG" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAABMPWL","AAABLTNT","AAABLTNU","AAABFDMZ","AAAAWGUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGUH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAWHDA" varchar(4) NOT NULL default ' ',
  "AAAAZCJK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMP","AAAAWHDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGUR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWGWC" varchar(10) NOT NULL default ' ',
  "AAAAWGWD" varchar(3) NOT NULL default ' ',
  "AAAAWGUV" varchar(1) NOT NULL default ' ',
  "AAAAWGUW" varchar(1) NOT NULL default ' ',
  "AAAAWGUX" varchar(30) NOT NULL default ' ',
  "AAAAWGUY" varchar(30) NOT NULL default ' ',
  "AAAAWGUZ" varchar(1) NOT NULL default ' ',
  "AAAAWGVA" varchar(30) NOT NULL default ' ',
  "AAAAWGVB" varchar(40) NOT NULL default ' ',
  "AAAAWGVC" varchar(20) NOT NULL default ' ',
  "AAAAWGVD" varchar(1) NOT NULL default ' ',
  "AAAAWGVE" varchar(1) NOT NULL default ' ',
  "AAAAWGVF" varchar(6) NOT NULL default ' ',
  "AAAAWGVG" varchar(6) NOT NULL default ' ',
  "AAAAWGVH" varchar(6) NOT NULL default ' ',
  "AAAAWGVI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWGWC","AAAAWGWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGVM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWGWC" varchar(10) NOT NULL default ' ',
  "AAAAWGWD" varchar(3) NOT NULL default ' ',
  "AAAAWGVR" varchar(30) NOT NULL default ' ',
  "AAAAWGVS" varchar(70) NOT NULL default ' ',
  "AAAAWGVT" varchar(70) NOT NULL default ' ',
  "AAAAWGVU" varchar(70) NOT NULL default ' ',
  "AAAAWGVV" varchar(70) NOT NULL default ' ',
  "AAAAWGVW" varchar(70) NOT NULL default ' ',
  "AAAAWGVX" varchar(70) NOT NULL default ' ',
  "AAAAWGVY" varchar(70) NOT NULL default ' ',
  "AAAAWGVZ" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWGWC","AAAAWGWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGWE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLTNT" varchar(2) NOT NULL default ' ',
  "AAABMCQQ" varchar(3) NOT NULL default ' ',
  "AAAAWGWO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAABLTNT","AAABMCQQ","AAAAWGWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGWP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLTNT" varchar(2) NOT NULL default ' ',
  "AAABLTNU" varchar(2) NOT NULL default ' ',
  "AAAAWHEG" varchar(8) NOT NULL default '00000000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAWGWW" decimal(15,2) NOT NULL default '0.00',
  "AAAAWGWX" decimal(15,2) NOT NULL default '0.00',
  "AAAAWGWY" decimal(15,2) NOT NULL default '0.00',
  "AAAAWGWZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAWGXA" decimal(15,2) NOT NULL default '0.00',
  "AAAAWGXB" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAABLTNT","AAABLTNU","AAAAWHEG","AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGXI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAWGYA" varchar(4) NOT NULL default ' ',
  "AAABLOFJ" varchar(40) NOT NULL default ' ',
  "AAAAWGXN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAAAWGYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGXR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAWGYA" varchar(4) NOT NULL default ' ',
  "AAABLOFJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMP","AAAAWGYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGYB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLTNT" varchar(2) NOT NULL default ' ',
  "AAAAWGYF" varchar(2) NOT NULL default '00',
  "AAAAWGYG" varchar(1) NOT NULL default ' ',
  "AAAAWGYH" varchar(1) NOT NULL default ' ',
  "AAAAWGYI" varchar(1) NOT NULL default ' ',
  "AAAAWGYJ" varchar(2) NOT NULL default ' ',
  "AAAAWGYK" varchar(1) NOT NULL default ' ',
  "AAAAWGYL" varchar(1) NOT NULL default ' ',
  "AAAAWGYM" varchar(1) NOT NULL default ' ',
  "AAAAWGYN" varchar(1) NOT NULL default ' ',
  "AAAAWGYO" varchar(1) NOT NULL default ' ',
  "AAAAWGYP" varchar(1) NOT NULL default ' ',
  "AAAAWGYQ" varchar(1) NOT NULL default ' ',
  "AAAAWGYR" varchar(2) NOT NULL default '00',
  "AAAAWGYS" varchar(1) NOT NULL default ' ',
  "AAAAWGYT" varchar(1) NOT NULL default ' ',
  "AAAAWGYU" varchar(1) NOT NULL default ' ',
  "AAAAWGYV" varchar(1) NOT NULL default ' ',
  "AAAAWGYW" varchar(1) NOT NULL default ' ',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  "AAABMBTQ" varchar(16) NOT NULL default ' ',
  "AAAAWGYZ" varchar(1) NOT NULL default ' ',
  "AAAAWGZA" varchar(1) NOT NULL default ' ',
  "AAAAWGZB" varchar(1) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAAAWGZD" varchar(1) NOT NULL default ' ',
  "AAAAWGZE" varchar(4) NOT NULL default ' ',
  "AAAAWGZF" varchar(4) NOT NULL default ' ',
  "AAAAWGZG" varchar(1) NOT NULL default ' ',
  "AAAAWGZH" varchar(1) NOT NULL default ' ',
  "AAAAWGZI" varchar(1) NOT NULL default ' ',
  "AAAAWGZJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAABLTNT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWGZN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMCPY" varchar(2) NOT NULL default ' ',
  "AAAAWHCT" varchar(4) NOT NULL default ' ',
  "AAAAWGZS" varchar(40) NOT NULL default ' ',
  "AAAAWHCU" decimal(5,2) NOT NULL default '0.00',
  "AAABLOIO" decimal(4,2) NOT NULL default '0.00',
  "AAABLTLX" varchar(1) NOT NULL default ' ',
  "AAABLTLY" varchar(1) NOT NULL default ' ',
  "AAAAWHCW" decimal(4,2) NOT NULL default '0.00',
  "AAAAWHCX" varchar(1) NOT NULL default ' ',
  "AAAAWGZZ" decimal(13,2) NOT NULL default '0.00',
  "AAAAWHAA" varchar(5) NOT NULL default ' ',
  "AAABHIPZ" varchar(3) NOT NULL default ' ',
  "AAAAWHCZ" varchar(3) NOT NULL default ' ',
  "AAAAWHDA" varchar(4) NOT NULL default ' ',
  "AAAAWHAE" decimal(4,2) NOT NULL default '0.00',
  "AAAAWHAF" varchar(10) NOT NULL default ' ',
  "AAAAWHAG" varchar(1) NOT NULL default ' ',
  "AAAAWHAH" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAABMCPY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHAL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABHIPZ" varchar(3) NOT NULL default ' ',
  "AAAAWHCZ" varchar(3) NOT NULL default ' ',
  "AAABLOHP" varchar(2) NOT NULL default ' ',
  "AAAAWHBF" varchar(3) NOT NULL default ' ',
  "AAAAYZUI" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAABHIPZ","AAAAWHCZ","AAABLOHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHAY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLTNT" varchar(2) NOT NULL default ' ',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  "AAAAWHCZ" varchar(3) NOT NULL default ' ',
  "AAABLOHP" varchar(2) NOT NULL default ' ',
  "AAAAWHBF" varchar(3) NOT NULL default ' ',
  "AAAAYZUI" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAABLTNT","AAABMQNB","AAAAWHCZ","AAABLOHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHBN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLTNT" varchar(2) NOT NULL default ' ',
  "AAABLOFJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMP","AAABLTNT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHBX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAWHCN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAAAWHCN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHCE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAWHCN" varchar(2) NOT NULL default ' ',
  "AAABMLMP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMP","AAAAWHCN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHCO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLTNT" varchar(2) NOT NULL default ' ',
  "AAABLTNU" varchar(2) NOT NULL default ' ',
  "AAAAWHCT" varchar(4) NOT NULL default ' ',
  "AAAAWHCU" decimal(5,2) NOT NULL default '0.00',
  "AAABLOIO" decimal(7,4) NOT NULL default '0.0000',
  "AAAAWHCW" decimal(7,4) NOT NULL default '0.0000',
  "AAAAWHCX" varchar(1) NOT NULL default ' ',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  "AAAAWHCZ" varchar(3) NOT NULL default ' ',
  "AAAAWHDA" varchar(4) NOT NULL default ' ',
  "AAAAWHDB" varchar(1) NOT NULL default ' ',
  "AAAAWHDC" smallint(6) NOT NULL default '0',
  "AAAAWHDD" smallint(6) NOT NULL default '0',
  "AAAAWHEX" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAABLTNT","AAABLTNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHDJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLTNT" varchar(2) NOT NULL default ' ',
  "AAABLTNU" varchar(2) NOT NULL default ' ',
  "AAABLOFJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMP","AAABLTNT","AAABLTNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHDV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLTNT" varchar(2) NOT NULL default ' ',
  "AAABLTNU" varchar(2) NOT NULL default ' ',
  "AAAAWHEG" varchar(8) NOT NULL default '00000000',
  "AAAAWHEB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAABLTNT","AAABLTNU","AAAAWHEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHEH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAWHEX" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAAAWHEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHEO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAWHEX" varchar(16) NOT NULL default ' ',
  "AAAAWHET" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMP","AAAAWHEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHEY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAEF" varchar(10) NOT NULL default ' ',
  "AAAAWHFB" varchar(1) NOT NULL default ' ',
  "AAABKXRS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBAEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHFF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBAEF" varchar(10) NOT NULL default ' ',
  "AAAAWYHG" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBAEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHFN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABLYAE" varchar(3) NOT NULL default '000',
  "AAABBAEF" varchar(10) NOT NULL default ' ',
  "AAABMPTM" decimal(15,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCPV","AAABMDBB","AAABLYAE","AAABBAEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHGB" (
  "AAABGKWP" varchar(6) NOT NULL default ' ',
  "AAABLZCP" varchar(35) NOT NULL default ' ',
  "AAABLVHI" varchar(35) NOT NULL default ' ',
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  "AAABLQTM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGKWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHGI" (
  "AAABGKWP" varchar(6) NOT NULL default ' ',
  "AAABMMYX" varchar(6) NOT NULL default ' ',
  "AAABGPKK" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABGKWP","AAABMMYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHGO" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGKWP" varchar(6) NOT NULL default ' ',
  "AAABMMYX" varchar(6) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABGKWP","AAABMMYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHGX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGKWP" varchar(6) NOT NULL default ' ',
  "AAABMMYX" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGKWP","AAABMMYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHHE" (
  "AAABGIKF" varchar(40) NOT NULL default ' ',
  "AAABENMX" varchar(2) NOT NULL default '00',
  "AAABENMA" varchar(30) NOT NULL default ' ',
  "AAABGKWP" varchar(6) NOT NULL default ' ',
  "AAABMMYX" varchar(6) NOT NULL default ' ',
  "AAABJTCY" varchar(30) NOT NULL default ' ',
  "AAABFWHA" varchar(8) NOT NULL default ' ',
  "AAAAWHHM" varchar(4) NOT NULL default ' ',
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAAAWHHO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGIKF","AAABENMX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHHR" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGIKF" varchar(40) NOT NULL default ' ',
  "AAABENMX" varchar(2) NOT NULL default '00',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  "AAABGKWP" varchar(6) NOT NULL default ' ',
  "AAABMMYX" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABGIKF","AAABENMX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHIB" (
  "AAABMGHE" varchar(4) NOT NULL default '0000',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABHSKS" varchar(2) NOT NULL default ' ',
  "AAAAWHIF" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMGHE","AAABMNLW","AAABHSKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHIJ" (
  "AAABLSDV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSDV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHIM" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLSDV" varchar(1) NOT NULL default ' ',
  "AAABJBVB" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLSDV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHIS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABMCPR" varchar(1) NOT NULL default ' ',
  "AAABMCPS" varchar(1) NOT NULL default ' ',
  "AAAAWHIX" varchar(1) NOT NULL default ' ',
  "AAAAWHIY" varchar(20) NOT NULL default ' ',
  "AAABHKKO" varchar(1) NOT NULL default ' ',
  "AAAAWHJA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPWP","AAABMCPR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHJE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWHKA" varchar(2) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLZBN" varchar(10) NOT NULL default ' ',
  "AAAAWHKE" varchar(1) NOT NULL default ' ',
  "AAAAWHJL" varchar(1) NOT NULL default ' ',
  "AAAAWHJM" varchar(1) NOT NULL default ' ',
  "AAABJRHX" varchar(4) NOT NULL default ' ',
  "AAAAWHJO" varchar(10) NOT NULL default ' ',
  "AAAAWHJP" varchar(3) NOT NULL default '000',
  "AAABLOLZ" varchar(8) NOT NULL default ' ',
  "AAAAWHJR" varchar(1) NOT NULL default ' ',
  "AAABLRBU" varchar(1) NOT NULL default ' ',
  "AAAAWHJT" varchar(1) NOT NULL default ' ',
  "AAABIEXL" varchar(2) NOT NULL default ' ',
  "AAABCDET" varchar(1) NOT NULL default ' ',
  "AAABJXED" varchar(8) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABGPJB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWHKA","AAABMPWP","AAABMREX","AAABLZBN","AAAAWHKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHKF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWHON" varchar(2) NOT NULL default ' ',
  "AAABLZBN" varchar(10) NOT NULL default ' ',
  "AAAAWHKO" varchar(35) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWHON","AAABLZBN","AAAAWHKO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHKP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWHON" varchar(2) NOT NULL default ' ',
  "AAABLZBN" varchar(10) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABJWRR" varchar(3) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAABMCZD" varchar(2) NOT NULL default ' ',
  "AAABJWRW" varchar(2) NOT NULL default ' ',
  "AAAAWHKZ" varchar(3) NOT NULL default ' ',
  "AAAAWHLA" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWHON","AAABLZBN","AAABMREX","AAABMCTJ","AAABJWRR","AAABMCTG","AAABMCZD","AAABJWRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHLK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWHON" varchar(2) NOT NULL default ' ',
  "AAABLZBN" varchar(10) NOT NULL default ' ',
  "AAAAWHOP" varchar(4) NOT NULL default ' ',
  "AAAAWHLX" varchar(30) NOT NULL default ' ',
  "AAAAWHLQ" varchar(20) NOT NULL default ' ',
  "AAAAWHLR" varchar(10) NOT NULL default ' ',
  "AAAAWHLS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWHON","AAABLZBN","AAAAWHOP","AAAAWHLX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHLY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWHON" varchar(2) NOT NULL default ' ',
  "AAABLZBN" varchar(10) NOT NULL default ' ',
  "AAAAWHOP" varchar(4) NOT NULL default ' ',
  "AAAAWHMX" varchar(30) NOT NULL default ' ',
  "AAABMEDK" varchar(20) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABKROQ" varchar(10) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABLXSX" varchar(8) NOT NULL default '00000000',
  "AAABMCXJ" varchar(12) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAABMCXN" varchar(10) NOT NULL default '0000000000',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAWHON","AAABLZBN","AAAAWHOP","AAAAWHMX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHMY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWHON" varchar(2) NOT NULL default ' ',
  "AAABLZBN" varchar(10) NOT NULL default ' ',
  "AAABMCQO" varchar(3) NOT NULL default ' ',
  "AAAAWHNK" varchar(17) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWHON","AAABLZBN","AAABMCQO","AAAAWHNK","AAABMAMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHNM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWHON" varchar(2) NOT NULL default ' ',
  "AAABLZBN" varchar(10) NOT NULL default ' ',
  "AAAAWHOP" varchar(4) NOT NULL default ' ',
  "AAAAWHNR" varchar(1) NOT NULL default ' ',
  "AAAAWHNS" varchar(1) NOT NULL default ' ',
  "AAAAWHNT" varchar(1) NOT NULL default ' ',
  "AAAAWHNU" varchar(1) NOT NULL default ' ',
  "AAAAWHNV" varchar(1) NOT NULL default ' ',
  "AAAAWHNW" varchar(1) NOT NULL default ' ',
  "AAAAWHNX" varchar(1) NOT NULL default ' ',
  "AAAAWHNY" varchar(2) NOT NULL default ' ',
  "AAAAWHNZ" varchar(2) NOT NULL default ' ',
  "AAAAWHOA" varchar(2) NOT NULL default ' ',
  "AAAAWHOB" varchar(2) NOT NULL default ' ',
  "AAAAWHOC" varchar(2) NOT NULL default ' ',
  "AAAAWHOD" varchar(2) NOT NULL default ' ',
  "AAAAWHOE" varchar(2) NOT NULL default ' ',
  "AAAAWHOF" varchar(3) NOT NULL default ' ',
  "AAAAWHOG" varchar(10) NOT NULL default ' ',
  "AAAAWHOH" varchar(10) NOT NULL default ' ',
  "AAAAWHOI" varchar(1) NOT NULL default ' ',
  "AAAAWHOJ" varchar(1) NOT NULL default ' ',
  "AAAAWHOK" varchar(1) NOT NULL default ' ',
  "AAAAWHOL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWHON","AAABLZBN","AAAAWHOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHOQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLMR" varchar(4) NOT NULL default ' ',
  "AAAAWHRB" varchar(40) NOT NULL default ' ',
  "AAAAWHRY" varchar(40) NOT NULL default ' ',
  "AAAAWHSQ" varchar(40) NOT NULL default ' ',
  "AAABIEWH" varchar(2) NOT NULL default ' ',
  "AAAAWHPT" varchar(1) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAAAWHOZ" varchar(1) NOT NULL default ' ',
  "AAAAWHPA" varchar(1) NOT NULL default ' ',
  "AAAAWHPB" varchar(1) NOT NULL default ' ',
  "AAAAWHPC" varchar(1) NOT NULL default ' ',
  "AAAAWHPD" varchar(1) NOT NULL default ' ',
  "AAAAWLJC" varchar(40) NOT NULL default ' ',
  "AAABHXNQ" varchar(40) NOT NULL default ' ',
  "AAAAWHRZ" varchar(40) NOT NULL default ' ',
  "AAAAWHPH" varchar(1) NOT NULL default ' ',
  "AAAAWLJD" varchar(40) NOT NULL default ' ',
  "AAAAWHSR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLMR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHPM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHXNO" varchar(4) NOT NULL default ' ',
  "AAAAWHRB" varchar(40) NOT NULL default ' ',
  "AAAAWHRY" varchar(40) NOT NULL default ' ',
  "AAAAWHSF" varchar(40) NOT NULL default ' ',
  "AAABIEWH" varchar(2) NOT NULL default ' ',
  "AAAAWHPT" varchar(1) NOT NULL default ' ',
  "AAAAWLJC" varchar(40) NOT NULL default ' ',
  "AAABHXNQ" varchar(40) NOT NULL default ' ',
  "AAAAWHSG" varchar(40) NOT NULL default ' ',
  "AAABFDPR" varchar(1) NOT NULL default ' ',
  "AAABFDPS" varchar(1) NOT NULL default ' ',
  "AAAAWHPZ" varchar(4) NOT NULL default ' ',
  "AAAAWHQA" varchar(4) NOT NULL default ' ',
  "AAAAWHQB" varchar(4) NOT NULL default ' ',
  "AAAAWHQC" varchar(1) NOT NULL default ' ',
  "AAABMLMR" varchar(4) NOT NULL default ' ',
  "AAAAWHRZ" varchar(40) NOT NULL default ' ',
  "AAAAWHSI" varchar(40) NOT NULL default ' ',
  "AAAAWHSJ" varchar(40) NOT NULL default ' ',
  "AAAAWHSR" varchar(40) NOT NULL default ' ',
  "AAAAWLJD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHXNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHQL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAAAWHRB" varchar(40) NOT NULL default ' ',
  "AAAAWHRY" varchar(40) NOT NULL default ' ',
  "AAAAWHSQ" varchar(40) NOT NULL default ' ',
  "AAAAWLJC" varchar(40) NOT NULL default ' ',
  "AAABHXNQ" varchar(40) NOT NULL default ' ',
  "AAAAWHRZ" varchar(40) NOT NULL default ' ',
  "AAAAWLJD" varchar(40) NOT NULL default ' ',
  "AAAAWHSR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAAAWHRB" varchar(40) NOT NULL default ' ',
  "AAAAWHRY" varchar(40) NOT NULL default ' ',
  "AAAAWHSF" varchar(40) NOT NULL default ' ',
  "AAAAWLJC" varchar(40) NOT NULL default ' ',
  "AAABHXNQ" varchar(40) NOT NULL default ' ',
  "AAAAWHSG" varchar(40) NOT NULL default ' ',
  "AAAAWHRZ" varchar(40) NOT NULL default ' ',
  "AAAAWHSI" varchar(40) NOT NULL default ' ',
  "AAAAWHSJ" varchar(40) NOT NULL default ' ',
  "AAAAWHSR" varchar(40) NOT NULL default ' ',
  "AAAAWLJD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHRO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAWHRY" varchar(40) NOT NULL default ' ',
  "AAAAWHRZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHRV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAWHRY" varchar(40) NOT NULL default ' ',
  "AAAAWHRZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHSC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAAAWHSF" varchar(40) NOT NULL default ' ',
  "AAAAWHSG" varchar(40) NOT NULL default ' ',
  "AAABHXNQ" varchar(40) NOT NULL default ' ',
  "AAAAWHSI" varchar(40) NOT NULL default ' ',
  "AAAAWHSJ" varchar(40) NOT NULL default ' ',
  "AAAAWHSR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLURS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHSN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAAAWHSQ" varchar(40) NOT NULL default ' ',
  "AAAAWHSR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHSU" (
  "AAAAWHTB" varchar(14) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABGYQM" varchar(30) NOT NULL default ' ',
  "AAAAWHSY" varchar(30) NOT NULL default ' ',
  "AAAAWHSZ" varchar(1) NOT NULL default ' ',
  "AAABDESO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAWHTB","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHTD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWJDS" varchar(1) NOT NULL default ' ',
  "AAABBTQG" varchar(2) NOT NULL default ' ',
  "AAABMGOZ" varchar(2) NOT NULL default '00',
  "AAAAWIVJ" varchar(1) NOT NULL default '0',
  "AAAAWIVK" varchar(1) NOT NULL default '0',
  "AAAAWIVL" varchar(1) NOT NULL default '0',
  "AAAAWIVM" varchar(1) NOT NULL default '0',
  "AAAAWIVN" varchar(1) NOT NULL default '0',
  "AAAAWIVO" varchar(1) NOT NULL default '0',
  "AAAAWIVP" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAWJDS","AAABBTQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHTS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWHVL" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAWHVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHTX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWHVL" varchar(2) NOT NULL default '00',
  "AAAAWHUB" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWHVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHUF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWHVL" varchar(2) NOT NULL default '00',
  "AAABBTQG" varchar(2) NOT NULL default ' ',
  "AAABGYNX" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAWHVL","AAABBTQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHUN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWHVL" varchar(2) NOT NULL default '00',
  "AAABBTQG" varchar(2) NOT NULL default ' ',
  "AAABGYNX" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAWHVL","AAABBTQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHUV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWHVL" varchar(2) NOT NULL default '00',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAABICIR" varchar(2) NOT NULL default '00',
  "AAABGYNX" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAWHVL","AAABICIU","AAABICIR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHVF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGQUK" varchar(20) NOT NULL default ' ',
  "AAAAWHVL" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABGQUK","AAAAWHVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHVM" (
  "AAAAWJDS" varchar(1) NOT NULL default ' ',
  "AAABBTQG" varchar(2) NOT NULL default ' ',
  "AAAAWJDT" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAAAWJDS","AAABBTQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHVS" (
  "AAABGYQM" varchar(30) NOT NULL default ' ',
  "AAABGNBZ" varchar(30) NOT NULL default ' ',
  "AAAAWJDS" varchar(1) NOT NULL default ' ',
  "AAABBTQG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABGYQM","AAABGNBZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHVZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWJDS" varchar(1) NOT NULL default ' ',
  "AAAAWHWZ" varchar(4) NOT NULL default ' ',
  "AAABBTQG" varchar(2) NOT NULL default ' ',
  "AAAAWHWE" varchar(1) NOT NULL default ' ',
  "AAAAWHWF" varchar(1) NOT NULL default ' ',
  "AAAAWHWG" varchar(1) NOT NULL default ' ',
  "AAAAWHWH" varchar(1) NOT NULL default ' ',
  "AAAAWHWI" varchar(1) NOT NULL default ' ',
  "AAAAWHWJ" varchar(1) NOT NULL default ' ',
  "AAAAWHWK" varchar(1) NOT NULL default ' ',
  "AAAAWHWL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWJDS","AAAAWHWZ","AAABBTQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHWQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWJDS" varchar(1) NOT NULL default ' ',
  "AAAAWHWZ" varchar(4) NOT NULL default ' ',
  "AAABKHGP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWJDS","AAAAWHWZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHXA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBXXD" varchar(4) NOT NULL default ' ',
  "AAABBTQG" varchar(2) NOT NULL default ' ',
  "AAAAWHXN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBXXD","AAABBTQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHXI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBXXD" varchar(4) NOT NULL default ' ',
  "AAABBXWV" varchar(2) NOT NULL default ' ',
  "AAABBTQG" varchar(2) NOT NULL default ' ',
  "AAAAWHXN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBXXD","AAABBXWV","AAABBTQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHXS" (
  "AAAAWJDS" varchar(1) NOT NULL default ' ',
  "AAAAWJDT" varchar(3) NOT NULL default '000',
  "AAABFHJQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAWJDS","AAAAWJDT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHXY" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWJDS" varchar(1) NOT NULL default ' ',
  "AAAAWJDT" varchar(3) NOT NULL default '000',
  "AAAAWHYC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAAWJDS","AAAAWJDT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHYG" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWJDS" varchar(1) NOT NULL default ' ',
  "AAABBTQG" varchar(2) NOT NULL default ' ',
  "AAABGRZV" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAAWJDS","AAABBTQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHYO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAWHZI" varchar(5) NOT NULL default '00000',
  "AAABICIR" varchar(2) NOT NULL default '00',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAAAWHYX" varchar(1) NOT NULL default ' ',
  "AAAAWHYY" varchar(20) NOT NULL default ' ',
  "AAAAWHYZ" varchar(20) NOT NULL default ' ',
  "AAAAWHZA" varchar(20) NOT NULL default ' ',
  "AAAAWHZB" varchar(20) NOT NULL default ' ',
  "AAAAWHZC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMQVT","AAABMQVU","AAABMDBB","AAAAWHZI","AAABICIR","AAABLXDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWHZL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAAAWIBR" varchar(2) NOT NULL default ' ',
  "AAABDGZI" varchar(8) NOT NULL default '00000000',
  "AAAAWIPQ" varchar(8) NOT NULL default '00000000',
  "AAAAWHZR" varchar(1) NOT NULL default ' ',
  "AAAAWHZS" varchar(2) NOT NULL default '00',
  "AAAAWHZT" varchar(2) NOT NULL default '00',
  "AAAAWHZU" varchar(1) NOT NULL default ' ',
  "AAAAWIBN" decimal(7,4) NOT NULL default '0.0000',
  "AAAAWIBO" decimal(13,2) NOT NULL default '0.00',
  "AAAAWHZX" varchar(3) NOT NULL default ' ',
  "AAAAWHZY" varchar(2) NOT NULL default '00',
  "AAAAWHZZ" varchar(2) NOT NULL default '00',
  "AAAAWIAA" varchar(1) NOT NULL default ' ',
  "AAAAWIAB" varchar(1) NOT NULL default ' ',
  "AAAAWIAC" varchar(1) NOT NULL default ' ',
  "AAAAWIAD" varchar(1) NOT NULL default ' ',
  "AAAAWIAE" varchar(1) NOT NULL default ' ',
  "AAAAWIAF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABICIU","AAAAWIBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIAJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAAAWIBR" varchar(2) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABKGBX" varchar(8) NOT NULL default ' ',
  "AAAAWIAS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABICIU","AAAAWIBR","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIAX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAAAWIBR" varchar(2) NOT NULL default ' ',
  "AAAAWIBC" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABICIU","AAAAWIBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIBH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAAAWIBR" varchar(2) NOT NULL default ' ',
  "AAAAWIPQ" varchar(8) NOT NULL default '00000000',
  "AAABDGZI" varchar(8) NOT NULL default '00000000',
  "AAAAWIBN" decimal(7,4) NOT NULL default '0.0000',
  "AAAAWIBO" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABICIU","AAAAWIBR","AAAAWIPQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIBT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWIDK" varchar(4) NOT NULL default ' ',
  "AAAAWIBW" varchar(30) NOT NULL default ' ',
  "AAAAWIBX" varchar(30) NOT NULL default ' ',
  "AAAAWIBY" varchar(1) NOT NULL default ' ',
  "AAAAWIBZ" varchar(1) NOT NULL default ' ',
  "AAAAWICA" varchar(30) NOT NULL default ' ',
  "AAAAWICB" varchar(30) NOT NULL default ' ',
  "AAAAWICC" varchar(1) NOT NULL default ' ',
  "AAAAWICD" varchar(1) NOT NULL default ' ',
  "AAAAWICE" varchar(30) NOT NULL default ' ',
  "AAAAWICF" varchar(30) NOT NULL default ' ',
  "AAAAWICG" varchar(1) NOT NULL default ' ',
  "AAAAWICH" varchar(1) NOT NULL default ' ',
  "AAAAWICI" varchar(30) NOT NULL default ' ',
  "AAAAWICJ" varchar(30) NOT NULL default ' ',
  "AAAAWICK" varchar(1) NOT NULL default ' ',
  "AAAAWICL" varchar(1) NOT NULL default ' ',
  "AAAAWICM" varchar(30) NOT NULL default ' ',
  "AAAAWICN" varchar(30) NOT NULL default ' ',
  "AAAAWICO" varchar(1) NOT NULL default ' ',
  "AAAAWICP" varchar(1) NOT NULL default ' ',
  "AAAAWICQ" varchar(1) NOT NULL default ' ',
  "AAAAWICR" varchar(1) NOT NULL default ' ',
  "AAAAWICS" varchar(2) NOT NULL default '00',
  "AAAAWICT" varchar(2) NOT NULL default '00',
  "AAAAWICU" varchar(2) NOT NULL default '00',
  "AAAAWICV" varchar(2) NOT NULL default '00',
  "AAAAWICW" varchar(2) NOT NULL default '00',
  "AAAAWICX" varchar(2) NOT NULL default '00',
  "AAAAWICY" varchar(2) NOT NULL default '00',
  "AAAAWICZ" varchar(2) NOT NULL default '00',
  "AAAAWIDA" varchar(2) NOT NULL default '00',
  "AAAAWIDB" varchar(2) NOT NULL default '00',
  "AAAAWIDC" varchar(1) NOT NULL default ' ',
  "AAAAWIDD" varchar(1) NOT NULL default ' ',
  "AAAAWIDE" varchar(1) NOT NULL default ' ',
  "AAAAWIDF" varchar(1) NOT NULL default ' ',
  "AAAAWIDG" varchar(1) NOT NULL default ' ',
  "AAAAWIDH" varchar(1) NOT NULL default '0',
  "AAAAWIDI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWIDK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIDL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNXB" varchar(1) NOT NULL default ' ',
  "AAAAWIFB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGNXB","AAAAWIFB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIDS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWIER" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWIER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIDX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKOVI" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKOVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIEC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKOVI" varchar(5) NOT NULL default ' ',
  "AAABEPBY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKOVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIEK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWIER" varchar(8) NOT NULL default ' ',
  "AAAAWIEO" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWIER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIES" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGNXB" varchar(1) NOT NULL default ' ',
  "AAAAWIFB" varchar(4) NOT NULL default ' ',
  "AAAAZJLS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGNXB","AAAAWIFB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIFC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWIFO" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWIFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIFH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWIFO" varchar(12) NOT NULL default ' ',
  "AAAAWIFL" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWIFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIFP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAAAWIFY" varchar(2) NOT NULL default '00',
  "AAAAWIFT" varchar(8) NOT NULL default '00000000',
  "AAAAWIFU" varchar(2) NOT NULL default '00',
  "AAAAWIFV" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABICIU","AAAAWIFY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIFZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWIRE" varchar(4) NOT NULL default ' ',
  "AAAAWISX" varchar(1) NOT NULL default ' ',
  "AAAAWINN" varchar(1) NOT NULL default ' ',
  "AAAAWIGE" varchar(1) NOT NULL default ' ',
  "AAAAWILA" decimal(3,2) NOT NULL default '0.00',
  "AAAAWILB" decimal(7,4) NOT NULL default '0.0000',
  "AAAAWILC" decimal(7,4) NOT NULL default '0.0000',
  "AAAAWIGI" varchar(4) NOT NULL default ' ',
  "AAAAWIKM" decimal(3,1) NOT NULL default '0.0',
  "AAAAWIYL" varchar(1) NOT NULL default ' ',
  "AAAAWIYI" varchar(1) NOT NULL default ' ',
  "AAAAWIYJ" varchar(1) NOT NULL default ' ',
  "AAAAWIYK" varchar(1) NOT NULL default ' ',
  "AAAAWIMS" varchar(2) NOT NULL default ' ',
  "AAAAWIMT" varchar(2) NOT NULL default ' ',
  "AAAAWIMU" varchar(2) NOT NULL default ' ',
  "AAAAWIMV" varchar(2) NOT NULL default ' ',
  "AAAAWIJL" varchar(1) NOT NULL default ' ',
  "AAAAWINP" varchar(1) NOT NULL default ' ',
  "AAAAWIPA" varchar(1) NOT NULL default ' ',
  "AAAAWIMW" varchar(1) NOT NULL default ' ',
  "AAAAWIKL" varchar(1) NOT NULL default ' ',
  "AAAAWIKQ" varchar(1) NOT NULL default ' ',
  "AAAAWINO" varchar(1) NOT NULL default ' ',
  "AAAAWIJM" varchar(1) NOT NULL default ' ',
  "AAAAWIYM" varchar(1) NOT NULL default ' ',
  "AAAAWIYN" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAWIRE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAAAWJKJ" varchar(4) NOT NULL default ' ',
  "AAAAWIHI" varchar(4) NOT NULL default ' ',
  "AAAAWIHJ" varchar(4) NOT NULL default ' ',
  "AAAAWIYO" varchar(4) NOT NULL default ' ',
  "AAAAWIUO" varchar(3) NOT NULL default ' ',
  "AAAAWIJG" varchar(1) NOT NULL default ' ',
  "AAAAWIJH" varchar(1) NOT NULL default ' ',
  "AAAAWIHO" varchar(1) NOT NULL default ' ',
  "AAAAWJAG" varchar(1) NOT NULL default ' ',
  "AAAAWIJJ" varchar(1) NOT NULL default ' ',
  "AAAAWIJK" varchar(1) NOT NULL default '0',
  "AAAAWIHS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABICIU","AAAAWJKJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIHW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAWJKJ" varchar(4) NOT NULL default ' ',
  "AAAAWIIH" varchar(4) NOT NULL default '0000',
  "AAAAWXUF" varchar(3) NOT NULL default '000',
  "AAAAWIIC" decimal(15,0) NOT NULL default '0',
  "AAAAWIID" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAAWJKJ","AAAAWIIH","AAAAWXUF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIIJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKGBX" varchar(8) NOT NULL default ' ',
  "AAABIAWX" varchar(2) NOT NULL default '00',
  "AAAAWILY" varchar(8) NOT NULL default '00000000',
  "AAAAWIIP" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKGBX","AAABIAWX","AAAAWILY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIIV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWIIX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIIZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAAAWJKJ" varchar(4) NOT NULL default ' ',
  "AAABCDTJ" varchar(1) NOT NULL default ' ',
  "AAAAWIMN" varchar(3) NOT NULL default ' ',
  "AAAAWIUO" varchar(3) NOT NULL default ' ',
  "AAAAWIJG" varchar(1) NOT NULL default ' ',
  "AAAAWIJH" varchar(1) NOT NULL default ' ',
  "AAAAWJAG" varchar(1) NOT NULL default ' ',
  "AAAAWIJJ" varchar(1) NOT NULL default ' ',
  "AAAAWIJK" varchar(1) NOT NULL default '0',
  "AAAAWIJL" varchar(1) NOT NULL default ' ',
  "AAAAWIJM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABICIU","AAAAWJKJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIJQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAAAWJKJ" varchar(4) NOT NULL default ' ',
  "AAAAWIRK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABICIU","AAAAWJKJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIKA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAAAWJKJ" varchar(4) NOT NULL default ' ',
  "AAAAWINN" varchar(1) NOT NULL default ' ',
  "AAABHFAO" varchar(1) NOT NULL default ' ',
  "AAAAWIRE" varchar(4) NOT NULL default ' ',
  "AAAAWILP" varchar(3) NOT NULL default ' ',
  "AAAAWIYP" varchar(3) NOT NULL default ' ',
  "AAAAWIQA" varchar(3) NOT NULL default ' ',
  "AAAAWIMN" varchar(3) NOT NULL default ' ',
  "AAAAWIKL" varchar(1) NOT NULL default ' ',
  "AAAAWIKM" decimal(3,1) NOT NULL default '0.0',
  "AAAAWIYM" varchar(1) NOT NULL default ' ',
  "AAAAWIYN" varchar(1) NOT NULL default '0',
  "AAAAWIYL" varchar(1) NOT NULL default ' ',
  "AAAAWIKQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABICIU","AAAAWJKJ","AAAAWINN","AAABHFAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAAAWILP" varchar(3) NOT NULL default ' ',
  "AAAAWILA" decimal(3,2) NOT NULL default '0.00',
  "AAAAWILB" decimal(7,4) NOT NULL default '0.0000',
  "AAAAWILC" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABICIU","AAAAWILP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWILG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAAAWILP" varchar(3) NOT NULL default ' ',
  "AAAAWILL" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABICIU","AAAAWILP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWILQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWIMN" varchar(3) NOT NULL default ' ',
  "AAAAWILY" varchar(8) NOT NULL default '00000000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAWILV" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAWIMN","AAAAWILY","AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIMA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWIMN" varchar(3) NOT NULL default ' ',
  "AAAAWIME" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWIMN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIMI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWIMN" varchar(3) NOT NULL default ' ',
  "AAAAWIML" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWIMN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIMO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAAAWIYP" varchar(3) NOT NULL default ' ',
  "AAAAWIMS" varchar(2) NOT NULL default ' ',
  "AAAAWIMT" varchar(2) NOT NULL default ' ',
  "AAAAWIMU" varchar(2) NOT NULL default ' ',
  "AAAAWIMV" varchar(2) NOT NULL default ' ',
  "AAAAWIMW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABICIU","AAAAWIYP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWINA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAAAWIYP" varchar(3) NOT NULL default ' ',
  "AAAAWINF" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABICIU","AAAAWIYP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWINK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWIRE" varchar(4) NOT NULL default ' ',
  "AAAAWINN" varchar(1) NOT NULL default ' ',
  "AAAAWINO" varchar(1) NOT NULL default ' ',
  "AAAAWINP" varchar(1) NOT NULL default ' ',
  "AAAAWIYI" varchar(1) NOT NULL default ' ',
  "AAAAWIYJ" varchar(1) NOT NULL default ' ',
  "AAAAWIYK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWIRE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWINV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWIRE" varchar(4) NOT NULL default ' ',
  "AAAAWINZ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWIRE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIOD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAAAWIQA" varchar(3) NOT NULL default ' ',
  "AAABJRIV" varchar(4) NOT NULL default '0000',
  "AAAAWITX" varchar(3) NOT NULL default '000',
  "AAAAWITY" varchar(3) NOT NULL default '000',
  "AAAAWIQH" varchar(2) NOT NULL default ' ',
  "AAAAWIQI" decimal(7,4) NOT NULL default '0.0000',
  "AAAAWIQJ" varchar(1) NOT NULL default ' ',
  "AAAAWIQK" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABICIU","AAAAWIQA","AAABJRIV","AAAAWITX","AAAAWITY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAAAWIQA" varchar(3) NOT NULL default ' ',
  "AAAAWIOY" varchar(1) NOT NULL default ' ',
  "AAAAWISX" varchar(1) NOT NULL default ' ',
  "AAAAWIPA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABICIU","AAAAWIQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIPE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAAAWIQA" varchar(3) NOT NULL default ' ',
  "AAAAWIPQ" varchar(8) NOT NULL default '00000000',
  "AAAAWIQH" varchar(2) NOT NULL default ' ',
  "AAAAWIQI" decimal(7,4) NOT NULL default '0.0000',
  "AAAAWIQJ" varchar(1) NOT NULL default ' ',
  "AAAAWIQK" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABICIU","AAAAWIQA","AAAAWIPQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIPR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAAAWIQA" varchar(3) NOT NULL default ' ',
  "AAAAWIPW" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABICIU","AAAAWIQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIQB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWIRE" varchar(4) NOT NULL default ' ',
  "AAABJRIV" varchar(4) NOT NULL default '0000',
  "AAAAWITX" varchar(3) NOT NULL default '000',
  "AAAAWITY" varchar(3) NOT NULL default '000',
  "AAAAWIQH" varchar(2) NOT NULL default ' ',
  "AAAAWIQI" decimal(7,4) NOT NULL default '0.0000',
  "AAAAWIQJ" varchar(1) NOT NULL default ' ',
  "AAAAWIQK" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAAAWIRE","AAABJRIV","AAAAWITX","AAAAWITY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGIIR" varchar(2) NOT NULL default ' ',
  "AAAAWIQT" varchar(1) NOT NULL default ' ',
  "AAAAWIQU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGIIR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIQX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWIRE" varchar(4) NOT NULL default ' ',
  "AAAAWIRK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWIRE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIRF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAAAWJKJ" varchar(4) NOT NULL default ' ',
  "AAAAWIRK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABICIU","AAAAWJKJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIRP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWIRW" varchar(2) NOT NULL default ' ',
  "AAAAWIRT" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWIRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIRX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGIIR" varchar(2) NOT NULL default ' ',
  "AAAAWISB" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGIIR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWISF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAABGIIR" varchar(2) NOT NULL default ' ',
  "AAAAXAKY" varchar(4) NOT NULL default '0000',
  "AAAAXAKZ" varchar(2) NOT NULL default '00',
  "AAAAWIST" varchar(2) NOT NULL default '00',
  "AAAAWISM" varchar(3) NOT NULL default '000',
  "AAAAWISN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLXQ","AAABGIIR","AAAAXAKY","AAAAXAKZ","AAAAWIST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWISU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWIUO" varchar(3) NOT NULL default ' ',
  "AAAAWISX" varchar(1) NOT NULL default ' ',
  "AAAAWISY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWIUO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWITB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWIUG" varchar(2) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABLZMT" varchar(2) NOT NULL default '00',
  "AAABMCMR" varchar(2) NOT NULL default '00',
  "AAAAWITI" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAWIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWITL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWIUO" varchar(3) NOT NULL default ' ',
  "AAABKKEO" varchar(4) NOT NULL default '0000',
  "AAABJRIV" varchar(4) NOT NULL default '0000',
  "AAAAWITX" varchar(3) NOT NULL default '000',
  "AAAAWITY" varchar(3) NOT NULL default '000',
  "AAAAWITS" decimal(5,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAAAWIUO","AAABKKEO","AAABJRIV","AAAAWITX","AAAAWITY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWITZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWIUG" varchar(2) NOT NULL default ' ',
  "AAABEPBY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIUH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWIUO" varchar(3) NOT NULL default ' ',
  "AAAAWIUL" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWIUO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIUP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAABICIR" varchar(2) NOT NULL default '00',
  "AAAAWJAM" varchar(1) NOT NULL default ' ',
  "AAAAWIUU" varchar(1) NOT NULL default ' ',
  "AAAAWIUV" varchar(2) NOT NULL default '00',
  "AAAAWIUW" varchar(1) NOT NULL default '0',
  "AAAAWIUX" varchar(1) NOT NULL default ' ',
  "AAAAWIUY" varchar(2) NOT NULL default '00',
  "AAAAWIUZ" varchar(1) NOT NULL default '0',
  "AAAAWIVA" varchar(1) NOT NULL default ' ',
  "AAAAWIVB" varchar(2) NOT NULL default '00',
  "AAAAWIVC" varchar(1) NOT NULL default '0',
  "AAAAWIVD" varchar(1) NOT NULL default ' ',
  "AAAAWIVE" varchar(2) NOT NULL default '00',
  "AAAAWIVF" varchar(1) NOT NULL default '0',
  "AAAAWIVG" varchar(1) NOT NULL default ' ',
  "AAAAWIVH" varchar(1) NOT NULL default ' ',
  "AAAAWIVI" varchar(1) NOT NULL default ' ',
  "AAAAWIVJ" varchar(1) NOT NULL default '0',
  "AAAAWIVK" varchar(1) NOT NULL default '0',
  "AAAAWIVL" varchar(1) NOT NULL default '0',
  "AAAAWIVM" varchar(1) NOT NULL default '0',
  "AAAAWIVN" varchar(1) NOT NULL default '0',
  "AAAAWIVO" varchar(1) NOT NULL default '0',
  "AAAAWIVP" varchar(1) NOT NULL default '0',
  "AAAAWIVQ" varchar(1) NOT NULL default ' ',
  "AAAAWIVR" varchar(1) NOT NULL default ' ',
  "AAAAWIVS" varchar(1) NOT NULL default ' ',
  "AAAAWIVT" varchar(1) NOT NULL default ' ',
  "AAAAWIVU" varchar(1) NOT NULL default ' ',
  "AAAAWIVV" varchar(1) NOT NULL default ' ',
  "AAAAWIVW" varchar(1) NOT NULL default ' ',
  "AAAAWIVX" varchar(1) NOT NULL default ' ',
  "AAAAWIVY" varchar(1) NOT NULL default ' ',
  "AAAAWIVZ" varchar(1) NOT NULL default ' ',
  "AAAAWIWA" varchar(2) NOT NULL default '00',
  "AAABBXWV" varchar(2) NOT NULL default ' ',
  "AAABCBVB" varchar(4) NOT NULL default ' ',
  "AAAAWJET" varchar(1) NOT NULL default ' ',
  "AAAAWJER" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABICIU","AAABICIR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIWI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAABIAWX" varchar(2) NOT NULL default '00',
  "AAAAWIWM" varchar(2) NOT NULL default '00',
  "AAAAWIWN" varchar(2) NOT NULL default '00',
  "AAAAWIWO" varchar(1) NOT NULL default ' ',
  "AAAAWIWP" varchar(1) NOT NULL default ' ',
  "AAAAWIWQ" varchar(1) NOT NULL default ' ',
  "AAAAWIWR" varchar(4) NOT NULL default ' ',
  "AAAAWIWS" varchar(1) NOT NULL default ' ',
  "AAAAWIWT" varchar(1) NOT NULL default ' ',
  "AAAAWIWU" varchar(1) NOT NULL default ' ',
  "AAAAWIWV" varchar(1) NOT NULL default ' ',
  "AAAAWIWW" varchar(3) NOT NULL default ' ',
  "AAAAWIWX" varchar(3) NOT NULL default ' ',
  "AAAAWIWY" varchar(1) NOT NULL default '0',
  "AAABGAJY" varchar(2) NOT NULL default ' ',
  "AAAAWIXA" varchar(2) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABICIU","AAABIAWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIXF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABIAWX" varchar(2) NOT NULL default '00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAWIXK" varchar(1) NOT NULL default '0',
  "AAAAWIXL" decimal(13,2) NOT NULL default '0.00',
  "AAAAWIXM" decimal(13,2) NOT NULL default '0.00',
  "AAAAWIXN" decimal(13,2) NOT NULL default '0.00',
  "AAAAWIXO" decimal(13,2) NOT NULL default '0.00',
  "AAABHDEA" varchar(1) NOT NULL default ' ',
  "AAAAWIXQ" varchar(1) NOT NULL default '0',
  "AAAAWIXR" varchar(4) NOT NULL default '0000',
  "AAAAWIXS" varchar(3) NOT NULL default '000',
  "AAAAWIXT" varchar(1) NOT NULL default ' ',
  "AAAAWIXU" varchar(4) NOT NULL default ' ',
  "AAAAWIXV" varchar(1) NOT NULL default ' ',
  "AAAAWIXW" varchar(1) NOT NULL default ' ',
  "AAAAWIXX" varchar(1) NOT NULL default ' ',
  "AAAAWIXY" varchar(1) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAABLYKS" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABIAWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIYE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABIAWX" varchar(2) NOT NULL default '00',
  "AAAAWIYI" varchar(1) NOT NULL default ' ',
  "AAAAWIYJ" varchar(1) NOT NULL default ' ',
  "AAAAWIYK" varchar(1) NOT NULL default ' ',
  "AAAAWIYL" varchar(1) NOT NULL default ' ',
  "AAAAWIYM" varchar(1) NOT NULL default ' ',
  "AAAAWIYN" varchar(1) NOT NULL default '0',
  "AAAAWIYO" varchar(4) NOT NULL default ' ',
  "AAAAWIYP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABIAWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIYT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAWIYW" varchar(4) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAAAWIYY" varchar(2) NOT NULL default '00',
  "AAAAWIYZ" varchar(1) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAAAWIZC" varchar(2) NOT NULL default '00',
  "AAAAWIZD" varchar(1) NOT NULL default ' ',
  "AAAAWIZE" varchar(1) NOT NULL default ' ',
  "AAAAWIZF" varchar(1) NOT NULL default '0',
  "AAAAWIZG" varchar(4) NOT NULL default '0000',
  "AAAAWIZH" varchar(2) NOT NULL default ' ',
  "AAAAWIZI" varchar(1) NOT NULL default ' ',
  "AAAAWIZJ" varchar(2) NOT NULL default ' ',
  "AAAAWIZK" varchar(2) NOT NULL default ' ',
  "AAAAWIZL" varchar(1) NOT NULL default ' ',
  "AAABHDLK" varchar(6) NOT NULL default ' ',
  "AAAAWIZN" tinyint(3) unsigned NOT NULL default '0',
  "AAAAWIZO" tinyint(3) unsigned NOT NULL default '0',
  "AAAAWIZP" varchar(1) NOT NULL default ' ',
  "AAAAWIZQ" varchar(14) NOT NULL default ' ',
  "AAAAWIZR" varchar(1) NOT NULL default ' ',
  "AAAAWIZS" varchar(1) NOT NULL default ' ',
  "AAAAWIZT" varchar(2) NOT NULL default ' ',
  "AAAAWIZU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWIZX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJAD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAWJAG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJAJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBXWV" varchar(2) NOT NULL default ' ',
  "AAAAWJAM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBXWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJAP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABBXWV" varchar(2) NOT NULL default ' ',
  "AAABEPBY" varchar(50) NOT NULL default ' ',
  "AAAAXQFJ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABBXWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJAY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAWJBG" varchar(2) NOT NULL default ' ',
  "AAABIAXA" varchar(3) NOT NULL default ' ',
  "AAABBXWD" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAAAWJBG","AAABIAXA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJBI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABICIR" varchar(2) NOT NULL default '00',
  "AAAAWJCF" varchar(5) NOT NULL default '00000',
  "AAAAWJBN" varchar(4) NOT NULL default '0000',
  "AAAAWJBO" varchar(4) NOT NULL default '0000',
  "AAAAWJBP" varchar(6) NOT NULL default '000000',
  "AAAAWJBQ" varchar(6) NOT NULL default '000000',
  "AAAAWJBR" varchar(6) NOT NULL default '000000',
  "AAABLXEL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABICIR","AAAAWJCF"),
  KEY "AAAAWJBX" ("AAABMRIL","AAABMREX","AAAAWJCF"),
  KEY "AAAAWJCB" ("AAABLXEL","AAABMRIL","AAABMREX","AAAAWJCF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJCG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMIK" varchar(2) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAWJDS" varchar(1) NOT NULL default ' ',
  "AAAAWJDT" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(4) NOT NULL default '0000',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABFHKK" varchar(7) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLMIK","AAABMREX","AAAAWJDS","AAAAWJDT","AAABMPMH","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJCX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAABICIR" varchar(2) NOT NULL default '00',
  "AAAAWJDC" varchar(12) NOT NULL default ' ',
  "AAAAWJDD" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABICIU","AAABICIR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJDI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAWJDS" varchar(1) NOT NULL default ' ',
  "AAAAWJDT" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(4) NOT NULL default '0000',
  "AAABFIGO" varchar(7) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAAWJDS","AAAAWJDT","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJDV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWJEK" varchar(30) NOT NULL default ' ',
  "AAABECDQ" varchar(1) NOT NULL default ' ',
  "AAAAWJDZ" varchar(1) NOT NULL default ' ',
  "AAABKKXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWJEK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJED" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWJEK" varchar(30) NOT NULL default ' ',
  "AAAAWJEH" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWJEK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJEL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABICIR" varchar(2) NOT NULL default '00',
  "AAABGTHF" varchar(4) NOT NULL default ' ',
  "AAAAWJEQ" varchar(4) NOT NULL default ' ',
  "AAAAWJER" varchar(2) NOT NULL default '00',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAAAWJET" varchar(1) NOT NULL default ' ',
  "AAAAWJEU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABICIR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJEY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBVGN" varchar(5) NOT NULL default ' ',
  "AAAAWJFB" decimal(7,3) NOT NULL default '0.000',
  "AAAAWJGH" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABBVGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJFF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBVGN" varchar(5) NOT NULL default ' ',
  "AAABKELB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABBVGN","AAABKELB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJFM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBVGN" varchar(5) NOT NULL default ' ',
  "AAAAWJFW" varchar(4) NOT NULL default '0000',
  "AAAAWJFX" varchar(4) NOT NULL default '0000',
  "AAAAWJFY" varchar(2) NOT NULL default '00',
  "AAAAWJFZ" varchar(2) NOT NULL default '00',
  "AAAAWJFT" decimal(8,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABBVGN","AAAAWJFW","AAAAWJFX","AAAAWJFY","AAAAWJFZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJGA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWJGH" varchar(1) NOT NULL default '0',
  "AAAAWJGE" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWJGH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJGI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABBVGN" varchar(5) NOT NULL default ' ',
  "AAAAWJGM" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABBVGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJGQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAAAWJKS" varchar(8) NOT NULL default ' ',
  "AAABIAWX" varchar(2) NOT NULL default '00',
  "AAAAWJGV" varchar(10) NOT NULL default ' ',
  "AAAAWJGW" varchar(10) NOT NULL default ' ',
  "AAAAWJGX" varchar(10) NOT NULL default ' ',
  "AAAAWJKD" varchar(10) NOT NULL default ' ',
  "AAAAWJGZ" varchar(10) NOT NULL default ' ',
  "AAAAWJKE" varchar(10) NOT NULL default ' ',
  "AAAAWJHB" varchar(10) NOT NULL default ' ',
  "AAAAWJHC" varchar(10) NOT NULL default ' ',
  "AAAAWJHD" varchar(10) NOT NULL default ' ',
  "AAAAWJHE" varchar(10) NOT NULL default ' ',
  "AAAAWJHF" varchar(10) NOT NULL default ' ',
  "AAAAWJHG" varchar(10) NOT NULL default ' ',
  "AAAAWJKF" varchar(10) NOT NULL default ' ',
  "AAAAWJHI" varchar(10) NOT NULL default ' ',
  "AAAAWJHJ" varchar(10) NOT NULL default ' ',
  "AAAAWJHK" varchar(10) NOT NULL default ' ',
  "AAAAWJHL" varchar(10) NOT NULL default ' ',
  "AAAAWJHM" varchar(10) NOT NULL default ' ',
  "AAAAWJHN" varchar(10) NOT NULL default ' ',
  "AAAAWJHO" varchar(10) NOT NULL default ' ',
  "AAAAWJHP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZBM","AAAAWJKS","AAABIAWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJHU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWJKS" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWJKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAAAWJKS" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZBM","AAAAWJKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJIG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAAAWJKS" varchar(8) NOT NULL default ' ',
  "AAABIAWX" varchar(2) NOT NULL default '00',
  "AAAAWJIL" varchar(10) NOT NULL default ' ',
  "AAAAWJIM" varchar(10) NOT NULL default ' ',
  "AAAAWJIN" varchar(10) NOT NULL default ' ',
  "AAAAWJIO" varchar(10) NOT NULL default ' ',
  "AAAAWJIP" varchar(10) NOT NULL default ' ',
  "AAAAWJIQ" varchar(10) NOT NULL default ' ',
  "AAAAWJIR" varchar(10) NOT NULL default ' ',
  "AAAAWJIS" varchar(10) NOT NULL default ' ',
  "AAAAWJIT" varchar(10) NOT NULL default ' ',
  "AAAAWJIU" varchar(10) NOT NULL default ' ',
  "AAAAWJIV" varchar(10) NOT NULL default ' ',
  "AAAAWJIW" varchar(10) NOT NULL default ' ',
  "AAAAWJIX" varchar(10) NOT NULL default ' ',
  "AAAAWJIY" varchar(10) NOT NULL default ' ',
  "AAAAWJIZ" varchar(10) NOT NULL default ' ',
  "AAAAWJJA" varchar(10) NOT NULL default ' ',
  "AAAAWJJB" varchar(10) NOT NULL default ' ',
  "AAAAWJJC" varchar(10) NOT NULL default ' ',
  "AAAAWJJD" varchar(10) NOT NULL default ' ',
  "AAAAWJJE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZBM","AAAAWJKS","AAABIAWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJJJ" (
  "AAAAWJJO" varchar(3) NOT NULL default '000',
  "AAABBXWA" varchar(10) NOT NULL default ' ',
  "AAAAWJKS" varchar(8) NOT NULL default ' ',
  "AAABBPOC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAAWJJO","AAABBXWA","AAAAWJKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJJR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAAAWJKS" varchar(8) NOT NULL default ' ',
  "AAAAWJKJ" varchar(4) NOT NULL default ' ',
  "AAABICIR" varchar(2) NOT NULL default '00',
  "AAAAWJJX" varchar(10) NOT NULL default ' ',
  "AAAAWJJY" varchar(10) NOT NULL default ' ',
  "AAAAWJJZ" varchar(10) NOT NULL default ' ',
  "AAAAWJKA" varchar(10) NOT NULL default ' ',
  "AAAAWJKB" varchar(10) NOT NULL default ' ',
  "AAAAWJKC" varchar(10) NOT NULL default ' ',
  "AAAAWJKD" varchar(10) NOT NULL default ' ',
  "AAAAWJKE" varchar(10) NOT NULL default ' ',
  "AAAAWJKF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZBM","AAAAWJKS","AAAAWJKJ","AAABICIR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJKL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWJKS" varchar(8) NOT NULL default ' ',
  "AAAAWJKP" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWJKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAAAWJKW" varchar(1) NOT NULL default ' ',
  "AAAAWJKX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABICIU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJLA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAABLXIS" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABICIU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJLI" (
  "AAAAWJLN" varchar(3) NOT NULL default '000',
  "AAAAWJLK" decimal(4,3) NOT NULL default '0.000',
  "AAAAWJLL" decimal(4,3) NOT NULL default '0.000',
  "AAAAWJLM" decimal(4,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAAAWJLN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJLO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHXQ" varchar(2) NOT NULL default ' ',
  "AAABIAWX" varchar(2) NOT NULL default '00',
  "AAAAWJLS" varchar(1) NOT NULL default ' ',
  "AAAAWJLT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJLW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYBRU" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYBRU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJMB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLHXQ" varchar(2) NOT NULL default ' ',
  "AAAAWJMF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLHXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJMJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAYBRU" varchar(2) NOT NULL default ' ',
  "AAAAWJMN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAYBRU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJMR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAWJRE" varchar(2) NOT NULL default ' ',
  "AAAAWJOX" varchar(2) NOT NULL default ' ',
  "AAABKNPA" varchar(2) NOT NULL default ' ',
  "AAAAWJMW" varchar(1) NOT NULL default ' ',
  "AAAAWJMX" varchar(1) NOT NULL default ' ',
  "AAAAZCOQ" varchar(1) NOT NULL default ' ',
  "AAAAWJMZ" varchar(1) NOT NULL default ' ',
  "AAABGPDC" varchar(9) NOT NULL default ' ',
  "AAAAWJQN" varchar(2) NOT NULL default ' ',
  "AAAAWJQT" varchar(2) NOT NULL default ' ',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAWJRE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJNG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAWJRE" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAWJNK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAWJRE","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJNO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAWJQN" varchar(2) NOT NULL default ' ',
  "AAAAWJQO" varchar(2) NOT NULL default ' ',
  "AAAAWJNS" varchar(1) NOT NULL default ' ',
  "AAAAWJNT" varchar(1) NOT NULL default ' ',
  "AAABGPDC" varchar(9) NOT NULL default ' ',
  "AAAAWJNV" varchar(2) NOT NULL default ' ',
  "AAAAWJRE" varchar(2) NOT NULL default ' ',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAWJQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJOA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAWJQN" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAWJOE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAWJQN","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJOI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAWJRE" varchar(2) NOT NULL default ' ',
  "AAAAWJOX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAWJRE","AAAAWJOX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJOP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAWJRE" varchar(2) NOT NULL default ' ',
  "AAAAWJOX" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAWJOU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAWJRE","AAAAWJOX","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJOZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAWJPK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAWJPK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJPE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAWJPK" varchar(20) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAWJPI" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAWJPK","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJPM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAWJPX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAWJPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJPR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAWJPX" varchar(40) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAWJPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAWJPX","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJPZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAWJQN" varchar(2) NOT NULL default ' ',
  "AAAAWJQO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAWJQN","AAAAWJQO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJQG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAWJQN" varchar(2) NOT NULL default ' ',
  "AAAAWJQO" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAWJQL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAWJQN","AAAAWJQO","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJQQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAWJQV" varchar(2) NOT NULL default ' ',
  "AAAAWJQT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAWJQV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJQW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAWJRE" varchar(2) NOT NULL default ' ',
  "AAABLLIO" varchar(10) NOT NULL default ' ',
  "AAAAWJRA" varchar(32) NOT NULL default ' ',
  "AAAAWJRB" varchar(32) NOT NULL default ' ',
  "AAAAWJRC" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAWJRE","AAABLLIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJRG" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAABMRCK" varchar(73) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABLTKC","AAABLXDR"),
  KEY "AAAAWJRO" ("AAABLTKC","AAABLXDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJRR" (
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABKMZH" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAAAWJRV" varchar(12) NOT NULL default ' ',
  "AAABDSTC" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABLPWW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJSB" (
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAABMIRY" varchar(1) NOT NULL default ' ',
  "AAABGKUN" varchar(10) NOT NULL default ' ',
  "AAABIYCS" varchar(20) NOT NULL default ' ',
  "AAABLMLR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLTKC","AAABLXDR","AAABMIRX","AAABMIRY","AAABGKUN","AAABIYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJSP" (
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJSS" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABLPWW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABLTKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJSY" (
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAABMLPV" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAAAWJTD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTKC","AAABLXDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJTG" (
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTKC","AAABLXDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJTM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAAAWJTQ" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTKC","AAABLXDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJTU" (
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAABGYUM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTKC","AAABLXDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJUA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWLJG" varchar(4) NOT NULL default ' ',
  "AAABHXNQ" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWLJG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJUG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKAFO" varchar(61) NOT NULL default ' ',
  "AAABLQQS" varchar(3) NOT NULL default ' ',
  "AAAAWJUK" varchar(3) NOT NULL default ' ',
  "AAABAHTV" varchar(15) NOT NULL default ' ',
  "AAAAWJUM" varchar(1) NOT NULL default ' ',
  "AAAAWKKY" varchar(3) NOT NULL default ' ',
  "AAAAYDBY" varchar(1) NOT NULL default ' ',
  "AAAAWJUP" varchar(1) NOT NULL default ' ',
  "AAABGKAS" varchar(10) NOT NULL default ' ',
  "AAABJTYR" varchar(1) NOT NULL default ' ',
  "AAAAWJUS" varchar(1) NOT NULL default ' ',
  "AAAAWJUT" varchar(1) NOT NULL default ' ',
  "AAAAWJUU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJUX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGKAS" varchar(10) NOT NULL default ' ',
  "AAAAWJVB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGKAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJVF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBOOQ" varchar(20) NOT NULL default ' ',
  "AAABLDNL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBOOQ","AAABLDNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJVM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLANS" varchar(1) NOT NULL default ' ',
  "AAABLDNL" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(61) NOT NULL default ' ',
  "AAAAWKAS" varchar(1) NOT NULL default ' ',
  "AAAAWKAT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLANS","AAABLDNL","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJVX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABAHTV" varchar(15) NOT NULL default ' ',
  "AAAAWLJG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJWE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWJWR" varchar(1) NOT NULL default ' ',
  "AAAAWTPR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWJWR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJWK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWJWR" varchar(1) NOT NULL default ' ',
  "AAAAZNRP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWJWR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJWS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMQW" varchar(2) NOT NULL default ' ',
  "AAABIMQG" varchar(2) NOT NULL default ' ',
  "AAABBIBV" varchar(1) NOT NULL default '0',
  "AAABEQVP" varchar(40) NOT NULL default ' ',
  "AAABHEQE" varchar(4) NOT NULL default ' ',
  "AAABAHTV" varchar(15) NOT NULL default ' ',
  "AAAAWJXA" varchar(20) NOT NULL default ' ',
  "AAAAWJXB" varchar(20) NOT NULL default ' ',
  "AAAAWJXC" varchar(1) NOT NULL default '0',
  "AAAAWJXD" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMQW","AAABIMQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJXH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIMQW" varchar(2) NOT NULL default ' ',
  "AAABIMQG" varchar(2) NOT NULL default ' ',
  "AAAAWJXM" varchar(30) NOT NULL default ' ',
  "AAABAERI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABIMQW","AAABIMQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJXS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMQW" varchar(2) NOT NULL default ' ',
  "AAABIMQG" varchar(2) NOT NULL default ' ',
  "AAAAWJYC" varchar(2) NOT NULL default '00',
  "AAAAWJXX" varchar(40) NOT NULL default ' ',
  "AAAAWJXY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMQW","AAABIMQG","AAAAWJYC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJYD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMQW" varchar(2) NOT NULL default ' ',
  "AAABIMQG" varchar(2) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAAAWJYI" varchar(2) NOT NULL default ' ',
  "AAAAWJYJ" varchar(20) NOT NULL default ' ',
  "AAAAWWCO" varchar(30) NOT NULL default ' ',
  "AAAAWJYL" varchar(1) NOT NULL default ' ',
  "AAABGPOF" decimal(2,0) NOT NULL default '0',
  "AAABGPOG" decimal(2,0) NOT NULL default '0',
  "AAABGPOH" decimal(2,0) NOT NULL default '0',
  "AAABGPOI" decimal(2,0) NOT NULL default '0',
  "AAAAWJYQ" varchar(1) NOT NULL default ' ',
  "AAAAWJYR" varchar(1) NOT NULL default ' ',
  "AAABFYDY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMQW","AAABIMQG","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJYX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMQW" varchar(2) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAAAWJZB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMQW","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJZF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWLJG" varchar(4) NOT NULL default ' ',
  "AAABHXNQ" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWLJG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJZL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZDQQ" varchar(2) NOT NULL default ' ',
  "AAABIMPS" varchar(2) NOT NULL default ' ',
  "AAAAWKBQ" varchar(4) NOT NULL default ' ',
  "AAAAWKFL" varchar(1) NOT NULL default ' ',
  "AAABIMQW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZDQQ","AAABIMPS","AAAAWKBQ","AAAAWKFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWJZX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWKAL" varchar(1) NOT NULL default ' ',
  "AAABLDNL" varchar(30) NOT NULL default ' ',
  "AAAAWKAJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWKAL","AAABLDNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWKAL" varchar(1) NOT NULL default ' ',
  "AAABKAFO" varchar(61) NOT NULL default ' ',
  "AAAAWKAJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWKAL","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLANS" varchar(1) NOT NULL default ' ',
  "AAABLDNL" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(61) NOT NULL default ' ',
  "AAAAWKAS" varchar(1) NOT NULL default ' ',
  "AAAAWKAT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLANS","AAABLDNL","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKAY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMQW" varchar(2) NOT NULL default ' ',
  "AAAAWKBB" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABIMQW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKBE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIMQW" varchar(2) NOT NULL default ' ',
  "AAAAWKBI" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABIMQW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKBM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCT" varchar(4) NOT NULL default ' ',
  "AAAAWKBP" varchar(4) NOT NULL default ' ',
  "AAAAWKBQ" varchar(4) NOT NULL default ' ',
  "AAAAWLJG" varchar(4) NOT NULL default ' ',
  "AAABHYJM" varchar(2) NOT NULL default ' ',
  "AAABHYJN" varchar(2) NOT NULL default ' ',
  "AAAAWSOO" varchar(1) NOT NULL default ' ',
  "AAAAWKOJ" varchar(1) NOT NULL default ' ',
  "AAAAWKOK" varchar(1) NOT NULL default ' ',
  "AAABAHTV" varchar(15) NOT NULL default ' ',
  "AAAAWSEN" varchar(4) NOT NULL default ' ',
  "AAABMCVT" varchar(1) NOT NULL default ' ',
  "AAAAWKCA" varchar(1) NOT NULL default ' ',
  "AAAAWKCB" varchar(4) NOT NULL default ' ',
  "AAAAWKCC" varchar(1) NOT NULL default ' ',
  "AAAAWKCD" varchar(1) NOT NULL default ' ',
  "AAABKPQL" varchar(1) NOT NULL default ' ',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAAAWKCG" varchar(1) NOT NULL default ' ',
  "AAAAWKDX" varchar(1) NOT NULL default ' ',
  "AAAAWKCI" varchar(1) NOT NULL default ' ',
  "AAAAWKCJ" varchar(1) NOT NULL default ' ',
  "AAAAWKCK" varchar(1) NOT NULL default ' ',
  "AAAAWKCL" varchar(1) NOT NULL default ' ',
  "AAAAWKCM" varchar(4) NOT NULL default '0000',
  "AAAAWKCN" varchar(1) NOT NULL default ' ',
  "AAABGATM" varchar(2) NOT NULL default ' ',
  "AAAAWKCP" varchar(1) NOT NULL default ' ',
  "AAABHYMV" varchar(3) NOT NULL default ' ',
  "AAABLOZE" varchar(18) NOT NULL default ' ',
  "AAAAWKPY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKCV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABMJXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKDD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVV" varchar(4) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVV","AAABMJXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKDL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXKVX" varchar(1) NOT NULL default ' ',
  "AAABMLCT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXKVX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKDR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVV" varchar(4) NOT NULL default ' ',
  "AAABMLCT" varchar(4) NOT NULL default ' ',
  "AAABBQTG" varchar(1) NOT NULL default ' ',
  "AAABBQTF" varchar(1) NOT NULL default ' ',
  "AAAAWKDX" varchar(1) NOT NULL default ' ',
  "AAAAWKDY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVV","AAABMLCT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKEC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLCT" varchar(4) NOT NULL default ' ',
  "AAAAWKEG" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLCT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKEK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWKFG" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAWKFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKEP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKEU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKEZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWKFG" varchar(2) NOT NULL default '00',
  "AAAAWKFD" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWKFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKFH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMOU" varchar(1) NOT NULL default ' ',
  "AAAAWKFK" varchar(1) NOT NULL default ' ',
  "AAAAWKFL" varchar(1) NOT NULL default ' ',
  "AAAAWLJG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMOU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKFP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIMOU" varchar(1) NOT NULL default ' ',
  "AAAAWKFT" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABIMOU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKFX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYIKV" varchar(2) NOT NULL default ' ',
  "AAAAWKGA" varchar(1) NOT NULL default ' ',
  "AAAAWKGB" varchar(1) NOT NULL default ' ',
  "AAAAWKGC" varchar(1) NOT NULL default ' ',
  "AAAAWKGD" varchar(1) NOT NULL default ' ',
  "AAAAWKGE" varchar(1) NOT NULL default ' ',
  "AAAAWKGF" varchar(1) NOT NULL default ' ',
  "AAAAWKGG" varchar(1) NOT NULL default ' ',
  "AAAAWKGH" varchar(1) NOT NULL default ' ',
  "AAAAWKGI" varchar(1) NOT NULL default ' ',
  "AAAAWKGJ" varchar(1) NOT NULL default ' ',
  "AAAAWKGK" varchar(1) NOT NULL default ' ',
  "AAAAWKGL" varchar(1) NOT NULL default ' ',
  "AAAAWKGM" varchar(1) NOT NULL default ' ',
  "AAAAWKGN" varchar(1) NOT NULL default ' ',
  "AAAAWKGO" varchar(1) NOT NULL default ' ',
  "AAAAWKGP" varchar(1) NOT NULL default ' ',
  "AAAAWKGQ" varchar(1) NOT NULL default ' ',
  "AAABHJJK" varchar(1) NOT NULL default ' ',
  "AAABGKPS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYIKV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKGV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAYIKV" varchar(2) NOT NULL default ' ',
  "AAAAWKGZ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAYIKV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKHD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVDW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKHI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHVDW" varchar(2) NOT NULL default ' ',
  "AAAAWKHM" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHVDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKHQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWKIC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWKIC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKHV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWKIC" varchar(2) NOT NULL default ' ',
  "AAAAWKHZ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWKIC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKID" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWKIP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWKIP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKII" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWKIP" varchar(2) NOT NULL default ' ',
  "AAAAWKIM" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWKIP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKIQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWKKF" varchar(2) NOT NULL default ' ',
  "AAAAWKJU" varchar(20) NOT NULL default ' ',
  "AAAAWKJV" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWKKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKIZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWKKF" varchar(2) NOT NULL default ' ',
  "AAAAWKKG" varchar(2) NOT NULL default '00',
  "AAAAWKJU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWKKF","AAAAWKKG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKJJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWKKL" varchar(2) NOT NULL default ' ',
  "AAAAWKJN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWKKL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKJR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWKKF" varchar(2) NOT NULL default ' ',
  "AAAAWKJU" varchar(20) NOT NULL default ' ',
  "AAAAWKJV" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWKKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKJY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWKKF" varchar(2) NOT NULL default ' ',
  "AAAAWKKG" varchar(2) NOT NULL default '00',
  "AAAAWKKC" varchar(1) NOT NULL default ' ',
  "AAABFTWM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWKKF","AAAAWKKG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKKH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWKKL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWKKL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXSU" varchar(1) NOT NULL default ' ',
  "AAAAWKKP" varchar(1) NOT NULL default ' ',
  "AAAAWKKQ" varchar(1) NOT NULL default ' ',
  "AAAAWKKR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKKU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKAFO" varchar(61) NOT NULL default ' ',
  "AAABLQQS" varchar(3) NOT NULL default ' ',
  "AAAAWKKY" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKLB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWKLJ" varchar(1) NOT NULL default ' ',
  "AAAAWKLE" varchar(4) NOT NULL default ' ',
  "AAAAWLJG" varchar(4) NOT NULL default ' ',
  "AAABFJAY" varchar(1) NOT NULL default ' ',
  "AAABLDNL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWKLJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKLK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXSU" varchar(1) NOT NULL default ' ',
  "AAABCPEA" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKLS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAAAWLJG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKLY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCHR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJCHR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKMD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJCHR" varchar(1) NOT NULL default ' ',
  "AAAAWKMH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJCHR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKML" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVV" varchar(4) NOT NULL default ' ',
  "AAABLLZG" varchar(1) NOT NULL default ' ',
  "AAAAWKNV" varchar(10) NOT NULL default ' ',
  "AAAAWKNW" varchar(1) NOT NULL default ' ',
  "AAAAWKNX" varchar(10) NOT NULL default ' ',
  "AAAAWKNY" varchar(1) NOT NULL default ' ',
  "AAAAWKNZ" varchar(1) NOT NULL default ' ',
  "AAAAWKOA" varchar(10) NOT NULL default ' ',
  "AAAAWKOB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVV","AAABLLZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKMZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVV" varchar(4) NOT NULL default ' ',
  "AAABMCVW" varchar(10) NOT NULL default ' ',
  "AAAAWSEN" varchar(4) NOT NULL default ' ',
  "AAAAWKOJ" varchar(1) NOT NULL default ' ',
  "AAAAWKOK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVV","AAABMCVW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKNJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVV" varchar(4) NOT NULL default ' ',
  "AAABLLZG" varchar(1) NOT NULL default ' ',
  "AAABMCVW" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVV","AAABLLZG","AAABMCVW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKNS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLZG" varchar(1) NOT NULL default ' ',
  "AAAAWKNV" varchar(10) NOT NULL default ' ',
  "AAAAWKNW" varchar(1) NOT NULL default ' ',
  "AAAAWKNX" varchar(10) NOT NULL default ' ',
  "AAAAWKNY" varchar(1) NOT NULL default ' ',
  "AAAAWKNZ" varchar(1) NOT NULL default ' ',
  "AAAAWKOA" varchar(10) NOT NULL default ' ',
  "AAAAWKOB" varchar(1) NOT NULL default ' ',
  "AAAAWKOC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKOF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVW" varchar(10) NOT NULL default ' ',
  "AAAAWSEN" varchar(4) NOT NULL default ' ',
  "AAAAWKOJ" varchar(1) NOT NULL default ' ',
  "AAAAWKOK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKON" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLZG" varchar(1) NOT NULL default ' ',
  "AAABMCVW" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLZG","AAABMCVW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLLZG" varchar(1) NOT NULL default ' ',
  "AAAAWKOY" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLLZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKPC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBPX" varchar(2) NOT NULL default ' ',
  "AAAAWKPF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKPI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMBPX" varchar(2) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMBPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKPQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAAAWKPU" varchar(1) NOT NULL default ' ',
  "AAAAWKPV" varchar(1) NOT NULL default ' ',
  "AAAAWOZU" varchar(1) NOT NULL default ' ',
  "AAAAWKPX" varchar(1) NOT NULL default ' ',
  "AAAAWKPY" varchar(1) NOT NULL default ' ',
  "AAAAWKPZ" varchar(1) NOT NULL default ' ',
  "AAAAWKQA" varchar(1) NOT NULL default ' ',
  "AAABHXKT" varchar(10) NOT NULL default ' ',
  "AAAAWLAS" varchar(1) NOT NULL default ' ',
  "AAAAWKQD" varchar(1) NOT NULL default ' ',
  "AAAAWKQE" varchar(1) NOT NULL default ' ',
  "AAAAWKQF" varchar(1) NOT NULL default ' ',
  "AAAAWKQG" varchar(1) NOT NULL default ' ',
  "AAAAWKQH" varchar(2) NOT NULL default ' ',
  "AAABJCHM" varchar(1) NOT NULL default ' ',
  "AAAAWKQJ" varchar(1) NOT NULL default ' ',
  "AAAAWKQK" varchar(1) NOT NULL default ' ',
  "AAAAWKQL" varchar(1) NOT NULL default ' ',
  "AAABDQWT" varchar(1) NOT NULL default ' ',
  "AAAAWKQN" varchar(1) NOT NULL default ' ',
  "AAABDQWU" varchar(1) NOT NULL default ' ',
  "AAAAWKQP" varchar(1) NOT NULL default ' ',
  "AAABHNDL" varchar(1) NOT NULL default ' ',
  "AAABHKMK" varchar(2) NOT NULL default ' ',
  "AAAAWLKA" varchar(2) NOT NULL default ' ',
  "AAAAWKQT" varchar(2) NOT NULL default ' ',
  "AAAAWKQU" varchar(1) NOT NULL default ' ',
  "AAAAWKQV" varchar(2) NOT NULL default ' ',
  "AAABHOTF" varchar(4) NOT NULL default ' ',
  "AAAAWKQX" varchar(1) NOT NULL default ' ',
  "AAAAWKQY" varchar(1) NOT NULL default ' ',
  "AAAAWKQZ" varchar(1) NOT NULL default ' ',
  "AAAAWKRA" varchar(1) NOT NULL default ' ',
  "AAAAWKVS" varchar(3) NOT NULL default ' ',
  "AAAAWKRC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKRF" (
  "AAAAWKRT" varchar(3) NOT NULL default ' ',
  "AAABBUSX" varchar(2) NOT NULL default '00',
  "AAAAWKRI" varchar(16) NOT NULL default ' ',
  "AAAAWKRJ" varchar(16) NOT NULL default ' ',
  "AAAAWKRK" varchar(16) NOT NULL default ' ',
  "AAAAWORZ" varchar(16) NOT NULL default ' ',
  "AAAAWKRM" varchar(1) NOT NULL default ' ',
  "AAAAWKRN" varchar(1) NOT NULL default ' ',
  "AAAAWKRO" varchar(1) NOT NULL default ' ',
  "AAABIBYA" varchar(1) NOT NULL default ' ',
  "AAAAWKRQ" varchar(1) NOT NULL default ' ',
  "AAAAWKRR" varchar(1) NOT NULL default ' ',
  "AAAAWKRS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAWKRT","AAABBUSX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKRV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABJBVB" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKSD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABLXSU" varchar(1) NOT NULL default ' ',
  "AAABMBFP" varchar(1) NOT NULL default ' ',
  "AAABIBYF" varchar(1) NOT NULL default ' ',
  "AAABLXTE" varchar(1) NOT NULL default ' ',
  "AAAAWKSK" varchar(1) NOT NULL default ' ',
  "AAABHOVQ" varchar(2) NOT NULL default ' ',
  "AAABJBUO" varchar(4) NOT NULL default ' ',
  "AAAAWKTT" varchar(2) NOT NULL default ' ',
  "AAAAWTBE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCFC","AAABLXSU","AAABMBFP","AAABIBYF","AAABLXTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKSV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABBQTF" varchar(1) NOT NULL default ' ',
  "AAABBQTG" varchar(1) NOT NULL default ' ',
  "AAABLXSU" varchar(1) NOT NULL default ' ',
  "AAABMBFP" varchar(1) NOT NULL default ' ',
  "AAABIBYF" varchar(1) NOT NULL default ' ',
  "AAABLXTE" varchar(1) NOT NULL default ' ',
  "AAABMCVX" varchar(4) NOT NULL default ' ',
  "AAAAWKVL" varchar(4) NOT NULL default ' ',
  "AAAAWKVM" varchar(1) NOT NULL default ' ',
  "AAAAWPSL" varchar(3) NOT NULL default '000',
  "AAAAWKUN" varchar(3) NOT NULL default ' ',
  "AAAAWKUO" varchar(3) NOT NULL default '000',
  "AAAAWKUP" varchar(3) NOT NULL default '000',
  "AAAAWKVN" varchar(1) NOT NULL default ' ',
  "AAAAWKUQ" varchar(1) NOT NULL default ' ',
  "AAAAWKUR" varchar(1) NOT NULL default ' ',
  "AAAAWKTO" varchar(2) NOT NULL default ' ',
  "AAAAWKUS" varchar(1) NOT NULL default ' ',
  "AAABKRRT" varchar(2) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAABJBUO" varchar(4) NOT NULL default ' ',
  "AAAAWKTT" varchar(2) NOT NULL default ' ',
  "AAAAWTBE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCFC","AAABBQTF","AAABBQTG","AAABLXSU","AAABMBFP","AAABIBYF","AAABLXTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKUD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABBQTF" varchar(1) NOT NULL default ' ',
  "AAABBQTG" varchar(1) NOT NULL default ' ',
  "AAABLXSU" varchar(1) NOT NULL default ' ',
  "AAABMBFP" varchar(1) NOT NULL default ' ',
  "AAABIBYF" varchar(1) NOT NULL default ' ',
  "AAABLXTE" varchar(1) NOT NULL default ' ',
  "AAAAWPSL" varchar(3) NOT NULL default '000',
  "AAAAWKUN" varchar(3) NOT NULL default ' ',
  "AAAAWKUO" varchar(3) NOT NULL default '000',
  "AAAAWKUP" varchar(3) NOT NULL default '000',
  "AAAAWKUQ" varchar(1) NOT NULL default ' ',
  "AAAAWKUR" varchar(1) NOT NULL default ' ',
  "AAAAWKUS" varchar(1) NOT NULL default ' ',
  "AAABKRRT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCFC","AAABBQTF","AAABBQTG","AAABLXSU","AAABMBFP","AAABIBYF","AAABLXTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKVC" (
  "AAAAWKVS" varchar(3) NOT NULL default ' ',
  "AAABBQTF" varchar(1) NOT NULL default ' ',
  "AAABBQTG" varchar(1) NOT NULL default ' ',
  "AAABLXSU" varchar(1) NOT NULL default ' ',
  "AAABMBFP" varchar(1) NOT NULL default ' ',
  "AAABIBYF" varchar(1) NOT NULL default ' ',
  "AAABLXTE" varchar(1) NOT NULL default ' ',
  "AAABMCVX" varchar(4) NOT NULL default ' ',
  "AAAAWKVL" varchar(4) NOT NULL default ' ',
  "AAAAWKVM" varchar(1) NOT NULL default ' ',
  "AAAAWKVN" varchar(1) NOT NULL default ' ',
  "AAAAWKVO" varchar(2) NOT NULL default ' ',
  "AAAAWKVP" varchar(2) NOT NULL default ' ',
  "AAAAWKVQ" varchar(4) NOT NULL default ' ',
  "AAAAWKVR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAAWKVS","AAABBQTF","AAABBQTG","AAABLXSU","AAABMBFP","AAABIBYF","AAABLXTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKVZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABLXSU" varchar(1) NOT NULL default ' ',
  "AAABMBFP" varchar(1) NOT NULL default ' ',
  "AAABIBYF" varchar(1) NOT NULL default ' ',
  "AAABLXTE" varchar(1) NOT NULL default ' ',
  "AAABJBVB" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCFC","AAABLXSU","AAABMBFP","AAABIBYF","AAABLXTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKWP" (
  "AAABMCVX" varchar(4) NOT NULL default ' ',
  "AAABBUSX" varchar(2) NOT NULL default '00',
  "AAABDJXO" varchar(3) NOT NULL default ' ',
  "AAABGRZB" varchar(16) NOT NULL default ' ',
  "AAAAWKWU" varchar(16) NOT NULL default ' ',
  "AAAAWKWV" varchar(16) NOT NULL default ' ',
  "AAAAWKWW" varchar(1) NOT NULL default ' ',
  "AAAAWKWX" varchar(1) NOT NULL default ' ',
  "AAAAWKWY" varchar(1) NOT NULL default ' ',
  "AAABMCSL" varchar(1) NOT NULL default ' ',
  "AAABMCSK" varchar(1) NOT NULL default ' ',
  "AAAAWKXB" varchar(1) NOT NULL default ' ',
  "AAABMCPL" varchar(1) NOT NULL default ' ',
  "AAAAWKXD" varchar(1) NOT NULL default ' ',
  "AAAAWKXE" varchar(1) NOT NULL default ' ',
  "AAAAWKXF" varchar(1) NOT NULL default ' ',
  "AAAAWKXG" varchar(1) NOT NULL default ' ',
  "AAAAWKXH" varchar(1) NOT NULL default ' ',
  "AAABEEPF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMCVX","AAABBUSX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKXL" (
  "AAABHSSZ" varchar(2) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABHSSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKXP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABBUSX" varchar(2) NOT NULL default '00',
  "AAABKJAF" varchar(12) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABMCEY" varchar(10) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMCVL" varchar(17) NOT NULL default ' ',
  "AAABMCVM" varchar(3) NOT NULL default ' ',
  "AAABMCVN" varchar(17) NOT NULL default ' ',
  "AAABMCVO" varchar(3) NOT NULL default ' ',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABKRZZ" varchar(10) NOT NULL default ' ',
  "AAABLMJL" varchar(4) NOT NULL default ' ',
  "AAABLZBN" varchar(16) NOT NULL default ' ',
  "AAABKSAB" varchar(1) NOT NULL default ' ',
  "AAABHVKX" varchar(10) NOT NULL default ' ',
  "AAABKPSV" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMMVH","AAABBUSX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKYT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAAAWKZD" varchar(1) NOT NULL default ' ',
  "AAAAWKYX" varchar(3) NOT NULL default '000',
  "AAAAWKYY" varchar(3) NOT NULL default '000',
  "AAAAWKYZ" varchar(2) NOT NULL default '00',
  "AAAAWKZA" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAAAWKZD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKZE" (
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTKC","AAABLXDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWKZJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABBUSX" varchar(2) NOT NULL default '00',
  "AAABKJAF" varchar(12) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCEY" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default ' ',
  "AAABLSCM" varchar(10) NOT NULL default ' ',
  "AAAAWKZV" varchar(3) NOT NULL default ' ',
  "AAAAWKZW" varchar(16) NOT NULL default ' ',
  "AAAAWKZX" varchar(8) NOT NULL default '00000000',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  "AAAAWKZZ" varchar(8) NOT NULL default '00000000',
  "AAABLSDV" varchar(1) NOT NULL default ' ',
  "AAABMCVL" varchar(17) NOT NULL default ' ',
  "AAABMCVM" varchar(3) NOT NULL default ' ',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAAAWLAE" varchar(1) NOT NULL default ' ',
  "AAAAWQXF" varchar(3) NOT NULL default ' ',
  "AAABLZMW" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMMVH","AAABBUSX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLAK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLSDV" varchar(1) NOT NULL default ' ',
  "AAAAWLAO" varchar(1) NOT NULL default ' ',
  "AAAAWLAP" varchar(1) NOT NULL default ' ',
  "AAABKQVZ" varchar(1) NOT NULL default ' ',
  "AAABKRNQ" varchar(1) NOT NULL default ' ',
  "AAAAWLAS" varchar(1) NOT NULL default ' ',
  "AAAAWLAT" varchar(3) NOT NULL default '000',
  "AAAAWLAU" varchar(3) NOT NULL default '000',
  "AAAAWLAV" varchar(1) NOT NULL default ' ',
  "AAABJFDW" varchar(2) NOT NULL default ' ',
  "AAABDTOD" varchar(4) NOT NULL default ' ',
  "AAAAWLAY" varchar(1) NOT NULL default ' ',
  "AAAAWLAZ" varchar(1) NOT NULL default ' ',
  "AAAAWLBA" varchar(1) NOT NULL default ' ',
  "AAAAWLBB" varchar(1) NOT NULL default ' ',
  "AAAAWLBC" varchar(1) NOT NULL default ' ',
  "AAAAWLBD" varchar(1) NOT NULL default ' ',
  "AAAAWLBE" varchar(1) NOT NULL default ' ',
  "AAAAWLBF" varchar(1) NOT NULL default ' ',
  "AAAAWLBG" varchar(1) NOT NULL default ' ',
  "AAAAWLBH" varchar(1) NOT NULL default ' ',
  "AAAAWLBI" varchar(1) NOT NULL default ' ',
  "AAAAWLBJ" varchar(1) NOT NULL default ' ',
  "AAAAWLBK" varchar(1) NOT NULL default ' ',
  "AAABLZMW" varchar(4) NOT NULL default '0000',
  "AAAAWLBM" varchar(1) NOT NULL default ' ',
  "AAAAWLBN" varchar(1) NOT NULL default ' ',
  "AAAAWLBO" varchar(1) NOT NULL default ' ',
  "AAAAWLBP" varchar(1) NOT NULL default ' ',
  "AAAAWLBQ" varchar(18) NOT NULL default ' ',
  "AAAAWLBR" varchar(1) NOT NULL default ' ',
  "AAAAWLBS" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLBV" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIZFS" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIZFS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLCB" (
  "AAABIZFS" varchar(4) NOT NULL default ' ',
  "AAAAWLCI" varchar(2) NOT NULL default ' ',
  "AAAAWLCE" varchar(1) NOT NULL default ' ',
  "AAAAWLCF" varchar(3) NOT NULL default ' ',
  "AAABINIL" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABIZFS","AAAAWLCI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLCJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAAAWLCZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABMCEO","AAABMCFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLCT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWLCZ" varchar(4) NOT NULL default ' ',
  "AAABHYJM" varchar(2) NOT NULL default ' ',
  "AAABHYJN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLDA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAABKPRV" varchar(2) NOT NULL default ' ',
  "AAABLBET" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXOB","AAABLURA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLDJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZSLF" varchar(8) NOT NULL default ' ',
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAAAWRTT" varchar(1) NOT NULL default ' ',
  "AAAAWLDP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZSLF","AAABLTKC","AAABLXDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLDU" (
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAAAWRTT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTKC","AAABLXDR","AAAAWRTT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLEB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKPRV" varchar(2) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKPRV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLEH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUHY" varchar(1) NOT NULL default ' ',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  "AAABKPNI" varchar(1) NOT NULL default ' ',
  "AAAAZRWT" varchar(5) NOT NULL default '00000',
  "AAABHYJM" varchar(2) NOT NULL default ' ',
  "AAABHYJN" varchar(2) NOT NULL default ' ',
  "AAAAWLEP" varchar(20) NOT NULL default ' ',
  "AAAAWLEQ" varchar(4) NOT NULL default ' ',
  "AAAAWLER" varchar(1) NOT NULL default ' ',
  "AAABLXSA" varchar(6) NOT NULL default ' ',
  "AAAAWLET" varchar(5) NOT NULL default '00000',
  "AAAAWLEU" varchar(1) NOT NULL default ' ',
  "AAABLKXQ" varchar(4) NOT NULL default ' ',
  "AAAAWLEW" varchar(2) NOT NULL default ' ',
  "AAABMJGC" varchar(1) NOT NULL default ' ',
  "AAAAWLEY" varchar(1) NOT NULL default ' ',
  "AAABKPQZ" varchar(1) NOT NULL default ' ',
  "AAABMOIB" varchar(10) NOT NULL default ' ',
  "AAAAWLFB" varchar(1) NOT NULL default ' ',
  "AAAAWLFC" varchar(1) NOT NULL default ' ',
  "AAAAWLFD" varchar(1) NOT NULL default ' ',
  "AAAAWLFE" varchar(1) NOT NULL default ' ',
  "AAAAWLFF" varchar(1) NOT NULL default ' ',
  "AAAAWLFG" varchar(1) NOT NULL default ' ',
  "AAABMBXF" varchar(10) NOT NULL default ' ',
  "AAAAWLFI" varchar(1) NOT NULL default ' ',
  "AAABFZXA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUHY","AAABLUFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLFN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUHY" varchar(1) NOT NULL default ' ',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLFU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAAAWLZS" varchar(1) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAAAWLGA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMX","AAABMAMY","AAAAWLZS","AAABMQIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLGG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  "AAABLUHY" varchar(1) NOT NULL default ' ',
  "AAABFLFS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUFD","AAABLUHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLGQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUHY" varchar(1) NOT NULL default ' ',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  "AAABLUFN" varchar(4) NOT NULL default ' ',
  "AAABKQYX" varchar(4) NOT NULL default ' ',
  "AAABLZZR" varchar(4) NOT NULL default ' ',
  "AAABKRRT" varchar(2) NOT NULL default ' ',
  "AAAAWLGY" varchar(4) NOT NULL default ' ',
  "AAAAWLGZ" varchar(4) NOT NULL default ' ',
  "AAAAWLHA" varchar(4) NOT NULL default ' ',
  "AAAAWLHB" varchar(1) NOT NULL default ' ',
  "AAAAWLHC" varchar(1) NOT NULL default ' ',
  "AAAAWLHD" varchar(4) NOT NULL default ' ',
  "AAAAWLHE" varchar(4) NOT NULL default ' ',
  "AAAAWLHF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUHY","AAABLUFD","AAABLUFN","AAABKQYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLHL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUFN" varchar(4) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLUHY" varchar(1) NOT NULL default ' ',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  "AAAAWLHR" varchar(1) NOT NULL default ' ',
  "AAAAZRRY" varchar(6) NOT NULL default ' ',
  "AAABHPPT" varchar(1) NOT NULL default ' ',
  "AAAAZROJ" varchar(8) NOT NULL default ' ',
  "AAAAWLHV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUFN","AAABMCVI","AAABLUHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLIA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWLJG" varchar(20) NOT NULL default ' ',
  "AAAAWLIE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWLJG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLII" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWLIQ" varchar(3) NOT NULL default '000',
  "AAAAWLIY" varchar(2) NOT NULL default ' ',
  "AAAAWLIN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWLIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLIR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWLIY" varchar(2) NOT NULL default ' ',
  "AAAAWLIV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWLIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLIZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWLJG" varchar(20) NOT NULL default ' ',
  "AAAAWLJC" varchar(70) NOT NULL default ' ',
  "AAAAWLJD" varchar(70) NOT NULL default ' ',
  "AAAAWLJE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWLJG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLJH" (
  "AAABMOIB" varchar(10) NOT NULL default ' ',
  "AAAAWMDV" varchar(3) NOT NULL default '000',
  "AAAAWMDS" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOIB","AAAAWMDV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUFX" varchar(1) NOT NULL default ' ',
  "AAAAWLJQ" varchar(1) NOT NULL default ' ',
  "AAAAWLJR" varchar(1) NOT NULL default ' ',
  "AAAAWLJS" varchar(1) NOT NULL default ' ',
  "AAABKSPM" varchar(1) NOT NULL default ' ',
  "AAAAWLNE" varchar(1) NOT NULL default ' ',
  "AAABKPOS" varchar(1) NOT NULL default ' ',
  "AAAAWLNG" varchar(1) NOT NULL default ' ',
  "AAABKSPN" varchar(1) NOT NULL default ' ',
  "AAAAWLNI" varchar(1) NOT NULL default ' ',
  "AAABLXSA" varchar(6) NOT NULL default ' ',
  "AAAAWLKA" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUFX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLKD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWLKS" varchar(1) NOT NULL default ' ',
  "AAABMCWE" varchar(1) NOT NULL default ' ',
  "AAAAXCAP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWLKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLKK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWLKS" varchar(1) NOT NULL default ' ',
  "AAAAWLKO" varchar(4) NOT NULL default ' ',
  "AAAAWLKP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWLKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWSWP" varchar(1) NOT NULL default ' ',
  "AAABEAZS" varchar(2) NOT NULL default ' ',
  "AAABLZZR" varchar(4) NOT NULL default ' ',
  "AAAAWLKY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWSWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLLB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEAZS" varchar(2) NOT NULL default ' ',
  "AAAAWLMF" varchar(1) NOT NULL default ' ',
  "AAAAWLMG" varchar(1) NOT NULL default ' ',
  "AAAAWLMH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEAZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLLJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEAZS" varchar(2) NOT NULL default ' ',
  "AAAAWLLN" varchar(20) NOT NULL default ' ',
  "AAAAWLLO" varchar(17) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABEAZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLLS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLURI" varchar(4) NOT NULL default ' ',
  "AAAAWLMO" varchar(1) NOT NULL default '0',
  "AAABEAZS" varchar(2) NOT NULL default ' ',
  "AAAAWLLX" varchar(2) NOT NULL default '00',
  "AAAAWLLY" varchar(1) NOT NULL default ' ',
  "AAAAWLLZ" varchar(2) NOT NULL default '00',
  "AAAAWLMA" varchar(3) NOT NULL default '000',
  "AAAAWLMB" varchar(3) NOT NULL default '000',
  "AAAAWLMC" varchar(3) NOT NULL default '000',
  "AAAAWLMD" varchar(3) NOT NULL default '000',
  "AAAAWLME" varchar(1) NOT NULL default ' ',
  "AAAAWLMF" varchar(1) NOT NULL default ' ',
  "AAAAWLMG" varchar(1) NOT NULL default ' ',
  "AAAAWLMH" varchar(1) NOT NULL default ' ',
  "AAAAWLMI" varchar(1) NOT NULL default ' ',
  "AAAAWLMJ" varchar(1) NOT NULL default ' ',
  "AAAAWLMK" decimal(4,2) NOT NULL default '0.00',
  "AAAAWLML" decimal(4,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLURI","AAAAWLMO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLMP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXTD" varchar(1) NOT NULL default ' ',
  "AAAAWLMT" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLMX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTD" varchar(1) NOT NULL default ' ',
  "AAABLXTE" varchar(1) NOT NULL default ' ',
  "AAAAWLNB" varchar(1) NOT NULL default ' ',
  "AAAAWLNC" varchar(1) NOT NULL default ' ',
  "AAABKSPM" varchar(1) NOT NULL default ' ',
  "AAAAWLNE" varchar(1) NOT NULL default ' ',
  "AAABKPOS" varchar(1) NOT NULL default ' ',
  "AAAAWLNG" varchar(1) NOT NULL default ' ',
  "AAABKSPN" varchar(1) NOT NULL default ' ',
  "AAAAWLNI" varchar(1) NOT NULL default ' ',
  "AAAAWLNJ" varchar(3) NOT NULL default ' ',
  "AAAAWLNK" varchar(1) NOT NULL default ' ',
  "AAABLXSU" varchar(1) NOT NULL default ' ',
  "AAAAWLNM" varchar(1) NOT NULL default ' ',
  "AAABGAGO" varchar(1) NOT NULL default ' ',
  "AAAAWLNO" varchar(1) NOT NULL default ' ',
  "AAABKPOP" varchar(1) NOT NULL default ' ',
  "AAABKPOQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLNT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTD" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABGAGO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTD","AAABMAMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLURI" varchar(4) NOT NULL default ' ',
  "AAAAWLOE" varchar(1) NOT NULL default ' ',
  "AAAAWLOF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLURI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLOI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUHY" varchar(1) NOT NULL default ' ',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABLURI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUHY","AAABLUFD","AAABMCVI","AAABMCEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLURI" varchar(4) NOT NULL default ' ',
  "AAAAWLOY" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLURI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLPC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAAAWLSH" varchar(4) NOT NULL default ' ',
  "AAAAWLPG" varchar(1) NOT NULL default ' ',
  "AAAAWLPH" varchar(1) NOT NULL default ' ',
  "AAAAWLPI" varchar(1) NOT NULL default ' ',
  "AAAAWLPJ" varchar(3) NOT NULL default '000',
  "AAAAWLPK" varchar(3) NOT NULL default '000',
  "AAAAWLPL" varchar(3) NOT NULL default '000',
  "AAAAWLPM" varchar(3) NOT NULL default '000',
  "AAAAWLPN" varchar(3) NOT NULL default '000',
  "AAAAWLPO" varchar(3) NOT NULL default '000',
  "AAAAWLPP" varchar(3) NOT NULL default '000',
  "AAAAWLPQ" varchar(3) NOT NULL default ' ',
  "AAAAWLPR" varchar(3) NOT NULL default '000',
  "AAAAWLPS" varchar(3) NOT NULL default ' ',
  "AAAAWLPT" varchar(3) NOT NULL default '000',
  "AAAAWLPU" varchar(3) NOT NULL default '000',
  "AAAAWLPV" varchar(1) NOT NULL default ' ',
  "AAAAWLPW" varchar(1) NOT NULL default ' ',
  "AAAAWLPX" varchar(1) NOT NULL default ' ',
  "AAAAWLPY" varchar(1) NOT NULL default ' ',
  "AAAAWLPZ" varchar(1) NOT NULL default ' ',
  "AAAAWLQA" varchar(3) NOT NULL default ' ',
  "AAAAWLQB" varchar(1) NOT NULL default ' ',
  "AAAAWLQC" varchar(3) NOT NULL default ' ',
  "AAAAWLQD" varchar(1) NOT NULL default ' ',
  "AAAAWLQE" varchar(1) NOT NULL default ' ',
  "AAAAWLQF" varchar(1) NOT NULL default ' ',
  "AAAAWLQG" varchar(3) NOT NULL default '000',
  "AAAAWLQH" varchar(3) NOT NULL default '000',
  "AAAAWLQI" varchar(3) NOT NULL default '000',
  "AAAAWLQJ" varchar(3) NOT NULL default '000',
  "AAAAWLQK" varchar(3) NOT NULL default '000',
  "AAAAWLQL" varchar(3) NOT NULL default '000',
  "AAAAWLQM" varchar(3) NOT NULL default '000',
  "AAAAWLQN" varchar(3) NOT NULL default '000',
  "AAAAWLQO" varchar(3) NOT NULL default '000',
  "AAAAWLQP" varchar(3) NOT NULL default '000',
  "AAAAWLQQ" varchar(3) NOT NULL default '000',
  "AAAAWLQR" varchar(3) NOT NULL default '000',
  "AAAAWLQS" varchar(3) NOT NULL default '000',
  "AAAAWLQT" varchar(3) NOT NULL default '000',
  "AAAAWLQU" varchar(3) NOT NULL default '000',
  "AAAAWLQV" varchar(3) NOT NULL default '000',
  "AAAAWLQW" varchar(3) NOT NULL default '000',
  "AAAAWLQX" varchar(3) NOT NULL default '000',
  "AAAAWLQY" varchar(1) NOT NULL default ' ',
  "AAAAWLQZ" varchar(1) NOT NULL default ' ',
  "AAAAWLRA" varchar(1) NOT NULL default ' ',
  "AAAAWLRB" varchar(1) NOT NULL default ' ',
  "AAAAWLRC" varchar(1) NOT NULL default ' ',
  "AAAAWLRD" varchar(1) NOT NULL default ' ',
  "AAAAWLRE" varchar(1) NOT NULL default ' ',
  "AAAAWLRF" varchar(1) NOT NULL default ' ',
  "AAAAWLRG" varchar(1) NOT NULL default ' ',
  "AAAAWLRH" varchar(1) NOT NULL default ' ',
  "AAAAWLRI" varchar(1) NOT NULL default ' ',
  "AAAAWLRJ" varchar(1) NOT NULL default ' ',
  "AAAAWLRK" varchar(1) NOT NULL default ' ',
  "AAAAWLRL" varchar(1) NOT NULL default ' ',
  "AAAAWLRM" varchar(1) NOT NULL default ' ',
  "AAAAWLRN" varchar(1) NOT NULL default ' ',
  "AAAAWLRO" varchar(1) NOT NULL default ' ',
  "AAAAWLRP" varchar(1) NOT NULL default ' ',
  "AAAAWLRQ" varchar(3) NOT NULL default '000',
  "AAAAWLRR" varchar(3) NOT NULL default '000',
  "AAAAWLRS" varchar(1) NOT NULL default ' ',
  "AAAAWLRT" varchar(1) NOT NULL default ' ',
  "AAABHWJS" varchar(1) NOT NULL default ' ',
  "AAAAWLRV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAAAWLSH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLRZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAAAWLSH" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWLSE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAAAWLSH","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLSJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWLSX" varchar(1) NOT NULL default ' ',
  "AAABLUFX" varchar(1) NOT NULL default ' ',
  "AAABIMUR" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWLSX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLSS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUFX" varchar(1) NOT NULL default ' ',
  "AAABIMUR" varchar(20) NOT NULL default ' ',
  "AAAAWLSX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUFX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLTB" (
  "AAAAWLTJ" varchar(8) NOT NULL default ' ',
  "AAAAWLTK" varchar(2) NOT NULL default ' ',
  "AAAAWLTL" varchar(3) NOT NULL default ' ',
  "AAAAWLTM" varchar(3) NOT NULL default ' ',
  "AAAAWLTN" varchar(24) NOT NULL default ' ',
  "AAAAWLTO" varchar(18) NOT NULL default ' ',
  "AAAAWLTI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAAWLTJ","AAAAWLTK","AAAAWLTL","AAAAWLTM","AAAAWLTN","AAAAWLTO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLTP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWLUT" varchar(10) NOT NULL default '0000000000',
  "AAAAWLUU" varchar(3) NOT NULL default ' ',
  "AAABHYJM" varchar(2) NOT NULL default ' ',
  "AAABIEWH" varchar(2) NOT NULL default ' ',
  "AAAAWLUX" varchar(1) NOT NULL default ' ',
  "AAAAWLUY" varchar(1) NOT NULL default ' ',
  "AAAAWLUZ" varchar(1) NOT NULL default ' ',
  "AAAAWLVA" varchar(1) NOT NULL default ' ',
  "AAAAWLVB" varchar(1) NOT NULL default ' ',
  "AAAAWLVC" varchar(1) NOT NULL default ' ',
  "AAAAWLVD" varchar(9) NOT NULL default ' ',
  "AAAAWLVE" varchar(1) NOT NULL default ' ',
  "AAAAWLVF" varchar(1) NOT NULL default ' ',
  "AAAAWLVG" varchar(1) NOT NULL default ' ',
  "AAAAWLUF" varchar(1) NOT NULL default ' ',
  "AAAAWLUG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLUI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUHY" varchar(1) NOT NULL default ' ',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUHY","AAABLUFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLUQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAAAWLUT" varchar(10) NOT NULL default '0000000000',
  "AAAAWLUU" varchar(3) NOT NULL default ' ',
  "AAABHYJM" varchar(2) NOT NULL default ' ',
  "AAABIEWH" varchar(2) NOT NULL default ' ',
  "AAAAWLUX" varchar(1) NOT NULL default ' ',
  "AAAAWLUY" varchar(1) NOT NULL default ' ',
  "AAAAWLUZ" varchar(1) NOT NULL default ' ',
  "AAAAWLVA" varchar(1) NOT NULL default ' ',
  "AAAAWLVB" varchar(1) NOT NULL default ' ',
  "AAAAWLVC" varchar(1) NOT NULL default ' ',
  "AAAAWLVD" varchar(9) NOT NULL default ' ',
  "AAAAWLVE" varchar(1) NOT NULL default ' ',
  "AAAAWLVF" varchar(1) NOT NULL default ' ',
  "AAAAWLVG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLURS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLVJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAAAWLXB" varchar(20) NOT NULL default ' ',
  "AAAAWLXC" varchar(20) NOT NULL default ' ',
  "AAAAWLXD" varchar(20) NOT NULL default ' ',
  "AAAAWLXE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLURS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAWLC" varchar(2) NOT NULL default '00',
  "AAAAWLVY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAWLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLWC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAWLC" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABAWLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLWH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSDL" varchar(1) NOT NULL default ' ',
  "AAAAWLWK" varchar(1) NOT NULL default ' ',
  "AAAAWLWL" varchar(1) NOT NULL default ' ',
  "AAAAWLWM" varchar(1) NOT NULL default ' ',
  "AAAAWLWN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLWQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLSDL" varchar(1) NOT NULL default ' ',
  "AAAAWLWU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLSDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLWY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWLXB" varchar(20) NOT NULL default ' ',
  "AAAAWLXC" varchar(20) NOT NULL default ' ',
  "AAAAWLXD" varchar(20) NOT NULL default ' ',
  "AAAAWLXE" varchar(20) NOT NULL default ' ',
  "AAAAWLXF" varchar(20) NOT NULL default ' ',
  "AAAAWLXG" varchar(20) NOT NULL default ' ',
  "AAAAWLXH" varchar(20) NOT NULL default ' ',
  "AAAAWLXI" varchar(20) NOT NULL default ' ',
  "AAAAWLXJ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLXM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWLZS" varchar(1) NOT NULL default ' ',
  "AAABLUHY" varchar(1) NOT NULL default ' ',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  "AAABLZNN" varchar(10) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAAAWLZN" varchar(2) NOT NULL default '00',
  "AAAAWMRU" varchar(70) NOT NULL default ' ',
  "AAAAWLZQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWLZS","AAABLUHY","AAABLUFD","AAABLZNN","AAABLZNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLYC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABGKNI" varchar(1) NOT NULL default ' ',
  "AAABGKNH" varchar(1) NOT NULL default ' ',
  "AAAAWLYI" varchar(1) NOT NULL default ' ',
  "AAAAWLYJ" varchar(1) NOT NULL default ' ',
  "AAAAWLYK" varchar(10) NOT NULL default ' ',
  "AAAAWLYL" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABLUAV","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLYP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWLZS" varchar(1) NOT NULL default ' ',
  "AAABLUHY" varchar(1) NOT NULL default ' ',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  "AAABLZNN" varchar(10) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAAAWLZN" varchar(2) NOT NULL default '00',
  "AAAAWMRU" varchar(70) NOT NULL default ' ',
  "AAAAWLZQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWLZS","AAABLUHY","AAABLUFD","AAABLZNN","AAABLZNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLZF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWLZS" varchar(1) NOT NULL default ' ',
  "AAABLUHY" varchar(1) NOT NULL default ' ',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  "AAABLUFX" varchar(1) NOT NULL default ' ',
  "AAABLZNN" varchar(10) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAAAWLZN" varchar(2) NOT NULL default '00',
  "AAAAWLZO" varchar(1) NOT NULL default '0',
  "AAAAWMRU" varchar(70) NOT NULL default ' ',
  "AAAAWLZQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWLZS","AAABLUHY","AAABLUFD","AAABLUFX","AAABLZNN","AAABLZNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWLZY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWMBD" varchar(10) NOT NULL default ' ',
  "AAAAWMAC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWMBD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMAG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWMBD" varchar(10) NOT NULL default ' ',
  "AAAAWMBE" varchar(8) NOT NULL default ' ',
  "AAAAWMAL" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWMBD","AAAAWMBE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMAQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWMBD" varchar(10) NOT NULL default ' ',
  "AAABFTSA" varchar(65) NOT NULL default ' ',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWMBD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMAX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWMBD" varchar(10) NOT NULL default ' ',
  "AAAAWMBE" varchar(8) NOT NULL default ' ',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWMBD","AAAAWMBE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMBF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLUFX" varchar(1) NOT NULL default ' ',
  "AAABKSPM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLUFX","AAABKSPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMBO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWMCH" varchar(12) NOT NULL default ' ',
  "AAAAWMBR" varchar(1) NOT NULL default ' ',
  "AAAAWMBS" varchar(1) NOT NULL default ' ',
  "AAAAWMBT" varchar(1) NOT NULL default ' ',
  "AAAAWMBU" varchar(4) NOT NULL default ' ',
  "AAAAWMBV" varchar(1) NOT NULL default ' ',
  "AAAAWMBW" varchar(1) NOT NULL default ' ',
  "AAAAWMBX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWMCH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMCA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWMCH" varchar(12) NOT NULL default ' ',
  "AAAAWMCE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWMCH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMCI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABFDNC" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  "AAAAWMCN" varchar(1) NOT NULL default ' ',
  "AAAAWMCO" decimal(13,2) NOT NULL default '0.00',
  "AAAAWMCP" varchar(1) NOT NULL default ' ',
  "AAAAWMCQ" decimal(13,2) NOT NULL default '0.00',
  "AAAAWMCR" varchar(1) NOT NULL default ' ',
  "AAAAWMCS" decimal(4,2) NOT NULL default '0.00',
  "AAAAWMCT" varchar(1) NOT NULL default ' ',
  "AAAAWMCU" decimal(13,2) NOT NULL default '0.00',
  "AAAAWMCV" varchar(1) NOT NULL default ' ',
  "AAAAWMCW" decimal(13,2) NOT NULL default '0.00',
  "AAAAWMCX" varchar(1) NOT NULL default ' ',
  "AAAAWMCY" decimal(4,2) NOT NULL default '0.00',
  "AAAAWMCZ" varchar(1) NOT NULL default ' ',
  "AAAAWMDA" decimal(13,2) NOT NULL default '0.00',
  "AAAAWMDB" varchar(1) NOT NULL default ' ',
  "AAAAWMDC" decimal(4,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABFDNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMDG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAWMDJ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMDM" (
  "AAABMOIB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMDP" (
  "AAABMOIB" varchar(10) NOT NULL default ' ',
  "AAAAWMDV" varchar(3) NOT NULL default '000',
  "AAAAWMDS" varchar(132) NOT NULL default ' ',
  "AAAAWMDT" decimal(3,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMOIB","AAAAWMDV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMDW" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOIB" varchar(10) NOT NULL default ' ',
  "AAABMLPV" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMOIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMEC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJLLP" varchar(4) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAAAWMEP" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJLLP","AAABMPLS","AAAAWMEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMEQ" (
  "AAAAWMFF" varchar(30) NOT NULL default ' ',
  "AAAAWMES" varchar(1) NOT NULL default ' ',
  "AAAAWMET" varchar(1) NOT NULL default ' ',
  "AAAAWMEU" varchar(1) NOT NULL default ' ',
  "AAAAWMEV" varchar(1) NOT NULL default ' ',
  "AAAAWMEW" varchar(1) NOT NULL default ' ',
  "AAAAWMEX" varchar(1) NOT NULL default ' ',
  "AAAAWMEY" varchar(1) NOT NULL default ' ',
  "AAAAWMEZ" varchar(1) NOT NULL default ' ',
  "AAAAWMFA" varchar(1) NOT NULL default ' ',
  "AAAAWMFB" varchar(1) NOT NULL default ' ',
  "AAAAWMFC" varchar(1) NOT NULL default ' ',
  "AAAAWMFD" varchar(1) NOT NULL default ' ',
  "AAAAWMFE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAWMFF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMFG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUHY" varchar(1) NOT NULL default ' ',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default ' ',
  "AAABLGUL" varchar(1) NOT NULL default ' ',
  "AAAAWMFN" varchar(1) NOT NULL default ' ',
  "AAAAWMFO" varchar(1) NOT NULL default ' ',
  "AAAAWMFP" varchar(1) NOT NULL default ' ',
  "AAAAWMFQ" varchar(1) NOT NULL default ' ',
  "AAAAWMFR" varchar(1) NOT NULL default ' ',
  "AAAAWMFS" varchar(1) NOT NULL default ' ',
  "AAAAWMFT" varchar(1) NOT NULL default ' ',
  "AAAAWMFU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUHY","AAABLUFD","AAABLURS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMFZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWMHS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWMHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMGE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAAAWMGO" varchar(30) NOT NULL default ' ',
  "AAABIFHZ" varchar(30) NOT NULL default ' ',
  "AAAAWMGK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUAV","AAABKAFO","AAAAWMGO","AAABIFHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMGQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAAAWMGV" varchar(1) NOT NULL default ' ',
  "AAAAWMGW" varchar(1) NOT NULL default ' ',
  "AAAAWMGX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUFD","AAABLUAV","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMHC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUFD","AAABLUAV","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMHL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWMHS" varchar(4) NOT NULL default ' ',
  "AAAAZKMJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWMHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMHT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWMLF" varchar(1) NOT NULL default ' ',
  "AAAAWMKS" varchar(1) NOT NULL default ' ',
  "AAAAWMKT" varchar(1) NOT NULL default ' ',
  "AAAAWMHY" decimal(4,1) NOT NULL default '0.0',
  PRIMARY KEY ("AAABMRIL","AAAAWMLF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMIB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKPQC" varchar(2) NOT NULL default ' ',
  "AAAAWMNF" varchar(2) NOT NULL default ' ',
  "AAAAWMIF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKPQC","AAAAWMNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMIJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKPQC" varchar(2) NOT NULL default ' ',
  "AAAAWMNF" varchar(2) NOT NULL default ' ',
  "AAAAWMIO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKPQC","AAAAWMNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMIT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWMLF" varchar(1) NOT NULL default ' ',
  "AAAAWMLC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWMLF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMJB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKPQC" varchar(2) NOT NULL default ' ',
  "AAAAWMJE" varchar(1) NOT NULL default ' ',
  "AAAAWMJF" varchar(1) NOT NULL default ' ',
  "AAAAWMJG" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKPQC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMJJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKPQC" varchar(2) NOT NULL default ' ',
  "AAAAWMJN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKPQC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMJR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKPQC" varchar(2) NOT NULL default ' ',
  "AAAAWMMU" varchar(2) NOT NULL default ' ',
  "AAAAWMMK" varchar(1) NOT NULL default ' ',
  "AAAAWMML" varchar(1) NOT NULL default ' ',
  "AAAAWMMM" varchar(1) NOT NULL default ' ',
  "AAAAWMMN" varchar(1) NOT NULL default ' ',
  "AAAAWMMO" varchar(1) NOT NULL default ' ',
  "AAAAWMMP" varchar(1) NOT NULL default ' ',
  "AAAAWMMQ" varchar(1) NOT NULL default ' ',
  "AAAAWMMR" varchar(1) NOT NULL default ' ',
  "AAAAWMKD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKPQC","AAAAWMMU","AAAAWMMK","AAAAWMML","AAAAWMMM","AAAAWMMN","AAAAWMMO","AAAAWMMP","AAAAWMMQ","AAAAWMMR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMKP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWMLF" varchar(1) NOT NULL default ' ',
  "AAAAWMKS" varchar(1) NOT NULL default ' ',
  "AAAAWMKT" varchar(1) NOT NULL default ' ',
  "AAAAWMKU" decimal(4,1) NOT NULL default '0.0',
  "AAAAWMKV" decimal(4,1) NOT NULL default '0.0',
  PRIMARY KEY ("AAABMRIL","AAAAWMLF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMKY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWMLF" varchar(1) NOT NULL default ' ',
  "AAAAWMLC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWMLF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMLG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKPQC" varchar(2) NOT NULL default ' ',
  "AAAAWMMU" varchar(2) NOT NULL default ' ',
  "AAAAWMLK" varchar(2) NOT NULL default ' ',
  "AAAAWMLL" varchar(2) NOT NULL default ' ',
  "AAAAWMLM" varchar(2) NOT NULL default ' ',
  "AAAAWMLN" varchar(2) NOT NULL default ' ',
  "AAAAWMLO" varchar(2) NOT NULL default ' ',
  "AAAAWMLP" varchar(2) NOT NULL default ' ',
  "AAAAWMLQ" varchar(2) NOT NULL default ' ',
  "AAAAWMLR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKPQC","AAAAWMMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMLV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKPQC" varchar(2) NOT NULL default ' ',
  "AAAAWMMU" varchar(2) NOT NULL default ' ',
  "AAAAWMMA" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKPQC","AAAAWMMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMMF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKPQC" varchar(2) NOT NULL default ' ',
  "AAAAWMMU" varchar(2) NOT NULL default ' ',
  "AAAAWMNF" varchar(2) NOT NULL default ' ',
  "AAAAWMMK" varchar(1) NOT NULL default ' ',
  "AAAAWMML" varchar(1) NOT NULL default ' ',
  "AAAAWMMM" varchar(1) NOT NULL default ' ',
  "AAAAWMMN" varchar(1) NOT NULL default ' ',
  "AAAAWMMO" varchar(1) NOT NULL default ' ',
  "AAAAWMMP" varchar(1) NOT NULL default ' ',
  "AAAAWMMQ" varchar(1) NOT NULL default ' ',
  "AAAAWMMR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKPQC","AAAAWMMU","AAAAWMNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMMW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKPQC" varchar(2) NOT NULL default ' ',
  "AAAAWMNF" varchar(2) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKPQC","AAAAWMNF","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMNH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLMFB" varchar(2) NOT NULL default ' ',
  "AAAAWMNL" varchar(4) NOT NULL default ' ',
  "AAAAWMNM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLMFB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMNQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWMQF" varchar(10) NOT NULL default ' ',
  "AAAAWMNT" varchar(4) NOT NULL default '0000',
  "AAAAWMNU" varchar(4) NOT NULL default '0000',
  "AAAAWMNV" varchar(4) NOT NULL default '0000',
  "AAAAWMNW" varchar(4) NOT NULL default '0000',
  "AAAAWMQD" varchar(1) NOT NULL default ' ',
  "AAAAWMNY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWMQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWMQF" varchar(10) NOT NULL default ' ',
  "AAAAWMOF" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWMQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMOJ" (
  "AAAAWMQB" varchar(3) NOT NULL default ' ',
  "AAAAWMQK" varchar(3) NOT NULL default ' ',
  "AAAAWMOM" varchar(1) NOT NULL default ' ',
  "AAABMCMG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAWMQB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMOP" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWMQB" varchar(3) NOT NULL default ' ',
  "AAAAWMOS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAAWMQB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMOV" (
  "AAABLYZH" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLYZH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMOY" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLYZH" varchar(3) NOT NULL default ' ',
  "AAAAWMPB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLYZH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMPE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAAAWMQF" varchar(10) NOT NULL default ' ',
  "AAAAWMPI" varchar(1) NOT NULL default ' ',
  "AAAAWMPJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMPM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWMQF" varchar(10) NOT NULL default ' ',
  "AAABMRCU" varchar(2) NOT NULL default '00',
  "AAABLYZH" varchar(3) NOT NULL default ' ',
  "AAAAWMPR" varchar(1) NOT NULL default ' ',
  "AAAAWMPS" varchar(1) NOT NULL default ' ',
  "AAAAWMPT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWMQF","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMPX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWMQF" varchar(10) NOT NULL default ' ',
  "AAABMRCU" varchar(2) NOT NULL default '00',
  "AAAAWMQB" varchar(3) NOT NULL default ' ',
  "AAAAWMQC" varchar(1) NOT NULL default ' ',
  "AAAAWMQD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWMQF","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMQH" (
  "AAAAWMRD" varchar(1) NOT NULL default ' ',
  "AAABFKUD" varchar(3) NOT NULL default ' ',
  "AAAAWMQK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAAAWMRD","AAABFKUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMQN" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWMRD" varchar(1) NOT NULL default ' ',
  "AAABFKUD" varchar(3) NOT NULL default ' ',
  "AAAAWMQR" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAAWMRD","AAABFKUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMQV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWMRC" varchar(10) NOT NULL default ' ',
  "AAAAWMRD" varchar(1) NOT NULL default ' ',
  "AAABMRCU" varchar(2) NOT NULL default '00',
  "AAABFKUD" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWMRC","AAAAWMRD","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMRF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWMRQ" varchar(10) NOT NULL default ' ',
  "AAAAWMRR" varchar(4) NOT NULL default ' ',
  "AAAAWMRS" varchar(10) NOT NULL default ' ',
  "AAABIJFM" varchar(4) NOT NULL default ' ',
  "AAAAWMRU" varchar(20) NOT NULL default ' ',
  "AAABGKBF" varchar(2) NOT NULL default '00',
  "AAAAWMRN" varchar(2) NOT NULL default ' ',
  "AAAAWMRO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWMRQ","AAAAWMRR","AAAAWMRS","AAABIJFM","AAAAWMRU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMRV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKSRG" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKSRG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMSA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKSRG" varchar(6) NOT NULL default ' ',
  "AAABCVUR" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKSRG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMSI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHXQ" varchar(2) NOT NULL default ' ',
  "AAABHQHH" varchar(2) NOT NULL default ' ',
  "AAABHNBY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMSP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLHXQ" varchar(2) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLHXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMSX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXIU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXIU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMTC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXIU" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXIU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMTK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXRP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXRP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMTP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXRP" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXRP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMTX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXOK" varchar(18) NOT NULL default ' ',
  "AAABKVPX" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLXOK"),
  KEY "AAAAWMUD" ("AAABMRIL","AAABKVPX","AAABLXOK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMUH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXOK" varchar(18) NOT NULL default ' ',
  "AAABMOHX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXOK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMUP" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABMDER" varchar(1) NOT NULL default ' ',
  "AAABMDBL" varchar(1) NOT NULL default ' ',
  "AAABMNLX" varchar(2) NOT NULL default ' ',
  "AAABMNLY" varchar(4) NOT NULL default ' ',
  "AAABMAGA" varchar(1) NOT NULL default ' ',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAAAWMUZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMVB" (
  "AAABLDNL" varchar(30) NOT NULL default ' ',
  "AAABJLIY" varchar(30) NOT NULL default ' ',
  "AAABDJPX" varchar(1) NOT NULL default ' ',
  "AAAAWNBM" varchar(1) NOT NULL default ' ',
  "AAAAWNBP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLDNL","AAABJLIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMVJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWMWM" varchar(3) NOT NULL default ' ',
  "AAABLWIT" varchar(16) NOT NULL default ' ',
  "AAABLWIV" varchar(16) NOT NULL default ' ',
  "AAABLWIX" varchar(16) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWMWM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMVS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWMWM" varchar(3) NOT NULL default ' ',
  "AAAAWMVW" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWMWM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMWA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHDEU" varchar(40) NOT NULL default ' ',
  "AAABMAGA" varchar(1) NOT NULL default ' ',
  "AAAAWNCF" varchar(1) NOT NULL default ' ',
  "AAABLSMW" varchar(3) NOT NULL default ' ',
  "AAAAWMWM" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHDEU","AAABMAGA","AAAAWNCF","AAABLSMW","AAAAWMWM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMWN" (
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  "AAABLELA" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABIGEG","AAABLELA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMWS" (
  "AAABHDEU" varchar(40) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAAAWMWW" varchar(40) NOT NULL default ' ',
  "AAAAWMWX" varchar(30) NOT NULL default ' ',
  "AAAAWMWY" varchar(1) NOT NULL default ' ',
  "AAAAWMWZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHDEU","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMXC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHDEU" varchar(40) NOT NULL default ' ',
  "AAABMAGA" varchar(1) NOT NULL default ' ',
  "AAAAWNCF" varchar(1) NOT NULL default ' ',
  "AAABLSMW" varchar(3) NOT NULL default ' ',
  "AAABMPMH" varchar(4) NOT NULL default ' ',
  "AAABFNFX" varchar(2) NOT NULL default '00',
  "AAABGRZB" varchar(16) NOT NULL default ' ',
  "AAAAWMXL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHDEU","AAABMAGA","AAAAWNCF","AAABLSMW","AAABMPMH","AAABFNFX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMXT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHDEU" varchar(40) NOT NULL default ' ',
  "AAABMAGA" varchar(1) NOT NULL default ' ',
  "AAAAWNCF" varchar(1) NOT NULL default ' ',
  "AAABLSMW" varchar(3) NOT NULL default ' ',
  "AAABLOXN" varchar(1) NOT NULL default '0',
  "AAABBNIW" varchar(2) NOT NULL default '00',
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABELZI" varchar(2) NOT NULL default '00',
  "AAABELZJ" varchar(2) NOT NULL default '00',
  "AAAAWMYF" varchar(1) NOT NULL default ' ',
  "AAABIHTL" varchar(2) NOT NULL default '00',
  "AAABHFFR" varchar(4) NOT NULL default ' ',
  "AAAAWMYI" varchar(30) NOT NULL default ' ',
  "AAAAWMYJ" varchar(30) NOT NULL default ' ',
  "AAABLTTF" varchar(1) NOT NULL default ' ',
  "AAABMQXO" varchar(1) NOT NULL default ' ',
  "AAABLTQJ" tinyint(3) unsigned NOT NULL default '0',
  "AAAAWMYN" varchar(1) NOT NULL default ' ',
  "AAABGCIW" varchar(1) NOT NULL default ' ',
  "AAAAWMYP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHDEU","AAABMAGA","AAAAWNCF","AAABLSMW","AAABLOXN","AAABBNIW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMYX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGIKF" varchar(40) NOT NULL default ' ',
  "AAAAWMZJ" varchar(4) NOT NULL default ' ',
  "AAAAWMZK" varchar(1) NOT NULL default ' ',
  "AAAAWMZC" varchar(4) NOT NULL default '0000',
  "AAAAWMZD" varchar(2) NOT NULL default '00',
  "AAAAWMZE" varchar(2) NOT NULL default '00',
  "AAAAWMZF" varchar(2) NOT NULL default '00',
  "AAAAWMZG" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABGIKF","AAAAWMZJ","AAAAWMZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWMZL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHDEU" varchar(40) NOT NULL default ' ',
  "AAABMAGA" varchar(1) NOT NULL default ' ',
  "AAAAWNCF" varchar(1) NOT NULL default ' ',
  "AAABLSMW" varchar(3) NOT NULL default ' ',
  "AAABDJPX" varchar(1) NOT NULL default ' ',
  "AAAAWNBM" varchar(1) NOT NULL default ' ',
  "AAAAWNBN" varchar(1) NOT NULL default ' ',
  "AAAAWNBO" varchar(1) NOT NULL default ' ',
  "AAAAWNBP" varchar(1) NOT NULL default ' ',
  "AAAAWMZW" varchar(1) NOT NULL default ' ',
  "AAABLCJF" varchar(1) NOT NULL default ' ',
  "AAAAWMZY" varchar(1) NOT NULL default ' ',
  "AAAAWMZZ" varchar(1) NOT NULL default ' ',
  "AAAAWNAA" varchar(1) NOT NULL default ' ',
  "AAAAWNAB" varchar(1) NOT NULL default ' ',
  "AAAAWNAC" varchar(1) NOT NULL default ' ',
  "AAABKUYQ" varchar(1) NOT NULL default ' ',
  "AAAAWNAE" varchar(3) NOT NULL default '000',
  "AAAAWNAF" varchar(8) NOT NULL default ' ',
  "AAABEEXB" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHDEU","AAABMAGA","AAAAWNCF","AAABLSMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNAM" (
  "AAABHDEU" varchar(40) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHDEU","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNAT" (
  "AAABHDEU" varchar(40) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAAAWNAW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABHDEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNAY" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABHDEU" varchar(40) NOT NULL default ' ',
  "AAABMAGA" varchar(1) NOT NULL default ' ',
  "AAAAWNCF" varchar(1) NOT NULL default ' ',
  "AAABBJKF" varchar(8) NOT NULL default ' ',
  "AAABIVAY" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNBG" (
  "AAABHDEU" varchar(40) NOT NULL default ' ',
  "AAABMAGA" varchar(1) NOT NULL default ' ',
  "AAAAWNCF" varchar(1) NOT NULL default ' ',
  "AAAAWNBK" varchar(1) NOT NULL default ' ',
  "AAABDJPX" varchar(1) NOT NULL default ' ',
  "AAAAWNBM" varchar(1) NOT NULL default ' ',
  "AAAAWNBN" varchar(1) NOT NULL default ' ',
  "AAAAWNBO" varchar(1) NOT NULL default ' ',
  "AAAAWNBP" varchar(1) NOT NULL default ' ',
  "AAAAWNBQ" varchar(1) NOT NULL default ' ',
  "AAAAWNBR" varchar(1) NOT NULL default ' ',
  "AAAAWNBS" varchar(1) NOT NULL default ' ',
  "AAAAWNBT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHDEU","AAABMAGA","AAAAWNCF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNBX" (
  "AAABHDEU" varchar(40) NOT NULL default ' ',
  "AAABMAGA" varchar(1) NOT NULL default ' ',
  "AAAAWNCF" varchar(1) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABMDCZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABHDEU","AAABMAGA","AAAAWNCF","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNCH" (
  "AAABHLLX" varchar(4) NOT NULL default ' ',
  "AAABHKSE" varchar(3) NOT NULL default '000',
  "AAABBQOI" varchar(8) NOT NULL default ' ',
  "AAABFNHW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHLLX","AAABHKSE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNCO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDSHO" varchar(40) NOT NULL default ' ',
  "AAABHSKS" varchar(3) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABHLLX" varchar(4) NOT NULL default ' ',
  "AAABHKSE" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABDSHO","AAABHSKS","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNCZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDSHO" varchar(40) NOT NULL default ' ',
  "AAABHSKS" varchar(3) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDSHO","AAABHSKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNDJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDSHO" varchar(40) NOT NULL default ' ',
  "AAAAWNDQ" varchar(12) NOT NULL default ' ',
  "AAABHSKS" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDSHO","AAAAWNDQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNDR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDSHO" varchar(40) NOT NULL default ' ',
  "AAABHSKS" varchar(3) NOT NULL default ' ',
  "AAABMNDR" varchar(16) NOT NULL default ' ',
  "AAABIADW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDSHO","AAABHSKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNEA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZUS" varchar(1) NOT NULL default ' ',
  "AAABLQTM" varchar(1) NOT NULL default ' ',
  "AAABFNEH" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABLZUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNEH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZIB" varchar(4) NOT NULL default ' ',
  "AAABLXOE" varchar(1) NOT NULL default ' ',
  "AAAAWNEL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZIB","AAABLXOE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNEP" (
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABMNLX" varchar(2) NOT NULL default ' ',
  "AAABMNLY" varchar(4) NOT NULL default ' ',
  "AAABMDBN" varchar(4) NOT NULL default ' ',
  "AAABMDBO" varchar(4) NOT NULL default ' ',
  "AAAAWNEW" varchar(2) NOT NULL default ' ',
  "AAAAWNEX" varchar(4) NOT NULL default ' ',
  "AAAAWNEY" varchar(20) NOT NULL default ' ',
  "AAABKVPX" decimal(1,0) NOT NULL default '0',
  "AAAAWSZT" varchar(1) NOT NULL default ' ',
  "AAAAWNFB" varchar(1) NOT NULL default ' ',
  "AAABMDEK" varchar(30) NOT NULL default ' ',
  "AAAAWNFD" varchar(30) NOT NULL default ' ',
  "AAABMDEL" varchar(40) NOT NULL default ' ',
  "AAAAWNFF" varchar(40) NOT NULL default ' ',
  "AAAAWNFG" varchar(1) NOT NULL default ' ',
  "AAAAWNFH" decimal(1,0) NOT NULL default '0',
  "AAAAWNFI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNLV","AAABMNLW","AAABMNLX","AAABMNLY","AAABMDBN","AAABMDBO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNFP" (
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABMNLX" varchar(2) NOT NULL default ' ',
  "AAABMNLY" varchar(4) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABHNRX" varchar(16) NOT NULL default ' ',
  "AAAAWNFV" varchar(16) NOT NULL default ' ',
  "AAAAWNFW" varchar(30) NOT NULL default ' ',
  "AAAAZTMH" varchar(40) NOT NULL default ' ',
  "AAAAWNFY" varchar(1) NOT NULL default ' ',
  "AAAAWNFZ" varchar(20) NOT NULL default ' ',
  "AAAAWNGA" varchar(20) NOT NULL default ' ',
  "AAAAWNGB" varchar(20) NOT NULL default ' ',
  "AAAAWNGC" varchar(1) NOT NULL default ' ',
  "AAAAWNGD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNLV","AAABMNLX","AAABMNLY","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNGI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABGHMU" varchar(1) NOT NULL default ' ',
  "AAAAWNGN" varchar(1) NOT NULL default ' ',
  "AAAAWNGO" varchar(1) NOT NULL default ' ',
  "AAAAWNGP" varchar(1) NOT NULL default ' ',
  "AAAAWNGQ" varchar(1) NOT NULL default ' ',
  "AAAAWNGR" varchar(1) NOT NULL default ' ',
  "AAAAWNGS" varchar(1) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default '0000',
  "AAAAWNGV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABMNLV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNGZ" (
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABMDER" varchar(1) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABMDBL" varchar(1) NOT NULL default ' ',
  "AAAAWNHE" varchar(1) NOT NULL default ' ',
  "AAAAWNHF" varchar(1) NOT NULL default ' ',
  "AAAAWNHG" varchar(1) NOT NULL default ' ',
  "AAAAWNHH" varchar(1) NOT NULL default ' ',
  "AAAAWNHI" varchar(1) NOT NULL default '0',
  "AAAAWNHJ" varchar(1) NOT NULL default ' ',
  "AAABMELA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNLV","AAABMDER","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNHO" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABDWNR" varchar(35) NOT NULL default ' ',
  "AAAAWNHT" varchar(35) NOT NULL default ' ',
  "AAAAWNHU" varchar(35) NOT NULL default ' ',
  "AAAAWNHV" varchar(35) NOT NULL default ' ',
  "AAAAWNHW" varchar(35) NOT NULL default ' ',
  "AAAAWNHX" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMNLV","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNIB" (
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABMNLX" varchar(2) NOT NULL default ' ',
  "AAABMNLY" varchar(4) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default '0000',
  "AAABGPOF" decimal(2,0) NOT NULL default '0',
  "AAABGPOG" decimal(2,0) NOT NULL default '0',
  "AAABGPOH" decimal(2,0) NOT NULL default '0',
  "AAABGPOI" decimal(2,0) NOT NULL default '0',
  "AAAAWNIK" varchar(1) NOT NULL default ' ',
  "AAAAWNIL" varchar(4) NOT NULL default '0000',
  "AAAAWNIM" decimal(2,0) NOT NULL default '0',
  "AAAAWNIN" decimal(2,0) NOT NULL default '0',
  "AAAAWNIO" decimal(2,0) NOT NULL default '0',
  "AAAAWNIP" decimal(2,0) NOT NULL default '0',
  "AAAAWNIQ" varchar(4) NOT NULL default '0000',
  "AAAAWNIR" decimal(2,0) NOT NULL default '0',
  "AAAAWNIS" decimal(2,0) NOT NULL default '0',
  "AAAAWNIT" decimal(2,0) NOT NULL default '0',
  "AAAAWNIU" decimal(2,0) NOT NULL default '0',
  "AAAAWNIV" varchar(5) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMRFO" varchar(20) NOT NULL default ' ',
  "AAABHVJG" varchar(20) NOT NULL default ' ',
  "AAABLQHH" varchar(16) NOT NULL default ' ',
  "AAABLQHI" varchar(16) NOT NULL default ' ',
  "AAABLQHJ" varchar(16) NOT NULL default ' ',
  "AAABLQHK" varchar(16) NOT NULL default ' ',
  "AAAAWNJE" varchar(30) NOT NULL default ' ',
  "AAABJYTM" varchar(40) NOT NULL default ' ',
  "AAAAWNJG" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMNLV","AAABMNLX","AAABMNLY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNJK" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABMNLX" varchar(2) NOT NULL default ' ',
  "AAABMNLY" varchar(4) NOT NULL default ' ',
  "AAAAWNJP" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMNLV","AAABMNLX","AAABMNLY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNJU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWNNK" varchar(4) NOT NULL default ' ',
  "AAAAWNJX" varchar(20) NOT NULL default ' ',
  "AAAAWNJY" varchar(1) NOT NULL default ' ',
  "AAAAWNJZ" varchar(1) NOT NULL default ' ',
  "AAAAWNKA" varchar(1) NOT NULL default ' ',
  "AAAAWNKB" varchar(1) NOT NULL default ' ',
  "AAAAXFFU" varchar(1) NOT NULL default ' ',
  "AAAAWNKD" varchar(1) NOT NULL default ' ',
  "AAABAHTY" varchar(1) NOT NULL default ' ',
  "AAAAWNKF" varchar(1) NOT NULL default ' ',
  "AAABJZUZ" varchar(1) NOT NULL default ' ',
  "AAAAWNKH" varchar(2) NOT NULL default '00',
  "AAAAWNKI" varchar(1) NOT NULL default ' ',
  "AAAAWNKJ" varchar(1) NOT NULL default ' ',
  "AAAAWNKK" varchar(1) NOT NULL default ' ',
  "AAAAWNKL" varchar(1) NOT NULL default ' ',
  "AAAAWNKM" varchar(1) NOT NULL default ' ',
  "AAAAWNKN" varchar(1) NOT NULL default ' ',
  "AAAAWNKO" varchar(4) NOT NULL default ' ',
  "AAAAWNKP" varchar(1) NOT NULL default ' ',
  "AAAAWNKQ" varchar(10) NOT NULL default ' ',
  "AAAAWNKR" varchar(20) NOT NULL default ' ',
  "AAAAWNKS" varchar(1) NOT NULL default ' ',
  "AAAAWNKT" varchar(1) NOT NULL default ' ',
  "AAAAWNKU" varchar(1) NOT NULL default ' ',
  "AAABJCFN" varchar(20) NOT NULL default ' ',
  "AAAAWNKW" varchar(4) NOT NULL default ' ',
  "AAAAWNKX" varchar(4) NOT NULL default ' ',
  "AAAAWNKY" varchar(4) NOT NULL default ' ',
  "AAAAWNKZ" varchar(14) NOT NULL default ' ',
  "AAAAWNLA" varchar(1) NOT NULL default ' ',
  "AAAAWNLB" varchar(1) NOT NULL default ' ',
  "AAAAWNLC" varchar(1) NOT NULL default ' ',
  "AAAAWNLD" varchar(1) NOT NULL default ' ',
  "AAAAWNLE" varchar(1) NOT NULL default ' ',
  "AAAAWNLF" varchar(1) NOT NULL default ' ',
  "AAAAWNLG" varchar(1) NOT NULL default ' ',
  "AAAAWNLH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWNNK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNLK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWNNK" varchar(4) NOT NULL default ' ',
  "AAAAWNLN" int(11) NOT NULL default '0',
  "AAABHTBL" int(11) NOT NULL default '0',
  "AAAAWNLP" int(11) NOT NULL default '0',
  "AAABLRWU" int(11) NOT NULL default '0',
  "AAAAWNLR" varchar(1) NOT NULL default ' ',
  "AAAAWNLS" int(11) NOT NULL default '0',
  "AAAAWNLT" varchar(1) NOT NULL default ' ',
  "AAABCOIG" varchar(20) NOT NULL default ' ',
  "AAAAWNLV" varchar(1) NOT NULL default ' ',
  "AAAAWNLW" varchar(1) NOT NULL default ' ',
  "AAAAWNLX" varchar(1) NOT NULL default ' ',
  "AAAAWNLY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWNNK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNMB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWNNK" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWNNK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNMJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWNNK" varchar(4) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABEGDW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWNNK","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNMR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWNNK" varchar(4) NOT NULL default ' ',
  "AAABMNMP" varchar(4) NOT NULL default '0000',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWNNK","AAABMNMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNMZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWNNK" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWNNK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNNH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYVC" varchar(4) NOT NULL default ' ',
  "AAAAWNNK" varchar(4) NOT NULL default ' ',
  "AAAAWNNL" varchar(1) NOT NULL default ' ',
  "AAAAWNNM" varchar(1) NOT NULL default ' ',
  "AAAAWNNN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNNQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKYVC" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKYVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNNY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKSRE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKSRE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNOD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKSRE" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKSRE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNOL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQAJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQAJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNOQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLQAJ" varchar(2) NOT NULL default ' ',
  "AAABIMUR" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLQAJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNOY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXUQ" varchar(18) NOT NULL default ' ',
  "AAAAWNPI" varchar(8) NOT NULL default '00000000',
  "AAAAWNQC" varchar(2) NOT NULL default '00',
  "AAAAWNQP" varchar(18) NOT NULL default ' ',
  "AAAAWNPE" varchar(1) NOT NULL default ' ',
  "AAAAWNPF" decimal(10,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABLXUQ","AAAAWNPI","AAAAWNQC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNPK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXUQ" varchar(18) NOT NULL default ' ',
  "AAAAWNQC" varchar(2) NOT NULL default '00',
  "AAABFXXG" varchar(30) NOT NULL default ' ',
  "AAAAWNPP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXUQ","AAAAWNQC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNPT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLXUQ" varchar(18) NOT NULL default ' ',
  "AAAAWNQC" varchar(2) NOT NULL default '00',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLXUQ","AAAAWNQC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNQD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWNQP" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWNQP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNQI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAWNQP" varchar(18) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAWNQP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXUQ" varchar(18) NOT NULL default ' ',
  "AAAAWNQT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXUQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNQW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLXUQ" varchar(18) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLXUQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNRE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFXXG" varchar(30) NOT NULL default ' ',
  "AAAAWUQC" int(11) NOT NULL default '0',
  "AAAAWUQD" int(11) NOT NULL default '0',
  "AAAAWNRJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFXXG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNRM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWNUL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWNUL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNRR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWNUT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWNUT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNRW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWNVB" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAWNVB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNSB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWNVJ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWNVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNSG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWNVR" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWNVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNSL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWNVZ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWNVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWNWH" varchar(7) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWNWH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNSV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWNWP" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWNWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNTA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWNWX" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWNWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNTF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWNXF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWNXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNTK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWNXN" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAWNXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNTP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWNXV" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAWNXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNTU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWNYD" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAWNYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNTZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWNYL" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAWNYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNUE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWNUL" varchar(5) NOT NULL default ' ',
  "AAABEECI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWNUL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNUM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWNUT" varchar(2) NOT NULL default ' ',
  "AAABEECI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWNUT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNUU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWNVB" varchar(2) NOT NULL default '00',
  "AAABEECI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWNVB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNVC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWNVJ" varchar(5) NOT NULL default ' ',
  "AAABEECI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWNVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNVK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWNVR" varchar(5) NOT NULL default ' ',
  "AAABEECI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWNVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNVS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWNVZ" varchar(5) NOT NULL default ' ',
  "AAABEECI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWNVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNWA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWNWH" varchar(7) NOT NULL default ' ',
  "AAABEECI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWNWH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNWI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWNWP" varchar(5) NOT NULL default ' ',
  "AAABEECI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWNWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNWQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWNWX" varchar(6) NOT NULL default ' ',
  "AAABEECI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWNWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNWY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWNXF" varchar(4) NOT NULL default ' ',
  "AAABEECI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWNXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNXG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWNXN" varchar(2) NOT NULL default '00',
  "AAABEECI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWNXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNXO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWNXV" varchar(2) NOT NULL default '00',
  "AAABEECI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWNXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNXW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWNYD" varchar(2) NOT NULL default '00',
  "AAABEECI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWNYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNYE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWNYL" varchar(2) NOT NULL default '00',
  "AAABEECI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWNYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNYM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFMPH" varchar(2) NOT NULL default ' ',
  "AAABMCMG" varchar(4) NOT NULL default '0000',
  "AAAAWNYR" varchar(1) NOT NULL default ' ',
  "AAAAWNYS" varchar(1) NOT NULL default ' ',
  "AAABKPVB" varchar(1) NOT NULL default ' ',
  "AAABFRJP" varchar(1) NOT NULL default ' ',
  "AAABHFNF" varchar(3) NOT NULL default ' ',
  "AAABHFNG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABFMPH","AAABMCMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNZB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFMPH" varchar(2) NOT NULL default ' ',
  "AAABMCMG" varchar(4) NOT NULL default '0000',
  "AAABKMGT" varchar(11) NOT NULL default ' ',
  "AAABFRZV" varchar(20) NOT NULL default ' ',
  "AAABIWTP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXDN","AAABFMPH","AAABMCMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWNZP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFMPH" varchar(2) NOT NULL default ' ',
  "AAABIISD" varchar(8) NOT NULL default '00000000',
  "AAABFREP" varchar(8) NOT NULL default '00000000',
  "AAABFREQ" varchar(12) NOT NULL default ' ',
  "AAABLPYC" varchar(8) NOT NULL default '00000000',
  "AAABLPYD" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABFMPH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOAB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFMPH" varchar(2) NOT NULL default ' ',
  "AAABLUDG" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXDN","AAABFMPH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOAL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMKXW" varchar(10) NOT NULL default ' ',
  "AAABIHSD" varchar(18) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABGRZV" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMKXW","AAABIHSD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOAW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABLMLB" varchar(3) NOT NULL default '000',
  "AAAAXKZP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABIHQV","AAABLMLB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOBG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFMLX" varchar(2) NOT NULL default '00',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABFMLX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOBO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFMBR" varchar(1) NOT NULL default ' ',
  "AAABAMRI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABFMBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOBW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMCNO" varchar(5) NOT NULL default ' ',
  "AAABMJVD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMCNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOCE" (
  "AAABMCNO" varchar(5) NOT NULL default ' ',
  "AAABBIKQ" varchar(5) NOT NULL default ' ',
  "AAABBIKG" varchar(20) NOT NULL default ' ',
  "AAABHFNT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMCNO","AAABBIKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOCL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFLRX" varchar(3) NOT NULL default '000',
  "AAABAFUA" varchar(1) NOT NULL default ' ',
  "AAAAWOCP" varchar(5) NOT NULL default ' ',
  "AAAAWOCQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFLRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOCT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKXW" varchar(10) NOT NULL default ' ',
  "AAABIHSD" varchar(18) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABHFNT" varchar(1) NOT NULL default ' ',
  "AAABELTQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKXW","AAABIHSD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWODD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMVH" varchar(8) NOT NULL default ' ',
  "AAABEBTG" varchar(10) NOT NULL default ' ',
  "AAABIHSD" varchar(50) NOT NULL default ' ',
  "AAABCVPG" varchar(10) NOT NULL default ' ',
  "AAABLUVE" varchar(6) NOT NULL default '000000',
  "AAAAZJOL" varchar(10) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABLGYD" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(50) NOT NULL default ' ',
  "AAABMNNA" varchar(50) NOT NULL default ' ',
  "AAABEIIS" varchar(60) NOT NULL default ' ',
  "AAABMKAI" varchar(3) NOT NULL default ' ',
  "AAABACWP" varchar(3) NOT NULL default ' ',
  "AAAAWODS" varchar(5) NOT NULL default ' ',
  "AAABIHTL" varchar(4) NOT NULL default '0000',
  "AAAAWODU" varchar(4) NOT NULL default '0000',
  "AAAAWODV" varchar(3) NOT NULL default '000',
  "AAABIIND" varchar(1) NOT NULL default ' ',
  "AAAAWODX" varchar(1) NOT NULL default ' ',
  "AAAAWODY" varchar(1) NOT NULL default ' ',
  "AAABBVGP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMVH","AAABEBTG","AAABIHSD","AAABCVPG","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOEG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWOEV" varchar(8) NOT NULL default '00000000',
  "AAABBLIL" varchar(4) NOT NULL default ' ',
  "AAAAWOEK" varchar(4) NOT NULL default '0000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  "AAABHFNT" varchar(1) NOT NULL default ' ',
  "AAABHFNU" varchar(46) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWOEV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOEW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABLMLB" varchar(3) NOT NULL default '000',
  "AAAAYADD" varchar(4) NOT NULL default ' ',
  "AAAAWOFB" varchar(30) NOT NULL default ' ',
  "AAAAWOFC" varchar(30) NOT NULL default ' ',
  "AAAAWOFD" varchar(8) NOT NULL default '00000000',
  "AAAAWOFE" varchar(30) NOT NULL default ' ',
  "AAAAWOFF" varchar(30) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAWOFH" varchar(30) NOT NULL default ' ',
  "AAAAWOFI" varchar(30) NOT NULL default ' ',
  "AAAAWOFJ" varchar(5) NOT NULL default ' ',
  "AAAAWOFK" varchar(1) NOT NULL default ' ',
  "AAABFYDZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABLMLB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOFP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFMLX" varchar(2) NOT NULL default '00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFMLX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOFW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFMBR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFMBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOGB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMVH" varchar(8) NOT NULL default ' ',
  "AAABMCNO" varchar(5) NOT NULL default ' ',
  "AAABGKXC" varchar(1) NOT NULL default ' ',
  "AAABLMLB" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMMVH","AAABMCNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNO" varchar(5) NOT NULL default ' ',
  "AAABHYNO" varchar(8) NOT NULL default ' ',
  "AAAAWOGO" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABFWHE" varchar(8) NOT NULL default ' ',
  "AAABBZMT" varchar(20) NOT NULL default ' ',
  "AAAAWOGS" varchar(40) NOT NULL default ' ',
  "AAAAWOGT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOGW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWOHI" varchar(10) NOT NULL default ' ',
  "AAABMLXR" varchar(10) NOT NULL default '0000000000',
  "AAABFSUI" varchar(8) NOT NULL default '00000000',
  "AAABHKSD" varchar(6) NOT NULL default '000000',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABJTAJ" varchar(6) NOT NULL default '000000',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABKUPG" int(11) NOT NULL default '0',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWOHI","AAABMLXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOHK" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAAAWOHN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOHQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOHV" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABLZWV" varchar(1) NOT NULL default ' ',
  "AAAAXHWX" varchar(2) NOT NULL default ' ',
  "AAAAXHWY" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABLZWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOIC" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWOII" varchar(2) NOT NULL default '00',
  "AAAAXGVE" varchar(3) NOT NULL default ' ',
  "AAABKSZO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAAWOII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOIJ" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAWOIO" varchar(1) NOT NULL default ' ',
  "AAAAXHWY" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAAAWOIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOIP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWOJU" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWOJU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOIU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWOJA" varchar(30) NOT NULL default ' ',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAAAWOIY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWOJA","AAABLXDN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOJC" (
  "AAAAWOJM" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFOVQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAAAWOJM","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOJI" (
  "AAAAWOJM" varchar(4) NOT NULL default ' ',
  "AAAAWOJK" varchar(30) NOT NULL default ' ',
  "AAAAWOJL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAWOJM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWOJU" varchar(8) NOT NULL default ' ',
  "AAABEECI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWOJU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOJV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWOLB" varchar(8) NOT NULL default ' ',
  "AAABDPBY" varchar(1) NOT NULL default ' ',
  "AAABGMWN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWOLB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOKC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWOLI" varchar(8) NOT NULL default ' ',
  "AAABMLZG" decimal(2,0) NOT NULL default '0',
  "AAAAWOKG" varchar(1) NOT NULL default ' ',
  "AAABLTXN" varchar(1) NOT NULL default ' ',
  "AAABKQHD" varchar(1) NOT NULL default ' ',
  "AAAAWQHJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWOLI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWOLB" varchar(8) NOT NULL default ' ',
  "AAABMRCU" varchar(2) NOT NULL default ' ',
  "AAAAWOLI" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWOLB","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOKU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWOLB" varchar(8) NOT NULL default ' ',
  "AAAAWOKY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWOLB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOLC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWOLI" varchar(8) NOT NULL default ' ',
  "AAAAWOLJ" varchar(2) NOT NULL default '00',
  "AAABLENC" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAWOLI","AAAAWOLJ","AAABLENC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOLL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMDER" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOLR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABELUC" varchar(8) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABHFNT" varchar(1) NOT NULL default ' ',
  "AAABELTQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABELUC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOLZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABELUC" varchar(8) NOT NULL default ' ',
  "AAABGRZV" varchar(20) NOT NULL default ' ',
  "AAABHIHE" varchar(40) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABELUC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOMJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAZSIQ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOMP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWOMT" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLFK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOMX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABKRSA" varchar(3) NOT NULL default ' ',
  "AAAAWOVK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABKRSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWONF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABKRSA" varchar(3) NOT NULL default ' ',
  "AAAAWONK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLFK","AAABKRSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWONP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABKRSA" varchar(3) NOT NULL default ' ',
  "AAAAWRIV" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABKRSA","AAAAWRIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWONY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABKRSA" varchar(3) NOT NULL default ' ',
  "AAAAWRIV" varchar(3) NOT NULL default ' ',
  "AAAAWOOE" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLFK","AAABKRSA","AAAAWRIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOOK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWRIP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAAAWRIP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOOR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWRIP" varchar(2) NOT NULL default ' ',
  "AAAAZGBW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLFK","AAAAWRIP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOPB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWQQV" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAAAWQQV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOPI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWQQV" varchar(3) NOT NULL default ' ',
  "AAAAWOPN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLFK","AAAAWQQV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOPS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWQVY" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAAAWQVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOPZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWQVY" varchar(3) NOT NULL default ' ',
  "AAAAWOQE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLFK","AAAAWQVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOQJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWPSP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAAAWPSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWPSP" varchar(1) NOT NULL default ' ',
  "AAAAWOQV" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLFK","AAAAWPSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWORA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABAAMP" varchar(3) NOT NULL default ' ',
  "AAAAWORE" decimal(11,3) NOT NULL default '0.000',
  "AAABFZVT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABAAMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWORJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABAAMP" varchar(3) NOT NULL default ' ',
  "AAAAWORO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLFK","AAABAAMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWORT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAXJOZ" varchar(1) NOT NULL default ' ',
  "AAABGRZB" varchar(16) NOT NULL default ' ',
  "AAAAWORY" varchar(1) NOT NULL default ' ',
  "AAAAWORZ" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAAAXJOZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOSD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAXJOZ" varchar(1) NOT NULL default ' ',
  "AAAAWOSI" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLFK","AAAAXJOZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOSN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWRIY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAAAWRIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOSU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWRIY" varchar(2) NOT NULL default ' ',
  "AAAAWOSZ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLFK","AAAAWRIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOTE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABKRSA" varchar(3) NOT NULL default ' ',
  "AAAAWRJA" varchar(3) NOT NULL default ' ',
  "AAABGCTJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABKRSA","AAAAWRJA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOTO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABKRSA" varchar(3) NOT NULL default ' ',
  "AAAAWRJA" varchar(3) NOT NULL default ' ',
  "AAAAWOTU" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLFK","AAABKRSA","AAAAWRJA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOUA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABLGXS" varchar(3) NOT NULL default ' ',
  "AAABGZKU" varchar(10) NOT NULL default ' ',
  "AAAAWOVG" varchar(1) NOT NULL default ' ',
  "AAAAWOVH" varchar(1) NOT NULL default ' ',
  "AAAAWOVI" varchar(1) NOT NULL default ' ',
  "AAAAWOVJ" varchar(1) NOT NULL default ' ',
  "AAAAWOVK" varchar(3) NOT NULL default ' ',
  "AAAAWOVL" varchar(2) NOT NULL default ' ',
  "AAABJSBP" varchar(1) NOT NULL default ' ',
  "AAABHZTR" varchar(2) NOT NULL default ' ',
  "AAAAWOVO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABLGXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOUR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABLGXS" varchar(3) NOT NULL default ' ',
  "AAAAWOUW" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLFK","AAABLGXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOVB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABGZKU" varchar(10) NOT NULL default ' ',
  "AAABLGXS" varchar(3) NOT NULL default ' ',
  "AAAAWOVG" varchar(1) NOT NULL default ' ',
  "AAAAWOVH" varchar(1) NOT NULL default ' ',
  "AAAAWOVI" varchar(1) NOT NULL default ' ',
  "AAAAWOVJ" varchar(1) NOT NULL default ' ',
  "AAAAWOVK" varchar(3) NOT NULL default ' ',
  "AAAAWOVL" varchar(2) NOT NULL default ' ',
  "AAABJSBP" varchar(1) NOT NULL default ' ',
  "AAABHZTR" varchar(2) NOT NULL default ' ',
  "AAAAWOVO" varchar(10) NOT NULL default ' ',
  "AAABLXWU" varchar(10) NOT NULL default ' ',
  "AAAAWQJB" decimal(10,3) NOT NULL default '0.000',
  "AAAAWQJC" decimal(10,3) NOT NULL default '0.000',
  "AAAAWQJD" decimal(10,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABGZKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOVW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABGZKU" varchar(10) NOT NULL default ' ',
  "AAAAWOWB" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLFK","AAABGZKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOWG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAAAWQWH" varchar(3) NOT NULL default '000',
  "AAABMCEY" varchar(10) NOT NULL default ' ',
  "AAABLXSU" varchar(1) NOT NULL default ' ',
  "AAABKMUX" varchar(16) NOT NULL default ' ',
  "AAABKRSA" varchar(3) NOT NULL default ' ',
  "AAABKRSB" varchar(10) NOT NULL default ' ',
  "AAAAXJOZ" varchar(1) NOT NULL default ' ',
  "AAAAWOWT" varchar(10) NOT NULL default ' ',
  "AAAAWOWU" decimal(13,3) NOT NULL default '0.000',
  "AAAAWOWV" decimal(13,3) NOT NULL default '0.000',
  "AAABJSHZ" varchar(3) NOT NULL default ' ',
  "AAABLLXX" decimal(5,0) NOT NULL default '0',
  "AAABLLXY" decimal(5,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABMLCZ","AAABMCVI","AAABMCEO","AAAAWQWH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOXF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABLQCR" varchar(10) NOT NULL default ' ',
  "AAAAWOXJ" varchar(40) NOT NULL default ' ',
  "AAAAWQFU" varchar(1) NOT NULL default ' ',
  "AAAAWOXL" varchar(1) NOT NULL default ' ',
  "AAABLWBE" varchar(1) NOT NULL default ' ',
  "AAAAWOZU" varchar(1) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAAAZTFM" varchar(2) NOT NULL default ' ',
  "AAABGCTJ" varchar(4) NOT NULL default ' ',
  "AAAAWOXR" varchar(8) NOT NULL default '00000000',
  "AAAAWOZX" varchar(1) NOT NULL default ' ',
  "AAAAWOZY" varchar(1) NOT NULL default ' ',
  "AAABMLGG" varchar(10) NOT NULL default ' ',
  "AAABMLFI" varchar(6) NOT NULL default ' ',
  "AAABMLFJ" varchar(4) NOT NULL default ' ',
  "AAAAWQMP" varchar(1) NOT NULL default ' ',
  "AAAAWPXW" varchar(1) NOT NULL default ' ',
  "AAAAWPXX" varchar(1) NOT NULL default ' ',
  "AAABKQWH" varchar(1) NOT NULL default ' ',
  "AAAAWOYB" varchar(1) NOT NULL default ' ',
  "AAAAWOYC" decimal(15,0) NOT NULL default '0',
  "AAAAWOYD" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABLQCR"),
  KEY "AAAAWOYH" ("AAABMRIL","AAABMRFQ"),
  KEY "AAAAWOYK" ("AAABMRIL","AAABMLGG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOYN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABLQCR" varchar(10) NOT NULL default ' ',
  "AAAAWOZL" varchar(10) NOT NULL default ' ',
  "AAAAWQFU" varchar(1) NOT NULL default ' ',
  "AAABMCNJ" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLWJT" varchar(72) NOT NULL default ' ',
  "AAABLUGE" varchar(1) NOT NULL default ' ',
  "AAABMLFX" varchar(4) NOT NULL default ' ',
  "AAAAWOYY" varchar(1) NOT NULL default ' ',
  "AAAAWOYZ" varchar(1) NOT NULL default ' ',
  "AAAAWOZA" varchar(1) NOT NULL default ' ',
  "AAAAWOZB" varchar(1) NOT NULL default ' ',
  "AAABKRZZ" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABLQCR","AAAAWOZL"),
  KEY "AAAAWOZH" ("AAABMRIL","AAABMLFK","AAAAWQFU","AAAAWOZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWOZM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABLQCR" varchar(10) NOT NULL default ' ',
  "AAAAWQWH" varchar(3) NOT NULL default '000',
  "AAAAWOZR" varchar(1) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAAAWOZU" varchar(1) NOT NULL default ' ',
  "AAAAZTFM" varchar(2) NOT NULL default ' ',
  "AAABGCTJ" varchar(4) NOT NULL default ' ',
  "AAAAWOZX" varchar(1) NOT NULL default ' ',
  "AAAAWOZY" varchar(1) NOT NULL default ' ',
  "AAAAWOZZ" varchar(1) NOT NULL default ' ',
  "AAAAWQXS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABLQCR","AAAAWQWH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWROO" varchar(3) NOT NULL default '000',
  "AAABLHLV" varchar(3) NOT NULL default ' ',
  "AAAAZEVB" varchar(3) NOT NULL default ' ',
  "AAAAWPBD" varchar(6) NOT NULL default ' ',
  "AAAAWPBQ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAAAWROO","AAABLHLV","AAAAZEVB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPAS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWPBD" varchar(6) NOT NULL default ' ',
  "AAAAWPAW" varchar(1) NOT NULL default ' ',
  "AAAAWPAX" varchar(1) NOT NULL default ' ',
  "AAAAWPAY" varchar(1) NOT NULL default ' ',
  "AAAAXQZD" varchar(1) NOT NULL default ' ',
  "AAAAWPBA" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAAAWPBD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPBE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWPBQ" varchar(6) NOT NULL default ' ',
  "AAAAWPBI" varchar(1) NOT NULL default ' ',
  "AAAAWPBJ" decimal(13,3) NOT NULL default '0.000',
  "AAAAWPBK" decimal(11,3) NOT NULL default '0.000',
  "AAAAWPBL" decimal(13,3) NOT NULL default '0.000',
  "AAAAWPEG" varchar(2) NOT NULL default '00',
  "AAAAWRGR" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAAAWPBQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPBR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWROO" varchar(3) NOT NULL default '000',
  "AAABLHLV" varchar(3) NOT NULL default ' ',
  "AAAAZEVB" varchar(3) NOT NULL default ' ',
  "AAAAWPBX" decimal(13,3) NOT NULL default '0.000',
  "AAAAWRGR" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAAAWROO","AAABLHLV","AAAAZEVB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPCE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWPEE" varchar(30) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWPEG" varchar(2) NOT NULL default '00',
  "AAAAWPCJ" varchar(30) NOT NULL default ' ',
  "AAAAWPCK" varchar(30) NOT NULL default ' ',
  "AAAAWPCL" varchar(30) NOT NULL default ' ',
  "AAAAWPCM" varchar(30) NOT NULL default ' ',
  "AAAAWPCN" varchar(30) NOT NULL default ' ',
  "AAAAWPCO" varchar(30) NOT NULL default ' ',
  "AAAAWPCP" varchar(30) NOT NULL default ' ',
  "AAAAWPCQ" varchar(30) NOT NULL default ' ',
  "AAAAWPCR" varchar(30) NOT NULL default ' ',
  "AAAAWPCS" varchar(30) NOT NULL default ' ',
  "AAAAWPCT" varchar(30) NOT NULL default ' ',
  "AAAAWPCU" varchar(30) NOT NULL default ' ',
  "AAAAWPCV" varchar(30) NOT NULL default ' ',
  "AAAAWPCW" varchar(30) NOT NULL default ' ',
  "AAAAWPCX" varchar(30) NOT NULL default ' ',
  "AAAAWPCY" varchar(1) NOT NULL default ' ',
  "AAAAWPCZ" varchar(1) NOT NULL default ' ',
  "AAAAWPDA" varchar(1) NOT NULL default ' ',
  "AAAAWPDB" varchar(1) NOT NULL default ' ',
  "AAAAWPDC" varchar(1) NOT NULL default ' ',
  "AAAAWPDD" varchar(1) NOT NULL default ' ',
  "AAAAWPDE" varchar(1) NOT NULL default ' ',
  "AAAAWPDF" varchar(1) NOT NULL default ' ',
  "AAAAWPDG" varchar(1) NOT NULL default ' ',
  "AAAAWPDH" varchar(1) NOT NULL default ' ',
  "AAAAWPDI" varchar(1) NOT NULL default ' ',
  "AAAAWPDJ" varchar(1) NOT NULL default ' ',
  "AAAAWPDK" varchar(1) NOT NULL default ' ',
  "AAAAWPDL" varchar(1) NOT NULL default ' ',
  "AAAAWPDM" varchar(1) NOT NULL default ' ',
  "AAAAWPDN" varchar(1) NOT NULL default ' ',
  "AAAAWPDO" varchar(1) NOT NULL default ' ',
  "AAAAWPDP" varchar(1) NOT NULL default ' ',
  "AAAAWPDQ" varchar(1) NOT NULL default ' ',
  "AAAAWPDR" varchar(1) NOT NULL default ' ',
  "AAAAWPDS" varchar(1) NOT NULL default ' ',
  "AAAAWPDT" varchar(1) NOT NULL default ' ',
  "AAAAWPDU" varchar(1) NOT NULL default ' ',
  "AAAAWPDV" varchar(1) NOT NULL default ' ',
  "AAAAWPDW" varchar(1) NOT NULL default ' ',
  "AAAAWPDX" varchar(1) NOT NULL default ' ',
  "AAAAWPDY" varchar(1) NOT NULL default ' ',
  "AAAAWPDZ" varchar(1) NOT NULL default ' ',
  "AAAAWPEA" varchar(1) NOT NULL default ' ',
  "AAAAWPEB" varchar(1) NOT NULL default ' ',
  "AAAAWPEC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWPEE","AAABMLFK","AAAAWPEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPEH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABLHLV" varchar(3) NOT NULL default ' ',
  "AAAAZEVB" varchar(3) NOT NULL default ' ',
  "AAAAWPEM" decimal(13,3) NOT NULL default '0.000',
  "AAAAWRGR" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABLHLV","AAAAZEVB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPES" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWPFP" varchar(20) NOT NULL default ' ',
  "AAABLMLR" varchar(2) NOT NULL default '00',
  "AAAAWPEX" varchar(1) NOT NULL default ' ',
  "AAAAWPEY" varchar(20) NOT NULL default ' ',
  "AAAAWPEZ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAAAWPFP","AAABLMLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPFE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWPFP" varchar(20) NOT NULL default ' ',
  "AAABLMLR" varchar(2) NOT NULL default '00',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  "AAABLPWW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLFK","AAAAWPFP","AAABLMLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPFR" (
  "AAAAWRIF" varchar(40) NOT NULL default ' ',
  "AAAAWPFY" varchar(10) NOT NULL default ' ',
  "AAAAWPFZ" varchar(2) NOT NULL default '00',
  "AAAAWPGO" varchar(4) NOT NULL default ' ',
  "AAAAWPGY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAAWRIF","AAAAWPFY","AAAAWPFZ","AAAAWPGO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPGB" (
  "AAAAWRIF" varchar(40) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWPGO" varchar(4) NOT NULL default ' ',
  "AAAAWPGF" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAAAWRIF","AAABMRHS","AAAAWPGO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPGJ" (
  "AAAAWRIF" varchar(40) NOT NULL default ' ',
  "AAAAWPGO" varchar(4) NOT NULL default ' ',
  "AAAAWPGM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAAWRIF","AAAAWPGO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPGP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWRIF" varchar(40) NOT NULL default ' ',
  "AAAAWPGY" varchar(4) NOT NULL default ' ',
  "AAAAWPHM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAAAWRIF","AAAAWPGY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPGZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWPHM" varchar(10) NOT NULL default ' ',
  "AAAAWPHN" varchar(1) NOT NULL default ' ',
  "AAAAWPHO" varchar(1) NOT NULL default ' ',
  "AAAAWPHF" varchar(70) NOT NULL default ' ',
  "AAAAWPHG" varchar(70) NOT NULL default ' ',
  "AAAAWPHH" varchar(70) NOT NULL default ' ',
  "AAAAWPHI" varchar(70) NOT NULL default ' ',
  "AAAAWPHJ" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAAAWPHM","AAAAWPHN","AAAAWPHO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPHP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABLHLV" varchar(3) NOT NULL default ' ',
  "AAAAZEVB" varchar(3) NOT NULL default ' ',
  "AAAAWROO" varchar(3) NOT NULL default '000',
  "AAABHDCP" varchar(6) NOT NULL default ' ',
  "AAABABNP" varchar(1) NOT NULL default ' ',
  "AAAAWPHX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABLHLV","AAAAZEVB","AAAAWROO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPID" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABHDCP" varchar(6) NOT NULL default ' ',
  "AAAAWPIH" varchar(1) NOT NULL default ' ',
  "AAAAWPII" varchar(1) NOT NULL default ' ',
  "AAAAWPIJ" varchar(1) NOT NULL default ' ',
  "AAAAWPIK" varchar(1) NOT NULL default ' ',
  "AAAAWPIL" varchar(1) NOT NULL default ' ',
  "AAAAWPIM" varchar(1) NOT NULL default ' ',
  "AAAAWPIN" varchar(1) NOT NULL default '0',
  "AAAAWPIO" varchar(1) NOT NULL default '0',
  "AAAAWPIP" varchar(1) NOT NULL default '0',
  "AAAAWPIQ" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABHDCP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPIU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWPIX" varchar(10) NOT NULL default ' ',
  "AAAAYOVL" varchar(10) NOT NULL default ' ',
  "AAAAWPIZ" varchar(10) NOT NULL default ' ',
  "AAAAWPJA" varchar(10) NOT NULL default ' ',
  "AAAAWPJB" varchar(10) NOT NULL default ' ',
  "AAAAWPJC" varchar(10) NOT NULL default ' ',
  "AAAAWPJD" varchar(10) NOT NULL default ' ',
  "AAAAWPJE" varchar(10) NOT NULL default ' ',
  "AAAAWPJF" varchar(10) NOT NULL default ' ',
  "AAAAWPJG" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPJJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWPNO" varchar(10) NOT NULL default ' ',
  "AAAAWPME" varchar(4) NOT NULL default ' ',
  "AAAAWPMF" varchar(1) NOT NULL default '0',
  "AAAAWPJO" varchar(1) NOT NULL default ' ',
  "AAAAWPJP" varchar(1) NOT NULL default ' ',
  "AAAAWPJQ" varchar(2) NOT NULL default '00',
  "AAAAWPNM" varchar(1) NOT NULL default ' ',
  "AAAAWPJS" varchar(1) NOT NULL default ' ',
  "AAAAWPJT" varchar(1) NOT NULL default '0',
  "AAAAWPJU" varchar(1) NOT NULL default '0',
  "AAAAWPJV" varchar(1) NOT NULL default '0',
  "AAAAWPJW" varchar(1) NOT NULL default '0',
  "AAAAWPJX" varchar(1) NOT NULL default ' ',
  "AAAAWPJY" varchar(3) NOT NULL default ' ',
  "AAAAWPJZ" varchar(3) NOT NULL default '000',
  "AAABGYLC" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAWPNO","AAAAWPME","AAAAWPMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPKF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWPNO" varchar(10) NOT NULL default ' ',
  "AAAAWPME" varchar(4) NOT NULL default ' ',
  "AAAAWPMF" varchar(1) NOT NULL default '0',
  "AAAAWPKL" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWPNO","AAAAWPME","AAAAWPMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPKR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABLQQC" varchar(10) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default '000',
  "AAABKRSA" varchar(3) NOT NULL default ' ',
  "AAAAWRJA" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABLQQC","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPLC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWPNO" varchar(10) NOT NULL default ' ',
  "AAAAZCSQ" varchar(1) NOT NULL default '0',
  "AAAAWRCH" varchar(1) NOT NULL default '0',
  "AAABLRQQ" varchar(10) NOT NULL default ' ',
  "AAAAWPNM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWPNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPLL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(6) NOT NULL default ' ',
  "AAAAWPNO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPLR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUVS" varchar(6) NOT NULL default ' ',
  "AAABMLPV" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPLZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(6) NOT NULL default ' ',
  "AAAAWPMI" varchar(2) NOT NULL default '00',
  "AAAAWPMJ" varchar(2) NOT NULL default '00',
  "AAAAWPME" varchar(4) NOT NULL default ' ',
  "AAAAWPMF" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAAAWPMI","AAAAWPMJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPMK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABJSAU" varchar(2) NOT NULL default ' ',
  "AAAAWPNP" varchar(8) NOT NULL default ' ',
  "AAABGCVE" varchar(16) NOT NULL default ' ',
  "AAAAWPMQ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQIJ","AAABJSAU","AAAAWPNP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPMV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJPDN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJPDN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPNA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJPDN" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAWPNE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJPDN","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPNI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWPNO" varchar(10) NOT NULL default ' ',
  "AAAAWPNP" varchar(8) NOT NULL default ' ',
  "AAAAWPNM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWPNO","AAAAWPNP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPNQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLFLE" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABMRCU" varchar(1) NOT NULL default '0',
  "AAAAWPOI" varchar(13) NOT NULL default ' ',
  "AAABLQRI" varchar(10) NOT NULL default ' ',
  "AAAAWPOK" varchar(2) NOT NULL default ' ',
  "AAAAWPOL" varchar(2) NOT NULL default ' ',
  "AAAAWPOM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLFLE","AAABMCEO","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPOE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABMRCU" varchar(1) NOT NULL default '0',
  "AAAAWPOI" varchar(13) NOT NULL default ' ',
  "AAABLQRI" varchar(10) NOT NULL default ' ',
  "AAAAWPOK" varchar(2) NOT NULL default ' ',
  "AAAAWPOL" varchar(2) NOT NULL default ' ',
  "AAAAWPOM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPOQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABMLFI" varchar(6) NOT NULL default ' ',
  "AAAAWPOU" varchar(1) NOT NULL default ' ',
  "AAAAWPOV" varchar(1) NOT NULL default ' ',
  "AAAAWPOW" varchar(1) NOT NULL default ' ',
  "AAAAWPOX" int(11) NOT NULL default '0',
  "AAAAWPOY" decimal(15,0) NOT NULL default '0',
  "AAAAWPOZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAWPPA" decimal(15,3) NOT NULL default '0.000',
  "AAAAWPPB" decimal(15,3) NOT NULL default '0.000',
  "AAAAWPPC" decimal(15,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABMLFI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPPG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABMLFI" varchar(6) NOT NULL default ' ',
  "AAABHYIY" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLFK","AAABMLFI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPPQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABLHLV" varchar(3) NOT NULL default ' ',
  "AAAAZEVB" varchar(3) NOT NULL default ' ',
  "AAAAWROO" varchar(3) NOT NULL default '000',
  "AAABLBIH" varchar(18) NOT NULL default ' ',
  "AAAAWPPX" varchar(1) NOT NULL default ' ',
  "AAABFTWM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABLHLV","AAAAZEVB","AAAAWROO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPQE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABJUPW" varchar(4) NOT NULL default ' ',
  "AAABAAMP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABJUPW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPQM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWPQR" varchar(1) NOT NULL default ' ',
  "AAAAWPQS" varchar(1) NOT NULL default ' ',
  "AAAAWPQT" varchar(1) NOT NULL default ' ',
  "AAABLCGT" varchar(3) NOT NULL default ' ',
  "AAAAWQWE" varchar(2) NOT NULL default ' ',
  "AAAAWPQW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABMCEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPRA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABKRSA" varchar(3) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABDWBA" varchar(1) NOT NULL default ' ',
  "AAAAXITD" varchar(2) NOT NULL default ' ',
  "AAABMCGA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABKRSA","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPRO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWPSL" varchar(3) NOT NULL default '000',
  "AAAAWPSM" varchar(1) NOT NULL default ' ',
  "AAABMBFP" varchar(1) NOT NULL default ' ',
  "AAABLXSU" varchar(1) NOT NULL default ' ',
  "AAAAWPSP" varchar(1) NOT NULL default ' ',
  "AAAAWQWE" varchar(2) NOT NULL default ' ',
  "AAAAWROO" varchar(3) NOT NULL default '000',
  "AAABJSIH" varchar(1) NOT NULL default ' ',
  "AAAAWPRZ" varchar(1) NOT NULL default ' ',
  "AAAAWPSA" varchar(1) NOT NULL default ' ',
  "AAAAWPSB" varchar(1) NOT NULL default ' ',
  "AAAAWPSC" varchar(1) NOT NULL default ' ',
  "AAABELVQ" varchar(1) NOT NULL default ' ',
  "AAAAWPSE" varchar(1) NOT NULL default ' ',
  "AAAAXITD" varchar(2) NOT NULL default ' ',
  "AAABLZZR" varchar(4) NOT NULL default ' ',
  "AAABLYAT" varchar(10) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAAAWPSL","AAAAWPSM","AAABMBFP","AAABLXSU","AAAAWPSP","AAAAWQWE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPSR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABJSIH" varchar(1) NOT NULL default ' ',
  "AAABLXSU" varchar(1) NOT NULL default ' ',
  "AAAAWPSW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABJSIH","AAABLXSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPTB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWPTP" varchar(3) NOT NULL default ' ',
  "AAAAWROO" varchar(3) NOT NULL default '000',
  "AAAAWPTR" varchar(1) NOT NULL default ' ',
  "AAABGPHI" varchar(5) NOT NULL default ' ',
  "AAAAWPTI" varchar(1) NOT NULL default ' ',
  "AAAAWPTJ" varchar(1) NOT NULL default ' ',
  "AAAAWPTK" varchar(1) NOT NULL default ' ',
  "AAAAWPTL" varchar(1) NOT NULL default ' ',
  "AAAAWPTM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAAAWPTP","AAAAWROO","AAAAWPTR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPTS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABKRSA" varchar(3) NOT NULL default ' ',
  "AAAAWRFV" varchar(3) NOT NULL default '000',
  "AAAAWPTX" varchar(3) NOT NULL default '000',
  "AAAAWQXS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABKRSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPUC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABJSIH" varchar(1) NOT NULL default ' ',
  "AAAAWRFV" varchar(3) NOT NULL default ' ',
  "AAAAWRFW" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABJSIH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPUL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWPWC" varchar(4) NOT NULL default ' ',
  "AAABIOAK" varchar(12) NOT NULL default ' ',
  "AAAAWQNJ" varchar(1) NOT NULL default ' ',
  "AAAAWPVW" varchar(1) NOT NULL default ' ',
  "AAAAWPUR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWPWC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPUU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWPWC" varchar(4) NOT NULL default ' ',
  "AAABIOAK" varchar(12) NOT NULL default ' ',
  "AAAAWQNJ" varchar(1) NOT NULL default ' ',
  "AAAAWPVW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAAAWPWC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPVE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWPVH" varchar(1) NOT NULL default ' ',
  "AAAAWPVI" varchar(1) NOT NULL default ' ',
  "AAAAWPVJ" varchar(1) NOT NULL default ' ',
  "AAAAWPVK" varchar(1) NOT NULL default ' ',
  "AAAAWPVL" varchar(1) NOT NULL default ' ',
  "AAABGCTJ" varchar(4) NOT NULL default ' ',
  "AAABFWHC" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPVQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAAAWPWC" varchar(4) NOT NULL default ' ',
  "AAABIOAK" varchar(12) NOT NULL default ' ',
  "AAAAWQNJ" varchar(1) NOT NULL default ' ',
  "AAAAWPVW" varchar(1) NOT NULL default ' ',
  "AAABIMHJ" varchar(12) NOT NULL default ' ',
  "AAABLEUZ" varchar(12) NOT NULL default ' ',
  "AAABHYJM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLEZ","AAAAWPWC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPWD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWQWE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAAAWQWE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPWK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWQWE" varchar(2) NOT NULL default ' ',
  "AAAAWPWP" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLFK","AAAAWQWE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPWU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWQVJ" varchar(2) NOT NULL default ' ',
  "AAABGPKK" varchar(3) NOT NULL default ' ',
  "AAAAWPWZ" varchar(10) NOT NULL default ' ',
  "AAAAWPXA" varchar(1) NOT NULL default ' ',
  "AAAAWQLJ" varchar(1) NOT NULL default ' ',
  "AAAAWTRV" varchar(1) NOT NULL default ' ',
  "AAAAWPXD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAAAWQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPXH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWQVJ" varchar(2) NOT NULL default ' ',
  "AAAAWPXM" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLFK","AAAAWQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPXR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWQFU" varchar(1) NOT NULL default ' ',
  "AAAAWQMP" varchar(1) NOT NULL default ' ',
  "AAAAWPXW" varchar(1) NOT NULL default ' ',
  "AAAAWPXX" varchar(1) NOT NULL default ' ',
  "AAAAWROO" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAAAWQFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPYC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABLHLV" varchar(3) NOT NULL default ' ',
  "AAAAZEVB" varchar(3) NOT NULL default ' ',
  "AAAAWROO" varchar(3) NOT NULL default '000',
  "AAAAWPYI" varchar(1) NOT NULL default ' ',
  "AAABCHHL" varchar(1) NOT NULL default ' ',
  "AAABMRFW" varchar(2) NOT NULL default '00',
  "AAABGPHI" varchar(5) NOT NULL default ' ',
  "AAAAWPZC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABLHLV","AAAAZEVB","AAAAWROO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPYS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABLHLV" varchar(3) NOT NULL default ' ',
  "AAAAZEVB" varchar(3) NOT NULL default ' ',
  "AAAAWROO" varchar(3) NOT NULL default '000',
  "AAAAWQWH" varchar(1) NOT NULL default '0',
  "AAAAWROJ" varchar(1) NOT NULL default ' ',
  "AAAAWPZA" varchar(10) NOT NULL default ' ',
  "AAABGCHL" varchar(3) NOT NULL default ' ',
  "AAAAWPZC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABLHLV","AAAAZEVB","AAAAWROO","AAAAWQWH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPZJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABLXDP" varchar(30) NOT NULL default ' ',
  "AAABGCHL" varchar(3) NOT NULL default ' ',
  "AAAAWROJ" varchar(1) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABLXDP","AAABGCHL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWPZU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABGCHL" varchar(3) NOT NULL default ' ',
  "AAABJHHP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLFK","AAABGCHL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQAE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABGCHL" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABGCHL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQAL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWQCD" varchar(2) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWROO" varchar(3) NOT NULL default '000',
  "AAABLHLV" varchar(3) NOT NULL default ' ',
  "AAAAZEVB" varchar(3) NOT NULL default ' ',
  "AAAAWQAS" decimal(13,3) NOT NULL default '0.000',
  "AAABKRZV" varchar(3) NOT NULL default ' ',
  "AAAAWQAU" varchar(2) NOT NULL default ' ',
  "AAABJRCJ" varchar(1) NOT NULL default ' ',
  "AAAAWQAW" varchar(1) NOT NULL default ' ',
  "AAAAWQAX" decimal(13,3) NOT NULL default '0.000',
  "AAABLWPB" varchar(3) NOT NULL default ' ',
  "AAAAWQAZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWQCD","AAABMLFK","AAAAWROO","AAABLHLV","AAAAZEVB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQBG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWQCD" varchar(2) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWQCE" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAWQCD","AAABMLFK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQBO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWQCD" varchar(2) NOT NULL default ' ',
  "AAAAWQCE" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAWQCD","AAAAWQCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQBV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWQCD" varchar(2) NOT NULL default ' ',
  "AAAAWQCE" varchar(2) NOT NULL default '00',
  "AAAAWQCA" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWQCD","AAAAWQCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQCF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABGCTJ" varchar(4) NOT NULL default ' ',
  "AAAAWQCJ" varchar(4) NOT NULL default ' ',
  "AAAAWQCK" varchar(4) NOT NULL default ' ',
  "AAAAWQCL" varchar(4) NOT NULL default ' ',
  "AAAAWQCM" varchar(4) NOT NULL default ' ',
  "AAAAWQCN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABGCTJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQCR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABLHLV" varchar(3) NOT NULL default ' ',
  "AAAAZEVB" varchar(3) NOT NULL default ' ',
  "AAABGCTJ" varchar(4) NOT NULL default ' ',
  "AAAAWQCX" varchar(1) NOT NULL default ' ',
  "AAAAWQCY" varchar(1) NOT NULL default ' ',
  "AAABBNBC" varchar(16) NOT NULL default ' ',
  "AAABFWHA" varchar(2) NOT NULL default ' ',
  "AAAAWQDB" varchar(1) NOT NULL default ' ',
  "AAAAYVAF" varchar(16) NOT NULL default ' ',
  "AAAAWQEI" varchar(2) NOT NULL default ' ',
  "AAAAWQDE" varchar(1) NOT NULL default ' ',
  "AAAAWQEJ" varchar(16) NOT NULL default ' ',
  "AAAAWQEK" varchar(2) NOT NULL default ' ',
  "AAAAWQDH" varchar(1) NOT NULL default ' ',
  "AAAAWQEL" varchar(16) NOT NULL default ' ',
  "AAAAWQEM" varchar(2) NOT NULL default ' ',
  "AAAAWQDK" varchar(16) NOT NULL default ' ',
  "AAAAWQDL" varchar(2) NOT NULL default ' ',
  "AAAAWQDM" varchar(4) NOT NULL default ' ',
  "AAAAWQDN" varchar(1) NOT NULL default ' ',
  "AAAAWQEO" varchar(16) NOT NULL default ' ',
  "AAAAWQEQ" varchar(2) NOT NULL default ' ',
  "AAAAWQEN" varchar(1) NOT NULL default '0',
  "AAAAWQES" varchar(16) NOT NULL default ' ',
  "AAAAWQET" varchar(2) NOT NULL default ' ',
  "AAAAWQDT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABLHLV","AAAAZEVB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQDY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAZTFM" varchar(2) NOT NULL default ' ',
  "AAABBNBC" varchar(16) NOT NULL default ' ',
  "AAAAWQED" varchar(1) NOT NULL default ' ',
  "AAAAWQEE" varchar(1) NOT NULL default ' ',
  "AAAAWQEF" varchar(2) NOT NULL default '00',
  "AAABFWHA" varchar(2) NOT NULL default ' ',
  "AAAAYVAF" varchar(16) NOT NULL default ' ',
  "AAAAWQEI" varchar(2) NOT NULL default ' ',
  "AAAAWQEJ" varchar(16) NOT NULL default ' ',
  "AAAAWQEK" varchar(2) NOT NULL default ' ',
  "AAAAWQEL" varchar(16) NOT NULL default ' ',
  "AAAAWQEM" varchar(2) NOT NULL default ' ',
  "AAAAWQEN" varchar(1) NOT NULL default '0',
  "AAAAWQEO" varchar(16) NOT NULL default ' ',
  "AAABCDEF" varchar(1) NOT NULL default ' ',
  "AAAAWQEQ" varchar(2) NOT NULL default ' ',
  "AAABDHAB" varchar(1) NOT NULL default ' ',
  "AAAAWQES" varchar(16) NOT NULL default ' ',
  "AAAAWQET" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAAAZTFM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQEX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABFWHA" varchar(2) NOT NULL default ' ',
  "AAABMEEL" varchar(3) NOT NULL default '000',
  "AAABMEEK" varchar(4) NOT NULL default ' ',
  "AAABMEEM" varchar(6) NOT NULL default ' ',
  "AAABMEEU" varchar(1) NOT NULL default ' ',
  "AAABMEEV" varchar(1) NOT NULL default ' ',
  "AAABLKWB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABFWHA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQFK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWQFU" varchar(1) NOT NULL default ' ',
  "AAAAWQFO" varchar(1) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAAAZTFM" varchar(2) NOT NULL default ' ',
  "AAABGCTJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAAAWQFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQFV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAZTFM" varchar(2) NOT NULL default ' ',
  "AAABILRJ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLFK","AAAAZTFM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQGF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABLGNY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABLGNY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQGM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABLGNY" varchar(1) NOT NULL default ' ',
  "AAAAWQGR" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLFK","AAABLGNY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQGW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABKRSA" varchar(3) NOT NULL default ' ',
  "AAAAWQHA" varchar(1) NOT NULL default ' ',
  "AAAAWQHB" varchar(1) NOT NULL default ' ',
  "AAAAWQHC" varchar(1) NOT NULL default ' ',
  "AAAAWQHD" varchar(1) NOT NULL default ' ',
  "AAAAWQHE" varchar(1) NOT NULL default ' ',
  "AAABLLTY" varchar(1) NOT NULL default ' ',
  "AAAAXUWO" varchar(1) NOT NULL default ' ',
  "AAABLRYT" varchar(1) NOT NULL default ' ',
  "AAAAWQHI" varchar(1) NOT NULL default ' ',
  "AAAAWQHJ" varchar(1) NOT NULL default ' ',
  "AAABKVKH" varchar(3) NOT NULL default ' ',
  "AAABAWEU" varchar(1) NOT NULL default ' ',
  "AAABGHHZ" varchar(1) NOT NULL default ' ',
  "AAAAWQHN" varchar(2) NOT NULL default ' ',
  "AAAAWQKN" varchar(1) NOT NULL default ' ',
  "AAAAWQHP" varchar(1) NOT NULL default ' ',
  "AAABGCTJ" varchar(4) NOT NULL default ' ',
  "AAAAWQHR" varchar(1) NOT NULL default ' ',
  "AAAAWQHS" varchar(1) NOT NULL default ' ',
  "AAAAWQHT" varchar(1) NOT NULL default ' ',
  "AAABLGNY" varchar(1) NOT NULL default ' ',
  "AAAAWQHV" varchar(1) NOT NULL default ' ',
  "AAABMAYW" varchar(1) NOT NULL default ' ',
  "AAAAWQWC" varchar(1) NOT NULL default '0',
  "AAAAWQHY" varchar(1) NOT NULL default ' ',
  "AAAAWQLU" varchar(1) NOT NULL default ' ',
  "AAAAWQIA" varchar(1) NOT NULL default ' ',
  "AAAAWQIB" varchar(1) NOT NULL default ' ',
  "AAAAWRFZ" varchar(1) NOT NULL default ' ',
  "AAAAWQID" varchar(1) NOT NULL default ' ',
  "AAAAWQIE" varchar(3) NOT NULL default ' ',
  "AAAAWQIF" varchar(1) NOT NULL default ' ',
  "AAAAZSTD" varchar(3) NOT NULL default ' ',
  "AAAAWQIH" varchar(3) NOT NULL default '000',
  "AAAAWQII" varchar(1) NOT NULL default ' ',
  "AAAAWQIJ" varchar(1) NOT NULL default ' ',
  "AAAAXITD" varchar(2) NOT NULL default ' ',
  "AAAAWQIL" varchar(1) NOT NULL default ' ',
  "AAAAWRGL" varchar(1) NOT NULL default ' ',
  "AAAAWQIN" varchar(1) NOT NULL default ' ',
  "AAAAWQIO" varchar(1) NOT NULL default ' ',
  "AAAAWQIP" varchar(1) NOT NULL default ' ',
  "AAAAWQIQ" varchar(1) NOT NULL default ' ',
  "AAAAWQIR" varchar(1) NOT NULL default ' ',
  "AAAAWQIS" varchar(1) NOT NULL default ' ',
  "AAAAWQIT" varchar(1) NOT NULL default ' ',
  "AAAAWQMF" varchar(1) NOT NULL default ' ',
  "AAAAWQIV" varchar(1) NOT NULL default ' ',
  "AAAAWQIW" varchar(1) NOT NULL default ' ',
  "AAAAWQIX" varchar(1) NOT NULL default ' ',
  "AAAAYVQI" varchar(1) NOT NULL default ' ',
  "AAAAWQIZ" varchar(1) NOT NULL default ' ',
  "AAABLXWU" varchar(10) NOT NULL default ' ',
  "AAAAWQJB" decimal(10,3) NOT NULL default '0.000',
  "AAAAWQJC" decimal(10,3) NOT NULL default '0.000',
  "AAAAWQJD" decimal(10,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABKRSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQJH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABKRSA" varchar(3) NOT NULL default ' ',
  "AAAAWQJL" varchar(1) NOT NULL default ' ',
  "AAAAWQJM" varchar(1) NOT NULL default ' ',
  "AAAAWQJN" varchar(1) NOT NULL default ' ',
  "AAAAWQJO" decimal(3,0) NOT NULL default '0',
  "AAABGFUB" varchar(1) NOT NULL default ' ',
  "AAAAWQJQ" varchar(1) NOT NULL default ' ',
  "AAABLGNY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABKRSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQJV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABKRSA" varchar(3) NOT NULL default ' ',
  "AAAAWQJZ" varchar(10) NOT NULL default ' ',
  "AAABKVKH" varchar(3) NOT NULL default ' ',
  "AAABLXVL" varchar(1) NOT NULL default ' ',
  "AAAAWQKC" varchar(1) NOT NULL default ' ',
  "AAAAWQKD" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABKRSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQKH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABKRSA" varchar(3) NOT NULL default ' ',
  "AAAAZSID" varchar(5) NOT NULL default ' ',
  "AAAAWQKM" varchar(1) NOT NULL default ' ',
  "AAAAWQKN" varchar(1) NOT NULL default ' ',
  "AAAAWQKO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABKRSA","AAAAZSID")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWROO" varchar(3) NOT NULL default '000',
  "AAABKQWH" varchar(1) NOT NULL default ' ',
  "AAABLHLV" varchar(3) NOT NULL default ' ',
  "AAAAWQKZ" varchar(10) NOT NULL default ' ',
  "AAAAZEVB" varchar(3) NOT NULL default ' ',
  "AAAAWQLB" varchar(10) NOT NULL default ' ',
  "AAABKVKH" varchar(3) NOT NULL default ' ',
  "AAAAWQLD" varchar(10) NOT NULL default ' ',
  "AAAAXJOZ" varchar(1) NOT NULL default ' ',
  "AAAAWQLF" varchar(1) NOT NULL default ' ',
  "AAAAZTFM" varchar(2) NOT NULL default ' ',
  "AAAAWQLH" varchar(1) NOT NULL default ' ',
  "AAAAWQLI" varchar(1) NOT NULL default ' ',
  "AAAAWQLJ" varchar(1) NOT NULL default ' ',
  "AAAAWQLK" varchar(1) NOT NULL default ' ',
  "AAAAWQLL" varchar(1) NOT NULL default ' ',
  "AAABLSGS" varchar(1) NOT NULL default ' ',
  "AAAAWQLN" varchar(1) NOT NULL default ' ',
  "AAAAWQLO" varchar(1) NOT NULL default ' ',
  "AAAAWQLP" varchar(1) NOT NULL default ' ',
  "AAAAWRNE" varchar(1) NOT NULL default ' ',
  "AAAAWQLR" varchar(1) NOT NULL default ' ',
  "AAAAWQLS" varchar(1) NOT NULL default ' ',
  "AAAAWQLT" varchar(1) NOT NULL default ' ',
  "AAAAWQLU" varchar(1) NOT NULL default ' ',
  "AAAAWQWD" varchar(3) NOT NULL default '000',
  "AAAAWQLW" varchar(1) NOT NULL default ' ',
  "AAAAWQOJ" varchar(1) NOT NULL default ' ',
  "AAAAWQLY" varchar(1) NOT NULL default ' ',
  "AAAAWRGI" varchar(6) NOT NULL default ' ',
  "AAAAWQMA" varchar(1) NOT NULL default ' ',
  "AAABHOTF" varchar(4) NOT NULL default ' ',
  "AAAAWQMC" varchar(1) NOT NULL default ' ',
  "AAAAXLRK" varchar(1) NOT NULL default ' ',
  "AAAAWQME" varchar(1) NOT NULL default ' ',
  "AAAAWQMF" varchar(1) NOT NULL default ' ',
  "AAAAWQMG" varchar(1) NOT NULL default ' ',
  "AAAAWQMH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAAAWROO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQML" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABKQWH" varchar(1) NOT NULL default ' ',
  "AAAAWQMP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABKQWH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQMT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABKQWH" varchar(1) NOT NULL default ' ',
  "AAAAZDPT" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLFK","AAABKQWH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQND" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAXITD" varchar(2) NOT NULL default ' ',
  "AAAAWQNH" varchar(1) NOT NULL default ' ',
  "AAABIOAK" varchar(12) NOT NULL default ' ',
  "AAAAWQNJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAAAXITD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQNN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWQOJ" varchar(1) NOT NULL default ' ',
  "AAAAWQNR" varchar(1) NOT NULL default ' ',
  "AAAAWQNS" varchar(1) NOT NULL default ' ',
  "AAAAWQNT" varchar(1) NOT NULL default ' ',
  "AAAAZTFM" varchar(2) NOT NULL default ' ',
  "AAAAWQNV" varchar(2) NOT NULL default ' ',
  "AAAAWQNW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAAAWQOJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQOA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWQOJ" varchar(1) NOT NULL default ' ',
  "AAAAWQOF" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLFK","AAAAWQOJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQOK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWROO" varchar(3) NOT NULL default '000',
  "AAAAWQOP" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLFK","AAAAWROO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWROO" varchar(3) NOT NULL default '000',
  "AAAAWQOY" varchar(1) NOT NULL default ' ',
  "AAAAWQOZ" varchar(1) NOT NULL default ' ',
  "AAAAWQPA" varchar(1) NOT NULL default ' ',
  "AAAAWQPB" varchar(1) NOT NULL default ' ',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAAAXITD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAAAWROO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABKRSA" varchar(3) NOT NULL default ' ',
  "AAAAWQQV" varchar(3) NOT NULL default ' ',
  "AAAAZSID" varchar(5) NOT NULL default ' ',
  "AAAAWQWC" varchar(1) NOT NULL default '0',
  "AAAAWQPO" varchar(3) NOT NULL default ' ',
  "AAAAWQPP" varchar(3) NOT NULL default ' ',
  "AAAAWQPQ" varchar(3) NOT NULL default ' ',
  "AAAAWQPR" varchar(3) NOT NULL default ' ',
  "AAAAWQPS" varchar(3) NOT NULL default ' ',
  "AAAAWQPT" varchar(3) NOT NULL default ' ',
  "AAAAWQPU" varchar(3) NOT NULL default ' ',
  "AAAAWQPV" varchar(3) NOT NULL default ' ',
  "AAAAWQPW" varchar(3) NOT NULL default ' ',
  "AAAAWQPX" varchar(3) NOT NULL default ' ',
  "AAAAWQPY" varchar(3) NOT NULL default ' ',
  "AAAAWQPZ" varchar(3) NOT NULL default ' ',
  "AAAAWQQA" varchar(3) NOT NULL default ' ',
  "AAAAWQQB" varchar(3) NOT NULL default ' ',
  "AAAAWQQC" varchar(3) NOT NULL default ' ',
  "AAAAWQQD" varchar(3) NOT NULL default ' ',
  "AAAAWQQE" varchar(3) NOT NULL default ' ',
  "AAAAWQQF" varchar(3) NOT NULL default ' ',
  "AAAAWQQG" varchar(3) NOT NULL default ' ',
  "AAAAWQQH" varchar(3) NOT NULL default ' ',
  "AAAAWQQI" varchar(3) NOT NULL default ' ',
  "AAAAWQQJ" varchar(3) NOT NULL default ' ',
  "AAAAWQQK" varchar(3) NOT NULL default ' ',
  "AAAAWQQL" varchar(3) NOT NULL default ' ',
  "AAAAWQQM" varchar(3) NOT NULL default ' ',
  "AAAAWQQN" varchar(3) NOT NULL default ' ',
  "AAAAWQQO" varchar(3) NOT NULL default ' ',
  "AAAAWQQP" varchar(3) NOT NULL default ' ',
  "AAAAWQQQ" varchar(3) NOT NULL default ' ',
  "AAAAWQQR" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABKRSA","AAAAWQQV","AAAAZSID","AAAAWQWC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABKRSA" varchar(3) NOT NULL default ' ',
  "AAAAWQRC" varchar(3) NOT NULL default ' ',
  "AAAAWQRD" varchar(3) NOT NULL default ' ',
  "AAAAWQRE" varchar(3) NOT NULL default ' ',
  "AAAAWQRF" varchar(3) NOT NULL default ' ',
  "AAAAWQRG" varchar(3) NOT NULL default ' ',
  "AAAAWQRH" varchar(3) NOT NULL default ' ',
  "AAAAWQRI" varchar(3) NOT NULL default ' ',
  "AAAAWQRJ" varchar(3) NOT NULL default ' ',
  "AAAAWQRK" varchar(3) NOT NULL default ' ',
  "AAAAWQRL" varchar(3) NOT NULL default ' ',
  "AAAAWQRM" varchar(3) NOT NULL default ' ',
  "AAAAWQRN" varchar(3) NOT NULL default ' ',
  "AAAAWQRO" varchar(3) NOT NULL default ' ',
  "AAAAWQRP" varchar(3) NOT NULL default ' ',
  "AAAAWQRQ" varchar(3) NOT NULL default ' ',
  "AAAAWQRR" varchar(3) NOT NULL default ' ',
  "AAAAWQRS" varchar(3) NOT NULL default ' ',
  "AAAAWQRT" varchar(3) NOT NULL default ' ',
  "AAAAWQRU" varchar(3) NOT NULL default ' ',
  "AAAAWQRV" varchar(3) NOT NULL default ' ',
  "AAAAWQRW" varchar(3) NOT NULL default ' ',
  "AAAAWQRX" varchar(3) NOT NULL default ' ',
  "AAAAWQRY" varchar(3) NOT NULL default ' ',
  "AAAAWQRZ" varchar(3) NOT NULL default ' ',
  "AAAAWQSA" varchar(3) NOT NULL default ' ',
  "AAAAWQSB" varchar(3) NOT NULL default ' ',
  "AAAAWQSC" varchar(3) NOT NULL default ' ',
  "AAAAWQSD" varchar(3) NOT NULL default ' ',
  "AAAAWQSE" varchar(3) NOT NULL default ' ',
  "AAAAWQSF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABKRSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQSJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABAAMP" varchar(3) NOT NULL default ' ',
  "AAAAWQSN" varchar(2) NOT NULL default ' ',
  "AAAAWQSO" varchar(2) NOT NULL default ' ',
  "AAAAWQSP" varchar(2) NOT NULL default ' ',
  "AAAAWQSQ" varchar(2) NOT NULL default ' ',
  "AAAAWQSR" varchar(2) NOT NULL default ' ',
  "AAAAWQSS" varchar(2) NOT NULL default ' ',
  "AAAAWQST" varchar(2) NOT NULL default ' ',
  "AAAAWQSU" varchar(2) NOT NULL default ' ',
  "AAAAWQSV" varchar(2) NOT NULL default ' ',
  "AAAAWQSW" varchar(2) NOT NULL default ' ',
  "AAAAWQSX" varchar(2) NOT NULL default ' ',
  "AAAAWQSY" varchar(2) NOT NULL default ' ',
  "AAAAWQSZ" varchar(2) NOT NULL default ' ',
  "AAAAWQTA" varchar(2) NOT NULL default ' ',
  "AAAAWQTB" varchar(2) NOT NULL default ' ',
  "AAAAWQTC" varchar(2) NOT NULL default ' ',
  "AAAAWQTD" varchar(2) NOT NULL default ' ',
  "AAAAWQTE" varchar(2) NOT NULL default ' ',
  "AAAAWQTF" varchar(2) NOT NULL default ' ',
  "AAAAWQTG" varchar(2) NOT NULL default ' ',
  "AAAAWQTH" varchar(2) NOT NULL default ' ',
  "AAAAWQTI" varchar(2) NOT NULL default ' ',
  "AAAAWQTJ" varchar(2) NOT NULL default ' ',
  "AAAAWQTK" varchar(2) NOT NULL default ' ',
  "AAAAWQTL" varchar(2) NOT NULL default ' ',
  "AAAAWQTM" varchar(2) NOT NULL default ' ',
  "AAAAWQTN" varchar(2) NOT NULL default ' ',
  "AAAAWQTO" varchar(2) NOT NULL default ' ',
  "AAAAWQTP" varchar(2) NOT NULL default ' ',
  "AAAAWQTQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABAAMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQTU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABKRNR" varchar(1) NOT NULL default ' ',
  "AAAAWQVY" varchar(3) NOT NULL default ' ',
  "AAABJSIH" varchar(1) NOT NULL default ' ',
  "AAABLXSU" varchar(1) NOT NULL default ' ',
  "AAAAZSID" varchar(5) NOT NULL default ' ',
  "AAAAWQWC" varchar(1) NOT NULL default '0',
  "AAAAWQWD" varchar(3) NOT NULL default '000',
  "AAAAWQWE" varchar(2) NOT NULL default ' ',
  "AAAAWQUF" varchar(3) NOT NULL default ' ',
  "AAAAWQUG" varchar(3) NOT NULL default ' ',
  "AAAAWQUH" varchar(3) NOT NULL default ' ',
  "AAAAWQUI" varchar(3) NOT NULL default ' ',
  "AAAAWQUJ" varchar(3) NOT NULL default ' ',
  "AAAAWQUK" varchar(3) NOT NULL default ' ',
  "AAAAWQUL" varchar(3) NOT NULL default ' ',
  "AAAAWQUM" varchar(3) NOT NULL default ' ',
  "AAAAWQUN" varchar(3) NOT NULL default ' ',
  "AAAAWQUO" varchar(3) NOT NULL default ' ',
  "AAAAWQUP" varchar(3) NOT NULL default ' ',
  "AAAAWQUQ" varchar(3) NOT NULL default ' ',
  "AAAAWQUR" varchar(3) NOT NULL default ' ',
  "AAAAWQUS" varchar(3) NOT NULL default ' ',
  "AAAAWQUT" varchar(3) NOT NULL default ' ',
  "AAAAWQUU" varchar(3) NOT NULL default ' ',
  "AAAAWQUV" varchar(3) NOT NULL default ' ',
  "AAAAWQUW" varchar(3) NOT NULL default ' ',
  "AAAAWQUX" varchar(3) NOT NULL default ' ',
  "AAAAWQUY" varchar(3) NOT NULL default ' ',
  "AAAAWQUZ" varchar(3) NOT NULL default ' ',
  "AAAAWQVA" varchar(3) NOT NULL default ' ',
  "AAAAWQVB" varchar(3) NOT NULL default ' ',
  "AAAAWQVC" varchar(3) NOT NULL default ' ',
  "AAAAWQVD" varchar(3) NOT NULL default ' ',
  "AAAAWQVE" varchar(3) NOT NULL default ' ',
  "AAAAWQVF" varchar(3) NOT NULL default ' ',
  "AAAAWQVG" varchar(3) NOT NULL default ' ',
  "AAAAWQVH" varchar(3) NOT NULL default ' ',
  "AAAAWQVI" varchar(3) NOT NULL default ' ',
  "AAAAWQVJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABKRNR","AAAAWQVY","AAABJSIH","AAABLXSU","AAAAZSID","AAAAWQWC","AAAAWQWD","AAAAWQWE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWQWH" varchar(3) NOT NULL default '000',
  "AAAAWQVY" varchar(1) NOT NULL default ' ',
  "AAABJSIH" varchar(1) NOT NULL default ' ',
  "AAABLXSU" varchar(1) NOT NULL default ' ',
  "AAAAZSID" varchar(1) NOT NULL default ' ',
  "AAAAWQWC" varchar(1) NOT NULL default ' ',
  "AAAAWQWD" varchar(1) NOT NULL default ' ',
  "AAAAWQWE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAAAWQWH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQWI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABKRSA" varchar(3) NOT NULL default ' ',
  "AAAAWQWM" varchar(1) NOT NULL default ' ',
  "AAAAWQWN" varchar(1) NOT NULL default ' ',
  "AAAAWQWO" varchar(1) NOT NULL default ' ',
  "AAAAWQWP" varchar(1) NOT NULL default ' ',
  "AAAAWQWQ" varchar(1) NOT NULL default ' ',
  "AAAAWQXF" decimal(3,0) NOT NULL default '0',
  "AAAAWQWS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABKRSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQWW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWQXS" varchar(1) NOT NULL default ' ',
  "AAABKRSA" varchar(3) NOT NULL default ' ',
  "AAABKRSB" varchar(10) NOT NULL default ' ',
  "AAAAWQXC" varchar(1) NOT NULL default ' ',
  "AAAAWTPS" varchar(1) NOT NULL default ' ',
  "AAAAWQXE" varchar(1) NOT NULL default ' ',
  "AAAAWQXF" decimal(3,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAAAWQXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQXJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWQXS" varchar(1) NOT NULL default ' ',
  "AAABJFBB" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLFK","AAAAWQXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWQXT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABKRSA" varchar(3) NOT NULL default ' ',
  "AAABKQMT" varchar(1) NOT NULL default ' ',
  "AAABAEAV" decimal(2,0) NOT NULL default '0',
  "AAAAWQXZ" varchar(2) NOT NULL default ' ',
  "AAAAWQYA" varchar(2) NOT NULL default ' ',
  "AAAAWQYB" varchar(2) NOT NULL default ' ',
  "AAAAWQYC" varchar(2) NOT NULL default ' ',
  "AAAAWQYD" varchar(2) NOT NULL default ' ',
  "AAAAWQYE" varchar(2) NOT NULL default ' ',
  "AAAAWQYF" varchar(2) NOT NULL default ' ',
  "AAAAWQYG" varchar(2) NOT NULL default ' ',
  "AAAAWQYH" varchar(2) NOT NULL default ' ',
  "AAAAWQYI" varchar(2) NOT NULL default ' ',
  "AAAAWQYJ" varchar(2) NOT NULL default ' ',
  "AAAAWQYK" varchar(2) NOT NULL default ' ',
  "AAAAWQYL" varchar(2) NOT NULL default ' ',
  "AAAAWQYM" varchar(2) NOT NULL default ' ',
  "AAAAWQYN" varchar(2) NOT NULL default ' ',
  "AAAAWQYO" varchar(2) NOT NULL default ' ',
  "AAAAWQYP" varchar(2) NOT NULL default ' ',
  "AAAAWQYQ" varchar(2) NOT NULL default ' ',
  "AAAAWQYR" varchar(2) NOT NULL default ' ',
  "AAAAWQYS" varchar(2) NOT NULL default ' ',
  "AAAAWQYT" varchar(2) NOT NULL default ' ',
  "AAAAWQYU" varchar(2) NOT NULL default ' ',
  "AAAAWQYV" varchar(2) NOT NULL default ' ',
  "AAAAWQYW" varchar(2) NOT NULL default ' ',
  "AAAAWQYX" varchar(2) NOT NULL default ' ',
  "AAAAWQYY" varchar(2) NOT NULL default ' ',
  "AAAAWQYZ" varchar(2) NOT NULL default ' ',
  "AAAAWQZA" varchar(2) NOT NULL default ' ',
  "AAAAWQZB" varchar(2) NOT NULL default ' ',
  "AAAAWQZC" varchar(2) NOT NULL default ' ',
  "AAAAWQZD" varchar(2) NOT NULL default ' ',
  "AAAAWQZE" varchar(2) NOT NULL default ' ',
  "AAAAWQZF" varchar(2) NOT NULL default ' ',
  "AAAAWQZG" varchar(2) NOT NULL default ' ',
  "AAAAWQZH" varchar(2) NOT NULL default ' ',
  "AAAAWQZI" varchar(2) NOT NULL default ' ',
  "AAAAWQZJ" varchar(2) NOT NULL default ' ',
  "AAAAWQZK" varchar(2) NOT NULL default ' ',
  "AAAAWQZL" varchar(2) NOT NULL default ' ',
  "AAAAWQZM" varchar(2) NOT NULL default ' ',
  "AAAAWQZN" varchar(2) NOT NULL default ' ',
  "AAAAWQZO" varchar(2) NOT NULL default ' ',
  "AAAAWQZP" varchar(2) NOT NULL default ' ',
  "AAAAWQZQ" varchar(2) NOT NULL default ' ',
  "AAAAWQZR" varchar(2) NOT NULL default ' ',
  "AAAAWQZS" varchar(2) NOT NULL default ' ',
  "AAAAWQZT" varchar(2) NOT NULL default ' ',
  "AAAAWQZU" varchar(2) NOT NULL default ' ',
  "AAAAWQZV" varchar(2) NOT NULL default ' ',
  "AAAAWQZW" varchar(2) NOT NULL default ' ',
  "AAAAWQZX" varchar(2) NOT NULL default ' ',
  "AAAAWQZY" varchar(2) NOT NULL default ' ',
  "AAAAWQZZ" varchar(2) NOT NULL default ' ',
  "AAAAWRAA" varchar(2) NOT NULL default ' ',
  "AAAAWRAB" varchar(2) NOT NULL default ' ',
  "AAAAWRAC" varchar(2) NOT NULL default ' ',
  "AAAAWRAD" varchar(2) NOT NULL default ' ',
  "AAAAWRAE" varchar(2) NOT NULL default ' ',
  "AAAAWRAF" varchar(2) NOT NULL default ' ',
  "AAAAWRAG" varchar(2) NOT NULL default ' ',
  "AAAAWRAH" varchar(2) NOT NULL default ' ',
  "AAAAWRAI" varchar(2) NOT NULL default ' ',
  "AAAAWRAJ" varchar(2) NOT NULL default ' ',
  "AAAAWRAK" varchar(2) NOT NULL default ' ',
  "AAAAWRAL" varchar(2) NOT NULL default ' ',
  "AAAAWRAM" varchar(2) NOT NULL default ' ',
  "AAAAWRAN" varchar(2) NOT NULL default ' ',
  "AAAAWRAO" varchar(2) NOT NULL default ' ',
  "AAAAWRAP" varchar(2) NOT NULL default ' ',
  "AAAAWRAQ" varchar(2) NOT NULL default ' ',
  "AAAAWRAR" varchar(2) NOT NULL default ' ',
  "AAAAWRAS" varchar(2) NOT NULL default ' ',
  "AAAAWRAT" varchar(2) NOT NULL default ' ',
  "AAAAWRAU" varchar(2) NOT NULL default ' ',
  "AAAAWRAV" varchar(2) NOT NULL default ' ',
  "AAAAWRAW" varchar(2) NOT NULL default ' ',
  "AAAAWRAX" varchar(2) NOT NULL default ' ',
  "AAAAWRAY" varchar(2) NOT NULL default ' ',
  "AAAAWRAZ" varchar(2) NOT NULL default ' ',
  "AAAAWRBA" varchar(2) NOT NULL default ' ',
  "AAAAWRBB" varchar(2) NOT NULL default ' ',
  "AAAAWRBC" varchar(2) NOT NULL default ' ',
  "AAAAWRBD" varchar(2) NOT NULL default ' ',
  "AAAAWRBE" varchar(2) NOT NULL default ' ',
  "AAAAWRBF" varchar(2) NOT NULL default ' ',
  "AAAAWRBG" varchar(2) NOT NULL default ' ',
  "AAAAWRBH" varchar(2) NOT NULL default ' ',
  "AAAAWRBI" varchar(2) NOT NULL default ' ',
  "AAAAWRBJ" varchar(2) NOT NULL default ' ',
  "AAAAWRBK" varchar(2) NOT NULL default ' ',
  "AAAAWRBL" varchar(2) NOT NULL default ' ',
  "AAAAWRBM" varchar(2) NOT NULL default ' ',
  "AAAAWRBN" varchar(2) NOT NULL default ' ',
  "AAAAWRBO" varchar(2) NOT NULL default ' ',
  "AAAAWRBP" varchar(2) NOT NULL default ' ',
  "AAAAWRBQ" varchar(2) NOT NULL default ' ',
  "AAAAWRBR" varchar(2) NOT NULL default ' ',
  "AAAAWRBS" varchar(2) NOT NULL default ' ',
  "AAAAWRBT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABKRSA","AAABKQMT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRBY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABKRSA" varchar(3) NOT NULL default ' ',
  "AAAAWRIP" varchar(2) NOT NULL default ' ',
  "AAABAAMP" varchar(3) NOT NULL default ' ',
  "AAABJLFF" varchar(2) NOT NULL default ' ',
  "AAAAWRCF" decimal(3,0) NOT NULL default '0',
  "AAAAXVXK" decimal(2,0) NOT NULL default '0',
  "AAAAWRCH" decimal(5,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABKRSA","AAAAWRIP","AAABAAMP","AAABJLFF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRCO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABKRSA" varchar(3) NOT NULL default ' ',
  "AAABGYAB" varchar(5) NOT NULL default ' ',
  "AAAAWRCT" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABKRSA","AAABGYAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRCY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABKRSA" varchar(3) NOT NULL default ' ',
  "AAAAWRDH" varchar(5) NOT NULL default '00000',
  "AAAAWRDD" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABKRSA","AAAAWRDH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRDI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABKRSA" varchar(3) NOT NULL default ' ',
  "AAAAWRIP" varchar(2) NOT NULL default ' ',
  "AAABAAMP" varchar(3) NOT NULL default ' ',
  "AAABKQMT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABKRSA","AAAAWRIP","AAABAAMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRDU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABJLFF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABJLFF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWREB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABJLFF" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJBVB" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABJLFF","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWREL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWRFB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAAAWRFB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRES" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWRFB" varchar(4) NOT NULL default ' ',
  "AAAAWREX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLFK","AAAAWRFB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRFC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWRFF" varchar(1) NOT NULL default ' ',
  "AAAAWRFG" varchar(1) NOT NULL default ' ',
  "AAAAWRFH" decimal(4,0) NOT NULL default '0',
  "AAAAWRFI" varchar(4) NOT NULL default '0000',
  "AAAAWRFJ" varchar(2) NOT NULL default ' ',
  "AAAAWRFK" varchar(2) NOT NULL default ' ',
  "AAAAWRFL" varchar(2) NOT NULL default ' ',
  "AAAAWRFM" varchar(2) NOT NULL default ' ',
  "AAABMLEW" varchar(3) NOT NULL default ' ',
  "AAAAWRFO" varchar(1) NOT NULL default ' ',
  "AAAAWRFP" varchar(1) NOT NULL default ' ',
  "AAAAWRFQ" varchar(2) NOT NULL default ' ',
  "AAAAWRFR" varchar(1) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAAAWRFT" varchar(1) NOT NULL default ' ',
  "AAAAWRFU" varchar(2) NOT NULL default ' ',
  "AAAAWRFV" varchar(3) NOT NULL default '000',
  "AAAAWRFW" varchar(3) NOT NULL default '000',
  "AAAAWRFX" varchar(2) NOT NULL default ' ',
  "AAAAWRFY" varchar(1) NOT NULL default ' ',
  "AAAAWRFZ" varchar(1) NOT NULL default ' ',
  "AAAAWRGA" varchar(2) NOT NULL default ' ',
  "AAAAWRGB" varchar(1) NOT NULL default ' ',
  "AAAAWRGC" varchar(1) NOT NULL default ' ',
  "AAABAHTY" varchar(1) NOT NULL default ' ',
  "AAAAWRGE" varchar(1) NOT NULL default ' ',
  "AAAAWRGF" varchar(1) NOT NULL default ' ',
  "AAAAWRGG" varchar(1) NOT NULL default ' ',
  "AAABHSSF" varchar(1) NOT NULL default ' ',
  "AAAAWRGI" varchar(6) NOT NULL default ' ',
  "AAAAWRGJ" varchar(1) NOT NULL default ' ',
  "AAAAWRGK" varchar(1) NOT NULL default ' ',
  "AAAAWRGL" varchar(1) NOT NULL default ' ',
  "AAAAWRGM" varchar(3) NOT NULL default '000',
  "AAABHRDU" varchar(3) NOT NULL default ' ',
  "AAAAWRGO" varchar(1) NOT NULL default ' ',
  "AAAAWRGP" varchar(1) NOT NULL default ' ',
  "AAAAWRGQ" varchar(1) NOT NULL default ' ',
  "AAAAWRGR" varchar(3) NOT NULL default ' ',
  "AAABMLEY" varchar(3) NOT NULL default ' ',
  "AAAAWRGT" varchar(1) NOT NULL default ' ',
  "AAAAWRGU" varchar(1) NOT NULL default ' ',
  "AAABLYAT" varchar(10) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAAAWRGX" varchar(2) NOT NULL default ' ',
  "AAAAWRGY" varchar(1) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAAAWRHD" varchar(1) NOT NULL default ' ',
  "AAAAWRML" decimal(15,3) NOT NULL default '0.000',
  "AAAAWRMM" decimal(15,3) NOT NULL default '0.000',
  "AAAAWRHG" varchar(3) NOT NULL default '000',
  "AAAAWRHH" varchar(1) NOT NULL default ' ',
  "AAAAWRHI" varchar(2) NOT NULL default ' ',
  "AAABEBCS" varchar(3) NOT NULL default ' ',
  "AAAAWRHK" varchar(10) NOT NULL default ' ',
  "AAAAWRHL" varchar(1) NOT NULL default ' ',
  "AAAAWRHM" varchar(1) NOT NULL default ' ',
  "AAAAWRHN" varchar(1) NOT NULL default ' ',
  "AAABDVMT" varchar(1) NOT NULL default ' ',
  "AAAAWRHP" varchar(1) NOT NULL default ' ',
  "AAAAWRHQ" varchar(1) NOT NULL default ' ',
  "AAABAZBK" varchar(4) NOT NULL default ' ',
  "AAAAWRHS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRHV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWRIF" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default '0000',
  "AAABMDER" varchar(1) NOT NULL default ' ',
  "AAABLOLZ" varchar(2) NOT NULL default ' ',
  "AAABMDBL" varchar(1) NOT NULL default ' ',
  "AAAAWWBI" varchar(8) NOT NULL default ' ',
  "AAABDWNR" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWRIF","AAABMGHE","AAABMDER","AAABLOLZ","AAABMDBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRIK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABKRSA" varchar(3) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAAAWRIP" varchar(2) NOT NULL default ' ',
  "AAABCDDE" varchar(10) NOT NULL default ' ',
  "AAAAWRKT" varchar(10) NOT NULL default ' ',
  "AAAAWRKU" varchar(10) NOT NULL default ' ',
  "AAAAWRKV" varchar(10) NOT NULL default ' ',
  "AAAAZIDK" varchar(10) NOT NULL default ' ',
  "AAAAWRIV" varchar(3) NOT NULL default ' ',
  "AAAAWRIW" decimal(11,3) NOT NULL default '0.000',
  "AAABMLEW" varchar(3) NOT NULL default ' ',
  "AAAAWRIY" varchar(2) NOT NULL default ' ',
  "AAAAWRIZ" decimal(11,3) NOT NULL default '0.000',
  "AAAAWRJA" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABKRSA","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRJF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABKRSA" varchar(3) NOT NULL default ' ',
  "AAAAWRKA" varchar(1) NOT NULL default ' ',
  "AAAAWRKB" varchar(1) NOT NULL default ' ',
  "AAAAWRKC" varchar(1) NOT NULL default ' ',
  "AAAAWRKD" varchar(1) NOT NULL default ' ',
  "AAAAWRKE" varchar(1) NOT NULL default ' ',
  "AAAAWRKF" varchar(1) NOT NULL default ' ',
  "AAAAWRKG" varchar(1) NOT NULL default ' ',
  "AAAAWRKH" varchar(1) NOT NULL default ' ',
  "AAAAWRKI" varchar(1) NOT NULL default ' ',
  "AAAAWRKJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABKRSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRJW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABKRSA" varchar(3) NOT NULL default ' ',
  "AAAAWRKA" varchar(1) NOT NULL default ' ',
  "AAAAWRKB" varchar(1) NOT NULL default ' ',
  "AAAAWRKC" varchar(1) NOT NULL default ' ',
  "AAAAWRKD" varchar(1) NOT NULL default ' ',
  "AAAAWRKE" varchar(1) NOT NULL default ' ',
  "AAAAWRKF" varchar(1) NOT NULL default ' ',
  "AAAAWRKG" varchar(1) NOT NULL default ' ',
  "AAAAWRKH" varchar(1) NOT NULL default ' ',
  "AAAAWRKI" varchar(1) NOT NULL default ' ',
  "AAAAWRKJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABKRSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRKN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABKRSA" varchar(3) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABCDDE" varchar(10) NOT NULL default ' ',
  "AAAAWRKT" varchar(10) NOT NULL default ' ',
  "AAAAWRKU" varchar(10) NOT NULL default ' ',
  "AAAAWRKV" varchar(10) NOT NULL default ' ',
  "AAAAZIDK" varchar(10) NOT NULL default ' ',
  "AAAAWRKX" decimal(10,3) NOT NULL default '0.000',
  "AAAAWRKY" decimal(10,3) NOT NULL default '0.000',
  "AAAAWRKZ" decimal(10,3) NOT NULL default '0.000',
  "AAAAWRLA" varchar(1) NOT NULL default ' ',
  "AAAAWRLB" varchar(1) NOT NULL default ' ',
  "AAAAWRLC" varchar(1) NOT NULL default ' ',
  "AAAAWRLD" decimal(10,3) NOT NULL default '0.000',
  "AAAAWRLE" decimal(10,3) NOT NULL default '0.000',
  "AAAAWRLF" decimal(10,3) NOT NULL default '0.000',
  "AAAAWRLG" decimal(4,1) NOT NULL default '0.0',
  "AAAAWRLH" decimal(10,3) NOT NULL default '0.000',
  "AAAAWRLI" decimal(10,3) NOT NULL default '0.000',
  "AAAAWRLJ" decimal(10,3) NOT NULL default '0.000',
  "AAAAWRLK" decimal(10,3) NOT NULL default '0.000',
  "AAABLXWU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABKRSA","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRLQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWRLS" varchar(1) NOT NULL default ' ',
  "AAAAXBPM" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRLV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABKRNW" varchar(10) NOT NULL default ' ',
  "AAABKRRY" varchar(1) NOT NULL default ' ',
  "AAABFIZY" varchar(1) NOT NULL default ' ',
  "AAABLZZR" varchar(4) NOT NULL default ' ',
  "AAABLYAT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABKRNW","AAABKRRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRMH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABBZMT" varchar(10) NOT NULL default ' ',
  "AAAAWRML" decimal(15,3) NOT NULL default '0.000',
  "AAAAWRMM" decimal(15,3) NOT NULL default '0.000',
  "AAAAWRMN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABBZMT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRMR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWRNE" varchar(1) NOT NULL default ' ',
  "AAABKQWH" varchar(1) NOT NULL default ' ',
  "AAABLHLV" varchar(3) NOT NULL default ' ',
  "AAAAWRNH" varchar(3) NOT NULL default ' ',
  "AAAAZEVB" varchar(3) NOT NULL default ' ',
  "AAAAWRNJ" varchar(3) NOT NULL default ' ',
  "AAABLGXS" varchar(3) NOT NULL default ' ',
  "AAABBZMT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAAAWRNE","AAABKQWH","AAABLHLV","AAAAWRNH","AAAAZEVB","AAAAWRNJ","AAABLGXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRNL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABBZMT" varchar(10) NOT NULL default ' ',
  "AAAAXJQN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLFK","AAABBZMT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRNV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAAAWROC" varchar(10) NOT NULL default ' ',
  "AAAAWRNZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAAAWROC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWROD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABLHLV" varchar(3) NOT NULL default ' ',
  "AAAAZEVB" varchar(3) NOT NULL default ' ',
  "AAAAWROO" varchar(3) NOT NULL default '000',
  "AAAAWROJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABLHLV","AAAAZEVB","AAAAWROO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWROP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABKNQF" varchar(3) NOT NULL default ' ',
  "AAAAWROT" varchar(1) NOT NULL default ' ',
  "AAABLKXQ" varchar(4) NOT NULL default ' ',
  "AAABKNQJ" varchar(1) NOT NULL default ' ',
  "AAABKMOY" varchar(4) NOT NULL default ' ',
  "AAABLGEF" varchar(1) NOT NULL default ' ',
  "AAABGOWT" varchar(10) NOT NULL default ' ',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAAAWRPA" varchar(1) NOT NULL default ' ',
  "AAABKNPA" varchar(2) NOT NULL default ' ',
  "AAABGOTW" varchar(2) NOT NULL default ' ',
  "AAAAWRPD" varchar(1) NOT NULL default ' ',
  "AAABGJLM" varchar(1) NOT NULL default ' ',
  "AAAAWRPF" varchar(1) NOT NULL default ' ',
  "AAABGIZL" varchar(3) NOT NULL default ' ',
  "AAAAWRPH" varchar(4) NOT NULL default ' ',
  "AAAAWRPI" varchar(1) NOT NULL default ' ',
  "AAABGPLQ" varchar(2) NOT NULL default ' ',
  "AAABKOVP" varchar(1) NOT NULL default ' ',
  "AAABKOVB" varchar(6) NOT NULL default ' ',
  "AAAAWRPM" varchar(1) NOT NULL default ' ',
  "AAABLZNN" varchar(10) NOT NULL default ' ',
  "AAABLBDX" varchar(70) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAABKSTO" varchar(8) NOT NULL default ' ',
  "AAABGOVN" varchar(1) NOT NULL default ' ',
  "AAABHKKY" varchar(2) NOT NULL default ' ',
  "AAABGOVP" varchar(2) NOT NULL default ' ',
  "AAAAWRPU" varchar(1) NOT NULL default ' ',
  "AAAAWRPV" varchar(2) NOT NULL default ' ',
  "AAAAWRPW" varchar(1) NOT NULL default ' ',
  "AAAAWRPX" varchar(1) NOT NULL default ' ',
  "AAAAWRPY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRQB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABLCGT" varchar(3) NOT NULL default ' ',
  "AAAAWRQG" varchar(1) NOT NULL default ' ',
  "AAAAWRQH" varchar(1) NOT NULL default ' ',
  "AAAAWRQI" varchar(1) NOT NULL default ' ',
  "AAAAWRQJ" varchar(1) NOT NULL default ' ',
  "AAAAWRQK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABMLHQ","AAABLCGT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRQP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABKNQF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLHQ","AAABKNQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRQW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHQRM" varchar(6) NOT NULL default ' ',
  "AAAAWRRU" varchar(3) NOT NULL default ' ',
  "AAAAWRRV" varchar(3) NOT NULL default ' ',
  "AAAAWRRB" double NOT NULL default '0',
  "AAAAWRRC" double NOT NULL default '0',
  "AAAAWRRD" double NOT NULL default '0',
  "AAAAWRRE" double NOT NULL default '0',
  "AAAAWRRF" varchar(2) NOT NULL default '00',
  "AAAAWRRG" varchar(2) NOT NULL default '00',
  "AAAAWRRH" varchar(3) NOT NULL default '000',
  "AAAAWRRI" varchar(3) NOT NULL default '000',
  "AAAAWRRJ" varchar(3) NOT NULL default '000',
  "AAAAWRRK" varchar(3) NOT NULL default '000',
  "AAAAWRRL" varchar(1) NOT NULL default ' ',
  "AAABKPVZ" varchar(3) NOT NULL default '000',
  "AAABLHMG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHQRM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRRQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHQRM" varchar(6) NOT NULL default ' ',
  "AAABMLEL" varchar(2) NOT NULL default '00',
  "AAAAWRRU" varchar(3) NOT NULL default ' ',
  "AAAAWRRV" varchar(3) NOT NULL default ' ',
  "AAAAWRRW" double NOT NULL default '0',
  "AAAAXLRL" double NOT NULL default '0',
  "AAAAWRRY" double NOT NULL default '0',
  "AAAAWRRZ" double NOT NULL default '0',
  "AAAAWRSA" varchar(2) NOT NULL default '00',
  "AAABKOQT" varchar(3) NOT NULL default '000',
  "AAAAWRSC" varchar(3) NOT NULL default '000',
  "AAAAWRSD" varchar(1) NOT NULL default ' ',
  "AAAAWRSE" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABHQRM","AAABMLEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRSI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHQRM" varchar(6) NOT NULL default ' ',
  "AAABLXIS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHQRM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHQRM" varchar(6) NOT NULL default ' ',
  "AAABDABU" varchar(2) NOT NULL default '00',
  "AAABKMIH" varchar(30) NOT NULL default ' ',
  "AAABKMII" varchar(30) NOT NULL default ' ',
  "AAAAWRSX" varchar(2) NOT NULL default ' ',
  "AAAAWRSY" varchar(2) NOT NULL default ' ',
  "AAAAWRSZ" varchar(2) NOT NULL default ' ',
  "AAAAWRTA" varchar(2) NOT NULL default ' ',
  "AAAAWRTB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHQRM","AAABDABU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRTG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGPDC" varchar(9) NOT NULL default ' ',
  "AAAAWRTJ" varchar(8) NOT NULL default ' ',
  "AAAAWRTK" varchar(8) NOT NULL default ' ',
  "AAAAYGHL" varchar(8) NOT NULL default ' ',
  "AAAAWRTM" varchar(8) NOT NULL default ' ',
  "AAAAWRTN" varchar(8) NOT NULL default ' ',
  "AAAAWRTO" varchar(18) NOT NULL default ' ',
  "AAAAWRTP" varchar(1) NOT NULL default ' ',
  "AAABKXRE" varchar(2) NOT NULL default ' ',
  "AAAAWRTR" varchar(3) NOT NULL default ' ',
  "AAABMNMM" tinyint(3) unsigned NOT NULL default '0',
  "AAAAWRTT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGPDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRTW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGPDC" varchar(9) NOT NULL default ' ',
  "AAAAWRUA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGPDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRUE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGPDC" varchar(9) NOT NULL default ' ',
  "AAAAWRUM" varchar(1) NOT NULL default ' ',
  "AAAAWRUN" varchar(8) NOT NULL default ' ',
  "AAAAWRUJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGPDC","AAAAWRUM","AAAAWRUN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRUO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABKNQB" varchar(8) NOT NULL default ' ',
  "AAAAWRUS" varchar(8) NOT NULL default '00000000',
  "AAAAWRUT" varchar(6) NOT NULL default '000000',
  "AAAAWRUU" varchar(8) NOT NULL default '00000000',
  "AAAAWRUV" varchar(6) NOT NULL default '000000',
  "AAAAWRUW" varchar(40) NOT NULL default ' ',
  "AAAAWRUX" varchar(1) NOT NULL default ' ',
  "AAABKNPK" varchar(2) NOT NULL default ' ',
  "AAABKNPL" varchar(8) NOT NULL default ' ',
  "AAABMQVW" varchar(18) NOT NULL default ' ',
  "AAABLKLZ" varchar(30) NOT NULL default ' ',
  "AAABLKMJ" varchar(8) NOT NULL default '00000000',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAWRVJ" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABKNQB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRVO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWSCE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWSCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRVT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWSCE" varchar(1) NOT NULL default ' ',
  "AAABCDGM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWSCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRWB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNQF" varchar(3) NOT NULL default ' ',
  "AAAAWRWE" varchar(1) NOT NULL default ' ',
  "AAAAWRWF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRWI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKNQF" varchar(3) NOT NULL default ' ',
  "AAAAWRWM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKNQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRWQ" (
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABJSBY" varchar(2) NOT NULL default '00',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNLV","AAABMNLW","AAABJSBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRWY" (
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABJGDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRXB" (
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAAAWRXF" varchar(30) NOT NULL default ' ',
  "AAABMNDU" varchar(40) NOT NULL default ' ',
  "AAAAWRXH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNLV","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRXK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGOUC" varchar(10) NOT NULL default ' ',
  "AAAAWRXN" varchar(2) NOT NULL default '00',
  "AAAAWRXO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGOUC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRXR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGOUC" varchar(10) NOT NULL default ' ',
  "AAAAWRXV" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGOUC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRXZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGOUC" varchar(10) NOT NULL default ' ',
  "AAABLKLY" varchar(1) NOT NULL default ' ',
  "AAAAWRYN" varchar(1) NOT NULL default ' ',
  "AAAAWRYO" varchar(8) NOT NULL default ' ',
  "AAAAWRYP" varchar(4) NOT NULL default ' ',
  "AAAAWRYG" decimal(7,0) NOT NULL default '0',
  "AAAAWRYH" varchar(3) NOT NULL default ' ',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAABMAKT" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGOUC","AAABLKLY","AAAAWRYN","AAAAWRYO","AAAAWRYP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRYQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGOUB" varchar(10) NOT NULL default ' ',
  "AAAAWRYT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGOUB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRYW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGOUB" varchar(10) NOT NULL default ' ',
  "AAABBAWZ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGOUB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRZE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGOUB" varchar(10) NOT NULL default ' ',
  "AAABMLPO" varchar(6) NOT NULL default '000000',
  "AAAAWRZI" varchar(1) NOT NULL default ' ',
  "AAAAWRZJ" varchar(1) NOT NULL default ' ',
  "AAAAWRZK" varchar(1) NOT NULL default ' ',
  "AAAAWRZL" varchar(1) NOT NULL default ' ',
  "AAAAWRZM" varchar(1) NOT NULL default ' ',
  "AAAAWRZN" varchar(1) NOT NULL default ' ',
  "AAAAWRZO" varchar(1) NOT NULL default ' ',
  "AAAAWRZP" varchar(1) NOT NULL default ' ',
  "AAAAWRZQ" varchar(1) NOT NULL default ' ',
  "AAAAWRZR" varchar(6) NOT NULL default '000000',
  "AAAAWRZS" varchar(6) NOT NULL default '000000',
  "AAABGOUC" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGOUB","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWRZX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNPA" varchar(2) NOT NULL default ' ',
  "AAABLKLY" varchar(1) NOT NULL default ' ',
  "AAAAWSAB" varchar(2) NOT NULL default '00',
  "AAAAWSAC" varchar(3) NOT NULL default ' ',
  "AAAAWSAD" varchar(2) NOT NULL default '00',
  "AAAAWSAE" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNPA","AAABLKLY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSAI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNPA" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKNPA" varchar(2) NOT NULL default ' ',
  "AAAAWSAR" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKNPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSAV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKNPA" varchar(2) NOT NULL default ' ',
  "AAABLKLY" varchar(1) NOT NULL default ' ',
  "AAAAWSBA" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKNPA","AAABLKLY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSBF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABGIZI" varchar(3) NOT NULL default ' ',
  "AAAAWSBJ" varchar(14) NOT NULL default ' ',
  "AAAAWSBK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABGIZI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSBO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCAU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYCAU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSBT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAYCAU" varchar(1) NOT NULL default ' ',
  "AAAAWSBX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAYCAU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSCB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWSCO" varchar(10) NOT NULL default ' ',
  "AAAAWSCE" varchar(1) NOT NULL default ' ',
  "AAABLJYA" varchar(1) NOT NULL default ' ',
  "AAAAWSCG" varchar(1) NOT NULL default ' ',
  "AAAAWSCH" varchar(1) NOT NULL default ' ',
  "AAAAWSCI" varchar(1) NOT NULL default ' ',
  "AAAAWSCJ" varchar(1) NOT NULL default ' ',
  "AAAAWSCK" varchar(1) NOT NULL default ' ',
  "AAAAWSCL" varchar(1) NOT NULL default ' ',
  "AAAAWSCM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWSCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSCP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWSCU" varchar(2) NOT NULL default ' ',
  "AAABLOZE" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWSCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSCV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLLEF" varchar(10) NOT NULL default ' ',
  "AAABCDGM" varchar(30) NOT NULL default ' ',
  "AAABLJYA" varchar(1) NOT NULL default ' ',
  "AAAAWSDB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLLEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKSB" varchar(1) NOT NULL default ' ',
  "AAAAWSDI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLKSB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSDL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLKSB" varchar(1) NOT NULL default ' ',
  "AAABLKRY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLKSB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSDT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWSEF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWSEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSDY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWSEF" varchar(1) NOT NULL default ' ',
  "AAAAWSEC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWSEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSEG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGATU" varchar(4) NOT NULL default ' ',
  "AAAAWSFO" varchar(4) NOT NULL default ' ',
  "AAABHYJM" varchar(2) NOT NULL default ' ',
  "AAABHYJN" varchar(2) NOT NULL default ' ',
  "AAAAWSOO" varchar(1) NOT NULL default ' ',
  "AAAAWSEN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGATU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWSFO" varchar(4) NOT NULL default ' ',
  "AAAAWSET" varchar(1) NOT NULL default ' ',
  "AAAAWSEU" varchar(1) NOT NULL default ' ',
  "AAAAWSEV" varchar(1) NOT NULL default ' ',
  "AAAAWSEW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWSFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSEZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGATU" varchar(4) NOT NULL default ' ',
  "AAAAWSFD" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGATU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSFH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWSFO" varchar(4) NOT NULL default ' ',
  "AAAAWSFL" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWSFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSFP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUKI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSFU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAAAWSFY" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUKI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSGC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWSIV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWSIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSGH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWSGZ" varchar(1) NOT NULL default ' ',
  "AAABJVUZ" varchar(3) NOT NULL default ' ',
  "AAAAWSGL" varchar(20) NOT NULL default ' ',
  "AAAAWSGM" varchar(20) NOT NULL default ' ',
  "AAABIDCR" varchar(1) NOT NULL default ' ',
  "AAAAWSHO" varchar(3) NOT NULL default ' ',
  "AAAAWSGP" varchar(5) NOT NULL default '00000',
  "AAAAWSGQ" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAAAWSGZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSGT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWSGZ" varchar(1) NOT NULL default ' ',
  "AAABIWXK" varchar(1) NOT NULL default ' ',
  "AAABASZN" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWSGZ","AAABIWXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSHB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJVUZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJVUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSHG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJVUZ" varchar(3) NOT NULL default ' ',
  "AAAAWSHO" varchar(3) NOT NULL default ' ',
  "AAAAWSHK" varchar(3) NOT NULL default ' ',
  "AAABIDCR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJVUZ","AAAAWSHO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSHP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJVUZ" varchar(3) NOT NULL default ' ',
  "AAABIWXK" varchar(1) NOT NULL default ' ',
  "AAABASZN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJVUZ","AAABIWXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSHX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWSIM" varchar(1) NOT NULL default ' ',
  "AAAAWSIA" varchar(1) NOT NULL default ' ',
  "AAAAWSIB" varchar(3) NOT NULL default '000',
  "AAAAWSIC" varchar(3) NOT NULL default '000',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWSIM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSIG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWSIM" varchar(1) NOT NULL default ' ',
  "AAABIWXK" varchar(1) NOT NULL default ' ',
  "AAABASZN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWSIM","AAABIWXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSIO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWSIV" varchar(1) NOT NULL default ' ',
  "AAAAWSIS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWSIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIZWM" varchar(10) NOT NULL default ' ',
  "AAAAWSQL" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIZWM","AAAAWSQL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSJD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWSJO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWSJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSJI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWSJO" varchar(10) NOT NULL default ' ',
  "AAABIWXK" varchar(1) NOT NULL default ' ',
  "AAABASZN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWSJO","AAABIWXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSJQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFCZB" varchar(1) NOT NULL default ' ',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABLKXQ" varchar(4) NOT NULL default ' ',
  "AAABMKEO" varchar(1) NOT NULL default ' ',
  "AAAAWSTB" varchar(1) NOT NULL default ' ',
  "AAABFCUI" varchar(1) NOT NULL default ' ',
  "AAAAWSOQ" varchar(1) NOT NULL default ' ',
  "AAAAWSST" varchar(10) NOT NULL default ' ',
  "AAABKYEV" varchar(10) NOT NULL default ' ',
  "AAAAWSTE" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFCZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSKE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWSMJ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWSMJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSKJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWSLC" varchar(1) NOT NULL default ' ',
  "AAAAWSKM" varchar(2) NOT NULL default ' ',
  "AAAAWSKN" varchar(3) NOT NULL default ' ',
  "AAAAWSKO" varchar(3) NOT NULL default ' ',
  "AAAAWSKP" varchar(1) NOT NULL default ' ',
  "AAAAWSKQ" varchar(1) NOT NULL default ' ',
  "AAAAWSKR" varchar(1) NOT NULL default ' ',
  "AAAAWSKS" varchar(1) NOT NULL default ' ',
  "AAAAWSKT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWSLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWSLC" varchar(1) NOT NULL default ' ',
  "AAABIWXK" varchar(1) NOT NULL default ' ',
  "AAABASZN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWSLC","AAABIWXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSLE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWSMJ" varchar(12) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABEMDR" varchar(3) NOT NULL default ' ',
  "AAAAWSLJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWSMJ","AAABMLCZ","AAABEMDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSLO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEMDR" varchar(3) NOT NULL default ' ',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAAAWSLS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEMDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSLV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEMDR" varchar(3) NOT NULL default ' ',
  "AAABIWXK" varchar(1) NOT NULL default ' ',
  "AAABASZN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEMDR","AAABIWXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSMD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWSMJ" varchar(12) NOT NULL default ' ',
  "AAABIWXK" varchar(1) NOT NULL default ' ',
  "AAABASZN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWSMJ","AAABIWXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSML" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIZWM" varchar(10) NOT NULL default ' ',
  "AAAAWSMO" varchar(2) NOT NULL default ' ',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIZWM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSMS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIZWM" varchar(10) NOT NULL default ' ',
  "AAAAWSMZ" varchar(1) NOT NULL default ' ',
  "AAAAWSNA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIZWM","AAAAWSMZ","AAAAWSNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSNB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFCZB" varchar(1) NOT NULL default ' ',
  "AAABHETL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFCZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSNJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWSNV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWSNV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSNO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWSNV" varchar(10) NOT NULL default ' ',
  "AAAAWSNS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWSNV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSNW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAAAWSOF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSOC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAAAWSOF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSOI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWSPA" varchar(1) NOT NULL default ' ',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABHYJM" varchar(2) NOT NULL default ' ',
  "AAABHYJN" varchar(2) NOT NULL default ' ',
  "AAAAWSOO" varchar(1) NOT NULL default ' ',
  "AAAAWSOP" varchar(1) NOT NULL default ' ',
  "AAAAWSOQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWSPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSOT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWSPA" varchar(1) NOT NULL default ' ',
  "AAABHETL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWSPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSPB" (
  "AAABLCGT" varchar(3) NOT NULL default ' ',
  "AAAAWSPD" varchar(1) NOT NULL default ' ',
  "AAAAWSPE" varchar(1) NOT NULL default ' ',
  "AAABDTRC" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default '0000',
  "AAAAWSPH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLCGT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSPJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFCUI" varchar(1) NOT NULL default ' ',
  "AAAAWSPM" varchar(1) NOT NULL default ' ',
  "AAAAWSPN" varchar(1) NOT NULL default ' ',
  "AAAAWSPO" varchar(1) NOT NULL default ' ',
  "AAAAXKIU" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABFCUI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSPS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWSQL" varchar(20) NOT NULL default ' ',
  "AAAAWSQM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWSQL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWSQL" varchar(20) NOT NULL default ' ',
  "AAABIWXK" varchar(1) NOT NULL default ' ',
  "AAABASZN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWSQL","AAABIWXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSQG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWSQL" varchar(20) NOT NULL default ' ',
  "AAAAWSQM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWSQL","AAAAWSQM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSQN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFCUI" varchar(1) NOT NULL default ' ',
  "AAAAWSQR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFCUI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSQV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKVKH" varchar(1) NOT NULL default ' ',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABFCUI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKVKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSRC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKVKH" varchar(1) NOT NULL default ' ',
  "AAABHETL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKVKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSRK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFCZA" varchar(5) NOT NULL default ' ',
  "AAAAWSRN" varchar(40) NOT NULL default ' ',
  "AAAAWSRO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFCZA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSRR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFCZA" varchar(5) NOT NULL default ' ',
  "AAAAWSRV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFCZA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSRZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIZWL" varchar(1) NOT NULL default ' ',
  "AAAAWSSC" varchar(1) NOT NULL default ' ',
  "AAABHYJM" varchar(2) NOT NULL default ' ',
  "AAABHYJN" varchar(2) NOT NULL default ' ',
  "AAAAWSSF" varchar(1) NOT NULL default ' ',
  "AAAAWSSG" varchar(1) NOT NULL default ' ',
  "AAAAWSSH" varchar(1) NOT NULL default ' ',
  "AAAAWSSI" varchar(1) NOT NULL default ' ',
  "AAAAWSSJ" varchar(1) NOT NULL default ' ',
  "AAAAWSSK" varchar(1) NOT NULL default ' ',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAAAWSSM" varchar(1) NOT NULL default ' ',
  "AAABLKXQ" varchar(4) NOT NULL default ' ',
  "AAAAWSSO" varchar(2) NOT NULL default ' ',
  "AAAAWSSP" varchar(2) NOT NULL default ' ',
  "AAAAWSSQ" varchar(2) NOT NULL default ' ',
  "AAAAWSSR" varchar(2) NOT NULL default ' ',
  "AAAAWSSS" varchar(2) NOT NULL default ' ',
  "AAAAWSST" varchar(10) NOT NULL default ' ',
  "AAAAWSSU" varchar(1) NOT NULL default ' ',
  "AAAAWSSV" varchar(1) NOT NULL default ' ',
  "AAAAWSSW" varchar(1) NOT NULL default ' ',
  "AAAAWSSX" varchar(1) NOT NULL default ' ',
  "AAAAWSSY" varchar(1) NOT NULL default ' ',
  "AAAAWSSZ" varchar(1) NOT NULL default ' ',
  "AAAAWSTA" varchar(1) NOT NULL default ' ',
  "AAAAWSTB" varchar(1) NOT NULL default ' ',
  "AAAAWSTC" varchar(1) NOT NULL default ' ',
  "AAABKYEV" varchar(10) NOT NULL default ' ',
  "AAAAWSTE" varchar(8) NOT NULL default ' ',
  "AAAAWSTF" varchar(1) NOT NULL default ' ',
  "AAAAWSTG" varchar(1) NOT NULL default ' ',
  "AAAAWSTH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIZWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSTK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIZWL" varchar(1) NOT NULL default ' ',
  "AAABHETL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABIZWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSTS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWSUD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWSUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSTX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWSUD" varchar(1) NOT NULL default ' ',
  "AAABIWXK" varchar(1) NOT NULL default ' ',
  "AAABASZN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWSUD","AAABIWXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSUF" (
  "AAAAWSUJ" varchar(2) NOT NULL default ' ',
  "AAAAWSUK" varchar(30) NOT NULL default ' ',
  "AAAAWSUI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAWSUJ","AAAAWSUK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSUL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFWHD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFWHD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSUQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFWHD" varchar(4) NOT NULL default ' ',
  "AAAAWSUU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABFWHD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSUY" (
  "AAABLFYC" varchar(4) NOT NULL default ' ',
  "AAABLLIO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFYC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSVC" (
  "AAABFTQU" varchar(4) NOT NULL default '0000',
  "AAABLLIO" varchar(10) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFTQU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSVH" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFTQU" varchar(4) NOT NULL default '0000',
  "AAABMDXM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABFTQU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSVN" (
  "AAABFTQU" varchar(4) NOT NULL default '0000',
  "AAABLLIO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABFTQU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSVR" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFTQU" varchar(4) NOT NULL default '0000',
  "AAABMDXM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABFTQU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSVX" (
  "AAAAWSWB" varchar(30) NOT NULL default ' ',
  "AAAAWSWC" varchar(30) NOT NULL default ' ',
  "AAABCODC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAWSWB","AAAAWSWC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSWD" (
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  "AAAAWSWJ" varchar(1) NOT NULL default ' ',
  "AAABEQVP" varchar(40) NOT NULL default ' ',
  "AAAAWSWH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGTL","AAAAWSWJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSWK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWSWP" varchar(2) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWSWP","AAABHGTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSWR" (
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  "AAAAWSWT" varchar(30) NOT NULL default ' ',
  "AAAAWSWU" varchar(30) NOT NULL default ' ',
  "AAAAWSWV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSWX" (
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  "AAABMNLW" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSXB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWSXM" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWSXM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSXG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWSXM" varchar(8) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAXGDW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWSXM","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSXO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWSYA" varchar(6) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWSYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSXU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWSYA" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWSXY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWSYA","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSYC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTR" varchar(4) NOT NULL default ' ',
  "AAABJWUZ" varchar(4) NOT NULL default ' ',
  "AAAAWSYR" varchar(2) NOT NULL default ' ',
  "AAAAWSYS" varchar(2) NOT NULL default ' ',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTR","AAABJWUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSYM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTR" varchar(4) NOT NULL default ' ',
  "AAABJWUZ" varchar(4) NOT NULL default ' ',
  "AAAAWTBE" varchar(4) NOT NULL default ' ',
  "AAAAWSYR" varchar(2) NOT NULL default ' ',
  "AAAAWSYS" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTR","AAABJWUZ","AAAAWTBE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSYX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTR" varchar(4) NOT NULL default ' ',
  "AAAAWSZA" varchar(18) NOT NULL default ' ',
  "AAAAWSZB" varchar(1) NOT NULL default ' ',
  "AAABIZWL" varchar(1) NOT NULL default ' ',
  "AAAAWSZD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSZG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXTR" varchar(4) NOT NULL default ' ',
  "AAAAWSZK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXTR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWSZO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJWUZ" varchar(4) NOT NULL default ' ',
  "AAABDEHY" varchar(2) NOT NULL default ' ',
  "AAAAWSZS" varchar(5) NOT NULL default ' ',
  "AAAAWSZT" varchar(1) NOT NULL default ' ',
  "AAABMAAC" varchar(1) NOT NULL default ' ',
  "AAABMNYU" varchar(30) NOT NULL default ' ',
  "AAAAWSZW" varchar(1) NOT NULL default ' ',
  "AAABIYCS" varchar(1) NOT NULL default ' ',
  "AAABMRDE" varchar(1) NOT NULL default ' ',
  "AAABLLJR" varchar(1) NOT NULL default ' ',
  "AAABLIMF" varchar(1) NOT NULL default ' ',
  "AAAAWTAB" varchar(1) NOT NULL default ' ',
  "AAABLOBS" varchar(1) NOT NULL default ' ',
  "AAABLJJO" varchar(1) NOT NULL default ' ',
  "AAABLUVS" varchar(1) NOT NULL default ' ',
  "AAABMKAH" varchar(1) NOT NULL default ' ',
  "AAAAWTAG" varchar(1) NOT NULL default ' ',
  "AAAAWTAH" varchar(1) NOT NULL default ' ',
  "AAABKNTE" varchar(1) NOT NULL default ' ',
  "AAAAWTAJ" varchar(1) NOT NULL default ' ',
  "AAAAWTAK" varchar(1) NOT NULL default ' ',
  "AAABMCIM" varchar(1) NOT NULL default ' ',
  "AAAAWTAM" varchar(1) NOT NULL default ' ',
  "AAABMCME" varchar(1) NOT NULL default ' ',
  "AAAAWTAO" varchar(1) NOT NULL default ' ',
  "AAABMCMG" varchar(1) NOT NULL default ' ',
  "AAABMCMO" varchar(1) NOT NULL default ' ',
  "AAABMCFC" varchar(1) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAAAWTAT" varchar(1) NOT NULL default ' ',
  "AAAAWTAU" varchar(1) NOT NULL default ' ',
  "AAAAWTAV" varchar(1) NOT NULL default ' ',
  "AAAAWTAW" varchar(1) NOT NULL default ' ',
  "AAAAWTAX" varchar(1) NOT NULL default ' ',
  "AAAAWTAY" varchar(1) NOT NULL default ' ',
  "AAAAWTAZ" varchar(1) NOT NULL default ' ',
  "AAABLXTN" varchar(1) NOT NULL default ' ',
  "AAAAWTBB" varchar(1) NOT NULL default ' ',
  "AAAAWTBC" varchar(1) NOT NULL default ' ',
  "AAAAWTBD" varchar(1) NOT NULL default ' ',
  "AAAAWTBE" varchar(1) NOT NULL default ' ',
  "AAAAWTBF" varchar(1) NOT NULL default ' ',
  "AAAAWTBG" varchar(1) NOT NULL default ' ',
  "AAAAWTBH" varchar(1) NOT NULL default ' ',
  "AAAAWTBI" varchar(1) NOT NULL default ' ',
  "AAAAWTBJ" varchar(1) NOT NULL default ' ',
  "AAAAWTBK" varchar(1) NOT NULL default ' ',
  "AAABGOWR" varchar(1) NOT NULL default ' ',
  "AAABLZUS" varchar(1) NOT NULL default ' ',
  "AAAAWTBN" varchar(1) NOT NULL default ' ',
  "AAAAWTBO" varchar(1) NOT NULL default ' ',
  "AAABLUFQ" varchar(1) NOT NULL default ' ',
  "AAAAWTBQ" varchar(1) NOT NULL default ' ',
  "AAAAWTBR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJWUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTBU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJWUZ" varchar(4) NOT NULL default ' ',
  "AAAAWTBY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJWUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTCC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWTEI" varchar(1) NOT NULL default ' ',
  "AAAAWTCL" varchar(4) NOT NULL default ' ',
  "AAAAWTEJ" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWTCI" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWTEI","AAAAWTCL","AAAAWTEJ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTCO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAWTEI" varchar(1) NOT NULL default ' ',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLKLM" varchar(3) NOT NULL default ' ',
  "AAAAWTEJ" varchar(4) NOT NULL default ' ',
  "AAABMEEK" varchar(4) NOT NULL default ' ',
  "AAABMEEL" tinyint(3) unsigned NOT NULL default '0',
  "AAABLKWB" varchar(1) NOT NULL default ' ',
  "AAABMEEU" varchar(1) NOT NULL default ' ',
  "AAABMEEV" varchar(1) NOT NULL default ' ',
  "AAABMEEP" varchar(1) NOT NULL default ' ',
  "AAABMEES" varchar(68) NOT NULL default ' ',
  "AAABMEEQ" varchar(12) NOT NULL default ' ',
  "AAAAWTED" varchar(1) NOT NULL default ' ',
  "AAABLKWD" varchar(1) NOT NULL default ' ',
  "AAABIFKP" varchar(30) NOT NULL default ' ',
  "AAABIFKO" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAAAWTEI","AAABLLIM","AAABLKLM","AAAAWTEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTDN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWTEI" varchar(1) NOT NULL default ' ',
  "AAAAWTEJ" varchar(4) NOT NULL default ' ',
  "AAABJTGD" varchar(30) NOT NULL default ' ',
  "AAAAWTEL" varchar(10) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABEFTK" varchar(18) NOT NULL default ' ',
  "AAABMEEK" varchar(4) NOT NULL default ' ',
  "AAABMEEL" tinyint(3) unsigned NOT NULL default '0',
  "AAABMEEU" varchar(1) NOT NULL default ' ',
  "AAABLKWB" varchar(1) NOT NULL default ' ',
  "AAABMEEV" varchar(1) NOT NULL default ' ',
  "AAABMEEP" varchar(1) NOT NULL default ' ',
  "AAABMEES" varchar(68) NOT NULL default ' ',
  "AAABMEEQ" varchar(12) NOT NULL default ' ',
  "AAAAWTED" varchar(1) NOT NULL default ' ',
  "AAABLKWD" varchar(1) NOT NULL default ' ',
  "AAABIFKP" varchar(30) NOT NULL default ' ',
  "AAABIFKO" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWTEI","AAAAWTEJ","AAABJTGD","AAAAWTEL","AAABMCVI","AAABEFTK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTEO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAAAWTFH" varchar(2) NOT NULL default ' ',
  "AAAAWTFI" varchar(30) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABKOWG" varchar(8) NOT NULL default ' ',
  "AAAAWTEV" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABDOSI" varchar(8) NOT NULL default ' ',
  "AAABDOSJ" varchar(2) NOT NULL default ' ',
  "AAABKQWU" varchar(1) NOT NULL default ' ',
  "AAAAWTFA" varchar(2) NOT NULL default ' ',
  "AAABJJJN" varchar(2) NOT NULL default ' ',
  "AAABJJJO" varchar(30) NOT NULL default ' ',
  "AAAAWTFD" varchar(18) NOT NULL default ' ',
  "AAABKNQF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAAAWTFH","AAAAWTFI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTFJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWTHC" varchar(1) NOT NULL default ' ',
  "AAAAWTFM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWTHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTFP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWTGB" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWTGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTFU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWTGB" varchar(20) NOT NULL default ' ',
  "AAABLUDG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWTGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTGC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAAAWTGF" varchar(1) NOT NULL default ' ',
  "AAABKQLH" varchar(3) NOT NULL default ' ',
  "AAABKQJS" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAAAWTGN" varchar(3) NOT NULL default '000',
  "AAAAWTGO" varchar(1) NOT NULL default ' ',
  "AAAAWTGP" varchar(1) NOT NULL default ' ',
  "AAAAWTGQ" varchar(1) NOT NULL default ' ',
  "AAAAWTGR" varchar(3) NOT NULL default '000',
  "AAAAWTGS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTGV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWTHC" varchar(1) NOT NULL default ' ',
  "AAAAWTGY" varchar(1) NOT NULL default ' ',
  "AAAAWTGZ" varchar(1) NOT NULL default ' ',
  "AAAAWTHA" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAAAWTHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTHD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFCUI" varchar(2) NOT NULL default ' ',
  "AAABLUDG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFCUI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTHL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABKQWU" varchar(1) NOT NULL default ' ',
  "AAAAWTHQ" varchar(4) NOT NULL default ' ',
  "AAABDPHY" varchar(12) NOT NULL default ' ',
  "AAABDPHZ" varchar(1) NOT NULL default ' ',
  "AAAAWTHT" varchar(1) NOT NULL default ' ',
  "AAABFKXE" varchar(3) NOT NULL default ' ',
  "AAAAWTHV" varchar(1) NOT NULL default ' ',
  "AAABDTOD" varchar(4) NOT NULL default ' ',
  "AAABDJGL" varchar(2) NOT NULL default ' ',
  "AAABKQPE" varchar(2) NOT NULL default '00',
  "AAABKNTF" varchar(4) NOT NULL default ' ',
  "AAABKNTG" varchar(4) NOT NULL default ' ',
  "AAAAWTIB" varchar(1) NOT NULL default ' ',
  "AAAAWTIC" varchar(1) NOT NULL default ' ',
  "AAAAWTID" varchar(1) NOT NULL default ' ',
  "AAABKOUS" varchar(6) NOT NULL default ' ',
  "AAABKRGH" varchar(6) NOT NULL default ' ',
  "AAABDJGN" varchar(1) NOT NULL default ' ',
  "AAAAWTIH" varchar(1) NOT NULL default ' ',
  "AAABDOMN" varchar(4) NOT NULL default ' ',
  "AAABDOLV" varchar(1) NOT NULL default ' ',
  "AAABKODZ" varchar(2) NOT NULL default ' ',
  "AAAAWTIL" varchar(1) NOT NULL default ' ',
  "AAABDJGO" varchar(1) NOT NULL default ' ',
  "AAAAWTIN" varchar(1) NOT NULL default ' ',
  "AAAAWTIO" varchar(1) NOT NULL default ' ',
  "AAAAWTIP" varchar(1) NOT NULL default ' ',
  "AAAAWTIQ" varchar(1) NOT NULL default ' ',
  "AAAAWTIR" varchar(1) NOT NULL default ' ',
  "AAABGJBT" varchar(1) NOT NULL default ' ',
  "AAAAWTIT" varchar(7) NOT NULL default ' ',
  "AAABGJBU" varchar(1) NOT NULL default ' ',
  "AAABDTRQ" varchar(1) NOT NULL default ' ',
  "AAABCVSP" varchar(1) NOT NULL default ' ',
  "AAAAWTIX" varchar(3) NOT NULL default ' ',
  "AAAAWTIY" varchar(3) NOT NULL default ' ',
  "AAABKNUG" varchar(1) NOT NULL default ' ',
  "AAABHQRM" varchar(2) NOT NULL default ' ',
  "AAAAWTJB" varchar(1) NOT NULL default ' ',
  "AAABKNVL" varchar(6) NOT NULL default ' ',
  "AAAAWTJD" varchar(1) NOT NULL default ' ',
  "AAAAWTJE" varchar(1) NOT NULL default ' ',
  "AAAAWTJF" varchar(1) NOT NULL default ' ',
  "AAAAWTJG" varchar(1) NOT NULL default ' ',
  "AAAAWTJH" varchar(1) NOT NULL default ' ',
  "AAAAWTJI" varchar(1) NOT NULL default ' ',
  "AAAAWTJJ" varchar(1) NOT NULL default ' ',
  "AAAAWTJK" varchar(1) NOT NULL default ' ',
  "AAABKRSH" varchar(1) NOT NULL default ' ',
  "AAABDQBD" varchar(6) NOT NULL default ' ',
  "AAAAWTJN" varchar(8) NOT NULL default ' ',
  "AAAAWTJO" varchar(1) NOT NULL default ' ',
  "AAAAWTJP" varchar(1) NOT NULL default ' ',
  "AAAAWTJQ" varchar(1) NOT NULL default ' ',
  "AAAAWTJR" varchar(1) NOT NULL default ' ',
  "AAAAWTJS" varchar(1) NOT NULL default ' ',
  "AAABKNVZ" varchar(1) NOT NULL default ' ',
  "AAAAWTJU" varchar(1) NOT NULL default ' ',
  "AAAAWTJV" varchar(1) NOT NULL default ' ',
  "AAABBMSB" varchar(1) NOT NULL default ' ',
  "AAAAWTJX" varchar(1) NOT NULL default ' ',
  "AAAAWTJY" varchar(2) NOT NULL default '00',
  "AAABKOVL" varchar(1) NOT NULL default ' ',
  "AAAAWTKA" varchar(1) NOT NULL default ' ',
  "AAAAWTKB" varchar(6) NOT NULL default ' ',
  "AAAAWTKC" varchar(6) NOT NULL default ' ',
  "AAAAWTKD" varchar(6) NOT NULL default ' ',
  "AAAAWTKE" varchar(1) NOT NULL default ' ',
  "AAAAWTKF" varchar(6) NOT NULL default ' ',
  "AAAAWTKG" varchar(1) NOT NULL default ' ',
  "AAAAWTKH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABMLHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTKL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQPZ" varchar(4) NOT NULL default ' ',
  "AAABKAFO" varchar(61) NOT NULL default ' ',
  "AAABANVS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQPZ","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKAFO" varchar(61) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTKY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLQPZ" varchar(4) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLQPZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTLG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWTLU" varchar(4) NOT NULL default ' ',
  "AAABLLYZ" decimal(3,0) NOT NULL default '0',
  "AAABLLZA" decimal(3,0) NOT NULL default '0',
  "AAABLLZB" decimal(3,0) NOT NULL default '0',
  "AAABLXPF" decimal(3,1) NOT NULL default '0.0',
  "AAABLXPE" decimal(3,1) NOT NULL default '0.0',
  "AAABLXPD" varchar(1) NOT NULL default ' ',
  "AAAAXMWI" varchar(2) NOT NULL default ' ',
  "AAAAWTLQ" varchar(1) NOT NULL default ' ',
  "AAABLSGV" decimal(3,1) NOT NULL default '0.0',
  "AAAAWTLS" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAAAWTLU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTLV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOZE" varchar(1) NOT NULL default ' ',
  "AAABDBZB" varchar(18) NOT NULL default ' ',
  "AAABHQFG" varchar(7) NOT NULL default ' ',
  "AAAAWTMA" varchar(1) NOT NULL default ' ',
  "AAAAWTMB" varchar(1) NOT NULL default ' ',
  "AAAAWTMC" varchar(1) NOT NULL default ' ',
  "AAAAWTMD" varchar(1) NOT NULL default ' ',
  "AAAAWTME" varchar(1) NOT NULL default ' ',
  "AAAAWTMF" varchar(1) NOT NULL default ' ',
  "AAAAWTMG" varchar(1) NOT NULL default ' ',
  "AAAAWTMH" varchar(1) NOT NULL default ' ',
  "AAAAWTMI" varchar(1) NOT NULL default ' ',
  "AAAAWTMJ" varchar(1) NOT NULL default ' ',
  "AAAAWTMK" varchar(1) NOT NULL default ' ',
  "AAAAWTML" varchar(1) NOT NULL default ' ',
  "AAAAWTMM" varchar(1) NOT NULL default ' ',
  "AAAAWTMN" varchar(1) NOT NULL default ' ',
  "AAAAWTMO" varchar(1) NOT NULL default ' ',
  "AAAAWTMP" varchar(1) NOT NULL default ' ',
  "AAAAWTMQ" varchar(1) NOT NULL default ' ',
  "AAAAWTMR" varchar(1) NOT NULL default ' ',
  "AAAAWTMS" varchar(1) NOT NULL default ' ',
  "AAAAWTMT" varchar(1) NOT NULL default ' ',
  "AAAAWTMU" varchar(1) NOT NULL default ' ',
  "AAAAWTMV" varchar(1) NOT NULL default ' ',
  "AAAAWTMW" varchar(1) NOT NULL default ' ',
  "AAAAWTMX" varchar(1) NOT NULL default ' ',
  "AAAAWTMY" varchar(1) NOT NULL default ' ',
  "AAAAWTMZ" varchar(1) NOT NULL default ' ',
  "AAAAWTNA" varchar(1) NOT NULL default ' ',
  "AAAAWTNB" varchar(1) NOT NULL default ' ',
  "AAAAWTNC" varchar(1) NOT NULL default ' ',
  "AAAAWTND" varchar(1) NOT NULL default ' ',
  "AAAAWTNE" varchar(1) NOT NULL default ' ',
  "AAAAWTNF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOZE","AAABDBZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTNJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLOZE" varchar(1) NOT NULL default ' ',
  "AAABDBZB" varchar(18) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLOZE","AAABDBZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTNT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEMIH" varchar(3) NOT NULL default ' ',
  "AAABGOCQ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEMIH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTNZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEMIH" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABEMIH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTOH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWTOW" varchar(3) NOT NULL default ' ',
  "AAAAWTOK" varchar(1) NOT NULL default ' ',
  "AAAAWTOL" varchar(1) NOT NULL default ' ',
  "AAAAWTOM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWTOW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTOP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWTOW" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWTOW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTOX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRQP" varchar(1) NOT NULL default ' ',
  "AAAAWTPA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRQP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKRQP" varchar(1) NOT NULL default ' ',
  "AAABHMSB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKRQP"),
  KEY "AAAAWTPL" ("AAABMRIL","AAABKRQP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTPO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLURO" varchar(2) NOT NULL default ' ',
  "AAAAWTPR" varchar(1) NOT NULL default ' ',
  "AAAAWTPS" varchar(1) NOT NULL default ' ',
  "AAAAWTPT" varchar(1) NOT NULL default ' ',
  "AAAAWTPU" varchar(1) NOT NULL default ' ',
  "AAAAWTPV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLURO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLURO" varchar(2) NOT NULL default ' ',
  "AAAAWTQC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLURO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTQG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABKQVZ" varchar(1) NOT NULL default ' ',
  "AAABLRYP" varchar(8) NOT NULL default '00000000',
  "AAABKQOS" varchar(12) NOT NULL default ' ',
  "AAABKRTQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABMCVI","AAABKQVZ","AAABLRYP","AAABKQOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTQU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABKQVZ" varchar(1) NOT NULL default ' ',
  "AAABMBZP" varchar(3) NOT NULL default ' ',
  "AAABKRTQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABKQVZ","AAABMBZP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTRE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKQVZ" varchar(1) NOT NULL default ' ',
  "AAAAWTRM" varchar(4) NOT NULL default ' ',
  "AAAAWTRN" varchar(4) NOT NULL default ' ',
  "AAAAWTRJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKQVZ","AAAAWTRM","AAAAWTRN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTRO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNSA" varchar(2) NOT NULL default '00',
  "AAAAWTRR" varchar(1) NOT NULL default ' ',
  "AAAAWTRS" varchar(1) NOT NULL default ' ',
  "AAAAWTRT" varchar(1) NOT NULL default ' ',
  "AAAAWTRU" varchar(1) NOT NULL default ' ',
  "AAAAWTRV" varchar(1) NOT NULL default ' ',
  "AAAAWTRW" varchar(1) NOT NULL default ' ',
  "AAAAWTRX" varchar(1) NOT NULL default ' ',
  "AAAAWTRY" varchar(2) NOT NULL default ' ',
  "AAABHJJK" varchar(1) NOT NULL default ' ',
  "AAABGKPS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTSD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKNSA" varchar(2) NOT NULL default '00',
  "AAAAZNRP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKNSA"),
  KEY "AAAAWTSL" ("AAABMRIL","AAABKNSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTSO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKQVZ" varchar(1) NOT NULL default ' ',
  "AAAAWTSR" varchar(1) NOT NULL default ' ',
  "AAAAWTSS" varchar(1) NOT NULL default ' ',
  "AAAAWTST" varchar(1) NOT NULL default ' ',
  "AAAAWTSU" varchar(1) NOT NULL default ' ',
  "AAAAWTSV" varchar(1) NOT NULL default ' ',
  "AAAAWTSW" varchar(1) NOT NULL default ' ',
  "AAAAWTSX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKQVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTTA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWTTM" varchar(1) NOT NULL default ' ',
  "AAAAWTTN" varchar(1) NOT NULL default ' ',
  "AAABDPJE" varchar(1) NOT NULL default ' ',
  "AAABDPJC" varchar(1) NOT NULL default ' ',
  "AAABKRPW" varchar(1) NOT NULL default ' ',
  "AAAAWTUC" varchar(1) NOT NULL default ' ',
  "AAABDPJG" varchar(1) NOT NULL default ' ',
  "AAABKRQP" varchar(1) NOT NULL default ' ',
  "AAABHPNU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWTTM","AAAAWTTN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTTO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKQVZ" varchar(1) NOT NULL default ' ',
  "AAAAYBBL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKQVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTTW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKQVZ" varchar(1) NOT NULL default ' ',
  "AAABDPJE" varchar(1) NOT NULL default ' ',
  "AAABDPJC" varchar(1) NOT NULL default ' ',
  "AAABKRPW" varchar(1) NOT NULL default ' ',
  "AAAAWTUC" varchar(1) NOT NULL default ' ',
  "AAABDPJG" varchar(1) NOT NULL default ' ',
  "AAABKRQP" varchar(1) NOT NULL default ' ',
  "AAABHPNU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKQVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTUI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRQO" varchar(1) NOT NULL default ' ',
  "AAAAWTUL" varchar(1) NOT NULL default ' ',
  "AAAAWTUM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRQO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTUP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKRQO" varchar(1) NOT NULL default ' ',
  "AAABJBVB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKRQO"),
  KEY "AAAAWTUX" ("AAABMRIL","AAABKRQO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTVA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRPC" varchar(1) NOT NULL default ' ',
  "AAAAWTVD" varchar(1) NOT NULL default ' ',
  "AAAAWTVE" varchar(1) NOT NULL default ' ',
  "AAAAWTVF" varchar(1) NOT NULL default ' ',
  "AAAAWTVG" varchar(1) NOT NULL default ' ',
  "AAAAWTVH" varchar(1) NOT NULL default ' ',
  "AAAAWTVI" varchar(1) NOT NULL default ' ',
  "AAAAWTVJ" varchar(1) NOT NULL default ' ',
  "AAAAWTVK" varchar(1) NOT NULL default ' ',
  "AAAAWTVL" varchar(1) NOT NULL default ' ',
  "AAAAWTVM" varchar(4) NOT NULL default ' ',
  "AAABKRTF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRPC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTVQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWTVX" varchar(10) NOT NULL default ' ',
  "AAAAWTVT" varchar(21) NOT NULL default ' ',
  "AAAAWTVU" varchar(21) NOT NULL default ' ',
  "AAAAWTVV" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWTVX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTVY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREO" varchar(2) NOT NULL default ' ',
  "AAABFTSA" varchar(65) NOT NULL default ' ',
  "AAAAWTWC" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTWF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREO" varchar(2) NOT NULL default ' ',
  "AAABLWJT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREO"),
  KEY "AAAAWTWN" ("AAABMRIL","AAABMREO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTWQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKRPC" varchar(1) NOT NULL default ' ',
  "AAABIMUR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKRPC"),
  KEY "AAAAWTWY" ("AAABMRIL","AAABKRPC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTXB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCT" varchar(4) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABKRPC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCT","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTXJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDDUJ" varchar(2) NOT NULL default '00',
  "AAAAWTXM" varchar(1) NOT NULL default ' ',
  "AAAAWTXN" varchar(1) NOT NULL default ' ',
  "AAABHJJK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDDUJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTXR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDDUJ" varchar(2) NOT NULL default '00',
  "AAAAWTXV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDDUJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTXZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABKODS" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABKODS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTYG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABKODS" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCVI","AAABKODS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTYQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABKODM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABKODM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTYX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABKODM" varchar(2) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCVI","AAABKODM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTZH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABKODO" varchar(10) NOT NULL default ' ',
  "AAABKODP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABKODO","AAABKODP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWTZQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABKODO" varchar(10) NOT NULL default ' ',
  "AAABKODP" varchar(10) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCVI","AAABKODO","AAABKODP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWUAC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABKODO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABKODO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWUAJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABKODO" varchar(10) NOT NULL default ' ',
  "AAABLUDG" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCVI","AAABKODO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWUAT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABKODQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABKODQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWUBA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABKODQ" varchar(2) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCVI","AAABKODQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWUBK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABKODF" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABKODF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWUBR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABKODF" varchar(5) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCVI","AAABKODF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWUCB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRBW" varchar(1) NOT NULL default ' ',
  "AAABKOCM" varchar(4) NOT NULL default ' ',
  "AAABHBRF" varchar(1) NOT NULL default ' ',
  "AAAAWUCG" varchar(1) NOT NULL default ' ',
  "AAAAWUCH" varchar(1) NOT NULL default ' ',
  "AAAAWUCI" varchar(70) NOT NULL default ' ',
  "AAABGKQQ" varchar(1) NOT NULL default ' ',
  "AAAAWUCK" varchar(1) NOT NULL default ' ',
  "AAABDQWW" varchar(1) NOT NULL default ' ',
  "AAAAWUCM" varchar(1) NOT NULL default ' ',
  "AAAAWUCN" varchar(1) NOT NULL default ' ',
  "AAAAWUCO" varchar(1) NOT NULL default ' ',
  "AAAAWUCP" varchar(1) NOT NULL default ' ',
  "AAAAWUCQ" varchar(1) NOT NULL default ' ',
  "AAAAWUCR" varchar(1) NOT NULL default ' ',
  "AAABDPCM" varchar(1) NOT NULL default ' ',
  "AAABLGEF" varchar(1) NOT NULL default ' ',
  "AAAAWUCU" varchar(1) NOT NULL default ' ',
  "AAAAWUCV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRBW","AAABKOCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWUCZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKRBW" varchar(1) NOT NULL default ' ',
  "AAABKOCM" varchar(4) NOT NULL default ' ',
  "AAABLUDG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKRBW","AAABKOCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWUDJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKQOV" varchar(5) NOT NULL default ' ',
  "AAAAWUDM" varchar(1) NOT NULL default ' ',
  "AAABKPVV" varchar(2) NOT NULL default ' ',
  "AAABKPXJ" varchar(2) NOT NULL default ' ',
  "AAAAWUDP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKQOV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWUDS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKQOV" varchar(5) NOT NULL default ' ',
  "AAABBCMQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKQOV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWUEA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAAAWUKU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAAAWUKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWUEH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAAAWUKU" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCVI","AAAAWUKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWUER" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAAAWUKU" varchar(4) NOT NULL default ' ',
  "AAABHOTF" varchar(4) NOT NULL default ' ',
  "AAAAWUEW" varchar(1) NOT NULL default ' ',
  "AAAAWUEX" varchar(1) NOT NULL default ' ',
  "AAAAWUEY" varchar(1) NOT NULL default ' ',
  "AAAAWUEZ" varchar(1) NOT NULL default ' ',
  "AAAAWUFA" varchar(1) NOT NULL default ' ',
  "AAAAWUFB" varchar(1) NOT NULL default ' ',
  "AAAAWUFC" varchar(1) NOT NULL default ' ',
  "AAAAWUFD" varchar(3) NOT NULL default '000',
  "AAAAWUFE" varchar(3) NOT NULL default '000',
  "AAABAFTV" varchar(1) NOT NULL default ' ',
  "AAAAWUFG" varchar(1) NOT NULL default ' ',
  "AAAAWUFH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAAAWUKU","AAABHOTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWUFM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAAAWUKU" varchar(4) NOT NULL default ' ',
  "AAABHOTF" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAAAWUKU","AAABHOTF","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWUFY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAAAWUKU" varchar(4) NOT NULL default ' ',
  "AAABHOTF" varchar(4) NOT NULL default ' ',
  "AAABJJLP" varchar(3) NOT NULL default '000',
  "AAAAWUGE" varchar(1) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAAAWUGG" varchar(3) NOT NULL default '000',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMCVW" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAAAWUKU","AAABHOTF","AAABJJLP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWUGO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHOTF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHOTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWUGT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHOTF" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHOTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWUHB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWUII" varchar(7) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWUII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWUHG" (
  "AAAAWUIA" varchar(8) NOT NULL default ' ',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAAAWUHT" varchar(1) NOT NULL default ' ',
  "AAABLEST" varchar(4) NOT NULL default ' ',
  "AAAAWUHM" varchar(1) NOT NULL default ' ',
  "AAAAWUHN" varchar(1) NOT NULL default ' ',
  "AAAAWUHO" varchar(1) NOT NULL default ' ',
  "AAAAWUHP" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAAWUIA","AAABMKIQ","AAABMKXW","AAAAWUHT","AAABLEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWUHV" (
  "AAAAWUIA" varchar(8) NOT NULL default ' ',
  "AAABDVBU" varchar(40) NOT NULL default ' ',
  "AAABDVBX" varchar(30) NOT NULL default ' ',
  "AAABDVBY" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAWUIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWUIB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWUII" varchar(7) NOT NULL default ' ',
  "AAABLUDG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWUII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWUIJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABGIQU" varchar(3) NOT NULL default ' ',
  "AAAAWUIO" varchar(3) NOT NULL default ' ',
  "AAABHOTF" varchar(4) NOT NULL default ' ',
  "AAAAWUIQ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWUIT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABKNSO" varchar(3) NOT NULL default ' ',
  "AAAAWUIX" varchar(3) NOT NULL default '000',
  "AAABKNSW" varchar(3) NOT NULL default '000',
  "AAABKNSV" varchar(3) NOT NULL default '000',
  "AAABKNSX" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABKNSO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWUJE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWUJG" varchar(1) NOT NULL default ' ',
  "AAAAWUJH" varchar(1) NOT NULL default ' ',
  "AAAAWUJI" varchar(1) NOT NULL default ' ',
  "AAABHANX" varchar(1) NOT NULL default ' ',
  "AAAAWUJK" decimal(3,0) NOT NULL default '0',
  "AAABIFZC" decimal(3,0) NOT NULL default '0',
  "AAAAWUJM" varchar(1) NOT NULL default ' ',
  "AAABIFZV" varchar(1) NOT NULL default ' ',
  "AAABIFZW" varchar(1) NOT NULL default ' ',
  "AAABIFZX" varchar(1) NOT NULL default ' ',
  "AAABIFZY" varchar(1) NOT NULL default ' ',
  "AAABIFZZ" varchar(1) NOT NULL default ' ',
  "AAABIGAE" varchar(1) NOT NULL default ' ',
  "AAABIGAF" varchar(1) NOT NULL default ' ',
  "AAABIGAH" varchar(1) NOT NULL default ' ',
  "AAAAWUJV" decimal(3,0) NOT NULL default '0',
  "AAAAWUJW" decimal(3,0) NOT NULL default '0',
  "AAAAWUJX" varchar(1) NOT NULL default ' ',
  "AAABIFZT" varchar(1) NOT NULL default ' ',
  "AAABIGAA" varchar(1) NOT NULL default ' ',
  "AAABIGAB" varchar(1) NOT NULL default ' ',
  "AAABIGAC" varchar(1) NOT NULL default ' ',
  "AAABIGAD" varchar(1) NOT NULL default ' ',
  "AAABIFZU" varchar(1) NOT NULL default ' ',
  "AAABIGAJ" varchar(1) NOT NULL default ' ',
  "AAABIFZA" varchar(1) NOT NULL default ' ',
  "AAABIGAN" varchar(1) NOT NULL default ' ',
  "AAABIFYZ" varchar(1) NOT NULL default ' ',
  "AAABIGAT" varchar(1) NOT NULL default ' ',
  "AAAAWUKJ" varchar(1) NOT NULL default ' ',
  "AAAAWUKK" varchar(1) NOT NULL default ' ',
  "AAAAWUKL" varchar(1) NOT NULL default ' ',
  "AAAAWUKM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWUKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWUKU" varchar(4) NOT NULL default ' ',
  "AAAAWUKV" varchar(2) NOT NULL default '00',
  "AAAAWUKS" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWUKU","AAAAWUKV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWUKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABKQWF" varchar(4) NOT NULL default ' ',
  "AAABFSDE" varchar(2) NOT NULL default '00',
  "AAAAWULC" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABMCVI","AAABKQWF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWULH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAAAZGQY" varchar(8) NOT NULL default ' ',
  "AAAAWULL" varchar(20) NOT NULL default ' ',
  "AAAAWULM" varchar(3) NOT NULL default ' ',
  "AAABLCKD" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAAAZGQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWULR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWUOI" varchar(4) NOT NULL default ' ',
  "AAAAWULV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWUOI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--enable_query_log
