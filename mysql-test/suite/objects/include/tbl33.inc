# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+
# |                             tbl33.inc                              |
# |                        Copyright 2006 by MySQL AB                  |
# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+

--disable_query_log
CREATE TABLE "AAABCDYV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLIDX" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLIDX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDZD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHEK" varchar(4) NOT NULL default ' ',
  "AAABCDZG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHEK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDZJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLHEK" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLHEK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDZR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCEAJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCEAJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDZW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCEAJ" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCEAJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCEAE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCEAJ" varchar(4) NOT NULL default ' ',
  "AAABLHEK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCEAJ","AAABLHEK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCEAL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEFVF" varchar(4) NOT NULL default ' ',
  "AAABLIDX" varchar(4) NOT NULL default ' ',
  "AAABCEAP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEFVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCEAS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEFVF" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABEFVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCEBA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default ' ',
  "AAABCEBE" varchar(1) NOT NULL default ' ',
  "AAABCEBF" varchar(1) NOT NULL default ' ',
  "AAABCEBG" varchar(1) NOT NULL default ' ',
  "AAABCEBH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJBH","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCEBL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABLHFA" varchar(4) NOT NULL default ' ',
  "AAABLHFY" varchar(4) NOT NULL default ' ',
  "AAABCEBQ" varchar(1) NOT NULL default ' ',
  "AAABCEBR" varchar(1) NOT NULL default ' ',
  "AAABCEBS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJBH","AAABLHFA","AAABLHFY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCEBX" (
  "AAABMLPX" varchar(40) NOT NULL default ' ',
  "AAABCECU" varchar(1) NOT NULL default ' ',
  "AAABCECA" varchar(2) NOT NULL default '00',
  "AAABLPSU" varchar(35) NOT NULL default ' ',
  "AAABCECC" varchar(8) NOT NULL default ' ',
  "AAABMRCK" varchar(8) NOT NULL default ' ',
  "AAABCECE" varchar(10) NOT NULL default ' ',
  "AAABCECF" varchar(10) NOT NULL default ' ',
  "AAABCECG" varchar(40) NOT NULL default ' ',
  "AAABLUUW" varchar(1) NOT NULL default ' ',
  "AAABIJNY" varchar(1) NOT NULL default ' ',
  "AAABLUER" varchar(24) NOT NULL default ' ',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABIHMM" varchar(1) NOT NULL default ' ',
  "AAABCECM" varchar(6) NOT NULL default '000000',
  "AAABCECN" varchar(8) NOT NULL default '00000000',
  "AAABCECO" varchar(12) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABIFVP" varchar(10) NOT NULL default ' ',
  "AAABCECR" varchar(1) NOT NULL default ' ',
  "AAABMADG" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPX","AAABCECU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCECV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABMLZS" int(11) NOT NULL default '0',
  "AAABLRTV" varchar(8) NOT NULL default '00000000',
  "AAABLRTW" varchar(6) NOT NULL default '000000',
  "AAABCEDB" varchar(12) NOT NULL default ' ',
  "AAABCPQI" varchar(1) NOT NULL default ' ',
  "AAABHDHE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOIA","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCEDH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABMADG" varchar(14) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABCEDM" varchar(40) NOT NULL default ' ',
  "AAABCEDN" varchar(1) NOT NULL default ' ',
  "AAABILGV" varchar(8) NOT NULL default '00000000',
  "AAABILGW" varchar(6) NOT NULL default '000000',
  "AAABCEDQ" varchar(12) NOT NULL default ' ',
  "AAABCEDR" varchar(1) NOT NULL default ' ',
  "AAABCEDS" varchar(8) NOT NULL default '00000000',
  "AAABCEDT" varchar(6) NOT NULL default '000000',
  "AAABCEDU" varchar(12) NOT NULL default ' ',
  "AAABCEDV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOIA","AAABMADG","AAABMGHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCEEA" (
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABMPMH" varchar(10) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOIA","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCEEG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABMADG" varchar(14) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABHIOL" varchar(30) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABMLZS" varchar(6) NOT NULL default '000000',
  "AAABLDNU" varchar(4) NOT NULL default ' ',
  "AAABJTEQ" varchar(6) NOT NULL default '000000',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(255) NOT NULL default ' ',
  "AAABMNNA" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOIA","AAABMADG","AAABMGHE","AAABHIOL","AAABMKXW","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCEFB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABMADG" varchar(14) NOT NULL default ' ',
  "AAABMLPV" varchar(8) NOT NULL default ' ',
  "AAABMNLA" varchar(1) NOT NULL default ' ',
  "AAABJHFV" varchar(1) NOT NULL default ' ',
  "AAABCEFI" varchar(1) NOT NULL default ' ',
  "AAABMCWV" varchar(30) NOT NULL default ' ',
  "AAABJFIP" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  "AAABMFSI" varchar(1) NOT NULL default ' ',
  "AAABCEFN" varchar(4) NOT NULL default ' ',
  "AAABCEFO" varchar(1) NOT NULL default ' ',
  "AAABJHFC" varchar(1) NOT NULL default ' ',
  "AAABGJXX" varchar(1) NOT NULL default ' ',
  "AAABCEFR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOIA","AAABMADG","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCEFW" (
  "AAABCEGA" varchar(10) NOT NULL default ' ',
  "AAABHYAG" varchar(10) NOT NULL default ' ',
  "AAABCEFZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABCEGA","AAABHYAG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCEGC" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMLTD" decimal(5,0) NOT NULL default '0',
  "AAABIIRR" varchar(79) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCEGI" (
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABHPJW" varchar(4) NOT NULL default ' ',
  "AAABHJFJ" varchar(4) NOT NULL default '0000',
  "AAABHPJO" varchar(4) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABCFGN" varchar(5) NOT NULL default '00000',
  "AAABHPJR" int(11) NOT NULL default '0',
  "AAABHPJS" varchar(8) NOT NULL default ' ',
  "AAABCFGQ" varchar(4) NOT NULL default '0000',
  "AAABCFGR" double NOT NULL default '0',
  "AAABCFGS" double NOT NULL default '0',
  "AAABCFGT" decimal(8,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABHPJU","AAABHPJV","AAABHPJW","AAABHJFJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCEHA" (
  "AAABHMXI" varchar(5) NOT NULL default '00000',
  "AAABHMXJ" varchar(5) NOT NULL default ' ',
  "AAABCFEY" varchar(5) NOT NULL default '00000',
  "AAABJSWC" double NOT NULL default '0',
  "AAABCFDS" tinyint(3) unsigned NOT NULL default '0',
  "AAABCFDT" smallint(6) NOT NULL default '0',
  "AAABCFDU" int(11) NOT NULL default '0',
  "AAABCFDV" varchar(5) NOT NULL default '00000',
  "AAABCFDW" varchar(8) NOT NULL default '00000000',
  "AAABCFDX" decimal(4,2) NOT NULL default '0.00',
  "AAABCFDY" decimal(8,2) NOT NULL default '0.00',
  "AAABCFDZ" decimal(14,7) NOT NULL default '0.0000000',
  "AAABCFEA" decimal(15,0) NOT NULL default '0',
  "AAABCFEB" varchar(8) NOT NULL default '00000000',
  "AAABCFEC" varchar(6) NOT NULL default '000000',
  "AAABCFED" varchar(1) NOT NULL default ' ',
  "AAABCFEE" varchar(20) NOT NULL default ' ',
  "AAABCFEF" varchar(33) NOT NULL default ' ',
  "AAABCFEG" VARBINARY(255),
  "AAABCFEH" varchar(226) NOT NULL default ' ',
  "AAABCFEI" varchar(254) NOT NULL default ' ',
  "AAABFWCT" smallint(6) NOT NULL default '0',
  "AAABHMLJ" text,
  PRIMARY KEY ("AAABHMXI","AAABHMXJ","AAABCFEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCEIB" (
  "AAABHMXI" varchar(5) NOT NULL default '00000',
  "AAABHMXJ" varchar(5) NOT NULL default ' ',
  "AAABCFEY" varchar(5) NOT NULL default '00000',
  "AAABJSWC" double NOT NULL default '0',
  "AAABCFDS" tinyint(3) unsigned NOT NULL default '0',
  "AAABCFDT" smallint(6) NOT NULL default '0',
  "AAABCFDU" int(11) NOT NULL default '0',
  "AAABCFDV" varchar(5) NOT NULL default '00000',
  "AAABCFDW" varchar(8) NOT NULL default '00000000',
  "AAABCFDX" decimal(4,2) NOT NULL default '0.00',
  "AAABCFDY" decimal(8,2) NOT NULL default '0.00',
  "AAABCFDZ" decimal(14,7) NOT NULL default '0.0000000',
  "AAABCFEA" decimal(15,0) NOT NULL default '0',
  "AAABCFEB" varchar(8) NOT NULL default '00000000',
  "AAABCFEC" varchar(6) NOT NULL default '000000',
  "AAABCFED" varchar(1) NOT NULL default ' ',
  "AAABCFEE" varchar(20) NOT NULL default ' ',
  "AAABCFEF" varchar(33) NOT NULL default ' ',
  "AAABCFEG" VARBINARY(255),
  "AAABCFEH" varchar(226) NOT NULL default ' ',
  "AAABCFEI" varchar(254) NOT NULL default ' ',
  "AAABFWCT" smallint(6) NOT NULL default '0',
  "AAABHMLJ" text,
  PRIMARY KEY ("AAABHMXI","AAABHMXJ","AAABCFEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCEJC" (
  "AAABCFFG" VARBINARY(255) NOT NULL,
  "AAABCFFH" VARBINARY(255) NOT NULL,
  "AAABHPJW" varchar(4) NOT NULL default ' ',
  "AAABCFFD" VARBINARY(255),
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABCFFF" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABCFFG"(255),"AAABCFFH"(255),"AAABHPJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCEJM" (
  "AAABCFFG" VARBINARY(255) NOT NULL,
  "AAABCFFH" VARBINARY(255) NOT NULL,
  "AAABHPJW" varchar(4) NOT NULL default ' ',
  "AAABCFFD" VARBINARY(255),
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABCFFF" int(11) NOT NULL default '0',
  "AAABCEZV" varchar(255) NOT NULL default ' ',
  "AAABCEZW" longtext,
  "AAABCEZX" longblob,
  "AAABCEZY" longtext,
  PRIMARY KEY ("AAABCFFG"(255),"AAABCFFH"(255),"AAABHPJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCEKA" (
  "AAABCFFX" varchar(3) NOT NULL default ' ',
  "AAABCFFY" int(11) NOT NULL default '0',
  "AAABCLOM" int(11) NOT NULL default '0',
  "AAABCFGA" varchar(30) NOT NULL default ' ',
  "AAABHPJO" varchar(4) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABCFGN" varchar(5) NOT NULL default '00000',
  "AAABHPJR" int(11) NOT NULL default '0',
  "AAABHPJS" varchar(8) NOT NULL default ' ',
  "AAABCFGQ" varchar(4) NOT NULL default '0000',
  "AAABCFGR" double NOT NULL default '0',
  "AAABCFGS" double NOT NULL default '0',
  "AAABCFGT" decimal(8,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABCFFX","AAABCFFY","AAABCLOM","AAABCFGA"),
  KEY "AAABCEKS" ("AAABHPJO","AAABHPJP","AAABCFGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCEKW" (
  "AAABCFFX" varchar(3) NOT NULL default ' ',
  "AAABCFFY" int(11) NOT NULL default '0',
  "AAABCLOM" int(11) NOT NULL default '0',
  "AAABCFGA" varchar(30) NOT NULL default ' ',
  "AAABHPJO" varchar(4) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABCFGN" varchar(5) NOT NULL default '00000',
  "AAABHPJR" int(11) NOT NULL default '0',
  "AAABHPJS" varchar(8) NOT NULL default ' ',
  "AAABCFGQ" varchar(4) NOT NULL default '0000',
  "AAABCFGR" double NOT NULL default '0',
  "AAABCFGS" double NOT NULL default '0',
  "AAABCFGT" decimal(8,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABCFFX","AAABCFFY","AAABCLOM","AAABCFGA"),
  KEY "AAABCELO" ("AAABHPJO","AAABHPJP","AAABCFGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCELS" (
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABHPJW" varchar(4) NOT NULL default ' ',
  "AAABHJFJ" varchar(4) NOT NULL default '0000',
  "AAABHPJO" varchar(4) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABCFGN" varchar(5) NOT NULL default '00000',
  "AAABHPJR" int(11) NOT NULL default '0',
  "AAABHPJS" varchar(8) NOT NULL default ' ',
  "AAABCFGQ" varchar(4) NOT NULL default '0000',
  "AAABCFGR" double NOT NULL default '0',
  "AAABCFGS" double NOT NULL default '0',
  "AAABCFGT" decimal(8,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABHPJU","AAABHPJV","AAABHPJW","AAABHJFJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCEMK" (
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABHPJW" varchar(4) NOT NULL default ' ',
  "AAABHJFJ" varchar(4) NOT NULL default '0000',
  "AAABHPJO" varchar(4) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABCFGN" varchar(5) NOT NULL default '00000',
  "AAABHPJR" int(11) NOT NULL default '0',
  "AAABHPJS" varchar(8) NOT NULL default ' ',
  "AAABCFGQ" varchar(4) NOT NULL default '0000',
  "AAABCFGR" double NOT NULL default '0',
  "AAABCFGS" double NOT NULL default '0',
  "AAABCFGT" decimal(8,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABHPJU","AAABHPJV","AAABHPJW","AAABHJFJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCENC" (
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABHPJW" varchar(1) NOT NULL default ' ',
  "AAABHJFJ" varchar(4) NOT NULL default '0000',
  "AAABHPJO" varchar(2) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABCFGN" varchar(3) NOT NULL default '000',
  "AAABHPJR" int(11) NOT NULL default '0',
  "AAABHPJS" varchar(1) NOT NULL default ' ',
  "AAABCFGQ" varchar(4) NOT NULL default '0000',
  "AAABCFGR" double NOT NULL default '0',
  "AAABCFGT" decimal(8,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABHPJU","AAABHPJV","AAABHPJW","AAABHJFJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCENT" (
  "AAABHMXI" varchar(5) NOT NULL default '00000',
  "AAABHMXJ" varchar(5) NOT NULL default ' ',
  "AAABCFEY" varchar(5) NOT NULL default '00000',
  "AAABHMXF" int(11) NOT NULL default '0',
  "AAABHMXG" varchar(200) NOT NULL default ' ',
  "AAABHMXH" double NOT NULL default '0',
  "AAABCFCI" double NOT NULL default '0',
  PRIMARY KEY ("AAABHMXI","AAABHMXJ","AAABCFEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCEOE" (
  "AAABHMXI" varchar(5) NOT NULL default '00000',
  "AAABHMXJ" varchar(5) NOT NULL default ' ',
  "AAABCFEY" varchar(3) NOT NULL default '000',
  "AAABHMXF" int(11) NOT NULL default '0',
  "AAABHMXG" varchar(200) NOT NULL default ' ',
  "AAABHMXH" double NOT NULL default '0',
  "AAABCFCI" double NOT NULL default '0',
  PRIMARY KEY ("AAABHMXI","AAABHMXJ","AAABCFEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCEOP" (
  "AAABHMXI" varchar(5) NOT NULL default '00000',
  "AAABHMXJ" varchar(5) NOT NULL default ' ',
  "AAABCFEY" varchar(5) NOT NULL default '00000',
  "AAABJSWC" double NOT NULL default '0',
  "AAABCFDS" tinyint(3) unsigned NOT NULL default '0',
  "AAABCFDT" smallint(6) NOT NULL default '0',
  "AAABCFDU" int(11) NOT NULL default '0',
  "AAABCFDV" varchar(5) NOT NULL default '00000',
  "AAABCFDW" varchar(8) NOT NULL default '00000000',
  "AAABCFDX" decimal(4,2) NOT NULL default '0.00',
  "AAABCFDY" decimal(8,2) NOT NULL default '0.00',
  "AAABCFDZ" decimal(14,7) NOT NULL default '0.0000000',
  "AAABCFEA" decimal(15,0) NOT NULL default '0',
  "AAABCFEB" varchar(8) NOT NULL default '00000000',
  "AAABCFEC" varchar(6) NOT NULL default '000000',
  "AAABCFED" varchar(1) NOT NULL default ' ',
  "AAABCFEE" varchar(20) NOT NULL default ' ',
  "AAABCFEF" varchar(33) NOT NULL default ' ',
  "AAABCFEG" VARBINARY(255),
  "AAABCFEH" varchar(226) NOT NULL default ' ',
  "AAABCFEI" varchar(254) NOT NULL default ' ',
  "AAABFWCT" smallint(6) NOT NULL default '0',
  "AAABHMLJ" text,
  PRIMARY KEY ("AAABHMXI","AAABHMXJ","AAABCFEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCEPQ" (
  "AAABHMXI" varchar(5) NOT NULL default '00000',
  "AAABHMXJ" varchar(5) NOT NULL default ' ',
  "AAABCFEY" varchar(5) NOT NULL default '00000',
  "AAABJSWC" double NOT NULL default '0',
  "AAABCFDS" tinyint(3) unsigned NOT NULL default '0',
  "AAABCFDT" smallint(6) NOT NULL default '0',
  "AAABCFDU" int(11) NOT NULL default '0',
  "AAABCFDV" varchar(5) NOT NULL default '00000',
  "AAABCFDW" varchar(8) NOT NULL default '00000000',
  "AAABCFDX" decimal(4,2) NOT NULL default '0.00',
  "AAABCFDY" decimal(8,2) NOT NULL default '0.00',
  "AAABCFDZ" decimal(14,7) NOT NULL default '0.0000000',
  "AAABCFEA" decimal(15,0) NOT NULL default '0',
  "AAABCFEB" varchar(8) NOT NULL default '00000000',
  "AAABCFEC" varchar(6) NOT NULL default '000000',
  "AAABCFED" varchar(1) NOT NULL default ' ',
  "AAABCFEE" varchar(20) NOT NULL default ' ',
  "AAABCFEF" varchar(33) NOT NULL default ' ',
  "AAABCFEG" VARBINARY(255),
  "AAABCFEH" varchar(226) NOT NULL default ' ',
  "AAABCFEI" varchar(254) NOT NULL default ' ',
  "AAABFWCT" smallint(6) NOT NULL default '0',
  "AAABHMLJ" text,
  PRIMARY KEY ("AAABHMXI","AAABHMXJ","AAABCFEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCEQR" (
  "AAABHMXI" varchar(5) NOT NULL default '00000',
  "AAABHMXJ" varchar(5) NOT NULL default ' ',
  "AAABCFEY" varchar(5) NOT NULL default '00000',
  "AAABHJSU" varchar(1) NOT NULL default ' ',
  "AAABHJSV" varchar(2) NOT NULL default ' ',
  "AAABCFEU" varchar(5) NOT NULL default ' ',
  "AAABCFEV" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABHMXI","AAABHMXJ","AAABCFEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCERC" (
  "AAABCFFG" VARBINARY(255) NOT NULL,
  "AAABCFFH" VARBINARY(255) NOT NULL,
  "AAABHPJW" varchar(4) NOT NULL default ' ',
  "AAABCFFD" VARBINARY(255),
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABCFFF" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABCFFG"(255),"AAABCFFH"(255),"AAABHPJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCERM" (
  "AAABCFFG" VARBINARY(255) NOT NULL,
  "AAABCFFH" VARBINARY(255) NOT NULL,
  "AAABHPJW" varchar(4) NOT NULL default ' ',
  "AAABCFFD" VARBINARY(255),
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABCFFF" int(11) NOT NULL default '0',
  "AAABCEZV" varchar(255) NOT NULL default ' ',
  "AAABCEZW" longtext,
  "AAABCEZX" longblob,
  "AAABCEZY" longtext,
  PRIMARY KEY ("AAABCFFG"(255),"AAABCFFH"(255),"AAABHPJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCESA" (
  "AAABCFFX" varchar(3) NOT NULL default ' ',
  "AAABCFFY" int(11) NOT NULL default '0',
  "AAABCLOM" int(11) NOT NULL default '0',
  "AAABCFGA" varchar(30) NOT NULL default ' ',
  "AAABHPJO" varchar(4) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABCFGN" varchar(5) NOT NULL default '00000',
  "AAABHPJR" int(11) NOT NULL default '0',
  "AAABHPJS" varchar(8) NOT NULL default ' ',
  "AAABCFGQ" varchar(4) NOT NULL default '0000',
  "AAABCFGR" double NOT NULL default '0',
  "AAABCFGS" double NOT NULL default '0',
  "AAABCFGT" decimal(8,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABCFFX","AAABCFFY","AAABCLOM","AAABCFGA"),
  KEY "AAABCESS" ("AAABHPJO","AAABHPJP","AAABCFGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCESW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABHPJW" varchar(4) NOT NULL default ' ',
  "AAABHJFJ" varchar(4) NOT NULL default '0000',
  "AAABHPJO" varchar(4) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABCFGN" varchar(5) NOT NULL default '00000',
  "AAABHPJR" int(11) NOT NULL default '0',
  "AAABHPJS" varchar(8) NOT NULL default ' ',
  "AAABCFGQ" varchar(4) NOT NULL default '0000',
  "AAABCFGR" double NOT NULL default '0',
  "AAABCFGS" double NOT NULL default '0',
  "AAABCFGT" decimal(8,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHPJU","AAABHPJV","AAABHPJW","AAABHJFJ"),
  KEY "AAABCETQ" ("AAABHPJO","AAABHPJP","AAABCFGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCETU" (
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABHPJW" varchar(4) NOT NULL default ' ',
  "AAABHJFJ" varchar(4) NOT NULL default '0000',
  "AAABHPJO" varchar(4) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABCFGN" varchar(5) NOT NULL default '00000',
  "AAABHPJR" int(11) NOT NULL default '0',
  "AAABHPJS" varchar(8) NOT NULL default ' ',
  "AAABCFGQ" varchar(4) NOT NULL default '0000',
  "AAABCFGR" double NOT NULL default '0',
  "AAABCFGS" double NOT NULL default '0',
  "AAABCFGT" decimal(8,2) NOT NULL default '0.00',
  "AAABCEUI" smallint(6) NOT NULL default '0',
  "AAABCEUJ" blob,
  PRIMARY KEY ("AAABHPJU","AAABHPJV","AAABHPJW","AAABHJFJ"),
  KEY "AAABCEUO" ("AAABHPJO","AAABHPJP","AAABCFGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCEUS" (
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABHPJW" varchar(4) NOT NULL default ' ',
  "AAABHJFJ" varchar(4) NOT NULL default '0000',
  "AAABHPJO" varchar(4) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABCFGN" varchar(5) NOT NULL default '00000',
  "AAABHPJR" int(11) NOT NULL default '0',
  "AAABHPJS" varchar(8) NOT NULL default ' ',
  "AAABCFGQ" varchar(4) NOT NULL default '0000',
  "AAABCFGR" double NOT NULL default '0',
  "AAABCFGS" double NOT NULL default '0',
  "AAABCFGT" decimal(8,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABHPJU","AAABHPJV","AAABHPJW","AAABHJFJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCEVK" (
  "AAABHMXI" varchar(5) NOT NULL default '00000',
  "AAABHMXJ" varchar(5) NOT NULL default ' ',
  "AAABCFEY" varchar(5) NOT NULL default '00000',
  "AAABHMXF" int(11) NOT NULL default '0',
  "AAABHMXG" varchar(200) NOT NULL default ' ',
  "AAABHMXH" double NOT NULL default '0',
  "AAABCFCI" double NOT NULL default '0',
  PRIMARY KEY ("AAABHMXI","AAABHMXJ","AAABCFEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCEVV" (
  "AAABHMXI" varchar(5) NOT NULL default '00000',
  "AAABHMXJ" varchar(5) NOT NULL default ' ',
  "AAABCFEY" varchar(3) NOT NULL default '000',
  "AAABHMXF" int(11) NOT NULL default '0',
  "AAABHMXG" varchar(200) NOT NULL default ' ',
  "AAABHMXH" double NOT NULL default '0',
  "AAABCFCI" double NOT NULL default '0',
  PRIMARY KEY ("AAABHMXI","AAABHMXJ","AAABCFEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCEWG" (
  "AAABHMXI" varchar(5) NOT NULL default '00000',
  "AAABHMXJ" varchar(5) NOT NULL default ' ',
  "AAABCFEY" varchar(5) NOT NULL default '00000',
  "AAABJSWC" double NOT NULL default '0',
  "AAABCFDS" tinyint(3) unsigned NOT NULL default '0',
  "AAABCFDT" smallint(6) NOT NULL default '0',
  "AAABCFDU" int(11) NOT NULL default '0',
  "AAABCFDV" varchar(5) NOT NULL default '00000',
  "AAABCFDW" varchar(8) NOT NULL default '00000000',
  "AAABCFDX" decimal(4,2) NOT NULL default '0.00',
  "AAABCFDY" decimal(8,2) NOT NULL default '0.00',
  "AAABCFDZ" decimal(14,7) NOT NULL default '0.0000000',
  "AAABCFEA" decimal(15,0) NOT NULL default '0',
  "AAABCFEB" varchar(8) NOT NULL default '00000000',
  "AAABCFEC" varchar(6) NOT NULL default '000000',
  "AAABCFED" varchar(1) NOT NULL default ' ',
  "AAABCFEE" varchar(20) NOT NULL default ' ',
  "AAABCFEF" varchar(33) NOT NULL default ' ',
  "AAABCFEG" VARBINARY(255),
  "AAABCFEH" varchar(226) NOT NULL default ' ',
  "AAABCFEI" varchar(254) NOT NULL default ' ',
  "AAABFWCT" smallint(6) NOT NULL default '0',
  "AAABHMLJ" text,
  PRIMARY KEY ("AAABHMXI","AAABHMXJ","AAABCFEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCEXH" (
  "AAABHMXI" varchar(5) NOT NULL default '00000',
  "AAABHMXJ" varchar(5) NOT NULL default ' ',
  "AAABCFEY" varchar(5) NOT NULL default '00000',
  "AAABJSWC" double NOT NULL default '0',
  "AAABCFDS" tinyint(3) unsigned NOT NULL default '0',
  "AAABCFDT" smallint(6) NOT NULL default '0',
  "AAABCFDU" int(11) NOT NULL default '0',
  "AAABCFDV" varchar(5) NOT NULL default '00000',
  "AAABCFDW" varchar(8) NOT NULL default '00000000',
  "AAABCFDX" decimal(4,2) NOT NULL default '0.00',
  "AAABCFDY" decimal(8,2) NOT NULL default '0.00',
  "AAABCFDZ" decimal(14,7) NOT NULL default '0.0000000',
  "AAABCFEA" decimal(15,0) NOT NULL default '0',
  "AAABCFEB" varchar(8) NOT NULL default '00000000',
  "AAABCFEC" varchar(6) NOT NULL default '000000',
  "AAABCFED" varchar(1) NOT NULL default ' ',
  "AAABCFEE" varchar(20) NOT NULL default ' ',
  "AAABCFEF" varchar(33) NOT NULL default ' ',
  "AAABCFEG" VARBINARY(255),
  "AAABCFEH" varchar(226) NOT NULL default ' ',
  "AAABCFEI" varchar(254) NOT NULL default ' ',
  "AAABFWCT" smallint(6) NOT NULL default '0',
  "AAABHMLJ" text,
  PRIMARY KEY ("AAABHMXI","AAABHMXJ","AAABCFEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCEYI" (
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABHPJW" varchar(4) NOT NULL default ' ',
  "AAABHJFJ" varchar(4) NOT NULL default '0000',
  "AAABCEZC" varchar(2) NOT NULL default ' ',
  "AAABCEZD" int(11) NOT NULL default '0',
  "AAABHPJO" varchar(4) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABCFGN" varchar(5) NOT NULL default '00000',
  "AAABHPJR" int(11) NOT NULL default '0',
  "AAABHPJS" varchar(8) NOT NULL default ' ',
  "AAABCFGQ" varchar(4) NOT NULL default '0000',
  "AAABCFGR" double NOT NULL default '0',
  "AAABCFGS" double NOT NULL default '0',
  "AAABCFGT" decimal(8,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABHPJU","AAABHPJV","AAABHPJW","AAABHJFJ","AAABCEZC","AAABCEZD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCEZE" (
  "AAABCFFG" VARBINARY(255) NOT NULL,
  "AAABCFFH" VARBINARY(255) NOT NULL,
  "AAABHPJW" varchar(4) NOT NULL default ' ',
  "AAABCFFD" VARBINARY(255),
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABCFFF" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABCFFG"(255),"AAABCFFH"(255),"AAABHPJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCEZO" (
  "AAABCFFG" VARBINARY(255) NOT NULL,
  "AAABCFFH" VARBINARY(255) NOT NULL,
  "AAABHPJW" varchar(4) NOT NULL default ' ',
  "AAABCFFD" VARBINARY(255),
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABCFFF" int(11) NOT NULL default '0',
  "AAABCEZV" varchar(255) NOT NULL default ' ',
  "AAABCEZW" longtext,
  "AAABCEZX" longblob,
  "AAABCEZY" longtext,
  PRIMARY KEY ("AAABCFFG"(255),"AAABCFFH"(255),"AAABHPJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFAC" (
  "AAABCFFX" varchar(3) NOT NULL default ' ',
  "AAABCFFY" int(11) NOT NULL default '0',
  "AAABCLOM" int(11) NOT NULL default '0',
  "AAABCFGA" varchar(30) NOT NULL default ' ',
  "AAABHPJO" varchar(4) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABCFGN" varchar(5) NOT NULL default '00000',
  "AAABHPJR" int(11) NOT NULL default '0',
  "AAABHPJS" varchar(8) NOT NULL default ' ',
  "AAABCFGQ" varchar(4) NOT NULL default '0000',
  "AAABCFGR" double NOT NULL default '0',
  "AAABCFGS" double NOT NULL default '0',
  "AAABCFGT" decimal(8,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABCFFX","AAABCFFY","AAABCLOM","AAABCFGA"),
  KEY "AAABCFAU" ("AAABHPJO","AAABHPJP","AAABCFGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFAY" (
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABHPJW" varchar(4) NOT NULL default ' ',
  "AAABHJFJ" varchar(4) NOT NULL default '0000',
  "AAABHPJO" varchar(4) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABCFGN" varchar(5) NOT NULL default '00000',
  "AAABHPJR" int(11) NOT NULL default '0',
  "AAABHPJS" varchar(8) NOT NULL default ' ',
  "AAABCFGQ" varchar(4) NOT NULL default '0000',
  "AAABCFGR" double NOT NULL default '0',
  "AAABCFGS" double NOT NULL default '0',
  "AAABCFGT" decimal(8,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABHPJU","AAABHPJV","AAABHPJW","AAABHJFJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFBQ" (
  "AAABHMXI" varchar(5) NOT NULL default '00000',
  "AAABHMXJ" varchar(5) NOT NULL default ' ',
  "AAABCFEY" varchar(5) NOT NULL default '00000',
  "AAABHMXF" int(11) NOT NULL default '0',
  "AAABHMXG" varchar(200) NOT NULL default ' ',
  "AAABHMXH" double NOT NULL default '0',
  "AAABCFCI" double NOT NULL default '0',
  PRIMARY KEY ("AAABHMXI","AAABHMXJ","AAABCFEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFCB" (
  "AAABHMXI" varchar(5) NOT NULL default '00000',
  "AAABHMXJ" varchar(5) NOT NULL default ' ',
  "AAABCFEY" varchar(3) NOT NULL default '000',
  "AAABHMXF" int(11) NOT NULL default '0',
  "AAABHMXG" varchar(200) NOT NULL default ' ',
  "AAABHMXH" double NOT NULL default '0',
  "AAABCFCI" double NOT NULL default '0',
  PRIMARY KEY ("AAABHMXI","AAABHMXJ","AAABCFEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFCM" (
  "AAABHMXI" varchar(5) NOT NULL default '00000',
  "AAABHMXJ" varchar(5) NOT NULL default ' ',
  "AAABCFEY" varchar(5) NOT NULL default '00000',
  "AAABJSWC" double NOT NULL default '0',
  "AAABCFDS" tinyint(3) unsigned NOT NULL default '0',
  "AAABCFDT" smallint(6) NOT NULL default '0',
  "AAABCFDU" int(11) NOT NULL default '0',
  "AAABCFDV" varchar(5) NOT NULL default '00000',
  "AAABCFDW" varchar(8) NOT NULL default '00000000',
  "AAABCFDX" decimal(4,2) NOT NULL default '0.00',
  "AAABCFDY" decimal(8,2) NOT NULL default '0.00',
  "AAABCFDZ" decimal(14,7) NOT NULL default '0.0000000',
  "AAABCFEA" decimal(15,0) NOT NULL default '0',
  "AAABCFEB" varchar(8) NOT NULL default '00000000',
  "AAABCFEC" varchar(6) NOT NULL default '000000',
  "AAABCFED" varchar(1) NOT NULL default ' ',
  "AAABCFEE" varchar(20) NOT NULL default ' ',
  "AAABCFEF" varchar(33) NOT NULL default ' ',
  "AAABCFEG" VARBINARY(255),
  "AAABCFEH" varchar(226) NOT NULL default ' ',
  "AAABCFEI" varchar(254) NOT NULL default ' ',
  "AAABFWCT" smallint(6) NOT NULL default '0',
  "AAABHMLJ" text,
  PRIMARY KEY ("AAABHMXI","AAABHMXJ","AAABCFEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFDN" (
  "AAABHMXI" varchar(5) NOT NULL default '00000',
  "AAABHMXJ" varchar(5) NOT NULL default ' ',
  "AAABCFEY" varchar(5) NOT NULL default '00000',
  "AAABJSWC" double NOT NULL default '0',
  "AAABCFDS" tinyint(3) unsigned NOT NULL default '0',
  "AAABCFDT" smallint(6) NOT NULL default '0',
  "AAABCFDU" int(11) NOT NULL default '0',
  "AAABCFDV" varchar(5) NOT NULL default '00000',
  "AAABCFDW" varchar(8) NOT NULL default '00000000',
  "AAABCFDX" decimal(4,2) NOT NULL default '0.00',
  "AAABCFDY" decimal(8,2) NOT NULL default '0.00',
  "AAABCFDZ" decimal(14,7) NOT NULL default '0.0000000',
  "AAABCFEA" decimal(15,0) NOT NULL default '0',
  "AAABCFEB" varchar(8) NOT NULL default '00000000',
  "AAABCFEC" varchar(6) NOT NULL default '000000',
  "AAABCFED" varchar(1) NOT NULL default ' ',
  "AAABCFEE" varchar(20) NOT NULL default ' ',
  "AAABCFEF" varchar(33) NOT NULL default ' ',
  "AAABCFEG" VARBINARY(255),
  "AAABCFEH" varchar(226) NOT NULL default ' ',
  "AAABCFEI" varchar(254) NOT NULL default ' ',
  "AAABFWCT" smallint(6) NOT NULL default '0',
  "AAABHMLJ" text,
  PRIMARY KEY ("AAABHMXI","AAABHMXJ","AAABCFEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFEO" (
  "AAABHMXI" varchar(5) NOT NULL default '00000',
  "AAABHMXJ" varchar(5) NOT NULL default ' ',
  "AAABCFEY" varchar(5) NOT NULL default '00000',
  "AAABHJSU" varchar(1) NOT NULL default ' ',
  "AAABHJSV" varchar(2) NOT NULL default ' ',
  "AAABCFEU" varchar(5) NOT NULL default ' ',
  "AAABCFEV" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABHMXI","AAABHMXJ","AAABCFEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFEZ" (
  "AAABCFFG" VARBINARY(255) NOT NULL,
  "AAABCFFH" VARBINARY(255) NOT NULL,
  "AAABHPJW" varchar(4) NOT NULL default ' ',
  "AAABCFFD" VARBINARY(255),
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABCFFF" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABCFFG"(255),"AAABCFFH"(255),"AAABHPJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFFJ" (
  "AAABCFFX" varchar(3) NOT NULL default ' ',
  "AAABCFFY" int(11) NOT NULL default '0',
  "AAABCLOM" int(11) NOT NULL default '0',
  "AAABCFGA" varchar(30) NOT NULL default ' ',
  "AAABHPJO" varchar(4) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABCFGN" varchar(5) NOT NULL default '00000',
  "AAABHPJR" int(11) NOT NULL default '0',
  "AAABHPJS" varchar(8) NOT NULL default ' ',
  "AAABCFGQ" varchar(4) NOT NULL default '0000',
  "AAABCFGR" double NOT NULL default '0',
  "AAABCFGS" double NOT NULL default '0',
  "AAABCFGT" decimal(8,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABCFFX","AAABCFFY","AAABCLOM","AAABCFGA"),
  KEY "AAABCFGB" ("AAABHPJO","AAABHPJP","AAABCFGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFGF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABHPJW" varchar(4) NOT NULL default ' ',
  "AAABHJFJ" varchar(4) NOT NULL default '0000',
  "AAABHPJO" varchar(4) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABCFGN" varchar(5) NOT NULL default '00000',
  "AAABHPJR" int(11) NOT NULL default '0',
  "AAABHPJS" varchar(8) NOT NULL default ' ',
  "AAABCFGQ" varchar(4) NOT NULL default '0000',
  "AAABCFGR" double NOT NULL default '0',
  "AAABCFGS" double NOT NULL default '0',
  "AAABCFGT" decimal(8,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHPJU","AAABHPJV","AAABHPJW","AAABHJFJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFGZ" (
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFHF" (
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAABLFYC" varchar(4) NOT NULL default ' ',
  "AAABCFHW" varchar(40) NOT NULL default ' ',
  "AAABCFHJ" varchar(8) NOT NULL default '00000000',
  "AAABCFHK" varchar(6) NOT NULL default ' ',
  "AAABCFHL" varchar(8) NOT NULL default '00000000',
  "AAABCFHM" varchar(6) NOT NULL default '000000',
  "AAABCFHN" varchar(1) NOT NULL default ' ',
  "AAABCFHO" smallint(6) NOT NULL default '0',
  "AAABCFHP" longblob,
  "AAABCFHQ" decimal(21,7) NOT NULL default '0.0000000',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAABCFHS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFYD","AAABLFYC"),
  UNIQUE KEY "AAABCFHV" ("AAABCFHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFHX" (
  "AAABCFHZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABCFHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFIA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFVKL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFVKL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFIF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFVKL" varchar(2) NOT NULL default ' ',
  "AAABILTS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFVKL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFIN" (
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABHMFI" varchar(2) NOT NULL default ' ',
  "AAABCFJF" varchar(4) NOT NULL default '0000',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABCFIS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNLV","AAABHMFI","AAABCFJF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFIW" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABHMFI" varchar(2) NOT NULL default ' ',
  "AAABCFJF" varchar(4) NOT NULL default '0000',
  "AAABCFJB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMNLV","AAABHMFI","AAABCFJF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFJG" (
  "AAABGBBM" varchar(3) NOT NULL default '000',
  "AAABHHZM" varchar(5) NOT NULL default ' ',
  "AAABCFJJ" varchar(1) NOT NULL default ' ',
  "AAABCFJK" varchar(1) NOT NULL default ' ',
  "AAABCFJL" varchar(1) NOT NULL default ' ',
  "AAABCFJM" varchar(3) NOT NULL default '000',
  "AAABHHZG" varchar(5) NOT NULL default ' ',
  "AAABCFJO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABGBBM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFKC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCFKC" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCFKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFJV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCFKC" varchar(3) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCFKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFKD" (
  "AAABMCNN" varchar(4) NOT NULL default ' ',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABCFKQ" varchar(5) NOT NULL default ' ',
  "AAABCFKI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMCNN","AAABMQCD","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFKM" (
  "AAABCFKQ" varchar(5) NOT NULL default ' ',
  "AAABLGEF" varchar(25) NOT NULL default ' ',
  "AAABMOIB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABCFKQ","AAABLGEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFKS" (
  "AAABKCWA" varchar(16) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKCWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMXF" varchar(4) NOT NULL default ' ',
  "AAABFSXC" varchar(2) NOT NULL default ' ',
  "AAABCFLA" varchar(1) NOT NULL default ' ',
  "AAABCFLB" varchar(1) NOT NULL default ' ',
  "AAABCHPN" varchar(1) NOT NULL default ' ',
  "AAABMLMR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFLG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGMXF" varchar(4) NOT NULL default ' ',
  "AAABCPGO" varchar(15) NOT NULL default ' ',
  "AAABHIHE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGMXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFLP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLCCC" varchar(6) NOT NULL default ' ',
  "AAABCGWM" varchar(6) NOT NULL default ' ',
  "AAABCFLT" varchar(1) NOT NULL default ' ',
  "AAABCFLU" varchar(6) NOT NULL default ' ',
  "AAABCHPN" varchar(1) NOT NULL default ' ',
  "AAABMLTD" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLUXQ","AAABLCCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFLZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCGWM" varchar(6) NOT NULL default ' ',
  "AAABCTSS" varchar(2) NOT NULL default ' ',
  "AAABCSCB" varchar(1) NOT NULL default ' ',
  "AAABCFME" varchar(1) NOT NULL default ' ',
  "AAABHAAO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCGWM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFMI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCGWM" varchar(6) NOT NULL default ' ',
  "AAABCFMM" varchar(25) NOT NULL default ' ',
  "AAABCFMN" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABCGWM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFMR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCFNT" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCFNT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFMW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCFNT" varchar(6) NOT NULL default ' ',
  "AAABCFNA" varchar(25) NOT NULL default ' ',
  "AAABCFNB" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABCFNT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFNF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCFNT" varchar(6) NOT NULL default ' ',
  "AAABLCCC" varchar(6) NOT NULL default ' ',
  "AAABLWVS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCFNT","AAABLCCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFNN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCFNT" varchar(6) NOT NULL default ' ',
  "AAABCFNU" varchar(6) NOT NULL default ' ',
  "AAABCFNV" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCFNT","AAABCFNU","AAABCFNV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFNW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCSNF" varchar(6) NOT NULL default ' ',
  "AAABCGWO" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCSNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFOC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCSNF" varchar(6) NOT NULL default ' ',
  "AAABCFOG" varchar(25) NOT NULL default ' ',
  "AAABCFOH" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABCSNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFOL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCSNF" varchar(6) NOT NULL default ' ',
  "AAABLCCC" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCSNF","AAABLCCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFOS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCGWO" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCGWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFOX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCGWO" varchar(6) NOT NULL default ' ',
  "AAABCFPB" varchar(25) NOT NULL default ' ',
  "AAABCFPC" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABCGWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFPG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLCCC" varchar(6) NOT NULL default ' ',
  "AAABCFPK" varchar(25) NOT NULL default ' ',
  "AAABDDSS" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABLCCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFPP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCGWM" varchar(6) NOT NULL default ' ',
  "AAABIYCS" varchar(4) NOT NULL default ' ',
  "AAABCFPT" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABCGWM","AAABIYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFPX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABDTFE" varchar(4) NOT NULL default ' ',
  "AAABKYNY" varchar(250) NOT NULL default ' ',
  "AAABKYNZ" varchar(250) NOT NULL default ' ',
  "AAABKYOA" varchar(250) NOT NULL default ' ',
  "AAABKWJH" varchar(250) NOT NULL default ' ',
  "AAABKWJI" varchar(250) NOT NULL default ' ',
  "AAABKWJJ" varchar(250) NOT NULL default ' ',
  "AAABKWJK" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABDTFE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFQJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDTFE" varchar(4) NOT NULL default ' ',
  "AAABLOFJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABDTFE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFQR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABDTFE" varchar(4) NOT NULL default ' ',
  "AAABCSYR" varchar(250) NOT NULL default ' ',
  "AAABCSYS" varchar(250) NOT NULL default ' ',
  "AAABCSYT" varchar(250) NOT NULL default ' ',
  "AAABCSYU" varchar(250) NOT NULL default ' ',
  "AAABCSYV" varchar(250) NOT NULL default ' ',
  "AAABCSYW" varchar(250) NOT NULL default ' ',
  "AAABCSYX" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABDTFE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFRD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLUZL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLUZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFRI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUZL" varchar(4) NOT NULL default ' ',
  "AAABCGJB" varchar(15) NOT NULL default ' ',
  "AAABCGJC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABLUZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFRR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKVKH" varchar(6) NOT NULL default ' ',
  "AAABKVKB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKVKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFRX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLJJO" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLJJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFSC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLJJO" varchar(6) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABLJJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFSK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLJJO" varchar(6) NOT NULL default ' ',
  "AAABKVKB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLJJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFSQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKVKB" varchar(10) NOT NULL default ' ',
  "AAABEPCU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKVKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFSW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKVKB" varchar(10) NOT NULL default ' ',
  "AAABCGJB" varchar(15) NOT NULL default ' ',
  "AAABCGJC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKVKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFTF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABJHHP" varchar(10) NOT NULL default ' ',
  "AAABLTST" varchar(1) NOT NULL default ' ',
  "AAABCTKA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABJHHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFTO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABJHHP" varchar(10) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKWSS","AAABJHHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFTY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABJHHP" varchar(10) NOT NULL default ' ',
  "AAABMDXP" varchar(10) NOT NULL default ' ',
  "AAABMDWW" varchar(10) NOT NULL default ' ',
  "AAABCTLF" tinyint(3) unsigned NOT NULL default '0',
  "AAABCTKV" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABJHHP","AAABMDXP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFUK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGGVN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGGVN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFUP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCTIV" varchar(6) NOT NULL default ' ',
  "AAABJHHP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABCTIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFUX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCTIV" varchar(6) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKWSS","AAABCTIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFVH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGGVN" varchar(2) NOT NULL default ' ',
  "AAABCGJB" varchar(15) NOT NULL default ' ',
  "AAABCGJC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABGGVN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFVQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKEVT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKEVT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFVV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKEVT" varchar(10) NOT NULL default ' ',
  "AAABCGJB" varchar(15) NOT NULL default ' ',
  "AAABCGJC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKEVT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFWE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCFWX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCFWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFWJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCFWX" varchar(4) NOT NULL default ' ',
  "AAABCFWM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCFWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFWP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCFWX" varchar(4) NOT NULL default ' ',
  "AAABCGJB" varchar(15) NOT NULL default ' ',
  "AAABCGJC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABCFWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFWY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGGSP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGGSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFXD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGGSP" varchar(1) NOT NULL default ' ',
  "AAABCHTP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABGGSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFXL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCFXX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCFXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFXQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCFXX" varchar(4) NOT NULL default ' ',
  "AAABGMNJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABCFXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFXY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKRSA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKRSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFYD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKRSA" varchar(4) NOT NULL default ' ',
  "AAABCHTP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKRSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFYL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABKQPE" varchar(8) NOT NULL default ' ',
  "AAABCFYP" varchar(61) NOT NULL default ' ',
  "AAABCFYQ" varchar(61) NOT NULL default ' ',
  "AAABCOUC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABKQPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFYV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABKQPE" varchar(8) NOT NULL default ' ',
  "AAABGMNJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKWSS","AAABKQPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFZF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABGNDC" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABGNDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFZM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIDUS" varchar(2) NOT NULL default ' ',
  "AAABCFZP" varchar(1) NOT NULL default ' ',
  "AAABGGVS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIDUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCFZT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIDUS" varchar(2) NOT NULL default ' ',
  "AAABCGJB" varchar(15) NOT NULL default ' ',
  "AAABCGJC" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABIDUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGAC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQPP" varchar(4) NOT NULL default ' ',
  "AAABDDWG" varchar(4) NOT NULL default ' ',
  "AAABCGAG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQPP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGAJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQPP" varchar(4) NOT NULL default ' ',
  "AAABLKKE" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQPP","AAABLKKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGAQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLQPP" varchar(4) NOT NULL default ' ',
  "AAABGMNJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABLQPP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGAY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQPP" varchar(4) NOT NULL default ' ',
  "AAABLKKE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQPP","AAABLKKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGBF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLKKE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLKKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGBK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLKKE" varchar(2) NOT NULL default ' ',
  "AAABGMNJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABLKKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGBS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJLQV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJLQV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGBX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJLQV" varchar(2) NOT NULL default ' ',
  "AAABCGCB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABJLQV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGCF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCMXH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCMXH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGCK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCMXH" varchar(10) NOT NULL default ' ',
  "AAABEPBY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABCMXH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGCS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCMXH" varchar(10) NOT NULL default ' ',
  "AAABLNZH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCMXH","AAABLNZH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGCZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCMXH" varchar(10) NOT NULL default ' ',
  "AAABLNZH" varchar(4) NOT NULL default ' ',
  "AAABGNCZ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABCMXH","AAABLNZH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGDJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCGEQ" varchar(4) NOT NULL default ' ',
  "AAABCGDM" varchar(1) NOT NULL default ' ',
  "AAABCGDN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCGEQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGDQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCGEQ" varchar(4) NOT NULL default ' ',
  "AAABKEVT" varchar(10) NOT NULL default ' ',
  "AAABCGDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCGEQ","AAABKEVT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGDY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCGEQ" varchar(4) NOT NULL default ' ',
  "AAABKEVT" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(100) NOT NULL default ' ',
  "AAABCGEE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABCGEQ","AAABKEVT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGEJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCGEQ" varchar(4) NOT NULL default ' ',
  "AAABGMNJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABCGEQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGER" (
  "AAABLRXL" varchar(6) NOT NULL default ' ',
  "AAABCGET" varchar(1) NOT NULL default ' ',
  "AAABCGEU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLRXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGEW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMOHM" varchar(6) NOT NULL default ' ',
  "AAABLRXL" varchar(6) NOT NULL default ' ',
  "AAABCGFA" varchar(1) NOT NULL default ' ',
  "AAABCGFB" varchar(1) NOT NULL default ' ',
  "AAABCGFC" varchar(1) NOT NULL default ' ',
  "AAABHAAO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGFG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(6) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGFO" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLRXL" varchar(6) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLRXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGFU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABGYQM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABGYQM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGGB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCGGJ" varchar(30) NOT NULL default ' ',
  "AAABCGGK" varchar(30) NOT NULL default ' ',
  "AAABCGGL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABCGGJ","AAABCGGK","AAABCGGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGGM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABGYQM" varchar(30) NOT NULL default ' ',
  "AAABGNBZ" varchar(30) NOT NULL default ' ',
  "AAABCGGR" varchar(3) NOT NULL default '000',
  "AAABJJLP" varchar(3) NOT NULL default '000',
  "AAABCGGT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABGYQM","AAABGNBZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGGY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABIMZU" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABIMZU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGHF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABIMZU" varchar(14) NOT NULL default ' ',
  "AAABMKYE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABKWSS","AAABIMZU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGHP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABKVKH" varchar(6) NOT NULL default ' ',
  "AAABCHPN" varchar(1) NOT NULL default ' ',
  "AAABMLTD" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABKVKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGHY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABKVKH" varchar(6) NOT NULL default ' ',
  "AAABCTRJ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKWSS","AAABKVKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGII" (
  "AAABLSMW" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGIL" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLSMW" varchar(10) NOT NULL default ' ',
  "AAABHTUP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLSMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGIR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCGJW" varchar(10) NOT NULL default ' ',
  "AAABEPCU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCGJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGIX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCGJW" varchar(10) NOT NULL default ' ',
  "AAABCGJB" varchar(15) NOT NULL default ' ',
  "AAABCGJC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABCGJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGJG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLJJO" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLJJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGJL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLJJO" varchar(6) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABLJJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGJT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLJJO" varchar(6) NOT NULL default ' ',
  "AAABCGJW" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLJJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGJZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCGKN" varchar(4) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCGKN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGKG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCGKN" varchar(4) NOT NULL default ' ',
  "AAABGMNJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABCGKN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGKO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCGLE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABCGLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGKV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCGLE" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKWSS","AAABCGLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGLF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABKVKH" varchar(6) NOT NULL default ' ',
  "AAABLSMW" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABKVKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGLN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABJTNV" varchar(30) NOT NULL default ' ',
  "AAABCTUY" varchar(6) NOT NULL default ' ',
  "AAABCTNQ" varchar(7) NOT NULL default '0000000',
  "AAABCTNR" varchar(7) NOT NULL default '0000000',
  "AAABCTNS" varchar(7) NOT NULL default '0000000',
  "AAABCTNI" varchar(1) NOT NULL default ' ',
  "AAABLSBY" varchar(200) NOT NULL default ' ',
  "AAABCTNK" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKWSS","AAABJTNV","AAABCTUY","AAABCTNQ","AAABCTNR","AAABCTNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGMH" (
  "AAABMKFH" varchar(32) NOT NULL default ' ',
  "AAABMLTD" varchar(7) NOT NULL default '0000000',
  "AAABIJQL" varchar(30) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGMO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCGMR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGMU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABKVKH" varchar(6) NOT NULL default ' ',
  "AAABCGMY" varchar(1) NOT NULL default ' ',
  "AAABCGMZ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABKVKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGND" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCGRI" varchar(3) NOT NULL default ' ',
  "AAABHILD" varchar(4) NOT NULL default ' ',
  "AAABCGOM" varchar(1) NOT NULL default ' ',
  "AAABCGNI" varchar(1) NOT NULL default ' ',
  "AAABMRCU" varchar(1) NOT NULL default '0',
  "AAABIYVW" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLUXQ","AAABCGRI","AAABHILD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGNO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABCGRI" varchar(3) NOT NULL default ' ',
  "AAABHILD" varchar(4) NOT NULL default ' ',
  "AAABCGNT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLFZK","AAABCGRI","AAABHILD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGNY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCGRI" varchar(3) NOT NULL default ' ',
  "AAABHILD" varchar(4) NOT NULL default ' ',
  "AAABCGOD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABCGRI","AAABHILD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGOI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCGRI" varchar(3) NOT NULL default ' ',
  "AAABHILD" varchar(4) NOT NULL default ' ',
  "AAABCGOM" varchar(1) NOT NULL default ' ',
  "AAABCGON" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCGRI","AAABHILD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGOR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCGRI" varchar(3) NOT NULL default ' ',
  "AAABCGQN" varchar(3) NOT NULL default ' ',
  "AAABHILD" varchar(4) NOT NULL default ' ',
  "AAABCGRK" varchar(20) NOT NULL default ' ',
  "AAABCGPJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCGRI","AAABCGQN","AAABHILD","AAABCGRK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGPD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCGRI" varchar(3) NOT NULL default ' ',
  "AAABCGQN" varchar(3) NOT NULL default ' ',
  "AAABHILD" varchar(4) NOT NULL default ' ',
  "AAABCGRK" varchar(20) NOT NULL default ' ',
  "AAABCGPJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCGRI","AAABCGQN","AAABHILD","AAABCGRK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGPP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCGRI" varchar(3) NOT NULL default ' ',
  "AAABCGQN" varchar(3) NOT NULL default ' ',
  "AAABHILD" varchar(4) NOT NULL default ' ',
  "AAABCGRK" varchar(20) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABCGRI","AAABCGQN","AAABHILD","AAABCGRK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGQD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCGRI" varchar(3) NOT NULL default ' ',
  "AAABCGQN" varchar(3) NOT NULL default ' ',
  "AAABHILD" varchar(4) NOT NULL default ' ',
  "AAABCGRK" varchar(20) NOT NULL default ' ',
  "AAABHBLT" varchar(4) NOT NULL default ' ',
  "AAABHBKP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCGRI","AAABCGQN","AAABHILD","AAABCGRK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGQQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCGRI" varchar(3) NOT NULL default ' ',
  "AAABHILD" varchar(4) NOT NULL default ' ',
  "AAABCGRA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCGRI","AAABHILD"),
  UNIQUE KEY "AAABCGQY" ("AAABLUXQ","AAABCGRA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGRB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCGRI" varchar(3) NOT NULL default ' ',
  "AAABHILD" varchar(4) NOT NULL default ' ',
  "AAABCGRK" varchar(20) NOT NULL default ' ',
  "AAABCGRG" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCGRI","AAABHILD","AAABCGRK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGRL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABKYNY" varchar(250) NOT NULL default ' ',
  "AAABKYNZ" varchar(250) NOT NULL default ' ',
  "AAABKYOA" varchar(250) NOT NULL default ' ',
  "AAABKWJH" varchar(250) NOT NULL default ' ',
  "AAABKWJI" varchar(250) NOT NULL default ' ',
  "AAABKWJJ" varchar(250) NOT NULL default ' ',
  "AAABKWJK" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGRX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCSYR" varchar(250) NOT NULL default ' ',
  "AAABCSYS" varchar(250) NOT NULL default ' ',
  "AAABCSYT" varchar(250) NOT NULL default ' ',
  "AAABCSYU" varchar(250) NOT NULL default ' ',
  "AAABCSYV" varchar(250) NOT NULL default ' ',
  "AAABCSYW" varchar(250) NOT NULL default ' ',
  "AAABCSYX" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGSJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABKVKH" varchar(6) NOT NULL default ' ',
  "AAABKYNY" varchar(250) NOT NULL default ' ',
  "AAABKYNZ" varchar(250) NOT NULL default ' ',
  "AAABKYOA" varchar(250) NOT NULL default ' ',
  "AAABKWJH" varchar(250) NOT NULL default ' ',
  "AAABKWJI" varchar(250) NOT NULL default ' ',
  "AAABKWJJ" varchar(250) NOT NULL default ' ',
  "AAABKWJK" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABKVKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGSX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABKVKH" varchar(6) NOT NULL default ' ',
  "AAABCSYR" varchar(250) NOT NULL default ' ',
  "AAABCSYS" varchar(250) NOT NULL default ' ',
  "AAABCSYT" varchar(250) NOT NULL default ' ',
  "AAABCSYU" varchar(250) NOT NULL default ' ',
  "AAABCSYV" varchar(250) NOT NULL default ' ',
  "AAABCSYW" varchar(250) NOT NULL default ' ',
  "AAABCSYX" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABKVKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGTL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCSPK" varchar(10) NOT NULL default ' ',
  "AAABKYNY" varchar(250) NOT NULL default ' ',
  "AAABKYNZ" varchar(250) NOT NULL default ' ',
  "AAABKYOA" varchar(250) NOT NULL default ' ',
  "AAABKWJH" varchar(250) NOT NULL default ' ',
  "AAABKWJI" varchar(250) NOT NULL default ' ',
  "AAABKWJJ" varchar(250) NOT NULL default ' ',
  "AAABKWJK" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABCSPK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGTZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCSPK" varchar(10) NOT NULL default ' ',
  "AAABCSYR" varchar(250) NOT NULL default ' ',
  "AAABCSYS" varchar(250) NOT NULL default ' ',
  "AAABCSYT" varchar(250) NOT NULL default ' ',
  "AAABCSYU" varchar(250) NOT NULL default ' ',
  "AAABCSYV" varchar(250) NOT NULL default ' ',
  "AAABCSYW" varchar(250) NOT NULL default ' ',
  "AAABCSYX" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABCSPK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGUN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABMDBL" varchar(2) NOT NULL default ' ',
  "AAABKYNY" varchar(250) NOT NULL default ' ',
  "AAABKYNZ" varchar(250) NOT NULL default ' ',
  "AAABKYOA" varchar(250) NOT NULL default ' ',
  "AAABKWJH" varchar(250) NOT NULL default ' ',
  "AAABKWJI" varchar(250) NOT NULL default ' ',
  "AAABKWJJ" varchar(250) NOT NULL default ' ',
  "AAABKWJK" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABMDBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGVB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABMDBL" varchar(2) NOT NULL default ' ',
  "AAABCSYR" varchar(250) NOT NULL default ' ',
  "AAABCSYS" varchar(250) NOT NULL default ' ',
  "AAABCSYT" varchar(250) NOT NULL default ' ',
  "AAABCSYU" varchar(250) NOT NULL default ' ',
  "AAABCSYV" varchar(250) NOT NULL default ' ',
  "AAABCSYW" varchar(250) NOT NULL default ' ',
  "AAABCSYX" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABMDBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGVP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLCCC" varchar(6) NOT NULL default ' ',
  "AAABKYNY" varchar(250) NOT NULL default ' ',
  "AAABKYNZ" varchar(250) NOT NULL default ' ',
  "AAABKYOA" varchar(250) NOT NULL default ' ',
  "AAABKWJH" varchar(250) NOT NULL default ' ',
  "AAABKWJI" varchar(250) NOT NULL default ' ',
  "AAABKWJJ" varchar(250) NOT NULL default ' ',
  "AAABKWJK" varchar(250) NOT NULL default ' ',
  "AAABCSYR" varchar(250) NOT NULL default ' ',
  "AAABCSYS" varchar(250) NOT NULL default ' ',
  "AAABCSYT" varchar(250) NOT NULL default ' ',
  "AAABCSYU" varchar(250) NOT NULL default ' ',
  "AAABCSYV" varchar(250) NOT NULL default ' ',
  "AAABCSYW" varchar(250) NOT NULL default ' ',
  "AAABCSYX" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLCCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGWI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMDBL" varchar(2) NOT NULL default ' ',
  "AAABCGWM" varchar(6) NOT NULL default ' ',
  "AAABLCCC" varchar(6) NOT NULL default ' ',
  "AAABCGWO" varchar(6) NOT NULL default ' ',
  "AAABCSNF" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGWS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCGXH" varchar(10) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABCGXD" varchar(3) NOT NULL default '000',
  "AAABILWE" varchar(10) NOT NULL default ' ',
  "AAABCGWY" varchar(3) NOT NULL default '000',
  "AAABKWVJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCGXH","AAABMREL","AAABCGXD"),
  KEY "AAABCGXE" ("AAABLUXQ","AAABILWE","AAABCGXH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGXI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCGXO" varchar(12) NOT NULL default ' ',
  "AAABKVKH" varchar(6) NOT NULL default ' ',
  "AAABMDBL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCGXO","AAABKVKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGXQ" (
  "AAABLUNJ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGXT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWVJ" varchar(10) NOT NULL default ' ',
  "AAABLUNJ" varchar(6) NOT NULL default ' ',
  "AAABLXTG" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWVJ","AAABLUNJ"),
  KEY "AAABCGYB" ("AAABLUXQ","AAABLXTG","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGYF" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUNJ" varchar(6) NOT NULL default ' ',
  "AAABGMNJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGYL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIFT" varchar(1) NOT NULL default ' ',
  "AAABCGYO" varchar(1) NOT NULL default ' ',
  "AAABCGYP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLIFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGYS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default ' ',
  "AAABCGYW" varchar(1) NOT NULL default ' ',
  "AAABCGYX" varchar(1) NOT NULL default ' ',
  "AAABCGYY" varchar(1) NOT NULL default ' ',
  "AAABCGYZ" varchar(2) NOT NULL default ' ',
  "AAABGVKP" varchar(2) NOT NULL default ' ',
  "AAABKPLY" varchar(2) NOT NULL default ' ',
  "AAABHENP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJBH","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGZG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLIFT" varchar(1) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLIFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGZO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCHEQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCHEQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCGZT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCHEQ" varchar(1) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCHEQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHAB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCHCO" varchar(4) NOT NULL default ' ',
  "AAABLUHY" varchar(1) NOT NULL default ' ',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  "AAABCHCT" varchar(5) NOT NULL default ' ',
  "AAABCHCU" varchar(4) NOT NULL default ' ',
  "AAABCHBV" varchar(1) NOT NULL default ' ',
  "AAABCHAJ" varchar(1) NOT NULL default ' ',
  "AAABLOOQ" varchar(2) NOT NULL default ' ',
  "AAABLHXK" varchar(1) NOT NULL default ' ',
  "AAABCHAM" varchar(6) NOT NULL default ' ',
  "AAABCHAN" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCHCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHAQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCHBE" varchar(4) NOT NULL default ' ',
  "AAABCHBF" varchar(4) NOT NULL default ' ',
  "AAABCHBG" varchar(4) NOT NULL default ' ',
  "AAABCHAV" varchar(4) NOT NULL default ' ',
  "AAABHLLW" varchar(1) NOT NULL default ' ',
  "AAABLHZN" varchar(1) NOT NULL default ' ',
  "AAABLXQH" varchar(1) NOT NULL default ' ',
  "AAABHLSE" varchar(1) NOT NULL default ' ',
  "AAABHLMG" varchar(1) NOT NULL default ' ',
  "AAABLIFT" varchar(1) NOT NULL default ' ',
  "AAABHLMJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCHBE","AAABCHBF","AAABCHBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHBH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCHCO" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCHCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHBP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCHCP" varchar(4) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABCHCT" varchar(5) NOT NULL default ' ',
  "AAABCHCU" varchar(4) NOT NULL default ' ',
  "AAABCHBV" varchar(1) NOT NULL default ' ',
  "AAABCHBW" varchar(1) NOT NULL default ' ',
  "AAABLOOU" varchar(2) NOT NULL default ' ',
  "AAABCHBY" varchar(6) NOT NULL default ' ',
  "AAABCHBZ" varchar(6) NOT NULL default ' ',
  "AAABCHCA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCHCP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHCD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCHCP" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCHCP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHCL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIFR" varchar(4) NOT NULL default ' ',
  "AAABCHCO" varchar(4) NOT NULL default ' ',
  "AAABCHCP" varchar(4) NOT NULL default ' ',
  "AAABCHEQ" varchar(1) NOT NULL default ' ',
  "AAABLYCG" varchar(2) NOT NULL default ' ',
  "AAABHOOP" varchar(6) NOT NULL default '000000',
  "AAABCHCT" varchar(5) NOT NULL default ' ',
  "AAABCHCU" varchar(4) NOT NULL default ' ',
  "AAABHYJM" varchar(2) NOT NULL default ' ',
  "AAABCHFM" varchar(2) NOT NULL default ' ',
  "AAABCHCX" varchar(1) NOT NULL default ' ',
  "AAABCHCY" varchar(1) NOT NULL default ' ',
  "AAABCHGA" varchar(4) NOT NULL default ' ',
  "AAABCHDA" varchar(1) NOT NULL default ' ',
  "AAABCHDB" varchar(1) NOT NULL default ' ',
  "AAABLOLT" varchar(2) NOT NULL default ' ',
  "AAABFZYX" varchar(2) NOT NULL default ' ',
  "AAABCHDE" varchar(6) NOT NULL default ' ',
  "AAABCHDF" varchar(6) NOT NULL default ' ',
  "AAABCHMS" varchar(132) NOT NULL default ' ',
  "AAABLIIZ" varchar(1) NOT NULL default ' ',
  "AAABCHDI" varchar(1) NOT NULL default ' ',
  "AAABCHDJ" varchar(1) NOT NULL default ' ',
  "AAABCHLY" varchar(10) NOT NULL default ' ',
  "AAABGJVA" varchar(4) NOT NULL default ' ',
  "AAABCHDM" varchar(1) NOT NULL default ' ',
  "AAABCHDN" varchar(1) NOT NULL default ' ',
  "AAABCHDO" varchar(1) NOT NULL default ' ',
  "AAABCHDP" varchar(1) NOT NULL default ' ',
  "AAABGJSJ" varchar(4) NOT NULL default ' ',
  "AAABEFVS" varchar(4) NOT NULL default ' ',
  "AAABCHDS" varchar(1) NOT NULL default ' ',
  "AAABCHDT" varchar(1) NOT NULL default ' ',
  "AAABLKXQ" varchar(4) NOT NULL default ' ',
  "AAABCHDV" varchar(1) NOT NULL default ' ',
  "AAABCHDW" varchar(1) NOT NULL default ' ',
  "AAABCHDX" varchar(1) NOT NULL default ' ',
  "AAABGACI" varchar(2) NOT NULL default ' ',
  "AAABCHDZ" varchar(1) NOT NULL default ' ',
  "AAABLIJI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLIFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHED" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLIFR" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLIFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHEL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCHEQ" varchar(1) NOT NULL default ' ',
  "AAABLIFT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCHEQ","AAABLIFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHES" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCHFM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCHFM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHEX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCHFM" varchar(2) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCHFM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHFF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCHFM" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABCHHE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCHFM","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHFP" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHFS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIBK" varchar(4) NOT NULL default ' ',
  "AAABMLGN" varchar(4) NOT NULL default ' ',
  "AAABKSPN" varchar(1) NOT NULL default ' ',
  "AAABLUHE" varchar(1) NOT NULL default ' ',
  "AAABKSPM" varchar(1) NOT NULL default ' ',
  "AAABLYCG" varchar(2) NOT NULL default ' ',
  "AAABCHGA" varchar(4) NOT NULL default ' ',
  "AAABCHGB" varchar(1) NOT NULL default ' ',
  "AAABEFVF" varchar(4) NOT NULL default ' ',
  "AAABLKXQ" varchar(4) NOT NULL default ' ',
  "AAABCHGE" varchar(1) NOT NULL default ' ',
  "AAABCHGF" varchar(1) NOT NULL default ' ',
  "AAABCHGG" varchar(1) NOT NULL default ' ',
  "AAABCHGH" varchar(1) NOT NULL default ' ',
  "AAABCHGI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLIBK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHGL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLIBK" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLIBK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHGT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIFR" varchar(4) NOT NULL default ' ',
  "AAABLIBK" varchar(4) NOT NULL default ' ',
  "AAABCHGX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLIFR","AAABLIBK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHHB" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABCHHE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHHH" (
  "AAABCHIF" varchar(1) NOT NULL default ' ',
  "AAABCHHL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABCHIF","AAABCHHL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHHM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIFT" varchar(1) NOT NULL default ' ',
  "AAABCHHT" varchar(1) NOT NULL default ' ',
  "AAABCHNO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLIFT","AAABCHHT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHHU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABLGLC" varchar(4) NOT NULL default ' ',
  "AAABCHIF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABLGLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHIC" (
  "AAABCHIF" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABCHIF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHIG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIFR" varchar(4) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABLGLC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLIFR","AAABMAMY","AAABLGLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHIP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGACI" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGACI","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHIY" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHJB" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHJG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGACI" varchar(2) NOT NULL default ' ',
  "AAABCHJJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGACI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHJM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGACI" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGACI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHJU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIFR" varchar(4) NOT NULL default ' ',
  "AAABLIBK" varchar(4) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAABCHKI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLIFR","AAABLIBK","AAABLZNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHKE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIFR" varchar(4) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAABCHKI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLIFR","AAABLZNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYCG" varchar(2) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYCG","AAABLZNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYCG" varchar(2) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYCG","AAABLZNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHLA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCHLY" varchar(10) NOT NULL default ' ',
  "AAABCHLD" varchar(1) NOT NULL default ' ',
  "AAABCHLE" varchar(1) NOT NULL default ' ',
  "AAABCHLF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCHLY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHLI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCHLY" varchar(10) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABCHLY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHLQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCHLY" varchar(10) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABCHLV" varchar(1) NOT NULL default ' ',
  "AAABCHLW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCHLY","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHMB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHAJ" varchar(6) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABLXID" varchar(4) NOT NULL default ' ',
  "AAABLXIC" varchar(3) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAABIYOR" varchar(4) NOT NULL default ' ',
  "AAABLIFR" varchar(4) NOT NULL default ' ',
  "AAABLIBK" varchar(4) NOT NULL default ' ',
  "AAABCHMP" varchar(4) NOT NULL default ' ',
  "AAABCHMQ" varchar(4) NOT NULL default ' ',
  "AAABCHMR" varchar(20) NOT NULL default ' ',
  "AAABCHMS" varchar(132) NOT NULL default ' ',
  "AAABCHMT" varchar(1) NOT NULL default ' ',
  "AAABCHMU" varchar(132) NOT NULL default ' ',
  "AAABCHMV" varchar(20) NOT NULL default ' ',
  "AAABCHMW" varchar(1) NOT NULL default ' ',
  "AAABCHMX" varchar(1) NOT NULL default ' ',
  "AAABCHMY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHAJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHNB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCHNO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCHNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHNG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCHNO" varchar(2) NOT NULL default ' ',
  "AAABCHNK" varchar(20) NOT NULL default ' ',
  "AAABCHNL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABCHNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHNP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCHOE" varchar(4) NOT NULL default ' ',
  "AAABCHNS" varchar(1) NOT NULL default ' ',
  "AAABCHNT" varchar(1) NOT NULL default ' ',
  "AAABCHNU" varchar(1) NOT NULL default ' ',
  "AAABCHNV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCHOE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHNY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCHOE" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCHOE","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHOH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCHOJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHOL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHFPO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABHFPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHOQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHFPO" varchar(40) NOT NULL default ' ',
  "AAABMDXM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABHFPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHOY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGNHE" varchar(6) NOT NULL default ' ',
  "AAABCTUP" varchar(1) NOT NULL default ' ',
  "AAABKYNY" varchar(250) NOT NULL default ' ',
  "AAABKYNZ" varchar(250) NOT NULL default ' ',
  "AAABKYOA" varchar(250) NOT NULL default ' ',
  "AAABKWJH" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGNHE","AAABCTUP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHPJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGNHE" varchar(6) NOT NULL default ' ',
  "AAABCTUP" varchar(1) NOT NULL default ' ',
  "AAABCHPN" varchar(1) NOT NULL default ' ',
  "AAABMLTD" varchar(3) NOT NULL default '000',
  "AAABCHPP" varchar(3) NOT NULL default '000',
  "AAABCHPQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGNHE","AAABCTUP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHPU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGNHE" varchar(6) NOT NULL default ' ',
  "AAABCTUP" varchar(1) NOT NULL default ' ',
  "AAABCTRJ" varchar(25) NOT NULL default ' ',
  "AAABGNCZ" varchar(50) NOT NULL default ' ',
  "AAABCTRI" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABGNHE","AAABCTUP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHQG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABFTSL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABFTSL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHQL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFTSL" varchar(4) NOT NULL default ' ',
  "AAABGNCZ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABFTSL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHQT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGNHE" varchar(6) NOT NULL default ' ',
  "AAABCTUP" varchar(1) NOT NULL default ' ',
  "AAABFTSL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGNHE","AAABCTUP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHRB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCHRR" varchar(4) NOT NULL default ' ',
  "AAABCTUP" varchar(1) NOT NULL default ' ',
  "AAABGNHE" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCHRR","AAABCTUP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHRJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCHRR" varchar(4) NOT NULL default ' ',
  "AAABCTUP" varchar(1) NOT NULL default ' ',
  "AAABMLMP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABCHRR","AAABCTUP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHRT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLQPX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLQPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHRY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLQPX" varchar(4) NOT NULL default ' ',
  "AAABCHTP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABLQPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHSG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLZNE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLZNE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHSL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLZNE" varchar(4) NOT NULL default ' ',
  "AAABGMNJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABLZNE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHST" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCHTF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCHTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHSY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCHTF" varchar(1) NOT NULL default ' ',
  "AAABCHTP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABCHTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHTG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHNUB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABHNUB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHTL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHNUB" varchar(1) NOT NULL default ' ',
  "AAABCHTP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABHNUB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHTT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGNHE" varchar(6) NOT NULL default ' ',
  "AAABLCCC" varchar(6) NOT NULL default ' ',
  "AAABCHTX" varchar(1) NOT NULL default ' ',
  "AAABCHTY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGNHE","AAABLCCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHUC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGGTM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGGTM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHUH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGGTM" varchar(2) NOT NULL default ' ',
  "AAABMLMP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABGGTM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHUP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJWNT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJWNT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHUU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJWNT" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABJWNT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHVC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCHWV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCHWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHVH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCHWV" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABCHWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHVP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCHWW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCHWW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHVU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCHWW" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABCHWW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHWC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCHWX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCHWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHWH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCHWX" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABCHWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHWP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCHWV" varchar(4) NOT NULL default ' ',
  "AAABCHWW" varchar(2) NOT NULL default ' ',
  "AAABCHWX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCHWV","AAABCHWW","AAABCHWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHWY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABMCPS" varchar(1) NOT NULL default ' ',
  "AAABKAFO" varchar(61) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKAPV" varchar(1) NOT NULL default ' ',
  "AAABLCFW" varchar(1) NOT NULL default ' ',
  "AAABCHXG" varchar(1) NOT NULL default ' ',
  "AAABCHXH" varchar(1) NOT NULL default ' ',
  "AAABCHXI" varchar(1) NOT NULL default ' ',
  "AAABCHXJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPWP","AAABMCPS","AAABKAFO","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHXP" (
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABMPMH" varchar(40) NOT NULL default ' ',
  "AAABJLPG" varchar(30) NOT NULL default ' ',
  "AAABLGAP" varchar(8) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABGYLC","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHXX" (
  "AAABCHYA" varchar(1) NOT NULL default ' ',
  "AAABCHXZ" varchar(254) NOT NULL default ' ',
  PRIMARY KEY ("AAABCHYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHYB" (
  "AAABEWWP" varchar(15) NOT NULL default ' ',
  "AAABCHYI" varchar(5) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABEWWP","AAABCHYI","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHYL" (
  "AAABEWWP" varchar(15) NOT NULL default ' ',
  "AAABCHYQ" varchar(5) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABEWWP","AAABCHYQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHYR" (
  "AAABEWWP" varchar(15) NOT NULL default ' ',
  "AAABCHYX" varchar(5) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABEWWP","AAABCHYX","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHYZ" (
  "AAABEWWP" varchar(15) NOT NULL default ' ',
  "AAABIMFT" varchar(15) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABEWWP","AAABIMFT","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHZI" (
  "AAABEWWP" varchar(15) NOT NULL default ' ',
  "AAABIMFT" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABEWWP","AAABIMFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHZN" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABEWWP" varchar(15) NOT NULL default ' ',
  "AAABIMFT" varchar(15) NOT NULL default ' ',
  "AAABCHZR" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABEWWP","AAABIMFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHZV" (
  "AAABEWWP" varchar(15) NOT NULL default ' ',
  "AAABCHZX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABEWWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCHZZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEWWP" varchar(15) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMNRG" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABEWWP","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIAJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEWWP" varchar(15) NOT NULL default ' ',
  "AAABCIAM" varchar(1) NOT NULL default ' ',
  "AAABCIAN" varchar(1) NOT NULL default ' ',
  "AAABCIAO" varchar(1) NOT NULL default ' ',
  "AAABCIAP" varchar(1) NOT NULL default ' ',
  "AAABCIAQ" varchar(1) NOT NULL default ' ',
  "AAABLUTZ" varchar(3) NOT NULL default '000',
  "AAABCIAS" varchar(1) NOT NULL default ' ',
  "AAABCIAT" varchar(1) NOT NULL default ' ',
  "AAABCIAU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEWWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIAX" (
  "AAABEWWP" varchar(15) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABCIBB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABEWWP","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIBF" (
  "AAABLOZE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIBI" (
  "AAABLOZE" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIBO" (
  "AAABLOZE" varchar(4) NOT NULL default ' ',
  "AAABMMYV" varchar(4) NOT NULL default ' ',
  "AAABLUEO" varchar(4) NOT NULL default '0000',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAABMMYV","AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIBW" (
  "AAABMMYV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMYV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIBZ" (
  "AAABMMYV" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMYV","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCICF" (
  "AAABMMYV" varchar(4) NOT NULL default ' ',
  "AAABLUEO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMMYV","AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCICK" (
  "AAABMMYV" varchar(4) NOT NULL default ' ',
  "AAABLUEO" varchar(4) NOT NULL default '0000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMYV","AAABLUEO","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCICS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJOTS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJOTS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCICX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCOLM" varchar(10) NOT NULL default ' ',
  "AAABCIFA" varchar(1) NOT NULL default ' ',
  "AAABCIFB" decimal(7,2) NOT NULL default '0.00',
  "AAABCIFC" varchar(3) NOT NULL default '000',
  "AAABCIFD" varchar(6) NOT NULL default ' ',
  "AAABCIFE" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLUXQ","AAABCOLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIDH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCOLM" varchar(10) NOT NULL default ' ',
  "AAABCOKL" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABCOLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIDP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCOLM" varchar(10) NOT NULL default ' ',
  "AAABCOKL" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABCOLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIDX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCOLM" varchar(10) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(10) NOT NULL default ' ',
  "AAABMNNA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCOLM","AAABMNML","AAABIJIG","AAABMNMZ","AAABMNNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIEK" (
  "AAABCOLM" varchar(10) NOT NULL default ' ',
  "AAABLUEO" varchar(4) NOT NULL default ' ',
  "AAABFZAX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABCOLM","AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIEQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCOLM" varchar(10) NOT NULL default ' ',
  "AAABLUEO" varchar(4) NOT NULL default ' ',
  "AAABFZAX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCOLM","AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIEY" (
  "AAABCOLM" varchar(10) NOT NULL default ' ',
  "AAABCIFA" varchar(1) NOT NULL default ' ',
  "AAABCIFB" decimal(7,2) NOT NULL default '0.00',
  "AAABCIFC" varchar(3) NOT NULL default '000',
  "AAABCIFD" varchar(6) NOT NULL default ' ',
  "AAABCIFE" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABCOLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIFG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCOLM" varchar(10) NOT NULL default ' ',
  "AAABLUVJ" varchar(20) NOT NULL default ' ',
  "AAABCOIG" varchar(20) NOT NULL default ' ',
  "AAABCOIC" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABCOLM","AAABLUVJ","AAABCOIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIFQ" (
  "AAABLUEO" varchar(4) NOT NULL default ' ',
  "AAABGXGV" varchar(3) NOT NULL default '000',
  "AAABCIFT" varchar(30) NOT NULL default ' ',
  "AAABCIFU" varchar(26) NOT NULL default ' ',
  "AAABCIFV" varchar(21) NOT NULL default ' ',
  "AAABCIFW" varchar(1) NOT NULL default ' ',
  "AAABCIFX" varchar(1) NOT NULL default ' ',
  "AAABCIFY" varchar(1) NOT NULL default ' ',
  "AAABCIFZ" varchar(1) NOT NULL default ' ',
  "AAABCIGA" varchar(132) NOT NULL default ' ',
  "AAABCIGB" varchar(1) NOT NULL default ' ',
  "AAABCIGC" varchar(1) NOT NULL default ' ',
  "AAABCIGD" varchar(1) NOT NULL default ' ',
  "AAABCIGE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUEO","AAABGXGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIGH" (
  "AAABLUEO" varchar(4) NOT NULL default ' ',
  "AAABGXGV" varchar(3) NOT NULL default '000',
  "AAABCIGK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUEO","AAABGXGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIGN" (
  "AAABLUEO" varchar(4) NOT NULL default ' ',
  "AAABGXGV" varchar(3) NOT NULL default '000',
  "AAABCIGQ" varchar(30) NOT NULL default ' ',
  "AAABCIGR" varchar(26) NOT NULL default ' ',
  "AAABCIGS" varchar(1) NOT NULL default ' ',
  "AAABCIGT" varchar(1) NOT NULL default ' ',
  "AAABCIGU" varchar(132) NOT NULL default ' ',
  "AAABCIGV" varchar(1) NOT NULL default ' ',
  "AAABCIGW" varchar(1) NOT NULL default ' ',
  "AAABCIGX" varchar(1) NOT NULL default ' ',
  "AAABCIGY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUEO","AAABGXGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIHB" (
  "AAABLUEO" varchar(4) NOT NULL default ' ',
  "AAABGXGV" varchar(3) NOT NULL default '000',
  "AAABCIHE" varchar(30) NOT NULL default ' ',
  "AAABCIHF" varchar(26) NOT NULL default ' ',
  "AAABCIHG" varchar(21) NOT NULL default ' ',
  "AAABCIHH" varchar(1) NOT NULL default ' ',
  "AAABCIHI" varchar(1) NOT NULL default ' ',
  "AAABCIHJ" varchar(1) NOT NULL default ' ',
  "AAABCIHK" varchar(1) NOT NULL default ' ',
  "AAABCIHL" varchar(132) NOT NULL default ' ',
  "AAABCIHM" varchar(1) NOT NULL default ' ',
  "AAABCIHN" varchar(1) NOT NULL default ' ',
  "AAABCIHO" varchar(1) NOT NULL default ' ',
  "AAABCIHP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUEO","AAABGXGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIHS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCOLM" varchar(10) NOT NULL default ' ',
  "AAABCIKX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCOLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIHY" (
  "AAABCOLM" varchar(10) NOT NULL default ' ',
  "AAABCIKX" varchar(1) NOT NULL default ' ',
  "AAABGYJW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABCOLM","AAABCIKX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIIE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCOLM" varchar(10) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABCOIS" varchar(10) NOT NULL default ' ',
  "AAABCOIT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCOLM","AAABMNML","AAABIJIG","AAABCOIS","AAABCOIT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIIR" (
  "AAABCOLM" varchar(10) NOT NULL default ' ',
  "AAABCIJC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABCOLM","AAABCIJC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIIW" (
  "AAABCOLM" varchar(10) NOT NULL default ' ',
  "AAABCIJC" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCIJA" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABCOLM","AAABCIJC","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIJE" (
  "AAABCOKA" varchar(4) NOT NULL default ' ',
  "AAABCOJJ" varchar(30) NOT NULL default ' ',
  "AAABCOJK" varchar(30) NOT NULL default ' ',
  "AAABCOJM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABCOKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIJK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCOKA" varchar(4) NOT NULL default ' ',
  "AAABCOJJ" varchar(30) NOT NULL default ' ',
  "AAABCOJK" varchar(30) NOT NULL default ' ',
  "AAABCOJM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCOKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIJS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCOKA" varchar(4) NOT NULL default ' ',
  "AAABCOJT" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABCOKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIKA" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCOKA" varchar(4) NOT NULL default ' ',
  "AAABCOJT" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABCOKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIKG" (
  "AAABCOLM" varchar(10) NOT NULL default ' ',
  "AAABCOKD" varchar(20) NOT NULL default ' ',
  "AAABCOKE" varchar(20) NOT NULL default ' ',
  "AAABCIKU" varchar(30) NOT NULL default ' ',
  "AAABCIKV" varchar(30) NOT NULL default ' ',
  "AAABCIKW" varchar(1) NOT NULL default ' ',
  "AAABCIKX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABCOLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIKP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCOLM" varchar(10) NOT NULL default ' ',
  "AAABCOKD" varchar(20) NOT NULL default ' ',
  "AAABCOKE" varchar(20) NOT NULL default ' ',
  "AAABCIKU" varchar(30) NOT NULL default ' ',
  "AAABCIKV" varchar(30) NOT NULL default ' ',
  "AAABCIKW" varchar(1) NOT NULL default ' ',
  "AAABCIKX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCOLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCILA" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCOLM" varchar(10) NOT NULL default ' ',
  "AAABCOKL" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABCOLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCILG" (
  "AAABLUEO" varchar(4) NOT NULL default ' ',
  "AAABCILI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCILK" (
  "AAABLUEO" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUEO","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCILQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDRC" varchar(40) NOT NULL default ' ',
  "AAABCILT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDRC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCILW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDRC" varchar(40) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDRC","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIME" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCINK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCINK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIMJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCINK" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCINK","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIMR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCINK" varchar(4) NOT NULL default ' ',
  "AAABCOLW" varchar(3) NOT NULL default '000',
  "AAABCIMZ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABCINK","AAABCOLW","AAABCIMZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCINA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCINK" varchar(4) NOT NULL default ' ',
  "AAABCOLW" varchar(3) NOT NULL default '000',
  "AAABLOSU" varchar(40) NOT NULL default ' ',
  "AAABCINF" varchar(14) NOT NULL default ' ',
  "AAABCING" varchar(32) NOT NULL default ' ',
  "AAABCINH" varchar(1) NOT NULL default ' ',
  "AAABCINI" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCINK","AAABCOLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCINM" (
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABINMV" varchar(3) NOT NULL default ' ',
  "AAABINMW" varchar(1) NOT NULL default '0',
  "AAABINMX" varchar(3) NOT NULL default ' ',
  "AAABINMN" varchar(1) NOT NULL default ' ',
  "AAABLUUW" varchar(1) NOT NULL default ' ',
  "AAABIISD" varchar(8) NOT NULL default '00000000',
  "AAABIISE" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABMREL","AAABINMV","AAABINMW","AAABINMX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIOB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGMXF" varchar(4) NOT NULL default ' ',
  "AAABHXNO" varchar(4) NOT NULL default ' ',
  "AAABCJRX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGMXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIOI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKVKH" varchar(6) NOT NULL default ' ',
  "AAABCIOL" varchar(1) NOT NULL default ' ',
  "AAABCIOM" varchar(1) NOT NULL default ' ',
  "AAABCION" varchar(1) NOT NULL default ' ',
  "AAABCIOO" varchar(1) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABCJSI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKVKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIOT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(6) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  "AAABMECK" varchar(1) NOT NULL default ' ',
  "AAABCIOY" varchar(1) NOT NULL default ' ',
  "AAABCIOZ" varchar(1) NOT NULL default ' ',
  "AAABCIPA" varchar(1) NOT NULL default ' ',
  "AAABCIPB" varchar(1) NOT NULL default ' ',
  "AAABCIPC" varchar(6) NOT NULL default ' ',
  "AAABCIPD" varchar(6) NOT NULL default ' ',
  "AAABCIPE" varchar(6) NOT NULL default ' ',
  "AAABCIPF" varchar(6) NOT NULL default ' ',
  "AAABCIPG" varchar(1) NOT NULL default ' ',
  "AAABCIPH" varchar(12) NOT NULL default ' ',
  "AAABCIPI" varchar(12) NOT NULL default ' ',
  "AAABCIPJ" varchar(12) NOT NULL default ' ',
  "AAABCIPK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIPO" (
  "AAABCLVY" varchar(6) NOT NULL default ' ',
  "AAABCIPQ" varchar(30) NOT NULL default ' ',
  "AAABCIPR" varchar(30) NOT NULL default ' ',
  "AAABCIPS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABCLVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIPU" (
  "AAABCLVY" varchar(6) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCIPX" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABCLVY","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIQA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUVS" varchar(6) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUVS","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIQK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCIQQ" varchar(4) NOT NULL default ' ',
  "AAABCOVJ" varchar(4) NOT NULL default ' ',
  "AAABCIQO" decimal(10,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAABCIQQ","AAABCOVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIQS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABCIQW" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIRA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABCIRD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIRG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABCIRN" varchar(1) NOT NULL default ' ',
  "AAABMPSX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABCIRN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIRO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABCIRR" varchar(1) NOT NULL default ' ',
  "AAABCIRS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIRV" (
  "AAABDNIV" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLFWP" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNIV","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCISB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCISN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCISN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCISG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCISN" varchar(1) NOT NULL default ' ',
  "AAABCISK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCISN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCISO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNIV" varchar(10) NOT NULL default ' ',
  "AAABCIUV" varchar(2) NOT NULL default ' ',
  "AAABCITI" varchar(4) NOT NULL default '0000',
  "AAABCITJ" VARBINARY(255) NOT NULL,
  "AAABCISU" varchar(1) NOT NULL default ' ',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAABCISW" varchar(3) NOT NULL default ' ',
  "AAABCISX" int(11) NOT NULL default '0',
  "AAABCISY" varchar(30) NOT NULL default ' ',
  "AAABCISZ" varchar(2) NOT NULL default ' ',
  "AAABCITA" varchar(45) NOT NULL default ' ',
  "AAABCITB" varchar(45) NOT NULL default ' ',
  "AAABLRSS" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMBYX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABDNIV","AAABCIUV","AAABCITI","AAABCITJ"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCITK" (
  "AAABCIUI" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABCIUI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCITN" (
  "AAABCIUI" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLFZO" varchar(10) NOT NULL default ' ',
  "AAABLFWP" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABCIUI","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCITU" (
  "AAABDNIV" varchar(10) NOT NULL default ' ',
  "AAABIYCS" varchar(2) NOT NULL default ' ',
  "AAABCITX" varchar(30) NOT NULL default ' ',
  "AAABCITY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNIV","AAABIYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIUB" (
  "AAABCIXA" varchar(2) NOT NULL default ' ',
  "AAABCIUH" varchar(2) NOT NULL default ' ',
  "AAABCIUI" varchar(2) NOT NULL default ' ',
  "AAABCIUF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABCIXA","AAABCIUH","AAABCIUI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIUJ" (
  "AAABDNIV" varchar(10) NOT NULL default ' ',
  "AAABEUUB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIUN" (
  "AAABCIUV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABCIUV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIUQ" (
  "AAABCIUV" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLFZO" varchar(10) NOT NULL default ' ',
  "AAABLFWP" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABCIUV","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIUX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNIV" varchar(10) NOT NULL default ' ',
  "AAABCIXA" varchar(2) NOT NULL default ' ',
  "AAABCIVG" varchar(2) NOT NULL default '00',
  "AAABFTRT" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABDNIV","AAABCIXA","AAABCIVG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIVH" (
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIVK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLFZO" varchar(10) NOT NULL default ' ',
  "AAABLFWP" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRFO","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIVT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABLYRW" varchar(6) NOT NULL default ' ',
  "AAABCIVX" varchar(3) NOT NULL default ' ',
  "AAABCIVY" varchar(15) NOT NULL default ' ',
  "AAABCIVZ" varchar(35) NOT NULL default ' ',
  "AAABCIWA" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABLYRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIWE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABCIWO" varchar(3) NOT NULL default ' ',
  "AAABCIWP" varchar(15) NOT NULL default ' ',
  "AAABCNNC" varchar(5) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAABLYRW" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABCIWO","AAABCIWP","AAABCNNC","AAABMCTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIWS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABDNIV" varchar(10) NOT NULL default ' ',
  "AAABCIXA" varchar(2) NOT NULL default ' ',
  "AAABCIWW" varchar(8) NOT NULL default '00000000',
  "AAABCIWX" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLUXQ","AAABDNIV","AAABCIXA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIXB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABCIXJ" varchar(35) NOT NULL default ' ',
  "AAABCNIO" varchar(4) NOT NULL default ' ',
  "AAABCIXG" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABCIXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIXK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCIXP" varchar(4) NOT NULL default ' ',
  "AAABCIXQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCIXP","AAABCIXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIXR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCJDR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCJDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIXW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCJDR" varchar(2) NOT NULL default ' ',
  "AAABCIYA" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCJDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIYE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABCIYH" varchar(2) NOT NULL default '00',
  "AAABCIYI" decimal(23,2) NOT NULL default '0.00',
  "AAABCIYJ" varchar(1) NOT NULL default ' ',
  "AAABCIYK" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABHFHO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIYN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCIYY" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCIYY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIYS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCIYY" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCIYW" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCIYY","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCIZL" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCIZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIZF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCIZL" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCIZJ" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCIZL","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIZN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABIYJJ" varchar(3) NOT NULL default ' ',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  "AAABCIZS" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLEHK","AAABIYJJ","AAABHFHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCIZX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABIYJJ" varchar(3) NOT NULL default ' ',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  "AAABCJAD" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLEHK","AAABIYJJ","AAABHFHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJAJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCJAU" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCJAU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJAO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCJAU" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCJAS" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCJAU","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJAW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABIYJJ" varchar(3) NOT NULL default ' ',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABCJBB" varchar(4) NOT NULL default ' ',
  "AAABCJBC" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABLEHK","AAABIYJJ","AAABKYMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJBH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABIYJJ" varchar(3) NOT NULL default ' ',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABCJBN" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLEHK","AAABIYJJ","AAABKYMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJBT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABIYJJ" varchar(3) NOT NULL default ' ',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABCNYI" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLEHK","AAABIYJJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJCC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABIYJJ" varchar(3) NOT NULL default ' ',
  "AAABIFHG" varchar(6) NOT NULL default ' ',
  "AAABCJCH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLEHK","AAABIYJJ","AAABIFHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJCM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABIYJJ" varchar(3) NOT NULL default ' ',
  "AAABIFHG" varchar(6) NOT NULL default ' ',
  "AAABCJCS" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLEHK","AAABIYJJ","AAABIFHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJCY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCJEQ" varchar(2) NOT NULL default '00',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCJEQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJDE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCJEQ" varchar(2) NOT NULL default '00',
  "AAABCJDI" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCJEQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJDM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABCJDQ" varchar(1) NOT NULL default ' ',
  "AAABCJDR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABKYMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJDV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCJEQ" varchar(2) NOT NULL default '00',
  "AAABJCXS" varchar(2) NOT NULL default ' ',
  "AAABLRLE" varchar(8) NOT NULL default '00000000',
  "AAABLRLF" varchar(8) NOT NULL default '00000000',
  "AAABCJEB" varchar(5) NOT NULL default '00000',
  "AAABCJEC" varchar(5) NOT NULL default ' ',
  "AAABCJED" decimal(23,2) NOT NULL default '0.00',
  "AAABMKXF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCJEQ","AAABJCXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJEI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCJEQ" varchar(2) NOT NULL default '00',
  "AAABJCXS" varchar(2) NOT NULL default ' ',
  "AAABCJEN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCJEQ","AAABJCXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJES" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABCJFB" varchar(3) NOT NULL default '000',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABKYMV","AAABCJFB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJFC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(6) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  "AAABDPQX" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJFK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUVS" varchar(6) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUVS","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJFU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(6) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  "AAABDPQW" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJGC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUVS" varchar(6) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUVS","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJGM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDPHX" varchar(4) NOT NULL default ' ',
  "AAABDPHY" varchar(12) NOT NULL default ' ',
  "AAABDPHZ" varchar(1) NOT NULL default ' ',
  "AAABCJGR" varchar(6) NOT NULL default ' ',
  "AAABCJGS" varchar(6) NOT NULL default ' ',
  "AAABDPHJ" varchar(6) NOT NULL default ' ',
  "AAABCJGU" varchar(6) NOT NULL default ' ',
  "AAABCJGV" varchar(6) NOT NULL default ' ',
  "AAABCJGW" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDPHX","AAABDPHY","AAABDPHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJHB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDPHX" varchar(4) NOT NULL default ' ',
  "AAABDPHY" varchar(12) NOT NULL default ' ',
  "AAABDPHZ" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDPHX","AAABDPHY","AAABDPHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJHN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABENQT" varchar(10) NOT NULL default ' ',
  "AAABCJKN" varchar(2) NOT NULL default '00',
  "AAABCJKB" varchar(1) NOT NULL default ' ',
  "AAABCJKC" varchar(3) NOT NULL default ' ',
  "AAABCJKD" varchar(15) NOT NULL default ' ',
  "AAABCJKE" varchar(35) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABENQT","AAABCJKN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJIA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABCJKK" varchar(3) NOT NULL default ' ',
  "AAABCJKL" varchar(15) NOT NULL default ' ',
  "AAABCJKG" varchar(3) NOT NULL default ' ',
  "AAABCJKH" varchar(15) NOT NULL default ' ',
  "AAABMCZD" varchar(2) NOT NULL default ' ',
  "AAABENQT" varchar(10) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPWL","AAABCJKK","AAABCJKL","AAABCJKG","AAABCJKH","AAABMCZD"),
  UNIQUE KEY "AAABCJIS" ("AAABMRIL","AAABMPWL","AAABCJKG","AAABCJKH","AAABCJKK","AAABCJKL","AAABMCZD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJJA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCJKG" varchar(3) NOT NULL default ' ',
  "AAABCJKH" varchar(15) NOT NULL default ' ',
  "AAABCJKI" varchar(35) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABCJKK" varchar(3) NOT NULL default ' ',
  "AAABCJKL" varchar(15) NOT NULL default ' ',
  "AAABMCZD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCJKG","AAABCJKH","AAABCJKI","AAABMPWL","AAABCJKK","AAABCJKL","AAABMCZD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJJR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCJKG" varchar(3) NOT NULL default ' ',
  "AAABCJKH" varchar(15) NOT NULL default ' ',
  "AAABCJKI" varchar(35) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABCJKK" varchar(3) NOT NULL default ' ',
  "AAABCJKL" varchar(15) NOT NULL default ' ',
  "AAABMCZD" varchar(2) NOT NULL default ' ',
  "AAABCJKN" varchar(2) NOT NULL default '00',
  "AAABCJKB" varchar(1) NOT NULL default ' ',
  "AAABCJKC" varchar(3) NOT NULL default ' ',
  "AAABCJKD" varchar(15) NOT NULL default ' ',
  "AAABCJKE" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCJKG","AAABCJKH","AAABCJKI","AAABMPWL","AAABCJKK","AAABCJKL","AAABMCZD","AAABCJKN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCJLK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJKS" (
  "AAABCJLK" varchar(4) NOT NULL default ' ',
  "AAABCJKU" varchar(1) NOT NULL default ' ',
  "AAABCJKV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABCJLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJKX" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCJLK" varchar(4) NOT NULL default ' ',
  "AAABCJLA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABCJLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJLD" (
  "AAABCJLK" varchar(4) NOT NULL default ' ',
  "AAABCJLL" varchar(1) NOT NULL default '0',
  "AAABCJLG" varchar(1) NOT NULL default ' ',
  "AAABCJLH" varchar(1) NOT NULL default ' ',
  "AAABCJLI" varchar(1) NOT NULL default ' ',
  "AAABCJLJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABCJLK","AAABCJLL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJLM" (
  "AAABIJUH" varchar(3) NOT NULL default '000',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABCJLZ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABIJUH","AAABLTJH","AAABCJLZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJLT" (
  "AAABIJUH" varchar(3) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABCJLZ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABIJUH","AAABLTJH","AAABCJLZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJMA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(6) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  "AAABDPQX" varchar(6) NOT NULL default ' ',
  "AAABCJNN" varchar(1) NOT NULL default ' ',
  "AAABMKOT" int(11) NOT NULL default '0',
  "AAABCJMH" int(11) NOT NULL default '0',
  "AAABLFTW" int(11) NOT NULL default '0',
  "AAABHAGF" int(11) NOT NULL default '0',
  "AAABCJNO" varchar(2) NOT NULL default '00',
  "AAABDPQW" varchar(6) NOT NULL default ' ',
  "AAABCJMM" varchar(3) NOT NULL default '000',
  "AAABCJMN" varchar(1) NOT NULL default ' ',
  "AAABCJMO" varchar(6) NOT NULL default ' ',
  "AAABCJMP" varchar(12) NOT NULL default ' ',
  "AAABCJMQ" varchar(1) NOT NULL default ' ',
  "AAABCJMR" int(11) NOT NULL default '0',
  "AAABCJMS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJMW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUVS" varchar(6) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUVS","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJNG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(6) NOT NULL default ' ',
  "AAABDPTM" varchar(3) NOT NULL default '000',
  "AAABDPTN" varchar(3) NOT NULL default '000',
  "AAABDPTO" varchar(3) NOT NULL default '000',
  "AAABDPTP" varchar(3) NOT NULL default '000',
  "AAABCJNN" varchar(1) NOT NULL default ' ',
  "AAABCJNO" varchar(2) NOT NULL default '00',
  "AAABCJNP" varchar(1) NOT NULL default ' ',
  "AAABLEEP" varchar(1) NOT NULL default ' ',
  "AAABCJNR" varchar(1) NOT NULL default ' ',
  "AAABCJNS" varchar(1) NOT NULL default ' ',
  "AAABCJNT" varchar(1) NOT NULL default ' ',
  "AAABCJNU" varchar(1) NOT NULL default ' ',
  "AAABCJNV" varchar(1) NOT NULL default ' ',
  "AAABCJNW" varchar(1) NOT NULL default ' ',
  "AAABCJNX" varchar(1) NOT NULL default ' ',
  "AAABCJNY" varchar(1) NOT NULL default ' ',
  "AAABCJNZ" varchar(1) NOT NULL default ' ',
  "AAABCJOA" varchar(1) NOT NULL default ' ',
  "AAABLTXX" varchar(4) NOT NULL default '0000',
  "AAABCJOC" varchar(1) NOT NULL default ' ',
  "AAABCJOD" varchar(1) NOT NULL default ' ',
  "AAABDPVO" varchar(1) NOT NULL default ' ',
  "AAABCJOF" varchar(1) NOT NULL default ' ',
  "AAABCJOG" varchar(2) NOT NULL default '00',
  "AAABCJOH" varchar(1) NOT NULL default ' ',
  "AAABDPVR" varchar(1) NOT NULL default ' ',
  "AAABCJOJ" varchar(3) NOT NULL default '000',
  "AAABCJOK" varchar(1) NOT NULL default '0',
  "AAABFUQE" varchar(4) NOT NULL default '0000',
  "AAABDPTZ" varchar(1) NOT NULL default ' ',
  "AAABDPTH" varchar(6) NOT NULL default '000000',
  "AAABCJOO" varchar(1) NOT NULL default ' ',
  "AAABCJOP" varchar(1) NOT NULL default ' ',
  "AAABCJOQ" varchar(1) NOT NULL default ' ',
  "AAABDFYR" varchar(1) NOT NULL default ' ',
  "AAABCJOS" varchar(3) NOT NULL default '000',
  "AAABCJOT" varchar(1) NOT NULL default ' ',
  "AAABCJOU" varchar(1) NOT NULL default ' ',
  "AAABCJOV" varchar(1) NOT NULL default ' ',
  "AAABCJOW" varchar(1) NOT NULL default ' ',
  "AAABCJOX" varchar(1) NOT NULL default ' ',
  "AAABCJOY" varchar(2) NOT NULL default '00',
  "AAABCJOZ" varchar(1) NOT NULL default ' ',
  "AAABDSFX" varchar(1) NOT NULL default ' ',
  "AAABCJPB" varchar(3) NOT NULL default ' ',
  "AAABCJPC" varchar(1) NOT NULL default ' ',
  "AAABCJPD" varchar(11) NOT NULL default ' ',
  "AAABDPTD" varchar(5) NOT NULL default '00000',
  "AAABCJPF" varchar(11) NOT NULL default ' ',
  "AAABCJPG" varchar(1) NOT NULL default ' ',
  "AAABCJPH" varchar(3) NOT NULL default '000',
  "AAABDPUH" varchar(1) NOT NULL default ' ',
  "AAABDPUJ" varchar(4) NOT NULL default '0000',
  "AAABDPUK" varchar(2) NOT NULL default '00',
  "AAABCJPL" varchar(1) NOT NULL default ' ',
  "AAABDPUN" varchar(1) NOT NULL default ' ',
  "AAABCJPN" varchar(1) NOT NULL default ' ',
  "AAABCJPO" varchar(1) NOT NULL default ' ',
  "AAABCJPP" varchar(1) NOT NULL default ' ',
  "AAABCJPQ" varchar(1) NOT NULL default ' ',
  "AAABCTKA" varchar(1) NOT NULL default ' ',
  "AAABCJPS" varchar(1) NOT NULL default ' ',
  "AAABCJPT" varchar(1) NOT NULL default ' ',
  "AAABEZGI" varchar(1) NOT NULL default ' ',
  "AAABDPTW" varchar(1) NOT NULL default ' ',
  "AAABCJPW" varchar(1) NOT NULL default ' ',
  "AAABCJPX" varchar(1) NOT NULL default ' ',
  "AAABCJPY" varchar(1) NOT NULL default ' ',
  "AAABCJPZ" varchar(1) NOT NULL default ' ',
  "AAABCJQA" varchar(1) NOT NULL default ' ',
  "AAABCJQB" varchar(1) NOT NULL default ' ',
  "AAABCJQC" varchar(1) NOT NULL default '0',
  "AAABCJQD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJQG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUVS" varchar(6) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJQO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCJQY" varchar(12) NOT NULL default ' ',
  "AAABCJQZ" varchar(12) NOT NULL default ' ',
  "AAABCJRA" varchar(15) NOT NULL default ' ',
  "AAABCJRB" varchar(15) NOT NULL default ' ',
  "AAABCJRC" varchar(15) NOT NULL default ' ',
  "AAABCJRD" varchar(2) NOT NULL default ' ',
  "AAABLJNI" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCJQY","AAABCJQZ","AAABCJRA","AAABCJRB","AAABCJRC","AAABCJRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJRE" (
  "AAABDIKT" varchar(1) NOT NULL default ' ',
  "AAABIJQL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABDIKT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJRI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMGE" varchar(10) NOT NULL default ' ',
  "AAABLMOC" varchar(8) NOT NULL default '00000000',
  "AAABLMOD" varchar(6) NOT NULL default '000000',
  "AAABCKRR" varchar(12) NOT NULL default ' ',
  "AAABCKGN" varchar(10) NOT NULL default ' ',
  "AAABCJTG" varchar(8) NOT NULL default '00000000',
  "AAABLEQN" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABIMGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJRT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGMXF" varchar(4) NOT NULL default ' ',
  "AAABMLMR" varchar(4) NOT NULL default ' ',
  "AAABCJRX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGMXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJSA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKVKH" varchar(6) NOT NULL default ' ',
  "AAABCJSD" varchar(1) NOT NULL default ' ',
  "AAABCJSE" varchar(1) NOT NULL default ' ',
  "AAABCJSF" varchar(1) NOT NULL default ' ',
  "AAABCJSG" varchar(1) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABCJSI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKVKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJSL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIMGE" varchar(10) NOT NULL default ' ',
  "AAABCKXF" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABIMGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJST" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMGE" varchar(10) NOT NULL default ' ',
  "AAABCJTG" varchar(8) NOT NULL default '00000000',
  "AAABCJUZ" varchar(10) NOT NULL default ' ',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABLMJO" varchar(10) NOT NULL default ' ',
  "AAABCKWN" varchar(4) NOT NULL default '0000',
  "AAABLPJD" varchar(30) NOT NULL default ' ',
  "AAABCJTM" varchar(70) NOT NULL default ' ',
  "AAABLEQN" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABIMGE","AAABCJTG","AAABCJUZ","AAABLMHB","AAABLMJO","AAABCKWN","AAABLPJD","AAABCJTM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJTN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMGE" varchar(10) NOT NULL default ' ',
  "AAABCJUZ" varchar(10) NOT NULL default ' ',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABLMJO" varchar(10) NOT NULL default ' ',
  "AAABCKWN" varchar(4) NOT NULL default '0000',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAABEPJZ" varchar(1) NOT NULL default ' ',
  "AAABCKXL" varchar(30) NOT NULL default ' ',
  "AAABCKWH" varchar(70) NOT NULL default ' ',
  "AAABCKWI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMGE","AAABCJUZ","AAABLMHB","AAABLMJO","AAABCKWN","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJUG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMGE" varchar(10) NOT NULL default ' ',
  "AAABCJUZ" varchar(10) NOT NULL default ' ',
  "AAABCKVK" varchar(10) NOT NULL default ' ',
  "AAABLUEO" varchar(32) NOT NULL default ' ',
  "AAABLMJO" varchar(10) NOT NULL default ' ',
  "AAABLUFA" varchar(1) NOT NULL default ' ',
  "AAABCKUQ" varchar(32) NOT NULL default ' ',
  "AAABCKWN" varchar(4) NOT NULL default '0000',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAABEPJZ" varchar(1) NOT NULL default ' ',
  "AAABCKXL" varchar(30) NOT NULL default ' ',
  "AAABCKWH" varchar(70) NOT NULL default ' ',
  "AAABCKBB" varchar(10) NOT NULL default ' ',
  "AAABCKSX" varchar(32) NOT NULL default ' ',
  "AAABCKWI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMGE","AAABCJUZ","AAABCKVK","AAABLUEO","AAABLMJO","AAABLUFA","AAABCKUQ","AAABCKWN","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJVH" (
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABKAOU" varchar(27) NOT NULL default ' ',
  "AAABLPJD" varchar(30) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABGBAQ" varchar(4) NOT NULL default '0000',
  "AAABCJWJ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLMHB","AAABKAOU","AAABLPJD","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJVT" (
  "AAABLPJD" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJVY" (
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABCJWA" varchar(30) NOT NULL default ' ',
  "AAABCJWB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJWD" (
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABKAOU" varchar(27) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABCJWH" varchar(10) NOT NULL default ' ',
  "AAABGBAQ" varchar(4) NOT NULL default '0000',
  "AAABCJWJ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLMHB","AAABKAOU","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJWN" (
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABLLIO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJWR" (
  "AAABCKVK" varchar(10) NOT NULL default ' ',
  "AAABLUEO" varchar(32) NOT NULL default ' ',
  "AAABLUFA" varchar(1) NOT NULL default ' ',
  "AAABCKUQ" varchar(32) NOT NULL default ' ',
  "AAABCKXL" varchar(30) NOT NULL default ' ',
  "AAABCJWX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABCKVK","AAABLUEO","AAABLUFA","AAABCKUQ","AAABCKXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJXD" (
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABCKWI" varchar(30) NOT NULL default ' ',
  "AAABCKXL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMHB","AAABCKWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJXJ" (
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJXN" (
  "AAABCKVK" varchar(10) NOT NULL default ' ',
  "AAABLUEO" varchar(32) NOT NULL default ' ',
  "AAABCJXV" varchar(30) NOT NULL default ' ',
  "AAABLAVR" varchar(30) NOT NULL default ' ',
  "AAABHYAT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABCKVK","AAABLUEO","AAABCJXV","AAABLAVR","AAABHYAT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJXY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDEKG" varchar(2) NOT NULL default ' ',
  "AAABDEKF" varchar(10) NOT NULL default ' ',
  "AAABDEKH" varchar(2) NOT NULL default ' ',
  "AAABEDRM" varchar(2) NOT NULL default ' ',
  "AAABLMHG" varchar(10) NOT NULL default ' ',
  "AAABCKEF" varchar(2) NOT NULL default ' ',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABKAOU" varchar(27) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDEKG","AAABDEKF","AAABDEKH","AAABEDRM","AAABLMHG","AAABCKEF","AAABLMHB","AAABKAOU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJYR" (
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABKAOU" varchar(27) NOT NULL default ' ',
  "AAABJEMU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMHB","AAABKAOU","AAABJEMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJYY" (
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABKAOU" varchar(27) NOT NULL default ' ',
  "AAABJEMU" varchar(30) NOT NULL default ' ',
  "AAABCKGF" varchar(15) NOT NULL default ' ',
  "AAABCJZJ" varchar(30) NOT NULL default ' ',
  "AAABCJZK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMHB","AAABKAOU","AAABJEMU","AAABCKGF","AAABCJZJ","AAABCJZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJZL" (
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABKAOU" varchar(27) NOT NULL default ' ',
  "AAABJEMU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMHB","AAABKAOU","AAABJEMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJZS" (
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABKAOU" varchar(27) NOT NULL default ' ',
  "AAABJEMU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMHB","AAABKAOU","AAABJEMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCJZZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDEKG" varchar(2) NOT NULL default ' ',
  "AAABDEKF" varchar(10) NOT NULL default ' ',
  "AAABDEKH" varchar(2) NOT NULL default ' ',
  "AAABEDRM" varchar(2) NOT NULL default ' ',
  "AAABLMHG" varchar(10) NOT NULL default ' ',
  "AAABCKEF" varchar(2) NOT NULL default ' ',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABKAOU" varchar(27) NOT NULL default ' ',
  "AAABCKAJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDEKG","AAABDEKF","AAABDEKH","AAABEDRM","AAABLMHG","AAABCKEF","AAABLMHB","AAABKAOU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKAT" (
  "AAABCKVK" varchar(10) NOT NULL default ' ',
  "AAABLUEO" varchar(32) NOT NULL default ' ',
  "AAABCKBB" varchar(10) NOT NULL default ' ',
  "AAABCKSX" varchar(32) NOT NULL default ' ',
  "AAABCKXL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABCKVK","AAABLUEO","AAABCKBB","AAABCKSX","AAABCKXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKBE" (
  "AAABCKVK" varchar(10) NOT NULL default ' ',
  "AAABLUEO" varchar(32) NOT NULL default ' ',
  "AAABCKWI" varchar(30) NOT NULL default ' ',
  "AAABCKXL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABCKVK","AAABLUEO","AAABCKWI","AAABCKXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKBN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCKRG" varchar(15) NOT NULL default ' ',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABCKMB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABCKRG","AAABLMHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKBV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCKRG" varchar(15) NOT NULL default ' ',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABCKCA" smallint(6) NOT NULL default '0',
  "AAABCKCB" varchar(1) NOT NULL default ' ',
  "AAABDKHE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCKRG","AAABLMHB","AAABMQDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKCH" (
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABCKCJ" varchar(1) NOT NULL default ' ',
  "AAABCKCK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKGJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKCM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGBMM" varchar(30) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABCKCQ" varchar(2) NOT NULL default '00',
  "AAABLNDO" varchar(32) NOT NULL default ' ',
  "AAABCKCS" varchar(32) NOT NULL default ' ',
  "AAABCKCT" varchar(10) NOT NULL default ' ',
  "AAABCKCU" varchar(10) NOT NULL default ' ',
  "AAABCKCV" varchar(10) NOT NULL default ' ',
  "AAABCKCW" varchar(32) NOT NULL default ' ',
  "AAABCKCX" varchar(32) NOT NULL default ' ',
  "AAABCKCY" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGBMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKDB" (
  "AAABLFYC" varchar(10) NOT NULL default ' ',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFYC","AAABLMHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKDG" (
  "AAABLFYC" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFYC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKDJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDEKF" varchar(10) NOT NULL default ' ',
  "AAABDEKG" varchar(2) NOT NULL default ' ',
  "AAABDEKH" varchar(2) NOT NULL default ' ',
  "AAABLFYC" varchar(10) NOT NULL default ' ',
  "AAABCKEB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDEKF","AAABDEKG","AAABDEKH","AAABLFYC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKDV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMHG" varchar(10) NOT NULL default ' ',
  "AAABEDRM" varchar(2) NOT NULL default ' ',
  "AAABCKEF" varchar(2) NOT NULL default ' ',
  "AAABLFYC" varchar(10) NOT NULL default ' ',
  "AAABCKEB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMHG","AAABEDRM","AAABCKEF","AAABLFYC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKEH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCKFI" varchar(10) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABCKFM" varchar(10) NOT NULL default ' ',
  "AAABCKFU" varchar(250) NOT NULL default ' ',
  "AAABCKFN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCKFI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKES" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCKFI" varchar(10) NOT NULL default ' ',
  "AAABCKFO" varchar(10) NOT NULL default ' ',
  "AAABCKFP" varchar(10) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  "AAABMGSU" varchar(6) NOT NULL default '000000',
  "AAABMGSS" varchar(12) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCKFI","AAABCKFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKFE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLUSV" varchar(14) NOT NULL default ' ',
  "AAABCKFX" varchar(32) NOT NULL default ' ',
  "AAABCKFI" varchar(10) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABCKFM" varchar(10) NOT NULL default ' ',
  "AAABCKFN" varchar(20) NOT NULL default ' ',
  "AAABCKFO" varchar(10) NOT NULL default ' ',
  "AAABCKFP" varchar(10) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  "AAABMGSU" varchar(6) NOT NULL default '000000',
  "AAABMGSS" varchar(12) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABCKFU" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLUSV","AAABCKFX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKFY" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABCKGF" varchar(15) NOT NULL default ' ',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABJEMU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABCKGF","AAABLMHB","AAABMREL","AAABJEMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKGJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCKHZ" varchar(6) NOT NULL default ' ',
  "AAABJFLQ" varchar(8) NOT NULL default '00000000',
  "AAABCKGN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCKHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKGQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCKHZ" varchar(6) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCKHZ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKGY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCKHG" varchar(1) NOT NULL default ' ',
  "AAABCKHH" varchar(30) NOT NULL default ' ',
  "AAABHBMG" varchar(120) NOT NULL default ' ',
  "AAABCKHZ" varchar(6) NOT NULL default ' ',
  "AAABLSBE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCKHG","AAABCKHH","AAABHBMG","AAABCKHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKHK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAABCKIL" varchar(10) NOT NULL default ' ',
  "AAABCKHZ" varchar(6) NOT NULL default ' ',
  "AAABCKID" varchar(1) NOT NULL default ' ',
  "AAABJAEL" varchar(8) NOT NULL default '00000000',
  "AAABLOCX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABILWI","AAABCKIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKHV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAABCKIL" varchar(10) NOT NULL default ' ',
  "AAABCKHZ" varchar(6) NOT NULL default ' ',
  "AAABJAEL" varchar(8) NOT NULL default '00000000',
  "AAABLOCX" varchar(6) NOT NULL default '000000',
  "AAABCKIC" varchar(12) NOT NULL default ' ',
  "AAABCKID" varchar(1) NOT NULL default ' ',
  "AAABFVIF" varchar(1) NOT NULL default ' ',
  "AAABCKIF" varchar(16) NOT NULL default '0000000000000000',
  "AAABCKIG" varchar(8) NOT NULL default '00000000',
  "AAABCKIH" varchar(6) NOT NULL default '000000',
  "AAABCKII" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABILWI","AAABCKIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKIM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKIQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABCKIU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKIX" (
  "AAABIWWJ" varchar(30) NOT NULL default ' ',
  "AAABMIRY" varchar(10) NOT NULL default ' ',
  "AAABLUEO" varchar(32) NOT NULL default ' ',
  "AAABJEHT" varchar(30) NOT NULL default ' ',
  "AAABCKNE" varchar(30) NOT NULL default ' ',
  "AAABCKJD" varchar(1) NOT NULL default ' ',
  "AAABCKNF" varchar(30) NOT NULL default ' ',
  "AAABCKJF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWWJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKJH" (
  "AAABCKJM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABCKJM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKJK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCKJM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKJO" (
  "AAABIWWJ" varchar(30) NOT NULL default ' ',
  "AAABMIRY" varchar(10) NOT NULL default ' ',
  "AAABLUEO" varchar(32) NOT NULL default ' ',
  "AAABJEHT" varchar(30) NOT NULL default ' ',
  "AAABCKNF" varchar(30) NOT NULL default ' ',
  "AAABCKNG" varchar(1) NOT NULL default ' ',
  "AAABCKJV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWWJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKJX" (
  "AAABMIRY" varchar(10) NOT NULL default ' ',
  "AAABLUEO" varchar(32) NOT NULL default ' ',
  "AAABCKMB" varchar(4) NOT NULL default '0000',
  "AAABCKKB" varchar(27) NOT NULL default ' ',
  PRIMARY KEY ("AAABMIRY","AAABLUEO","AAABCKMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKKF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMGE" varchar(10) NOT NULL default ' ',
  "AAABCKLH" varchar(10) NOT NULL default ' ',
  "AAABLFYC" varchar(10) NOT NULL default ' ',
  "AAABLUEO" varchar(32) NOT NULL default ' ',
  "AAABCKLK" varchar(10) NOT NULL default ' ',
  "AAABCKLU" varchar(32) NOT NULL default ' ',
  "AAABCKUG" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABIMGE","AAABCKLH","AAABLFYC","AAABLUEO","AAABCKLK","AAABCKLU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKKV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMGE" varchar(10) NOT NULL default ' ',
  "AAABCKLH" varchar(10) NOT NULL default ' ',
  "AAABLFYC" varchar(10) NOT NULL default ' ',
  "AAABLUEO" varchar(32) NOT NULL default ' ',
  "AAABCKLK" varchar(10) NOT NULL default ' ',
  "AAABCKLU" varchar(32) NOT NULL default ' ',
  "AAABELZB" varchar(30) NOT NULL default ' ',
  "AAABCKUG" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABIMGE","AAABCKLH","AAABLFYC","AAABLUEO","AAABCKLK","AAABCKLU","AAABELZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKLN" (
  "AAABLFYC" varchar(10) NOT NULL default ' ',
  "AAABLUEO" varchar(32) NOT NULL default ' ',
  "AAABCKLU" varchar(32) NOT NULL default ' ',
  "AAABELZB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFYC","AAABLUEO","AAABCKLU","AAABELZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKLW" (
  "AAABCKMA" varchar(30) NOT NULL default ' ',
  "AAABCKMB" varchar(4) NOT NULL default '0000',
  "AAABLRPK" varchar(27) NOT NULL default ' ',
  PRIMARY KEY ("AAABCKMA","AAABCKMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKMC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCKMP" varchar(1) NOT NULL default ' ',
  "AAABCKMQ" varchar(1) NOT NULL default ' ',
  "AAABCKMR" varchar(1) NOT NULL default ' ',
  "AAABKYNY" varchar(250) NOT NULL default ' ',
  "AAABKYNZ" varchar(250) NOT NULL default ' ',
  "AAABKYOA" varchar(250) NOT NULL default ' ',
  "AAABKWJH" varchar(250) NOT NULL default ' ',
  "AAABKWJI" varchar(250) NOT NULL default ' ',
  "AAABKWJJ" varchar(250) NOT NULL default ' ',
  "AAABKWJK" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCKMP","AAABCKMQ","AAABCKMR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKMS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIAEU" varchar(6) NOT NULL default ' ',
  "AAABCKMY" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLUXQ","AAABIAEU","AAABCKMY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKMZ" (
  "AAABIWWJ" varchar(30) NOT NULL default ' ',
  "AAABMIRY" varchar(10) NOT NULL default ' ',
  "AAABLUEO" varchar(32) NOT NULL default ' ',
  "AAABJEHT" varchar(30) NOT NULL default ' ',
  "AAABCKNE" varchar(30) NOT NULL default ' ',
  "AAABCKNF" varchar(30) NOT NULL default ' ',
  "AAABCKNG" varchar(1) NOT NULL default ' ',
  "AAABCKNH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWWJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKNJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCKNL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKNN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRIM" varchar(10) NOT NULL default ' ',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABCKOQ" varchar(18) NOT NULL default ' ',
  "AAABCKOR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRIM","AAABLTUX","AAABCKOQ","AAABCKOR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKNY" (
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKOB" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLPWW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMQDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKOH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRIM" varchar(10) NOT NULL default ' ',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABCKOQ" varchar(18) NOT NULL default ' ',
  "AAABCKOR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRIM","AAABLMHB","AAABCKOQ","AAABCKOR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKOS" (
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABCKOU" varchar(30) NOT NULL default ' ',
  "AAABCKOV" varchar(30) NOT NULL default ' ',
  "AAABGIHK" varchar(30) NOT NULL default ' ',
  "AAABJAEN" varchar(12) NOT NULL default ' ',
  "AAABJAEL" varchar(8) NOT NULL default '00000000',
  "AAABCKOZ" varchar(12) NOT NULL default ' ',
  "AAABCKPA" varchar(8) NOT NULL default '00000000',
  "AAABLPJD" varchar(30) NOT NULL default ' ',
  "AAABCKPC" varchar(1) NOT NULL default ' ',
  "AAABCKPD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKPF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCKPW" varchar(25) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCKPW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKPL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCKPW" varchar(25) NOT NULL default ' ',
  "AAABMLPO" varchar(10) NOT NULL default '0000000000',
  "AAABJGOK" varchar(8) NOT NULL default ' ',
  "AAABMNLA" varchar(1) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABDGCT" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(45) NOT NULL default ' ',
  "AAABMNNA" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCKPW","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKPY" (
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABHJHF" varchar(30) NOT NULL default ' ',
  "AAABHJGM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKQD" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABLSEB" varchar(16) NOT NULL default '0000000000000000',
  "AAABLSCQ" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABCKQI" varchar(1) NOT NULL default ' ',
  "AAABCKQJ" varchar(8) NOT NULL default '00000000',
  "AAABCKQK" varchar(6) NOT NULL default '000000',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABJTNV" varchar(70) NOT NULL default ' ',
  "AAABCKQN" varchar(1) NOT NULL default ' ',
  "AAABCKQO" varchar(8) NOT NULL default '00000000',
  "AAABCKQP" varchar(6) NOT NULL default '000000',
  "AAABCKQQ" varchar(10) NOT NULL default ' ',
  "AAABCPXN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABLSEB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKQU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCKRG" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCKRG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKQZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCKRG" varchar(15) NOT NULL default ' ',
  "AAABLPWW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABCKRG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKRH" (
  "AAABKAOU" varchar(27) NOT NULL default ' ',
  "AAABCKRJ" varchar(27) NOT NULL default ' ',
  "AAABMLCA" varchar(30) NOT NULL default ' ',
  "AAABKCWA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKAOU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKRN" (
  "AAABCKXT" varchar(20) NOT NULL default ' ',
  "AAABLMOC" varchar(8) NOT NULL default '00000000',
  "AAABLMOD" varchar(6) NOT NULL default '000000',
  "AAABCKRR" varchar(12) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default ' ',
  "AAABCKRT" varchar(60) NOT NULL default ' ',
  "AAABCKRU" varchar(60) NOT NULL default ' ',
  "AAABCKRV" varchar(60) NOT NULL default ' ',
  "AAABCKRW" varchar(60) NOT NULL default ' ',
  "AAABCKRX" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABCKXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKRZ" (
  "AAABCKXT" varchar(20) NOT NULL default ' ',
  "AAABEDJP" varchar(10) NOT NULL default ' ',
  "AAABLZLD" varchar(10) NOT NULL default ' ',
  "AAABLFYC" varchar(10) NOT NULL default ' ',
  "AAABLUEO" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABCKXT","AAABEDJP","AAABLZLD","AAABLFYC","AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKSK" (
  "AAABCKXT" varchar(20) NOT NULL default ' ',
  "AAABEDJP" varchar(10) NOT NULL default ' ',
  "AAABLZLD" varchar(10) NOT NULL default ' ',
  "AAABLFYC" varchar(10) NOT NULL default ' ',
  "AAABLUEO" varchar(32) NOT NULL default ' ',
  "AAABCKUQ" varchar(32) NOT NULL default ' ',
  "AAABCKWN" varchar(4) NOT NULL default '0000',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAABEPJZ" varchar(30) NOT NULL default ' ',
  "AAABCKXL" varchar(30) NOT NULL default ' ',
  "AAABCKWF" varchar(1) NOT NULL default ' ',
  "AAABCKWH" varchar(70) NOT NULL default ' ',
  "AAABCKSX" varchar(32) NOT NULL default ' ',
  "AAABCKWI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABCKXT","AAABEDJP","AAABLZLD","AAABLFYC","AAABLUEO","AAABCKUQ","AAABCKWN","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKTH" (
  "AAABCKXT" varchar(20) NOT NULL default ' ',
  "AAABEDJP" varchar(10) NOT NULL default ' ',
  "AAABLZLD" varchar(10) NOT NULL default ' ',
  "AAABJADR" varchar(10) NOT NULL default ' ',
  "AAABLUEO" varchar(32) NOT NULL default ' ',
  "AAABCKUQ" varchar(32) NOT NULL default ' ',
  "AAABLTYA" varchar(10) NOT NULL default ' ',
  "AAABCKTP" varchar(1) NOT NULL default ' ',
  "AAABCKUG" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABCKXT","AAABEDJP","AAABLZLD","AAABJADR","AAABLUEO","AAABCKUQ","AAABLTYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKTY" (
  "AAABCKXT" varchar(20) NOT NULL default ' ',
  "AAABEDJP" varchar(10) NOT NULL default ' ',
  "AAABJADR" varchar(10) NOT NULL default ' ',
  "AAABLUEO" varchar(32) NOT NULL default ' ',
  "AAABLZLD" varchar(10) NOT NULL default ' ',
  "AAABCKUQ" varchar(32) NOT NULL default ' ',
  "AAABELZB" varchar(30) NOT NULL default ' ',
  "AAABCKUG" varchar(1) NOT NULL default '0',
  "AAABCKUH" varchar(4) NOT NULL default '0000',
  "AAABCKUI" varchar(6) NOT NULL default '000000',
  "AAABCKUJ" varchar(4) NOT NULL default '0000',
  "AAABCKUK" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABCKXT","AAABEDJP","AAABJADR","AAABLUEO","AAABLZLD","AAABCKUQ","AAABELZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKUS" (
  "AAABCKXT" varchar(20) NOT NULL default ' ',
  "AAABEDJP" varchar(10) NOT NULL default ' ',
  "AAABLZLD" varchar(10) NOT NULL default ' ',
  "AAABCKVK" varchar(10) NOT NULL default ' ',
  "AAABLUEO" varchar(32) NOT NULL default ' ',
  "AAABCKWN" varchar(4) NOT NULL default '0000',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAABEPJZ" varchar(1) NOT NULL default ' ',
  "AAABCKXL" varchar(30) NOT NULL default ' ',
  "AAABCKWH" varchar(70) NOT NULL default ' ',
  "AAABCKWF" varchar(1) NOT NULL default ' ',
  "AAABCKVE" varchar(30) NOT NULL default ' ',
  "AAABCKVF" varchar(10) NOT NULL default ' ',
  "AAABCKVG" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABCKXT","AAABEDJP","AAABLZLD","AAABCKVK","AAABLUEO","AAABCKWN","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKVO" (
  "AAABCKXT" varchar(20) NOT NULL default ' ',
  "AAABEDJP" varchar(10) NOT NULL default ' ',
  "AAABLZLD" varchar(10) NOT NULL default ' ',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABCKXT","AAABEDJP","AAABLZLD","AAABLMHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKVX" (
  "AAABCKXT" varchar(20) NOT NULL default ' ',
  "AAABEDJP" varchar(10) NOT NULL default ' ',
  "AAABLZLD" varchar(10) NOT NULL default ' ',
  "AAABCKWM" varchar(30) NOT NULL default ' ',
  "AAABCKWN" varchar(4) NOT NULL default '0000',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAABEPJZ" varchar(1) NOT NULL default ' ',
  "AAABCKWF" varchar(1) NOT NULL default ' ',
  "AAABCKXL" varchar(30) NOT NULL default ' ',
  "AAABCKWH" varchar(70) NOT NULL default ' ',
  "AAABCKWI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABCKXT","AAABEDJP","AAABLZLD","AAABCKWM","AAABCKWN","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKWP" (
  "AAABCKXT" varchar(20) NOT NULL default ' ',
  "AAABKWVJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABCKXT","AAABKWVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKWU" (
  "AAABCKXT" varchar(20) NOT NULL default ' ',
  "AAABKWVJ" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCKWY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABCKXT","AAABKWVJ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKXC" (
  "AAABCKXT" varchar(20) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCKXF" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABCKXT","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKXI" (
  "AAABCKXT" varchar(20) NOT NULL default ' ',
  "AAABIMZU" varchar(10) NOT NULL default ' ',
  "AAABCKXL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABCKXT","AAABIMZU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKXO" (
  "AAABCKXT" varchar(20) NOT NULL default ' ',
  "AAABIMZU" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCKXS" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABCKXT","AAABIMZU","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKXW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCKXY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKYA" (
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMMYV" varchar(30) NOT NULL default ' ',
  "AAABGKVK" varchar(1) NOT NULL default ' ',
  "AAABGKVM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKYG" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMJVD" varchar(50) NOT NULL default ' ',
  "AAABMJVE" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKYN" (
  "AAABGKVZ" varchar(1) NOT NULL default ' ',
  "AAABGKWA" varchar(24) NOT NULL default ' ',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABGKVZ","AAABGKWA","AAABMPMH"),
  KEY "AAABCKYU" ("AAABMPMH","AAABGKVZ","AAABGKWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKYY" (
  "AAABLLNS" varchar(6) NOT NULL default ' ',
  "AAABLQTM" varchar(1) NOT NULL default ' ',
  "AAABCKZB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKZD" (
  "AAABGKWP" varchar(6) NOT NULL default ' ',
  "AAABLQTM" varchar(1) NOT NULL default ' ',
  "AAABLZCP" varchar(40) NOT NULL default ' ',
  "AAABLVHI" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABGKWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKZJ" (
  "AAABMMYX" varchar(8) NOT NULL default ' ',
  "AAABGKWP" varchar(6) NOT NULL default ' ',
  "AAABCLAH" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMYX","AAABGKWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKZP" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMYX" varchar(8) NOT NULL default ' ',
  "AAABGKWP" varchar(6) NOT NULL default ' ',
  "AAABEPBY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMMYX","AAABGKWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCKZX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMYX" varchar(8) NOT NULL default ' ',
  "AAABGKWP" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMYX","AAABGKWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLAE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMYX" varchar(8) NOT NULL default ' ',
  "AAABCLAH" varchar(32) NOT NULL default ' ',
  "AAABLQTM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLAL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMYX" varchar(8) NOT NULL default ' ',
  "AAABEPBY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMMYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLAT" (
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABLYTE" varchar(3) NOT NULL default ' ',
  "AAABLLNS" varchar(6) NOT NULL default ' ',
  "AAABLENG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMPMH","AAABLYTE","AAABLLNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLBB" (
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABGKWP" varchar(6) NOT NULL default ' ',
  "AAABMMYX" varchar(8) NOT NULL default ' ',
  "AAABENMX" varchar(2) NOT NULL default '00',
  "AAABLYTE" varchar(3) NOT NULL default ' ',
  "AAABLLNS" varchar(6) NOT NULL default ' ',
  "AAABLENG" varchar(30) NOT NULL default ' ',
  "AAABGKXC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMPMH","AAABGKWP","AAABMMYX","AAABENMX","AAABLYTE","AAABLLNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLBQ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABGKWP" varchar(6) NOT NULL default ' ',
  "AAABMMYX" varchar(8) NOT NULL default ' ',
  "AAABENMX" varchar(2) NOT NULL default '00',
  "AAABLYTE" varchar(3) NOT NULL default ' ',
  "AAABLLNS" varchar(6) NOT NULL default ' ',
  "AAABKHGP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMPMH","AAABGKWP","AAABMMYX","AAABENMX","AAABLYTE","AAABLLNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLCG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMMYX" varchar(8) NOT NULL default ' ',
  "AAABLYTE" varchar(3) NOT NULL default ' ',
  "AAABLLNS" varchar(6) NOT NULL default ' ',
  "AAABLENG" varchar(30) NOT NULL default ' ',
  "AAABGKXC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPMH","AAABMMYX","AAABLYTE","AAABLLNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLCT" (
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMLXR" varchar(6) NOT NULL default ' ',
  "AAABMMYX" varchar(8) NOT NULL default ' ',
  "AAABENMX" varchar(2) NOT NULL default '00',
  "AAABLYTE" varchar(3) NOT NULL default ' ',
  "AAABLLNS" varchar(6) NOT NULL default ' ',
  "AAABCLDI" varchar(1) NOT NULL default ' ',
  "AAABMALF" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMPMH","AAABMLXR","AAABMMYX","AAABENMX","AAABLYTE","AAABLLNS","AAABCLDI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLDJ" (
  "AAABGKVZ" varchar(1) NOT NULL default ' ',
  "AAABGKWA" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABGKVZ","AAABGKWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLDO" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGKVZ" varchar(1) NOT NULL default ' ',
  "AAABGKWA" varchar(24) NOT NULL default ' ',
  "AAABEPBY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABGKVZ","AAABGKWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLDW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCLEA" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCLEA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLEB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCLEH" varchar(2) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCLEH","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLEJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCLEL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLEN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCLFG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCLFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLES" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCLFG" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCLFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLFA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCLLL" varchar(12) NOT NULL default ' ',
  "AAABCLFG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCLLL","AAABCLFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLFH" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLFK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCLLL" varchar(12) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCLLL","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLFT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCLLL" varchar(12) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCLLL","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLGA" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFZRW" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABFZRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLGG" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFZRW" varchar(30) NOT NULL default ' ',
  "AAABHFQD" varchar(4) NOT NULL default ' ',
  "AAABLRJQ" varchar(3) NOT NULL default '000',
  "AAABLEKJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABFZRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLGO" (
  "AAABFZRW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLGR" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFZRW" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMREL","AAABFZRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLGZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCLLL" varchar(12) NOT NULL default ' ',
  "AAABGJBM" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFZRW" varchar(30) NOT NULL default ' ',
  "AAABHFQD" varchar(4) NOT NULL default ' ',
  "AAABLRJQ" varchar(3) NOT NULL default '000',
  "AAABLEKJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCLLL","AAABGJBM","AAABMREL","AAABFZRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLHN" (
  "AAABHFQD" varchar(4) NOT NULL default ' ',
  "AAABCLIZ" varchar(3) NOT NULL default '000',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABCLHR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFQD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLHT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFTXT" varchar(4) NOT NULL default ' ',
  "AAABDTRC" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFTXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLIA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFTXT" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFTXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLII" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCLLL" varchar(12) NOT NULL default ' ',
  "AAABFTXT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCLLL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLIO" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHFQD" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABHFQD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLIU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCLLL" varchar(12) NOT NULL default ' ',
  "AAABGJBM" varchar(1) NOT NULL default ' ',
  "AAABHFQD" varchar(4) NOT NULL default ' ',
  "AAABCLIZ" varchar(3) NOT NULL default '000',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCLLL","AAABGJBM","AAABHFQD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLJF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCLLL" varchar(12) NOT NULL default ' ',
  "AAABGJBM" varchar(1) NOT NULL default ' ',
  "AAABHFQD" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCLLL","AAABGJBM","AAABHFQD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLJR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABMLPX" varchar(40) NOT NULL default ' ',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABCRXQ" varchar(20) NOT NULL default ' ',
  "AAABCLJY" varchar(8) NOT NULL default '00000000',
  "AAABCRXG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLKC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCLLL" varchar(12) NOT NULL default ' ',
  "AAABHYAL" varchar(1) NOT NULL default ' ',
  "AAABCLKG" varchar(8) NOT NULL default '00000000',
  "AAABCLKH" varchar(1) NOT NULL default ' ',
  "AAABCLKI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCLLL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLKL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCLLL" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCLLL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTJH","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLLB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCLNE" varchar(14) NOT NULL default ' ',
  "AAABCLLL" varchar(12) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABCLLG" varchar(1) NOT NULL default ' ',
  "AAABCLLH" varchar(18) NOT NULL default '000000000000000000',
  PRIMARY KEY ("AAABMRIL","AAABCLNE"),
  KEY "AAABCLLK" ("AAABCLLL","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLLN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCLNE" varchar(14) NOT NULL default ' ',
  "AAABCLLR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCLNE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLLV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCLNE" varchar(14) NOT NULL default ' ',
  "AAABCLNF" varchar(8) NOT NULL default ' ',
  "AAABCLLZ" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCLNE","AAABCLNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLMD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCLNE" varchar(14) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAABCLNG" varchar(4) NOT NULL default '0000',
  "AAABCLMJ" varchar(3) NOT NULL default ' ',
  "AAABCLMK" int(11) NOT NULL default '0',
  "AAABCLML" varchar(30) NOT NULL default ' ',
  "AAABCLNA" varchar(2) NOT NULL default ' ',
  "AAABCLNB" varchar(45) NOT NULL default ' ',
  "AAABCLNC" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCLNE","AAABMGHE","AAABMNMH","AAABCLNG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLMU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCLNE" varchar(14) NOT NULL default ' ',
  "AAABCLNF" varchar(8) NOT NULL default ' ',
  "AAABCLNG" varchar(4) NOT NULL default '0000',
  "AAABCLMZ" varchar(1) NOT NULL default ' ',
  "AAABCLNA" varchar(2) NOT NULL default ' ',
  "AAABCLNB" varchar(45) NOT NULL default ' ',
  "AAABCLNC" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCLNE","AAABCLNF","AAABCLNG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLNH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAABMLPX" varchar(40) NOT NULL default ' ',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  "AAABCRXQ" varchar(20) NOT NULL default ' ',
  "AAABCLPT" varchar(1) NOT NULL default ' ',
  "AAABCLPU" varchar(10) NOT NULL default '0000000000',
  "AAABCLPV" varchar(1) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMRFR" varchar(6) NOT NULL default '000000',
  "AAABCLPY" varchar(4) NOT NULL default '0000',
  "AAABCLPZ" varchar(10) NOT NULL default '0000000000',
  "AAABCLQA" varchar(10) NOT NULL default '0000000000',
  "AAABGNSP" varchar(32) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTJH","AAABLTJI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLOB" (
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABCLOM" int(11) NOT NULL default '0',
  "AAABHMDS" varchar(4) NOT NULL default ' ',
  "AAABHPJO" varchar(4) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABHPJQ" varchar(4) NOT NULL default '0000',
  "AAABHPJR" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABHPJU","AAABHPJV","AAABCLOM","AAABHMDS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLOO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCLQK" varchar(32) NOT NULL default ' ',
  "AAABMRCU" varchar(6) NOT NULL default ' ',
  "AAABGBZZ" varchar(32) NOT NULL default ' ',
  "AAABCLQM" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAABLUFA" varchar(1) NOT NULL default ' ',
  "AAABMLPX" varchar(40) NOT NULL default ' ',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  "AAABCRXQ" varchar(20) NOT NULL default ' ',
  "AAABCLPT" varchar(1) NOT NULL default ' ',
  "AAABCLPU" varchar(10) NOT NULL default '0000000000',
  "AAABCLPV" varchar(1) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMRFR" varchar(6) NOT NULL default '000000',
  "AAABCLPY" varchar(4) NOT NULL default '0000',
  "AAABCLPZ" varchar(10) NOT NULL default '0000000000',
  "AAABCLQA" varchar(10) NOT NULL default '0000000000',
  "AAABGNSP" varchar(32) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCLQK","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLPM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAABMLPX" varchar(40) NOT NULL default ' ',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  "AAABCRXQ" varchar(20) NOT NULL default ' ',
  "AAABCLPT" varchar(1) NOT NULL default ' ',
  "AAABCLPU" varchar(10) NOT NULL default '0000000000',
  "AAABCLPV" varchar(1) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMRFR" varchar(6) NOT NULL default '000000',
  "AAABCLPY" varchar(4) NOT NULL default '0000',
  "AAABCLPZ" varchar(10) NOT NULL default '0000000000',
  "AAABCLQA" varchar(10) NOT NULL default '0000000000',
  "AAABGNSP" varchar(32) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLQF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCLQM" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAABGBZZ" varchar(32) NOT NULL default ' ',
  "AAABCLQK" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCLQM","AAABLTJI"),
  KEY "AAABCLQO" ("AAABGBZZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCLQY" varchar(15) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABCLQU" varchar(3) NOT NULL default ' ',
  "AAABCLQV" varchar(15) NOT NULL default ' ',
  "AAABMGVM" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCLQY","AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLRA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLRJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABCPDS" varchar(8) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMPWL","AAABCPDS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLRU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABCLRY" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMAMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLSC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABJWRR" varchar(3) NOT NULL default ' ',
  "AAABCLSL" varchar(3) NOT NULL default ' ',
  "AAABCLSH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPWL","AAABJWRR","AAABCLSL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLSM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCLSO" varchar(1) NOT NULL default ' ',
  "AAABCLSP" varchar(1) NOT NULL default ' ',
  "AAABCLSQ" varchar(1) NOT NULL default ' ',
  "AAABCLSR" varchar(4) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABCLST" varchar(2) NOT NULL default '00',
  "AAABIMGE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLSW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCLSZ" varchar(1) NOT NULL default ' ',
  "AAABCLTA" varchar(15) NOT NULL default ' ',
  "AAABCLTB" varchar(1) NOT NULL default ' ',
  "AAABCLTC" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLTF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABJWRR" varchar(3) NOT NULL default ' ',
  "AAABMGVL" varchar(15) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABCLVJ" varchar(5) NOT NULL default ' ',
  "AAABCLTN" varchar(5) NOT NULL default ' ',
  "AAABCLTO" varchar(2) NOT NULL default ' ',
  "AAABCLTP" varchar(1) NOT NULL default '0',
  "AAABCLTQ" varchar(6) NOT NULL default '000000',
  "AAABDLQQ" varchar(4) NOT NULL default ' ',
  "AAABCLTS" varchar(1) NOT NULL default ' ',
  "AAABDULE" varchar(1) NOT NULL default ' ',
  "AAABCMEZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO"),
  UNIQUE KEY "AAABCLTX" ("AAABMRIL","AAABMGVL","AAABJWRR","AAABHFHO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLUC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABCLUG" varchar(35) NOT NULL default ' ',
  "AAABCLUH" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLUL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABCNCL" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLUS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABCLUW" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHFHO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLVA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABCLVJ" varchar(5) NOT NULL default ' ',
  "AAABCLVE" varchar(5) NOT NULL default ' ',
  "AAABCLVF" varchar(8) NOT NULL default '00000000',
  "AAABCLVG" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABCLVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLVK" (
  "AAABCLVZ" varchar(2) NOT NULL default ' ',
  "AAABHTWG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABCLVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLVO" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCLVZ" varchar(2) NOT NULL default ' ',
  "AAABCLVR" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABCLVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLVU" (
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABCLWK" varchar(4) NOT NULL default ' ',
  "AAABLUVE" varchar(2) NOT NULL default '00',
  "AAABCLVY" varchar(6) NOT NULL default ' ',
  "AAABCLVZ" varchar(2) NOT NULL default ' ',
  "AAABCLXK" varchar(4) NOT NULL default ' ',
  "AAABCLWB" varchar(35) NOT NULL default ' ',
  "AAABCLWC" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEHK","AAABCLWK","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLWG" (
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABCLWK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEHK","AAABCLWK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLWL" (
  "AAABCLXK" varchar(4) NOT NULL default ' ',
  "AAABCLWN" tinyint(3) unsigned NOT NULL default '0',
  "AAABCLWO" tinyint(3) unsigned NOT NULL default '0',
  "AAABCLWP" tinyint(3) unsigned NOT NULL default '0',
  "AAABCLWQ" tinyint(3) unsigned NOT NULL default '0',
  "AAABCLWR" tinyint(3) unsigned NOT NULL default '0',
  "AAABCLWS" tinyint(3) unsigned NOT NULL default '0',
  "AAABCLWT" tinyint(3) unsigned NOT NULL default '0',
  "AAABCLWU" tinyint(3) unsigned NOT NULL default '0',
  "AAABCLWV" tinyint(3) unsigned NOT NULL default '0',
  "AAABCLWW" tinyint(3) unsigned NOT NULL default '0',
  "AAABCLWX" tinyint(3) unsigned NOT NULL default '0',
  "AAABCLWY" tinyint(3) unsigned NOT NULL default '0',
  "AAABCLWZ" tinyint(3) unsigned NOT NULL default '0',
  "AAABCLXA" varchar(1) NOT NULL default ' ',
  "AAABCLXB" varchar(1) NOT NULL default ' ',
  "AAABCLXC" varchar(1) NOT NULL default ' ',
  "AAABCLXD" tinyint(3) unsigned NOT NULL default '0',
  "AAABCLXE" varchar(1) NOT NULL default ' ',
  "AAABCLXF" varchar(1) NOT NULL default '0',
  "AAABCLXG" varchar(1) NOT NULL default ' ',
  "AAABLEPD" varchar(1) NOT NULL default '0',
  "AAABCLXI" varchar(2) NOT NULL default '00',
  "AAABCLXJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABCLXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLXL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABHTWG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLEHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLXR" (
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABIWAT" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMOIA","AAABIWAT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLXW" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABIWAT" varchar(3) NOT NULL default '000',
  "AAABCLYA" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMOIA","AAABIWAT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLYE" (
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABIWAT" varchar(3) NOT NULL default '000',
  "AAABLUBZ" varchar(30) NOT NULL default ' ',
  "AAABCLZK" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLAVP" varchar(30) NOT NULL default ' ',
  "AAABLAVO" varchar(30) NOT NULL default ' ',
  "AAABCLYN" varchar(30) NOT NULL default ' ',
  "AAABCLYO" varchar(30) NOT NULL default ' ',
  "AAABCMJB" int(11) NOT NULL default '0',
  "AAABCLYQ" int(11) NOT NULL default '0',
  "AAABCLYR" varchar(1) NOT NULL default ' ',
  "AAABLAUL" varchar(1) NOT NULL default ' ',
  "AAABMLNB" varchar(1) NOT NULL default ' ',
  "AAABLAUJ" varchar(1) NOT NULL default ' ',
  "AAABMLNG" varchar(1) NOT NULL default ' ',
  "AAABMLND" varchar(1) NOT NULL default ' ',
  "AAABCLYX" varchar(6) NOT NULL default '000000',
  "AAABMLMY" varchar(6) NOT NULL default '000000',
  "AAABMLNE" varchar(1) NOT NULL default ' ',
  "AAABMLNC" varchar(1) NOT NULL default ' ',
  "AAABLUWW" varchar(1) NOT NULL default ' ',
  "AAABEBBN" varchar(4) NOT NULL default ' ',
  "AAABMNMM" varchar(6) NOT NULL default '000000',
  "AAABLAUU" varchar(1) NOT NULL default ' ',
  "AAABCLZF" varchar(3) NOT NULL default ' ',
  "AAABCLZG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOIA","AAABIWAT","AAABLUBZ","AAABCLZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLZL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABKYMP" varchar(6) NOT NULL default ' ',
  "AAABCMAE" varchar(4) NOT NULL default '0000',
  "AAABCMVZ" varchar(3) NOT NULL default '000',
  "AAABCMWA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABKYMP","AAABCMAE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLZW" (
  "AAABCMAE" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABCMAE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCLZZ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCMAE" varchar(4) NOT NULL default '0000',
  "AAABCMAC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABCMAE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMAF" (
  "AAABCMWA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABCMWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMAI" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCMWA" varchar(1) NOT NULL default ' ',
  "AAABCMAL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABCMWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMAO" (
  "AAABCMAW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABCMAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMAR" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCMAW" varchar(2) NOT NULL default ' ',
  "AAABCMAU" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABCMAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMAX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCMBK" varchar(4) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCMBK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMBD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCMBK" varchar(4) NOT NULL default ' ',
  "AAABCMBH" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCMBK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMBL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABKYMP" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABKYMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMBS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCMCE" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCMCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMBX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCMCE" varchar(3) NOT NULL default ' ',
  "AAABCMCB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCMCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMCF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCMED" varchar(6) NOT NULL default ' ',
  "AAABCMCI" varchar(7) NOT NULL default '0000000',
  "AAABCMCJ" varchar(1) NOT NULL default ' ',
  "AAABCMCK" varchar(1) NOT NULL default ' ',
  "AAABCMCL" varchar(1) NOT NULL default ' ',
  "AAABCMCM" varchar(1) NOT NULL default ' ',
  "AAABCMCN" varchar(1) NOT NULL default ' ',
  "AAABCMCO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCMED")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMCR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCMED" varchar(6) NOT NULL default ' ',
  "AAABCMTE" varchar(30) NOT NULL default ' ',
  "AAABLELJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABCMED")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMDA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCMED" varchar(6) NOT NULL default ' ',
  "AAABCMDD" varchar(30) NOT NULL default ' ',
  "AAABCMDE" varchar(30) NOT NULL default ' ',
  "AAABCMDF" varchar(30) NOT NULL default ' ',
  "AAABCMDG" varchar(30) NOT NULL default ' ',
  "AAABCMDH" varchar(30) NOT NULL default ' ',
  "AAABCMDI" varchar(30) NOT NULL default ' ',
  "AAABCMDJ" varchar(30) NOT NULL default ' ',
  "AAABCMDK" varchar(30) NOT NULL default ' ',
  "AAABCMDL" varchar(30) NOT NULL default ' ',
  "AAABCMDM" varchar(30) NOT NULL default ' ',
  "AAABCMDN" varchar(30) NOT NULL default ' ',
  "AAABCMDO" varchar(30) NOT NULL default ' ',
  "AAABCMDP" varchar(30) NOT NULL default ' ',
  "AAABCMDQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCMED")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMDT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCMED" varchar(6) NOT NULL default ' ',
  "AAABCMDW" varchar(1) NOT NULL default ' ',
  "AAABCMDX" varchar(1) NOT NULL default ' ',
  "AAABCMDY" varchar(1) NOT NULL default ' ',
  "AAABCMDZ" varchar(1) NOT NULL default ' ',
  "AAABCMEA" varchar(1) NOT NULL default ' ',
  "AAABCNUZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCMED")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMEE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMEJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABHFHP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMEP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYMP" varchar(6) NOT NULL default ' ',
  "AAABHFHP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMEV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCMEX" varchar(1) NOT NULL default ' ',
  "AAABCMEY" varchar(1) NOT NULL default ' ',
  "AAABCMEZ" varchar(1) NOT NULL default ' ',
  "AAABCMFA" varchar(1) NOT NULL default ' ',
  "AAABCMFB" varchar(2) NOT NULL default '00',
  "AAABCMFC" varchar(1) NOT NULL default ' ',
  "AAABCMFD" varchar(1) NOT NULL default ' ',
  "AAABCMFE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMFG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCMFI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMFK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHFHP" varchar(4) NOT NULL default ' ',
  "AAABCMFO" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHFHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMFS" (
  "AAABCMJM" varchar(4) NOT NULL default ' ',
  "AAABCMIF" varchar(2) NOT NULL default ' ',
  "AAABCMHW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABCMJM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMFX" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCMJM" varchar(4) NOT NULL default ' ',
  "AAABCMGA" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABCMJM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMGD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCMMR" varchar(3) NOT NULL default ' ',
  "AAABCMGG" varchar(30) NOT NULL default ' ',
  "AAABCMGH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCMMR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCMMR" varchar(3) NOT NULL default ' ',
  "AAABCMGO" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCMMR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMGS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHP" varchar(4) NOT NULL default ' ',
  "AAABCMHW" varchar(1) NOT NULL default ' ',
  "AAABHFHQ" varchar(4) NOT NULL default ' ',
  "AAABCMGX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHP","AAABCMHW","AAABHFHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMHC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHFHP" varchar(4) NOT NULL default ' ',
  "AAABCMHW" varchar(1) NOT NULL default ' ',
  "AAABHFHQ" varchar(4) NOT NULL default ' ',
  "AAABCMHI" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHFHP","AAABCMHW","AAABHFHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMHO" (
  "AAABCMHW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABCMHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMHR" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCMHW" varchar(1) NOT NULL default ' ',
  "AAABCMHU" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABCMHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMHX" (
  "AAABCMIF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABCMIF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMIA" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCMIF" varchar(2) NOT NULL default ' ',
  "AAABCMID" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABCMIF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMIG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHP" varchar(4) NOT NULL default ' ',
  "AAABCMJM" varchar(4) NOT NULL default ' ',
  "AAABCMIU" varchar(1) NOT NULL default ' ',
  "AAABCMIX" varchar(80) NOT NULL default ' ',
  "AAABCMIY" varchar(80) NOT NULL default ' ',
  "AAABCMIZ" varchar(80) NOT NULL default ' ',
  "AAABCMJA" varchar(80) NOT NULL default ' ',
  "AAABCMJB" varchar(80) NOT NULL default ' ',
  "AAABCMJC" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHP","AAABCMJM","AAABCMIU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMIV" (
  "AAABCMJM" varchar(4) NOT NULL default ' ',
  "AAABCMIX" varchar(80) NOT NULL default ' ',
  "AAABCMIY" varchar(80) NOT NULL default ' ',
  "AAABCMIZ" varchar(80) NOT NULL default ' ',
  "AAABCMJA" varchar(80) NOT NULL default ' ',
  "AAABCMJB" varchar(80) NOT NULL default ' ',
  "AAABCMJC" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABCMJM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMJE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHP" varchar(4) NOT NULL default ' ',
  "AAABCMMR" varchar(3) NOT NULL default ' ',
  "AAABCMJM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHP","AAABCMMR","AAABCMJM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZSF" varchar(4) NOT NULL default ' ',
  "AAABCMJQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZSF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMJT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHP" varchar(4) NOT NULL default ' ',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  "AAABLRXL" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHP","AAABHFHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMKB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXRR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXRR" varchar(4) NOT NULL default ' ',
  "AAABCMKK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHP" varchar(4) NOT NULL default ' ',
  "AAABCNUY" varchar(4) NOT NULL default ' ',
  "AAABLRXL" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHP","AAABCNUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHP" varchar(4) NOT NULL default ' ',
  "AAABCMMY" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHP","AAABCMMY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMLD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCMLP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCMLP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMLI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCMLP" varchar(4) NOT NULL default ' ',
  "AAABCMLM" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCMLP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMLQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHFHP" varchar(4) NOT NULL default ' ',
  "AAABCMMY" varchar(3) NOT NULL default ' ',
  "AAABCMLV" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHFHP","AAABCMMY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMMA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCMMR" varchar(3) NOT NULL default ' ',
  "AAABCMMS" varchar(4) NOT NULL default ' ',
  "AAABCMME" int(11) NOT NULL default '0',
  "AAABCMMF" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABCMMR","AAABCMMS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMMJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCMMR" varchar(3) NOT NULL default ' ',
  "AAABCMMS" varchar(4) NOT NULL default ' ',
  "AAABCMMO" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCMMR","AAABCMMS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMMT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHP" varchar(4) NOT NULL default ' ',
  "AAABCMRJ" varchar(3) NOT NULL default ' ',
  "AAABCMOK" varchar(3) NOT NULL default ' ',
  "AAABCMMY" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHP","AAABCMRJ","AAABCMOK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMND" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHP" varchar(4) NOT NULL default ' ',
  "AAABCMRJ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHP","AAABCMRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMNK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHFHP" varchar(4) NOT NULL default ' ',
  "AAABCMRJ" varchar(3) NOT NULL default ' ',
  "AAABCMRA" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHFHP","AAABCMRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMNU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHP" varchar(4) NOT NULL default ' ',
  "AAABCMOK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHP","AAABCMOK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHFHP" varchar(4) NOT NULL default ' ',
  "AAABCMOK" varchar(3) NOT NULL default ' ',
  "AAABCMOG" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHFHP","AAABCMOK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMOL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZSF" varchar(4) NOT NULL default ' ',
  "AAABCMOU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZSF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMOR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZSF" varchar(4) NOT NULL default ' ',
  "AAABCMOU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZSF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMOX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABCMPB" decimal(3,0) NOT NULL default '0',
  "AAABKJMK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABKYMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMPG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHP" varchar(4) NOT NULL default ' ',
  "AAABCMPW" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHP","AAABCMPW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMPN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHFHP" varchar(4) NOT NULL default ' ',
  "AAABCMPW" varchar(3) NOT NULL default ' ',
  "AAABCMPS" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHFHP","AAABCMPW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMPX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHP" varchar(4) NOT NULL default ' ',
  "AAABCMRJ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHP","AAABCMRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMQE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHP" varchar(4) NOT NULL default ' ',
  "AAABCMRR" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHP","AAABCMRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMQL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHFHP" varchar(4) NOT NULL default ' ',
  "AAABCMRR" varchar(3) NOT NULL default ' ',
  "AAABCMQQ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHFHP","AAABCMRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMQV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHFHP" varchar(4) NOT NULL default ' ',
  "AAABCMRJ" varchar(3) NOT NULL default ' ',
  "AAABCMRA" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHFHP","AAABCMRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMRF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHP" varchar(4) NOT NULL default ' ',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  "AAABCMRJ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHP","AAABHFHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMRN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHP" varchar(4) NOT NULL default ' ',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  "AAABCMRR" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHP","AAABHFHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMRV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHP" varchar(4) NOT NULL default ' ',
  "AAABGZMH" varchar(2) NOT NULL default ' ',
  "AAABJABF" varchar(5) NOT NULL default ' ',
  "AAABCMSA" decimal(17,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHFHP","AAABGZMH","AAABJABF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMSF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQGL" varchar(4) NOT NULL default ' ',
  "AAABCMSI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMSL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCMTI" varchar(4) NOT NULL default ' ',
  "AAABCMSO" varchar(1) NOT NULL default ' ',
  "AAABCMSP" varchar(4) NOT NULL default ' ',
  "AAABCMSQ" varchar(1) NOT NULL default ' ',
  "AAABCMSR" varchar(1) NOT NULL default ' ',
  "AAABCMSS" varchar(1) NOT NULL default ' ',
  "AAABCMST" varchar(1) NOT NULL default ' ',
  "AAABCMSU" varchar(1) NOT NULL default ' ',
  "AAABCMSV" varchar(1) NOT NULL default ' ',
  "AAABCMSW" varchar(1) NOT NULL default ' ',
  "AAABCMSX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCMTI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMTA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCMTI" varchar(4) NOT NULL default ' ',
  "AAABCMTE" varchar(30) NOT NULL default ' ',
  "AAABLELJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABCMTI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMTJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCMTV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCMTV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMTO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCMTV" varchar(4) NOT NULL default ' ',
  "AAABCMTS" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCMTV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMTW" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABCNIO" varchar(4) NOT NULL default ' ',
  "AAABCMTZ" varchar(2) NOT NULL default '00',
  "AAABLYCI" varchar(2) NOT NULL default ' ',
  "AAABCMUB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABCNIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMUE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCNIO" varchar(4) NOT NULL default ' ',
  "AAABCMUI" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCNIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMUM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCMUS" varchar(2) NOT NULL default '00',
  "AAABCMUP" varchar(1) NOT NULL default ' ',
  "AAABCMUQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCMUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMUT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCMUZ" varchar(4) NOT NULL default ' ',
  "AAABCMVA" varchar(5) NOT NULL default ' ',
  "AAABCMUX" decimal(17,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABCMUZ","AAABCMVA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMVB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCMVN" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABCMVN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMVG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCMVN" varchar(2) NOT NULL default '00',
  "AAABCMVK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABCMVN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMVO" (
  "AAABCMVS" varchar(10) NOT NULL default ' ',
  "AAABCMVT" varchar(4) NOT NULL default ' ',
  "AAABCMVR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABCMVS","AAABCMVT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABCMWE" varchar(10) NOT NULL default ' ',
  "AAABCMVZ" varchar(3) NOT NULL default '000',
  "AAABCMWA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABKYMV","AAABCMWE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMWF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCMXH" varchar(10) NOT NULL default ' ',
  "AAABLNZH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCMXH","AAABLNZH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMWM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCMXH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCMXH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMWR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCMXH" varchar(10) NOT NULL default ' ',
  "AAABLUDG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABCMXH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMWZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCMXH" varchar(10) NOT NULL default ' ',
  "AAABLNZH" varchar(4) NOT NULL default ' ',
  "AAABLUDG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABCMXH","AAABLNZH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMXJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCMXL" varchar(4) NOT NULL default ' ',
  "AAABCMXM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMXO" (
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABKWJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMXR" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABCNZS" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABKWJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMXX" (
  "AAABCMYI" varchar(4) NOT NULL default ' ',
  "AAABCMYJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABCMYI","AAABCMYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMYC" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCMYI" varchar(4) NOT NULL default ' ',
  "AAABCMYJ" varchar(4) NOT NULL default ' ',
  "AAABCMYG" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABCMYI","AAABCMYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMYK" (
  "AAABCMYS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABCMYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMYN" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCMYS" varchar(4) NOT NULL default ' ',
  "AAABCMYQ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABCMYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMYT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABCNTB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKWJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMYZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCNCL" varchar(4) NOT NULL default ' ',
  "AAABCNBQ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCNCL","AAABCNBQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMZG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCNCL" varchar(4) NOT NULL default ' ',
  "AAABCNBQ" varchar(6) NOT NULL default ' ',
  "AAABCMZL" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCNCL","AAABCNBQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCMZQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCNAG" varchar(1) NOT NULL default ' ',
  "AAABCNCL" varchar(4) NOT NULL default ' ',
  "AAABCNBL" varchar(4) NOT NULL default ' ',
  "AAABCNBQ" varchar(6) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABKBHA" varchar(6) NOT NULL default ' ',
  "AAABCMZY" varchar(10) NOT NULL default ' ',
  "AAABCMZZ" varchar(10) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABCNAB" varchar(1) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMCRA" varchar(6) NOT NULL default ' ',
  "AAABMDGC" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCNAG","AAABCNCL","AAABCNBL","AAABCNBQ","AAABMPWL","AAABKBHA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNAM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCNCL" varchar(4) NOT NULL default ' ',
  "AAABCNAU" varchar(10) NOT NULL default ' ',
  "AAABCNAQ" varchar(10) NOT NULL default ' ',
  "AAABCNAR" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCNCL","AAABCNAU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNAV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCNCL" varchar(4) NOT NULL default ' ',
  "AAABCNBL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCNCL","AAABCNBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNBC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCNCL" varchar(4) NOT NULL default ' ',
  "AAABCNBL" varchar(4) NOT NULL default ' ',
  "AAABCNBH" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCNCL","AAABCNBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNBM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCNCL" varchar(4) NOT NULL default ' ',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  "AAABCNBQ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCNCL","AAABHFHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNBU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCNCL" varchar(4) NOT NULL default ' ',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABKSUB" varchar(10) NOT NULL default ' ',
  "AAABCNCA" varchar(2) NOT NULL default ' ',
  "AAABCNCB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCNCL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNCE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCNCL" varchar(4) NOT NULL default ' ',
  "AAABCNCI" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCNCL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNCM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAABCNCP" varchar(25) NOT NULL default ' ',
  "AAABJSTP" varchar(25) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPPR" varchar(4) NOT NULL default ' ',
  "AAABCNCV" varchar(2) NOT NULL default ' ',
  "AAABCNCW" varchar(20) NOT NULL default ' ',
  "AAABININ" varchar(6) NOT NULL default ' ',
  "AAABCNCY" varchar(10) NOT NULL default ' ',
  "AAABCNCZ" varchar(2) NOT NULL default ' ',
  "AAABCNDA" varchar(3) NOT NULL default ' ',
  "AAABCNDB" varchar(2) NOT NULL default ' ',
  "AAABMGSS" varchar(12) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  "AAABMGSU" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUXQ","AAABLPMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNDH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABGRZV" varchar(20) NOT NULL default ' ',
  "AAABEPBY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLZBM","AAABMOAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNDS" (
  "AAABCNFB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABCNFB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNDV" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCNFB" varchar(2) NOT NULL default ' ',
  "AAABCNDY" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABCNFB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNEB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHP" varchar(4) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABCNFB" varchar(2) NOT NULL default ' ',
  "AAABCNFC" varchar(2) NOT NULL default ' ',
  "AAABCNEO" varchar(8) NOT NULL default '00000000',
  "AAABCNEI" decimal(17,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHFHP","AAABMPWL","AAABCNFB","AAABCNFC","AAABCNEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNEP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHFHP" varchar(4) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABCNFB" varchar(2) NOT NULL default ' ',
  "AAABCNFC" varchar(2) NOT NULL default ' ',
  "AAABCNEW" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHFHP","AAABMPWL","AAABCNFB","AAABCNFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNFD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABKYMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNFK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCNUY" varchar(4) NOT NULL default ' ',
  "AAABCNRJ" varchar(2) NOT NULL default ' ',
  "AAABCNFO" varchar(1) NOT NULL default ' ',
  "AAABCNFP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCNUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNFS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABCNGA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABKYMV","AAABCNGA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNGB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCNUY" varchar(4) NOT NULL default ' ',
  "AAABCNGF" varchar(25) NOT NULL default ' ',
  "AAABCNGG" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCNUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABCNUY" varchar(4) NOT NULL default ' ',
  "AAABCNNP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP","AAABCNUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNGS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABCNUZ" varchar(4) NOT NULL default ' ',
  "AAABCNNP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP","AAABCNUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNHA" (
  "AAABLQGL" varchar(4) NOT NULL default ' ',
  "AAABCNHC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNHE" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLQGL" varchar(4) NOT NULL default ' ',
  "AAABCNHH" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNHK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQGL" varchar(4) NOT NULL default ' ',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABCNHO" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLQGL","AAABHFHO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNHS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAABCNHX" varchar(5) NOT NULL default '00000',
  "AAABCNHY" varchar(5) NOT NULL default '00000',
  "AAABCNHZ" varchar(2) NOT NULL default ' ',
  "AAABCNIA" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABKYMV","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNIF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  "AAABCODK" varchar(1) NOT NULL default ' ',
  "AAABCNIJ" varchar(1) NOT NULL default ' ',
  "AAABCNIK" varchar(1) NOT NULL default ' ',
  "AAABCNIL" varchar(1) NOT NULL default ' ',
  "AAABCNIM" varchar(1) NOT NULL default ' ',
  "AAABCNIN" varchar(1) NOT NULL default ' ',
  "AAABCNIO" varchar(4) NOT NULL default ' ',
  "AAABCNIP" varchar(1) NOT NULL default ' ',
  "AAABCNIQ" varchar(1) NOT NULL default ' ',
  "AAABCNIR" varchar(1) NOT NULL default ' ',
  "AAABCNIS" varchar(1) NOT NULL default '0',
  "AAABCNIT" varchar(1) NOT NULL default ' ',
  "AAABCNIU" varchar(1) NOT NULL default ' ',
  "AAABCNIV" varchar(1) NOT NULL default ' ',
  "AAABCNIW" varchar(1) NOT NULL default ' ',
  "AAABCNIX" varchar(1) NOT NULL default ' ',
  "AAABCNIY" varchar(1) NOT NULL default ' ',
  "AAABCNIZ" varchar(1) NOT NULL default ' ',
  "AAABCNJA" varchar(1) NOT NULL default ' ',
  "AAABCNJB" varchar(1) NOT NULL default ' ',
  "AAABCNJC" varchar(1) NOT NULL default ' ',
  "AAABCNJD" varchar(1) NOT NULL default ' ',
  "AAABCNJE" varchar(1) NOT NULL default ' ',
  "AAABCNJF" varchar(1) NOT NULL default ' ',
  "AAABCNJG" varchar(1) NOT NULL default ' ',
  "AAABCNJH" varchar(1) NOT NULL default ' ',
  "AAABCNJI" varchar(1) NOT NULL default ' ',
  "AAABCNJJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNJM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  "AAABCPAJ" varchar(2) NOT NULL default ' ',
  "AAABCNJQ" varchar(4) NOT NULL default ' ',
  "AAABCNJR" varchar(4) NOT NULL default ' ',
  "AAABCNJS" varchar(1) NOT NULL default ' ',
  "AAABCNJT" varchar(4) NOT NULL default ' ',
  "AAABCNJU" varchar(1) NOT NULL default ' ',
  "AAABCNJV" decimal(3,0) NOT NULL default '0',
  "AAABCNJW" varchar(4) NOT NULL default ' ',
  "AAABCNJX" varchar(1) NOT NULL default ' ',
  "AAABCNJY" varchar(1) NOT NULL default ' ',
  "AAABCNJZ" varchar(1) NOT NULL default ' ',
  "AAABCNKA" varchar(1) NOT NULL default ' ',
  "AAABCNKB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHY","AAABCPAJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNKF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  "AAABCPAJ" varchar(2) NOT NULL default ' ',
  "AAABCNKK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHFHY","AAABCPAJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNKP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  "AAABCNKW" varchar(4) NOT NULL default ' ',
  "AAABCNNP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHY","AAABCNKW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNKX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABCNLJ" varchar(3) NOT NULL default '000',
  "AAABCNLC" decimal(17,2) NOT NULL default '0.00',
  "AAABCNLD" varchar(1) NOT NULL default ' ',
  "AAABCNLE" varchar(1) NOT NULL default ' ',
  "AAABCNLF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHY","AAABMPWL","AAABCNLJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNLK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCPAJ" varchar(2) NOT NULL default ' ',
  "AAABCNRJ" varchar(2) NOT NULL default ' ',
  "AAABCNWE" varchar(3) NOT NULL default '000',
  "AAABCNLR" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHY","AAABMRHS","AAABCPAJ","AAABCNRJ","AAABCNWE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNLY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCPAJ" varchar(2) NOT NULL default ' ',
  "AAABCNRJ" varchar(2) NOT NULL default ' ',
  "AAABCNME" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHFHY","AAABMRHS","AAABCPAJ","AAABCNRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNMK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLAUG" varchar(60) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  "AAABCPAJ" varchar(2) NOT NULL default ' ',
  "AAABCNRJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLAUG","AAABMRHS","AAABHFHY","AAABCPAJ","AAABCNRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNMX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCNNC" varchar(5) NOT NULL default ' ',
  "AAABCNNA" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCNNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNND" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  "AAABGZSF" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHFHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNNL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  "AAABCNNP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP","AAABHFHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNNT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABHRVE" varchar(18) NOT NULL default ' ',
  "AAABKGKQ" varchar(2) NOT NULL default ' ',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  "AAABCNNZ" varchar(2) NOT NULL default ' ',
  "AAABCNOA" varchar(2) NOT NULL default ' ',
  "AAABCNOB" varchar(2) NOT NULL default ' ',
  "AAABCNOC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABHRVE","AAABKGKQ","AAABHFHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNOI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCNQU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCNQU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNON" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCNQU" varchar(4) NOT NULL default ' ',
  "AAABCNOR" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCNQU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNOV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCNQU" varchar(4) NOT NULL default ' ',
  "AAABCNUY" varchar(4) NOT NULL default ' ',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCNQU","AAABCNUY","AAABHFHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNPE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCNQU" varchar(4) NOT NULL default ' ',
  "AAABLQGL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCNQU","AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNPL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCNQM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCNQM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNPQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIGIV" varchar(4) NOT NULL default ' ',
  "AAABCNPU" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABIGIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCNQM" varchar(4) NOT NULL default ' ',
  "AAABCNUY" varchar(4) NOT NULL default ' ',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCNQM","AAABCNUY","AAABHFHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNQH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCNQM" varchar(4) NOT NULL default ' ',
  "AAABLQGL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCNQM","AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNQO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYMP" varchar(6) NOT NULL default ' ',
  "AAABCNQU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYMP","AAABCNQU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNQV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCNRJ" varchar(2) NOT NULL default ' ',
  "AAABCNQY" varchar(2) NOT NULL default '00',
  "AAABCNQZ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABCNRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNRC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCNRJ" varchar(2) NOT NULL default ' ',
  "AAABCNRG" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCNRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNRK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABKYMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNRR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABCOVJ" varchar(4) NOT NULL default ' ',
  "AAABKGMO" decimal(17,2) NOT NULL default '0.00',
  "AAABCOPZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABKYMV","AAABCOVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNSC" (
  "AAABCNSP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABCNSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNSF" (
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNSL" (
  "AAABCNSP" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABCNSP","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNSR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABCNUY" varchar(4) NOT NULL default ' ',
  "AAABCNUZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKWJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNSY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABCNTB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKWJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNTE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  "AAABCOVJ" varchar(4) NOT NULL default ' ',
  "AAABKGMO" decimal(17,2) NOT NULL default '0.00',
  "AAABCOPZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABKYMV","AAABHFHY","AAABCOVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNTR" (
  "AAABCOBJ" varchar(2) NOT NULL default '00',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABCOBJ","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNTW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABKYMP" varchar(6) NOT NULL default ' ',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  "AAABCOPI" varchar(4) NOT NULL default '0000',
  "AAABKGMO" decimal(17,2) NOT NULL default '0.00',
  "AAABCOVJ" varchar(4) NOT NULL default ' ',
  "AAABCOPZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABKYMP","AAABHFHY","AAABCOPI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNUK" (
  "AAABIYQF" varchar(2) NOT NULL default ' ',
  "AAABCNUM" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABIYQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNUO" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIYQF" varchar(2) NOT NULL default ' ',
  "AAABCNUR" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABIYQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNUU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  "AAABCNUY" varchar(4) NOT NULL default ' ',
  "AAABCNUZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNVC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABIYQF" varchar(2) NOT NULL default ' ',
  "AAABMDBL" varchar(2) NOT NULL default ' ',
  "AAABCNVH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNVK" (
  "AAABCNVQ" varchar(1) NOT NULL default '0',
  "AAABCNVR" varchar(2) NOT NULL default ' ',
  "AAABCOBW" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(15) NOT NULL default ' ',
  "AAABHWLM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABCNVQ","AAABCNVR","AAABCOBW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNVT" (
  "AAABIYQF" varchar(2) NOT NULL default ' ',
  "AAABMDBL" varchar(2) NOT NULL default ' ',
  "AAABCOBJ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABIYQF","AAABMDBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNVZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABCNYI" varchar(35) NOT NULL default ' ',
  "AAABMLPO" varchar(5) NOT NULL default '00000',
  "AAABCNWE" varchar(3) NOT NULL default '000',
  "AAABIFVP" varchar(65) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABCNYI","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNWK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABCNYI" varchar(35) NOT NULL default ' ',
  "AAABCNXY" varchar(5) NOT NULL default ' ',
  "AAABCPEO" varchar(3) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABLCFI" varchar(10) NOT NULL default ' ',
  "AAABMCZD" varchar(2) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABCNYI","AAABCNXY","AAABCPEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNXA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABKWVJ" varchar(10) NOT NULL default ' ',
  "AAABCNYI" varchar(35) NOT NULL default ' ',
  "AAABCNXW" varchar(1) NOT NULL default ' ',
  "AAABCNXX" varchar(2) NOT NULL default ' ',
  "AAABCNXY" varchar(5) NOT NULL default ' ',
  "AAABCPEO" varchar(3) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABLCFI" varchar(10) NOT NULL default ' ',
  "AAABMCZD" varchar(2) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  "AAABCNXO" varchar(2) NOT NULL default ' ',
  "AAABCNXP" varchar(2) NOT NULL default ' ',
  "AAABKQHD" varchar(1) NOT NULL default ' ',
  "AAABCNXR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABKWVJ","AAABCNYI","AAABCNXW","AAABCNXX","AAABCNXY","AAABCPEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNYA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMGVL" varchar(15) NOT NULL default ' ',
  "AAABJRHR" varchar(18) NOT NULL default ' ',
  "AAABJRIA" varchar(15) NOT NULL default ' ',
  "AAABJRIB" varchar(18) NOT NULL default ' ',
  "AAABJABF" varchar(5) NOT NULL default ' ',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABCNYI" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMGVL","AAABJRHR","AAABJRIA","AAABJRIB","AAABJABF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNYP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGTBZ" varchar(8) NOT NULL default ' ',
  "AAABCNYZ" varchar(3) NOT NULL default ' ',
  "AAABCNYT" varchar(2) NOT NULL default ' ',
  "AAABCNYU" varchar(2) NOT NULL default ' ',
  "AAABCNYV" varchar(2) NOT NULL default ' ',
  "AAABCNYW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGTBZ","AAABCNYZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABCNZK" varchar(4) NOT NULL default ' ',
  "AAABCNZF" tinyint(3) unsigned NOT NULL default '0',
  "AAABCNZG" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABKYMV","AAABCNZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNZL" (
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABCNZN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKWJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNZP" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABCNZS" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABKWJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCNZV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCOAD" varchar(4) NOT NULL default ' ',
  "AAABMELA" varchar(2) NOT NULL default '00',
  "AAABIWUT" varchar(1) NOT NULL default ' ',
  "AAABCODC" varchar(1) NOT NULL default ' ',
  "AAABIADW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCOAD","AAABMELA","AAABIWUT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOAG" (
  "AAABHWLM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHWLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOAJ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHWLM" varchar(1) NOT NULL default ' ',
  "AAABCOAM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABHWLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOAP" (
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABCOBJ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMKFH","AAABCOBJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOAU" (
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABIYQF" varchar(2) NOT NULL default ' ',
  "AAABMDBL" varchar(2) NOT NULL default ' ',
  "AAABCOBJ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMKFH","AAABIYQF","AAABMDBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOBC" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABCOBJ" varchar(2) NOT NULL default '00',
  "AAABCOBG" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMKFH","AAABCOBJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOBK" (
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABCOBW" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABCOBW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOBP" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABCOBW" varchar(3) NOT NULL default ' ',
  "AAABCOBT" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMKFH","AAABCOBW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOBX" (
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABMDBL" varchar(2) NOT NULL default ' ',
  "AAABCOCW" varchar(3) NOT NULL default ' ',
  "AAABHWLM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABMDBL","AAABCOCW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOCF" (
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABIYQF" varchar(2) NOT NULL default ' ',
  "AAABCOCW" varchar(3) NOT NULL default ' ',
  "AAABHWLM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABIYQF","AAABCOCW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOCN" (
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABMOIB" varchar(1) NOT NULL default ' ',
  "AAABHFQD" varchar(4) NOT NULL default '0000',
  "AAABCOCW" varchar(3) NOT NULL default ' ',
  "AAABHWLM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABMOIB","AAABHFQD","AAABCOCW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOCX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABMELA" varchar(2) NOT NULL default '00',
  "AAABMOIB" varchar(1) NOT NULL default ' ',
  "AAABCODC" varchar(1) NOT NULL default ' ',
  "AAABIADW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABMELA","AAABMOIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCODI" (
  "AAABGZMH" varchar(2) NOT NULL default ' ',
  "AAABCODK" varchar(1) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABGZMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCODN" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGZMH" varchar(2) NOT NULL default ' ',
  "AAABCODQ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABGZMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCODT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZMH" varchar(2) NOT NULL default ' ',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCODZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  "AAABCOFX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOEF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCOES" varchar(2) NOT NULL default '00',
  "AAABCOGB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCOES")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOEL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCOES" varchar(2) NOT NULL default '00',
  "AAABCOEP" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABCOES")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOET" (
  "AAABCOFB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABCOFB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOEW" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCOFB" varchar(4) NOT NULL default '0000',
  "AAABCOEZ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABCOFB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOFC" (
  "AAABCOFK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABCOFK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOFF" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCOFK" varchar(4) NOT NULL default ' ',
  "AAABCOFI" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABCOFK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOFL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCOFX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCOFX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOFQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCOFX" varchar(3) NOT NULL default ' ',
  "AAABCOFU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABCOFX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOFY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCOGL" varchar(6) NOT NULL default ' ',
  "AAABCOGB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCOGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOGE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCOGL" varchar(6) NOT NULL default ' ',
  "AAABCOGI" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABCOGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOGM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCOLV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCOLV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOGR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCOLV" varchar(4) NOT NULL default ' ',
  "AAABCOGV" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABCOLV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOGZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABCOLM" varchar(4) NOT NULL default ' ',
  "AAABCOHD" int(11) NOT NULL default '0',
  "AAABCOHE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABCOLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOHI" (
  "AAABLUEO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOHL" (
  "AAABLUEO" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCOHO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUEO","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOHR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(4) NOT NULL default ' ',
  "AAABCOHU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOHX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCOLM" varchar(4) NOT NULL default ' ',
  "AAABLUVJ" varchar(40) NOT NULL default ' ',
  "AAABCOIG" varchar(20) NOT NULL default ' ',
  "AAABCOIC" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABCOLM","AAABLUVJ","AAABCOIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOIH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCOLM" varchar(4) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABCOIS" varchar(4) NOT NULL default ' ',
  "AAABCOIT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCOLM","AAABMNML","AAABIJIG","AAABCOIS","AAABCOIT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOIU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABCOIY" varchar(1) NOT NULL default ' ',
  "AAABCOIZ" varchar(1) NOT NULL default ' ',
  "AAABCOJA" varchar(1) NOT NULL default ' ',
  "AAABCOJB" varchar(1) NOT NULL default ' ',
  "AAABCOJC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABKYMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOJG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCOKA" varchar(1) NOT NULL default ' ',
  "AAABCOJJ" varchar(30) NOT NULL default ' ',
  "AAABCOJK" varchar(30) NOT NULL default ' ',
  "AAABCOJL" varchar(30) NOT NULL default ' ',
  "AAABCOJM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCOKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOJP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCOKA" varchar(1) NOT NULL default ' ',
  "AAABCOJT" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABCOKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOJX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCOLM" varchar(4) NOT NULL default ' ',
  "AAABCOKA" varchar(1) NOT NULL default ' ',
  "AAABCOLI" varchar(1) NOT NULL default ' ',
  "AAABCOKC" varchar(4) NOT NULL default ' ',
  "AAABCOKD" varchar(20) NOT NULL default ' ',
  "AAABCOKE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCOLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOKH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCOLM" varchar(4) NOT NULL default ' ',
  "AAABCOKL" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABCOLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOKP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCOLM" varchar(4) NOT NULL default ' ',
  "AAABCOKW" varchar(4) NOT NULL default ' ',
  "AAABCOKT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCOLM","AAABCOKW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOKX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMPMH" varchar(4) NOT NULL default ' ',
  "AAABCOLB" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOLF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCOLM" varchar(4) NOT NULL default ' ',
  "AAABCOLI" varchar(1) NOT NULL default ' ',
  "AAABCOLJ" varchar(20) NOT NULL default ' ',
  "AAABCOLK" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABCOLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOLN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCOLV" varchar(4) NOT NULL default ' ',
  "AAABCOLW" varchar(3) NOT NULL default '000',
  "AAABCOML" varchar(40) NOT NULL default ' ',
  "AAABCOLS" varchar(14) NOT NULL default ' ',
  "AAABCOLT" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCOLV","AAABCOLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOLX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCOML" varchar(40) NOT NULL default ' ',
  "AAABCOMA" varchar(1) NOT NULL default ' ',
  "AAABCOMB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCOML")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOME" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCOML" varchar(40) NOT NULL default ' ',
  "AAABCOMI" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABCOML")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOMM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABCOVI" varchar(2) NOT NULL default ' ',
  "AAABCOMR" varchar(1) NOT NULL default ' ',
  "AAABCOMS" varchar(1) NOT NULL default ' ',
  "AAABECAI" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABKYMV","AAABCOVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOMY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCONL" varchar(3) NOT NULL default ' ',
  "AAABCONB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCONL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCONE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCONL" varchar(3) NOT NULL default ' ',
  "AAABCONI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCONL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCONM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABCONP" varchar(3) NOT NULL default '000',
  "AAABCONQ" varchar(4) NOT NULL default '0000',
  "AAABCONR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABHFHO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCONU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  "AAABCOVJ" varchar(4) NOT NULL default ' ',
  "AAABKGMO" decimal(17,2) NOT NULL default '0.00',
  "AAABCOPZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABKYMV","AAABHFHY","AAABCOVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOOH" (
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOOK" (
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCOON" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOOQ" (
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLUEO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOOV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABKYMP" varchar(6) NOT NULL default ' ',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  "AAABCOPI" varchar(4) NOT NULL default '0000',
  "AAABKGMO" decimal(17,2) NOT NULL default '0.00',
  "AAABCOVJ" varchar(4) NOT NULL default ' ',
  "AAABCOPZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABKYMP","AAABHFHY","AAABCOPI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOPJ" (
  "AAABIYUS" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABIYUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOPM" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIYUS" varchar(5) NOT NULL default ' ',
  "AAABCOPP" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABIYUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOPS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  "AAABCOVJ" varchar(4) NOT NULL default ' ',
  "AAABKGMO" decimal(17,2) NOT NULL default '0.00',
  "AAABCOPZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABKYMV","AAABHFHY","AAABCOVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOQF" (
  "AAABCOQN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABCOQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOQI" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCOQN" varchar(1) NOT NULL default ' ',
  "AAABCOQL" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABCOQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOQO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCOQU" varchar(12) NOT NULL default ' ',
  "AAABCOQV" varchar(8) NOT NULL default '00000000',
  "AAABCOQW" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCOQU","AAABCOQV","AAABCOQW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOQX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABCORH" varchar(3) NOT NULL default '000',
  "AAABCORC" varchar(1) NOT NULL default ' ',
  "AAABCORD" decimal(17,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABKYMV","AAABCORH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCORI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABMCFY" varchar(4) NOT NULL default ' ',
  "AAABKGMO" decimal(17,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABMCFY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCORQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABCOVI" varchar(2) NOT NULL default ' ',
  "AAABKGMO" decimal(17,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABKYMV","AAABCOVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOSA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOSF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABKYNY" varchar(250) NOT NULL default ' ',
  "AAABKYNZ" varchar(250) NOT NULL default ' ',
  "AAABKYOA" varchar(250) NOT NULL default ' ',
  "AAABKWJH" varchar(250) NOT NULL default ' ',
  "AAABKWJI" varchar(250) NOT NULL default ' ',
  "AAABKWJJ" varchar(250) NOT NULL default ' ',
  "AAABKWJK" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOSR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABKYNY" varchar(250) NOT NULL default ' ',
  "AAABKYNZ" varchar(250) NOT NULL default ' ',
  "AAABKYOA" varchar(250) NOT NULL default ' ',
  "AAABKWJH" varchar(250) NOT NULL default ' ',
  "AAABKWJI" varchar(250) NOT NULL default ' ',
  "AAABKWJJ" varchar(250) NOT NULL default ' ',
  "AAABKWJK" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOTD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABHRVE" varchar(18) NOT NULL default ' ',
  "AAABCOTH" varchar(2) NOT NULL default ' ',
  "AAABCOTI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLEHK","AAABHRVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOTM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCOUB" varchar(6) NOT NULL default ' ',
  "AAABCOTP" varchar(3) NOT NULL default ' ',
  "AAABCOTQ" varchar(15) NOT NULL default ' ',
  "AAABCOTR" varchar(35) NOT NULL default ' ',
  "AAABCOTS" varchar(35) NOT NULL default ' ',
  "AAABCOTT" varchar(11) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCOUB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOTW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJWRR" varchar(3) NOT NULL default ' ',
  "AAABMGVL" varchar(15) NOT NULL default ' ',
  "AAABCPAQ" varchar(6) NOT NULL default '000000',
  "AAABCOUB" varchar(6) NOT NULL default ' ',
  "AAABCOUC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJWRR","AAABMGVL","AAABCPAQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOUH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCOUW" varchar(4) NOT NULL default ' ',
  "AAABCOUX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCOUW","AAABCOUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOUO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCOUW" varchar(4) NOT NULL default ' ',
  "AAABCOUX" varchar(2) NOT NULL default ' ',
  "AAABCOUT" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCOUW","AAABCOUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOUY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABCOVI" varchar(2) NOT NULL default ' ',
  "AAABCOVJ" varchar(4) NOT NULL default ' ',
  "AAABKGMO" decimal(17,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABKYMV","AAABCOVI","AAABCOVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOVK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABCOWE" varchar(14) NOT NULL default ' ',
  "AAABCOVP" varchar(20) NOT NULL default ' ',
  "AAABCOVQ" varchar(1) NOT NULL default ' ',
  "AAABCOVR" varchar(1) NOT NULL default ' ',
  "AAABCOVS" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHFHO","AAABCOWE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOVX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCOWD" varchar(4) NOT NULL default ' ',
  "AAABCOWE" varchar(14) NOT NULL default ' ',
  "AAABCOWB" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCOWD","AAABCOWE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOWF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABCOWI" varchar(1) NOT NULL default ' ',
  "AAABCOWJ" varchar(1) NOT NULL default ' ',
  "AAABCOWK" varchar(1) NOT NULL default ' ',
  "AAABCOWL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOWO" (
  "AAABCOZK" varchar(8) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLUEO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABCOZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOWT" (
  "AAABCOZK" varchar(8) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCOZJ" varchar(55) NOT NULL default ' ',
  PRIMARY KEY ("AAABCOZK","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOWZ" (
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOXC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(10) NOT NULL default ' ',
  "AAABCOXF" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOXI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCOXK" varchar(1) NOT NULL default ' ',
  "AAABCOXL" varchar(1) NOT NULL default ' ',
  "AAABCOXM" varchar(1) NOT NULL default ' ',
  "AAABIXBL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOXP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(10) NOT NULL default ' ',
  "AAABCOXX" varchar(3) NOT NULL default ' ',
  "AAABHLTE" varchar(1) NOT NULL default ' ',
  "AAABCOXU" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABLECD","AAABCOXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOXY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABERBJ" varchar(18) NOT NULL default ' ',
  "AAABCOYC" varchar(1) NOT NULL default ' ',
  "AAABCOYD" varchar(1) NOT NULL default ' ',
  "AAABKPIV" varchar(2) NOT NULL default ' ',
  "AAABCOYF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABERBJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOYJ" (
  "AAABMMVH" varchar(15) NOT NULL default ' ',
  "AAABIOFM" varchar(5) NOT NULL default ' ',
  "AAABLEPD" varchar(5) NOT NULL default ' ',
  "AAABIOFN" varchar(10) NOT NULL default ' ',
  "AAABLEPE" varchar(35) NOT NULL default ' ',
  "AAABIOFO" varchar(10) NOT NULL default ' ',
  "AAABLEPF" varchar(35) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default '0',
  "AAABCOYS" varchar(6) NOT NULL default '000000',
  "AAABCOYT" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMMVH","AAABIOFM","AAABLEPD","AAABIOFN","AAABLEPE","AAABIOFO","AAABLEPF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOZB" (
  "AAABCOZK" varchar(8) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLUEO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABCOZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOZG" (
  "AAABCOZK" varchar(8) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCOZJ" varchar(55) NOT NULL default ' ',
  PRIMARY KEY ("AAABCOZK","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCOZM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  "AAABCPAJ" varchar(2) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABCPAL" varchar(4) NOT NULL default ' ',
  "AAABCPAM" varchar(1) NOT NULL default ' ',
  "AAABCPAO" varchar(2) NOT NULL default '00',
  "AAABCPAP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHY","AAABCPAJ","AAABLEHK","AAABCPAL","AAABCPAM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPAB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCPAO" varchar(2) NOT NULL default '00',
  "AAABCPAP" varchar(3) NOT NULL default ' ',
  "AAABCPAQ" varchar(5) NOT NULL default '00000',
  "AAABCPAG" decimal(17,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  "AAABCPAJ" varchar(2) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABCPAL" varchar(4) NOT NULL default ' ',
  "AAABCPAM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCPAO","AAABCPAP","AAABCPAQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPAR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  "AAABCPAU" tinyint(3) unsigned NOT NULL default '0',
  "AAABCPAV" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABHFHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPAY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABCPBD" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCTJ","AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPBI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABCPBT" varchar(1) NOT NULL default ' ',
  "AAABCPBO" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMAMP","AAABMPWL","AAABCPBT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPBU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABENPM" varchar(4) NOT NULL default ' ',
  "AAABJWRR" varchar(3) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAABCPCA" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABENPM","AAABJWRR","AAABMPWL","AAABMCTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPCG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCPCU" varchar(2) NOT NULL default ' ',
  "AAABCPCJ" varchar(1) NOT NULL default ' ',
  "AAABCPCK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCPCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPCN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCPCU" varchar(2) NOT NULL default ' ',
  "AAABCPCR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCPCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPCV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCPDS" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCPDS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPDA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAABCPEF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMP","AAABMCTG","AAABCPEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPDL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCPDS" varchar(8) NOT NULL default ' ',
  "AAABCPDP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCPDS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPDT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAABCPEF" varchar(1) NOT NULL default ' ',
  "AAABCPEG" varchar(2) NOT NULL default '00',
  "AAABCPEA" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMP","AAABMCTG","AAABCPEF","AAABCPEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPEH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABCPEO" varchar(3) NOT NULL default ' ',
  "AAABCPEL" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABCPEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPEP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAABCPET" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAABMCTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPEX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABCPFB" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPFE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKSTP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKSTP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPFJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKSTP" varchar(2) NOT NULL default ' ',
  "AAABCPFN" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKSTP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPFR" (
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABHHPE" varchar(1) NOT NULL default ' ',
  "AAABCPFU" varchar(1) NOT NULL default ' ',
  "AAABCPFV" varchar(1) NOT NULL default ' ',
  "AAABCPFW" varchar(1) NOT NULL default ' ',
  "AAABCPFX" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFDDP" varchar(30) NOT NULL default ' ',
  "AAABCPGA" varchar(30) NOT NULL default ' ',
  "AAABCPGB" varchar(30) NOT NULL default ' ',
  "AAABCPGC" varchar(30) NOT NULL default ' ',
  "AAABCPGD" varchar(30) NOT NULL default ' ',
  "AAABCPGE" varchar(30) NOT NULL default ' ',
  "AAABCPGF" varchar(30) NOT NULL default ' ',
  "AAABCPGG" varchar(30) NOT NULL default ' ',
  "AAABEINZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPGJ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABFFDZ" varchar(3) NOT NULL default ' ',
  "AAABCPGN" varchar(10) NOT NULL default ' ',
  "AAABCPGO" varchar(15) NOT NULL default ' ',
  "AAABGRZV" varchar(20) NOT NULL default ' ',
  "AAABHALU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLTXG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPGT" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKRHS" varchar(2) NOT NULL default ' ',
  "AAABCPGW" varchar(5) NOT NULL default ' ',
  "AAABCPGX" varchar(10) NOT NULL default ' ',
  "AAABCPGY" varchar(15) NOT NULL default ' ',
  "AAABCPGZ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABKRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPHC" (
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABLDZY" varchar(5) NOT NULL default ' ',
  "AAABMECK" varchar(120) NOT NULL default ' ',
  "AAABLRQK" varchar(120) NOT NULL default ' ',
  "AAABFYUL" decimal(10,2) NOT NULL default '0.00',
  "AAABCPHI" varchar(40) NOT NULL default ' ',
  "AAABLHAH" varchar(40) NOT NULL default ' ',
  "AAABJSDF" smallint(6) NOT NULL default '0',
  "AAABMJVD" text,
  PRIMARY KEY ("AAABMOHM","AAABLDZY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPHO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAABKRXE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPWL","AAABMLXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPHW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFFG" varchar(6) NOT NULL default ' ',
  "AAABEDZZ" varchar(1) NOT NULL default ' ',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAABCPIB" varchar(1) NOT NULL default ' ',
  "AAABCPIC" varchar(1) NOT NULL default ' ',
  "AAABCPID" varchar(1) NOT NULL default ' ',
  "AAABCPIE" varchar(1) NOT NULL default ' ',
  "AAABCPIF" tinyint(3) unsigned NOT NULL default '0',
  "AAABCPIG" tinyint(3) unsigned NOT NULL default '0',
  "AAABCPIH" tinyint(3) unsigned NOT NULL default '0',
  "AAABCPII" varchar(2) NOT NULL default '00',
  "AAABEIJR" varchar(1) NOT NULL default '0',
  "AAABIIXH" varchar(5) NOT NULL default ' ',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABHUQZ" varchar(8) NOT NULL default '00000000',
  "AAABCPIN" varchar(1) NOT NULL default ' ',
  "AAABLXUA" varchar(4) NOT NULL default ' ',
  "AAABCPIP" varchar(1) NOT NULL default ' ',
  "AAABFPVV" varchar(4) NOT NULL default ' ',
  "AAABFAYD" varchar(15) NOT NULL default ' ',
  "AAABCPIS" varchar(15) NOT NULL default ' ',
  "AAABCPIT" varchar(15) NOT NULL default ' ',
  "AAABFAYH" varchar(15) NOT NULL default ' ',
  "AAABFAXZ" varchar(8) NOT NULL default ' ',
  "AAABFAYA" varchar(8) NOT NULL default ' ',
  "AAABFAYE" varchar(15) NOT NULL default ' ',
  "AAABHETT" varchar(1) NOT NULL default ' ',
  "AAABCPIZ" varchar(4) NOT NULL default ' ',
  "AAABIJFT" varchar(1) NOT NULL default ' ',
  "AAABCPJB" varchar(1) NOT NULL default ' ',
  "AAABCPJC" decimal(5,2) NOT NULL default '0.00',
  "AAABCPJD" varchar(10) NOT NULL default ' ',
  "AAABKPVV" varchar(2) NOT NULL default ' ',
  "AAABCPJF" varchar(1) NOT NULL default ' ',
  "AAABCPJG" varchar(1) NOT NULL default ' ',
  "AAABCPJH" varchar(1) NOT NULL default ' ',
  "AAABCPJI" varchar(1) NOT NULL default ' ',
  "AAABCPJJ" varchar(1) NOT NULL default ' ',
  "AAABCPJK" varchar(1) NOT NULL default ' ',
  "AAABCPJL" varchar(15) NOT NULL default ' ',
  "AAABCPJM" varchar(15) NOT NULL default ' ',
  "AAABCPJN" varchar(15) NOT NULL default ' ',
  "AAABCPJO" varchar(15) NOT NULL default ' ',
  "AAABCPJP" varchar(15) NOT NULL default ' ',
  "AAABCPJQ" varchar(15) NOT NULL default ' ',
  "AAABCPJR" varchar(1) NOT NULL default ' ',
  "AAABLENA" varchar(1) NOT NULL default ' ',
  "AAABCPJT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKFFG","AAABEDZZ","AAABKRXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPJY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKFFG" varchar(6) NOT NULL default ' ',
  "AAABEDZZ" varchar(1) NOT NULL default ' ',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAABIMUR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKFFG","AAABEDZZ","AAABKRXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPKK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRXK" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABKRXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPKP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default '000',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABKRXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPKX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAABEDZZ" varchar(1) NOT NULL default ' ',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRXK","AAABEDZZ","AAABKRXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPLG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAABEDZZ" varchar(1) NOT NULL default ' ',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABKRXK","AAABEDZZ","AAABKRXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPLS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABCPTL" varchar(4) NOT NULL default ' ',
  "AAABCPLW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABCPTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPMA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOLZ" varchar(4) NOT NULL default ' ',
  "AAABCPTL" varchar(4) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABCPMF" varchar(1) NOT NULL default ' ',
  "AAABCPMG" varchar(1) NOT NULL default ' ',
  "AAABCPMH" varchar(1) NOT NULL default ' ',
  "AAABCPMI" varchar(1) NOT NULL default ' ',
  "AAABCPMJ" varchar(1) NOT NULL default ' ',
  "AAABCPMK" varchar(1) NOT NULL default ' ',
  "AAABCPML" varchar(1) NOT NULL default ' ',
  "AAABCPMM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOLZ","AAABCPTL","AAABMCNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPMR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABLOLZ" varchar(4) NOT NULL default ' ',
  "AAABLBEK" varchar(1) NOT NULL default ' ',
  "AAABCPMW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABLOLZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPNA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCPTL" varchar(4) NOT NULL default ' ',
  "AAABMJVH" varchar(5) NOT NULL default ' ',
  "AAABGYQH" varchar(20) NOT NULL default ' ',
  "AAABMOHX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMRHS","AAABCPTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPNM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABCPOE" varchar(4) NOT NULL default '0000',
  "AAABCPNV" varchar(6) NOT NULL default ' ',
  "AAABCPOB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAABCPOE","AAABCPNV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPNW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABCPOE" varchar(4) NOT NULL default '0000',
  "AAABKJYV" varchar(10) NOT NULL default ' ',
  "AAABCPOB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAABCPOE","AAABKJYV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPOG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFZAB" varchar(72) NOT NULL default ' ',
  "AAABFNBJ" varchar(32) NOT NULL default ' ',
  "AAABCPRD" varchar(1) NOT NULL default ' ',
  "AAABCPOL" varchar(2) NOT NULL default ' ',
  "AAABCPOM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFZAB","AAABFNBJ","AAABCPRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPOR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFZAB" varchar(72) NOT NULL default ' ',
  "AAABFNBJ" varchar(32) NOT NULL default ' ',
  "AAABCPRD" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFNBV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFZAB","AAABFNBJ","AAABCPRD","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFNBJ" varchar(32) NOT NULL default ' ',
  "AAABFMZT" varchar(30) NOT NULL default ' ',
  "AAABCPRD" varchar(1) NOT NULL default ' ',
  "AAABFMYS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFNBJ","AAABFMZT","AAABCPRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPPN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFZAB" varchar(72) NOT NULL default ' ',
  "AAABFNBJ" varchar(32) NOT NULL default ' ',
  "AAABMRCU" varchar(2) NOT NULL default '00',
  "AAABFMZT" varchar(30) NOT NULL default ' ',
  "AAABCPRD" varchar(1) NOT NULL default ' ',
  "AAABFMYR" varchar(30) NOT NULL default ' ',
  "AAABFMYS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFZAB","AAABFNBJ","AAABMRCU","AAABFMZT","AAABCPRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPQC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCPQV" varchar(10) NOT NULL default '0000000000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default ' ',
  "AAABKGOS" varchar(4) NOT NULL default '0000',
  "AAABCPQI" varchar(1) NOT NULL default ' ',
  "AAABHBRG" varchar(12) NOT NULL default ' ',
  "AAABCPRB" varchar(8) NOT NULL default '00000000',
  "AAABCPRC" varchar(6) NOT NULL default '000000',
  "AAABFSUF" varchar(1) NOT NULL default ' ',
  "AAABCPRD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCPQV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCPQV" varchar(10) NOT NULL default '0000000000',
  "AAABCPRM" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABCPQV","AAABCPRM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPQX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCPRL" varchar(10) NOT NULL default '0000000000',
  "AAABHBRG" varchar(12) NOT NULL default ' ',
  "AAABCPRB" varchar(8) NOT NULL default '00000000',
  "AAABCPRC" varchar(6) NOT NULL default '000000',
  "AAABCPRD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCPRL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPRG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCPRL" varchar(10) NOT NULL default '0000000000',
  "AAABCPRM" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABCPRL","AAABCPRM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPRN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABCPTL" varchar(4) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABFTKZ" varchar(1) NOT NULL default '0',
  "AAABKPSP" decimal(5,2) NOT NULL default '0.00',
  "AAABCPRX" decimal(15,2) NOT NULL default '0.00',
  "AAABCPRY" decimal(5,2) NOT NULL default '0.00',
  "AAABCPRZ" decimal(15,2) NOT NULL default '0.00',
  "AAABCPUN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB","AAABCPTL","AAABMCZG","AAABMCZF","AAABKRUI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPSI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABCPTJ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABCPTJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPSP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCPTJ" varchar(6) NOT NULL default ' ',
  "AAABIMUR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMRHS","AAABCPTJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPSZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABCPTJ" varchar(6) NOT NULL default ' ',
  "AAABLOLZ" varchar(4) NOT NULL default ' ',
  "AAABCPTL" varchar(4) NOT NULL default ' ',
  "AAABCPTF" varchar(1) NOT NULL default ' ',
  "AAABCPTG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABCPTJ","AAABLOLZ","AAABCPTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPTM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEDZZ" varchar(1) NOT NULL default ' ',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABKFFG" varchar(6) NOT NULL default ' ',
  "AAABCPUT" varchar(2) NOT NULL default ' ',
  "AAABLUFA" varchar(1) NOT NULL default '0',
  "AAABGCXZ" decimal(5,2) NOT NULL default '0.00',
  "AAABCPUM" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABEDZZ","AAABKRXF","AAABMCRJ","AAABKFFG","AAABCPUT","AAABLUFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPUD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEDZZ" varchar(1) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABKFFG" varchar(6) NOT NULL default ' ',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAABCPUT" varchar(2) NOT NULL default ' ',
  "AAABLUFA" varchar(1) NOT NULL default '0',
  "AAABGCXZ" decimal(5,2) NOT NULL default '0.00',
  "AAABCPUM" decimal(15,2) NOT NULL default '0.00',
  "AAABCPUN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEDZZ","AAABMCNU","AAABKFFG","AAABKRXF","AAABCPUT","AAABLUFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPUV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABMCXV" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABKOTY","AAABMCXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPVE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKFFG" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMCZF","AAABMCXH","AAABMCZG","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPVS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAABKFFG" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMCZF","AAABKRUI","AAABMCZG","AAABMDBB","AAABCPWH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPWI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFFG" varchar(6) NOT NULL default ' ',
  "AAABEDZZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKFFG","AAABEDZZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPWP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKFFG" varchar(6) NOT NULL default ' ',
  "AAABEDZZ" varchar(1) NOT NULL default ' ',
  "AAABIMUR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKFFG","AAABEDZZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPWZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFFG" varchar(6) NOT NULL default ' ',
  "AAABEDZZ" varchar(1) NOT NULL default ' ',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAABESZT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKFFG","AAABEDZZ","AAABKRXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPXJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABDHLV" varchar(6) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABCPXN" varchar(1) NOT NULL default ' ',
  "AAABCPXO" varchar(1) NOT NULL default ' ',
  "AAABHNLS" varchar(1) NOT NULL default ' ',
  "AAABCPXQ" varchar(1) NOT NULL default ' ',
  "AAABCPXR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABDHLV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPXU" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABCPYA" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPYC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTLR" varchar(4) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTLR","AAABMCNU","AAABMCZF","AAABMCXH","AAABMCZG","AAABLHPM","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPYT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCPZF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCPZF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPYY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCPZF" varchar(10) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCPZF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPZG" (
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHQV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPZJ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCRGY" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIHQV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCPZP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABMKFH" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABLRXL" varchar(1) NOT NULL default ' ',
  "AAABLBDX" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABMKFH","AAABLTXI","AAABLRXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQAD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAABLEBK" decimal(15,0) NOT NULL default '0',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAABCQAK" varchar(30) NOT NULL default ' ',
  "AAABCQAL" varchar(1) NOT NULL default ' ',
  "AAABLBDX" varchar(70) NOT NULL default ' ',
  "AAABCQAN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQAR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCRGY" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABMLQA","AAABIHQV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQBB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABCRGA" varchar(1) NOT NULL default ' ',
  "AAABCRGC" varchar(1) NOT NULL default ' ',
  "AAABGXUV" varchar(20) NOT NULL default ' ',
  "AAABCRGE" varchar(4) NOT NULL default '0000',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAABLEBK" decimal(15,0) NOT NULL default '0',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAABCRGB" varchar(20) NOT NULL default ' ',
  "AAABCQBP" varchar(1) NOT NULL default ' ',
  "AAABLBDX" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABMPMH","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQBW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABCRGY" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABMLQA","AAABIHQV","AAABMPMH","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQCK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABCQCQ" varchar(1) NOT NULL default ' ',
  "AAABCQCR" varchar(20) NOT NULL default ' ',
  "AAABCQCS" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABMPMH","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQCY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCQNX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIHQV","AAABCQNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQDF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCQNX" varchar(30) NOT NULL default ' ',
  "AAABCRGY" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABIHQV","AAABCQNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQDP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCRLO" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABCQDV" varchar(1) NOT NULL default ' ',
  "AAABCQDW" varchar(1) NOT NULL default ' ',
  "AAABCQDX" varchar(1) NOT NULL default ' ',
  "AAABCQDY" varchar(1) NOT NULL default ' ',
  "AAABCQDZ" varchar(1) NOT NULL default ' ',
  "AAABCQEA" varchar(20) NOT NULL default ' ',
  "AAABCQEB" varchar(20) NOT NULL default ' ',
  "AAABCQEC" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABCRLO","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQEI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCRLO" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABCQEO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABCRLO","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQEU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCRLO" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABCQFA" varchar(8) NOT NULL default '00000000',
  "AAABCQFB" varchar(8) NOT NULL default '00000000',
  "AAABCQFC" varchar(20) NOT NULL default ' ',
  "AAABCQFD" varchar(20) NOT NULL default ' ',
  "AAABCQFE" varchar(20) NOT NULL default ' ',
  "AAABCQFF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABCRLO","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQFL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABCQGD" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABMPMH","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQFX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCRLO" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABCQGD" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABCRLO","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQGJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCRLO" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABMMRH" varchar(4) NOT NULL default '0000',
  "AAABCQGQ" varchar(30) NOT NULL default ' ',
  "AAABCREF" varchar(20) NOT NULL default ' ',
  "AAABCREW" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABCRLO","AAABLTXI","AAABMMRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQGZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCRLO" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABCQHF" decimal(23,0) NOT NULL default '0',
  "AAABCREF" varchar(20) NOT NULL default ' ',
  "AAABCQHH" decimal(23,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABCRLO","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQHN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCRLO" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABCQHT" varchar(32) NOT NULL default ' ',
  "AAABCQHU" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABCRLO","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQIA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABMJWH" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABCQLF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABMOHM","AAABMJWH","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQIO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCRLO" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABCQIV" varchar(1) NOT NULL default ' ',
  "AAABMJWH" varchar(30) NOT NULL default ' ',
  "AAABCQIX" varchar(4) NOT NULL default '0000',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABCQKB" varchar(30) NOT NULL default ' ',
  "AAABMJCM" varchar(8) NOT NULL default ' ',
  "AAABCQJB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABCRLO","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQJH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCRLO" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABJETU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABCRLO","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQJT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCRLO" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABLMLR" varchar(4) NOT NULL default '0000',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABCQKB" varchar(30) NOT NULL default ' ',
  "AAABCQKC" varchar(8) NOT NULL default ' ',
  "AAABCREE" varchar(1) NOT NULL default ' ',
  "AAABCREF" varchar(20) NOT NULL default ' ',
  "AAABCREG" varchar(4) NOT NULL default '0000',
  "AAABHDJO" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABCRLO","AAABLTXI","AAABLMLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQKN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCRLO" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABMMRE" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABCRLO","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQKZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCRLO" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABCQLF" varchar(30) NOT NULL default ' ',
  "AAABCQLG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABCRLO","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQLM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCRLO" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABCQLS" varchar(30) NOT NULL default ' ',
  "AAABLUEL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABCRLO","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQLZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCRLO" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABMMRH" varchar(4) NOT NULL default '0000',
  "AAABCQMG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABCRLO","AAABLTXI","AAABMMRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQMN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCRLO" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABMMRH" varchar(4) NOT NULL default '0000',
  "AAABCREE" varchar(1) NOT NULL default ' ',
  "AAABCREF" varchar(20) NOT NULL default ' ',
  "AAABCREG" varchar(4) NOT NULL default '0000',
  "AAABCQMX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABCRLO","AAABLTXI","AAABMMRH","AAABCREE","AAABCREF","AAABCREG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQNH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCRLO" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAABLEBK" decimal(15,0) NOT NULL default '0',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAABCRGC" varchar(1) NOT NULL default ' ',
  "AAABGXUV" varchar(20) NOT NULL default ' ',
  "AAABCRGE" varchar(4) NOT NULL default '0000',
  "AAABCQNT" varchar(1) NOT NULL default ' ',
  "AAABCQNU" varchar(1) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABCQNW" varchar(4) NOT NULL default '0000',
  "AAABCQNX" varchar(30) NOT NULL default ' ',
  "AAABCREP" varchar(1) NOT NULL default ' ',
  "AAABCREQ" varchar(6) NOT NULL default '000000',
  "AAABCQOA" varchar(6) NOT NULL default '000000',
  "AAABCQOB" varchar(6) NOT NULL default '000000',
  "AAABCRGB" varchar(20) NOT NULL default ' ',
  "AAABLBDX" varchar(70) NOT NULL default ' ',
  "AAABCQOE" varchar(1) NOT NULL default ' ',
  "AAABCRJZ" varchar(32) NOT NULL default ' ',
  "AAABCQOG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABCRLO","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQOM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCRLO" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABCRGY" varchar(80) NOT NULL default ' ',
  "AAABCQOU" varchar(80) NOT NULL default ' ',
  "AAABCQOV" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABMLQA","AAABIHQV","AAABCRLO","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQPC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABLUFA" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAABLEBK" decimal(15,0) NOT NULL default '0',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAABCRGB" varchar(20) NOT NULL default ' ',
  "AAABLBDX" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABLUFA","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQPS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABLUFA" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABCRGY" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABMLQA","AAABIHQV","AAABLUFA","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQQG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABLUFA" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABCRGC" varchar(1) NOT NULL default ' ',
  "AAABGXUV" varchar(20) NOT NULL default ' ',
  "AAABCRGE" varchar(4) NOT NULL default '0000',
  "AAABCQQP" int(11) NOT NULL default '0',
  "AAABCQQQ" int(11) NOT NULL default '0',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAABLEBK" decimal(15,0) NOT NULL default '0',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABLUFA","AAABLTXI","AAABCRGC","AAABGXUV","AAABCRGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQRC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABLUFA" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABCRCP" varchar(1) NOT NULL default ' ',
  "AAABCQRR" varchar(30) NOT NULL default ' ',
  "AAABLMLR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABLUFA","AAABLTXI","AAABCRCP","AAABCQRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQRS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABLUFA" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABCRGC" varchar(1) NOT NULL default ' ',
  "AAABGXUV" varchar(20) NOT NULL default ' ',
  "AAABCRGE" varchar(4) NOT NULL default '0000',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABLUFA","AAABLTXI","AAABCRGC","AAABGXUV","AAABCRGE","AAABMLDW","AAABMLDX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQSO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABLUFA" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABCRGC" varchar(1) NOT NULL default ' ',
  "AAABGXUV" varchar(20) NOT NULL default ' ',
  "AAABCRGE" varchar(4) NOT NULL default '0000',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABLMLR" varchar(1) NOT NULL default '0',
  "AAABCQTA" varchar(1) NOT NULL default ' ',
  "AAABCRLN" varchar(1) NOT NULL default ' ',
  "AAABCRLO" varchar(20) NOT NULL default ' ',
  "AAABCRLP" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABLUFA","AAABLTXI","AAABCRGC","AAABGXUV","AAABCRGE","AAABMLDW","AAABMLDX","AAABLMLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQTP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABLUFA" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABCRGC" varchar(1) NOT NULL default ' ',
  "AAABGXUV" varchar(20) NOT NULL default ' ',
  "AAABCRGE" varchar(4) NOT NULL default '0000',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABLMLR" varchar(1) NOT NULL default '0',
  "AAABCQUC" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABCRLJ","AAABIHQV","AAABLUFA","AAABLTXI","AAABCRGC","AAABGXUV","AAABCRGE","AAABMLDW","AAABMLDX","AAABLMLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQUP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABGXUV" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABLRXL" varchar(1) NOT NULL default ' ',
  "AAABCQUX" varchar(30) NOT NULL default ' ',
  "AAABCQUY" varchar(30) NOT NULL default ' ',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAABLEBK" decimal(15,0) NOT NULL default '0',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  "AAABLBDX" varchar(70) NOT NULL default ' ',
  "AAABCQVE" varchar(30) NOT NULL default ' ',
  "AAABCQVF" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABGXUV","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQVL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGXUV" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABCRGY" varchar(80) NOT NULL default ' ',
  "AAABMJYX" varchar(30) NOT NULL default ' ',
  "AAABCQVU" varchar(70) NOT NULL default ' ',
  "AAABCQVV" varchar(70) NOT NULL default ' ',
  "AAABCQVW" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABMLQA","AAABGXUV","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQWD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMJCM" varchar(8) NOT NULL default ' ',
  "AAABCQXT" varchar(1) NOT NULL default ' ',
  "AAABCQWH" varchar(1) NOT NULL default ' ',
  "AAABCQWI" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABMJCM","AAABCQXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQWM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMJCM" varchar(8) NOT NULL default ' ',
  "AAABCQXT" varchar(1) NOT NULL default ' ',
  "AAABCRGY" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABMJCM","AAABCQXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQWW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIJYV" varchar(1) NOT NULL default ' ',
  "AAABIJYX" varchar(1) NOT NULL default ' ',
  "AAABMJCM" varchar(8) NOT NULL default ' ',
  "AAABCQXT" varchar(1) NOT NULL default ' ',
  "AAABCREP" varchar(1) NOT NULL default ' ',
  "AAABEFRD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIJYV","AAABIJYX","AAABMJCM","AAABCQXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQXJ" (
  "AAABIJYV" varchar(1) NOT NULL default ' ',
  "AAABIJYX" varchar(1) NOT NULL default ' ',
  "AAABMJCM" varchar(8) NOT NULL default ' ',
  "AAABCQXT" varchar(1) NOT NULL default ' ',
  "AAABCREP" varchar(1) NOT NULL default ' ',
  "AAABEFRD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIJYV","AAABIJYX","AAABMJCM","AAABCQXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQXU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCQYK" varchar(1) NOT NULL default ' ',
  "AAABCQYL" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABLRXL" varchar(1) NOT NULL default ' ',
  "AAABCRHW" varchar(1) NOT NULL default ' ',
  "AAABMKFH" varchar(20) NOT NULL default ' ',
  "AAABCRHY" varchar(4) NOT NULL default '0000',
  "AAABCRHP" int(11) NOT NULL default '0',
  "AAABCRFG" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABCQYK","AAABCQYL","AAABLTXI","AAABLRXL","AAABCRHW","AAABMKFH","AAABCRHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCQYR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABCRLN" varchar(1) NOT NULL default ' ',
  "AAABCRLO" varchar(20) NOT NULL default ' ',
  "AAABCRLP" varchar(4) NOT NULL default '0000',
  "AAABCRLQ" varchar(1) NOT NULL default ' ',
  "AAABLUFA" varchar(20) NOT NULL default ' ',
  "AAABCRLS" varchar(4) NOT NULL default '0000',
  "AAABCRIJ" varchar(1) NOT NULL default ' ',
  "AAABCRII" varchar(20) NOT NULL default ' ',
  "AAABCRIK" varchar(4) NOT NULL default '0000',
  "AAABCRLA" varchar(1) NOT NULL default ' ',
  "AAABCRLB" varchar(1) NOT NULL default ' ',
  "AAABCRLC" varchar(1) NOT NULL default ' ',
  "AAABCRLD" varchar(1) NOT NULL default ' ',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAABLEBK" decimal(15,0) NOT NULL default '0',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAABGZIA" varchar(4) NOT NULL default '0000',
  "AAABCRFL" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABMPMH","AAABLTXI","AAABCRLN","AAABCRLO","AAABCRLP","AAABCRLQ","AAABLUFA","AAABCRLS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRAA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIHQV","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRAH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCRGB" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIHQV","AAABCRGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRAO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCRGB" varchar(20) NOT NULL default ' ',
  "AAABCRGY" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABIHQV","AAABCRGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRAY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABLRXL" varchar(1) NOT NULL default ' ',
  "AAABCRHW" varchar(1) NOT NULL default ' ',
  "AAABMKFH" varchar(20) NOT NULL default ' ',
  "AAABCRHY" varchar(4) NOT NULL default '0000',
  "AAABCRHP" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABMPMH","AAABLTXI","AAABLRXL","AAABCRHW","AAABMKFH","AAABCRHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRBS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABLUFA" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABCRGC" varchar(1) NOT NULL default ' ',
  "AAABGXUV" varchar(20) NOT NULL default ' ',
  "AAABCRGE" varchar(4) NOT NULL default '0000',
  "AAABCRCP" varchar(1) NOT NULL default ' ',
  "AAABCRCQ" varchar(4) NOT NULL default '0000',
  "AAABCRFG" varchar(4) NOT NULL default '0000',
  "AAABCREE" varchar(1) NOT NULL default ' ',
  "AAABCREF" varchar(20) NOT NULL default ' ',
  "AAABCREG" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABLUFA","AAABLTXI","AAABCRGC","AAABGXUV","AAABCRGE","AAABCRCP","AAABCRCQ","AAABCRFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRCS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABLUFA" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABCRGC" varchar(1) NOT NULL default ' ',
  "AAABGXUV" varchar(20) NOT NULL default ' ',
  "AAABCRGE" varchar(4) NOT NULL default '0000',
  "AAABCRLN" varchar(1) NOT NULL default ' ',
  "AAABCRLO" varchar(20) NOT NULL default ' ',
  "AAABCRLP" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABLUFA","AAABLTXI","AAABCRGC","AAABGXUV","AAABCRGE","AAABCRLN","AAABCRLO","AAABCRLP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRDP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCRLO" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABCREE" varchar(1) NOT NULL default ' ',
  "AAABCREF" varchar(20) NOT NULL default ' ',
  "AAABCREG" varchar(4) NOT NULL default '0000',
  "AAABCRFG" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABCRLO","AAABLTXI","AAABCREE","AAABCREF","AAABCREG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCREH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCRLO" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABCREW" varchar(4) NOT NULL default '0000',
  "AAABJETU" varchar(50) NOT NULL default ' ',
  "AAABCREP" varchar(1) NOT NULL default ' ',
  "AAABCREQ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABCRLO","AAABLTXI","AAABCREW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCREX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCRLO" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABCRFR" varchar(1) NOT NULL default ' ',
  "AAABCRFS" varchar(20) NOT NULL default ' ',
  "AAABCRFT" varchar(4) NOT NULL default '0000',
  "AAABCRFG" varchar(4) NOT NULL default '0000',
  "AAABCRIJ" varchar(1) NOT NULL default ' ',
  "AAABCRII" varchar(20) NOT NULL default ' ',
  "AAABCRIK" varchar(4) NOT NULL default '0000',
  "AAABGZIA" varchar(4) NOT NULL default '0000',
  "AAABCRFL" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABCRLO","AAABLTXI","AAABCRFR","AAABCRFS","AAABCRFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRFU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCRIW" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABCRGA" varchar(1) NOT NULL default ' ',
  "AAABCRGB" varchar(20) NOT NULL default ' ',
  "AAABCRGC" varchar(1) NOT NULL default ' ',
  "AAABGXUV" varchar(20) NOT NULL default ' ',
  "AAABCRGE" varchar(4) NOT NULL default '0000',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAABLEBK" decimal(15,0) NOT NULL default '0',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAABCRGI" varchar(1) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABCRII" varchar(20) NOT NULL default ' ',
  "AAABLBDX" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABCRIW","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRGR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCRIW" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABCRGY" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABMLQA","AAABIHQV","AAABCRIW","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRHF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCRIW" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABLRXL" varchar(1) NOT NULL default ' ',
  "AAABCRHW" varchar(1) NOT NULL default ' ',
  "AAABMKFH" varchar(20) NOT NULL default ' ',
  "AAABCRHY" varchar(4) NOT NULL default '0000',
  "AAABCRHP" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABCRIW","AAABLTXI","AAABLRXL","AAABCRHW","AAABMKFH","AAABCRHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRHZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCRIW" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABCRLO" varchar(20) NOT NULL default ' ',
  "AAABLUFA" varchar(20) NOT NULL default ' ',
  "AAABCRII" varchar(20) NOT NULL default ' ',
  "AAABCRIJ" varchar(1) NOT NULL default ' ',
  "AAABCRIK" varchar(4) NOT NULL default '0000',
  "AAABCRLA" varchar(1) NOT NULL default ' ',
  "AAABCRLB" varchar(1) NOT NULL default ' ',
  "AAABCRLC" varchar(1) NOT NULL default ' ',
  "AAABCRLD" varchar(1) NOT NULL default ' ',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAABLEBK" decimal(15,0) NOT NULL default '0',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAABGZIA" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABCRIW","AAABLTXI","AAABMPMH","AAABCRLO","AAABLUFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRJB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCRJZ" varchar(32) NOT NULL default ' ',
  "AAABCRJG" varchar(4) NOT NULL default '0000',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAABLEBK" decimal(15,0) NOT NULL default '0',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABIHQV","AAABCRJZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRJO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(30) NOT NULL default ' ',
  "AAABCRJZ" varchar(32) NOT NULL default ' ',
  "AAABCRJU" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABCRLJ","AAABIHQV","AAABCRJZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRKA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLEUZ" varchar(12) NOT NULL default ' ',
  "AAABCRKD" varchar(4) NOT NULL default ' ',
  "AAABCRKE" varchar(12) NOT NULL default ' ',
  "AAABCRKF" varchar(1) NOT NULL default ' ',
  "AAABDPTW" varchar(1) NOT NULL default ' ',
  "AAABCRKH" varchar(1) NOT NULL default ' ',
  "AAABDPSI" varchar(5) NOT NULL default '00000',
  "AAABCRKJ" varchar(1) NOT NULL default ' ',
  "AAABCRKK" varchar(1) NOT NULL default ' ',
  "AAABLHAH" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLEUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRKO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCRLJ" varchar(1) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABCRLM" varchar(1) NOT NULL default ' ',
  "AAABCRLN" varchar(1) NOT NULL default ' ',
  "AAABCRLO" varchar(20) NOT NULL default ' ',
  "AAABCRLP" varchar(4) NOT NULL default '0000',
  "AAABCRLQ" varchar(1) NOT NULL default ' ',
  "AAABLUFA" varchar(20) NOT NULL default ' ',
  "AAABCRLS" varchar(4) NOT NULL default '0000',
  "AAABCRLA" varchar(1) NOT NULL default ' ',
  "AAABCRLB" varchar(1) NOT NULL default ' ',
  "AAABCRLC" varchar(1) NOT NULL default ' ',
  "AAABCRLD" varchar(1) NOT NULL default ' ',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAABLEBK" decimal(15,0) NOT NULL default '0',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAABGZIA" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLUXQ","AAABCRLJ","AAABMPMH","AAABLTXI","AAABCRLM","AAABCRLN","AAABCRLO","AAABCRLP","AAABCRLQ","AAABLUFA","AAABCRLS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRLT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCRMI" varchar(10) NOT NULL default ' ',
  "AAABFUGD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCRMI","AAABFUGD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRMA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCRMI" varchar(10) NOT NULL default ' ',
  "AAABFUGD" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCRMI","AAABFUGD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRMK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCRMW" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCRMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRMP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCRMW" varchar(3) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCRMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRMX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOK" varchar(16) NOT NULL default ' ',
  "AAABCRNA" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLTOK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRND" (
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABDSZA" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRNH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABLUEO" varchar(32) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQDU","AAABLPJD","AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRNR" (
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABLUVJ" varchar(8) NOT NULL default ' ',
  "AAABCROC" varchar(30) NOT NULL default ' ',
  "AAABJGIU" varchar(70) NOT NULL default ' ',
  "AAABCRNW" varchar(6) NOT NULL default '000000',
  "AAABCRNX" varchar(8) NOT NULL default '00000000',
  "AAABCRNY" varchar(40) NOT NULL default ' ',
  "AAABCRNZ" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLZKF","AAABLUVJ","AAABCROC","AAABJGIU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCROE" (
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAABCRXM" int(11) NOT NULL default '0',
  "AAABCROI" varchar(32) NOT NULL default ' ',
  "AAABCROJ" varchar(8) NOT NULL default ' ',
  "AAABCRSF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTJH","AAABLTJI","AAABCRXM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCROO" (
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAABCRRT" int(11) NOT NULL default '0',
  "AAABCRSF" varchar(1) NOT NULL default ' ',
  "AAABCROT" varchar(32) NOT NULL default ' ',
  "AAABCROU" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTJH","AAABLTJI","AAABCRRT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCROY" (
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAABCRPU" varchar(32) NOT NULL default ' ',
  "AAABCRPK" varchar(32) NOT NULL default ' ',
  "AAABCRPD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTJH","AAABLTJI","AAABCRPU","AAABCRPK"),
  KEY "AAABCRPI" ("AAABCRPU","AAABCRPK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRPL" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABCRPU" varchar(32) NOT NULL default ' ',
  "AAABCRPO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABCRPU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRPR" (
  "AAABCRPU" varchar(32) NOT NULL default ' ',
  "AAABCRPT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABCRPU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRPV" (
  "AAABCRWY" varchar(40) NOT NULL default ' ',
  "AAABCRPX" int(11) NOT NULL default '0',
  "AAABCRPY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABCRWY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRQA" (
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAABLEUA" int(11) NOT NULL default '0',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  "AAABCRXK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTJH","AAABLTJI","AAABLEUA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRQK" (
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAABCRWX" varchar(12) NOT NULL default ' ',
  "AAABCRWY" varchar(40) NOT NULL default ' ',
  "AAABLEUA" int(11) NOT NULL default '0',
  "AAABFORA" varchar(8) NOT NULL default '00000000',
  "AAABLESZ" varchar(6) NOT NULL default '000000',
  "AAABFOQZ" varchar(32) NOT NULL default ' ',
  "AAABCRTT" varchar(8) NOT NULL default '00000000',
  "AAABCRTU" varchar(6) NOT NULL default '000000',
  "AAABCRXJ" varchar(12) NOT NULL default ' ',
  "AAABCRQW" varchar(8) NOT NULL default '00000000',
  "AAABCRQX" varchar(6) NOT NULL default '000000',
  "AAABCRQY" varchar(12) NOT NULL default ' ',
  "AAABDCDZ" varchar(8) NOT NULL default '00000000',
  "AAABCRRA" varchar(6) NOT NULL default '000000',
  "AAABLNZY" varchar(12) NOT NULL default ' ',
  "AAABLNZW" varchar(8) NOT NULL default '00000000',
  "AAABLNZX" varchar(6) NOT NULL default '000000',
  "AAABLEQN" varchar(8) NOT NULL default '00000000',
  "AAABLEQO" varchar(6) NOT NULL default '000000',
  "AAABCRXB" varchar(2) NOT NULL default '00',
  "AAABCRXC" varchar(2) NOT NULL default '00',
  "AAABCRXD" varchar(3) NOT NULL default '000',
  "AAABCRXE" varchar(2) NOT NULL default '00',
  "AAABCRXF" varchar(2) NOT NULL default '00',
  "AAABCRXG" varchar(1) NOT NULL default ' ',
  "AAABCRXH" varchar(1) NOT NULL default ' ',
  "AAABCRXI" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABCRRP" varchar(1) NOT NULL default ' ',
  "AAABCRXK" varchar(12) NOT NULL default ' ',
  "AAABCRXL" varchar(3) NOT NULL default '000',
  "AAABCRXM" int(11) NOT NULL default '0',
  "AAABCRRT" int(11) NOT NULL default '0',
  "AAABLEQZ" varchar(20) NOT NULL default ' ',
  "AAABDEFP" varchar(8) NOT NULL default '00000000',
  "AAABDEFQ" varchar(6) NOT NULL default '000000',
  "AAABCRRX" int(11) NOT NULL default '0',
  "AAABCRTE" int(11) NOT NULL default '0',
  "AAABLESX" varchar(32) NOT NULL default ' ',
  "AAABLESY" varchar(64) NOT NULL default ' ',
  "AAABCRTF" varchar(32) NOT NULL default ' ',
  "AAABCRXO" varchar(1) NOT NULL default ' ',
  "AAABMNRG" int(11) NOT NULL default '0',
  "AAABCRSE" varchar(8) NOT NULL default ' ',
  "AAABCRSF" varchar(1) NOT NULL default ' ',
  "AAABHHSJ" varchar(2) NOT NULL default ' ',
  "AAABCRSH" varchar(1) NOT NULL default ' ',
  "AAABCRXQ" varchar(20) NOT NULL default ' ',
  "AAABCRXN" int(11) NOT NULL default '0',
  "AAABCRXR" int(11) NOT NULL default '0',
  "AAABCRSL" varchar(20) NOT NULL default ' ',
  "AAABCRSM" varchar(10) NOT NULL default ' ',
  "AAABCRSN" varchar(10) NOT NULL default ' ',
  "AAABCRSO" varchar(70) NOT NULL default ' ',
  "AAABCRSP" varchar(10) NOT NULL default ' ',
  "AAABCRXS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTJH","AAABLTJI"),
  KEY "AAABCRST" ("AAABCRXJ","AAABCRXL"),
  KEY "AAABCRSW" ("AAABMRFO","AAABLTJH"),
  KEY "AAABCRSZ" ("AAABLESX","AAABLESY"),
  KEY "AAABCRTC" ("AAABMRFO","AAABCRTE","AAABCRTF"),
  KEY "AAABCRTG" ("AAABFORA","AAABLESZ","AAABMRFO","AAABCRXJ","AAABLTJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRTM" (
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAABLEUA" int(11) NOT NULL default '0',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABCRTR" int(11) NOT NULL default '0',
  "AAABCRTS" int(11) NOT NULL default '0',
  "AAABCRTT" varchar(8) NOT NULL default '00000000',
  "AAABCRTU" varchar(6) NOT NULL default '000000',
  "AAABCRXJ" varchar(12) NOT NULL default ' ',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  "AAABCRXK" varchar(12) NOT NULL default ' ',
  "AAABCRTY" varchar(10) NOT NULL default '0000000000',
  "AAABCRTZ" varchar(10) NOT NULL default ' ',
  "AAABCRUA" varchar(32) NOT NULL default ' ',
  "AAABCRUB" varchar(32) NOT NULL default ' ',
  "AAABLESU" varchar(128) NOT NULL default ' ',
  "AAABLESV" varchar(255) NOT NULL default ' ',
  "AAABCRUE" varchar(1) NOT NULL default ' ',
  "AAABCRUF" varchar(1) NOT NULL default ' ',
  "AAABCRUG" varchar(1) NOT NULL default ' ',
  "AAABCRUH" varchar(1) NOT NULL default ' ',
  "AAABCRUI" varchar(1) NOT NULL default ' ',
  "AAABCRUJ" varchar(1) NOT NULL default ' ',
  "AAABCRUK" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABCRUM" int(11) NOT NULL default '0',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABCRUO" varchar(18) NOT NULL default ' ',
  "AAABHBMV" varchar(10) NOT NULL default ' ',
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
  "AAABMAZE" int(11) NOT NULL default '0',
  "AAABMBXE" varchar(10) NOT NULL default ' ',
  "AAABMBXF" varchar(10) NOT NULL default ' ',
  "AAABMAZT" varchar(2) NOT NULL default ' ',
  "AAABLPNX" varchar(20) NOT NULL default ' ',
  "AAABGETL" varchar(32) NOT NULL default ' ',
  "AAABGETM" varchar(32) NOT NULL default ' ',
  "AAABMMYV" varchar(14) NOT NULL default ' ',
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABHMNR" varchar(3) NOT NULL default ' ',
  "AAABCRVV" varchar(40) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default ' ',
  "AAABCRVX" varchar(12) NOT NULL default ' ',
  "AAABGEXP" varchar(4) NOT NULL default ' ',
  "AAABMNDR" varchar(16) NOT NULL default ' ',
  "AAABGETF" varchar(70) NOT NULL default ' ',
  "AAABMINC" varchar(8) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default ' ',
  "AAABCRWD" int(11) NOT NULL default '0',
  "AAABCRWE" varchar(16) NOT NULL default ' ',
  "AAABFYOH" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTJH","AAABLTJI","AAABLEUA"),
  KEY "AAABCRWJ" ("AAABLESS","AAABLEST"),
  KEY "AAABCRWM" ("AAABLESU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRWO" (
  "AAABCRWS" varchar(40) NOT NULL default ' ',
  "AAABCRWQ" varchar(8) NOT NULL default '00000000',
  "AAABIJNY" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABCRWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRWT" (
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAABFOQZ" varchar(32) NOT NULL default ' ',
  "AAABCRWX" varchar(12) NOT NULL default ' ',
  "AAABCRWY" varchar(40) NOT NULL default ' ',
  "AAABFORA" varchar(8) NOT NULL default '00000000',
  "AAABLESZ" varchar(6) NOT NULL default '000000',
  "AAABCRXB" varchar(2) NOT NULL default '00',
  "AAABCRXC" varchar(2) NOT NULL default '00',
  "AAABCRXD" varchar(3) NOT NULL default '000',
  "AAABCRXE" varchar(2) NOT NULL default '00',
  "AAABCRXF" varchar(2) NOT NULL default '00',
  "AAABCRXG" varchar(1) NOT NULL default ' ',
  "AAABCRXH" varchar(1) NOT NULL default ' ',
  "AAABCRXI" varchar(1) NOT NULL default ' ',
  "AAABCRXJ" varchar(12) NOT NULL default ' ',
  "AAABCRXK" varchar(12) NOT NULL default ' ',
  "AAABCRXL" varchar(3) NOT NULL default '000',
  "AAABCRXM" int(11) NOT NULL default '0',
  "AAABCRXN" int(11) NOT NULL default '0',
  "AAABCRXO" varchar(1) NOT NULL default ' ',
  "AAABMNRG" int(11) NOT NULL default '0',
  "AAABCRXQ" varchar(20) NOT NULL default ' ',
  "AAABCRXR" int(11) NOT NULL default '0',
  "AAABCRXS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTJH","AAABLTJI"),
  KEY "AAABCRXV" ("AAABFORA","AAABLESZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRXY" (
  "AAABCRYI" varchar(64) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABCRYH" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABCRYI","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRYE" (
  "AAABCRYI" varchar(64) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABCRYH" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABCRYI","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRYK" (
  "AAABLOLZ" varchar(4) NOT NULL default ' ',
  "AAABIADW" varchar(1) NOT NULL default ' ',
  "AAABCRYN" varchar(1) NOT NULL default ' ',
  "AAABCRYO" varchar(1) NOT NULL default ' ',
  "AAABLRTR" varchar(1) NOT NULL default ' ',
  "AAABCRYQ" varchar(1) NOT NULL default ' ',
  "AAABCRYR" varchar(1) NOT NULL default ' ',
  "AAABCRYS" varchar(1) NOT NULL default ' ',
  "AAABLRTQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOLZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRYV" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLOLZ" varchar(4) NOT NULL default ' ',
  "AAABLXIS" varchar(5) NOT NULL default ' ',
  "AAABGGHA" varchar(20) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLOLZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRZD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLCGL" varchar(4) NOT NULL default ' ',
  "AAABLHAH" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLCGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRZJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLCGL" varchar(4) NOT NULL default ' ',
  "AAABCRZN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABLCGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCRZR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCRZZ" varchar(4) NOT NULL default ' ',
  "AAABCSAA" varchar(4) NOT NULL default ' ',
  "AAABCSAB" varchar(2) NOT NULL default ' ',
  "AAABCRZW" varchar(2) NOT NULL default ' ',
  "AAABCRZX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCRZZ","AAABCSAA","AAABCSAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSAC" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABCSAG" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABCSAG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSAH" (
  "AAABCSBR" varchar(5) NOT NULL default '00000',
  "AAABCSBP" varchar(5) NOT NULL default '00000',
  "AAABCSBQ" varchar(20) NOT NULL default ' ',
  "AAABCSAZ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABCSBR","AAABCSBP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSAO" (
  "AAABCSBR" varchar(5) NOT NULL default '00000',
  "AAABCSBP" varchar(5) NOT NULL default '00000',
  "AAABCSBQ" varchar(20) NOT NULL default ' ',
  "AAABCSAZ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABCSBR","AAABCSBP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSAV" (
  "AAABCSBR" varchar(5) NOT NULL default '00000',
  "AAABCSBP" varchar(5) NOT NULL default '00000',
  "AAABCSBQ" varchar(20) NOT NULL default ' ',
  "AAABCSAZ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABCSBR","AAABCSBP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSBC" (
  "AAABCSBR" varchar(5) NOT NULL default '00000',
  "AAABCSBP" varchar(5) NOT NULL default '00000',
  "AAABCSBQ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABCSBR","AAABCSBP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSBI" (
  "AAABCSBR" varchar(10) NOT NULL default '0000000000',
  "AAABCSBP" varchar(20) NOT NULL default ' ',
  "AAABCSBQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABCSBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSBN" (
  "AAABCSBR" varchar(10) NOT NULL default '0000000000',
  "AAABCSBP" varchar(20) NOT NULL default ' ',
  "AAABCSBQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABCSBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSBS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABKVKH" varchar(6) NOT NULL default ' ',
  "AAABLSMW" varchar(6) NOT NULL default ' ',
  "AAABCSBX" varchar(6) NOT NULL default ' ',
  "AAABCTSS" varchar(2) NOT NULL default ' ',
  "AAABCSBZ" varchar(1) NOT NULL default ' ',
  "AAABCSCA" varchar(1) NOT NULL default ' ',
  "AAABCSCB" varchar(1) NOT NULL default ' ',
  "AAABCSCC" varchar(1) NOT NULL default ' ',
  "AAABCSCD" varchar(1) NOT NULL default ' ',
  "AAABCSCE" varchar(6) NOT NULL default ' ',
  "AAABJQZB" varchar(1) NOT NULL default ' ',
  "AAABCSCG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABKVKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSCK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABGYLC" varchar(4) NOT NULL default ' ',
  "AAABCSCO" varchar(10) NOT NULL default ' ',
  "AAABCSCP" varchar(10) NOT NULL default ' ',
  "AAABCSNP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABGYLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSCU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABGYLC" varchar(4) NOT NULL default ' ',
  "AAABGMNJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKWSS","AAABGYLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSDE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABKVKH" varchar(6) NOT NULL default ' ',
  "AAABGYLC" varchar(4) NOT NULL default ' ',
  "AAABCTUD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABKVKH","AAABGYLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSDO" (
  "AAABCTSS" varchar(2) NOT NULL default ' ',
  "AAABHLJJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABCTSS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSDS" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCTSS" varchar(2) NOT NULL default ' ',
  "AAABGNCZ" varchar(50) NOT NULL default ' ',
  "AAABCSQB" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABCTSS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSDZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABKVKH" varchar(6) NOT NULL default ' ',
  "AAABHSKS" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABKVKH","AAABHSKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSEI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCSNF" varchar(6) NOT NULL default ' ',
  "AAABLSMW" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABCSNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSEQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCSNF" varchar(6) NOT NULL default ' ',
  "AAABGNCZ" varchar(50) NOT NULL default ' ',
  "AAABCTRJ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKWSS","AAABCSNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSFB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCSNF" varchar(6) NOT NULL default ' ',
  "AAABKVKH" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABCSNF","AAABKVKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSFK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCSNF" varchar(6) NOT NULL default ' ',
  "AAABCTUY" varchar(6) NOT NULL default ' ',
  "AAABLSMW" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABCSNF","AAABCTUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSFU" (
  "AAABMDBL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSFX" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMDBL" varchar(2) NOT NULL default ' ',
  "AAABGNCZ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMDBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSGD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABLUFA" varchar(2) NOT NULL default ' ',
  "AAABMDBL" varchar(2) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABLUFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSGN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABLUFA" varchar(2) NOT NULL default ' ',
  "AAABKHGP" varchar(30) NOT NULL default ' ',
  "AAABMECK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKWSS","AAABLUFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSGY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABLUFA" varchar(2) NOT NULL default ' ',
  "AAABKYNY" varchar(250) NOT NULL default ' ',
  "AAABKYNZ" varchar(250) NOT NULL default ' ',
  "AAABKYOA" varchar(250) NOT NULL default ' ',
  "AAABKWJH" varchar(250) NOT NULL default ' ',
  "AAABKWJI" varchar(250) NOT NULL default ' ',
  "AAABKWJJ" varchar(250) NOT NULL default ' ',
  "AAABKWJK" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABLUFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSHM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABLUFA" varchar(2) NOT NULL default ' ',
  "AAABCSYR" varchar(250) NOT NULL default ' ',
  "AAABCSYS" varchar(250) NOT NULL default ' ',
  "AAABCSYT" varchar(250) NOT NULL default ' ',
  "AAABCSYU" varchar(250) NOT NULL default ' ',
  "AAABCSYV" varchar(250) NOT NULL default ' ',
  "AAABCSYW" varchar(250) NOT NULL default ' ',
  "AAABCSYX" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABLUFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSIA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABIAEU" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABIAEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSIH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABIAEU" varchar(6) NOT NULL default ' ',
  "AAABMECK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKWSS","AAABIAEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSIR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABKVKH" varchar(6) NOT NULL default ' ',
  "AAABIAEU" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABKVKH","AAABIAEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSJA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABKVKH" varchar(6) NOT NULL default ' ',
  "AAABGNCZ" varchar(50) NOT NULL default ' ',
  "AAABCTRJ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKWSS","AAABKVKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSJL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABKVKH" varchar(6) NOT NULL default ' ',
  "AAABCTUY" varchar(6) NOT NULL default ' ',
  "AAABLSMW" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABKVKH","AAABCTUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSJV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCSJY" varchar(1) NOT NULL default ' ',
  "AAABCSRC" varchar(1) NOT NULL default ' ',
  "AAABCSWU" varchar(1) NOT NULL default ' ',
  "AAABCSKB" varchar(1) NOT NULL default ' ',
  "AAABCSKC" varchar(30) NOT NULL default ' ',
  "AAABCSKD" varchar(30) NOT NULL default ' ',
  "AAABCSKE" varchar(30) NOT NULL default ' ',
  "AAABCSKF" varchar(30) NOT NULL default ' ',
  "AAABCSKG" varchar(1) NOT NULL default ' ',
  "AAABCSKH" varchar(1) NOT NULL default ' ',
  "AAABCSKI" varchar(1) NOT NULL default ' ',
  "AAABCSKJ" varchar(30) NOT NULL default ' ',
  "AAABCSKK" varchar(30) NOT NULL default ' ',
  "AAABCSKL" varchar(30) NOT NULL default ' ',
  "AAABCSKM" varchar(30) NOT NULL default ' ',
  "AAABCSKN" varchar(30) NOT NULL default ' ',
  "AAABCSKO" varchar(30) NOT NULL default ' ',
  "AAABCSKP" varchar(10) NOT NULL default ' ',
  "AAABCSKQ" varchar(10) NOT NULL default ' ',
  "AAABCSKR" varchar(20) NOT NULL default ' ',
  "AAABCSKS" varchar(3) NOT NULL default ' ',
  "AAABCSKT" varchar(20) NOT NULL default ' ',
  "AAABCSKU" varchar(3) NOT NULL default ' ',
  "AAABCSKV" varchar(20) NOT NULL default ' ',
  "AAABCSKW" varchar(3) NOT NULL default ' ',
  "AAABCSKX" varchar(10) NOT NULL default ' ',
  "AAABCSKY" varchar(10) NOT NULL default ' ',
  "AAABCSKZ" varchar(30) NOT NULL default ' ',
  "AAABCSLA" varchar(30) NOT NULL default ' ',
  "AAABCSLB" varchar(30) NOT NULL default ' ',
  "AAABCSLC" varchar(30) NOT NULL default ' ',
  "AAABCTIZ" varchar(30) NOT NULL default ' ',
  "AAABCTJA" varchar(30) NOT NULL default ' ',
  "AAABCSLF" varchar(10) NOT NULL default ' ',
  "AAABCSLG" varchar(15) NOT NULL default ' ',
  "AAABCTJB" varchar(30) NOT NULL default ' ',
  "AAABCSLI" varchar(30) NOT NULL default ' ',
  "AAABCSLJ" varchar(1) NOT NULL default ' ',
  "AAABLKXV" varchar(10) NOT NULL default ' ',
  "AAABCSLL" varchar(3) NOT NULL default ' ',
  "AAABCSLM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSLP" (
  "AAABCSNR" varchar(4) NOT NULL default ' ',
  "AAABGMML" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABCSNR","AAABGMML")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSLU" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCSNR" varchar(4) NOT NULL default ' ',
  "AAABGMML" varchar(5) NOT NULL default ' ',
  "AAABEPBY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABCSNR","AAABGMML")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSMC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCSML" varchar(30) NOT NULL default ' ',
  "AAABCSMM" varchar(30) NOT NULL default ' ',
  "AAABCSPQ" varchar(30) NOT NULL default ' ',
  "AAABCSPR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABCSML","AAABCSMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSMN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCTJU" varchar(4) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABCTJU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSMV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABKVKH" varchar(6) NOT NULL default ' ',
  "AAABCSNF" varchar(6) NOT NULL default ' ',
  "AAABMDBL" varchar(2) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABKVKH","AAABCSNF","AAABMDBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSNH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCSNR" varchar(4) NOT NULL default ' ',
  "AAABGMML" varchar(5) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABMLTD" varchar(7) NOT NULL default '0000000',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABGYLC" varchar(4) NOT NULL default ' ',
  "AAABCSNP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCSNR","AAABGMML","AAABKWSS","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSNV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCTSS" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABCTSS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSOC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABGYQM" varchar(30) NOT NULL default ' ',
  "AAABCSOG" varchar(1) NOT NULL default ' ',
  "AAABCSOH" varchar(30) NOT NULL default ' ',
  "AAABCSOI" varchar(30) NOT NULL default ' ',
  "AAABCSOJ" varchar(30) NOT NULL default ' ',
  "AAABCSOK" varchar(15) NOT NULL default ' ',
  "AAABCSOL" varchar(30) NOT NULL default ' ',
  "AAABCTQC" varchar(6) NOT NULL default ' ',
  "AAABCSON" varchar(30) NOT NULL default ' ',
  "AAABCSOO" varchar(30) NOT NULL default ' ',
  "AAABCSOP" varchar(30) NOT NULL default ' ',
  "AAABCSOQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABGYQM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSOU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCSPK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABCSPK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSPB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCSPK" varchar(10) NOT NULL default ' ',
  "AAABCSPG" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKWSS","AAABCSPK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSPL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCSPU" varchar(30) NOT NULL default ' ',
  "AAABCSPV" varchar(30) NOT NULL default ' ',
  "AAABCSPQ" varchar(30) NOT NULL default ' ',
  "AAABCSPR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABCSPU","AAABCSPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSPW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABGNCZ" varchar(50) NOT NULL default ' ',
  "AAABCSQB" varchar(5) NOT NULL default ' ',
  "AAABCSQC" varchar(25) NOT NULL default ' ',
  "AAABCSQD" varchar(30) NOT NULL default ' ',
  "AAABCSQE" varchar(30) NOT NULL default ' ',
  "AAABJFPC" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKWSS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSQJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABLSMW" varchar(6) NOT NULL default ' ',
  "AAABCSQN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABLSMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSQR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCSWL" varchar(6) NOT NULL default ' ',
  "AAABCSQV" varchar(1) NOT NULL default ' ',
  "AAABCSQW" varchar(1) NOT NULL default ' ',
  "AAABCSQX" varchar(1) NOT NULL default ' ',
  "AAABCSQY" varchar(1) NOT NULL default ' ',
  "AAABGPHD" varchar(4) NOT NULL default '0000',
  "AAABCSRA" varchar(1) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABCSRC" varchar(1) NOT NULL default ' ',
  "AAABCSRD" varchar(1) NOT NULL default ' ',
  "AAABCSRE" varchar(1) NOT NULL default ' ',
  "AAABCSRF" varchar(1) NOT NULL default ' ',
  "AAABCSRG" varchar(2) NOT NULL default '00',
  "AAABCSRH" varchar(2) NOT NULL default '00',
  "AAABCSRI" varchar(2) NOT NULL default '00',
  "AAABCSRJ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABCSWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSRN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCSWL" varchar(6) NOT NULL default ' ',
  "AAABCSRR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABCSWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSRV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCSWL" varchar(6) NOT NULL default ' ',
  "AAABGPHD" varchar(4) NOT NULL default '0000',
  "AAABCSSA" varchar(4) NOT NULL default '0000',
  "AAABCSSB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABCSWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSSF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCSWL" varchar(6) NOT NULL default ' ',
  "AAABCSSK" varchar(50) NOT NULL default ' ',
  "AAABCSSL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKWSS","AAABCSWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSSQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCSVP" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABCSVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSSX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCSVP" varchar(6) NOT NULL default ' ',
  "AAABGNCZ" varchar(50) NOT NULL default ' ',
  "AAABHTIK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKWSS","AAABCSVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSTI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABHSKS" varchar(6) NOT NULL default ' ',
  "AAABCTSS" varchar(2) NOT NULL default ' ',
  "AAABCTSA" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABCSTP" varchar(30) NOT NULL default ' ',
  "AAABCSTQ" varchar(30) NOT NULL default ' ',
  "AAABCSUU" varchar(30) NOT NULL default ' ',
  "AAABGYLC" varchar(4) NOT NULL default ' ',
  "AAABCSTT" varchar(1) NOT NULL default ' ',
  "AAABCSTU" varchar(1) NOT NULL default ' ',
  "AAABCSTV" varchar(10) NOT NULL default ' ',
  "AAABCSTW" varchar(30) NOT NULL default ' ',
  "AAABCSTX" varchar(1) NOT NULL default ' ',
  "AAABCSTY" varchar(1) NOT NULL default ' ',
  "AAABCSTZ" varchar(1) NOT NULL default ' ',
  "AAABFJCO" varchar(1) NOT NULL default ' ',
  "AAABIAEU" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABHSKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSUF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABHSKS" varchar(6) NOT NULL default ' ',
  "AAABGNCZ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKWSS","AAABHSKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSUP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABHSKS" varchar(6) NOT NULL default ' ',
  "AAABGYLC" varchar(4) NOT NULL default ' ',
  "AAABCSUU" varchar(30) NOT NULL default ' ',
  "AAABCSWB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABHSKS","AAABGYLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSVA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCSVP" varchar(6) NOT NULL default ' ',
  "AAABMLTD" varchar(7) NOT NULL default '0000000',
  "AAABHSKS" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABCSVP","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSVK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCSWL" varchar(6) NOT NULL default ' ',
  "AAABMLTD" varchar(7) NOT NULL default '0000000',
  "AAABCSVP" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABCSWL","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSVU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABGNDC" varchar(4) NOT NULL default '0000',
  "AAABMLTD" varchar(7) NOT NULL default '0000000',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABGYLC" varchar(4) NOT NULL default ' ',
  "AAABCSWB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABGNDC","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSWG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABLSMW" varchar(6) NOT NULL default ' ',
  "AAABMLTD" varchar(7) NOT NULL default '0000000',
  "AAABCSWL" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABLSMW","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSWQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCTUY" varchar(6) NOT NULL default ' ',
  "AAABCSWU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABCTUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSWY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCTUY" varchar(6) NOT NULL default ' ',
  "AAABEPBY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKWSS","AAABCTUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSXI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCTUY" varchar(6) NOT NULL default ' ',
  "AAABLSMW" varchar(6) NOT NULL default ' ',
  "AAABCSXN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABCTUY","AAABLSMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSXS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCSZH" varchar(10) NOT NULL default ' ',
  "AAABGMNJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKWSS","AAABCSZH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSYC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABKYNY" varchar(250) NOT NULL default ' ',
  "AAABKYNZ" varchar(250) NOT NULL default ' ',
  "AAABKYOA" varchar(250) NOT NULL default ' ',
  "AAABKWJH" varchar(250) NOT NULL default ' ',
  "AAABKWJI" varchar(250) NOT NULL default ' ',
  "AAABKWJJ" varchar(250) NOT NULL default ' ',
  "AAABKWJK" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSYO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABCSYR" varchar(250) NOT NULL default ' ',
  "AAABCSYS" varchar(250) NOT NULL default ' ',
  "AAABCSYT" varchar(250) NOT NULL default ' ',
  "AAABCSYU" varchar(250) NOT NULL default ' ',
  "AAABCSYV" varchar(250) NOT NULL default ' ',
  "AAABCSYW" varchar(250) NOT NULL default ' ',
  "AAABCSYX" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSZA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCSZH" varchar(10) NOT NULL default ' ',
  "AAABCSZE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABCSZH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--enable_query_log
