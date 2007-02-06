# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+
# |                             tbl22.inc                              |
# |                        Copyright 2006 by MySQL AB                  |
# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+

--disable_query_log
CREATE TABLE "AAAAUHRY" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJTDU" varchar(3) NOT NULL default ' ',
  "AAAAVCGL" varchar(8) NOT NULL default '00000000',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJTDU","AAAAVCGL","AAABINAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHSI" (
  "AAAAUZZV" varchar(1) NOT NULL default ' ',
  "AAABLFYJ" varchar(5) NOT NULL default '00000',
  "AAABMBXY" varchar(5) NOT NULL default '00000',
  "AAABLFYL" varchar(2) NOT NULL default '00',
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABMOIA" varchar(48) NOT NULL default ' ',
  "AAABLQJP" varchar(10) NOT NULL default ' ',
  "AAAAUIKM" varchar(40) NOT NULL default ' ',
  "AAAAUIAO" varchar(1) NOT NULL default ' ',
  "AAAAVRVK" varchar(32) NOT NULL default ' ',
  "AAABLOSB" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAAAUZZV","AAABLFYJ","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHSX" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMBXY" varchar(5) NOT NULL default '00000',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHTD" (
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABHEQX" varchar(48) NOT NULL default ' ',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  "AAABEPTB" varchar(20) NOT NULL default ' ',
  "AAAAUHYF" varchar(2) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAUHVN" varchar(8) NOT NULL default '00000000',
  "AAAAUHTM" varchar(1) NOT NULL default ' ',
  "AAABFZBP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABHJPB","AAABMOIA","AAABHEQX","AAABLEST","AAABEPTB","AAAAUHYF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHTU" (
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABMOIA" varchar(48) NOT NULL default ' ',
  "AAABEPTB" varchar(20) NOT NULL default ' ',
  "AAAAUHYF" varchar(2) NOT NULL default ' ',
  "AAABFZBP" varchar(40) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAUHVN" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABHJPB","AAABMOIA","AAABEPTB","AAAAUHYF","AAABFZBP"),
  KEY "AAAAUHUH" ("AAABFZBP"),
  KEY "AAAAUHUJ" ("AAABEPTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHUL" (
  "AAABILGV" varchar(8) NOT NULL default '00000000',
  "AAABILGW" varchar(6) NOT NULL default '000000',
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABMOIA" varchar(48) NOT NULL default ' ',
  "AAABEPTB" varchar(20) NOT NULL default ' ',
  "AAAAUHYF" varchar(2) NOT NULL default ' ',
  "AAABFZBP" varchar(40) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAUHVN" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABILGV","AAABILGW","AAABHJPB","AAABMOIA","AAABEPTB","AAAAUHYF","AAABFZBP"),
  KEY "AAAAUHVC" ("AAABFZBP"),
  KEY "AAAAUHVE" ("AAABEPTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHVG" (
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABMOIA" varchar(48) NOT NULL default ' ',
  "AAABEPTB" varchar(20) NOT NULL default ' ',
  "AAAAUHYF" varchar(2) NOT NULL default ' ',
  "AAABFZBP" varchar(40) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAUHVN" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABHJPB","AAABMOIA","AAABEPTB","AAAAUHYF","AAABFZBP"),
  KEY "AAAAUHVT" ("AAABFZBP"),
  KEY "AAAAUHVV" ("AAABEPTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHVX" (
  "AAAAUZZV" varchar(1) NOT NULL default ' ',
  "AAABLFYJ" varchar(5) NOT NULL default '00000',
  "AAABMBXY" varchar(5) NOT NULL default '00000',
  "AAABLFYL" varchar(2) NOT NULL default '00',
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABMOIA" varchar(48) NOT NULL default ' ',
  "AAABLQJP" varchar(10) NOT NULL default ' ',
  "AAAAUIKM" varchar(40) NOT NULL default ' ',
  "AAAAUIAO" varchar(1) NOT NULL default ' ',
  "AAAAVRVK" varchar(32) NOT NULL default ' ',
  "AAABLOSB" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAAAUZZV","AAABLFYJ","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHWM" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMBXY" varchar(5) NOT NULL default '00000',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHWS" (
  "AAAAUZZV" varchar(1) NOT NULL default ' ',
  "AAABLFYJ" varchar(5) NOT NULL default '00000',
  "AAABMBXY" varchar(5) NOT NULL default '00000',
  "AAABLFYL" varchar(2) NOT NULL default '00',
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABMOIA" varchar(48) NOT NULL default ' ',
  "AAABLQJP" varchar(10) NOT NULL default ' ',
  "AAAAUIKM" varchar(40) NOT NULL default ' ',
  "AAAAUIAO" varchar(1) NOT NULL default ' ',
  "AAAAVRVK" varchar(32) NOT NULL default ' ',
  "AAABLOSB" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAAAUZZV","AAABLFYJ","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHXH" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMBXY" varchar(5) NOT NULL default '00000',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHXN" (
  "AAAAUHYF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUHYF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHXQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAUHXZ" varchar(10) NOT NULL default ' ',
  "AAABLQJM" varchar(8) NOT NULL default '00000000',
  "AAABLQJN" varchar(6) NOT NULL default '000000',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAAAUHXZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHYA" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAUHYF" varchar(2) NOT NULL default ' ',
  "AAABMECK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAAUHYF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHYG" (
  "AAAAUZZV" varchar(1) NOT NULL default ' ',
  "AAABLFYJ" varchar(5) NOT NULL default '00000',
  "AAABMBXY" varchar(5) NOT NULL default '00000',
  "AAABLFYL" varchar(2) NOT NULL default '00',
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABMOIA" varchar(48) NOT NULL default ' ',
  "AAABLQJP" varchar(10) NOT NULL default ' ',
  "AAAAUIKM" varchar(40) NOT NULL default ' ',
  "AAAAUIAO" varchar(1) NOT NULL default ' ',
  "AAAAVRVK" varchar(32) NOT NULL default ' ',
  "AAABLOSB" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAAAUZZV","AAABLFYJ","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHYV" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMBXY" varchar(5) NOT NULL default '00000',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHZB" (
  "AAABLUSV" varchar(48) NOT NULL default ' ',
  "AAAAUHZD" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHZF" (
  "AAAAUZZV" varchar(1) NOT NULL default ' ',
  "AAABLFYJ" varchar(5) NOT NULL default '00000',
  "AAABMBXY" varchar(5) NOT NULL default '00000',
  "AAABLFYL" varchar(2) NOT NULL default '00',
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABMOIA" varchar(48) NOT NULL default ' ',
  "AAABLQJP" varchar(10) NOT NULL default ' ',
  "AAAAUIKM" varchar(40) NOT NULL default ' ',
  "AAABLOSB" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAAAUZZV","AAABLFYJ","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHZS" (
  "AAAAUZZV" varchar(1) NOT NULL default ' ',
  "AAABLFYJ" varchar(5) NOT NULL default '00000',
  "AAABMBXY" varchar(5) NOT NULL default '00000',
  "AAABLFYL" varchar(2) NOT NULL default '00',
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  "AAABLQJP" varchar(10) NOT NULL default ' ',
  "AAAAUIKM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUZZV","AAABLFYJ","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIAF" (
  "AAAAUZZV" varchar(1) NOT NULL default ' ',
  "AAABLFYJ" varchar(5) NOT NULL default '00000',
  "AAABMBXY" varchar(5) NOT NULL default '00000',
  "AAABLFYL" varchar(2) NOT NULL default '00',
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABMOIA" varchar(48) NOT NULL default ' ',
  "AAABLQJP" varchar(10) NOT NULL default ' ',
  "AAAAUIKM" varchar(40) NOT NULL default ' ',
  "AAAAUIAO" varchar(1) NOT NULL default ' ',
  "AAAAVRVK" varchar(32) NOT NULL default ' ',
  "AAABLOSB" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAAAUZZV","AAABLFYJ","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIAU" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMBXY" varchar(5) NOT NULL default '00000',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIBA" (
  "AAABLOLC" varchar(4) NOT NULL default ' ',
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABMOIA" varchar(48) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOLC","AAABHJPB","AAABMOIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIBH" (
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABMOIA" varchar(48) NOT NULL default ' ',
  "AAAAUIKM" varchar(40) NOT NULL default ' ',
  "AAAAUIKN" varchar(40) NOT NULL default ' ',
  "AAAAUIBW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHJPB","AAABMOIA","AAAAUIKM","AAAAUIKN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIBR" (
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABMOIA" varchar(48) NOT NULL default ' ',
  "AAAAUIKM" varchar(40) NOT NULL default ' ',
  "AAAAUIKN" varchar(40) NOT NULL default ' ',
  "AAAAUIBW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHJPB","AAABMOIA","AAAAUIKM","AAAAUIKN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUICB" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJTDU" varchar(3) NOT NULL default ' ',
  "AAAAUICK" varchar(4) NOT NULL default '0000',
  "AAAAVAVT" varchar(8) NOT NULL default '00000000',
  "AAAAUICG" varchar(8) NOT NULL default '00000000',
  "AAAAUICH" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMONG","AAABJTDU","AAAAUICK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUICL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLQJM" varchar(8) NOT NULL default '00000000',
  "AAABLQJN" varchar(6) NOT NULL default '000000',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUICV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMVU" varchar(2) NOT NULL default ' ',
  "AAABLUSV" varchar(12) NOT NULL default ' ',
  "AAABLFYJ" varchar(5) NOT NULL default '00000',
  "AAABMBXY" varchar(5) NOT NULL default '00000',
  "AAABLFYL" varchar(2) NOT NULL default '00',
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABMOIA" varchar(48) NOT NULL default ' ',
  "AAABLQJP" varchar(10) NOT NULL default ' ',
  "AAAAVRVK" varchar(32) NOT NULL default ' ',
  "AAABLOSB" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABMMVU","AAABLUSV","AAABLFYJ","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIDM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLFYJ" varchar(10) NOT NULL default '0000000000',
  "AAABMBXY" varchar(10) NOT NULL default '0000000000',
  "AAABLFYL" varchar(2) NOT NULL default '00',
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABMOIA" varchar(48) NOT NULL default ' ',
  "AAABLQJP" varchar(10) NOT NULL default ' ',
  "AAAAVRVK" varchar(32) NOT NULL default ' ',
  "AAABLOSB" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABLFYJ","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIEB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMBXY" varchar(10) NOT NULL default '0000000000',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABMRHS","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIEL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMVU" varchar(2) NOT NULL default ' ',
  "AAABLUSV" varchar(12) NOT NULL default ' ',
  "AAABLFYJ" varchar(5) NOT NULL default '00000',
  "AAABMBXY" varchar(5) NOT NULL default '00000',
  "AAABLFYL" varchar(2) NOT NULL default '00',
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABMOIA" varchar(48) NOT NULL default ' ',
  "AAABLQJP" varchar(10) NOT NULL default ' ',
  "AAAAVRVK" varchar(32) NOT NULL default ' ',
  "AAABLOSB" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABMMVU","AAABLUSV","AAABLFYJ","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIFC" (
  "AAABMMVU" varchar(2) NOT NULL default ' ',
  "AAABLUSV" varchar(12) NOT NULL default ' ',
  "AAABLFYJ" varchar(5) NOT NULL default '00000',
  "AAABMBXY" varchar(5) NOT NULL default '00000',
  "AAABLFYL" varchar(2) NOT NULL default '00',
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  "AAABLQJP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMVU","AAABLUSV","AAABLFYJ","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIFQ" (
  "AAABLFYL" varchar(2) NOT NULL default '00',
  "AAAAUIGD" varchar(2) NOT NULL default ' ',
  "AAABMBXY" varchar(5) NOT NULL default '00000',
  "AAABLFYJ" varchar(5) NOT NULL default '00000',
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAAAVABY" varchar(1) NOT NULL default ' ',
  "AAABMOIA" varchar(48) NOT NULL default ' ',
  "AAABLQJP" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(100) NOT NULL default ' ',
  "AAAAVRVK" varchar(32) NOT NULL default ' ',
  "AAAAUIGP" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFYL","AAAAUIGD","AAABMBXY","AAABLFYJ","AAABHJPB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIGH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABMOIA" varchar(48) NOT NULL default ' ',
  "AAABLFYJ" varchar(5) NOT NULL default '00000',
  "AAABMBXY" varchar(5) NOT NULL default '00000',
  "AAABLFYL" varchar(2) NOT NULL default '00',
  "AAAAUIGP" varchar(12) NOT NULL default ' ',
  "AAABLQJP" varchar(10) NOT NULL default ' ',
  "AAABLOSB" varchar(5) NOT NULL default '00000',
  "AAABEBKQ" varchar(5) NOT NULL default '00000',
  "AAABEBKR" varchar(5) NOT NULL default '00000',
  "AAAAVRVK" varchar(32) NOT NULL default ' ',
  "AAABMRCK" varchar(100) NOT NULL default ' ',
  "AAABHWQS" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABHJPB","AAABMOIA","AAABLFYJ","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIHD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMBXY" varchar(5) NOT NULL default '00000',
  "AAABMJXI" varchar(1) NOT NULL default ' ',
  "AAABLHAH" varchar(255) NOT NULL default ' ',
  "AAAAUIHJ" varchar(1) NOT NULL default ' ',
  "AAAAUIHK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIHO" (
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABMOIA" varchar(48) NOT NULL default ' ',
  "AAABLYTE" varchar(3) NOT NULL default ' ',
  "AAABLEHK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHJPB","AAABMOIA","AAABLYTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIHW" (
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  "AAABLYTE" varchar(3) NOT NULL default ' ',
  "AAABLEHK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHJPB","AAABMOIA","AAABLEST","AAABLYTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIIG" (
  "AAABMMVU" varchar(2) NOT NULL default ' ',
  "AAABLUSV" varchar(12) NOT NULL default ' ',
  "AAABLQJM" varchar(8) NOT NULL default '00000000',
  "AAABLQJN" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMMVU","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIIN" (
  "AAABMMVU" varchar(2) NOT NULL default ' ',
  "AAABLUSV" varchar(12) NOT NULL default ' ',
  "AAABLQJM" varchar(8) NOT NULL default '00000000',
  "AAABLQJN" varchar(6) NOT NULL default '000000',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMMVU","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIIV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" varchar(12) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLQJM" varchar(8) NOT NULL default '00000000',
  "AAABLQJN" varchar(6) NOT NULL default '000000',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLUSV","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIJF" (
  "AAABFZBP" varchar(40) NOT NULL default ' ',
  "AAAAUIJO" varchar(2) NOT NULL default ' ',
  "AAAAUIJP" varchar(2) NOT NULL default ' ',
  "AAAAUIJQ" varchar(2) NOT NULL default ' ',
  "AAAAUIJR" varchar(2) NOT NULL default ' ',
  "AAABMBXY" varchar(5) NOT NULL default '00000',
  "AAAAVRVK" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZBP","AAAAUIJO","AAAAUIJP","AAAAUIJQ","AAAAUIJR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIJS" (
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABHJPB","AAABMOIA","AAABLEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIJZ" (
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABMOIA" varchar(48) NOT NULL default ' ',
  PRIMARY KEY ("AAABHJPB","AAABMOIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIKE" (
  "AAABILHK" varchar(10) NOT NULL default ' ',
  "AAAAUZZV" varchar(1) NOT NULL default ' ',
  "AAABLFYJ" varchar(5) NOT NULL default '00000',
  "AAABMBXY" varchar(5) NOT NULL default '00000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABMOIA" varchar(48) NOT NULL default ' ',
  "AAAAUIKM" varchar(40) NOT NULL default ' ',
  "AAAAUIKN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABILHK","AAAAUZZV","AAABLFYJ","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIKS" (
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABMCMG" varchar(22) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(73) NOT NULL default ' ',
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABLSIN" varchar(3) NOT NULL default ' ',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFU","AAABMCMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUILF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAAUILO" varchar(10) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAAAUILO","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUILQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" decimal(21,7) NOT NULL default '0.0000000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABDHLV" varchar(1) NOT NULL default ' ',
  "AAABLJNI" varchar(100) NOT NULL default ' ',
  "AAABHTWG" varchar(30) NOT NULL default ' ',
  "AAAAUJYD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIMA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" decimal(21,7) NOT NULL default '0.0000000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABDHLV" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABFJTV" varchar(2) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAUIMI" varchar(3) NOT NULL default '000',
  "AAAAUIMJ" varchar(3) NOT NULL default '000',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIMN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" decimal(21,7) NOT NULL default '0.0000000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABDHLV" varchar(1) NOT NULL default ' ',
  "AAABLTHM" varchar(8) NOT NULL default '00000000',
  "AAABLTHN" varchar(6) NOT NULL default ' ',
  "AAABCRPK" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIMX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" decimal(21,7) NOT NULL default '0.0000000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABDHLV" varchar(1) NOT NULL default ' ',
  "AAABLTHM" varchar(8) NOT NULL default '00000000',
  "AAABLTHN" varchar(6) NOT NULL default ' ',
  "AAABCRPK" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUINH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" decimal(21,7) NOT NULL default '0.0000000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABDHLV" varchar(1) NOT NULL default ' ',
  "AAABLTHM" varchar(8) NOT NULL default '00000000',
  "AAABLTHN" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUINQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" decimal(21,7) NOT NULL default '0.0000000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABDHLV" varchar(1) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAAAUJXQ" varchar(3) NOT NULL default '000',
  "AAAAUJXR" varchar(3) NOT NULL default '000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABLHZO" varchar(4) NOT NULL default '0000',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIOJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKQPE" varchar(2) NOT NULL default '00',
  "AAAAUJCP" varchar(10) NOT NULL default ' ',
  "AAABBCMQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABKQPE","AAAAUJCP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIOT" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIPA" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABHZCK" varchar(12) NOT NULL default ' ',
  "AAABHZCL" varchar(14) NOT NULL default '00000000000000',
  "AAABHZCM" varchar(1) NOT NULL default ' ',
  "AAABHZCN" varchar(1) NOT NULL default ' ',
  "AAABLECQ" varchar(15) NOT NULL default ' ',
  "AAABHZCP" varchar(255) NOT NULL default ' ',
  "AAABHZCQ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR","AAABHZJH"),
  KEY "AAAAUIPP" ("AAABHZJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIPR" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABMLQA","AAABLLLR","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIQF" (
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
  KEY "AAAAUIQT" ("AAABMRFO","AAABMLQA","AAABHZDJ"),
  KEY "AAAAUIQX" ("AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIQZ" (
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
  PRIMARY KEY ("AAABLLLR"),
  KEY "AAAAUIRS" ("AAABLLLH"),
  KEY "AAAAUIRU" ("AAABLLLI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIRW" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUISD" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUISN" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUISR" (
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
  UNIQUE KEY "AAAAUITE" ("AAABLLNL"),
  KEY "AAAAUITG" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUITI" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNN","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUITV" ("AAABLLMD","AAABLLME","AAABLLND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUITZ" (
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
  UNIQUE KEY "AAAAUIUM" ("AAABLLNL"),
  KEY "AAAAUIUO" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIUQ" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNI","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUIVD" ("AAABLLMD","AAABLLME","AAABLLNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIVH" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABHZIS" varchar(4) NOT NULL default ' ',
  "AAABHZGP" varchar(10) NOT NULL default ' ',
  "AAABKERC" varchar(128) NOT NULL default ' ',
  "AAABICGK" varchar(12) NOT NULL default '000000000000',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  "AAABHZGT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR","AAABHZJH","AAABHZGV"),
  KEY "AAAAUIVX" ("AAABHZJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIVZ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
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
  PRIMARY KEY ("AAABMLQA","AAABLLLR","AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAAUIWR" ("AAABLLNL"),
  KEY "AAAAUIWT" ("AAABLLNN"),
  KEY "AAAAUIWV" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIWX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLVCU" varchar(4) NOT NULL default '0000',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
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
  PRIMARY KEY ("AAABMLQA","AAABLLLR","AAABHZJH"),
  KEY "AAAAUIXZ" ("AAABLLLR"),
  KEY "AAAAUIYB" ("AAABHZJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIYD" (
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
  UNIQUE KEY "AAAAUIYR" ("AAABLLNL"),
  KEY "AAAAUIYT" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIYV" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUIZJ" ("AAABLLMD","AAABLLME","AAABLLMF"),
  KEY "AAAAUIZN" ("AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUIZP" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
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
  PRIMARY KEY ("AAABMLQA","AAABLLLR","AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAAUJAG" ("AAABLLNL"),
  KEY "AAAAUJAI" ("AAABLLNN"),
  KEY "AAAAUJAK" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJAM" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
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
  PRIMARY KEY ("AAABMLQA","AAABLLLR","AAABLLNI","AAABLLNL"),
  UNIQUE KEY "AAAAUJBD" ("AAABLLNL"),
  KEY "AAAAUJBF" ("AAABLLNN"),
  KEY "AAAAUJBH" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJBJ" (
  "AAABJZWV" varchar(4) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABJQWM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJZWV","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJBP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" decimal(21,7) NOT NULL default '0.0000000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABDHLV" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAAAUJXQ" varchar(3) NOT NULL default '000',
  "AAAAUJXR" varchar(3) NOT NULL default '000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABLHZO" varchar(4) NOT NULL default '0000',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJCE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKQPE" varchar(2) NOT NULL default '00',
  "AAAAUJCP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKQPE","AAAAUJCP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJCL" (
  "AAAAUJFL" varchar(3) NOT NULL default '000',
  "AAABKQPE" varchar(2) NOT NULL default '00',
  "AAAAUJFN" varchar(10) NOT NULL default ' ',
  "AAAAUJCP" varchar(10) NOT NULL default ' ',
  "AAABKQOV" varchar(5) NOT NULL default ' ',
  "AAABKQOW" varchar(12) NOT NULL default ' ',
  "AAABKQOX" varchar(8) NOT NULL default '00000000',
  "AAABKQOY" varchar(12) NOT NULL default ' ',
  "AAABKQOZ" varchar(8) NOT NULL default '00000000',
  "AAABKQQB" varchar(3) NOT NULL default '000',
  "AAABKQPQ" varchar(1) NOT NULL default ' ',
  "AAABKQPR" varchar(1) NOT NULL default ' ',
  "AAABKQPS" decimal(5,0) NOT NULL default '0',
  "AAABKQPT" varchar(3) NOT NULL default ' ',
  "AAABKQPU" decimal(3,0) NOT NULL default '0',
  "AAABKQPV" varchar(2) NOT NULL default ' ',
  "AAABKQPX" varchar(1) NOT NULL default ' ',
  "AAABKQPY" varchar(1) NOT NULL default ' ',
  "AAABKQPZ" varchar(1) NOT NULL default ' ',
  "AAABKQQA" varchar(1) NOT NULL default ' ',
  "AAABKQQR" varchar(1) NOT NULL default ' ',
  "AAABKQQS" varchar(1) NOT NULL default ' ',
  "AAABKQQT" varchar(1) NOT NULL default ' ',
  "AAABKQQU" varchar(1) NOT NULL default ' ',
  "AAABKQQV" varchar(1) NOT NULL default ' ',
  "AAABKQQW" varchar(1) NOT NULL default ' ',
  "AAABKQRD" varchar(5) NOT NULL default ' ',
  "AAABKQRE" varchar(5) NOT NULL default ' ',
  "AAABKQRF" varchar(5) NOT NULL default ' ',
  "AAABKQRG" varchar(5) NOT NULL default ' ',
  "AAABKQRH" varchar(5) NOT NULL default ' ',
  "AAABKQRI" varchar(5) NOT NULL default ' ',
  "AAABKQRP" varchar(7) NOT NULL default ' ',
  "AAABKQRQ" varchar(7) NOT NULL default ' ',
  "AAABKQRR" varchar(7) NOT NULL default ' ',
  "AAABKQRS" varchar(7) NOT NULL default ' ',
  "AAABKQRT" varchar(7) NOT NULL default ' ',
  "AAABKQRU" varchar(7) NOT NULL default ' ',
  "AAABKQRV" varchar(1) NOT NULL default ' ',
  "AAABKQRW" varchar(1) NOT NULL default ' ',
  "AAABKQRX" varchar(1) NOT NULL default ' ',
  "AAABKQRY" varchar(1) NOT NULL default ' ',
  "AAABKQRZ" varchar(1) NOT NULL default ' ',
  "AAABKQSA" varchar(1) NOT NULL default ' ',
  "AAABKQSB" varchar(1) NOT NULL default ' ',
  "AAABKQSC" varchar(1) NOT NULL default ' ',
  "AAABKQSD" varchar(1) NOT NULL default ' ',
  "AAABKQSE" varchar(1) NOT NULL default ' ',
  "AAABKQSF" varchar(1) NOT NULL default ' ',
  "AAABKQSG" varchar(1) NOT NULL default ' ',
  "AAABKQSH" varchar(1) NOT NULL default ' ',
  "AAABKQSI" varchar(1) NOT NULL default ' ',
  "AAABKQSJ" varchar(1) NOT NULL default ' ',
  "AAABKQSK" varchar(1) NOT NULL default ' ',
  "AAABKQSL" varchar(1) NOT NULL default ' ',
  "AAABKQSM" varchar(1) NOT NULL default ' ',
  "AAABKQRJ" varchar(5) NOT NULL default ' ',
  "AAABKQRK" varchar(5) NOT NULL default ' ',
  "AAABKQRL" varchar(5) NOT NULL default ' ',
  "AAABKQRM" varchar(5) NOT NULL default ' ',
  "AAABKQRN" varchar(5) NOT NULL default ' ',
  "AAABKQRO" varchar(5) NOT NULL default ' ',
  "AAABKQQG" varchar(4) NOT NULL default ' ',
  "AAABKQQH" varchar(8) NOT NULL default ' ',
  "AAABKQQI" varchar(2) NOT NULL default ' ',
  "AAABKQQJ" varchar(8) NOT NULL default ' ',
  "AAABKQQK" varchar(2) NOT NULL default ' ',
  "AAABKQQL" varchar(4) NOT NULL default ' ',
  "AAABKQQM" varchar(8) NOT NULL default ' ',
  "AAABKQQN" varchar(2) NOT NULL default ' ',
  "AAABKQQO" varchar(4) NOT NULL default ' ',
  "AAABKQQP" varchar(4) NOT NULL default ' ',
  "AAABKQQQ" varchar(14) NOT NULL default ' ',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABMFTE" decimal(5,2) NOT NULL default '0.00',
  "AAABKQPW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUJFL","AAABKQPE","AAAAUJFN"),
  KEY "AAAAUJFO" ("AAABKQOV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJFQ" (
  "AAAAUJFX" varchar(3) NOT NULL default '000',
  "AAAAUJFY" varchar(10) NOT NULL default ' ',
  "AAAAUJFZ" varchar(2) NOT NULL default '00',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  "AAABKRPT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUJFX","AAAAUJFY","AAAAUJFZ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJGB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" decimal(21,7) NOT NULL default '0.0000000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABDHLV" varchar(1) NOT NULL default ' ',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABLKMO" varchar(8) NOT NULL default '00000000',
  "AAABJTAJ" varchar(6) NOT NULL default '000000',
  "AAABLKMP" varchar(6) NOT NULL default '000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAUKBN" varchar(1) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAAAUJGN" decimal(21,7) NOT NULL default '0.0000000',
  "AAAAUKFC" varchar(12) NOT NULL default ' ',
  "AAABLQGL" varchar(4) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABIYCS" varchar(2) NOT NULL default ' ',
  "AAABMOIB" varchar(14) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAAAUJGV" varchar(1) NOT NULL default ' ',
  "AAAAUJGW" int(11) NOT NULL default '0',
  "AAAAUJGX" varchar(30) NOT NULL default ' ',
  "AAAAUJGY" varchar(1) NOT NULL default ' ',
  "AAAAUJGZ" varchar(1) NOT NULL default ' ',
  "AAAAUJHA" varchar(1) NOT NULL default ' ',
  "AAAAUJHB" varchar(1) NOT NULL default ' ',
  "AAAAUJHC" varchar(1) NOT NULL default ' ',
  "AAAAUJHD" varchar(1) NOT NULL default ' ',
  "AAAAUJHE" varchar(1) NOT NULL default ' ',
  "AAAAUJHF" varchar(1) NOT NULL default ' ',
  "AAABLETU" varchar(1) NOT NULL default ' ',
  "AAAAUJHH" varchar(1) NOT NULL default ' ',
  "AAAAUKFG" varchar(1) NOT NULL default ' ',
  "AAAAUJHJ" varchar(1) NOT NULL default ' ',
  "AAAAUJHK" varchar(1) NOT NULL default ' ',
  "AAAAUJHL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV"),
  KEY "AAAAUJHO" ("AAAAUKFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJHQ" (
  "AAABLOBS" varchar(18) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABJQWM" varchar(1) NOT NULL default ' ',
  "AAABIYVH" varchar(2) NOT NULL default ' ',
  "AAABLROO" varchar(9) NOT NULL default ' ',
  "AAAAUZXX" varchar(4) NOT NULL default ' ',
  "AAABIZGB" varchar(18) NOT NULL default ' ',
  "AAAAUJHY" varchar(32) NOT NULL default ' ',
  "AAAAUJHZ" decimal(17,3) NOT NULL default '0.000',
  "AAABIYVO" decimal(17,3) NOT NULL default '0.000',
  "AAAAUJIB" varchar(3) NOT NULL default ' ',
  "AAABIYVQ" decimal(17,3) NOT NULL default '0.000',
  "AAAAUJID" varchar(3) NOT NULL default ' ',
  "AAAAUJIE" varchar(18) NOT NULL default ' ',
  "AAABKEVT" varchar(1) NOT NULL default ' ',
  "AAABIYVJ" varchar(3) NOT NULL default ' ',
  "AAAAUJIH" varchar(10) NOT NULL default ' ',
  "AAAAUJII" varchar(3) NOT NULL default ' ',
  "AAAAUJIJ" varchar(18) NOT NULL default ' ',
  "AAAAUJIK" decimal(17,3) NOT NULL default '0.000',
  "AAABLOBU" varchar(2) NOT NULL default ' ',
  "AAAAUJIM" decimal(17,3) NOT NULL default '0.000',
  "AAAAUJIN" varchar(3) NOT NULL default ' ',
  "AAAAUJIO" varchar(2) NOT NULL default ' ',
  "AAAAUJIP" varchar(18) NOT NULL default ' ',
  "AAAAUJIQ" varchar(3) NOT NULL default ' ',
  "AAAAUJIR" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOBS","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJIU" (
  "AAABLROO" varchar(9) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABJQWM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLROO","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJJA" (
  "AAAAUZXX" varchar(4) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABJQWM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUZXX","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJJG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" decimal(21,7) NOT NULL default '0.0000000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABDHLV" varchar(1) NOT NULL default ' ',
  "AAABDKVR" varchar(4) NOT NULL default ' ',
  "AAABMBXY" varchar(4) NOT NULL default ' ',
  "AAABJUQY" varchar(12) NOT NULL default '000000000000',
  "AAABLRFC" varchar(22) NOT NULL default ' ',
  "AAAAUJJP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJJS" (
  "AAABLFYC" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAABKMVW" varchar(4) NOT NULL default ' ',
  "AAABKMVX" varchar(120) NOT NULL default ' ',
  "AAABKMVY" varchar(4) NOT NULL default ' ',
  "AAABKMVZ" varchar(40) NOT NULL default ' ',
  "AAABLJJO" varchar(4) NOT NULL default ' ',
  "AAAAURLV" varchar(40) NOT NULL default ' ',
  "AAABLJIM" varchar(40) NOT NULL default ' ',
  "AAAAURLT" varchar(40) NOT NULL default ' ',
  "AAAAURKX" varchar(1) NOT NULL default ' ',
  "AAAAVCLU" varchar(12) NOT NULL default ' ',
  "AAAAVCKZ" varchar(8) NOT NULL default '00000000',
  "AAAAVCLA" varchar(6) NOT NULL default '000000',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  "AAAAVCLD" varchar(1) NOT NULL default ' ',
  "AAAAURLE" varchar(1) NOT NULL default ' ',
  "AAAAVCLE" varchar(1) NOT NULL default ' ',
  "AAAAVCLF" varchar(1) NOT NULL default ' ',
  "AAABMKGE" varchar(1) NOT NULL default ' ',
  "AAAAVCLH" varchar(1) NOT NULL default ' ',
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAAAVCLJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFYC","AAABLFYD","AAABKMVW","AAABKMVX","AAABKMVY","AAABKMVZ","AAABLJJO"),
  KEY "AAAAUJKY" ("AAAAURLT"),
  KEY "AAAAUJLA" ("AAAAURLV"),
  KEY "AAAAUJLC" ("AAABKMVW","AAABKMVX"),
  KEY "AAAAUJLF" ("AAABILWI"),
  KEY "AAAAUJLH" ("AAAAVCLU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJLJ" (
  "AAABLFYC" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAABKMVW" varchar(4) NOT NULL default ' ',
  "AAABKMVX" varchar(120) NOT NULL default ' ',
  "AAABKMVY" varchar(4) NOT NULL default ' ',
  "AAABKMVZ" varchar(40) NOT NULL default ' ',
  "AAABLJJO" varchar(4) NOT NULL default ' ',
  "AAAAURLV" varchar(40) NOT NULL default ' ',
  "AAABLJIM" varchar(40) NOT NULL default ' ',
  "AAAAURLT" varchar(40) NOT NULL default ' ',
  "AAAAURKX" varchar(1) NOT NULL default ' ',
  "AAAAVCLU" varchar(12) NOT NULL default ' ',
  "AAAAVCKZ" varchar(8) NOT NULL default '00000000',
  "AAAAVCLA" varchar(6) NOT NULL default '000000',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  "AAAAVCLD" varchar(1) NOT NULL default ' ',
  "AAAAURLE" varchar(1) NOT NULL default ' ',
  "AAAAVCLE" varchar(1) NOT NULL default ' ',
  "AAAAVCLF" varchar(1) NOT NULL default ' ',
  "AAABMKGE" varchar(1) NOT NULL default ' ',
  "AAAAVCLH" varchar(1) NOT NULL default ' ',
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAAAVCLJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFYC","AAABLFYD","AAABKMVW","AAABKMVX","AAABKMVY","AAABKMVZ","AAABLJJO"),
  KEY "AAAAUJMP" ("AAABKMVW","AAABKMVX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJMS" (
  "AAABKMVW" varchar(4) NOT NULL default ' ',
  "AAABKMVX" varchar(120) NOT NULL default ' ',
  "AAABJEXI" varchar(20) NOT NULL default ' ',
  "AAAAUJMW" varchar(20) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAAVCLU" varchar(12) NOT NULL default ' ',
  "AAAAVCKZ" varchar(8) NOT NULL default '00000000',
  "AAAAVCLA" varchar(6) NOT NULL default '000000',
  "AAABIFVP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABKMVW","AAABKMVX","AAABJEXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJNF" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLFYC" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAABKMVW" varchar(4) NOT NULL default ' ',
  "AAABKMVX" varchar(120) NOT NULL default ' ',
  "AAABKMVY" varchar(4) NOT NULL default ' ',
  "AAABKMVZ" varchar(40) NOT NULL default ' ',
  "AAABLJJO" varchar(4) NOT NULL default ' ',
  "AAABKMWB" int(11) NOT NULL default '0',
  "AAABLTXI" varchar(6) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLFYC","AAABLFYD","AAABKMVW","AAABKMVX","AAABKMVY","AAABKMVZ","AAABLJJO","AAABKMWB"),
  KEY "AAAAUJOB" ("AAABLFYC","AAABLFYD","AAABKMVW","AAABKMVX"),
  KEY "AAAAUJOG" ("AAABKMVW","AAABKMVX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJOJ" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLFYC" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAABKMVW" varchar(4) NOT NULL default ' ',
  "AAABKMVX" varchar(120) NOT NULL default ' ',
  "AAABKMVY" varchar(4) NOT NULL default ' ',
  "AAABKMVZ" varchar(40) NOT NULL default ' ',
  "AAABLJJO" varchar(4) NOT NULL default ' ',
  "AAABKMWB" int(11) NOT NULL default '0',
  "AAABLTXI" varchar(6) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLFYC","AAABLFYD","AAABKMVW","AAABKMVX","AAABKMVY","AAABKMVZ","AAABLJJO","AAABKMWB"),
  KEY "AAAAUJPF" ("AAABLFYC","AAABLFYD","AAABKMVW","AAABKMVX"),
  KEY "AAAAUJPK" ("AAABKMVW","AAABKMVX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJPN" (
  "AAABKMVW" varchar(4) NOT NULL default ' ',
  "AAABKMVX" varchar(120) NOT NULL default ' ',
  "AAAAVCLU" varchar(12) NOT NULL default ' ',
  "AAAAVCKZ" varchar(8) NOT NULL default '00000000',
  "AAAAVCLA" varchar(6) NOT NULL default '000000',
  "AAABILWI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABKMVW","AAABKMVX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJPW" (
  "AAABKMVW" varchar(4) NOT NULL default ' ',
  "AAABKMVX" varchar(120) NOT NULL default ' ',
  "AAABEDIF" varchar(12) NOT NULL default ' ',
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAAAVCKZ" varchar(8) NOT NULL default '00000000',
  "AAAAVCLA" varchar(6) NOT NULL default '000000',
  "AAAAVCLH" varchar(1) NOT NULL default ' ',
  "AAAAUJQE" varchar(1) NOT NULL default ' ',
  "AAABLNXD" varchar(1) NOT NULL default ' ',
  "AAABLCCC" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABEBYS" varchar(12) NOT NULL default ' ',
  "AAABKWET" varchar(8) NOT NULL default '00000000',
  "AAABGCZV" varchar(6) NOT NULL default '000000',
  "AAABIFVP" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABKMVW","AAABKMVX","AAABEDIF","AAABILWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" decimal(21,7) NOT NULL default '0.0000000',
  "AAABMJWH" varchar(35) NOT NULL default ' ',
  "AAABMKXW" varchar(35) NOT NULL default ' ',
  "AAABJZCI" int(11) NOT NULL default '0',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABDHLV" varchar(1) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABLGYD" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(200) NOT NULL default ' ',
  "AAABMNNA" varchar(200) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV","AAABMJWH","AAABMKXW","AAABJZCI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJRI" (
  "AAABMREL" varchar(10) NOT NULL default ' ',
  "AAABLUSR" varchar(110) NOT NULL default ' ',
  "AAABLUSO" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABMREL","AAABLUSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJRP" (
  "AAABHMLH" varchar(20) NOT NULL default ' ',
  "AAAAVFSH" varchar(20) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABLTIZ" decimal(15,0) NOT NULL default '0',
  "AAABHLQU" varchar(1) NOT NULL default ' ',
  "AAAAUJRV" varchar(1) NOT NULL default ' ',
  "AAAAUJRW" varchar(1) NOT NULL default ' ',
  "AAAAVASF" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABHMLH","AAAAVFSH","AAABLZKF","AAABLTIZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJSC" (
  "AAABMLPV" varchar(20) NOT NULL default ' ',
  "AAAAVQAB" varchar(60) NOT NULL default ' ',
  "AAAAVQAC" varchar(120) NOT NULL default ' ',
  "AAABGRPQ" varchar(60) NOT NULL default ' ',
  "AAABEDJP" varchar(60) NOT NULL default ' ',
  "AAABGRPS" varchar(60) NOT NULL default ' ',
  "AAABLZLD" varchar(60) NOT NULL default ' ',
  "AAAAVQAE" varchar(60) NOT NULL default ' ',
  "AAAAVQAF" varchar(120) NOT NULL default ' ',
  "AAABLKZT" int(11) NOT NULL default '0',
  "AAAAUJUA" varchar(60) NOT NULL default ' ',
  "AAAAUJUB" varchar(120) NOT NULL default ' ',
  "AAAAUJUC" varchar(60) NOT NULL default ' ',
  "AAAAUJUD" varchar(120) NOT NULL default ' ',
  "AAABLUZL" varchar(40) NOT NULL default ' ',
  "AAABLECQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJSU" (
  "AAABMBXY" VARBINARY(255) NOT NULL,
  "AAAAUJWU" VARBINARY(255) NOT NULL,
  "AAAAUJWV" tinyint(3) unsigned NOT NULL default '0',
  "AAABLKZT" int(11) NOT NULL default '0',
  "AAABLTLR" tinyint(3) unsigned NOT NULL default '0',
  "AAAAUJUS" tinyint(3) unsigned NOT NULL default '0',
  "AAABCZFZ" varchar(20) NOT NULL default ' ',
  "AAABMNYT" varchar(180) NOT NULL default ' ',
  "AAAAUJUA" varchar(60) NOT NULL default ' ',
  "AAAAUJUB" varchar(120) NOT NULL default ' ',
  "AAAAUJUC" varchar(60) NOT NULL default ' ',
  "AAAAUJUD" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY"(255),"AAAAUJWU"(255),"AAAAUJWV","AAABLKZT","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJTM" (
  "AAABMBXY" VARBINARY(255) NOT NULL,
  "AAAAUJWU" VARBINARY(255) NOT NULL,
  "AAAAUJWV" int(11) NOT NULL default '0',
  "AAABLKZT" int(11) NOT NULL default '0',
  "AAABLTLR" int(11) NOT NULL default '0',
  "AAAAUJVP" int(11) NOT NULL default '0',
  "AAABCZFZ" varchar(20) NOT NULL default ' ',
  "AAAAUJTU" varchar(60) NOT NULL default ' ',
  "AAAAUJTV" VARBINARY(255),
  "AAAAUJTW" int(11) NOT NULL default '0',
  "AAABMNYT" varchar(180) NOT NULL default ' ',
  "AAAAUJVU" varchar(60) NOT NULL default ' ',
  "AAAAUJVV" varchar(120) NOT NULL default ' ',
  "AAAAUJUA" varchar(60) NOT NULL default ' ',
  "AAAAUJUB" varchar(120) NOT NULL default ' ',
  "AAAAUJUC" varchar(60) NOT NULL default ' ',
  "AAAAUJUD" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY"(255),"AAAAUJWU"(255),"AAAAUJWV","AAABLKZT","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJUJ" (
  "AAAAVQAB" varchar(60) NOT NULL default ' ',
  "AAAAVQAC" varchar(120) NOT NULL default ' ',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAABEDJP" varchar(60) NOT NULL default ' ',
  "AAABLZLD" varchar(60) NOT NULL default ' ',
  "AAAAVQAE" varchar(60) NOT NULL default ' ',
  "AAAAVQAF" varchar(120) NOT NULL default ' ',
  "AAABMBXY" VARBINARY(255),
  "AAAAUJUS" tinyint(3) unsigned NOT NULL default '0',
  "AAAAUJVQ" VARBINARY(255),
  "AAAAUJVR" VARBINARY(255),
  "AAAAUJVS" tinyint(3) unsigned NOT NULL default '0',
  "AAAAUJVT" varchar(64) NOT NULL default ' ',
  "AAAAUJVU" varchar(60) NOT NULL default ' ',
  "AAAAUJVV" varchar(120) NOT NULL default ' ',
  "AAABMLPO" tinyint(3) unsigned NOT NULL default '0',
  "AAABFNUR" VARBINARY(255),
  PRIMARY KEY ("AAAAVQAB","AAAAVQAC","AAABLTVR"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJVE" (
  "AAAAVPIQ" varchar(60) NOT NULL default ' ',
  "AAAAVPIR" varchar(120) NOT NULL default ' ',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAABGRPQ" varchar(60) NOT NULL default ' ',
  "AAABGRPR" varchar(60) NOT NULL default ' ',
  "AAABGRPS" varchar(60) NOT NULL default ' ',
  "AAABGRPT" varchar(60) NOT NULL default ' ',
  "AAAAVPIU" varchar(60) NOT NULL default ' ',
  "AAAAVPIV" varchar(120) NOT NULL default ' ',
  "AAABMBXY" VARBINARY(255),
  "AAAAUJVP" int(11) NOT NULL default '0',
  "AAAAUJVQ" VARBINARY(255),
  "AAAAUJVR" VARBINARY(255),
  "AAAAUJVS" int(11) NOT NULL default '0',
  "AAAAUJVT" varchar(64) NOT NULL default ' ',
  "AAAAUJVU" varchar(60) NOT NULL default ' ',
  "AAAAUJVV" varchar(120) NOT NULL default ' ',
  "AAABMLPO" int(11) NOT NULL default '0',
  "AAABFNUR" VARBINARY(255),
  PRIMARY KEY ("AAAAVPIQ","AAAAVPIR","AAABLTVR"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJWB" (
  "AAABMBXY" VARBINARY(255) NOT NULL,
  "AAAAUJWU" VARBINARY(255) NOT NULL,
  "AAAAUJWV" int(11) NOT NULL default '0',
  "AAABDNQB" varchar(1) NOT NULL default ' ',
  "AAAAUJWQ" int(11) NOT NULL default '0',
  "AAAAUJWH" int(11) NOT NULL default '0',
  "AAAAUJWT" varchar(60) NOT NULL default ' ',
  "AAAAUJWS" varchar(120) NOT NULL default ' ',
  "AAAAUJWK" varchar(60) NOT NULL default ' ',
  "AAAAUJWL" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY"(255),"AAAAUJWU"(255),"AAAAUJWV","AAABDNQB","AAAAUJWQ"),
  KEY "AAAAUJWR" ("AAAAUJWS","AAAAUJWT","AAAAUJWU"(255),"AAAAUJWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJWW" (
  "AAABCZFZ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABCZFZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJWZ" (
  "AAABCZFZ" varchar(20) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" longtext,
  PRIMARY KEY ("AAABCZFZ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJXF" (
  "AAAAUJXJ" varchar(32) NOT NULL default ' ',
  "AAAAUJXH" decimal(15,0) NOT NULL default '0',
  "AAAAUJXI" longblob,
  PRIMARY KEY ("AAAAUJXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJXK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" decimal(21,7) NOT NULL default '0.0000000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABDHLV" varchar(1) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAAAUJXQ" varchar(3) NOT NULL default '000',
  "AAAAUJXR" varchar(3) NOT NULL default '000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABLHZO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJXW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" decimal(21,7) NOT NULL default '0.0000000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABDHLV" varchar(1) NOT NULL default ' ',
  "AAABLJNI" varchar(100) NOT NULL default ' ',
  "AAABHTWG" varchar(30) NOT NULL default ' ',
  "AAAAUJYD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJYG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABMRHK" varchar(12) NOT NULL default ' ',
  "AAAAUKFF" varchar(10) NOT NULL default '0000000000',
  "AAAAUJYL" varchar(5) NOT NULL default '00000',
  "AAAAUJYM" int(11) NOT NULL default '0',
  "AAABHMPC" int(11) NOT NULL default '0',
  "AAAAUJYO" int(11) NOT NULL default '0',
  "AAAAUJYP" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABMRHK","AAAAUKFF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJYU" (
  "AAAAVCGB" varchar(4) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABJQWM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAVCGB","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" decimal(21,7) NOT NULL default '0.0000000',
  "AAAAUJZD" decimal(21,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJZG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" decimal(21,7) NOT NULL default '0.0000000',
  "AAABMJWH" varchar(35) NOT NULL default ' ',
  "AAABMKXW" varchar(35) NOT NULL default ' ',
  "AAABJZCI" int(11) NOT NULL default '0',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABDHLV" varchar(1) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABLGYD" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(200) NOT NULL default ' ',
  "AAABMNNA" varchar(200) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV","AAABMJWH","AAABMKXW","AAABJZCI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUJZY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" decimal(21,7) NOT NULL default '0.0000000',
  "AAABJZCI" int(11) NOT NULL default '0',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABDHLV" varchar(1) NOT NULL default ' ',
  "AAABKWSB" int(11) NOT NULL default '0',
  "AAAAUKAF" varchar(68) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV","AAABJZCI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKAJ" (
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABMRHK" varchar(12) NOT NULL default ' ',
  "AAABLFYJ" varchar(12) NOT NULL default ' ',
  "AAAAUKAN" varchar(12) NOT NULL default ' ',
  "AAABIKDN" varchar(40) NOT NULL default ' ',
  "AAABDNPH" varchar(1) NOT NULL default ' ',
  "AAAAUKAQ" varchar(40) NOT NULL default ' ',
  "AAABDNOU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUVS","AAABMRHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKAU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" decimal(21,7) NOT NULL default '0.0000000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABDHLV" varchar(1) NOT NULL default ' ',
  "AAAAUKBN" varchar(1) NOT NULL default ' ',
  "AAABIZZP" varchar(1) NOT NULL default ' ',
  "AAAAUKBP" varchar(8) NOT NULL default '00000000',
  "AAAAUKBQ" varchar(6) NOT NULL default '000000',
  "AAAAUKBR" varchar(12) NOT NULL default ' ',
  "AAABBDGX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKBH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" decimal(21,7) NOT NULL default '0.0000000',
  "AAABMLZS" varchar(4) NOT NULL default '0000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABDHLV" varchar(1) NOT NULL default ' ',
  "AAAAUKBN" varchar(1) NOT NULL default ' ',
  "AAABIZZP" varchar(1) NOT NULL default ' ',
  "AAAAUKBP" varchar(8) NOT NULL default '00000000',
  "AAAAUKBQ" varchar(6) NOT NULL default '000000',
  "AAAAUKBR" varchar(12) NOT NULL default ' ',
  "AAABBDGX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKBW" (
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABMKXW" varchar(35) NOT NULL default ' ',
  "AAABLRRL" varchar(35) NOT NULL default ' ',
  "AAAAUKCA" varchar(1) NOT NULL default '0',
  "AAAAUKCB" varchar(1) NOT NULL default '0',
  "AAAAUKCC" varchar(1) NOT NULL default '0',
  "AAAAUKCD" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABLUER","AAABMKXW","AAABLRRL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKCH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVE" varchar(3) NOT NULL default '000',
  "AAAAUKCK" varchar(255) NOT NULL default ' ',
  "AAABHMNR" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKCO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHAUB" varchar(15) NOT NULL default ' ',
  "AAAAUKCR" varchar(255) NOT NULL default ' ',
  "AAABLLLP" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHAUB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKCV" (
  "AAABIJSB" varchar(3) NOT NULL default '000',
  "AAABKNBI" varchar(220) NOT NULL default ' ',
  PRIMARY KEY ("AAABIJSB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKCZ" (
  "AAAAUKDD" varchar(1) NOT NULL default ' ',
  "AAAAUKDE" varchar(3) NOT NULL default '000',
  "AAAAUKDC" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAAAUKDD","AAAAUKDE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAAAUKDT" varchar(10) NOT NULL default ' ',
  "AAAAUKDJ" varchar(50) NOT NULL default ' ',
  "AAAAUKDK" varchar(1) NOT NULL default ' ',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKDO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUKDT" varchar(10) NOT NULL default ' ',
  "AAABETRK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUKDT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKDU" (
  "AAABLUVE" varchar(3) NOT NULL default '000',
  "AAAAURNE" varchar(40) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  "AAABLTQF" varchar(40) NOT NULL default ' ',
  "AAAAUKDZ" varchar(40) NOT NULL default ' ',
  "AAAAWRXN" varchar(3) NOT NULL default '000',
  "AAABEZNY" varchar(40) NOT NULL default ' ',
  "AAABMREF" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUVE"),
  KEY "AAAAUKEE" ("AAAAURNE","AAABMLPV","AAABLTQF"),
  KEY "AAAAUKEI" ("AAABMLPV","AAABLTQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKEL" (
  "AAABLTXI" varchar(3) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABJQWM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKER" (
  "AAABJFIP" varchar(3) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABJQWM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJFIP","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKEX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" decimal(21,7) NOT NULL default '0.0000000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABDHLV" varchar(1) NOT NULL default ' ',
  "AAAAUKFC" varchar(12) NOT NULL default ' ',
  "AAABLTQH" varchar(12) NOT NULL default '000000000000',
  "AAAAUKFE" varchar(12) NOT NULL default '000000000000',
  "AAAAUKFF" varchar(10) NOT NULL default '0000000000',
  "AAAAUKFG" varchar(1) NOT NULL default ' ',
  "AAABGWYP" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKFK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHMRI" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHMRI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKFP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHMRI" varchar(10) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHMRI","AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKFW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHMRI" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHMRI","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKGE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEKS" varchar(12) NOT NULL default ' ',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAABLTQF" varchar(10) NOT NULL default ' ',
  "AAABHHCG" varchar(10) NOT NULL default ' ',
  "AAAAUKGK" varchar(1) NOT NULL default ' ',
  "AAABJTNV" varchar(70) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAUKGO" int(11) NOT NULL default '0',
  "AAABMRCK" varchar(132) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLEKS","AAABLTVR"(255)),
  KEY "AAAAUKGU" ("AAABLEKS","AAABLTQF","AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKGY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(30) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKHJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABDOEA" varchar(22) NOT NULL default ' ',
  "AAABDOED" varchar(22) NOT NULL default ' ',
  "AAABJJLO" varchar(22) NOT NULL default ' ',
  "AAABJJLF" varchar(4) NOT NULL default ' ',
  "AAABDODY" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABDOEA","AAABDOED"),
  KEY "AAAAUKHT" ("AAABJJLO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKHV" (
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAAAUKIO" varchar(32) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFFA" varchar(1) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default ' ',
  "AAAAUKIV" varchar(1) NOT NULL default ' ',
  "AAAAZVQQ" varchar(200) NOT NULL default ' ',
  "AAAAUKIF" varchar(200) NOT NULL default ' ',
  "AAAAUKIG" varchar(200) NOT NULL default ' ',
  "AAAAUKIH" varchar(200) NOT NULL default ' ',
  "AAAAUKII" varchar(200) NOT NULL default ' ',
  "AAAAUKIJ" varchar(200) NOT NULL default ' ',
  "AAAAUKIK" varchar(200) NOT NULL default ' ',
  "AAAAUKIL" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRFQ","AAABMLES","AAAAUKIO","AAABMDEN","AAABMRIL","AAAAYFFA","AAABLUVE"),
  KEY "AAAAUKIT" ("AAABLUVE","AAAAUKIV","AAABMRFQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKIX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUKJO" varchar(30) NOT NULL default ' ',
  "AAABBRWV" varchar(1) NOT NULL default ' ',
  "AAABLUVE" varchar(4) NOT NULL default ' ',
  "AAABAMWO" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAUKJO","AAABBRWV","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKJH" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAUKJO" varchar(30) NOT NULL default ' ',
  "AAABBRWV" varchar(1) NOT NULL default ' ',
  "AAABMNLA" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAAUKJO","AAABBRWV","AAABMNLA","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKJS" (
  "AAABMMVU" varchar(1) NOT NULL default ' ',
  "AAABILZZ" varchar(8) NOT NULL default ' ',
  "AAAAUKKQ" varchar(32) NOT NULL default ' ',
  "AAABILZL" varchar(32) NOT NULL default ' ',
  "AAABILZK" varchar(255) NOT NULL default ' ',
  "AAAAUKJY" varchar(1) NOT NULL default ' ',
  "AAAAUKJZ" varchar(1) NOT NULL default ' ',
  "AAAAUKKA" varchar(1) NOT NULL default ' ',
  "AAAAUKKB" varchar(1) NOT NULL default ' ',
  "AAAAUKKC" varchar(1) NOT NULL default ' ',
  "AAABIMEG" varchar(12) NOT NULL default ' ',
  "AAABLMKH" varchar(8) NOT NULL default '00000000',
  "AAABLIRO" varchar(6) NOT NULL default '000000',
  "AAABIMEJ" varchar(12) NOT NULL default ' ',
  "AAABIMEK" varchar(8) NOT NULL default '00000000',
  "AAABIMEL" varchar(6) NOT NULL default '000000',
  "AAABILZS" tinyint(3) unsigned NOT NULL default '0',
  "AAABILZT" VARBINARY(255),
  "AAABILZU" varchar(8) NOT NULL default ' ',
  "AAABILZV" smallint(6) NOT NULL default '0',
  "AAABILZW" blob,
  PRIMARY KEY ("AAABMMVU","AAABILZZ","AAAAUKKQ","AAABILZL"),
  KEY "AAAAUKKS" ("AAABILZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKKU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTF" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYWJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTF" varchar(8) NOT NULL default ' ',
  "AAAAUKLC" varchar(8) NOT NULL default '00000000',
  "AAABLXVN" varchar(12) NOT NULL default ' ',
  "AAAAUKLE" varchar(1) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAAAUKLH" varchar(40) NOT NULL default ' ',
  "AAAAUKLI" varchar(1) NOT NULL default ' ',
  "AAABKRNP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKLM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJTDU" varchar(3) NOT NULL default ' ',
  "AAABJTDV" varchar(4) NOT NULL default '0000',
  "AAAAVNUK" varchar(8) NOT NULL default '00000000',
  "AAAAVDBE" varchar(8) NOT NULL default '00000000',
  "AAAAVDBF" varchar(8) NOT NULL default '00000000',
  "AAAAUKME" varchar(4) NOT NULL default '0000',
  "AAAAUKLU" varchar(1) NOT NULL default ' ',
  "AAABKGMO" decimal(15,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAAAVDAZ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABJTDU","AAABJTDV","AAAAVNUK","AAAAVDBE","AAAAVDBF","AAAAUKME")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKMF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(30) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKMQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJTNV" varchar(70) NOT NULL default ' ',
  "AAABLLIO" varchar(10) NOT NULL default ' ',
  "AAABLCOV" varchar(3) NOT NULL default ' ',
  "AAABLCOW" varchar(2) NOT NULL default ' ',
  "AAABLCOX" varchar(12) NOT NULL default ' ',
  "AAAAUNDI" varchar(3) NOT NULL default ' ',
  "AAAAUNDJ" varchar(2) NOT NULL default ' ',
  "AAAAUNDK" varchar(12) NOT NULL default ' ',
  "AAAAUMEX" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJTNV","AAABLLIO","AAABLCOV","AAABLCOW","AAABLCOX"),
  KEY "AAAAUKNH" ("AAABLCOX","AAABLCOW","AAABLCOV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKNL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(30) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKNW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUMYG" varchar(3) NOT NULL default ' ',
  "AAAAUMYH" varchar(2) NOT NULL default ' ',
  "AAAAUMYI" varchar(12) NOT NULL default ' ',
  "AAABFDJZ" varchar(1) NOT NULL default ' ',
  "AAAAUKOW" varchar(10) NOT NULL default ' ',
  "AAAAUKOX" varchar(3) NOT NULL default '000',
  "AAAAUKOY" varchar(3) NOT NULL default ' ',
  "AAAAUKOZ" varchar(2) NOT NULL default ' ',
  "AAAAUKPA" varchar(12) NOT NULL default ' ',
  "AAABLCOV" varchar(3) NOT NULL default ' ',
  "AAABLCOW" varchar(2) NOT NULL default ' ',
  "AAABLCOX" varchar(12) NOT NULL default ' ',
  "AAABHUAL" varchar(3) NOT NULL default ' ',
  "AAAAUMZD" varchar(2) NOT NULL default ' ',
  "AAAAUMZE" varchar(12) NOT NULL default ' ',
  "AAAAUNEG" varchar(3) NOT NULL default ' ',
  "AAAAUNEH" varchar(2) NOT NULL default ' ',
  "AAABETFA" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUMYG","AAAAUMYH","AAAAUMYI"),
  KEY "AAAAUKOU" ("AAABFDJZ","AAAAUKOW","AAAAUKOX","AAAAUKOY","AAAAUKOZ","AAAAUKPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKPB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCOV" varchar(3) NOT NULL default ' ',
  "AAABLCOW" varchar(2) NOT NULL default ' ',
  "AAABLCOX" varchar(12) NOT NULL default ' ',
  "AAAAUKPG" varchar(2) NOT NULL default ' ',
  "AAAAUKPH" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCOV","AAABLCOW","AAABLCOX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKPM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUMYG" varchar(3) NOT NULL default ' ',
  "AAAAUMYH" varchar(2) NOT NULL default ' ',
  "AAAAUMYI" varchar(12) NOT NULL default ' ',
  "AAABFDJZ" varchar(1) NOT NULL default ' ',
  "AAAAUKPS" varchar(241) NOT NULL default ' ',
  "AAAAUKPT" varchar(241) NOT NULL default ' ',
  "AAAAUKPU" varchar(241) NOT NULL default ' ',
  "AAAAUKPV" varchar(241) NOT NULL default ' ',
  "AAABLCOV" varchar(3) NOT NULL default ' ',
  "AAABLCOW" varchar(2) NOT NULL default ' ',
  "AAABLCOX" varchar(12) NOT NULL default ' ',
  "AAABHUAL" varchar(3) NOT NULL default ' ',
  "AAAAUMZD" varchar(2) NOT NULL default ' ',
  "AAAAUMZE" varchar(12) NOT NULL default ' ',
  "AAAAUNEG" varchar(3) NOT NULL default ' ',
  "AAAAUNEH" varchar(2) NOT NULL default ' ',
  "AAABETFA" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUMYG","AAAAUMYH","AAAAUMYI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKQJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUKQR" varchar(241) NOT NULL default ' ',
  "AAABMLPO" VARBINARY(255) NOT NULL,
  "AAAAUMYG" varchar(3) NOT NULL default ' ',
  "AAAAUMYH" varchar(2) NOT NULL default ' ',
  "AAAAUMYI" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUKQR","AAABMLPO"(255)),
  UNIQUE KEY "AAAAUKQT" ("AAABMRIL","AAAAUMYG","AAAAUMYH","AAAAUMYI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUNSX" varchar(3) NOT NULL default ' ',
  "AAAAUNSY" varchar(2) NOT NULL default ' ',
  "AAAAUNSZ" varchar(12) NOT NULL default ' ',
  "AAABHBVV" varchar(12) NOT NULL default ' ',
  "AAABGBXV" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUNSX","AAAAUNSY","AAAAUNSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKRJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAULVC" varchar(3) NOT NULL default ' ',
  "AAAAULVB" varchar(2) NOT NULL default ' ',
  "AAAAULVA" varchar(12) NOT NULL default ' ',
  "AAAAUKTA" varchar(3) NOT NULL default ' ',
  "AAAAUKSZ" varchar(2) NOT NULL default ' ',
  "AAABAWCT" varchar(12) NOT NULL default ' ',
  "AAAAUKRR" VARBINARY(255),
  "AAAAUNPY" varchar(8) NOT NULL default ' ',
  "AAAAUMFH" varchar(3) NOT NULL default ' ',
  "AAAAUMFI" varchar(30) NOT NULL default ' ',
  "AAAAVYWV" varchar(1) NOT NULL default ' ',
  "AAAAVYWW" varchar(1) NOT NULL default ' ',
  "AAAAVYWX" varchar(1) NOT NULL default ' ',
  "AAAAVYWY" varchar(1) NOT NULL default ' ',
  "AAAAVYWZ" varchar(1) NOT NULL default ' ',
  "AAAAUMXM" varchar(1) NOT NULL default ' ',
  "AAAAUKSB" VARBINARY(255),
  "AAAAVYXA" varchar(1) NOT NULL default ' ',
  "AAAAUMFV" VARBINARY(255),
  "AAAAUKSE" VARBINARY(255),
  "AAAAVYXC" varchar(1) NOT NULL default ' ',
  "AAAAVYXD" varchar(1) NOT NULL default ' ',
  "AAAAWUOJ" varchar(3) NOT NULL default ' ',
  "AAAAUMGJ" varchar(2) NOT NULL default ' ',
  "AAABCOLW" varchar(12) NOT NULL default ' ',
  "AAAAUNCN" varchar(3) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAAAVIMQ" varchar(1) NOT NULL default ' ',
  "AAAAULYE" varchar(1) NOT NULL default ' ',
  "AAAAULYF" varchar(1) NOT NULL default ' ',
  "AAAAULYC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAULVC","AAAAULVB","AAAAULVA","AAAAUKTA","AAAAUKSZ","AAABAWCT"),
  KEY "AAAAUKSX" ("AAABAWCT","AAAAUKSZ","AAAAUKTA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKTB" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKYTL" varchar(10) NOT NULL default '0000000000',
  "AAAAUKTE" varchar(255) NOT NULL default ' ',
  "AAAAUKTF" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABKYTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKTI" (
  "AAABHKDA" varchar(63) NOT NULL default ' ',
  "AAAAUKTK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABHKDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKTM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGFCP" varchar(255) NOT NULL default ' ',
  "AAABLUVJ" varchar(255) NOT NULL default ' ',
  "AAABLEIH" int(11) NOT NULL default '0',
  "AAAAUKTR" varchar(255) NOT NULL default ' ',
  "AAAAUKTS" varchar(255) NOT NULL default ' ',
  "AAAAUKTT" varchar(255) NOT NULL default ' ',
  "AAAAUKTU" int(11) NOT NULL default '0',
  "AAAAUKTV" int(11) NOT NULL default '0',
  "AAAAUKTW" varchar(1) NOT NULL default ' ',
  "AAAAUKTX" varchar(1) NOT NULL default ' ',
  "AAAAUKTY" varchar(1) NOT NULL default ' ',
  "AAAAUKTZ" varchar(1) NOT NULL default ' ',
  "AAAAUKUA" varchar(1) NOT NULL default ' ',
  "AAAAUKUB" varchar(1) NOT NULL default ' ',
  "AAAAUKUC" varchar(1) NOT NULL default ' ',
  "AAAAUKUD" varchar(1) NOT NULL default ' ',
  "AAAAUKUE" varchar(1) NOT NULL default ' ',
  "AAAAUKUF" int(11) NOT NULL default '0',
  "AAAAUKUG" int(11) NOT NULL default '0',
  "AAAAUKUH" varchar(1) NOT NULL default ' ',
  "AAAAUKUI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKUK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHWXN" varchar(16) NOT NULL default ' ',
  "AAAAUKUN" varchar(10) NOT NULL default '0000000000',
  "AAAAUKUO" varchar(10) NOT NULL default '0000000000',
  "AAAAUKUP" varchar(10) NOT NULL default '0000000000',
  "AAAAUKUQ" varchar(10) NOT NULL default '0000000000',
  "AAAAUKUR" varchar(10) NOT NULL default '0000000000',
  "AAABGFBX" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKUU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMCU" varchar(30) NOT NULL default ' ',
  "AAABMNMP" varchar(1) NOT NULL default ' ',
  "AAAAUKUY" varchar(20) NOT NULL default ' ',
  "AAAAUKUZ" varchar(20) NOT NULL default ' ',
  "AAAAWART" int(11) NOT NULL default '0',
  "AAABHMPB" varchar(8) NOT NULL default '00000000',
  "AAABHIYF" varchar(6) NOT NULL default '000000',
  "AAAAUKVD" varchar(1) NOT NULL default ' ',
  "AAAAUKVE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKVH" (
  "AAABLZLD" varchar(12) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default ' ',
  "AAAAUNTG" varchar(1) NOT NULL default ' ',
  "AAABIZCH" varchar(8) NOT NULL default '00000000',
  "AAABGBTN" varchar(6) NOT NULL default '000000',
  "AAAAUNTJ" varchar(8) NOT NULL default '00000000',
  "AAABGFNS" varchar(6) NOT NULL default '000000',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABJTNV" varchar(70) NOT NULL default ' ',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAAAUNTV" varchar(1) NOT NULL default ' ',
  "AAAAUNTW" varchar(1) NOT NULL default ' ',
  "AAAAUNTX" varchar(1) NOT NULL default ' ',
  "AAABMECK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLZLD","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKWF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCOV" varchar(3) NOT NULL default ' ',
  "AAABLCOW" varchar(2) NOT NULL default ' ',
  "AAABLCOX" varchar(12) NOT NULL default ' ',
  "AAABDIKT" varchar(1) NOT NULL default ' ',
  "AAABMLPO" VARBINARY(255) NOT NULL,
  "AAABECVI" varchar(3) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAAAYKRL" varchar(10) NOT NULL default ' ',
  "AAAAUNPY" varchar(6) NOT NULL default ' ',
  "AAAAUKWQ" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(12) NOT NULL default ' ',
  "AAAAUMYG" varchar(3) NOT NULL default ' ',
  "AAAAUMYH" varchar(2) NOT NULL default ' ',
  "AAAAUMYI" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCOV","AAABLCOW","AAABLCOX","AAABDIKT","AAABMLPO"(255)),
  KEY "AAAAUKXB" ("AAAAYKRL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKXD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUNEG" varchar(3) NOT NULL default ' ',
  "AAAAUNEH" varchar(2) NOT NULL default ' ',
  "AAABETFA" varchar(12) NOT NULL default ' ',
  "AAAAUKYU" varchar(8) NOT NULL default '00000000',
  "AAAAUKYV" varchar(6) NOT NULL default '000000',
  "AAAAUNPY" varchar(6) NOT NULL default ' ',
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAAAUMYG" varchar(3) NOT NULL default ' ',
  "AAAAUMYH" varchar(2) NOT NULL default ' ',
  "AAAAUMYI" varchar(12) NOT NULL default ' ',
  "AAAAUKYX" varchar(8) NOT NULL default '00000000',
  "AAAAUKYY" varchar(6) NOT NULL default '000000',
  "AAAAVIMQ" varchar(1) NOT NULL default ' ',
  "AAAAULYE" varchar(1) NOT NULL default ' ',
  "AAAAUKXU" varchar(1) NOT NULL default ' ',
  "AAAAUKXV" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(12) NOT NULL default ' ',
  "AAAAULYC" varchar(1) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  "AAABHMRI" varchar(6) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUNEG","AAAAUNEH","AAABETFA"),
  KEY "AAAAUKYL" ("AAAAUMYG","AAAAUMYH","AAAAUMYI","AAABLXWK","AAAAUNPY","AAABMRCU"),
  KEY "AAAAUKYS" ("AAABMRIL","AAAAUKYU","AAAAUKYV"),
  KEY "AAAAUKYW" ("AAAAUKYX","AAAAUKYY","AAABMLDY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUKZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUNDI" varchar(3) NOT NULL default ' ',
  "AAAAUNDJ" varchar(2) NOT NULL default ' ',
  "AAAAUNDK" varchar(12) NOT NULL default ' ',
  "AAAAUNSX" varchar(3) NOT NULL default ' ',
  "AAAAUNSY" varchar(2) NOT NULL default ' ',
  "AAAAUNSZ" varchar(12) NOT NULL default ' ',
  "AAAAULAM" varchar(1) NOT NULL default ' ',
  "AAABJHGJ" varchar(1) NOT NULL default ' ',
  "AAABJHGK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUNDI","AAAAUNDJ","AAAAUNDK","AAAAUNSX","AAAAUNSY","AAAAUNSZ"),
  KEY "AAAAUKZS" ("AAAAUNSZ","AAAAUNSY","AAAAUNSX","AAABJHGK","AAAAUNDI","AAAAUNDJ","AAAAUNDK","AAABMRIL","AAAAULAM","AAABJHGJ"),
  KEY "AAAAULAD" ("AAAAUNDK","AAAAUNDJ","AAAAUNDI","AAABJHGJ","AAABMRIL","AAAAUNSX","AAAAUNSY","AAAAUNSZ","AAAAULAM","AAABJHGK"),
  KEY "AAAAULAO" ("AAAAUNDK","AAAAUNDJ","AAAAUNDI","AAABJHGK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAULAT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUNDI" varchar(3) NOT NULL default ' ',
  "AAAAUNDJ" varchar(2) NOT NULL default ' ',
  "AAAAUNDK" varchar(12) NOT NULL default ' ',
  "AAAAUMUW" varchar(3) NOT NULL default ' ',
  "AAAAUMUX" varchar(2) NOT NULL default ' ',
  "AAAAUMUY" varchar(12) NOT NULL default ' ',
  "AAAAULZS" varchar(1) NOT NULL default ' ',
  "AAAAULZT" varchar(10) NOT NULL default ' ',
  "AAAAZTMV" varchar(12) NOT NULL default ' ',
  "AAAAULZV" varchar(50) NOT NULL default ' ',
  "AAAAUNNN" varchar(3) NOT NULL default ' ',
  "AAAAUNNO" varchar(2) NOT NULL default ' ',
  "AAAAUNNP" varchar(12) NOT NULL default ' ',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABHESL" varchar(6) NOT NULL default '000000',
  "AAAAUNNT" varchar(3) NOT NULL default ' ',
  "AAAAUNNU" varchar(2) NOT NULL default ' ',
  "AAAAUNNV" varchar(12) NOT NULL default ' ',
  "AAABJFLQ" varchar(8) NOT NULL default '00000000',
  "AAABHFNY" varchar(6) NOT NULL default '000000',
  "AAAAUNOT" varchar(3) NOT NULL default ' ',
  "AAAAUNPR" varchar(2) NOT NULL default ' ',
  "AAABCNWE" varchar(12) NOT NULL default ' ',
  "AAABHBJW" varchar(1) NOT NULL default ' ',
  "AAABLNZF" varchar(8) NOT NULL default '00000000',
  "AAAAUMBH" varchar(8) NOT NULL default '00000000',
  "AAAAUMBI" varchar(6) NOT NULL default '000000',
  "AAAAVYXF" varchar(1) NOT NULL default ' ',
  "AAABECCJ" varchar(1) NOT NULL default ' ',
  "AAAAULCV" varchar(3) NOT NULL default ' ',
  "AAAAULCU" varchar(2) NOT NULL default ' ',
  "AAABGXGV" varchar(12) NOT NULL default ' ',
  "AAAAULCB" varchar(5) NOT NULL default '00000',
  "AAAAULCC" varchar(5) NOT NULL default '00000',
  "AAAAULCD" varchar(3) NOT NULL default ' ',
  "AAABKSPV" varchar(5) NOT NULL default '00000',
  "AAAAULCF" varchar(1) NOT NULL default ' ',
  "AAAAULCG" varchar(1) NOT NULL default ' ',
  "AAAAULCH" varchar(4) NOT NULL default ' ',
  "AAAAULVC" varchar(3) NOT NULL default ' ',
  "AAAAULVB" varchar(2) NOT NULL default ' ',
  "AAAAULVA" varchar(12) NOT NULL default ' ',
  "AAAAULCL" varchar(1) NOT NULL default ' ',
  "AAAAULCM" varchar(4) NOT NULL default ' ',
  "AAAAULCN" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAUNDI","AAAAUNDJ","AAAAUNDK"),
  KEY "AAAAULCS" ("AAABGXGV","AAAAULCU","AAAAULCV","AAABLNZF"),
  KEY "AAAAULCX" ("AAABHBJW","AAABLNZF"),
  KEY "AAAAULDA" ("AAAAUMUY","AAAAUMUX","AAAAUMUW","AAABHBJW","AAAAZTMV"),
  KEY "AAAAULDG" ("AAABHBJW","AAAAZTMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAULDJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAULDS" (
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
CREATE TABLE "AAAAULEF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAULER" (
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
CREATE TABLE "AAAAULFM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAULFV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAULGH" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAULGL" (
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
  UNIQUE KEY "AAAAULHA" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAULHD" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAULHF" (
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
  UNIQUE KEY "AAAAULHU" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAULHX" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAULHZ" (
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
CREATE TABLE "AAAAULIN" (
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
  PRIMARY KEY ("AAABMRIL","AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAAULJD" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAULJG" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAULJI" (
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
  PRIMARY KEY ("AAABMRIL","AAABHZJH"),
  KEY "AAAAULKK" ("AAABMRIL","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAULKN" (
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
  UNIQUE KEY "AAAAULLD" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAULLG" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAULLI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAULLU" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAULLY" (
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
  UNIQUE KEY "AAAAULMN" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAULMQ" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAULMS" (
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
  UNIQUE KEY "AAAAULNH" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAULNK" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAULNM" (
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
  KEY "AAAAULOA" ("AAABLLNN"),
  KEY "AAAAULOC" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAULOE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAULOQ" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAULOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOHM" varchar(3) NOT NULL default ' ',
  "AAAAULZV" varchar(50) NOT NULL default ' ',
  "AAABLCOV" varchar(3) NOT NULL default ' ',
  "AAABLCOW" varchar(2) NOT NULL default ' ',
  "AAABLCOX" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOHM","AAAAULZV","AAABLCOV","AAABLCOW","AAABLCOX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAULPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUNDI" varchar(3) NOT NULL default ' ',
  "AAAAUNDJ" varchar(2) NOT NULL default ' ',
  "AAAAUNDK" varchar(12) NOT NULL default ' ',
  "AAABHSCN" varchar(3) NOT NULL default ' ',
  "AAAAULYQ" varchar(2) NOT NULL default ' ',
  "AAABLDNI" varchar(12) NOT NULL default ' ',
  "AAABHUAL" varchar(3) NOT NULL default ' ',
  "AAAAUMZD" varchar(2) NOT NULL default ' ',
  "AAAAUMZE" varchar(12) NOT NULL default ' ',
  "AAAAULPS" varchar(1) NOT NULL default ' ',
  "AAAAULPT" varchar(1) NOT NULL default ' ',
  "AAAAUMBF" varchar(1) NOT NULL default ' ',
  "AAAAUMBH" varchar(8) NOT NULL default '00000000',
  "AAAAUMBI" varchar(6) NOT NULL default '000000',
  "AAAAUNOT" varchar(3) NOT NULL default ' ',
  "AAAAUNPR" varchar(2) NOT NULL default ' ',
  "AAABCNWE" varchar(12) NOT NULL default ' ',
  "AAAAUMHR" varchar(8) NOT NULL default '00000000',
  "AAAAUMHS" varchar(6) NOT NULL default '000000',
  "AAAAVYWU" varchar(1) NOT NULL default ' ',
  "AAAAVYWV" varchar(1) NOT NULL default ' ',
  "AAAAVYWW" varchar(1) NOT NULL default ' ',
  "AAAAVYWX" varchar(1) NOT NULL default ' ',
  "AAAAVYXB" varchar(1) NOT NULL default ' ',
  "AAAAVYXC" varchar(1) NOT NULL default ' ',
  "AAAAUMTM" varchar(1) NOT NULL default ' ',
  "AAAAUNBE" varchar(1) NOT NULL default ' ',
  "AAAAULQK" varchar(1) NOT NULL default ' ',
  "AAABJHGJ" varchar(1) NOT NULL default ' ',
  "AAABJHGK" varchar(1) NOT NULL default ' ',
  "AAABIKRS" varchar(1) NOT NULL default ' ',
  "AAABIKRT" varchar(1) NOT NULL default ' ',
  "AAABIKRU" varchar(1) NOT NULL default ' ',
  "AAAAULQQ" varchar(8) NOT NULL default '00000000',
  "AAAAULQR" varchar(3) NOT NULL default ' ',
  "AAAAULQS" varchar(2) NOT NULL default ' ',
  "AAAAULQT" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUNDI","AAAAUNDJ","AAAAUNDK","AAABHSCN","AAAAULYQ","AAABLDNI","AAABHUAL","AAAAUMZD","AAAAUMZE"),
  KEY "AAAAULRE" ("AAAAUNDK","AAAAUNDJ","AAAAUNDI","AAAAUNBE","AAABJHGJ"),
  KEY "AAAAULRK" ("AAAAUNDK","AAAAUNDJ","AAAAUNDI","AAAAUMTM"),
  KEY "AAAAULRP" ("AAAAUNDK","AAAAUNDJ","AAAAUNDI","AAABMRIL","AAAAUMHR","AAAAUMTM","AAAAUNBE","AAABJHGJ"),
  KEY "AAAAULRY" ("AAABLDNI","AAAAULYQ","AAABHSCN","AAAAUNDI","AAAAUNDJ","AAAAUNDK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAULSF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNMP" varchar(4) NOT NULL default ' ',
  "AAABLITM" varchar(10) NOT NULL default ' ',
  "AAABENHM" varchar(20) NOT NULL default ' ',
  "AAAAULSK" varchar(20) NOT NULL default ' ',
  "AAAAULSL" varchar(8) NOT NULL default ' ',
  "AAAAULSM" varchar(60) NOT NULL default ' ',
  "AAABLZLD" varchar(12) NOT NULL default ' ',
  "AAAAULSO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAULSR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPO" varchar(10) NOT NULL default '0000000000',
  "AAABDIKT" varchar(3) NOT NULL default ' ',
  "AAAAULTM" varchar(10) NOT NULL default ' ',
  "AAAAULSW" varchar(3) NOT NULL default ' ',
  "AAAAULSX" varchar(1) NOT NULL default ' ',
  "AAAAULTQ" varchar(3) NOT NULL default ' ',
  "AAAAULTP" varchar(10) NOT NULL default ' ',
  "AAAAULTA" varchar(3) NOT NULL default ' ',
  "AAAAULTB" varchar(1) NOT NULL default ' ',
  "AAABIYQF" varchar(3) NOT NULL default ' ',
  "AAAAULTD" varchar(1) NOT NULL default ' ',
  "AAABHVRO" varchar(1) NOT NULL default ' ',
  "AAAAULTF" varchar(3) NOT NULL default ' ',
  "AAAAULTG" varchar(110) NOT NULL default ' ',
  "AAABLOAK" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMLPO"),
  KEY "AAAAULTL" ("AAAAULTM","AAABDIKT"),
  KEY "AAAAULTO" ("AAAAULTP","AAAAULTQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAULTR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(30) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAULUC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUMUW" varchar(3) NOT NULL default ' ',
  "AAAAUMUX" varchar(2) NOT NULL default ' ',
  "AAAAUMUY" varchar(12) NOT NULL default ' ',
  "AAAAZTMV" varchar(12) NOT NULL default ' ',
  "AAAAULVC" varchar(3) NOT NULL default ' ',
  "AAAAULVB" varchar(2) NOT NULL default ' ',
  "AAAAULVA" varchar(12) NOT NULL default ' ',
  "AAAAULUL" varchar(1) NOT NULL default ' ',
  "AAAAUNDI" varchar(3) NOT NULL default ' ',
  "AAAAUNDJ" varchar(2) NOT NULL default ' ',
  "AAAAUNDK" varchar(12) NOT NULL default ' ',
  "AAABHUAL" varchar(3) NOT NULL default ' ',
  "AAAAUMZD" varchar(2) NOT NULL default ' ',
  "AAAAUMZE" varchar(12) NOT NULL default ' ',
  "AAABLNZE" varchar(8) NOT NULL default '00000000',
  "AAABILMX" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAUMUW","AAAAUMUX","AAAAUMUY","AAAAZTMV"),
  KEY "AAAAULUZ" ("AAAAULVA","AAAAULVB","AAAAULVC"),
  KEY "AAAAULVD" ("AAAAUMUY","AAAAUMUX","AAAAUMUW","AAAAZTMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAULVI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAULVO" varchar(10) NOT NULL default '0000000000',
  "AAAAULVL" varchar(255) NOT NULL default ' ',
  "AAABILJC" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAULVO"),
  KEY "AAAAULVP" ("AAABILJC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAULVR" (
  "AAABLTOK" varchar(10) NOT NULL default ' ',
  "AAAAULVT" varchar(5) NOT NULL default ' ',
  "AAAAULVU" varchar(5) NOT NULL default ' ',
  "AAAAZGSF" varchar(10) NOT NULL default ' ',
  "AAAAULVW" varchar(10) NOT NULL default ' ',
  "AAAAULVX" varchar(10) NOT NULL default ' ',
  "AAAAULVY" varchar(10) NOT NULL default ' ',
  "AAAAULVZ" varchar(10) NOT NULL default ' ',
  "AAAAULWA" varchar(10) NOT NULL default ' ',
  "AAAAULWB" varchar(10) NOT NULL default ' ',
  "AAAAULWC" varchar(10) NOT NULL default ' ',
  "AAAAULWD" varchar(10) NOT NULL default ' ',
  "AAAAULWE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTOK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAULWG" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLTOK" varchar(10) NOT NULL default ' ',
  "AAAAULWJ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLTOK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAULWM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCOV" varchar(3) NOT NULL default ' ',
  "AAABLCOW" varchar(2) NOT NULL default ' ',
  "AAABLCOX" varchar(12) NOT NULL default ' ',
  "AAAAULWR" varchar(3) NOT NULL default ' ',
  "AAAAULWS" varchar(2) NOT NULL default ' ',
  "AAAAULWT" varchar(12) NOT NULL default ' ',
  "AAAAULWU" varchar(8) NOT NULL default ' ',
  "AAAAULWV" varchar(3) NOT NULL default ' ',
  "AAAAUNDI" varchar(3) NOT NULL default ' ',
  "AAAAUNDJ" varchar(2) NOT NULL default ' ',
  "AAAAUNDK" varchar(12) NOT NULL default ' ',
  "AAABHSCN" varchar(3) NOT NULL default ' ',
  "AAAAULYQ" varchar(2) NOT NULL default ' ',
  "AAABLDNI" varchar(12) NOT NULL default ' ',
  "AAABHUAL" varchar(3) NOT NULL default ' ',
  "AAAAUMZD" varchar(2) NOT NULL default ' ',
  "AAAAUMZE" varchar(12) NOT NULL default ' ',
  "AAAAULXF" varchar(90) NOT NULL default ' ',
  "AAAAULXG" varchar(3) NOT NULL default ' ',
  "AAAAULXH" varchar(2) NOT NULL default ' ',
  "AAAAULXI" varchar(12) NOT NULL default ' ',
  "AAAAULXJ" varchar(1) NOT NULL default ' ',
  "AAAAULXK" varchar(3) NOT NULL default ' ',
  "AAAAULXL" varchar(2) NOT NULL default ' ',
  "AAAAULXM" varchar(12) NOT NULL default ' ',
  "AAAAULXN" varchar(1) NOT NULL default ' ',
  "AAABEDJP" varchar(90) NOT NULL default ' ',
  "AAAAUNOT" varchar(3) NOT NULL default ' ',
  "AAAAUNPR" varchar(2) NOT NULL default ' ',
  "AAABCNWE" varchar(12) NOT NULL default ' ',
  "AAAAULXS" varchar(90) NOT NULL default ' ',
  "AAAAVYWV" varchar(1) NOT NULL default ' ',
  "AAAAVYWW" varchar(1) NOT NULL default ' ',
  "AAAAVYWX" varchar(1) NOT NULL default ' ',
  "AAAAVYWY" varchar(1) NOT NULL default ' ',
  "AAAAVYWZ" varchar(1) NOT NULL default ' ',
  "AAAAUMXM" varchar(1) NOT NULL default ' ',
  "AAAAVYXA" varchar(1) NOT NULL default ' ',
  "AAAAUMFV" VARBINARY(255),
  "AAAAVYXC" varchar(1) NOT NULL default ' ',
  "AAAAULYC" varchar(1) NOT NULL default ' ',
  "AAAAVIMQ" varchar(1) NOT NULL default ' ',
  "AAAAULYE" varchar(1) NOT NULL default ' ',
  "AAAAULYF" varchar(1) NOT NULL default ' ',
  "AAABKUSC" varchar(1) NOT NULL default ' ',
  "AAAAVYXD" varchar(1) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABLCOV","AAABLCOW","AAABLCOX"),
  KEY "AAAAULYO" ("AAABLDNI","AAAAULYQ","AAABHSCN","AAAAUNDK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAULYT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAAAULZD" varchar(3) NOT NULL default ' ',
  "AAAAULZE" varchar(1) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABLBGA" varchar(20) NOT NULL default ' ',
  "AAABLBGB" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLENH","AAAAULZD","AAAAULZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAULZF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCOV" varchar(3) NOT NULL default ' ',
  "AAABLCOW" varchar(2) NOT NULL default ' ',
  "AAABLCOX" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCOV","AAABLCOW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAULZN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCOV" varchar(3) NOT NULL default ' ',
  "AAABLCOW" varchar(2) NOT NULL default ' ',
  "AAABLCOX" varchar(12) NOT NULL default ' ',
  "AAAAULZS" varchar(1) NOT NULL default ' ',
  "AAAAULZT" varchar(10) NOT NULL default ' ',
  "AAAAZTMV" varchar(12) NOT NULL default ' ',
  "AAAAULZV" varchar(50) NOT NULL default ' ',
  "AAAAUMUW" varchar(3) NOT NULL default ' ',
  "AAAAUMUX" varchar(2) NOT NULL default ' ',
  "AAAAUMUY" varchar(12) NOT NULL default ' ',
  "AAAAULZZ" varchar(12) NOT NULL default ' ',
  "AAAAUNNN" varchar(3) NOT NULL default ' ',
  "AAAAUNNO" varchar(2) NOT NULL default ' ',
  "AAAAUNNP" varchar(12) NOT NULL default ' ',
  "AAAAUNNQ" varchar(12) NOT NULL default ' ',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABHESL" varchar(6) NOT NULL default '000000',
  "AAAAUNNT" varchar(3) NOT NULL default ' ',
  "AAAAUNNU" varchar(2) NOT NULL default ' ',
  "AAAAUNNV" varchar(12) NOT NULL default ' ',
  "AAAAUNNW" varchar(12) NOT NULL default ' ',
  "AAABJFLQ" varchar(8) NOT NULL default '00000000',
  "AAABHFNY" varchar(6) NOT NULL default '000000',
  "AAAAUNOT" varchar(3) NOT NULL default ' ',
  "AAAAUNPR" varchar(2) NOT NULL default ' ',
  "AAABCNWE" varchar(12) NOT NULL default ' ',
  "AAAAUMAP" varchar(1) NOT NULL default ' ',
  "AAAAUMAQ" varchar(1) NOT NULL default ' ',
  "AAABHBJW" varchar(1) NOT NULL default ' ',
  "AAABFVFF" varchar(1) NOT NULL default ' ',
  "AAAAUMAT" VARBINARY(255),
  "AAAAUMAU" VARBINARY(255),
  "AAAAUMAV" VARBINARY(255),
  "AAAAUMAW" VARBINARY(255),
  "AAAAUMAX" VARBINARY(255),
  "AAAAUMAY" VARBINARY(255),
  "AAAAUMAZ" VARBINARY(255),
  "AAAAUMBA" VARBINARY(255),
  "AAAAUMBB" VARBINARY(255),
  "AAAAUMBC" VARBINARY(255),
  "AAAAUMBD" VARBINARY(255),
  "AAAAUMBE" VARBINARY(255),
  "AAAAUMBF" varchar(1) NOT NULL default ' ',
  "AAABLNZF" varchar(8) NOT NULL default '00000000',
  "AAAAUMBH" varchar(8) NOT NULL default '00000000',
  "AAAAUMBI" varchar(6) NOT NULL default '000000',
  "AAAAZTMW" varchar(1) NOT NULL default ' ',
  "AAAAVYXF" varchar(1) NOT NULL default ' ',
  "AAAAUMJB" varchar(1) NOT NULL default ' ',
  "AAAAUMBM" varchar(1) NOT NULL default ' ',
  "AAABKQPV" varchar(8) NOT NULL default '00000000',
  "AAAAUMET" varchar(6) NOT NULL default '000000',
  "AAAAUMBP" VARBINARY(255),
  "AAAAUMBQ" VARBINARY(255),
  "AAAAUMBR" VARBINARY(255),
  "AAAAUMBS" VARBINARY(255),
  "AAAAUMBT" VARBINARY(255),
  "AAAAUMBU" VARBINARY(255),
  "AAAAUMBV" VARBINARY(255),
  "AAAAUMBW" VARBINARY(255),
  "AAAAUMBX" VARBINARY(255),
  "AAAAUMBY" VARBINARY(255),
  "AAAAUMFV" VARBINARY(255),
  "AAAAUMCA" varchar(1) NOT NULL default ' ',
  "AAABLXFH" varchar(1) NOT NULL default ' ',
  "AAABIFHV" varchar(1) NOT NULL default ' ',
  "AAABIFHW" varchar(1) NOT NULL default ' ',
  "AAABIFHU" varchar(54) NOT NULL default ' ',
  "AAAAUMCF" varchar(32) NOT NULL default ' ',
  "AAAAUMCG" varchar(3) NOT NULL default ' ',
  "AAAAUMCH" varchar(2) NOT NULL default ' ',
  "AAAAUMCI" varchar(12) NOT NULL default ' ',
  "AAAAUMCJ" varchar(3) NOT NULL default ' ',
  "AAAAUMCK" varchar(2) NOT NULL default ' ',
  "AAAAUMCL" varchar(12) NOT NULL default ' ',
  "AAAAUMCM" varchar(1) NOT NULL default ' ',
  "AAAAUMCN" varchar(3) NOT NULL default ' ',
  "AAAAUMCO" varchar(2) NOT NULL default ' ',
  "AAAAUMCP" varchar(12) NOT NULL default ' ',
  "AAABDDFD" varchar(1) NOT NULL default ' ',
  "AAAAUMCR" varchar(1) NOT NULL default ' ',
  "AAABECCJ" varchar(1) NOT NULL default ' ',
  "AAAAUMCT" varchar(1) NOT NULL default ' ',
  "AAAAUMCU" varchar(1) NOT NULL default ' ',
  "AAABHBWG" varchar(3) NOT NULL default ' ',
  "AAAAUMCW" varchar(12) NOT NULL default ' ',
  "AAAAUMCX" varchar(12) NOT NULL default ' ',
  "AAAAZOEQ" varchar(4) NOT NULL default ' ',
  "AAAAUMCZ" varchar(12) NOT NULL default ' ',
  "AAAAUMDN" VARBINARY(255),
  "AAAAUMDB" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAABLCOV","AAABLCOW","AAABLCOX"),
  KEY "AAAAUMDG" ("AAABLCOX","AAABLCOW","AAABLCOV","AAABLNZF"),
  KEY "AAAAUMDL" ("AAABMRIL","AAAAUMDN"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUMDO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUMUW" varchar(3) NOT NULL default ' ',
  "AAAAUMUX" varchar(2) NOT NULL default ' ',
  "AAAAUMUY" varchar(12) NOT NULL default ' ',
  "AAAAUMEA" varchar(3) NOT NULL default ' ',
  "AAAAUMEB" varchar(2) NOT NULL default ' ',
  "AAAAUMEC" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUMUW","AAAAUMUX","AAAAUMUY","AAAAUMEA","AAAAUMEB","AAAAUMEC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUMED" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCOV" varchar(3) NOT NULL default ' ',
  "AAABLCOW" varchar(2) NOT NULL default ' ',
  "AAABLCOX" varchar(12) NOT NULL default ' ',
  "AAABHUAL" varchar(3) NOT NULL default ' ',
  "AAAAUMZD" varchar(2) NOT NULL default ' ',
  "AAAAUMZE" varchar(12) NOT NULL default ' ',
  "AAAAUNEG" varchar(3) NOT NULL default ' ',
  "AAAAUNEH" varchar(2) NOT NULL default ' ',
  "AAABETFA" varchar(12) NOT NULL default ' ',
  "AAAAUMEO" varchar(3) NOT NULL default ' ',
  "AAAAUMIG" varchar(2) NOT NULL default ' ',
  "AAAAUMIF" varchar(12) NOT NULL default ' ',
  "AAAAUMER" varchar(12) NOT NULL default ' ',
  "AAABKQPV" varchar(8) NOT NULL default '00000000',
  "AAAAUMET" varchar(6) NOT NULL default '000000',
  "AAABLCPL" varchar(12) NOT NULL default ' ',
  "AAAAUMHR" varchar(8) NOT NULL default '00000000',
  "AAAAUMHS" varchar(6) NOT NULL default '000000',
  "AAAAUMEX" varchar(12) NOT NULL default ' ',
  "AAAAUNOT" varchar(3) NOT NULL default ' ',
  "AAAAUNPR" varchar(2) NOT NULL default ' ',
  "AAABCNWE" varchar(12) NOT NULL default ' ',
  "AAABLADN" varchar(1) NOT NULL default ' ',
  "AAAAUMFC" varchar(8) NOT NULL default '00000000',
  "AAAAUMFD" varchar(6) NOT NULL default '000000',
  "AAAAUMTM" varchar(1) NOT NULL default ' ',
  "AAAAUNPY" varchar(8) NOT NULL default ' ',
  "AAAAUMFG" int(11) NOT NULL default '0',
  "AAAAUMFH" varchar(3) NOT NULL default ' ',
  "AAAAUMFI" varchar(30) NOT NULL default ' ',
  "AAAAVYWU" varchar(1) NOT NULL default ' ',
  "AAAAVYWV" varchar(1) NOT NULL default ' ',
  "AAAAVYWW" varchar(1) NOT NULL default ' ',
  "AAAAVYWX" varchar(1) NOT NULL default ' ',
  "AAAAVYWY" varchar(1) NOT NULL default ' ',
  "AAAAVYWZ" varchar(1) NOT NULL default ' ',
  "AAAAUMXM" varchar(1) NOT NULL default ' ',
  "AAABLNZE" varchar(8) NOT NULL default '00000000',
  "AAABHVKJ" varchar(6) NOT NULL default '000000',
  "AAAAUMFS" varchar(8) NOT NULL default '00000000',
  "AAAAUMFT" varchar(6) NOT NULL default '000000',
  "AAAAVYXA" varchar(1) NOT NULL default ' ',
  "AAAAUMFV" VARBINARY(255),
  "AAAAUMTL" varchar(4) NOT NULL default ' ',
  "AAAAVYXB" varchar(1) NOT NULL default ' ',
  "AAAAXKQB" varchar(8) NOT NULL default '00000000',
  "AAAAUMFZ" varchar(6) NOT NULL default '000000',
  "AAABKSPH" varchar(8) NOT NULL default '00000000',
  "AAABBMRX" varchar(6) NOT NULL default '000000',
  "AAAAVYXC" varchar(1) NOT NULL default ' ',
  "AAAAUMTN" varchar(1) NOT NULL default ' ',
  "AAAAVYXD" varchar(1) NOT NULL default ' ',
  "AAAAUMIU" varchar(1) NOT NULL default ' ',
  "AAAAUMIV" varchar(1) NOT NULL default ' ',
  "AAAAUMIW" varchar(1) NOT NULL default ' ',
  "AAAAWUOJ" varchar(3) NOT NULL default ' ',
  "AAAAUMGJ" varchar(2) NOT NULL default ' ',
  "AAABCOLW" varchar(12) NOT NULL default ' ',
  "AAAAUMGL" varchar(12) NOT NULL default ' ',
  "AAAAUMGM" varchar(12) NOT NULL default ' ',
  "AAABKPXJ" varchar(3) NOT NULL default ' ',
  "AAAAUMHW" varchar(2) NOT NULL default ' ',
  "AAAAUMHU" varchar(12) NOT NULL default ' ',
  "AAAAUMHN" varchar(3) NOT NULL default ' ',
  "AAAAUMHO" varchar(2) NOT NULL default ' ',
  "AAAAUMHM" varchar(12) NOT NULL default ' ',
  "AAAAUMWY" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAABLCOV","AAABLCOW","AAABLCOX","AAABHUAL","AAAAUMZD","AAAAUMZE","AAAAUNEG","AAAAUNEH","AAABETFA"),
  KEY "AAAAUMHE" ("AAABLCOX","AAABLCOV","AAABLCOW","AAAAUNEG","AAAAUNEH","AAABETFA"),
  KEY "AAAAUMHL" ("AAAAUMHM","AAAAUMHN","AAAAUMHO"),
  KEY "AAAAUMHP" ("AAABMRIL","AAAAUMHR","AAAAUMHS"),
  KEY "AAAAUMHT" ("AAAAUMHU","AAABKPXJ","AAAAUMHW"),
  KEY "AAAAUMHX" ("AAABETFA","AAAAUNEG","AAAAUNEH","AAABLCOX","AAABLCOV","AAABLCOW"),
  KEY "AAAAUMIE" ("AAAAUMIF","AAAAUMIG"),
  KEY "AAAAUMIH" ("AAABMRIL","AAAAUMWY"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUMIK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMKFU" varchar(14) NOT NULL default '00000000000000',
  "AAABLRJS" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABMKFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUMIS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUMIU" varchar(1) NOT NULL default ' ',
  "AAAAUMIV" varchar(1) NOT NULL default ' ',
  "AAAAUMIW" varchar(1) NOT NULL default ' ',
  "AAAAUMIX" varchar(1) NOT NULL default ' ',
  "AAAAUMIY" varchar(1) NOT NULL default ' ',
  "AAAAUMIZ" varchar(1) NOT NULL default ' ',
  "AAAAUMJA" varchar(1) NOT NULL default ' ',
  "AAAAUMJB" varchar(1) NOT NULL default ' ',
  "AAAAUMJC" varchar(1) NOT NULL default ' ',
  "AAAAUMJD" varchar(1) NOT NULL default ' ',
  "AAAAUMJE" varchar(40) NOT NULL default ' ',
  "AAAAUNOC" VARBINARY(255),
  "AAAAUNOD" VARBINARY(255),
  "AAAAUNOF" varchar(1) NOT NULL default ' ',
  "AAAAUNOE" VARBINARY(255),
  "AAAAUNOG" varchar(1) NOT NULL default ' ',
  "AAAAUNOH" varchar(1) NOT NULL default ' ',
  "AAAAUNOI" varchar(1) NOT NULL default ' ',
  "AAAAUNOJ" varchar(1) NOT NULL default ' ',
  "AAAAUNOK" varchar(1) NOT NULL default ' ',
  "AAAAUNOL" varchar(1) NOT NULL default ' ',
  "AAAAUNOM" varchar(1) NOT NULL default ' ',
  "AAAAUNON" VARBINARY(255),
  "AAAAUNOP" VARBINARY(255),
  "AAAAUNOQ" VARBINARY(255),
  "AAABBEDR" varchar(1) NOT NULL default ' ',
  "AAAAUNOT" varchar(3) NOT NULL default ' ',
  "AAAAYFIA" varchar(1) NOT NULL default ' ',
  "AAAAVYXE" varchar(1) NOT NULL default ' ',
  "AAAAUNOW" varchar(1) NOT NULL default ' ',
  "AAAAUNOX" varchar(1) NOT NULL default ' ',
  "AAAAUNOY" varchar(1) NOT NULL default ' ',
  "AAAAUNOZ" varchar(1) NOT NULL default ' ',
  "AAAAUNPJ" varchar(2) NOT NULL default ' ',
  "AAAAUNPK" varchar(12) NOT NULL default ' ',
  "AAAAUNPL" varchar(2) NOT NULL default ' ',
  "AAAAUNPM" varchar(12) NOT NULL default ' ',
  "AAAAUNPN" varchar(2) NOT NULL default ' ',
  "AAAAUNPO" varchar(12) NOT NULL default ' ',
  "AAAAUMKH" varchar(2) NOT NULL default ' ',
  "AAAAUMKI" varchar(12) NOT NULL default ' ',
  "AAAAUNPU" varchar(1) NOT NULL default ' ',
  "AAAAUNPV" varchar(2) NOT NULL default ' ',
  "AAAAUNRY" varchar(10) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAAAUNPY" varchar(3) NOT NULL default ' ',
  "AAABJHGJ" varchar(1) NOT NULL default ' ',
  "AAABJHGK" varchar(1) NOT NULL default ' ',
  "AAABIKRS" varchar(1) NOT NULL default ' ',
  "AAABHALW" varchar(1) NOT NULL default ' ',
  "AAABGBXV" varchar(32) NOT NULL default ' ',
  "AAAAUMKT" varchar(3) NOT NULL default '000',
  "AAABLNZF" varchar(3) NOT NULL default '000',
  "AAAAUMKV" varchar(32) NOT NULL default ' ',
  "AAAAUMKW" varchar(50) NOT NULL default ' ',
  "AAAAUNQG" varchar(1) NOT NULL default ' ',
  "AAAAUNQH" varchar(1) NOT NULL default ' ',
  "AAAAUNQI" varchar(1) NOT NULL default ' ',
  "AAAAUNQJ" varchar(1) NOT NULL default ' ',
  "AAAAUMLB" varchar(1) NOT NULL default ' ',
  "AAAAUMLC" varchar(1) NOT NULL default ' ',
  "AAAAUMLD" varchar(1) NOT NULL default ' ',
  "AAAAUMLE" varchar(3) NOT NULL default '000',
  "AAAAUMLF" varchar(1) NOT NULL default ' ',
  "AAAAUMLG" varchar(241) NOT NULL default ' ',
  "AAAAUMLH" varchar(12) NOT NULL default '000000000000',
  "AAAAUMLI" varchar(8) NOT NULL default '00000000',
  "AAAAUMLJ" varchar(6) NOT NULL default '000000',
  "AAAAUMLK" varchar(12) NOT NULL default '000000000000',
  "AAAAUMLL" varchar(1) NOT NULL default ' ',
  "AAAAUMLM" varchar(1) NOT NULL default ' ',
  "AAAAUMLN" varchar(1) NOT NULL default ' ',
  "AAAAUMLO" varchar(1) NOT NULL default ' ',
  "AAAAUMLP" varchar(1) NOT NULL default ' ',
  "AAAAUMLQ" varchar(1) NOT NULL default ' ',
  "AAAAUMLR" varchar(1) NOT NULL default ' ',
  "AAAAUMLS" varchar(1) NOT NULL default ' ',
  "AAAAUMLT" varchar(1) NOT NULL default ' ',
  "AAAAUMLU" varchar(10) NOT NULL default ' ',
  "AAAAUMLV" varchar(14) NOT NULL default '00000000000000',
  "AAAAUMLW" varchar(4) NOT NULL default '0000',
  "AAAAUMLX" varchar(1) NOT NULL default ' ',
  "AAAAUMLY" varchar(32) NOT NULL default ' ',
  "AAAAUMLZ" varchar(8) NOT NULL default ' ',
  "AAAAUMMA" varchar(3) NOT NULL default '000',
  "AAAAUMMB" varchar(4) NOT NULL default ' ',
  "AAAAUZTA" varchar(1) NOT NULL default ' ',
  "AAAAUMMD" varchar(8) NOT NULL default ' ',
  "AAAAUMME" varchar(3) NOT NULL default '000',
  "AAAAUMMF" varchar(3) NOT NULL default ' ',
  "AAAAUMMG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUMMI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHBVV" varchar(12) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAAAUMMM" int(11) NOT NULL default '0',
  "AAAAUMMN" int(11) NOT NULL default '0',
  "AAAAUMMO" int(11) NOT NULL default '0',
  "AAAAUMMP" int(11) NOT NULL default '0',
  "AAAAUMMQ" int(11) NOT NULL default '0',
  "AAAAUMMR" int(11) NOT NULL default '0',
  "AAAAUMMS" int(11) NOT NULL default '0',
  "AAAAUMMT" int(11) NOT NULL default '0',
  "AAAAUMMU" int(11) NOT NULL default '0',
  "AAAAUMMV" int(11) NOT NULL default '0',
  "AAAAUMMW" int(11) NOT NULL default '0',
  "AAAAUMMX" int(11) NOT NULL default '0',
  "AAAAUMMY" int(11) NOT NULL default '0',
  "AAAAUMMZ" int(11) NOT NULL default '0',
  "AAAAUMNA" int(11) NOT NULL default '0',
  "AAAAUMNB" int(11) NOT NULL default '0',
  "AAAAUMNC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHBVV","AAABMKFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUMNG" (
  "AAABLRXL" varchar(64) NOT NULL default ' ',
  "AAABMKFU" varchar(14) NOT NULL default ' ',
  "AAABMMJT" decimal(17,0) NOT NULL default '0',
  "AAAAUMNK" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLRXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUMNM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUNDI" varchar(3) NOT NULL default ' ',
  "AAAAUNDJ" varchar(2) NOT NULL default ' ',
  "AAAAUNDK" varchar(12) NOT NULL default ' ',
  "AAAAUNSX" varchar(3) NOT NULL default ' ',
  "AAAAUNSY" varchar(2) NOT NULL default ' ',
  "AAAAUNSZ" varchar(12) NOT NULL default ' ',
  "AAABLCOV" varchar(3) NOT NULL default ' ',
  "AAABLCOW" varchar(2) NOT NULL default ' ',
  "AAABLCOX" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUNDI","AAAAUNDJ","AAAAUNDK","AAAAUNSX","AAAAUNSY","AAAAUNSZ","AAABLCOV","AAABLCOW","AAABLCOX"),
  KEY "AAAAUMOH" ("AAAAUNSZ","AAAAUNSY","AAAAUNSX","AAABMRIL","AAAAUNDI","AAAAUNDJ","AAAAUNDK"),
  KEY "AAAAUMOP" ("AAABLCOX","AAABLCOV","AAABLCOW"),
  KEY "AAAAUMOT" ("AAAAUNSZ","AAAAUNSY","AAAAUNSX","AAABLCOV","AAABMRIL","AAABLCOW","AAABLCOX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUMPB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCOV" varchar(3) NOT NULL default ' ',
  "AAABLCOW" varchar(2) NOT NULL default ' ',
  "AAABLCOX" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCOV","AAABLCOW","AAABLCOX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUMPK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUMQH" varchar(8) NOT NULL default ' ',
  "AAAAUMQI" varchar(3) NOT NULL default ' ',
  "AAAAUMQJ" varchar(3) NOT NULL default ' ',
  "AAAAUMQK" varchar(2) NOT NULL default ' ',
  "AAAAUMQL" varchar(12) NOT NULL default ' ',
  "AAAAUMQM" varchar(3) NOT NULL default ' ',
  "AAAAUMQN" varchar(2) NOT NULL default ' ',
  "AAAAUMQO" varchar(12) NOT NULL default ' ',
  "AAAAUMQT" varchar(8) NOT NULL default ' ',
  "AAAAUMQU" varchar(3) NOT NULL default ' ',
  "AAAAUMQV" varchar(3) NOT NULL default ' ',
  "AAAAUMQW" varchar(2) NOT NULL default ' ',
  "AAAAUMQX" varchar(12) NOT NULL default ' ',
  "AAAAUMQY" varchar(3) NOT NULL default ' ',
  "AAAAUMQZ" varchar(2) NOT NULL default ' ',
  "AAAAUMRA" varchar(12) NOT NULL default ' ',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABHESL" varchar(6) NOT NULL default '000000',
  "AAABJFLQ" varchar(8) NOT NULL default '00000000',
  "AAABHFNY" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAAAUMQH","AAAAUMQI","AAAAUMQJ","AAAAUMQK","AAAAUMQL","AAAAUMQM","AAAAUMQN","AAAAUMQO","AAAAUMQT","AAAAUMQU"),
  UNIQUE KEY "AAAAUMQR" ("AAABMRIL","AAAAUMQT","AAAAUMQU","AAAAUMQV","AAAAUMQW","AAAAUMQX","AAAAUMQY","AAAAUMQZ","AAAAUMRA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUMRB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUMRX" varchar(3) NOT NULL default ' ',
  "AAAAUMRY" varchar(2) NOT NULL default ' ',
  "AAAAUMRZ" varchar(12) NOT NULL default ' ',
  "AAAAUMSA" varchar(3) NOT NULL default ' ',
  "AAAAUMSB" varchar(2) NOT NULL default ' ',
  "AAAAUMSC" varchar(12) NOT NULL default ' ',
  "AAAAUMSD" varchar(3) NOT NULL default ' ',
  "AAAAUMSQ" varchar(2) NOT NULL default ' ',
  "AAAAUMSR" varchar(12) NOT NULL default ' ',
  "AAAAUMSG" varchar(10) NOT NULL default ' ',
  "AAAAUMSJ" varchar(3) NOT NULL default ' ',
  "AAAAUMSK" varchar(2) NOT NULL default ' ',
  "AAAAUMSL" varchar(12) NOT NULL default ' ',
  "AAAAUMSM" varchar(3) NOT NULL default ' ',
  "AAAAUMSN" varchar(2) NOT NULL default ' ',
  "AAAAUMSO" varchar(12) NOT NULL default ' ',
  "AAAAUMSP" varchar(3) NOT NULL default ' ',
  "AAAAUMRU" varchar(2) NOT NULL default ' ',
  "AAAAUMRV" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUMRX","AAAAUMRY","AAAAUMRZ","AAAAUMSA","AAAAUMSB","AAAAUMSC","AAAAUMSD","AAAAUMSQ","AAAAUMSR","AAAAUMSG"),
  UNIQUE KEY "AAAAUMSH" ("AAABMRIL","AAAAUMSJ","AAAAUMSK","AAAAUMSL","AAAAUMSM","AAAAUMSN","AAAAUMSO","AAAAUMSP","AAAAUMSQ","AAAAUMSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUMSS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUMUW" varchar(3) NOT NULL default ' ',
  "AAAAUMUX" varchar(2) NOT NULL default ' ',
  "AAAAUMUY" varchar(12) NOT NULL default ' ',
  "AAABLCOV" varchar(3) NOT NULL default ' ',
  "AAABLCOW" varchar(2) NOT NULL default ' ',
  "AAABLCOX" varchar(12) NOT NULL default ' ',
  "AAABHUAL" varchar(3) NOT NULL default ' ',
  "AAAAUMZD" varchar(2) NOT NULL default ' ',
  "AAAAUMZE" varchar(12) NOT NULL default ' ',
  "AAAAUMUP" varchar(2) NOT NULL default '00',
  "AAABKSPH" varchar(8) NOT NULL default '00000000',
  "AAABBMRX" varchar(6) NOT NULL default '000000',
  "AAAAUMTG" varchar(2) NOT NULL default '00',
  "AAAAUMTH" varchar(1) NOT NULL default ' ',
  "AAAAVBWM" varchar(6) NOT NULL default '000000',
  "AAABJZAF" varchar(8) NOT NULL default '00000000',
  "AAAAUMTK" varchar(1) NOT NULL default ' ',
  "AAAAUMTL" varchar(4) NOT NULL default ' ',
  "AAAAUMTM" varchar(1) NOT NULL default ' ',
  "AAAAUMTN" varchar(1) NOT NULL default ' ',
  "AAAAUMTO" varchar(8) NOT NULL default '00000000',
  "AAAAUMTP" varchar(6) NOT NULL default '000000',
  "AAAAUMTQ" varchar(2) NOT NULL default '00',
  "AAAAUMTR" varchar(2) NOT NULL default '00',
  "AAAAUMTS" varchar(1) NOT NULL default ' ',
  "AAAAUNNN" varchar(3) NOT NULL default ' ',
  "AAAAUNNO" varchar(2) NOT NULL default ' ',
  "AAAAUNNP" varchar(12) NOT NULL default ' ',
  "AAAAUMTW" varchar(8) NOT NULL default '00000000',
  "AAAAUMTX" varchar(6) NOT NULL default '000000',
  "AAAAUNNT" varchar(3) NOT NULL default ' ',
  "AAAAUNNU" varchar(2) NOT NULL default ' ',
  "AAAAUNNV" varchar(12) NOT NULL default ' ',
  "AAAAUMUB" varchar(8) NOT NULL default '00000000',
  "AAAAUMUC" varchar(6) NOT NULL default '000000',
  "AAABJHGJ" varchar(1) NOT NULL default ' ',
  "AAABJHGK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUMUW","AAAAUMUX","AAAAUMUY","AAABLCOV","AAABLCOW","AAABLCOX","AAABHUAL","AAAAUMZD","AAAAUMZE","AAAAUMUP"),
  KEY "AAAAUMUQ" ("AAAAUMUY","AAAAUMUX","AAAAUMUW","AAABKSPH"),
  KEY "AAAAUMUV" ("AAAAUMUW","AAAAUMUX","AAAAUMUY","AAABLCOV","AAABLCOW","AAABLCOX","AAABHUAL","AAAAUMZD","AAAAUMZE","AAABKSPH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUMVG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUNSX" varchar(3) NOT NULL default ' ',
  "AAAAUNSY" varchar(2) NOT NULL default ' ',
  "AAAAUNSZ" varchar(12) NOT NULL default ' ',
  "AAAAUMVW" varchar(4) NOT NULL default ' ',
  "AAAAUNDI" varchar(3) NOT NULL default ' ',
  "AAAAUNDJ" varchar(2) NOT NULL default ' ',
  "AAAAUNDK" varchar(12) NOT NULL default ' ',
  "AAAAUMVP" varchar(50) NOT NULL default ' ',
  "AAABDRLH" varchar(4) NOT NULL default ' ',
  "AAAAUMVR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUNSX","AAAAUNSY","AAAAUNSZ","AAAAUMVW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUMVX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUNPY" varchar(6) NOT NULL default ' ',
  "AAABLCOV" varchar(3) NOT NULL default ' ',
  "AAABLCOW" varchar(2) NOT NULL default ' ',
  "AAABLCOX" varchar(12) NOT NULL default ' ',
  "AAABHUAL" varchar(3) NOT NULL default ' ',
  "AAAAUMZD" varchar(2) NOT NULL default ' ',
  "AAAAUMZE" varchar(12) NOT NULL default ' ',
  "AAAAUNEG" varchar(3) NOT NULL default ' ',
  "AAAAUNEH" varchar(2) NOT NULL default ' ',
  "AAABETFA" varchar(12) NOT NULL default ' ',
  "AAAAWRTT" varchar(1) NOT NULL default ' ',
  "AAAAUMZL" varchar(8) NOT NULL default '00000000',
  "AAAAUMYK" varchar(6) NOT NULL default '000000',
  "AAAAUMWM" varchar(4) NOT NULL default ' ',
  "AAAAUMWN" varchar(8) NOT NULL default '00000000',
  "AAAAUMWO" varchar(6) NOT NULL default '000000',
  "AAAAUMXM" varchar(1) NOT NULL default ' ',
  "AAAAUNOT" varchar(3) NOT NULL default ' ',
  "AAAAUNPR" varchar(2) NOT NULL default ' ',
  "AAABCNWE" varchar(12) NOT NULL default ' ',
  "AAABMRFO" varchar(4) NOT NULL default ' ',
  "AAABJCRB" varchar(12) NOT NULL default ' ',
  "AAAAUMWV" varchar(3) NOT NULL default ' ',
  "AAAAUMWW" varchar(2) NOT NULL default ' ',
  "AAAAUMWX" varchar(12) NOT NULL default ' ',
  "AAAAUMWY" VARBINARY(255),
  "AAAAUMWZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUNPY","AAABLCOV","AAABLCOW","AAABLCOX","AAABHUAL","AAAAUMZD","AAAAUMZE","AAAAUNEG","AAAAUNEH","AAABETFA"),
  KEY "AAAAUMXL" ("AAAAUMXM","AAAAUMZL","AAAAUMYK"),
  KEY "AAAAUMXP" ("AAABETFA","AAAAUNEG","AAAAUNEH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUMXT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCOV" varchar(3) NOT NULL default ' ',
  "AAABLCOW" varchar(2) NOT NULL default ' ',
  "AAABLCOX" varchar(12) NOT NULL default ' ',
  "AAABHUAL" varchar(3) NOT NULL default ' ',
  "AAAAUMZD" varchar(2) NOT NULL default ' ',
  "AAAAUMZE" varchar(12) NOT NULL default ' ',
  "AAAAUNEG" varchar(3) NOT NULL default ' ',
  "AAAAUNEH" varchar(2) NOT NULL default ' ',
  "AAABETFA" varchar(12) NOT NULL default ' ',
  "AAABMLPO" varchar(5) NOT NULL default ' ',
  "AAAAUNPY" varchar(6) NOT NULL default ' ',
  "AAAAUMYG" varchar(3) NOT NULL default ' ',
  "AAAAUMYH" varchar(2) NOT NULL default ' ',
  "AAAAUMYI" varchar(12) NOT NULL default ' ',
  "AAAAUMZL" varchar(8) NOT NULL default '00000000',
  "AAAAUMYK" varchar(6) NOT NULL default '000000',
  "AAAAUMYL" varchar(8) NOT NULL default '00000000',
  "AAAAUMYM" varchar(6) NOT NULL default '000000',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  "AAAAZOEQ" varchar(12) NOT NULL default ' ',
  "AAABEDJP" varchar(30) NOT NULL default ' ',
  "AAABLKZT" varchar(1) NOT NULL default ' ',
  "AAAAUMYX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCOV","AAABLCOW","AAABLCOX","AAABHUAL","AAAAUMZD","AAAAUMZE","AAAAUNEG","AAAAUNEH","AAABETFA","AAABMLPO"),
  KEY "AAAAUMZJ" ("AAAAUNPY","AAAAUMZL"),
  KEY "AAAAUMZM" ("AAABLKZT"),
  KEY "AAAAUMZO" ("AAABMLDX","AAABMLDW"),
  KEY "AAAAUMZR" ("AAABETFA","AAAAUNEG","AAAAUNEH"),
  KEY "AAAAUMZV" ("AAABEDJP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUMZX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUNAD" varchar(8) NOT NULL default '00000000',
  "AAAAUNAE" varchar(6) NOT NULL default ' ',
  "AAAAUNAB" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAUNAD","AAAAUNAE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUNSX" varchar(3) NOT NULL default ' ',
  "AAAAUNSY" varchar(2) NOT NULL default ' ',
  "AAAAUNSZ" varchar(12) NOT NULL default ' ',
  "AAABFLVC" varchar(3) NOT NULL default ' ',
  "AAAAUNCZ" varchar(2) NOT NULL default ' ',
  "AAABFOMB" varchar(12) NOT NULL default ' ',
  "AAAAUNCW" varchar(2) NOT NULL default '00',
  "AAABLGOZ" varchar(8) NOT NULL default '00000000',
  "AAAAUNAP" varchar(6) NOT NULL default '000000',
  "AAAAUNAQ" varchar(8) NOT NULL default '00000000',
  "AAAAUNAR" varchar(6) NOT NULL default '000000',
  "AAAAUNAS" varchar(1) NOT NULL default ' ',
  "AAAAUNAT" varchar(1) NOT NULL default ' ',
  "AAAAUNAU" varchar(1) NOT NULL default ' ',
  "AAABBEDR" varchar(1) NOT NULL default ' ',
  "AAAAURNF" varchar(1) NOT NULL default ' ',
  "AAAAUNAX" varchar(1) NOT NULL default ' ',
  "AAAAUNAY" varchar(1) NOT NULL default ' ',
  "AAAAUNAZ" varchar(1) NOT NULL default ' ',
  "AAAAUNBA" varchar(1) NOT NULL default ' ',
  "AAAAUNBB" varchar(1) NOT NULL default ' ',
  "AAAAUNBC" varchar(1) NOT NULL default ' ',
  "AAAAUNBD" varchar(1) NOT NULL default ' ',
  "AAAAUNBE" varchar(1) NOT NULL default ' ',
  "AAAAWWDZ" varchar(1) NOT NULL default ' ',
  "AAAAUNBG" varchar(1) NOT NULL default ' ',
  "AAAAUNBH" varchar(1) NOT NULL default ' ',
  "AAABFYMZ" varchar(1) NOT NULL default ' ',
  "AAABLOTC" varchar(1) NOT NULL default ' ',
  "AAABLOTD" varchar(1) NOT NULL default ' ',
  "AAABLOTE" varchar(1) NOT NULL default ' ',
  "AAABLOTF" varchar(1) NOT NULL default ' ',
  "AAAAYVTL" varchar(1) NOT NULL default ' ',
  "AAAAYVTM" varchar(1) NOT NULL default ' ',
  "AAAAUNBP" varchar(1) NOT NULL default ' ',
  "AAAAUNBQ" varchar(1) NOT NULL default ' ',
  "AAAAUNBR" varchar(1) NOT NULL default ' ',
  "AAAAUNBS" varchar(3) NOT NULL default ' ',
  "AAAAUNBT" varchar(2) NOT NULL default ' ',
  "AAAAUNBU" varchar(12) NOT NULL default ' ',
  "AAAAUNBV" varchar(3) NOT NULL default ' ',
  "AAAAUNBW" varchar(2) NOT NULL default ' ',
  "AAAAUNBX" varchar(12) NOT NULL default ' ',
  "AAAAUNBY" varchar(3) NOT NULL default ' ',
  "AAAAUNBZ" varchar(2) NOT NULL default ' ',
  "AAAAUNCA" varchar(12) NOT NULL default ' ',
  "AAABFYOD" varchar(1) NOT NULL default ' ',
  "AAABJHGJ" varchar(1) NOT NULL default ' ',
  "AAABJHGK" varchar(1) NOT NULL default ' ',
  "AAABIKRS" varchar(1) NOT NULL default ' ',
  "AAABIKRT" varchar(1) NOT NULL default ' ',
  "AAABIKRU" varchar(1) NOT NULL default ' ',
  "AAABIKRV" varchar(1) NOT NULL default ' ',
  "AAABIKRW" varchar(1) NOT NULL default ' ',
  "AAABIKRX" varchar(1) NOT NULL default ' ',
  "AAABIKRY" varchar(1) NOT NULL default ' ',
  "AAAAUNCL" varchar(1) NOT NULL default ' ',
  "AAAAUNCM" varchar(1) NOT NULL default ' ',
  "AAAAUNCN" varchar(3) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUNSX","AAAAUNSY","AAAAUNSZ","AAABFLVC","AAAAUNCZ","AAABFOMB","AAAAUNCW"),
  KEY "AAAAUNCX" ("AAABFOMB","AAAAUNCZ","AAABFLVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNDB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUNSX" varchar(3) NOT NULL default ' ',
  "AAAAUNSY" varchar(2) NOT NULL default ' ',
  "AAAAUNSZ" varchar(12) NOT NULL default ' ',
  "AAABMDXP" varchar(12) NOT NULL default ' ',
  "AAABMDWW" varchar(12) NOT NULL default ' ',
  "AAAAUNDI" varchar(3) NOT NULL default ' ',
  "AAAAUNDJ" varchar(2) NOT NULL default ' ',
  "AAAAUNDK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUNSX","AAAAUNSY","AAAAUNSZ","AAABMDXP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNDQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUZBG" varchar(20) NOT NULL default ' ',
  "AAABLCOV" varchar(3) NOT NULL default ' ',
  "AAABLCOW" varchar(2) NOT NULL default ' ',
  "AAABLCOX" varchar(12) NOT NULL default ' ',
  "AAAAUNEG" varchar(3) NOT NULL default ' ',
  "AAAAUNEH" varchar(2) NOT NULL default ' ',
  "AAABETFA" varchar(12) NOT NULL default ' ',
  "AAAAUNDZ" varchar(8) NOT NULL default '00000000',
  "AAAAUNEA" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAAAUZBG","AAABLCOV","AAABLCOW","AAABLCOX","AAAAUNEG","AAAAUNEH","AAABETFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNEJ" (
  "AAABHZDJ" varchar(32) NOT NULL default ' ',
  "AAAAURPM" varchar(1) NOT NULL default ' ',
  "AAAAURPN" varchar(3) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLTLE" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABMGSU" varchar(14) NOT NULL default '00000000000000',
  "AAAAURYG" varchar(10) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAAURSV" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLOZE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNEX" (
  "AAABBZTO" varchar(50) NOT NULL default ' ',
  "AAAAVCVI" varchar(32) NOT NULL default ' ',
  "AAABDABU" varchar(30) NOT NULL default ' ',
  "AAAAVJBE" varchar(12) NOT NULL default ' ',
  "AAAAVCWZ" varchar(14) NOT NULL default '00000000000000',
  "AAAAVJBH" varchar(12) NOT NULL default ' ',
  "AAAAVCXD" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABBZTO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNFG" (
  "AAABBZTO" varchar(50) NOT NULL default ' ',
  "AAAAVCVI" varchar(32) NOT NULL default ' ',
  "AAABDABU" varchar(30) NOT NULL default ' ',
  "AAAAVJBE" varchar(12) NOT NULL default ' ',
  "AAAAVCWZ" varchar(14) NOT NULL default '00000000000000',
  "AAAAVJBH" varchar(12) NOT NULL default ' ',
  "AAAAVCXD" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABBZTO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNFP" (
  "AAAAVCQL" varchar(3) NOT NULL default '000',
  "AAABLRXL" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(36) NOT NULL default ' ',
  PRIMARY KEY ("AAAAVCQL","AAABLRXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNFV" (
  "AAAAUNGJ" varchar(1) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  "AAAAVCQL" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAAAUNGJ","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNGD" (
  "AAAAUNGJ" varchar(1) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIFON" varchar(40) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUNGJ","AAABMNLW","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNGM" (
  "AAAAVCVI" varchar(32) NOT NULL default ' ',
  "AAABDABU" varchar(30) NOT NULL default ' ',
  "AAAAVCRT" varchar(1) NOT NULL default ' ',
  "AAAAUNHL" varchar(1) NOT NULL default ' ',
  "AAABBZTO" varchar(50) NOT NULL default ' ',
  "AAAAVJBE" varchar(12) NOT NULL default ' ',
  "AAAAVCWZ" varchar(14) NOT NULL default '00000000000000',
  "AAAAVJBH" varchar(12) NOT NULL default ' ',
  "AAAAVCXD" varchar(14) NOT NULL default '00000000000000',
  "AAAAUNHR" varchar(8) NOT NULL default '00000000',
  "AAABHWXD" varchar(8) NOT NULL default ' ',
  "AAAAUNHT" VARBINARY(255),
  "AAAAUNHU" VARBINARY(255),
  "AAAAUNHV" VARBINARY(255),
  "AAABIIDV" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAAAVCVI"),
  KEY "AAAAUNHD" ("AAABBZTO"),
  KEY "AAAAUNHF" ("AAABDABU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNHH" (
  "AAAAVCVI" varchar(32) NOT NULL default ' ',
  "AAABDABU" varchar(30) NOT NULL default ' ',
  "AAAAVCRT" varchar(1) NOT NULL default ' ',
  "AAAAUNHL" varchar(1) NOT NULL default ' ',
  "AAABBZTO" varchar(50) NOT NULL default ' ',
  "AAAAVJBE" varchar(12) NOT NULL default ' ',
  "AAAAVCWZ" varchar(14) NOT NULL default '00000000000000',
  "AAAAVJBH" varchar(12) NOT NULL default ' ',
  "AAAAVCXD" varchar(14) NOT NULL default '00000000000000',
  "AAAAUNHR" varchar(8) NOT NULL default '00000000',
  "AAABHWXD" varchar(8) NOT NULL default ' ',
  "AAAAUNHT" VARBINARY(255),
  "AAAAUNHU" VARBINARY(255),
  "AAAAUNHV" VARBINARY(255),
  "AAABIIDV" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAAAVCVI"),
  KEY "AAAAUNHY" ("AAABBZTO"),
  KEY "AAAAUNIA" ("AAABDABU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNIC" (
  "AAAAVCVI" varchar(32) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMKFH" varchar(32) NOT NULL default ' ',
  "AAAAUNIM" varchar(4) NOT NULL default '0000',
  "AAAAUNIH" varchar(4) NOT NULL default '0000',
  "AAAAUNII" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAVCVI","AAABMLQA","AAABMKFH","AAAAUNIM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNIN" (
  "AAAAUNIS" varchar(32) NOT NULL default ' ',
  "AAAAUNIP" varchar(1) NOT NULL default ' ',
  "AAAAUNIQ" varchar(32) NOT NULL default ' ',
  "AAAAUNIR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUNIS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNIT" (
  "AAAAUNIX" varchar(2) NOT NULL default '00',
  "AAABGZSJ" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUNIX","AAABGZSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNIZ" (
  "AAAAVCVI" varchar(32) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMKFH" varchar(32) NOT NULL default ' ',
  "AAAAVCQL" varchar(4) NOT NULL default '0000',
  "AAAAUNKD" varchar(1) NOT NULL default ' ',
  "AAAAVCUF" varchar(4) NOT NULL default ' ',
  "AAAAUNKF" varchar(12) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABJAAI" varchar(10) NOT NULL default ' ',
  "AAAAUNKI" varchar(30) NOT NULL default ' ',
  "AAAAVJBE" varchar(12) NOT NULL default ' ',
  "AAAAVCWZ" varchar(14) NOT NULL default '00000000000000',
  "AAAAVJBH" varchar(12) NOT NULL default ' ',
  "AAAAVCXD" varchar(14) NOT NULL default '00000000000000',
  "AAAAVCYL" varchar(1) NOT NULL default ' ',
  "AAAAUNKO" varchar(1) NOT NULL default ' ',
  "AAAAUNKP" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABJSDF" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAAVCVI","AAABMLQA","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNJY" (
  "AAAAVCVI" varchar(32) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMKFH" varchar(32) NOT NULL default ' ',
  "AAAAVCQL" varchar(4) NOT NULL default '0000',
  "AAAAUNKD" varchar(1) NOT NULL default ' ',
  "AAAAVCUF" varchar(4) NOT NULL default ' ',
  "AAAAUNKF" varchar(12) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABJAAI" varchar(10) NOT NULL default ' ',
  "AAAAUNKI" varchar(30) NOT NULL default ' ',
  "AAAAVJBE" varchar(12) NOT NULL default ' ',
  "AAAAVCWZ" varchar(14) NOT NULL default '00000000000000',
  "AAAAVJBH" varchar(12) NOT NULL default ' ',
  "AAAAVCXD" varchar(14) NOT NULL default '00000000000000',
  "AAAAVCYL" varchar(1) NOT NULL default ' ',
  "AAAAUNKO" varchar(1) NOT NULL default ' ',
  "AAAAUNKP" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABKERC" varchar(20) NOT NULL default ' ',
  "AAABJSDF" smallint(6) NOT NULL default '0',
  "AAABMRCK" longtext,
  PRIMARY KEY ("AAAAVCVI","AAABMLQA","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNKY" (
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(120) NOT NULL default ' ',
  "AAAAVCVI" varchar(32) NOT NULL default ' ',
  "AAAAVCQL" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLRST","AAABMKFH","AAABLFYD","AAAAVCVI","AAAAVCQL"),
  KEY "AAAAUNLJ" ("AAAAVCVI","AAAAVCQL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNLM" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABHBPB" varchar(1) NOT NULL default ' ',
  "AAABHBPC" varchar(1) NOT NULL default ' ',
  "AAABIJZP" varchar(1) NOT NULL default ' ',
  "AAABGMZC" varchar(1) NOT NULL default ' ',
  "AAAAVMBC" varchar(1) NOT NULL default ' ',
  "AAAAUNLT" varchar(1) NOT NULL default ' ',
  "AAAAUNLU" varchar(1) NOT NULL default ' ',
  "AAAAUNLV" varchar(1) NOT NULL default ' ',
  "AAABMAYQ" varchar(3) NOT NULL default '000',
  "AAAAUNLX" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNLZ" (
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(120) NOT NULL default ' ',
  "AAAAVCVI" varchar(32) NOT NULL default ' ',
  "AAAAVCQL" varchar(4) NOT NULL default '0000',
  "AAABEXFJ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLRST","AAABMKFH","AAABLFYD","AAAAVCVI","AAAAVCQL"),
  KEY "AAAAUNML" ("AAAAVCVI","AAAAVCQL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNMO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAVYXR" varchar(12) NOT NULL default ' ',
  "AAAAUNSX" varchar(3) NOT NULL default ' ',
  "AAAAUNSY" varchar(2) NOT NULL default ' ',
  "AAAAUNSZ" varchar(12) NOT NULL default ' ',
  "AAABHBVV" varchar(12) NOT NULL default ' ',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAVYXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNMY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHBVV" varchar(12) NOT NULL default ' ',
  "AAAAUNSX" varchar(3) NOT NULL default ' ',
  "AAAAUNSY" varchar(2) NOT NULL default ' ',
  "AAAAUNSZ" varchar(12) NOT NULL default ' ',
  "AAAAVYXR" varchar(12) NOT NULL default ' ',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHBVV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNNI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUNSX" varchar(3) NOT NULL default ' ',
  "AAAAUNSY" varchar(2) NOT NULL default ' ',
  "AAAAUNSZ" varchar(12) NOT NULL default ' ',
  "AAAAUNNN" varchar(3) NOT NULL default ' ',
  "AAAAUNNO" varchar(2) NOT NULL default ' ',
  "AAAAUNNP" varchar(12) NOT NULL default ' ',
  "AAAAUNNQ" varchar(12) NOT NULL default ' ',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABHESL" varchar(6) NOT NULL default '000000',
  "AAAAUNNT" varchar(3) NOT NULL default ' ',
  "AAAAUNNU" varchar(2) NOT NULL default ' ',
  "AAAAUNNV" varchar(12) NOT NULL default ' ',
  "AAAAUNNW" varchar(12) NOT NULL default ' ',
  "AAABJFLQ" varchar(8) NOT NULL default '00000000',
  "AAABHFNY" varchar(6) NOT NULL default '000000',
  "AAABHBVV" varchar(12) NOT NULL default ' ',
  "AAAAVYXR" varchar(12) NOT NULL default ' ',
  "AAAAUNOB" varchar(8) NOT NULL default '00000000',
  "AAAAUNOC" VARBINARY(255),
  "AAAAUNOD" VARBINARY(255),
  "AAAAUNOE" VARBINARY(255),
  "AAAAUNOF" varchar(1) NOT NULL default ' ',
  "AAAAUNOG" varchar(1) NOT NULL default ' ',
  "AAAAUNOH" varchar(1) NOT NULL default ' ',
  "AAAAUNOI" varchar(1) NOT NULL default ' ',
  "AAAAUNOJ" varchar(1) NOT NULL default ' ',
  "AAAAUNOK" varchar(1) NOT NULL default ' ',
  "AAAAUNOL" varchar(1) NOT NULL default ' ',
  "AAAAUNOM" varchar(1) NOT NULL default ' ',
  "AAAAUNON" VARBINARY(255),
  "AAAAUNOO" VARBINARY(255),
  "AAAAUNOP" VARBINARY(255),
  "AAAAUNOQ" VARBINARY(255),
  "AAAAUNOR" VARBINARY(255),
  "AAABBEDR" varchar(1) NOT NULL default ' ',
  "AAAAUNOT" varchar(3) NOT NULL default ' ',
  "AAAAYFIA" varchar(1) NOT NULL default ' ',
  "AAAAVYXE" varchar(1) NOT NULL default ' ',
  "AAAAUNOW" varchar(1) NOT NULL default ' ',
  "AAAAUNOX" varchar(1) NOT NULL default ' ',
  "AAAAUNOY" varchar(1) NOT NULL default ' ',
  "AAAAUNOZ" varchar(1) NOT NULL default ' ',
  "AAAAUNPA" varchar(1) NOT NULL default ' ',
  "AAAAUNPB" varchar(2) NOT NULL default ' ',
  "AAAAUNPC" varchar(12) NOT NULL default ' ',
  "AAAAUNPD" varchar(2) NOT NULL default ' ',
  "AAAAUNPE" varchar(12) NOT NULL default ' ',
  "AAAAUNPF" varchar(2) NOT NULL default ' ',
  "AAAAUNPG" varchar(12) NOT NULL default ' ',
  "AAAAUNPH" varchar(2) NOT NULL default ' ',
  "AAAAUNPI" varchar(12) NOT NULL default ' ',
  "AAAAUNPJ" varchar(2) NOT NULL default ' ',
  "AAAAUNPK" varchar(12) NOT NULL default ' ',
  "AAAAUNPL" varchar(2) NOT NULL default ' ',
  "AAAAUNPM" varchar(12) NOT NULL default ' ',
  "AAAAUNPN" varchar(2) NOT NULL default ' ',
  "AAAAUNPO" varchar(12) NOT NULL default ' ',
  "AAAAUNPP" varchar(2) NOT NULL default ' ',
  "AAAAUNPQ" varchar(12) NOT NULL default ' ',
  "AAAAUNPR" varchar(2) NOT NULL default ' ',
  "AAABCNWE" varchar(12) NOT NULL default ' ',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAAAUNPU" varchar(1) NOT NULL default ' ',
  "AAAAUNPV" varchar(2) NOT NULL default ' ',
  "AAAAUNRY" varchar(10) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAAAUNPY" varchar(3) NOT NULL default ' ',
  "AAABJHGJ" varchar(1) NOT NULL default ' ',
  "AAABJHGK" varchar(1) NOT NULL default ' ',
  "AAABIKRS" varchar(1) NOT NULL default ' ',
  "AAABHALW" varchar(1) NOT NULL default ' ',
  "AAABLNZF" varchar(3) NOT NULL default '000',
  "AAAAUNQE" varchar(8) NOT NULL default '00000000',
  "AAAAUNQF" varchar(8) NOT NULL default '00000000',
  "AAAAUNQG" varchar(1) NOT NULL default ' ',
  "AAAAUNQH" varchar(1) NOT NULL default ' ',
  "AAAAUNQI" varchar(1) NOT NULL default ' ',
  "AAAAUNQJ" varchar(1) NOT NULL default ' ',
  "AAAAUNQK" varchar(2) NOT NULL default ' ',
  "AAAAUNQL" varchar(12) NOT NULL default ' ',
  "AAAAUNQM" varchar(8) NOT NULL default '00000000',
  "AAAAUNQN" varchar(8) NOT NULL default '00000000',
  "AAAAUNQO" varchar(16) NOT NULL default ' ',
  "AAAAUNQP" varchar(16) NOT NULL default ' ',
  "AAAAUNQQ" varchar(1) NOT NULL default ' ',
  "AAAAUNQR" varchar(1) NOT NULL default ' ',
  "AAAAUNQS" varchar(1) NOT NULL default ' ',
  "AAAAUNQT" varchar(1) NOT NULL default ' ',
  "AAAAUNQU" varchar(1) NOT NULL default ' ',
  "AAAAUNQV" varchar(1) NOT NULL default ' ',
  "AAAAUNQW" varchar(1) NOT NULL default ' ',
  "AAAAUNQX" varchar(1) NOT NULL default ' ',
  "AAAAUNQY" varchar(1) NOT NULL default ' ',
  "AAAAUNQZ" varchar(3) NOT NULL default ' ',
  "AAAAUNRA" varchar(8) NOT NULL default '00000000',
  "AAAAUNRB" varchar(2) NOT NULL default ' ',
  "AAAAUNRC" varchar(12) NOT NULL default ' ',
  "AAAAUNRD" varchar(2) NOT NULL default ' ',
  "AAAAUNRE" varchar(12) NOT NULL default ' ',
  "AAAAUNRF" varchar(2) NOT NULL default ' ',
  "AAAAUNRG" varchar(12) NOT NULL default ' ',
  "AAAAUNRH" varchar(2) NOT NULL default ' ',
  "AAAAUNRI" varchar(12) NOT NULL default ' ',
  "AAAAUNRJ" varchar(1) NOT NULL default ' ',
  "AAAAUNRK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUNSX","AAAAUNSY","AAAAUNSZ"),
  UNIQUE KEY "AAAAUNRP" ("AAABMRIL","AAAAUNSX","AAAAUNSY","AAAAUNSZ","AAABMPMT","AAAAVYXR","AAABHBVV"),
  KEY "AAAAUNRX" ("AAAAUNRY"),
  KEY "AAAAUNRZ" ("AAAAVYXR"),
  KEY "AAAAUNSB" ("AAABHBVV"),
  KEY "AAAAUNSD" ("AAAAUNSZ","AAAAUNSX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNSG" (
  "AAAAUNSS" varchar(12) NOT NULL default ' ',
  "AAABMRCU" varchar(12) NOT NULL default ' ',
  "AAAAUNSJ" varchar(110) NOT NULL default ' ',
  "AAAAUNSK" varchar(32) NOT NULL default ' ',
  "AAABLEEW" varchar(35) NOT NULL default ' ',
  "AAABLQEI" varchar(1) NOT NULL default ' ',
  "AAABLQEJ" varchar(4) NOT NULL default ' ',
  "AAAAUNSO" varchar(255) NOT NULL default ' ',
  "AAAAUNSP" varchar(255) NOT NULL default ' ',
  "AAAAUNSQ" varchar(110) NOT NULL default ' ',
  "AAAAUNSR" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUNSS","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNSU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAUNSX" varchar(3) NOT NULL default ' ',
  "AAAAUNSY" varchar(2) NOT NULL default ' ',
  "AAAAUNSZ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNTC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZLD" varchar(12) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default ' ',
  "AAAAUNTG" varchar(1) NOT NULL default ' ',
  "AAABIZCH" varchar(8) NOT NULL default '00000000',
  "AAABGBTN" varchar(6) NOT NULL default '000000',
  "AAAAUNTJ" varchar(8) NOT NULL default '00000000',
  "AAABGFNS" varchar(6) NOT NULL default '000000',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABJTNV" varchar(70) NOT NULL default ' ',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAAAUNTV" varchar(1) NOT NULL default ' ',
  "AAAAUNTW" varchar(1) NOT NULL default ' ',
  "AAAAUNTX" varchar(1) NOT NULL default ' ',
  "AAABMECK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZLD","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNUC" (
  "AAABLGCM" varchar(30) NOT NULL default ' ',
  "AAABJECS" varchar(60) NOT NULL default ' ',
  "AAABHBVL" varchar(1) NOT NULL default ' ',
  "AAABLKNN" varchar(30) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAAAUNVJ" varchar(8) NOT NULL default '00000000',
  "AAAAUNVK" varchar(6) NOT NULL default '000000',
  "AAAAUNUK" varchar(4) NOT NULL default ' ',
  "AAAAUNUL" varchar(1) NOT NULL default ' ',
  "AAAAUNUM" varchar(6) NOT NULL default '000000',
  "AAAAUNUN" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLGCM","AAABJECS","AAABHBVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNUR" (
  "AAABLGCM" varchar(30) NOT NULL default ' ',
  "AAABMKYH" varchar(14) NOT NULL default ' ',
  "AAABJECS" varchar(60) NOT NULL default ' ',
  "AAABEETX" varchar(5) NOT NULL default '00000',
  "AAAAUNYF" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGCM","AAABMKYH","AAABJECS","AAABEETX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNVB" (
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAAAUNVN" varchar(60) NOT NULL default ' ',
  "AAAAUNVE" varchar(30) NOT NULL default ' ',
  "AAAAUNVF" varchar(30) NOT NULL default ' ',
  "AAAAUNVG" varchar(30) NOT NULL default ' ',
  "AAAAUNVH" varchar(30) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAAAUNVJ" varchar(8) NOT NULL default '00000000',
  "AAAAUNVK" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLTVR"),
  KEY "AAAAUNVM" ("AAAAUNVN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNVO" (
  "AAABLGCM" varchar(30) NOT NULL default ' ',
  "AAABJECS" varchar(60) NOT NULL default ' ',
  "AAABHBVL" varchar(1) NOT NULL default ' ',
  "AAABLUEO" varchar(61) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGCM","AAABJECS","AAABHBVL","AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNVX" (
  "AAABLGCM" varchar(30) NOT NULL default ' ',
  "AAABLKNN" varchar(30) NOT NULL default ' ',
  "AAABMKXF" varchar(30) NOT NULL default ' ',
  "AAABLPJD" varchar(8) NOT NULL default ' ',
  "AAABEETX" varchar(5) NOT NULL default '00000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUNWG" varchar(4) NOT NULL default '0000',
  "AAAAUNWH" varchar(6) NOT NULL default '000000',
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  "AAABJEDB" varchar(30) NOT NULL default ' ',
  "AAABLBEI" varchar(6) NOT NULL default '000000',
  "AAABJTEQ" varchar(6) NOT NULL default '000000',
  "AAABMLMY" varchar(6) NOT NULL default '000000',
  "AAABMNMM" varchar(6) NOT NULL default '000000',
  "AAABLDNU" varchar(4) NOT NULL default ' ',
  "AAABLUWW" varchar(1) NOT NULL default ' ',
  "AAABCXXG" varchar(30) NOT NULL default ' ',
  "AAABMKXX" varchar(30) NOT NULL default ' ',
  "AAABHCYR" varchar(30) NOT NULL default ' ',
  "AAAAUNWU" varchar(3) NOT NULL default ' ',
  "AAABBVNZ" varchar(20) NOT NULL default ' ',
  "AAAAUNWW" varchar(1) NOT NULL default ' ',
  "AAABGYPG" varchar(20) NOT NULL default ' ',
  "AAAAUNWY" varchar(4) NOT NULL default '0000',
  "AAABHVRR" varchar(5) NOT NULL default ' ',
  "AAABDSOB" varchar(2) NOT NULL default '00',
  "AAAAUNXB" varchar(2) NOT NULL default '00',
  "AAAAUNXC" varchar(2) NOT NULL default '00',
  "AAAAUNXD" varchar(2) NOT NULL default '00',
  "AAABHHTI" varchar(60) NOT NULL default ' ',
  "AAABLAUS" varchar(55) NOT NULL default ' ',
  "AAABKGYO" varchar(10) NOT NULL default ' ',
  "AAABKGYP" varchar(20) NOT NULL default ' ',
  "AAABKGYQ" varchar(40) NOT NULL default ' ',
  "AAABIRTO" varchar(1) NOT NULL default ' ',
  "AAABJEDH" varchar(1) NOT NULL default ' ',
  "AAAAUNXL" varchar(1) NOT NULL default ' ',
  "AAAAUNXM" varchar(1) NOT NULL default ' ',
  "AAAAUNXN" varchar(1) NOT NULL default ' ',
  "AAABEGEB" varchar(1) NOT NULL default ' ',
  "AAAAUNXP" varchar(1) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAAAUNXR" varchar(1) NOT NULL default ' ',
  "AAABLTXS" varchar(1) NOT NULL default ' ',
  "AAAAVQDY" varchar(1) NOT NULL default ' ',
  "AAABJHBK" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGCM","AAABLKNN","AAABMKXF","AAABLPJD","AAABEETX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNYA" (
  "AAABLGCM" varchar(30) NOT NULL default ' ',
  "AAABMKYH" varchar(14) NOT NULL default ' ',
  "AAABJECS" varchar(60) NOT NULL default ' ',
  "AAABEETX" varchar(5) NOT NULL default '00000',
  "AAAAUNYF" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGCM","AAABMKYH","AAABJECS","AAABEETX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLDBD" varchar(10) NOT NULL default ' ',
  "AAABJWRR" varchar(3) NOT NULL default ' ',
  "AAABMGVL" varchar(15) NOT NULL default ' ',
  "AAABMGVM" varchar(18) NOT NULL default ' ',
  "AAABJWRQ" varchar(2) NOT NULL default ' ',
  "AAABMCTK" varchar(4) NOT NULL default ' ',
  "AAAAWFTV" varchar(1) NOT NULL default ' ',
  "AAAAUNYT" varchar(1) NOT NULL default ' ',
  "AAAAUNYU" varchar(12) NOT NULL default ' ',
  "AAAAUNYV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLDBD","AAABJWRR","AAABMGVL","AAABMGVM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNZB" (
  "AAABIOAE" varchar(12) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMLMZ" int(11) NOT NULL default '0',
  "AAABMLNA" int(11) NOT NULL default '0',
  "AAABMLNB" varchar(1) NOT NULL default ' ',
  "AAABMLNC" varchar(1) NOT NULL default ' ',
  "AAABHVTT" varchar(40) NOT NULL default ' ',
  "AAABKGYQ" varchar(40) NOT NULL default ' ',
  "AAABKGYP" varchar(20) NOT NULL default ' ',
  "AAABKGYO" varchar(10) NOT NULL default ' ',
  "AAABFPPK" varchar(1) NOT NULL default ' ',
  "AAABEZDS" varchar(1) NOT NULL default ' ',
  "AAABEZDR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIOAE","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUNZT" (
  "AAABIOAE" varchar(12) NOT NULL default ' ',
  "AAABCYFE" varchar(10) NOT NULL default ' ',
  "AAAAUOAA" varchar(4) NOT NULL default ' ',
  "AAABMMJT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABIOAE","AAABCYFE","AAAAUOAA","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOAC" (
  "AAABIOAE" varchar(12) NOT NULL default ' ',
  "AAABCYFE" varchar(10) NOT NULL default ' ',
  "AAAAUOAF" varchar(1) NOT NULL default ' ',
  "AAAAUQQN" tinyint(3) unsigned NOT NULL default '0',
  "AAABMALF" varchar(1) NOT NULL default ' ',
  "AAAAUQQP" varchar(1) NOT NULL default ' ',
  "AAAAUQQQ" tinyint(3) unsigned NOT NULL default '0',
  "AAAAUQQR" varchar(3) NOT NULL default ' ',
  "AAABENKB" varchar(3) NOT NULL default ' ',
  "AAAAUQQT" varchar(50) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAAAUQUY" decimal(15,0) NOT NULL default '0',
  "AAAAUQUZ" varchar(4) NOT NULL default ' ',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAAAUQVB" decimal(15,0) NOT NULL default '0',
  "AAAAUQVC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABIOAE","AAABCYFE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOAV" (
  "AAABIOAE" varchar(12) NOT NULL default ' ',
  "AAABFPQD" varchar(2) NOT NULL default '00',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABKRAD" varchar(1) NOT NULL default ' ',
  "AAABKRAE" varchar(1) NOT NULL default ' ',
  "AAAAVBUX" varchar(2) NOT NULL default ' ',
  "AAABEZHB" varchar(1) NOT NULL default ' ',
  "AAABEZHC" varchar(1) NOT NULL default ' ',
  "AAABEZHD" varchar(1) NOT NULL default ' ',
  "AAABLAUT" varchar(40) NOT NULL default ' ',
  "AAABLISJ" varchar(1) NOT NULL default ' ',
  "AAABLPVW" int(11) NOT NULL default '0',
  "AAABMLNB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIOAE","AAABFPQD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOBL" (
  "AAABLEKS" varchar(12) NOT NULL default ' ',
  "AAABLSBY" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOBP" (
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAAAUODH" varchar(64) NOT NULL default ' ',
  "AAABIABC" varchar(8) NOT NULL default '00000000',
  "AAAAUODJ" varchar(8) NOT NULL default '00000000',
  "AAABBRLC" varchar(64) NOT NULL default ' ',
  "AAABLSBY" varchar(255) NOT NULL default ' ',
  "AAAAUODB" varchar(1) NOT NULL default ' ',
  "AAAAUODC" varchar(1) NOT NULL default ' ',
  "AAABJHGJ" varchar(1) NOT NULL default ' ',
  "AAABJHGK" varchar(1) NOT NULL default ' ',
  "AAABIKRS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLZKF","AAAAUODH","AAABIABC","AAAAUODJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOCF" (
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLGCZ" varchar(30) NOT NULL default ' ',
  "AAABLGDB" varchar(64) NOT NULL default ' ',
  "AAABMBIU" varchar(80) NOT NULL default ' ',
  "AAAAUODB" varchar(1) NOT NULL default ' ',
  "AAAAUODC" varchar(1) NOT NULL default ' ',
  "AAABJHGJ" varchar(1) NOT NULL default ' ',
  "AAABJHGK" varchar(1) NOT NULL default ' ',
  "AAABIKRS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLZKF","AAABMQDU","AAABLGCZ","AAABLGDB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOCU" (
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAAAUODH" varchar(64) NOT NULL default ' ',
  "AAABIABC" varchar(8) NOT NULL default '00000000',
  "AAAAUODJ" varchar(8) NOT NULL default '00000000',
  "AAABBRLC" varchar(64) NOT NULL default ' ',
  "AAABLSBY" varchar(255) NOT NULL default ' ',
  "AAAAUODB" varchar(1) NOT NULL default ' ',
  "AAAAUODC" varchar(1) NOT NULL default ' ',
  "AAABJHGJ" varchar(1) NOT NULL default ' ',
  "AAABJHGK" varchar(1) NOT NULL default ' ',
  "AAABIKRS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLZKF","AAAAUODH","AAABIABC","AAAAUODJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUODK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUOEI" varchar(1) NOT NULL default ' ',
  "AAABMBXY" varchar(30) NOT NULL default ' ',
  "AAAAUOFW" varchar(30) NOT NULL default ' ',
  "AAAAUODP" varchar(255) NOT NULL default ' ',
  "AAAAVHIH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUOEI","AAABMBXY","AAAAUOFW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUODV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUOEI" varchar(1) NOT NULL default ' ',
  "AAABMBXY" varchar(30) NOT NULL default ' ',
  "AAAAUOFW" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABFOKP" varchar(22) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABJTEQ" varchar(6) NOT NULL default '000000',
  "AAABGBAO" smallint(6) NOT NULL default '0',
  "AAABEFTK" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUOEI","AAABMBXY","AAAAUOFW","AAABMREF","AAABFOKP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOEN" (
  "AAAAUOFW" varchar(30) NOT NULL default ' ',
  "AAAAUOEP" varchar(30) NOT NULL default ' ',
  "AAAAUOEQ" varchar(30) NOT NULL default ' ',
  "AAAAUOER" varchar(30) NOT NULL default ' ',
  "AAABLMNC" varchar(24) NOT NULL default ' ',
  "AAABLDNU" varchar(30) NOT NULL default ' ',
  "AAAAUOEU" varchar(1) NOT NULL default ' ',
  "AAAAUOEV" varchar(1) NOT NULL default ' ',
  "AAAAUOEW" varchar(1) NOT NULL default ' ',
  "AAABJHCT" varchar(1) NOT NULL default ' ',
  "AAAAUOEY" varchar(1) NOT NULL default ' ',
  "AAAAUOEZ" varchar(1) NOT NULL default ' ',
  "AAABJHGJ" varchar(1) NOT NULL default ' ',
  "AAABJHGK" varchar(1) NOT NULL default ' ',
  "AAABIKRS" varchar(1) NOT NULL default ' ',
  "AAABIKRT" varchar(1) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABJSNO" varchar(30) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAAUOFW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOFJ" (
  "AAAAUOFW" varchar(30) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUOFW","AAABMOHM","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOFR" (
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAAAUOFW" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNJW","AAAAUOFW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOFX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJTDU" varchar(3) NOT NULL default ' ',
  "AAABJTDV" varchar(4) NOT NULL default '0000',
  "AAAAUOGB" varchar(3) NOT NULL default ' ',
  "AAABJTDJ" varchar(20) NOT NULL default ' ',
  "AAABJTDK" varchar(3) NOT NULL default ' ',
  "AAAAUOGE" varchar(3) NOT NULL default ' ',
  "AAABJTDL" varchar(20) NOT NULL default ' ',
  "AAABJTDM" varchar(3) NOT NULL default ' ',
  "AAABJTDN" int(11) NOT NULL default '0',
  "AAABJTDO" varchar(6) NOT NULL default '000000',
  "AAABJTDP" varchar(6) NOT NULL default '000000',
  "AAABJTDQ" decimal(9,4) NOT NULL default '0.0000',
  "AAABJTDR" varchar(3) NOT NULL default ' ',
  "AAABJTDS" varchar(1) NOT NULL default ' ',
  "AAABLUTZ" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABJTDU","AAABJTDV"),
  KEY "AAAAUOGR" ("AAABMRIL","AAABJTDJ","AAABJTDL"),
  KEY "AAAAUOGV" ("AAABJTDO"),
  KEY "AAAAUOGX" ("AAABMRIL","AAABJTDK","AAABJTDM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOHB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUOOA" varchar(10) NOT NULL default ' ',
  "AAABJAAI" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUOOA","AAABJAAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOHI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUOOA" varchar(10) NOT NULL default ' ',
  "AAAAVAJL" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUOOA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOHO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUOOA" varchar(10) NOT NULL default ' ',
  "AAAAUOJX" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUOOA","AAAAUOJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOHV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAAAUOHY" varchar(30) NOT NULL default ' ',
  "AAAAUOHZ" varchar(30) NOT NULL default ' ',
  "AAAAUOIA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOID" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUOIK" varchar(4) NOT NULL default ' ',
  "AAABLLLP" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAAAUOIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOIL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUOJO" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLP" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUOJO","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOIT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUOJO" varchar(10) NOT NULL default ' ',
  "AAABAWKR" varchar(30) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUOJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOJA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUOPD" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLP" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUOPD","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOJI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUOPD" varchar(10) NOT NULL default ' ',
  "AAAAUOJO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUOPD","AAAAUOJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOJP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUOMU" varchar(10) NOT NULL default ' ',
  "AAABJAAI" varchar(10) NOT NULL default ' ',
  "AAAAUOJX" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUOMU","AAABJAAI","AAAAUOJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOJY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUOMU" varchar(10) NOT NULL default ' ',
  "AAAAUOKB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUOMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOKE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUOMU" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLP" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUOMU","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOKM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABILXJ" varchar(20) NOT NULL default '00000000000000000000',
  "AAABAWKR" varchar(10) NOT NULL default ' ',
  "AAABLITM" varchar(6) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABIGIJ" varchar(1) NOT NULL default ' ',
  "AAAAUOKT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABILXJ","AAABAWKR","AAABLITM"),
  UNIQUE KEY "AAAAUOKY" ("AAABLUXQ","AAABAWKR","AAABLITM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOLC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABERBG" varchar(1) NOT NULL default ' ',
  "AAABLUEO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABERBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOLI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABILXJ" varchar(36) NOT NULL default ' ',
  "AAABAWKR" varchar(10) NOT NULL default ' ',
  "AAABLITM" varchar(6) NOT NULL default ' ',
  "AAAAUOLN" varchar(1) NOT NULL default ' ',
  "AAAAUOLO" varchar(1) NOT NULL default ' ',
  "AAAAUOLP" varchar(1) NOT NULL default ' ',
  "AAAAUOLQ" varchar(1) NOT NULL default ' ',
  "AAAAUOPN" varchar(12) NOT NULL default ' ',
  "AAAAUOLS" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAUOLT" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABILXJ"),
  UNIQUE KEY "AAAAUOLW" ("AAABLUXQ","AAABAWKR","AAABLITM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOMA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABLITM" varchar(6) NOT NULL default ' ',
  "AAAAVQZV" varchar(8) NOT NULL default '00000000',
  "AAAAUOMF" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUXQ","AAABLTVR","AAABLITM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOMJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLITM" varchar(6) NOT NULL default ' ',
  "AAABGFCP" varchar(32) NOT NULL default ' ',
  "AAAAUOMN" varchar(1) NOT NULL default ' ',
  "AAABMMYW" varchar(24) NOT NULL default ' ',
  "AAAAUONP" tinyint(3) unsigned NOT NULL default '0',
  "AAAAUOMQ" varchar(1) NOT NULL default ' ',
  "AAAAUOMR" varchar(8) NOT NULL default ' ',
  "AAAAUOMS" varchar(1) NOT NULL default ' ',
  "AAAAUOMT" varchar(1) NOT NULL default ' ',
  "AAAAUOMU" varchar(10) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAAAUOMW" varchar(1) NOT NULL default ' ',
  "AAAAUOMX" varchar(1) NOT NULL default ' ',
  "AAAAUOMY" varchar(1) NOT NULL default ' ',
  "AAAAUOMZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLITM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUONC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLITM" varchar(6) NOT NULL default ' ',
  "AAABLLLP" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABLITM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUONK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLHNP" varchar(25) NOT NULL default ' ',
  "AAABMMYW" varchar(24) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAAAUONP" tinyint(3) unsigned NOT NULL default '0',
  "AAAAUONQ" varchar(1) NOT NULL default ' ',
  "AAABBZWX" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLHNP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUONU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLHNP" varchar(25) NOT NULL default ' ',
  "AAAAUOOA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLHNP","AAAAUOOA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOOB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLHNP" varchar(25) NOT NULL default ' ',
  "AAABJAAI" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLHNP","AAABJAAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOOI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLHNP" varchar(25) NOT NULL default ' ',
  "AAABLITM" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLHNP","AAABLITM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOOP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUOPH" varchar(12) NOT NULL default ' ',
  "AAAAUOOS" varchar(1) NOT NULL default ' ',
  "AAAAUOOT" varchar(1) NOT NULL default ' ',
  "AAABIOAH" varchar(1) NOT NULL default ' ',
  "AAAAUOOV" varchar(1) NOT NULL default ' ',
  "AAAAUOOW" varchar(14) NOT NULL default ' ',
  "AAAAUOOX" varchar(1) NOT NULL default ' ',
  "AAAAUOOY" varchar(1) NOT NULL default ' ',
  "AAAAUOOZ" varchar(6) NOT NULL default '000000',
  "AAAAUOPA" varchar(1) NOT NULL default ' ',
  "AAAAUOPB" varchar(2) NOT NULL default '00',
  "AAABHCXI" varchar(1) NOT NULL default ' ',
  "AAAAUOPD" varchar(10) NOT NULL default ' ',
  "AAAAUOPE" varchar(15) NOT NULL default ' ',
  "AAAAUOPF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUOPH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOPI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABILXJ" varchar(20) NOT NULL default '00000000000000000000',
  "AAABAWKR" varchar(10) NOT NULL default ' ',
  "AAABLITM" varchar(6) NOT NULL default ' ',
  "AAAAUOPN" varchar(12) NOT NULL default ' ',
  "AAAAUOPO" varchar(12) NOT NULL default ' ',
  "AAABHBVI" varchar(255) NOT NULL default ' ',
  "AAAAUOPQ" varchar(1) NOT NULL default ' ',
  "AAAAUOPR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABILXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOPU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABMLPO" varchar(2) NOT NULL default '00',
  "AAABAWKR" varchar(30) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABMLPV" varchar(55) NOT NULL default ' ',
  "AAABININ" varchar(55) NOT NULL default ' ',
  "AAABJDMJ" varchar(55) NOT NULL default ' ',
  "AAABILWE" varchar(10) NOT NULL default ' ',
  "AAABILWD" varchar(10) NOT NULL default ' ',
  "AAABHAEW" varchar(1) NOT NULL default ' ',
  "AAAAUOQG" varchar(140) NOT NULL default ' ',
  "AAAAUOQH" varchar(140) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLZKF","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOQL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABMALF" varchar(1) NOT NULL default ' ',
  "AAAAUOQP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLZKF","AAABMALF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOQT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLDBD" varchar(10) NOT NULL default ' ',
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  "AAABLDAU" varchar(2) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLDBD","AAABLXWK","AAABLDAU","AAABLCPX"),
  UNIQUE KEY "AAAAUORF" ("AAABMRIL","AAABMCSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUORI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUORP" varchar(10) NOT NULL default ' ',
  "AAAAUORL" tinyint(3) unsigned NOT NULL default '0',
  "AAAAUORM" tinyint(3) unsigned NOT NULL default '0',
  "AAAAUORN" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAUORP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUORQ" (
  "AAAAUOSA" varchar(4) NOT NULL default ' ',
  "AAABMKKB" varchar(8) NOT NULL default '00000000',
  "AAAAUORT" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUOSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUORV" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUOSA" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAUOSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOSB" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOSI" (
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
CREATE TABLE "AAAAUOST" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOTD" (
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
  KEY "AAAAUOTR" ("AAABMRFO","AAABMLQA","AAABHZDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOTV" (
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
  PRIMARY KEY ("AAABLLLR"),
  KEY "AAAAUOUO" ("AAABLLLJ"),
  KEY "AAAAUOUQ" ("AAABLLLH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOUS" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOUZ" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUOVJ" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOVN" (
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
  UNIQUE KEY "AAAAUOWA" ("AAABLLNL"),
  KEY "AAAAUOWC" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOWE" (
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
  UNIQUE KEY "AAAAUOWR" ("AAABLLNL"),
  KEY "AAAAUOWT" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOWV" (
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
CREATE TABLE "AAAAUOXH" (
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
  UNIQUE KEY "AAAAUOXV" ("AAABLLNL"),
  KEY "AAAAUOXX" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOXZ" (
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
  PRIMARY KEY ("AAABHZJH"),
  KEY "AAAAUOYZ" ("AAABLLNH","AAABHZJH"),
  KEY "AAAAUOZC" ("AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOZE" (
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
  UNIQUE KEY "AAAAUOZS" ("AAABLLNL"),
  KEY "AAAAUOZU" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUOZW" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUPAG" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPAK" (
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
  UNIQUE KEY "AAAAUPAX" ("AAABLLNL"),
  KEY "AAAAUPAZ" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPBB" (
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
  UNIQUE KEY "AAAAUPBO" ("AAABLLNL"),
  KEY "AAAAUPBQ" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPBS" (
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(30) NOT NULL default ' ',
  "AAAAUPJP" varchar(4) NOT NULL default ' ',
  "AAAAUPJU" varchar(30) NOT NULL default ' ',
  "AAAAUPDO" varchar(4) NOT NULL default ' ',
  "AAAAUPDP" varchar(30) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  "AAAAUPDT" varchar(4) NOT NULL default ' ',
  "AAAAUPDS" varchar(30) NOT NULL default ' ',
  "AAABLUSV" longtext,
  "AAAAUPIU" varchar(30) NOT NULL default ' ',
  "AAAAUPFE" varchar(100) NOT NULL default ' ',
  "AAAAUPCF" varchar(100) NOT NULL default ' ',
  "AAAAUPCG" varchar(1) NOT NULL default ' ',
  "AAAAUPCH" int(11) NOT NULL default '0',
  "AAAAUPCI" varchar(1) NOT NULL default ' ',
  "AAAAUPCJ" longtext,
  "AAAAUPCK" varchar(1) NOT NULL default ' ',
  "AAAAUPCL" varchar(30) NOT NULL default ' ',
  "AAAAUPCM" int(11) NOT NULL default '0',
  "AAAAUPCN" int(11) NOT NULL default '0',
  "AAAAUPCO" varchar(20) NOT NULL default ' ',
  "AAAAUPCP" int(11) NOT NULL default '0',
  "AAAAUPCQ" int(11) NOT NULL default '0',
  "AAAAUPCR" varchar(1) NOT NULL default ' ',
  "AAAAUPCS" varchar(4) NOT NULL default ' ',
  "AAAAUPCT" varchar(30) NOT NULL default ' ',
  "AAAAUPCU" varchar(60) NOT NULL default ' ',
  "AAAAUPCV" varchar(4) NOT NULL default ' ',
  "AAAAUPCW" varchar(6) NOT NULL default '000000',
  "AAAAUPCX" varchar(6) NOT NULL default '000000',
  "AAAAUPCY" varchar(1) NOT NULL default ' ',
  "AAAAUPCZ" varchar(1) NOT NULL default ' ',
  "AAAAUPDA" varchar(6) NOT NULL default '000000',
  "AAABLRXL" varchar(30) NOT NULL default ' ',
  "AAABLKZT" varchar(1) NOT NULL default ' ',
  "AAABHFAP" varchar(1) NOT NULL default ' ',
  "AAAAVLSN" varchar(1) NOT NULL default ' ',
  "AAAAUPDF" varchar(1) NOT NULL default ' ',
  "AAAAUPDG" varchar(20) NOT NULL default ' ',
  "AAAAUPDH" varchar(20) NOT NULL default ' ',
  "AAAAUPDI" varchar(10) NOT NULL default ' ',
  "AAAAUPDJ" longtext,
  PRIMARY KEY ("AAABMKFH","AAABLFYD","AAAAUPJP","AAAAUPJU","AAAAUPDO","AAAAUPDP","AAABJASN"),
  KEY "AAAAUPDR" ("AAAAUPDS","AAAAUPDT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPDU" (
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(30) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  "AAAAUPDY" varchar(4) NOT NULL default ' ',
  "AAAAUPDZ" varchar(30) NOT NULL default ' ',
  "AAAAUPGE" varchar(40) NOT NULL default ' ',
  "AAABLUSV" longtext NOT NULL,
  "AAAAUPEC" varchar(1) NOT NULL default ' ',
  "AAAAUPIU" varchar(30) NOT NULL default ' ',
  "AAAAUPFE" varchar(100) NOT NULL default ' ',
  "AAAAUPIV" varchar(60) NOT NULL default ' ',
  "AAAAUPIN" varchar(60) NOT NULL default ' ',
  "AAABKNAR" decimal(15,0) NOT NULL default '0',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAABKNCM" decimal(15,0) NOT NULL default '0',
  "AAABLEBN" varchar(12) NOT NULL default ' ',
  "AAAAUPIO" varchar(4) NOT NULL default '0000',
  "AAABLRQQ" varchar(30) NOT NULL default ' ',
  "AAABLRXL" varchar(30) NOT NULL default ' ',
  "AAABLKZT" varchar(1) NOT NULL default ' ',
  "AAAAVLSN" varchar(1) NOT NULL default ' ',
  "AAAAUPEQ" varchar(30) NOT NULL default ' ',
  "AAAAUPFA" varchar(100) NOT NULL default ' ',
  "AAAAUPFB" varchar(60) NOT NULL default ' ',
  "AAAAUPET" varchar(60) NOT NULL default ' ',
  "AAAAUPEU" varchar(100) NOT NULL default ' ',
  "AAAAUPEV" longtext,
  PRIMARY KEY ("AAABMKFH","AAABLFYD","AAABJASN"),
  KEY "AAAAUPEZ" ("AAAAUPFA","AAAAUPFB"),
  KEY "AAAAUPFC" ("AAAAUPIV","AAAAUPFE"),
  KEY "AAAAUPFF" ("AAABLFYD","AAABMKFH","AAABJASN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPFJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(30) NOT NULL default ' ',
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
CREATE TABLE "AAAAUPGB" (
  "AAAAUPGE" varchar(40) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUPGE","AAABJASN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPGG" (
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(30) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABLSAX" varchar(128) NOT NULL default ' ',
  "AAABLSBA" varchar(4) NOT NULL default '0000',
  "AAABLSAT" varchar(128) NOT NULL default ' ',
  "AAABMMJT" varchar(128) NOT NULL default ' ',
  "AAAAUPGP" longtext,
  PRIMARY KEY ("AAABMKFH","AAABLFYD","AAABJASN","AAABLQGL","AAABLSAX","AAABLSBA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPGW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUYGO" VARBINARY(255) NOT NULL,
  "AAABEDQH" varchar(10) NOT NULL default ' ',
  "AAAAUYGY" VARBINARY(255) NOT NULL,
  "AAAAUYHA" varchar(32) NOT NULL default ' ',
  "AAAAUYHC" varchar(2) NOT NULL default ' ',
  "AAAAUYGZ" varchar(70) NOT NULL default ' ',
  "AAAAUYHB" varchar(32) NOT NULL default ' ',
  "AAAAUYHD" varchar(2) NOT NULL default ' ',
  "AAAAUYGH" varchar(10) NOT NULL default ' ',
  "AAAAUYGI" varchar(1) NOT NULL default ' ',
  "AAAAUYGJ" varchar(10) NOT NULL default ' ',
  "AAAAUYGK" varchar(1) NOT NULL default ' ',
  "AAABDODY" decimal(15,0) NOT NULL default '0',
  "AAAAUYGM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUYGO"(255)),
  KEY "AAAAUPHO" ("AAAAUYGY"(255),"AAAAUYHA","AAAAUYHC"),
  KEY "AAAAUPHS" ("AAAAUYGZ","AAAAUYHB","AAAAUYHD"),
  KEY "AAAAUPHW" ("AAAAUYGY"(255),"AAAAUYGZ","AAAAUYHA","AAAAUYHB","AAAAUYHC","AAAAUYHD","AAABEDQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPIE" (
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(30) NOT NULL default ' ',
  "AAAAUPJP" varchar(4) NOT NULL default ' ',
  "AAAAUPJU" varchar(30) NOT NULL default ' ',
  "AAAAUPIJ" varchar(30) NOT NULL default ' ',
  "AAAAUPIU" varchar(20) NOT NULL default ' ',
  "AAAAUPIV" varchar(60) NOT NULL default ' ',
  "AAAAUPIW" varchar(120) NOT NULL default ' ',
  "AAAAUPIN" varchar(60) NOT NULL default ' ',
  "AAAAUPIO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMKFH","AAABLFYD","AAAAUPJP","AAAAUPJU"),
  KEY "AAAAUPIT" ("AAAAUPIU","AAAAUPIV","AAAAUPIW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPIX" (
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPJB" (
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(30) NOT NULL default ' ',
  "AAAAUPJP" varchar(4) NOT NULL default ' ',
  "AAAAUPJU" varchar(30) NOT NULL default ' ',
  "AAABMJWN" varchar(30) NOT NULL default ' ',
  "AAABJSNO" varchar(30) NOT NULL default ' ',
  "AAAAUPJI" varchar(1) NOT NULL default '0',
  "AAAAUPJJ" varchar(120) NOT NULL default ' ',
  "AAAAUPJK" varchar(60) NOT NULL default ' ',
  "AAAAUPJL" varchar(120) NOT NULL default ' ',
  "AAAAUPJM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABLFYD","AAAAUPJP","AAAAUPJU","AAABMJWN"),
  KEY "AAAAUPJS" ("AAABLFYD","AAAAUPJU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPJV" (
  "AAABMOHM" varchar(30) NOT NULL default ' ',
  "AAABMLPV" varchar(100) NOT NULL default ' ',
  "AAABJSPH" varchar(60) NOT NULL default ' ',
  "AAAAUPJZ" longtext,
  PRIMARY KEY ("AAABMOHM","AAABMLPV","AAABJSPH"),
  KEY "AAAAUPKD" ("AAABJSPH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPKF" (
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABMLPV" varchar(20) NOT NULL default ' ',
  "AAABMKFU" longtext,
  "AAABIADW" varchar(1) NOT NULL default ' ',
  "AAAAUPKM" varchar(1) NOT NULL default ' ',
  "AAABGCIW" varchar(1) NOT NULL default ' ',
  "AAAAUPKO" varchar(1) NOT NULL default ' ',
  "AAAAUPKP" varchar(1) NOT NULL default ' ',
  "AAAAUPKQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPKT" (
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAAAUPKW" varchar(40) NOT NULL default ' ',
  "AAAAUPKX" varchar(40) NOT NULL default ' ',
  "AAABMKFU" longtext,
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABGCIW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPLD" (
  "AAABLGEL" varchar(5) NOT NULL default ' ',
  "AAAAUPLF" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLGEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPLH" (
  "AAABMIRX" varchar(10) NOT NULL default ' ',
  "AAABMKFH" varchar(32) NOT NULL default ' ',
  "AAABILJL" varchar(10) NOT NULL default ' ',
  "AAAAUPMG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMIRX"),
  KEY "AAAAUPLN" ("AAABILJL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPLP" (
  "AAABILJL" varchar(10) NOT NULL default ' ',
  "AAABMKFH" varchar(32) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABILJL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPLU" (
  "AAABLTXI" varchar(8) NOT NULL default ' ',
  "AAABILJL" varchar(10) NOT NULL default ' ',
  "AAABMKFH" varchar(32) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABILJL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPMB" (
  "AAABLTXI" varchar(8) NOT NULL default ' ',
  "AAABMIRX" varchar(10) NOT NULL default ' ',
  "AAABMKFH" varchar(32) NOT NULL default ' ',
  "AAABILJL" varchar(10) NOT NULL default ' ',
  "AAAAUPMG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABMIRX"),
  KEY "AAAAUPMJ" ("AAABILJL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPML" (
  "AAABMIRX" varchar(10) NOT NULL default ' ',
  "AAABDUFB" varchar(3) NOT NULL default '000',
  "AAAAUPRC" varchar(2) NOT NULL default '00',
  "AAABGEZT" varchar(32) NOT NULL default ' ',
  "AAABCXXG" varchar(30) NOT NULL default ' ',
  "AAABMKXX" varchar(30) NOT NULL default ' ',
  "AAAAUPRL" varchar(30) NOT NULL default ' ',
  "AAABBVNZ" varchar(20) NOT NULL default ' ',
  "AAAAUPQV" varchar(2) NOT NULL default '00',
  "AAABKMUE" varchar(2) NOT NULL default '00',
  "AAAAVLSZ" varchar(1) NOT NULL default ' ',
  "AAABMLXR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMIRX","AAABDUFB","AAAAUPRC"),
  KEY "AAAAUPNB" ("AAAAUPRL","AAABMIRX","AAABMLXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPNF" (
  "AAAAVISJ" varchar(10) NOT NULL default ' ',
  "AAAAUPOI" varchar(32) NOT NULL default ' ',
  "AAAAVIXY" varchar(10) NOT NULL default ' ',
  "AAABFGJW" varchar(1) NOT NULL default ' ',
  "AAAAUPNZ" varchar(128) NOT NULL default ' ',
  "AAAAUPOA" varchar(1) NOT NULL default ' ',
  "AAAAUPOB" varchar(1) NOT NULL default ' ',
  "AAAAVLSR" varchar(1) NOT NULL default ' ',
  "AAABJSLL" varchar(1) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABJDFW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAAAVISJ","AAAAUPOI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPNT" (
  "AAABLTXI" varchar(8) NOT NULL default ' ',
  "AAAAVISJ" varchar(10) NOT NULL default ' ',
  "AAAAUPOI" varchar(32) NOT NULL default ' ',
  "AAAAVIXY" varchar(10) NOT NULL default ' ',
  "AAABFGJW" varchar(1) NOT NULL default ' ',
  "AAAAUPNZ" varchar(128) NOT NULL default ' ',
  "AAAAUPOA" varchar(1) NOT NULL default ' ',
  "AAAAUPOB" varchar(1) NOT NULL default ' ',
  "AAAAVLSR" varchar(1) NOT NULL default ' ',
  "AAABJSLL" varchar(1) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABJDFW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAAAVISJ","AAAAUPOI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPOJ" (
  "AAABMIRX" varchar(10) NOT NULL default ' ',
  "AAAAUPSZ" varchar(8) NOT NULL default ' ',
  "AAAAUPPG" varchar(30) NOT NULL default ' ',
  "AAAAUPOV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMIRX","AAAAUPSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPOQ" (
  "AAABLTXI" varchar(8) NOT NULL default ' ',
  "AAABMIRX" varchar(10) NOT NULL default ' ',
  "AAAAUPSZ" varchar(8) NOT NULL default ' ',
  "AAAAUPPG" varchar(30) NOT NULL default ' ',
  "AAAAUPOV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABMIRX","AAAAUPSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPOZ" (
  "AAAAUPPG" varchar(30) NOT NULL default ' ',
  "AAAAUPTA" varchar(30) NOT NULL default ' ',
  "AAAAUPPC" varchar(1) NOT NULL default ' ',
  "AAAAUPPD" varchar(1) NOT NULL default ' ',
  "AAABMKXF" varchar(132) NOT NULL default ' ',
  "AAABMJWI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUPPG","AAAAUPTA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPPI" (
  "AAABMIRX" varchar(10) NOT NULL default ' ',
  "AAAAUPSZ" varchar(8) NOT NULL default ' ',
  "AAAAUPTA" varchar(30) NOT NULL default ' ',
  "AAAAUPXH" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMIRX","AAAAUPSZ","AAAAUPTA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPPR" (
  "AAABLTXI" varchar(8) NOT NULL default ' ',
  "AAABMIRX" varchar(10) NOT NULL default ' ',
  "AAAAUPSZ" varchar(8) NOT NULL default ' ',
  "AAAAUPTA" varchar(30) NOT NULL default ' ',
  "AAAAUPXH" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABMIRX","AAAAUPSZ","AAAAUPTA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPQC" (
  "AAABMIRX" varchar(10) NOT NULL default ' ',
  "AAABGFET" varchar(32) NOT NULL default ' ',
  "AAABLEEK" varchar(1) NOT NULL default ' ',
  "AAABLDNU" varchar(1) NOT NULL default ' ',
  "AAAAUZWM" varchar(6) NOT NULL default '000000',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMIRX","AAABGFET")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPQL" (
  "AAABLTXI" varchar(8) NOT NULL default ' ',
  "AAABMIRX" varchar(10) NOT NULL default ' ',
  "AAABDUFB" varchar(3) NOT NULL default '000',
  "AAAAUPRC" varchar(2) NOT NULL default '00',
  "AAABGEZT" varchar(32) NOT NULL default ' ',
  "AAABCXXG" varchar(30) NOT NULL default ' ',
  "AAABMKXX" varchar(30) NOT NULL default ' ',
  "AAAAUPRL" varchar(30) NOT NULL default ' ',
  "AAABBVNZ" varchar(20) NOT NULL default ' ',
  "AAAAUPQV" varchar(2) NOT NULL default '00',
  "AAABKMUE" varchar(2) NOT NULL default '00',
  "AAAAVLSZ" varchar(1) NOT NULL default ' ',
  "AAABMLXR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABMIRX","AAABDUFB","AAAAUPRC"),
  KEY "AAAAUPRD" ("AAAAUPRL","AAABMIRX","AAABMLXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPRH" (
  "AAAAUPRL" varchar(30) NOT NULL default ' ',
  "AAABMIRX" varchar(10) NOT NULL default ' ',
  "AAABDUFB" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAAAUPRL"),
  KEY "AAAAUPRM" ("AAABMIRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPRO" (
  "AAABMIRX" varchar(10) NOT NULL default ' ',
  "AAAAUPSZ" varchar(8) NOT NULL default ' ',
  "AAAAUPRY" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMIRX","AAAAUPSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPRU" (
  "AAABLTXI" varchar(8) NOT NULL default ' ',
  "AAABMIRX" varchar(10) NOT NULL default ' ',
  "AAAAUPSZ" varchar(8) NOT NULL default ' ',
  "AAAAUPRY" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABMIRX","AAAAUPSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPSC" (
  "AAABMIRX" varchar(10) NOT NULL default ' ',
  "AAAAUPSZ" varchar(8) NOT NULL default ' ',
  "AAAAUPTA" varchar(70) NOT NULL default ' ',
  "AAAAUPTB" varchar(1) NOT NULL default ' ',
  "AAAAUPXH" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMIRX","AAAAUPSZ","AAAAUPTA","AAAAUPTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPSO" (
  "AAABLTXI" varchar(8) NOT NULL default ' ',
  "AAABMIRX" varchar(10) NOT NULL default ' ',
  "AAAAUPSZ" varchar(8) NOT NULL default ' ',
  "AAAAUPTA" varchar(70) NOT NULL default ' ',
  "AAAAUPTB" varchar(1) NOT NULL default ' ',
  "AAAAUPXH" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABMIRX","AAAAUPSZ","AAAAUPTA","AAAAUPTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPTC" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMIRX" varchar(10) NOT NULL default ' ',
  "AAABKJAU" varchar(1) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABMFSV" varchar(4) NOT NULL default '0000',
  "AAAAUPTI" varchar(1) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP","AAABMIRX"),
  KEY "AAAAUPTM" ("AAABMIRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPTO" (
  "AAABMIRX" varchar(10) NOT NULL default ' ',
  "AAABLPVX" varchar(1) NOT NULL default ' ',
  "AAABLPVY" varchar(40) NOT NULL default ' ',
  "AAAAUPUE" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMIRX","AAABLPVX","AAABLPVY"),
  KEY "AAAAUPTW" ("AAABLPVX","AAABLPVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPTZ" (
  "AAABLTXI" varchar(8) NOT NULL default ' ',
  "AAABMIRX" varchar(10) NOT NULL default ' ',
  "AAABLPVX" varchar(1) NOT NULL default ' ',
  "AAABLPVY" varchar(40) NOT NULL default ' ',
  "AAAAUPUE" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABMIRX","AAABLPVX","AAABLPVY"),
  KEY "AAAAUPUJ" ("AAABLPVX","AAABLPVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPUM" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMIRX" varchar(10) NOT NULL default ' ',
  "AAAAUPUZ" varchar(132) NOT NULL default ' ',
  "AAAAUPVA" varchar(30) NOT NULL default ' ',
  "AAAAUPVB" varchar(30) NOT NULL default ' ',
  "AAAAUPVC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP","AAABMIRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPUV" (
  "AAABLTXI" varchar(8) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMIRX" varchar(10) NOT NULL default ' ',
  "AAAAUPUZ" varchar(132) NOT NULL default ' ',
  "AAAAUPVA" varchar(30) NOT NULL default ' ',
  "AAAAUPVB" varchar(30) NOT NULL default ' ',
  "AAAAUPVC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABMDEP","AAABMIRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPVG" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMIRX" varchar(10) NOT NULL default ' ',
  "AAAAUPVZ" varchar(30) NOT NULL default ' ',
  "AAAAUPXH" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP","AAABMIRX","AAAAUPVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPVP" (
  "AAABLTXI" varchar(8) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMIRX" varchar(10) NOT NULL default ' ',
  "AAAAUPVZ" varchar(30) NOT NULL default ' ',
  "AAAAUPXH" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABMDEP","AAABMIRX","AAAAUPVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPWA" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAAAUPWJ" varchar(2) NOT NULL default '00',
  "AAABMIRX" varchar(10) NOT NULL default ' ',
  "AAABKAFO" varchar(132) NOT NULL default ' ',
  "AAABEJLD" varchar(132) NOT NULL default ' ',
  "AAAAUPXH" varchar(1) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP","AAAAUPWJ","AAABMIRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPWL" (
  "AAABMIRX" varchar(10) NOT NULL default ' ',
  "AAABLPVX" varchar(1) NOT NULL default ' ',
  "AAABLPVY" varchar(40) NOT NULL default ' ',
  "AAABIHNW" varchar(2) NOT NULL default '00',
  "AAABKAFO" varchar(132) NOT NULL default ' ',
  "AAAAUPXH" varchar(1) NOT NULL default ' ',
  "AAABEJLD" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMIRX","AAABLPVX","AAABLPVY","AAABIHNW"),
  KEY "AAAAUPWX" ("AAABLPVX","AAABLPVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPXA" (
  "AAABLTXI" varchar(8) NOT NULL default ' ',
  "AAABMIRX" varchar(10) NOT NULL default ' ',
  "AAABLPVX" varchar(1) NOT NULL default ' ',
  "AAABLPVY" varchar(40) NOT NULL default ' ',
  "AAABIHNW" varchar(2) NOT NULL default '00',
  "AAABKAFO" varchar(132) NOT NULL default ' ',
  "AAAAUPXH" varchar(1) NOT NULL default ' ',
  "AAABEJLD" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABMIRX","AAABLPVX","AAABLPVY","AAABIHNW"),
  KEY "AAAAUPXO" ("AAABLPVX","AAABLPVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPXR" (
  "AAABMIRX" varchar(10) NOT NULL default ' ',
  "AAABGPVW" varchar(1) NOT NULL default ' ',
  "AAAAUPXU" varchar(12) NOT NULL default ' ',
  "AAAAUPXV" varchar(8) NOT NULL default '00000000',
  "AAAAUPXW" varchar(6) NOT NULL default '000000',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMIRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPXZ" (
  "AAABLTXI" varchar(8) NOT NULL default ' ',
  "AAABMIRX" varchar(10) NOT NULL default ' ',
  "AAAAUPYC" varchar(20) NOT NULL default ' ',
  "AAAAUPYD" varchar(14) NOT NULL default ' ',
  "AAABHRNH" varchar(14) NOT NULL default ' ',
  "AAAAVHQQ" varchar(14) NOT NULL default ' ',
  "AAAAUPYG" varchar(14) NOT NULL default ' ',
  "AAAAUPYH" varchar(1) NOT NULL default ' ',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAABILFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABMIRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPYM" (
  "AAAAVOMG" varchar(60) NOT NULL default ' ',
  "AAAAUPYO" varchar(60) NOT NULL default ' ',
  "AAAAUPYP" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAAAVOMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPYR" (
  "AAABLQGL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPYU" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLQGL" varchar(4) NOT NULL default ' ',
  "AAABLPWW" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPZA" (
  "AAABLQHQ" varchar(86) NOT NULL default ' ',
  "AAAAUPZI" varchar(1) NOT NULL default ' ',
  "AAAAUPZD" varchar(1) NOT NULL default ' ',
  "AAAAUPZE" varchar(1) NOT NULL default ' ',
  "AAAAUPZR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLQHQ"),
  KEY "AAAAUPZH" ("AAAAUPZI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPZJ" (
  "AAAAUPZR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUPZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPZM" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAUPZR" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAAAUPZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUPZS" (
  "AAAAURAW" varchar(17) NOT NULL default ' ',
  "AAAAURAL" varchar(26) NOT NULL default ' ',
  "AAAAUQBU" varchar(10) NOT NULL default ' ',
  "AAAAUQBV" varchar(10) NOT NULL default ' ',
  "AAAAUQBW" varchar(6) NOT NULL default ' ',
  "AAAAUQBX" varchar(18) NOT NULL default ' ',
  "AAAAUQBY" varchar(10) NOT NULL default ' ',
  "AAAAUQBZ" varchar(10) NOT NULL default ' ',
  "AAAAUQCA" varchar(18) NOT NULL default ' ',
  "AAAAUQCB" varchar(18) NOT NULL default ' ',
  "AAAAUQCC" decimal(15,0) NOT NULL default '0',
  "AAAAUQCD" decimal(15,0) NOT NULL default '0',
  "AAAAUQCE" decimal(15,0) NOT NULL default '0',
  "AAAAUQCF" decimal(15,0) NOT NULL default '0',
  "AAAAUQCG" decimal(15,0) NOT NULL default '0',
  "AAAAUQCH" decimal(15,0) NOT NULL default '0',
  "AAAAUQCI" decimal(15,0) NOT NULL default '0',
  "AAAAUQCJ" decimal(15,0) NOT NULL default '0',
  "AAAAUQCK" varchar(26) NOT NULL default ' ',
  "AAAAUQCL" varchar(26) NOT NULL default ' ',
  "AAAAUQCM" varchar(26) NOT NULL default ' ',
  "AAAAUQCN" decimal(15,0) NOT NULL default '0',
  "AAAAUQCO" decimal(15,0) NOT NULL default '0',
  "AAAAUQHW" decimal(15,0) NOT NULL default '0',
  "AAAAUQCQ" decimal(15,0) NOT NULL default '0',
  "AAAAUQCR" varchar(1) NOT NULL default ' ',
  "AAAAUQCS" varchar(2) NOT NULL default ' ',
  "AAAAUQCT" varchar(1) NOT NULL default ' ',
  "AAAAUQCU" varchar(1) NOT NULL default ' ',
  "AAAAUQCV" varchar(4) NOT NULL default ' ',
  "AAAAUQCW" varchar(1) NOT NULL default ' ',
  "AAAAUQCX" decimal(15,0) NOT NULL default '0',
  "AAAAUQCY" decimal(15,0) NOT NULL default '0',
  "AAAAUQCZ" decimal(15,0) NOT NULL default '0',
  "AAAAUQDA" decimal(15,0) NOT NULL default '0',
  "AAAAUQDB" decimal(15,0) NOT NULL default '0',
  "AAAAUQDC" decimal(15,0) NOT NULL default '0',
  "AAAAUQDD" decimal(15,0) NOT NULL default '0',
  "AAAAUQDE" decimal(15,0) NOT NULL default '0',
  "AAAAURAM" varchar(1) NOT NULL default ' ',
  "AAAAURAN" varchar(1) NOT NULL default ' ',
  "AAAAURAO" varchar(2) NOT NULL default ' ',
  "AAAAURAP" varchar(2) NOT NULL default ' ',
  "AAAAURAQ" int(11) NOT NULL default '0',
  "AAAAURAR" int(11) NOT NULL default '0',
  "AAAAURAS" varchar(30) NOT NULL default ' ',
  "AAAAURAT" varchar(30) NOT NULL default ' ',
  "AAAAUQNM" varchar(255) NOT NULL default ' ',
  "AAAAUQDO" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAAURAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQBR" (
  "AAAAURAW" varchar(17) NOT NULL default ' ',
  "AAAAURAL" varchar(26) NOT NULL default ' ',
  "AAAAUQBU" varchar(10) NOT NULL default ' ',
  "AAAAUQBV" varchar(10) NOT NULL default ' ',
  "AAAAUQBW" varchar(6) NOT NULL default ' ',
  "AAAAUQBX" varchar(18) NOT NULL default ' ',
  "AAAAUQBY" varchar(10) NOT NULL default ' ',
  "AAAAUQBZ" varchar(10) NOT NULL default ' ',
  "AAAAUQCA" varchar(18) NOT NULL default ' ',
  "AAAAUQCB" varchar(18) NOT NULL default ' ',
  "AAAAUQCC" decimal(15,0) NOT NULL default '0',
  "AAAAUQCD" decimal(15,0) NOT NULL default '0',
  "AAAAUQCE" decimal(15,0) NOT NULL default '0',
  "AAAAUQCF" decimal(15,0) NOT NULL default '0',
  "AAAAUQCG" decimal(15,0) NOT NULL default '0',
  "AAAAUQCH" decimal(15,0) NOT NULL default '0',
  "AAAAUQCI" decimal(15,0) NOT NULL default '0',
  "AAAAUQCJ" decimal(15,0) NOT NULL default '0',
  "AAAAUQCK" varchar(26) NOT NULL default ' ',
  "AAAAUQCL" varchar(26) NOT NULL default ' ',
  "AAAAUQCM" varchar(26) NOT NULL default ' ',
  "AAAAUQCN" decimal(15,0) NOT NULL default '0',
  "AAAAUQCO" decimal(15,0) NOT NULL default '0',
  "AAAAUQHW" decimal(15,0) NOT NULL default '0',
  "AAAAUQCQ" decimal(15,0) NOT NULL default '0',
  "AAAAUQCR" varchar(1) NOT NULL default ' ',
  "AAAAUQCS" varchar(2) NOT NULL default ' ',
  "AAAAUQCT" varchar(1) NOT NULL default ' ',
  "AAAAUQCU" varchar(1) NOT NULL default ' ',
  "AAAAUQCV" varchar(4) NOT NULL default ' ',
  "AAAAUQCW" varchar(1) NOT NULL default ' ',
  "AAAAUQCX" decimal(15,0) NOT NULL default '0',
  "AAAAUQCY" decimal(15,0) NOT NULL default '0',
  "AAAAUQCZ" decimal(15,0) NOT NULL default '0',
  "AAAAUQDA" decimal(15,0) NOT NULL default '0',
  "AAAAUQDB" decimal(15,0) NOT NULL default '0',
  "AAAAUQDC" decimal(15,0) NOT NULL default '0',
  "AAAAUQDD" decimal(15,0) NOT NULL default '0',
  "AAAAUQDE" decimal(15,0) NOT NULL default '0',
  "AAAAURAM" varchar(1) NOT NULL default ' ',
  "AAAAURAN" varchar(1) NOT NULL default ' ',
  "AAAAURAO" varchar(2) NOT NULL default ' ',
  "AAAAURAP" varchar(2) NOT NULL default ' ',
  "AAAAURAQ" int(11) NOT NULL default '0',
  "AAAAURAR" int(11) NOT NULL default '0',
  "AAAAURAS" varchar(30) NOT NULL default ' ',
  "AAAAURAT" varchar(30) NOT NULL default ' ',
  "AAAAUQNM" varchar(255) NOT NULL default ' ',
  "AAAAUQDO" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAAURAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQDQ" (
  "AAAAURAW" varchar(17) NOT NULL default ' ',
  "AAAAUQNO" decimal(15,0) NOT NULL default '0',
  "AAAAURAL" varchar(26) NOT NULL default ' ',
  "AAAAUQMY" decimal(15,0) NOT NULL default '0',
  "AAAAUQMZ" decimal(15,0) NOT NULL default '0',
  "AAAAUQNA" decimal(15,0) NOT NULL default '0',
  "AAAAUQNB" decimal(15,0) NOT NULL default '0',
  "AAAAUQIC" decimal(15,0) NOT NULL default '0',
  "AAAAUQLX" varchar(10) NOT NULL default ' ',
  "AAAAUQLY" varchar(30) NOT NULL default ' ',
  "AAAAUQLZ" varchar(10) NOT NULL default ' ',
  "AAAAUQMA" varchar(30) NOT NULL default ' ',
  "AAAAUQHW" decimal(15,0) NOT NULL default '0',
  "AAAAUQHY" decimal(15,0) NOT NULL default '0',
  "AAAAUQIB" decimal(15,0) NOT NULL default '0',
  "AAAAUQII" decimal(15,0) NOT NULL default '0',
  "AAAAUQID" decimal(15,0) NOT NULL default '0',
  "AAAAUQIE" varchar(2) NOT NULL default ' ',
  "AAAAUQIF" varchar(2) NOT NULL default ' ',
  "AAAAUQIG" varchar(2) NOT NULL default ' ',
  "AAAAUQIM" varchar(1) NOT NULL default ' ',
  "AAAAUQIN" varchar(1) NOT NULL default ' ',
  "AAAAUQLB" varchar(2) NOT NULL default ' ',
  "AAAAUQMD" varchar(128) NOT NULL default ' ',
  "AAAAUQME" varchar(128) NOT NULL default ' ',
  "AAAAUQMF" varchar(128) NOT NULL default ' ',
  "AAAAUQMG" varchar(128) NOT NULL default ' ',
  "AAAAURAM" varchar(1) NOT NULL default ' ',
  "AAAAURAN" varchar(1) NOT NULL default ' ',
  "AAAAURAO" varchar(2) NOT NULL default ' ',
  "AAAAURAP" varchar(2) NOT NULL default ' ',
  "AAAAURAQ" int(11) NOT NULL default '0',
  "AAAAURAR" int(11) NOT NULL default '0',
  "AAAAURAS" varchar(30) NOT NULL default ' ',
  "AAAAURAT" varchar(30) NOT NULL default ' ',
  "AAAAUQNM" varchar(255) NOT NULL default ' ',
  "AAAAUQHC" smallint(6) NOT NULL default '0',
  "AAAAUQIS" text,
  PRIMARY KEY ("AAAAURAW","AAAAUQNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQFF" (
  "AAAAURAW" varchar(17) NOT NULL default ' ',
  "AAAAUQNO" decimal(15,0) NOT NULL default '0',
  "AAAAURAL" varchar(26) NOT NULL default ' ',
  "AAAAUQMY" decimal(15,0) NOT NULL default '0',
  "AAAAUQMZ" decimal(15,0) NOT NULL default '0',
  "AAAAUQNA" decimal(15,0) NOT NULL default '0',
  "AAAAUQNB" decimal(15,0) NOT NULL default '0',
  "AAAAUQLX" varchar(10) NOT NULL default ' ',
  "AAAAUQLY" varchar(30) NOT NULL default ' ',
  "AAAAUQLZ" varchar(10) NOT NULL default ' ',
  "AAAAUQMA" varchar(30) NOT NULL default ' ',
  "AAAAUQHS" varchar(10) NOT NULL default ' ',
  "AAAAUQHT" varchar(18) NOT NULL default ' ',
  "AAAAUQHW" decimal(15,0) NOT NULL default '0',
  "AAAAUQHY" decimal(15,0) NOT NULL default '0',
  "AAAAUQHZ" decimal(15,0) NOT NULL default '0',
  "AAAAUQIA" decimal(15,0) NOT NULL default '0',
  "AAAAUQIB" decimal(15,0) NOT NULL default '0',
  "AAAAUQII" decimal(15,0) NOT NULL default '0',
  "AAAAUQIC" decimal(15,0) NOT NULL default '0',
  "AAAAUQID" decimal(15,0) NOT NULL default '0',
  "AAAAUQIE" varchar(2) NOT NULL default ' ',
  "AAAAUQIF" varchar(2) NOT NULL default ' ',
  "AAAAUQIG" varchar(2) NOT NULL default ' ',
  "AAAAUQIH" decimal(15,0) NOT NULL default '0',
  "AAAAUQIJ" varchar(1) NOT NULL default ' ',
  "AAAAUQIK" decimal(15,0) NOT NULL default '0',
  "AAAAUQIL" varchar(1) NOT NULL default ' ',
  "AAAAUQIM" varchar(1) NOT NULL default ' ',
  "AAAAUQIN" varchar(1) NOT NULL default ' ',
  "AAAAUQLB" varchar(2) NOT NULL default ' ',
  "AAAAUQIO" varchar(1) NOT NULL default ' ',
  "AAAAUQIP" varchar(18) NOT NULL default ' ',
  "AAAAUQMD" varchar(128) NOT NULL default ' ',
  "AAAAUQME" varchar(128) NOT NULL default ' ',
  "AAAAUQMF" varchar(128) NOT NULL default ' ',
  "AAAAUQMG" varchar(128) NOT NULL default ' ',
  "AAAAUQIX" varchar(128) NOT NULL default ' ',
  "AAAAUQIY" varchar(128) NOT NULL default ' ',
  "AAAAURAM" varchar(1) NOT NULL default ' ',
  "AAAAURAN" varchar(1) NOT NULL default ' ',
  "AAAAURAO" varchar(2) NOT NULL default ' ',
  "AAAAURAP" varchar(2) NOT NULL default ' ',
  "AAAAURAQ" int(11) NOT NULL default '0',
  "AAAAURAR" int(11) NOT NULL default '0',
  "AAAAURAS" varchar(30) NOT NULL default ' ',
  "AAAAURAT" varchar(30) NOT NULL default ' ',
  "AAAAUQNM" varchar(255) NOT NULL default ' ',
  "AAAAUQHC" smallint(6) NOT NULL default '0',
  "AAAAUQIS" text,
  PRIMARY KEY ("AAAAURAW","AAAAUQNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQHG" (
  "AAAAURAW" varchar(17) NOT NULL default ' ',
  "AAAAUQNO" decimal(15,0) NOT NULL default '0',
  "AAAAURAL" varchar(26) NOT NULL default ' ',
  "AAAAUQMY" decimal(15,0) NOT NULL default '0',
  "AAAAUQMZ" decimal(15,0) NOT NULL default '0',
  "AAAAUQNA" decimal(15,0) NOT NULL default '0',
  "AAAAUQNB" decimal(15,0) NOT NULL default '0',
  "AAAAUQLX" varchar(10) NOT NULL default ' ',
  "AAAAUQLY" varchar(30) NOT NULL default ' ',
  "AAAAUQLZ" varchar(10) NOT NULL default ' ',
  "AAAAUQMA" varchar(30) NOT NULL default ' ',
  "AAAAUQHS" varchar(10) NOT NULL default ' ',
  "AAAAUQHT" varchar(18) NOT NULL default ' ',
  "AAAAUQHU" varchar(10) NOT NULL default ' ',
  "AAAAUQHV" varchar(10) NOT NULL default ' ',
  "AAAAUQHW" decimal(15,0) NOT NULL default '0',
  "AAAAUQHX" decimal(15,0) NOT NULL default '0',
  "AAAAUQHY" decimal(15,0) NOT NULL default '0',
  "AAAAUQHZ" decimal(15,0) NOT NULL default '0',
  "AAAAUQIA" decimal(15,0) NOT NULL default '0',
  "AAAAUQIB" decimal(15,0) NOT NULL default '0',
  "AAAAUQIC" decimal(15,0) NOT NULL default '0',
  "AAAAUQID" decimal(15,0) NOT NULL default '0',
  "AAAAUQIE" varchar(2) NOT NULL default ' ',
  "AAAAUQIF" varchar(2) NOT NULL default ' ',
  "AAAAUQIG" varchar(2) NOT NULL default ' ',
  "AAAAUQIH" decimal(15,0) NOT NULL default '0',
  "AAAAUQII" decimal(15,0) NOT NULL default '0',
  "AAAAUQIJ" varchar(1) NOT NULL default ' ',
  "AAAAUQIK" decimal(15,0) NOT NULL default '0',
  "AAAAUQIL" varchar(1) NOT NULL default ' ',
  "AAAAUQIM" varchar(1) NOT NULL default ' ',
  "AAAAUQIN" varchar(1) NOT NULL default ' ',
  "AAAAUQIO" varchar(1) NOT NULL default ' ',
  "AAAAUQIP" varchar(18) NOT NULL default ' ',
  "AAAAUQLB" varchar(2) NOT NULL default ' ',
  "AAAAUQKY" decimal(15,0) NOT NULL default '0',
  "AAAAUQIS" varchar(255) NOT NULL default ' ',
  "AAAAUQMD" varchar(128) NOT NULL default ' ',
  "AAAAUQME" varchar(128) NOT NULL default ' ',
  "AAAAUQMF" varchar(128) NOT NULL default ' ',
  "AAAAUQMG" varchar(128) NOT NULL default ' ',
  "AAAAUQIX" varchar(128) NOT NULL default ' ',
  "AAAAUQIY" varchar(128) NOT NULL default ' ',
  "AAAAUQIZ" varchar(128) NOT NULL default ' ',
  "AAAAUQJA" varchar(128) NOT NULL default ' ',
  "AAAAURAM" varchar(1) NOT NULL default ' ',
  "AAAAURAN" varchar(1) NOT NULL default ' ',
  "AAAAURAO" varchar(2) NOT NULL default ' ',
  "AAAAURAP" varchar(2) NOT NULL default ' ',
  "AAAAURAQ" int(11) NOT NULL default '0',
  "AAAAURAR" int(11) NOT NULL default '0',
  "AAAAURAS" varchar(30) NOT NULL default ' ',
  "AAAAURAT" varchar(30) NOT NULL default ' ',
  "AAAAUQJJ" smallint(6) NOT NULL default '0',
  "AAAAUQJK" text,
  PRIMARY KEY ("AAAAURAW","AAAAUQNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQJN" (
  "AAAAURAW" varchar(17) NOT NULL default ' ',
  "AAAAUQNO" decimal(15,0) NOT NULL default '0',
  "AAAAURAL" varchar(26) NOT NULL default ' ',
  "AAAAUQMY" decimal(15,0) NOT NULL default '0',
  "AAAAUQMZ" decimal(15,0) NOT NULL default '0',
  "AAAAUQNA" decimal(15,0) NOT NULL default '0',
  "AAAAUQNB" decimal(15,0) NOT NULL default '0',
  "AAAAUQKY" decimal(15,0) NOT NULL default '0',
  "AAAAUQJW" decimal(15,0) NOT NULL default '0',
  "AAAAUQJX" decimal(15,0) NOT NULL default '0',
  "AAAAUQJY" decimal(15,0) NOT NULL default '0',
  "AAAAUQJZ" decimal(15,0) NOT NULL default '0',
  "AAAAUQKA" decimal(15,0) NOT NULL default '0',
  "AAAAUQKB" decimal(15,0) NOT NULL default '0',
  "AAAAUQLB" varchar(2) NOT NULL default ' ',
  "AAAAURAM" varchar(1) NOT NULL default ' ',
  "AAAAURAN" varchar(1) NOT NULL default ' ',
  "AAAAURAO" varchar(2) NOT NULL default ' ',
  "AAAAURAP" varchar(2) NOT NULL default ' ',
  "AAAAURAQ" int(11) NOT NULL default '0',
  "AAAAURAR" int(11) NOT NULL default '0',
  "AAAAURAS" varchar(30) NOT NULL default ' ',
  "AAAAURAT" varchar(30) NOT NULL default ' ',
  "AAAAUQNM" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAAURAW","AAAAUQNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQKO" (
  "AAAAURAW" varchar(17) NOT NULL default ' ',
  "AAAAUQNO" decimal(15,0) NOT NULL default '0',
  "AAAAURAL" varchar(26) NOT NULL default ' ',
  "AAAAUQMY" decimal(15,0) NOT NULL default '0',
  "AAAAUQMZ" decimal(15,0) NOT NULL default '0',
  "AAAAUQNA" decimal(15,0) NOT NULL default '0',
  "AAAAUQNB" decimal(15,0) NOT NULL default '0',
  "AAAAUQLX" varchar(10) NOT NULL default ' ',
  "AAAAUQLY" varchar(30) NOT NULL default ' ',
  "AAAAUQKY" decimal(15,0) NOT NULL default '0',
  "AAAAUQKZ" decimal(15,0) NOT NULL default '0',
  "AAAAUQLA" varchar(1) NOT NULL default ' ',
  "AAAAUQLB" varchar(2) NOT NULL default ' ',
  "AAAAUQMD" varchar(128) NOT NULL default ' ',
  "AAAAUQME" varchar(128) NOT NULL default ' ',
  "AAAAURAM" varchar(1) NOT NULL default ' ',
  "AAAAURAN" varchar(1) NOT NULL default ' ',
  "AAAAURAO" varchar(2) NOT NULL default ' ',
  "AAAAURAP" varchar(2) NOT NULL default ' ',
  "AAAAURAQ" int(11) NOT NULL default '0',
  "AAAAURAR" int(11) NOT NULL default '0',
  "AAAAURAS" varchar(30) NOT NULL default ' ',
  "AAAAURAT" varchar(30) NOT NULL default ' ',
  "AAAAUQNM" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAAURAW","AAAAUQNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQLP" (
  "AAAAURAW" varchar(17) NOT NULL default ' ',
  "AAAAUQNO" decimal(15,0) NOT NULL default '0',
  "AAAAURAL" varchar(26) NOT NULL default ' ',
  "AAAAUQMY" decimal(15,0) NOT NULL default '0',
  "AAAAUQMZ" decimal(15,0) NOT NULL default '0',
  "AAAAUQNA" decimal(15,0) NOT NULL default '0',
  "AAAAUQNB" decimal(15,0) NOT NULL default '0',
  "AAAAUQLX" varchar(10) NOT NULL default ' ',
  "AAAAUQLY" varchar(30) NOT NULL default ' ',
  "AAAAUQLZ" varchar(10) NOT NULL default ' ',
  "AAAAUQMA" varchar(30) NOT NULL default ' ',
  "AAAAUQMB" varchar(1) NOT NULL default ' ',
  "AAAAUQMC" varchar(30) NOT NULL default ' ',
  "AAAAUQMD" varchar(128) NOT NULL default ' ',
  "AAAAUQME" varchar(128) NOT NULL default ' ',
  "AAAAUQMF" varchar(128) NOT NULL default ' ',
  "AAAAUQMG" varchar(128) NOT NULL default ' ',
  "AAAAURAM" varchar(1) NOT NULL default ' ',
  "AAAAURAN" varchar(1) NOT NULL default ' ',
  "AAAAURAO" varchar(2) NOT NULL default ' ',
  "AAAAURAP" varchar(2) NOT NULL default ' ',
  "AAAAURAQ" int(11) NOT NULL default '0',
  "AAAAURAR" int(11) NOT NULL default '0',
  "AAAAURAS" varchar(30) NOT NULL default ' ',
  "AAAAURAT" varchar(30) NOT NULL default ' ',
  "AAAAUQNM" varchar(255) NOT NULL default ' ',
  "AAAAUQMQ" smallint(6) NOT NULL default '0',
  "AAAAUQMR" text,
  PRIMARY KEY ("AAAAURAW","AAAAUQNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQMU" (
  "AAAAURAW" varchar(17) NOT NULL default ' ',
  "AAAAUQNO" decimal(15,0) NOT NULL default '0',
  "AAAAURAL" varchar(26) NOT NULL default ' ',
  "AAAAUQMY" decimal(15,0) NOT NULL default '0',
  "AAAAUQMZ" decimal(15,0) NOT NULL default '0',
  "AAAAUQNA" decimal(15,0) NOT NULL default '0',
  "AAAAUQNB" decimal(15,0) NOT NULL default '0',
  "AAAAUQNC" decimal(15,0) NOT NULL default '0',
  "AAAAUQND" decimal(15,0) NOT NULL default '0',
  "AAAAURAM" varchar(1) NOT NULL default ' ',
  "AAAAURAN" varchar(1) NOT NULL default ' ',
  "AAAAURAO" varchar(2) NOT NULL default ' ',
  "AAAAURAP" varchar(2) NOT NULL default ' ',
  "AAAAURAQ" int(11) NOT NULL default '0',
  "AAAAURAR" int(11) NOT NULL default '0',
  "AAAAURAS" varchar(30) NOT NULL default ' ',
  "AAAAURAT" varchar(30) NOT NULL default ' ',
  "AAAAUQNM" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAAURAW","AAAAUQNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQNP" (
  "AAABHZQZ" varchar(15) NOT NULL default ' ',
  "AAAAUQNR" varchar(15) NOT NULL default ' ',
  "AAABLLNR" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZQZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQNU" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHZQZ" varchar(15) NOT NULL default ' ',
  "AAABJBVB" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHZQZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQOA" (
  "AAABDIIT" varchar(10) NOT NULL default ' ',
  "AAABHGZO" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABDIIT","AAABHGZO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQOF" (
  "AAAAUQOI" varchar(32) NOT NULL default ' ',
  "AAABLPJD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUQOI","AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQOK" (
  "AAABKOUC" varchar(20) NOT NULL default ' ',
  "AAAAUQTM" varchar(32) NOT NULL default ' ',
  "AAABHGZO" varchar(32) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAAAUQUY" decimal(15,0) NOT NULL default '0',
  "AAAAUQUZ" varchar(4) NOT NULL default ' ',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAAAUQVB" decimal(15,0) NOT NULL default '0',
  "AAAAUQVC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABKOUC","AAAAUQTM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQOW" (
  "AAABGPHH" varchar(20) NOT NULL default ' ',
  "AAAAUQOY" varchar(1) NOT NULL default ' ',
  "AAAAUQOZ" varchar(1) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAAAUQUY" decimal(15,0) NOT NULL default '0',
  "AAAAUQUZ" varchar(4) NOT NULL default ' ',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAAAUQVB" decimal(15,0) NOT NULL default '0',
  "AAAAUQVC" varchar(4) NOT NULL default ' ',
  "AAABHFMF" varchar(30) NOT NULL default ' ',
  "AAAAUQPH" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABGPHH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQPK" (
  "AAABGPHH" varchar(20) NOT NULL default ' ',
  "AAAAUQPM" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABGPHH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQPO" (
  "AAABGPHH" varchar(20) NOT NULL default ' ',
  "AAAAUQUB" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABGPHH","AAAAUQUB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQPT" (
  "AAABGPHH" varchar(20) NOT NULL default ' ',
  "AAABLMOZ" varchar(20) NOT NULL default ' ',
  "AAABKWXP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGPHH","AAABLMOZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQPZ" (
  "AAABGPHH" varchar(20) NOT NULL default ' ',
  "AAABLMOZ" varchar(20) NOT NULL default ' ',
  "AAABMMJT" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABGPHH","AAABLMOZ","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQQG" (
  "AAABGPHH" varchar(20) NOT NULL default ' ',
  "AAABMOHM" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABGPHH","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQQL" (
  "AAABGPHH" varchar(20) NOT NULL default ' ',
  "AAAAUQQN" tinyint(3) unsigned NOT NULL default '0',
  "AAABMALF" varchar(1) NOT NULL default ' ',
  "AAAAUQQP" varchar(1) NOT NULL default ' ',
  "AAAAUQQQ" tinyint(3) unsigned NOT NULL default '0',
  "AAAAUQQR" varchar(3) NOT NULL default ' ',
  "AAABENKB" varchar(3) NOT NULL default ' ',
  "AAAAUQQT" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABGPHH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQQV" (
  "AAABGPHH" varchar(20) NOT NULL default ' ',
  "AAAAVCVI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABGPHH","AAAAVCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQRA" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUQRO" varchar(15) NOT NULL default ' ',
  "AAABGPHH" varchar(20) NOT NULL default ' ',
  "AAAAXKZP" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAUQRO","AAABGPHH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQRI" (
  "AAAAUQRO" varchar(15) NOT NULL default ' ',
  "AAABGPHH" varchar(20) NOT NULL default ' ',
  "AAAAUQTH" varchar(15) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  "AAABHLPB" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAAUQRO","AAABGPHH","AAAAUQTH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQRR" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUQRY" varchar(15) NOT NULL default ' ',
  "AAABGPHH" varchar(20) NOT NULL default ' ',
  "AAAAUQTH" varchar(15) NOT NULL default ' ',
  "AAAAUQRW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAUQRY","AAABGPHH","AAAAUQTH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQSB" (
  "AAABGPHH" varchar(20) NOT NULL default ' ',
  "AAAAUQTH" varchar(15) NOT NULL default ' ',
  "AAABLPJD" varchar(4) NOT NULL default ' ',
  "AAABLUNJ" varchar(32) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGPHH","AAAAUQTH","AAABLPJD","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQSL" (
  "AAAAUQTG" varchar(32) NOT NULL default ' ',
  "AAABGPHH" varchar(20) NOT NULL default ' ',
  "AAAAUQSO" varchar(32) NOT NULL default ' ',
  "AAAAUQTM" varchar(32) NOT NULL default ' ',
  "AAAAUQSQ" varchar(32) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAAAUQUY" decimal(15,0) NOT NULL default '0',
  "AAAAUQUZ" varchar(4) NOT NULL default ' ',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAAAUQVB" decimal(15,0) NOT NULL default '0',
  "AAAAUQVC" varchar(4) NOT NULL default ' ',
  "AAABLRQO" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUQTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQTA" (
  "AAAAUQTG" varchar(32) NOT NULL default ' ',
  "AAAAUQTH" varchar(15) NOT NULL default ' ',
  "AAABBORB" varchar(1) NOT NULL default ' ',
  "AAAAUQTE" varchar(1) NOT NULL default ' ',
  "AAABHSZC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUQTG","AAAAUQTH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQTI" (
  "AAAAUQTM" varchar(32) NOT NULL default ' ',
  "AAABDIIT" varchar(10) NOT NULL default ' ',
  "AAABLSBE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUQTM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQTN" (
  "AAAAUQUB" varchar(20) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAAAUQUY" decimal(15,0) NOT NULL default '0',
  "AAAAUQUZ" varchar(4) NOT NULL default ' ',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAAAUQVB" decimal(15,0) NOT NULL default '0',
  "AAAAUQVC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUQUB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQTW" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUQUB" varchar(20) NOT NULL default ' ',
  "AAABKZAW" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAUQUB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQUC" (
  "AAABLMOZ" varchar(20) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAAAUQUY" decimal(15,0) NOT NULL default '0',
  "AAAAUQUZ" varchar(4) NOT NULL default ' ',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAAAUQVB" decimal(15,0) NOT NULL default '0',
  "AAAAUQVC" varchar(4) NOT NULL default ' ',
  "AAAAUQUK" varchar(30) NOT NULL default ' ',
  "AAAAUQUL" varchar(30) NOT NULL default ' ',
  "AAABHUHN" varchar(30) NOT NULL default ' ',
  "AAAAUQUN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMOZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQUP" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLMOZ" varchar(20) NOT NULL default ' ',
  "AAABKZAW" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLMOZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQUV" (
  "AAAAUQWD" varchar(20) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAAAUQUY" decimal(15,0) NOT NULL default '0',
  "AAAAUQUZ" varchar(4) NOT NULL default ' ',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAAAUQVB" decimal(15,0) NOT NULL default '0',
  "AAAAUQVC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUQWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQVE" (
  "AAABLLNR" varchar(15) NOT NULL default ' ',
  "AAAAUQWD" varchar(20) NOT NULL default ' ',
  "AAABLPJD" varchar(4) NOT NULL default ' ',
  "AAABLUNJ" varchar(32) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default ' ',
  "AAABJDQU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNR","AAAAUQWD","AAABLPJD","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQVP" (
  "AAABLLNR" varchar(15) NOT NULL default ' ',
  "AAAAUQWD" varchar(20) NOT NULL default ' ',
  "AAAAUQVX" varchar(20) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default ' ',
  "AAABJDQU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNR","AAAAUQWD","AAAAUQVX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQVY" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUQWD" varchar(20) NOT NULL default ' ',
  "AAABLXIS" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAUQWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQWE" (
  "AAAAURAW" varchar(17) NOT NULL default ' ',
  "AAAAURAL" varchar(26) NOT NULL default ' ',
  "AAAAURAM" varchar(1) NOT NULL default ' ',
  "AAAAURAN" varchar(1) NOT NULL default ' ',
  "AAAAURAO" varchar(2) NOT NULL default ' ',
  "AAAAURAP" varchar(2) NOT NULL default ' ',
  "AAAAURAQ" int(11) NOT NULL default '0',
  "AAAAURAR" int(11) NOT NULL default '0',
  "AAAAURAS" varchar(30) NOT NULL default ' ',
  "AAAAURAT" varchar(30) NOT NULL default ' ',
  "AAAAUQWP" smallint(6) NOT NULL default '0',
  "AAAAUQWQ" text,
  PRIMARY KEY ("AAAAURAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQWS" (
  "AAAAUQWV" varchar(4) NOT NULL default ' ',
  "AAAAUQWU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUQWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQYO" (
  "AAAAUQYN" varchar(40) NOT NULL default ' ',
  "AAAAUQYO" varchar(30) NOT NULL default ' ',
  "AAAAUQZR" varchar(4) NOT NULL default ' ',
  "AAAAUQZE" varchar(3) NOT NULL default ' ',
  "AAAAUQYR" varchar(4) NOT NULL default ' ',
  "AAAAUQYS" varchar(4) NOT NULL default ' ',
  "AAAAUQXD" varchar(1) NOT NULL default ' ',
  "AAAAUQXE" varchar(1) NOT NULL default ' ',
  "AAAAUQXF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUQYN","AAAAUQYO","AAAAUQZR","AAAAUQZE","AAAAUQYR","AAAAUQYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQXM" (
  "AAAAUQZR" varchar(4) NOT NULL default ' ',
  "AAAAUQZE" varchar(3) NOT NULL default ' ',
  "AAAAUQZS" varchar(1) NOT NULL default ' ',
  "AAAAUQXQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUQZR","AAAAUQZE","AAAAUQZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQXU" (
  "AAAAUQZR" varchar(4) NOT NULL default ' ',
  "AAAAUQZS" varchar(1) NOT NULL default ' ',
  "AAAAUQXX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUQZR","AAAAUQZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQYA" (
  "AAAAUQYN" varchar(40) NOT NULL default ' ',
  "AAAAUQYO" varchar(30) NOT NULL default ' ',
  "AAAAUQZR" varchar(4) NOT NULL default ' ',
  "AAAAUQZE" varchar(3) NOT NULL default ' ',
  "AAAAUQYR" varchar(4) NOT NULL default ' ',
  "AAAAUQYS" varchar(4) NOT NULL default ' ',
  "AAAAZOEQ" varchar(12) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABHGYG" varchar(6) NOT NULL default '000000',
  "AAABLRPZ" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABIJJL" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAAUQYN","AAAAUQYO","AAAAUQZR","AAAAUQZE","AAAAUQYR","AAAAUQYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQYT" (
  "AAAAUQZR" varchar(4) NOT NULL default ' ',
  "AAAAUQZE" varchar(3) NOT NULL default ' ',
  "AAAAUQZS" varchar(1) NOT NULL default ' ',
  "AAAAZOEQ" varchar(12) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABHGYG" varchar(6) NOT NULL default '000000',
  "AAABLRPZ" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABIJJL" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAAUQZR","AAAAUQZE","AAAAUQZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQZG" (
  "AAAAUQZR" varchar(4) NOT NULL default ' ',
  "AAAAUQZS" varchar(1) NOT NULL default ' ',
  "AAAAZOEQ" varchar(12) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABHGYG" varchar(6) NOT NULL default '000000',
  "AAABLRPZ" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABIJJL" varchar(6) NOT NULL default '000000',
  "AAABLRTW" varchar(12) NOT NULL default ' ',
  "AAABLEQO" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUQZR","AAAAUQZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUQZT" (
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
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURAJ" (
  "AAAAURAW" varchar(17) NOT NULL default ' ',
  "AAAAURAL" varchar(26) NOT NULL default ' ',
  "AAAAURAM" varchar(1) NOT NULL default ' ',
  "AAAAURAN" varchar(1) NOT NULL default ' ',
  "AAAAURAO" varchar(2) NOT NULL default ' ',
  "AAAAURAP" varchar(2) NOT NULL default ' ',
  "AAAAURAQ" int(11) NOT NULL default '0',
  "AAAAURAR" int(11) NOT NULL default '0',
  "AAAAURAS" varchar(30) NOT NULL default ' ',
  "AAAAURAT" varchar(30) NOT NULL default ' ',
  "AAAAURAU" smallint(6) NOT NULL default '0',
  "AAAAURAV" text,
  PRIMARY KEY ("AAAAURAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURAX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJJLO" varchar(22) NOT NULL default ' ',
  "AAABJJLP" varchar(4) NOT NULL default '0000',
  "AAAAURBB" varchar(1) NOT NULL default ' ',
  "AAAAURBC" varchar(10) NOT NULL default ' ',
  "AAAAURBD" varchar(6) NOT NULL default '000000',
  "AAAAURBE" varchar(1) NOT NULL default ' ',
  "AAAAURBF" varchar(10) NOT NULL default ' ',
  "AAAAURBG" varchar(6) NOT NULL default '000000',
  "AAABKXBV" varchar(17) NOT NULL default ' ',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAAAURBJ" varchar(17) NOT NULL default ' ',
  "AAABLONK" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJJLO","AAABJJLP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURBO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCCPB" varchar(32) NOT NULL default ' ',
  "AAABLUZL" varchar(1) NOT NULL default ' ',
  "AAABLLLP" varchar(50) NOT NULL default ' ',
  "AAABLTST" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCCPB","AAABLUZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURBX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCCPB" varchar(32) NOT NULL default ' ',
  "AAABMOHM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCCPB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURCD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUYGO" VARBINARY(255) NOT NULL,
  "AAABEDQH" varchar(10) NOT NULL default ' ',
  "AAAAUYGY" varchar(70) NOT NULL default ' ',
  "AAAAUYHA" varchar(32) NOT NULL default ' ',
  "AAAAUYHC" varchar(2) NOT NULL default ' ',
  "AAAAUYGZ" varchar(70) NOT NULL default ' ',
  "AAAAUYHB" varchar(32) NOT NULL default ' ',
  "AAAAUYHD" varchar(2) NOT NULL default ' ',
  "AAAAUYGH" varchar(10) NOT NULL default ' ',
  "AAAAUYGI" varchar(1) NOT NULL default ' ',
  "AAAAUYGJ" varchar(10) NOT NULL default ' ',
  "AAAAUYGK" varchar(1) NOT NULL default ' ',
  "AAABDODY" decimal(15,0) NOT NULL default '0',
  "AAAAUYGM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUYGO"(255)),
  KEY "AAAAURCV" ("AAAAUYGY","AAAAUYHA","AAAAUYHC"),
  KEY "AAAAURCZ" ("AAAAUYGZ","AAAAUYHB","AAAAUYHD"),
  KEY "AAAAURDD" ("AAAAUYGY","AAAAUYGZ","AAAAUYHA","AAAAUYHB","AAAAUYHC","AAAAUYHD","AAABEDQH"),
  KEY "AAAAURDL" ("AAABLUXQ","AAABDODY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURDO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABDOEA" varchar(22) NOT NULL default ' ',
  "AAABDOED" varchar(22) NOT NULL default ' ',
  "AAABJJLO" varchar(22) NOT NULL default ' ',
  "AAABJJLF" varchar(4) NOT NULL default ' ',
  "AAABDODY" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABDOEA","AAABDOED"),
  KEY "AAAAURDY" ("AAABJJLO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUREA" (
  "AAABLLIO" varchar(4) NOT NULL default ' ',
  "AAABLUNJ" varchar(40) NOT NULL default ' ',
  "AAABMMJT" varchar(10) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLIO","AAABLUNJ","AAABMMJT"),
  KEY "AAAAUREI" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUREK" (
  "AAABLLIO" varchar(4) NOT NULL default ' ',
  "AAABLUNJ" varchar(40) NOT NULL default ' ',
  "AAABMMJT" varchar(10) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLIO","AAABLUNJ","AAABMMJT"),
  KEY "AAAAURES" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUREU" (
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  "AAABJHFW" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUREY" (
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABLGEY" varchar(16) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEHK","AAABLGEY","AAABMOIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURFF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLGEY" varchar(16) NOT NULL default ' ',
  "AAABHMRI" varchar(30) NOT NULL default ' ',
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABHEQX" varchar(48) NOT NULL default ' ',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABJTAJ" varchar(6) NOT NULL default '000000',
  "AAABIOAE" varchar(12) NOT NULL default ' ',
  "AAAAURGJ" varchar(2) NOT NULL default '00',
  "AAAAURFR" varchar(40) NOT NULL default ' ',
  "AAABLKIV" varchar(32) NOT NULL default ' ',
  "AAABJAEL" varchar(8) NOT NULL default '00000000',
  "AAABLOCX" varchar(6) NOT NULL default '000000',
  "AAABJAEN" varchar(12) NOT NULL default ' ',
  "AAAAURFW" varchar(1) NOT NULL default ' ',
  "AAABLTWP" varchar(8) NOT NULL default '00000000',
  "AAAAURFY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLGEY","AAABHMRI","AAABHJPB","AAABMOIA","AAABHEQX","AAABLEST","AAABKOUD","AAABJTAJ","AAABIOAE","AAAAURGJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLGEY" varchar(16) NOT NULL default ' ',
  "AAABHMRI" varchar(30) NOT NULL default ' ',
  "AAABIOAE" varchar(12) NOT NULL default ' ',
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABHEQX" varchar(48) NOT NULL default ' ',
  "AAABLPUZ" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLGEY","AAABHMRI","AAABIOAE","AAABHJPB","AAABMOIA","AAABHEQX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURHC" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAAAVNHH" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHJPB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURHI" (
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABHJPB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURHL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIOAE" varchar(12) NOT NULL default ' ',
  "AAABLGEY" varchar(16) NOT NULL default ' ',
  "AAABHMRI" varchar(30) NOT NULL default ' ',
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABHEQX" varchar(48) NOT NULL default ' ',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIOAE","AAABLGEY","AAABHMRI","AAABHJPB","AAABMOIA","AAABHEQX","AAABLEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURIC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLGEY" varchar(16) NOT NULL default ' ',
  "AAABHMRI" varchar(30) NOT NULL default ' ',
  "AAABIOAE" varchar(12) NOT NULL default ' ',
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABHEQX" varchar(48) NOT NULL default ' ',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  "AAABHERA" varchar(4) NOT NULL default '0000',
  "AAABHERB" varchar(4) NOT NULL default '0000',
  "AAABHERC" varchar(1) NOT NULL default ' ',
  "AAABHERD" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLGEY","AAABHMRI","AAABIOAE","AAABHJPB","AAABMOIA","AAABHEQX","AAABLEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURIY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAURJD" VARBINARY(255) NOT NULL,
  "AAAAURJE" VARBINARY(255) NOT NULL,
  PRIMARY KEY ("AAABMRIL","AAAAURJD"(255),"AAAAURJE"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURJF" (
  "AAABLFYC" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAABKMVW" varchar(4) NOT NULL default ' ',
  "AAABKMVX" varchar(120) NOT NULL default ' ',
  "AAABKMVY" varchar(4) NOT NULL default ' ',
  "AAABKMVZ" varchar(40) NOT NULL default ' ',
  "AAABLJJO" varchar(4) NOT NULL default ' ',
  "AAAAURLV" varchar(40) NOT NULL default ' ',
  "AAAAURKX" varchar(1) NOT NULL default ' ',
  "AAAAVCLU" varchar(12) NOT NULL default ' ',
  "AAAAVCKZ" varchar(8) NOT NULL default '00000000',
  "AAAAVCLA" varchar(6) NOT NULL default '000000',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  "AAAAVCLD" varchar(1) NOT NULL default ' ',
  "AAAAURLE" varchar(1) NOT NULL default ' ',
  "AAAAVCLE" varchar(1) NOT NULL default ' ',
  "AAAAVCLF" varchar(1) NOT NULL default ' ',
  "AAABMKGE" varchar(1) NOT NULL default ' ',
  "AAAAVCLH" varchar(1) NOT NULL default ' ',
  "AAABILWI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFYC","AAABLFYD","AAABKMVW","AAABKMVX","AAABKMVY","AAABKMVZ","AAABLJJO"),
  KEY "AAAAURKH" ("AAABKMVW","AAABKMVX"),
  KEY "AAAAURKK" ("AAABILWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURKM" (
  "AAABLFYC" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAABKMVW" varchar(4) NOT NULL default ' ',
  "AAABKMVX" varchar(120) NOT NULL default ' ',
  "AAABKMVY" varchar(4) NOT NULL default ' ',
  "AAABKMVZ" varchar(40) NOT NULL default ' ',
  "AAABLJJO" varchar(4) NOT NULL default ' ',
  "AAAAURLV" varchar(40) NOT NULL default ' ',
  "AAABLJIM" varchar(40) NOT NULL default ' ',
  "AAAAURLT" varchar(40) NOT NULL default ' ',
  "AAAAURKX" varchar(1) NOT NULL default ' ',
  "AAAAVCLU" varchar(12) NOT NULL default ' ',
  "AAAAVCKZ" varchar(8) NOT NULL default '00000000',
  "AAAAVCLA" varchar(6) NOT NULL default '000000',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  "AAAAVCLD" varchar(1) NOT NULL default ' ',
  "AAAAURLE" varchar(1) NOT NULL default ' ',
  "AAAAVCLE" varchar(1) NOT NULL default ' ',
  "AAAAVCLF" varchar(1) NOT NULL default ' ',
  "AAABMKGE" varchar(1) NOT NULL default ' ',
  "AAAAVCLH" varchar(1) NOT NULL default ' ',
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAAAVCLJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFYC","AAABLFYD","AAABKMVW","AAABKMVX","AAABKMVY","AAABKMVZ","AAABLJJO"),
  KEY "AAAAURLS" ("AAAAURLT"),
  KEY "AAAAURLU" ("AAAAURLV"),
  KEY "AAAAURLW" ("AAABKMVW","AAABKMVX"),
  KEY "AAAAURLZ" ("AAABILWI"),
  KEY "AAAAURMB" ("AAAAVCLU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURMD" (
  "AAABHATI" varchar(10) NOT NULL default ' ',
  "AAAAURMM" varchar(1) NOT NULL default ' ',
  "AAABLUEU" varchar(30) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLTLE" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABMGSU" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABHATI","AAAAURMM","AAABLUEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURMO" (
  "AAAAURNF" varchar(12) NOT NULL default ' ',
  "AAABHATI" varchar(64) NOT NULL default ' ',
  "AAABMLPO" varchar(5) NOT NULL default '00000',
  "AAABLOZE" varchar(64) NOT NULL default ' ',
  "AAABMKXF" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAAAURNF","AAABHATI","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURMX" (
  "AAAAURNF" varchar(12) NOT NULL default ' ',
  "AAABMLPO" varchar(5) NOT NULL default '00000',
  "AAABHATI" varchar(64) NOT NULL default ' ',
  "AAAAURNI" varchar(5) NOT NULL default '00000',
  "AAAAURNC" varchar(5) NOT NULL default '00000',
  "AAAAURND" varchar(255) NOT NULL default ' ',
  "AAAAURNE" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAAAURNF","AAABMLPO","AAABHATI","AAAAURNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURNJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZDJ" varchar(32) NOT NULL default ' ',
  "AAABMBXO" varchar(64) NOT NULL default ' ',
  "AAABLLNF" decimal(21,7) NOT NULL default '0.0000000',
  "AAABDSSV" varchar(2) NOT NULL default ' ',
  "AAABLUFA" varchar(1) NOT NULL default ' ',
  "AAABHZDD" varchar(4) NOT NULL default '0000',
  "AAABLLNH" decimal(21,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAABHZDJ","AAABMBXO","AAABLLNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURNW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZDJ" varchar(32) NOT NULL default ' ',
  "AAABMBXO" varchar(64) NOT NULL default ' ',
  "AAABDSSV" varchar(2) NOT NULL default ' ',
  "AAABLUFA" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABHZCZ" varchar(4) NOT NULL default '0000',
  "AAABHZDA" tinyint(3) unsigned NOT NULL default '0',
  "AAABLLNF" decimal(21,7) NOT NULL default '0.0000000',
  "AAABHZDD" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABHZDJ","AAABMBXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUROK" (
  "AAAAUROP" varchar(12) NOT NULL default ' ',
  "AAABKUQR" varchar(8) NOT NULL default '00000000',
  "AAABLWWO" varchar(6) NOT NULL default '000000',
  "AAAAURUX" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUROP","AAABKUQR","AAABLWWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUROS" (
  "AAABLXDT" varchar(21) NOT NULL default ' ',
  "AAABLUFA" varchar(14) NOT NULL default ' ',
  "AAAAUROV" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLXDT","AAABLUFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUROY" (
  "AAABHZDJ" varchar(32) NOT NULL default ' ',
  "AAABGFET" varchar(25) NOT NULL default ' ',
  "AAABLEEK" varchar(1) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLTLE" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABMGSU" varchar(14) NOT NULL default '00000000000000',
  "AAAAURPG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZDJ","AAABGFET","AAABLEEK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURPK" (
  "AAABHZDJ" varchar(32) NOT NULL default ' ',
  "AAAAURPM" varchar(1) NOT NULL default ' ',
  "AAAAURPN" varchar(3) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLTLE" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABMGSU" varchar(14) NOT NULL default '00000000000000',
  "AAAAURYG" varchar(10) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAAURSV" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLOZE" varchar(1) NOT NULL default ' ',
  "AAAAURPX" varchar(1) NOT NULL default ' ',
  "AAABLHPU" varchar(1) NOT NULL default ' ',
  "AAAAURPZ" varchar(1) NOT NULL default ' ',
  "AAAAURQA" varchar(1) NOT NULL default ' ',
  "AAAAURQB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURQD" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHZDJ" varchar(32) NOT NULL default ' ',
  "AAABLLLP" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHZDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURQJ" (
  "AAABHZDJ" varchar(32) NOT NULL default ' ',
  "AAAAURQR" varchar(32) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLTLE" varchar(14) NOT NULL default '00000000000000',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZDJ","AAAAURQR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURQS" (
  "AAABHZDJ" varchar(32) NOT NULL default ' ',
  "AAABHATI" varchar(10) NOT NULL default ' ',
  "AAABGFET" varchar(25) NOT NULL default ' ',
  "AAAAURRG" varchar(5) NOT NULL default '00000',
  "AAABIMAF" varchar(64) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLTLE" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABMGSU" varchar(14) NOT NULL default '00000000000000',
  "AAAAURRC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZDJ","AAABHATI","AAABGFET","AAAAURRG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURRH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHZDJ" varchar(32) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDSSV" varchar(2) NOT NULL default ' ',
  "AAAAURSK" varchar(12) NOT NULL default '000000000000',
  "AAAAURSL" varchar(10) NOT NULL default ' ',
  "AAAAURSM" varchar(12) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLTLE" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABMGSU" varchar(14) NOT NULL default '00000000000000',
  "AAABLUMJ" varchar(14) NOT NULL default '00000000000000',
  "AAABHFNT" varchar(1) NOT NULL default ' ',
  "AAAAURST" varchar(1) NOT NULL default ' ',
  "AAAAURSU" varchar(1) NOT NULL default ' ',
  "AAAAURSV" varchar(1) NOT NULL default ' ',
  "AAAAURSW" varchar(20) NOT NULL default ' ',
  "AAABLIRO" varchar(14) NOT NULL default '00000000000000',
  "AAABGWVQ" varchar(14) NOT NULL default '00000000000000',
  "AAABEXVV" varchar(1) NOT NULL default ' ',
  "AAAAURSC" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABHZDJ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURSG" (
  "AAABHZDJ" varchar(32) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDSSV" varchar(2) NOT NULL default ' ',
  "AAAAURSK" varchar(12) NOT NULL default '000000000000',
  "AAAAURSL" varchar(10) NOT NULL default ' ',
  "AAAAURSM" varchar(12) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLTLE" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABMGSU" varchar(14) NOT NULL default '00000000000000',
  "AAABLUMJ" varchar(14) NOT NULL default '00000000000000',
  "AAABHFNT" varchar(1) NOT NULL default ' ',
  "AAAAURST" varchar(1) NOT NULL default ' ',
  "AAAAURSU" varchar(1) NOT NULL default ' ',
  "AAAAURSV" varchar(1) NOT NULL default ' ',
  "AAAAURSW" varchar(20) NOT NULL default ' ',
  "AAABLIRO" varchar(14) NOT NULL default '00000000000000',
  "AAABGWVQ" varchar(14) NOT NULL default '00000000000000',
  "AAABEXVV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZDJ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURTC" (
  "AAABHZDJ" varchar(32) NOT NULL default ' ',
  "AAAAURTE" varchar(30) NOT NULL default ' ',
  "AAAAURTF" varchar(30) NOT NULL default ' ',
  "AAAAURTG" varchar(1) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURTM" (
  "AAABMINV" varchar(1) NOT NULL default ' ',
  "AAAAURTO" varchar(7) NOT NULL default '0000000',
  "AAAAURTP" varchar(7) NOT NULL default '0000000',
  "AAAAURTQ" varchar(7) NOT NULL default '0000000',
  PRIMARY KEY ("AAABMINV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURTS" (
  "AAABHZDJ" varchar(32) NOT NULL default ' ',
  "AAABMBXO" varchar(64) NOT NULL default ' ',
  "AAABLLNF" decimal(21,7) NOT NULL default '0.0000000',
  "AAABDSSV" varchar(2) NOT NULL default ' ',
  "AAABLUFA" varchar(1) NOT NULL default ' ',
  "AAABHZDD" varchar(4) NOT NULL default '0000',
  "AAABLLNH" decimal(21,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABHZDJ","AAABMBXO","AAABLLNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURUD" (
  "AAABHZDJ" varchar(32) NOT NULL default ' ',
  "AAABMBXO" varchar(64) NOT NULL default ' ',
  "AAABDSSV" varchar(2) NOT NULL default ' ',
  "AAABLUFA" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABHZCZ" varchar(4) NOT NULL default '0000',
  "AAABHZDA" tinyint(3) unsigned NOT NULL default '0',
  "AAABLLNF" decimal(21,7) NOT NULL default '0.0000000',
  "AAABHZDD" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABHZDJ","AAABMBXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURUP" (
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABLUZL" varchar(20) NOT NULL default ' ',
  "AAABMECK" varchar(64) NOT NULL default ' ',
  "AAABIXBZ" varchar(8) NOT NULL default '00000000',
  "AAAAURUV" tinyint(3) unsigned NOT NULL default '0',
  "AAABLUVS" varchar(20) NOT NULL default ' ',
  "AAAAURUX" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLZKF","AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURVA" (
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABLUVS" varchar(20) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  "AAABMQBS" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABLZKF","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURVH" (
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABLUVS" varchar(20) NOT NULL default ' ',
  "AAABMLPO" varchar(10) NOT NULL default '0000000000',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABKMWV" int(11) NOT NULL default '0',
  "AAABDSSV" varchar(2) NOT NULL default ' ',
  "AAABMLPV" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLZKF","AAABLUVS","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURVS" (
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABLUVS" varchar(20) NOT NULL default ' ',
  "AAABLUZL" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLZKF","AAABLUVS","AAABLUZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURVZ" (
  "AAAAURXT" varchar(8) NOT NULL default ' ',
  "AAABLTXI" varchar(10) NOT NULL default ' ',
  "AAABGFET" varchar(25) NOT NULL default ' ',
  "AAABIMAF" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAAAURXT","AAABLTXI","AAABGFET")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURWH" (
  "AAABLUZL" varchar(20) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  "AAABMOHM" tinyint(3) unsigned NOT NULL default '0',
  "AAAAURWL" varchar(40) NOT NULL default ' ',
  "AAAAURWM" double NOT NULL default '0',
  "AAAAURWN" smallint(6) NOT NULL default '0',
  "AAAAURWO" text,
  PRIMARY KEY ("AAABLUZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURWQ" (
  "AAAAURYG" varchar(10) NOT NULL default ' ',
  "AAABLUFA" varchar(1) NOT NULL default ' ',
  "AAAAURXQ" varchar(3) NOT NULL default '000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAAAURXT" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAAAURYG","AAABLUFA","AAAAURXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURXA" (
  "AAAAURYG" varchar(10) NOT NULL default ' ',
  "AAABLUFA" varchar(1) NOT NULL default ' ',
  "AAAAURXQ" varchar(3) NOT NULL default '000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAAAURXG" varchar(32) NOT NULL default ' ',
  "AAAAURXT" varchar(8) NOT NULL default ' ',
  "AAABKPCZ" varchar(5) NOT NULL default '00000',
  "AAAAURXJ" varchar(5) NOT NULL default '00000',
  "AAAAURXK" varchar(10) NOT NULL default '0000000000',
  "AAAAURXL" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABMGSU" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAAAURYG","AAABLUFA","AAAAURXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURXR" (
  "AAAAURYG" varchar(10) NOT NULL default ' ',
  "AAAAURXT" varchar(8) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLTLE" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABMGSU" varchar(14) NOT NULL default '00000000000000',
  "AAAAURXY" varchar(10) NOT NULL default '0000000000',
  "AAAAURXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAURYG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURYB" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAURYG" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAURYG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURYH" (
  "AAABLEUZ" varchar(12) NOT NULL default ' ',
  "AAAAURYJ" int(11) NOT NULL default '0',
  "AAAAURYK" int(11) NOT NULL default '0',
  "AAAAURYL" int(11) NOT NULL default '0',
  "AAAAURYM" int(11) NOT NULL default '0',
  "AAAAURYN" varchar(1) NOT NULL default ' ',
  "AAAAURYO" varchar(3) NOT NULL default ' ',
  "AAAAURYP" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURYR" (
  "AAABMOHM" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURYU" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURZA" (
  "AAABENKB" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABENKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURZD" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABENKB" varchar(3) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABENKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURZJ" (
  "AAAAURZR" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAAAURZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURZM" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAURZR" varchar(3) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAURZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAURZS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABDOEA" varchar(22) NOT NULL default ' ',
  "AAABDOED" varchar(22) NOT NULL default ' ',
  "AAABJJLO" varchar(22) NOT NULL default ' ',
  "AAABJJLF" varchar(4) NOT NULL default ' ',
  "AAABDODY" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABDOEA","AAABDOED"),
  KEY "AAAAUSAC" ("AAABJJLO"),
  KEY "AAAAUSAE" ("AAABDOED","AAABJJLF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSAH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUYGO" VARBINARY(255) NOT NULL,
  "AAABEDQH" varchar(10) NOT NULL default ' ',
  "AAAAUYGY" varchar(70) NOT NULL default ' ',
  "AAAAUYHA" varchar(32) NOT NULL default ' ',
  "AAAAUYHC" varchar(2) NOT NULL default ' ',
  "AAAAUYGZ" varchar(70) NOT NULL default ' ',
  "AAAAUYHB" varchar(32) NOT NULL default ' ',
  "AAAAUYHD" varchar(2) NOT NULL default ' ',
  "AAAAUYGH" varchar(10) NOT NULL default ' ',
  "AAAAUYGI" varchar(1) NOT NULL default ' ',
  "AAAAUYGJ" varchar(10) NOT NULL default ' ',
  "AAAAUYGK" varchar(1) NOT NULL default ' ',
  "AAABDODY" decimal(15,0) NOT NULL default '0',
  "AAAAUYGM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUYGO"(255)),
  KEY "AAAAUSAZ" ("AAAAUYGY","AAAAUYHA","AAAAUYHC"),
  KEY "AAAAUSBD" ("AAAAUYGZ","AAAAUYHB","AAAAUYHD"),
  KEY "AAAAUSBH" ("AAAAUYGY","AAAAUYGZ","AAAAUYHA","AAAAUYHB","AAAAUYHC","AAAAUYHD","AAABEDQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSBP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUYLG" varchar(32) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAAAUSFT" varchar(1) NOT NULL default ' ',
  "AAAAWEOI" varchar(241) NOT NULL default ' ',
  "AAABLVJJ" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUYLG","AAABLZKF","AAAAUSFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSCA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIOBO" varchar(32) NOT NULL default ' ',
  "AAABMMJT" varchar(32) NOT NULL default ' ',
  "AAAAUYTB" varchar(32) NOT NULL default ' ',
  "AAAAUSFT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIOBO","AAABMMJT","AAAAUYTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSCK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUSDB" varchar(3) NOT NULL default ' ',
  "AAAAVMBH" varchar(32) NOT NULL default ' ',
  "AAAAUSCT" varchar(32) NOT NULL default ' ',
  "AAAAUSCP" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUSDB","AAAAVMBH","AAAAUSCT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSCU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUSDB" varchar(3) NOT NULL default ' ',
  "AAAAVMBH" varchar(32) NOT NULL default ' ',
  "AAAAUSEW" varchar(32) NOT NULL default ' ',
  "AAAAVJSG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUSDB","AAAAVMBH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSDD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUSEW" varchar(32) NOT NULL default ' ',
  "AAAAUYLG" varchar(32) NOT NULL default ' ',
  "AAABIOBO" varchar(32) NOT NULL default ' ',
  "AAABCWRL" varchar(32) NOT NULL default ' ',
  "AAAAUSFT" varchar(1) NOT NULL default ' ',
  "AAABCWRM" varchar(32) NOT NULL default ' ',
  "AAAAUYQZ" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAAAUSEW","AAAAUYLG","AAABIOBO","AAABCWRL","AAAAUSFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSDS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUSEW" varchar(32) NOT NULL default ' ',
  "AAAAUSFT" varchar(1) NOT NULL default ' ',
  "AAABLLLP" varchar(50) NOT NULL default ' ',
  "AAAAUSDX" varchar(8) NOT NULL default '00000000',
  "AAABJDPL" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLUXQ","AAAAUSEW","AAAAUSFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSEC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUSEW" varchar(32) NOT NULL default ' ',
  "AAABIOBO" varchar(32) NOT NULL default ' ',
  "AAAAUSFT" varchar(1) NOT NULL default ' ',
  "AAABLLLP" varchar(50) NOT NULL default ' ',
  "AAABMALF" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLUXQ","AAAAUSEW","AAABIOBO","AAAAUSFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSEN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUSEW" varchar(32) NOT NULL default ' ',
  "AAAAUSFT" varchar(1) NOT NULL default ' ',
  "AAABIJSB" varchar(2) NOT NULL default '00',
  "AAABMOHM" varchar(3) NOT NULL default ' ',
  "AAABIYJF" varchar(241) NOT NULL default ' ',
  "AAABLLLP" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUSEW","AAAAUSFT","AAABIJSB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSEZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUSFI" varchar(32) NOT NULL default ' ',
  "AAABIOBO" varchar(32) NOT NULL default ' ',
  "AAABIJSB" varchar(32) NOT NULL default ' ',
  "AAAAUSFT" varchar(1) NOT NULL default ' ',
  "AAAAUXDW" varchar(241) NOT NULL default ' ',
  "AAAAUYTB" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUSFI","AAABIOBO","AAABIJSB","AAAAUSFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSFM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHBKQ" varchar(65) NOT NULL default ' ',
  "AAAAUSFT" varchar(1) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABHBKQ","AAAAUSFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSFU" (
  "AAAAUXOM" varchar(32) NOT NULL default ' ',
  "AAAAUSGY" varchar(32) NOT NULL default ' ',
  "AAAAUSFX" varchar(1) NOT NULL default ' ',
  "AAAAUSFY" varchar(1) NOT NULL default ' ',
  "AAAAUSFZ" varchar(1) NOT NULL default ' ',
  "AAAAUSGA" varchar(1) NOT NULL default ' ',
  "AAAAUXHW" decimal(15,0) NOT NULL default '0',
  "AAAAUXYA" decimal(15,0) NOT NULL default '0',
  "AAAAUXHY" varchar(12) NOT NULL default ' ',
  "AAAAUXYB" varchar(12) NOT NULL default ' ',
  "AAAAUXFW" varchar(32) NOT NULL default ' ',
  "AAAAUXFX" varchar(1) NOT NULL default ' ',
  "AAAAUXFY" varchar(1) NOT NULL default ' ',
  "AAAAUXFZ" varchar(1) NOT NULL default ' ',
  "AAAAUXGA" varchar(1) NOT NULL default ' ',
  "AAAAUXGB" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAAUXOM","AAAAUSGY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSGN" (
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAAAUXOM" varchar(32) NOT NULL default ' ',
  "AAAAUSGY" varchar(32) NOT NULL default ' ',
  "AAAAUXIE" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFZK","AAAAUXOM","AAAAUSGY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSGV" (
  "AAAAUXGM" varchar(32) NOT NULL default ' ',
  "AAAAUXOM" varchar(32) NOT NULL default ' ',
  "AAAAUSGY" varchar(32) NOT NULL default ' ',
  "AAAAUSGZ" varchar(64) NOT NULL default ' ',
  "AAAAUXHW" decimal(15,0) NOT NULL default '0',
  "AAAAUXYA" decimal(15,0) NOT NULL default '0',
  "AAAAUXHY" varchar(12) NOT NULL default ' ',
  "AAAAUXYB" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUXGM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSHF" (
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAAAUXGM" varchar(32) NOT NULL default ' ',
  "AAAAUXIE" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFZK","AAAAUXGM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSHL" (
  "AAAAUXOM" varchar(32) NOT NULL default ' ',
  "AAABGEXH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUXOM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSHP" (
  "AAAAUXVO" varchar(6) NOT NULL default ' ',
  "AAAAUSHR" varchar(30) NOT NULL default ' ',
  "AAAAUSHS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUXVO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSHU" (
  "AAAAUXVO" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAUSHX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUXVO","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSIA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUXVO" varchar(6) NOT NULL default ' ',
  "AAABIYCS" varchar(8) NOT NULL default ' ',
  "AAAAUTGW" varchar(8) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAUSIG" varchar(40) NOT NULL default ' ',
  "AAAAUSIH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUXVO","AAABIYCS","AAAAUTGW","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSIN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUXVO" varchar(6) NOT NULL default ' ',
  "AAABIYCS" varchar(8) NOT NULL default ' ',
  "AAAAUTGW" varchar(8) NOT NULL default ' ',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUXVO","AAABIYCS","AAAAUTGW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSIX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUXVO" varchar(6) NOT NULL default ' ',
  "AAABIYCS" varchar(8) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAUSJC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUXVO","AAABIYCS","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSJH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUXVO" varchar(6) NOT NULL default ' ',
  "AAABIYCS" varchar(8) NOT NULL default ' ',
  "AAAAUSJL" varchar(30) NOT NULL default ' ',
  "AAAAUSJM" varchar(14) NOT NULL default ' ',
  "AAAAUUZZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUXVO","AAABIYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSJR" (
  "AAAAUXOM" varchar(32) NOT NULL default ' ',
  "AAAAUXHW" decimal(15,0) NOT NULL default '0',
  "AAAAUXYA" decimal(15,0) NOT NULL default '0',
  "AAAAUXHY" varchar(12) NOT NULL default ' ',
  "AAAAUXYB" varchar(12) NOT NULL default ' ',
  "AAAAUSJX" varchar(16) NOT NULL default ' ',
  "AAAAUSJY" varchar(16) NOT NULL default ' ',
  "AAAAUSJZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUXOM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSKB" (
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAAAUXOM" varchar(32) NOT NULL default ' ',
  "AAAAUXIE" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFZK","AAAAUXOM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSKH" (
  "AAAAUXON" varchar(32) NOT NULL default ' ',
  "AAABHRRK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUXON")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSKL" (
  "AAAAUXIL" varchar(32) NOT NULL default ' ',
  "AAABGEXH" varchar(30) NOT NULL default ' ',
  "AAAAUXDW" varchar(32) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAAAUXEG" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAAAUXIL"),
  KEY "AAAAUSKS" ("AAAAUXEG"),
  KEY "AAAAUSKU" ("AAAAUXDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSKW" (
  "AAABIXSA" varchar(1) NOT NULL default ' ',
  "AAAAUSKY" varchar(1) NOT NULL default ' ',
  "AAAAUSKZ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABIXSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSLB" (
  "AAAAUXEC" varchar(32) NOT NULL default ' ',
  "AAAAUXED" varchar(32) NOT NULL default ' ',
  "AAAAUXEE" varchar(2) NOT NULL default ' ',
  "AAAAUXEG" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAAAUXEC","AAAAUXED","AAAAUXEE"),
  KEY "AAAAUSLJ" ("AAAAUXEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSLL" (
  "AAAAUXOM" varchar(32) NOT NULL default ' ',
  "AAAAUXEE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUXOM","AAAAUXEE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSLQ" (
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAAAUXOM" varchar(32) NOT NULL default ' ',
  "AAAAUXEE" varchar(2) NOT NULL default ' ',
  "AAAAUXIE" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFZK","AAAAUXOM","AAAAUXEE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSLY" (
  "AAAAUXOM" varchar(32) NOT NULL default ' ',
  "AAAAUXIG" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUXOM","AAAAUXIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSMD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUYBY" varchar(32) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUYBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSMJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAUSMM" varchar(1) NOT NULL default ' ',
  "AAAAUSMN" tinyint(3) unsigned NOT NULL default '0',
  "AAAAUUTB" varchar(1) NOT NULL default ' ',
  "AAAAUUTC" varchar(1) NOT NULL default ' ',
  "AAABDPTW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSMT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAUUTP" int(11) NOT NULL default '0',
  "AAABLUSV" varchar(12) NOT NULL default ' ',
  "AAABLFTX" int(11) NOT NULL default '0',
  "AAAAUUTM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMDEN","AAAAUUTP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSND" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAUUTP" int(11) NOT NULL default '0',
  "AAABLUSV" varchar(12) NOT NULL default ' ',
  "AAABLFTX" int(11) NOT NULL default '0',
  "AAAAUUTM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMDEN","AAAAUUTP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSNN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUSNP" longtext,
  "AAAAUSNQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSNS" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSNZ" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSOG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSOP" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSOW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSPF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSPO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSPX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSQG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSQP" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSQW" (
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
CREATE TABLE "AAAAUSRH" (
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
CREATE TABLE "AAAAUSRS" (
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
CREATE TABLE "AAAAUSSF" (
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
CREATE TABLE "AAAAUSSQ" (
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
CREATE TABLE "AAAAUSTD" (
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
CREATE TABLE "AAAAUSTQ" (
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
CREATE TABLE "AAAAUSUD" (
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
CREATE TABLE "AAAAUSUQ" (
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
CREATE TABLE "AAAAUSVD" (
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
CREATE TABLE "AAAAUSVO" (
  "AAAAUXHT" varchar(32) NOT NULL default ' ',
  "AAAAUXHW" decimal(15,0) NOT NULL default '0',
  "AAAAUXYA" decimal(15,0) NOT NULL default '0',
  "AAAAUXHY" varchar(12) NOT NULL default ' ',
  "AAAAUXYB" varchar(12) NOT NULL default ' ',
  "AAAAUSVU" varchar(1) NOT NULL default ' ',
  "AAAAUSVV" varchar(32) NOT NULL default ' ',
  "AAAAUSVW" varchar(32) NOT NULL default ' ',
  "AAAAUSVX" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUXHT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSVZ" (
  "AAAAUXHT" varchar(32) NOT NULL default ' ',
  "AAAAUXGE" varchar(32) NOT NULL default ' ',
  "AAAAUSWC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUXHT","AAAAUXGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSWF" (
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAAAUXHT" varchar(32) NOT NULL default ' ',
  "AAAAUXIE" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFZK","AAAAUXHT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSWL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEEW" varchar(30) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLEEW","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSWT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEEW" varchar(30) NOT NULL default ' ',
  "AAABLDNU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLEEW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSWZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUSZA" varchar(32) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUSZA","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSXH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUSXW" varchar(30) NOT NULL default ' ',
  "AAAAUSXX" int(11) NOT NULL default '0',
  "AAAAUSZA" varchar(30) NOT NULL default ' ',
  "AAAAUSXM" varchar(30) NOT NULL default ' ',
  "AAAAUSXN" varchar(30) NOT NULL default ' ',
  "AAAAUSXO" varchar(30) NOT NULL default ' ',
  "AAAAUSYS" varchar(30) NOT NULL default ' ',
  "AAAAUSYF" varchar(30) NOT NULL default ' ',
  "AAAAUSYK" varchar(30) NOT NULL default ' ',
  "AAAAUSXS" varchar(30) NOT NULL default ' ',
  "AAAAUSYV" varchar(30) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUSXW","AAAAUSXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSXY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUSZA" varchar(30) NOT NULL default ' ',
  "AAABLEBM" varchar(2) NOT NULL default ' ',
  "AAAAUSYK" varchar(30) NOT NULL default ' ',
  "AAABLEEW" varchar(25) NOT NULL default ' ',
  "AAABGZSJ" varchar(30) NOT NULL default ' ',
  "AAAAUSYF" varchar(30) NOT NULL default ' ',
  "AAAAUSYG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUSZA","AAABLEBM","AAAAUSYK","AAABLEEW","AAABGZSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSYN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUSZA" varchar(30) NOT NULL default ' ',
  "AAAAUSYQ" varchar(30) NOT NULL default ' ',
  "AAAAUSYR" varchar(30) NOT NULL default ' ',
  "AAAAUSYS" varchar(30) NOT NULL default ' ',
  "AAAAUSYT" varchar(30) NOT NULL default ' ',
  "AAAAUSYU" varchar(30) NOT NULL default ' ',
  "AAAAUSYV" varchar(30) NOT NULL default ' ',
  "AAAAUSYW" varchar(1) NOT NULL default ' ',
  "AAAAUSYX" varchar(1) NOT NULL default ' ',
  "AAABDVWP" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAUSZA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSZB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEEW" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLEEW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSZG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUSZS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTAE" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTAO" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTAY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUXHT" varchar(32) NOT NULL default ' ',
  "AAAAUXXM" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUXHT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTBE" (
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
CREATE TABLE "AAAAUTBZ" (
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
CREATE TABLE "AAAAUTCS" (
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
  PRIMARY KEY ("AAABMRIL","AAABHZJH"),
  KEY "AAAAUTDU" ("AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTDW" (
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
  PRIMARY KEY ("AAABHZJH"),
  KEY "AAAAUTEW" ("AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTEY" (
  "AAABMLPV" varchar(10) NOT NULL default ' ',
  "AAABGFCP" varchar(32) NOT NULL default ' ',
  "AAAAUYBY" varchar(32) NOT NULL default ' ',
  "AAAAUXCY" varchar(32) NOT NULL default ' ',
  "AAAAUTFD" varchar(10) NOT NULL default ' ',
  "AAAAUTFE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV","AAABGFCP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTFH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUXAV" varchar(32) NOT NULL default ' ',
  "AAABLUVS" varchar(32) NOT NULL default ' ',
  "AAAAUTFL" varchar(128) NOT NULL default ' ',
  "AAAAUTFM" varchar(128) NOT NULL default ' ',
  "AAAAUTFN" varchar(128) NOT NULL default ' ',
  "AAABLPSU" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUXAV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTFR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUTHB" varchar(12) NOT NULL default '000000000000',
  "AAAAUXVO" varchar(6) NOT NULL default ' ',
  "AAAAUXVP" varchar(12) NOT NULL default '000000000000',
  "AAAAUTFW" varchar(32) NOT NULL default ' ',
  "AAAAZOEQ" varchar(12) NOT NULL default ' ',
  "AAABLTZM" varchar(8) NOT NULL default '00000000',
  "AAAAUTFZ" varchar(6) NOT NULL default '000000',
  "AAAAUXCY" varchar(32) NOT NULL default ' ',
  "AAAAUYBY" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUTHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTGE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUTHB" varchar(12) NOT NULL default '000000000000',
  "AAAAUXIL" varchar(32) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAAAUTGJ" varchar(12) NOT NULL default ' ',
  "AAAAUTGK" varchar(8) NOT NULL default '00000000',
  "AAAAUTGL" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAAAUTHB","AAAAUXIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTGP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUTHB" varchar(12) NOT NULL default '000000000000',
  "AAAAUXIL" varchar(32) NOT NULL default ' ',
  "AAABKRJY" varchar(5) NOT NULL default '00000',
  "AAABLQVJ" varchar(5) NOT NULL default '00000',
  "AAABIYCS" varchar(8) NOT NULL default ' ',
  "AAAAUTGW" varchar(8) NOT NULL default ' ',
  "AAAAUTGX" varchar(12) NOT NULL default ' ',
  "AAABKWVU" varchar(8) NOT NULL default '00000000',
  "AAABLOCY" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAAAUTHB","AAAAUXIL","AAABKRJY","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTHF" (
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
CREATE TABLE "AAAAUTHY" (
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
CREATE TABLE "AAAAUTIY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAUUTB" varchar(1) NOT NULL default ' ',
  "AAAAUUTC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTJF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAUUTP" int(11) NOT NULL default '0',
  "AAABLUSV" varchar(12) NOT NULL default ' ',
  "AAABLFTX" int(11) NOT NULL default '0',
  "AAAAUUTM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMDEN","AAAAUUTP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTJP" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJGGK" varchar(16) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABJGGK","AAABLLMD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTJX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTWR" varchar(42) NOT NULL default ' ',
  "AAABLFYD" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTWR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTKD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRQO" varchar(42) NOT NULL default ' ',
  "AAABMKFU" decimal(21,7) NOT NULL default '0.0000000',
  "AAABLJJO" varchar(8) NOT NULL default ' ',
  "AAAAUWQF" varchar(12) NOT NULL default ' ',
  "AAAAUTKJ" int(11) NOT NULL default '0',
  "AAAAUTKK" int(11) NOT NULL default '0',
  "AAAAUTKL" varchar(32) NOT NULL default ' ',
  "AAAAUTKM" longtext,
  PRIMARY KEY ("AAABMRIL","AAABLRQO","AAABMKFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTKQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWNJY" varchar(32) NOT NULL default ' ',
  "AAAAUTMP" varchar(32) NOT NULL default ' ',
  "AAABFNHW" varchar(32) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLLLP" longtext,
  "AAAAUTKX" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWNJY","AAAAUTMP","AAABFNHW","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTLD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWNJY" varchar(32) NOT NULL default ' ',
  "AAAAUTMP" varchar(32) NOT NULL default ' ',
  "AAABFNHW" varchar(32) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAAAUTMS" longtext,
  PRIMARY KEY ("AAABMRIL","AAAAWNJY","AAAAUTMP","AAABFNHW","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTLP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWNJY" varchar(32) NOT NULL default ' ',
  "AAAAUTMP" varchar(32) NOT NULL default ' ',
  "AAABFNHW" varchar(32) NOT NULL default ' ',
  "AAABMOHM" longtext,
  "AAABMLPV" longtext,
  "AAABHMMT" varchar(32) NOT NULL default ' ',
  "AAAAUTLX" varchar(32) NOT NULL default ' ',
  "AAAAUTLY" varchar(32) NOT NULL default ' ',
  "AAAAUTMS" longtext,
  "AAABLGEL" varchar(32) NOT NULL default ' ',
  "AAABHFFQ" varchar(64) NOT NULL default ' ',
  "AAAAUTMC" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWNJY","AAAAUTMP","AAABFNHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTMH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWNJY" varchar(32) NOT NULL default ' ',
  "AAAAUTMP" varchar(32) NOT NULL default ' ',
  "AAAAUTML" varchar(1) NOT NULL default ' ',
  "AAABLTXI" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAWNJY","AAAAUTMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTMQ" (
  "AAAAWNJY" varchar(32) NOT NULL default ' ',
  "AAAAUTMS" longtext,
  PRIMARY KEY ("AAAAWNJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTMU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
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
  PRIMARY KEY ("AAABMRIL","AAABHZDJ","AAABHZJH"),
  KEY "AAAAUTNK" ("AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTNM" (
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
  KEY "AAAAUTOA" ("AAABMRFO","AAABMLQA","AAABHZDJ"),
  KEY "AAAAUTOE" ("AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTOG" (
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
  KEY "AAAAUTOU" ("AAABMRFO","AAABMLQA","AAABHZDJ"),
  KEY "AAAAUTOY" ("AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTPA" (
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
  KEY "AAAAUTPO" ("AAABMRFO","AAABMLQA","AAABHZDJ"),
  KEY "AAAAUTPS" ("AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTPU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
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
  PRIMARY KEY ("AAABMRIL","AAABHZDJ","AAABHZJH"),
  KEY "AAAAUTQK" ("AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTQM" (
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
  KEY "AAAAUTRA" ("AAABMRFO","AAABMLQA","AAABHZDJ"),
  KEY "AAAAUTRE" ("AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTRG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
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
  PRIMARY KEY ("AAABMRIL","AAABHZDJ","AAABHZJH"),
  KEY "AAAAUTRW" ("AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTRY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
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
  PRIMARY KEY ("AAABMRIL","AAABHZDJ","AAABHZJH"),
  KEY "AAAAUTSO" ("AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
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
  PRIMARY KEY ("AAABMRIL","AAABHZDJ","AAABHZJH"),
  KEY "AAAAUTTG" ("AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTTI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
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
  PRIMARY KEY ("AAABMRIL","AAABHZDJ","AAABHZJH"),
  KEY "AAAAUTTY" ("AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTUA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHSD" varchar(32) NOT NULL default ' ',
  "AAABJTAQ" varchar(64) NOT NULL default ' ',
  "AAABLPSU" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHSD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTUH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUTUM" varchar(32) NOT NULL default ' ',
  "AAABIHSD" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUTUM","AAABIHSD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTUO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAUXXM" varchar(32) NOT NULL default ' ',
  "AAABFGDI" varchar(32) NOT NULL default ' ',
  "AAAAUTVA" varchar(32) NOT NULL default ' ',
  "AAAAUTUU" varchar(32) NOT NULL default ' ',
  "AAABMRCK" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMDEN","AAAAUXXM","AAABFGDI","AAAAUTVA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTVB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(52) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTVM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTVV" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTWC" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTWJ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTWQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTWZ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTXG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTXP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTXY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTYH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTYQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUTZC" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTZG" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUTZQ" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUTZU" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUUAE" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUAI" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUUAS" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUAW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUUBI" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUBM" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUUBW" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUCA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUUCM" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUCQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUUDC" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUDG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUUDS" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUDW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUUEI" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUEM" (
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
  UNIQUE KEY "AAAAUUFB" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUUFE" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUFG" (
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
  UNIQUE KEY "AAAAUUFT" ("AAABLLNL"),
  KEY "AAAAUUFV" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUFX" (
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
  UNIQUE KEY "AAAAUUGK" ("AAABLLNL"),
  KEY "AAAAUUGM" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUGO" (
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
  UNIQUE KEY "AAAAUUHB" ("AAABLLNL"),
  KEY "AAAAUUHD" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUHF" (
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
  UNIQUE KEY "AAAAUUHU" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUUHX" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUHZ" (
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
  UNIQUE KEY "AAAAUUIM" ("AAABLLNL"),
  KEY "AAAAUUIO" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUIQ" (
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
  UNIQUE KEY "AAAAUUJF" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUUJI" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUJK" (
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
  UNIQUE KEY "AAAAUUJZ" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUUKC" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUKE" (
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
  UNIQUE KEY "AAAAUUKT" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUUKW" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUKY" (
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
  UNIQUE KEY "AAAAUULN" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUULQ" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUULS" (
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
  UNIQUE KEY "AAAAUUMH" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUUMK" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUMM" (
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
  UNIQUE KEY "AAAAUUMZ" ("AAABLLNL"),
  KEY "AAAAUUNB" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUND" (
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
  UNIQUE KEY "AAAAUUNQ" ("AAABLLNL"),
  KEY "AAAAUUNS" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUNU" (
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
  UNIQUE KEY "AAAAUUOH" ("AAABLLNL"),
  KEY "AAAAUUOJ" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUOL" (
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
  UNIQUE KEY "AAAAUUPA" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUUPD" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUPF" (
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
  UNIQUE KEY "AAAAUUPS" ("AAABLLNL"),
  KEY "AAAAUUPU" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUPW" (
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
  UNIQUE KEY "AAAAUUQL" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUUQO" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUQQ" (
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
  UNIQUE KEY "AAAAUURF" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUURI" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUURK" (
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
  UNIQUE KEY "AAAAUURZ" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUUSC" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUSE" (
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
  UNIQUE KEY "AAAAUUST" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUUSW" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUSY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAUUTB" varchar(1) NOT NULL default ' ',
  "AAAAUUTC" varchar(1) NOT NULL default ' ',
  "AAABDPTW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUTG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAUUTP" int(11) NOT NULL default '0',
  "AAABLUSV" varchar(12) NOT NULL default ' ',
  "AAABLFTX" int(11) NOT NULL default '0',
  "AAAAUUTM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMDEN","AAAAUUTP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUTQ" (
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
CREATE TABLE "AAAAUUUJ" (
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
CREATE TABLE "AAAAUUVJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABKYTL" varchar(5) NOT NULL default ' ',
  "AAABFGDI" varchar(32) NOT NULL default ' ',
  "AAABMDXP" varchar(64) NOT NULL default ' ',
  "AAABHHAD" varchar(64) NOT NULL default ' ',
  "AAABCTLF" int(11) NOT NULL default '0',
  "AAAAUWQD" varchar(32) NOT NULL default ' ',
  "AAAAUUXW" varchar(1) NOT NULL default ' ',
  "AAABEZGK" varchar(64) NOT NULL default ' ',
  "AAAAUUXY" varchar(46) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMDEN","AAABKYTL","AAABFGDI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUVZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABFGDI" varchar(32) NOT NULL default ' ',
  "AAABKYTL" smallint(6) NOT NULL default '0',
  "AAABHHSB" decimal(15,0) NOT NULL default '0',
  "AAABKCAK" decimal(15,0) NOT NULL default '0',
  "AAABLTXO" varchar(46) NOT NULL default ' ',
  "AAABMRCK" longtext,
  "AAAAUUWI" longblob,
  PRIMARY KEY ("AAABLUXQ","AAABMDEN","AAABFGDI","AAABKYTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUWN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKGBL" varchar(32) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAUXCY" varchar(32) NOT NULL default ' ',
  "AAAAUUWS" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABKGBL","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUWW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABFGDI" varchar(32) NOT NULL default ' ',
  "AAAAUUXL" varchar(32) NOT NULL default ' ',
  "AAAAUUXB" varchar(8) NOT NULL default '00000000',
  "AAAAUUXC" varchar(8) NOT NULL default '00000000',
  "AAABMNRG" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLTXO" varchar(46) NOT NULL default ' ',
  "AAABMRCK" longtext,
  "AAABLUFA" longtext,
  PRIMARY KEY ("AAABLUXQ","AAABMDEN","AAABFGDI","AAAAUUXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUXM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLPVQ" varchar(30) NOT NULL default ' ',
  "AAAAUUZQ" varchar(3) NOT NULL default ' ',
  "AAABFGDI" varchar(32) NOT NULL default ' ',
  "AAABKYTL" varchar(5) NOT NULL default ' ',
  "AAABMDXP" varchar(64) NOT NULL default ' ',
  "AAABHHAD" varchar(64) NOT NULL default ' ',
  "AAABCTLF" int(11) NOT NULL default '0',
  "AAAAUWQD" varchar(32) NOT NULL default ' ',
  "AAAAUUXW" varchar(1) NOT NULL default ' ',
  "AAABEZGK" varchar(64) NOT NULL default ' ',
  "AAAAUUXY" varchar(46) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLPVQ","AAAAUUZQ","AAABFGDI","AAABKYTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUYE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPVQ" varchar(30) NOT NULL default ' ',
  "AAAAUUZQ" varchar(3) NOT NULL default ' ',
  "AAABFGDI" varchar(32) NOT NULL default ' ',
  "AAABKYTL" varchar(5) NOT NULL default ' ',
  "AAABEZGK" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABLPVQ","AAAAUUZQ","AAABFGDI","AAABKYTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUYS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLPVQ" varchar(30) NOT NULL default ' ',
  "AAABFGDI" varchar(32) NOT NULL default ' ',
  "AAAAUUZQ" varchar(3) NOT NULL default ' ',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAABLEBK" decimal(15,0) NOT NULL default '0',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  "AAABKMZH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLPVQ","AAABFGDI","AAAAUUZQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUZF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPVQ" varchar(30) NOT NULL default ' ',
  "AAABFGDI" varchar(32) NOT NULL default ' ',
  "AAAAUUZQ" varchar(3) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABLPVQ","AAABFGDI","AAAAUUZQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUZR" (
  "AAABMOHM" varchar(64) NOT NULL default ' ',
  "AAABMLZS" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUUZV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUXVO" varchar(6) NOT NULL default ' ',
  "AAABLFYC" varchar(2) NOT NULL default ' ',
  "AAAAUUZZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUXVO","AAABLFYC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUVAD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAUXVO" varchar(6) NOT NULL default ' ',
  "AAABLFYC" varchar(2) NOT NULL default ' ',
  "AAAAUVAI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAUXVO","AAABLFYC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUVAN" (
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
CREATE TABLE "AAAAUVAZ" (
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
CREATE TABLE "AAAAUVBL" (
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
CREATE TABLE "AAAAUVBZ" (
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
CREATE TABLE "AAAAUVCL" (
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
CREATE TABLE "AAAAUVCZ" (
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
CREATE TABLE "AAAAUVDN" (
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
CREATE TABLE "AAAAUVEB" (
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
CREATE TABLE "AAAAUVEP" (
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
CREATE TABLE "AAAAUVFD" (
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
CREATE TABLE "AAAAUVFP" (
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
  UNIQUE KEY "AAAAUVGD" ("AAABLLNL"),
  KEY "AAAAUVGF" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUVGH" (
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
  UNIQUE KEY "AAAAUVGV" ("AAABLLNL"),
  KEY "AAAAUVGX" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUVGZ" (
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
  PRIMARY KEY ("AAABMRIL","AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAAUVHP" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUVHS" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUVHU" (
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
  UNIQUE KEY "AAAAUVII" ("AAABLLNL"),
  KEY "AAAAUVIK" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUVIM" (
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
  PRIMARY KEY ("AAABMRIL","AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAAUVJC" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUVJF" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUVJH" (
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
  PRIMARY KEY ("AAABMRIL","AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAAUVJX" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUVKA" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUVKC" (
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
  PRIMARY KEY ("AAABMRIL","AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAAUVKS" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUVKV" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUVKX" (
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
  PRIMARY KEY ("AAABMRIL","AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAAUVLN" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUVLQ" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUVLS" (
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
  PRIMARY KEY ("AAABMRIL","AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAAUVMI" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUVML" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUVMN" (
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
  UNIQUE KEY "AAAAUVNB" ("AAABLLNL"),
  KEY "AAAAUVND" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUVNF" (
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
  UNIQUE KEY "AAAAUVNT" ("AAABLLNL"),
  KEY "AAAAUVNV" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUVNX" (
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
  UNIQUE KEY "AAAAUVOL" ("AAABLLNL"),
  KEY "AAAAUVON" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUVOP" (
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
  UNIQUE KEY "AAAAUVPF" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUVPI" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUVPK" (
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
  UNIQUE KEY "AAAAUVPY" ("AAABLLNL"),
  KEY "AAAAUVQA" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUVQC" (
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
  UNIQUE KEY "AAAAUVQS" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUVQV" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUVQX" (
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
  UNIQUE KEY "AAAAUVRN" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUVRQ" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUVRS" (
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
  UNIQUE KEY "AAAAUVSI" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUVSL" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUVSN" (
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
  UNIQUE KEY "AAAAUVTD" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUVTG" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUVTI" (
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
  UNIQUE KEY "AAAAUVTY" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUVUB" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUVUD" (
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
  UNIQUE KEY "AAAAUVUR" ("AAABLLNL"),
  KEY "AAAAUVUT" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUVUV" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUVVF" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUVVJ" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUVVT" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUVVX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUVWJ" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUVWN" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUVWX" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUVXB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUVXN" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUVXR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUVYD" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUVYH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUVYT" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUVYX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUVZJ" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUVZN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUVZZ" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUWAD" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUWAN" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUWAR" (
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
  UNIQUE KEY "AAAAUWBE" ("AAABLLNL"),
  KEY "AAAAUWBG" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUWBI" (
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
  UNIQUE KEY "AAAAUWBV" ("AAABLLNL"),
  KEY "AAAAUWBX" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUWBZ" (
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
  UNIQUE KEY "AAAAUWCO" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUWCR" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUWCT" (
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
  UNIQUE KEY "AAAAUWDG" ("AAABLLNL"),
  KEY "AAAAUWDI" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUWDK" (
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
  UNIQUE KEY "AAAAUWDZ" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUWEC" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUWEE" (
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
  UNIQUE KEY "AAAAUWET" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUWEW" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUWEY" (
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
  UNIQUE KEY "AAAAUWFN" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUWFQ" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUWFS" (
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
  UNIQUE KEY "AAAAUWGH" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUWGK" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUWGM" (
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
  UNIQUE KEY "AAAAUWHB" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUWHE" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUWHG" (
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
  UNIQUE KEY "AAAAUWHT" ("AAABLLNL"),
  KEY "AAAAUWHV" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUWHX" (
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
  UNIQUE KEY "AAAAUWIK" ("AAABLLNL"),
  KEY "AAAAUWIM" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUWIO" (
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
  UNIQUE KEY "AAAAUWJB" ("AAABLLNL"),
  KEY "AAAAUWJD" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUWJF" (
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
  UNIQUE KEY "AAAAUWJU" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUWJX" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUWJZ" (
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
  UNIQUE KEY "AAAAUWKM" ("AAABLLNL"),
  KEY "AAAAUWKO" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUWKQ" (
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
  UNIQUE KEY "AAAAUWLF" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUWLI" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUWLK" (
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
  UNIQUE KEY "AAAAUWLZ" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUWMC" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUWME" (
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
  UNIQUE KEY "AAAAUWMT" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUWMW" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUWMY" (
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
  UNIQUE KEY "AAAAUWNN" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUWNQ" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUWNS" (
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
  UNIQUE KEY "AAAAUWOH" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAUWOK" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUWOM" (
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
  UNIQUE KEY "AAAAUWOZ" ("AAABLLNL"),
  KEY "AAAAUWPB" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUWPD" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUWPN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUWQD" varchar(32) NOT NULL default ' ',
  "AAABMKFU" decimal(21,7) NOT NULL default '0.0000000',
  "AAAAUWQF" varchar(12) NOT NULL default ' ',
  "AAAAUWPS" varchar(32) NOT NULL default ' ',
  "AAABIZCB" varchar(32) NOT NULL default ' ',
  "AAABDKYN" varchar(32) NOT NULL default ' ',
  "AAABJTIL" varchar(128) NOT NULL default ' ',
  "AAABJTBL" varchar(128) NOT NULL default ' ',
  "AAAAUWPX" longtext,
  "AAAAUWPY" varchar(50) NOT NULL default ' ',
  "AAAAUWPZ" varchar(8) NOT NULL default '00000000',
  "AAABLNSA" varchar(128) NOT NULL default ' ',
  "AAAAUWQB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUWQD","AAABMKFU","AAAAUWQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUWQG" (
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
CREATE TABLE "AAAAUWRB" (
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
CREATE TABLE "AAAAUWRW" (
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
  PRIMARY KEY ("AAABMRIL","AAABHZJH"),
  KEY "AAAAUWSY" ("AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUWTA" (
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
  PRIMARY KEY ("AAABMRIL","AAABHZJH"),
  KEY "AAAAUWUC" ("AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUWUE" (
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
CREATE TABLE "AAAAUWUZ" (
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
  PRIMARY KEY ("AAABMRIL","AAABHZJH"),
  KEY "AAAAUWWB" ("AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUWWD" (
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
CREATE TABLE "AAAAUWWY" (
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
CREATE TABLE "AAAAUWXR" (
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
  PRIMARY KEY ("AAABMRIL","AAABHZJH"),
  KEY "AAAAUWYT" ("AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUWYV" (
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
  PRIMARY KEY ("AAABHZJH"),
  KEY "AAAAUWZV" ("AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUWZX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUXAV" varchar(32) NOT NULL default ' ',
  "AAAAUXAG" varchar(32) NOT NULL default ' ',
  "AAAAUXAK" varchar(50) NOT NULL default ' ',
  "AAABLMOZ" longtext NOT NULL,
  "AAAAUXAD" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAUXAV","AAAAUXAG"),
  KEY "AAAAUXAH" ("AAABMRIL","AAAAUXAV","AAAAUXAK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXAL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUXAV" varchar(32) NOT NULL default ' ',
  "AAAAUXCY" varchar(32) NOT NULL default ' ',
  "AAAAUXAP" varchar(3) NOT NULL default ' ',
  "AAAAUXAQ" varchar(32) NOT NULL default ' ',
  "AAAAUXAR" varchar(3) NOT NULL default ' ',
  "AAAAUXAS" varchar(32) NOT NULL default ' ',
  "AAAAUXCP" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUXAV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXAW" (
  "AAABJSNO" varchar(32) NOT NULL default ' ',
  "AAABJSLF" varchar(30) NOT NULL default ' ',
  "AAABLTXO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABJSNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXBB" (
  "AAABJSNO" varchar(32) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABJSNO","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXBH" (
  "AAABJSNO" varchar(32) NOT NULL default ' ',
  "AAAAUXBN" varchar(3) NOT NULL default ' ',
  "AAABJSLF" varchar(30) NOT NULL default ' ',
  "AAABBIEG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJSNO","AAAAUXBN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXBO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOAT" varchar(32) NOT NULL default ' ',
  "AAAAUXCY" varchar(32) NOT NULL default ' ',
  "AAAAUXCZ" varchar(3) NOT NULL default ' ',
  "AAABEUQB" varchar(32) NOT NULL default ' ',
  "AAABMKXF" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOAT"),
  UNIQUE KEY "AAAAUXBX" ("AAABMKXF","AAABEUQB","AAAAUXCZ","AAAAUXCY","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXCD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOAT" varchar(32) NOT NULL default ' ',
  "AAAAUXCZ" varchar(3) NOT NULL default ' ',
  "AAABEUQB" varchar(32) NOT NULL default ' ',
  "AAAAUXCP" varchar(32) NOT NULL default ' ',
  "AAAAUXCQ" varchar(1) NOT NULL default '0',
  "AAABMMJT" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOAT","AAAAUXCZ","AAABEUQB","AAAAUXCP","AAAAUXCQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXCR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUXCY" varchar(32) NOT NULL default ' ',
  "AAAAUXCZ" varchar(3) NOT NULL default ' ',
  "AAABEUQB" varchar(32) NOT NULL default ' ',
  "AAABMKXF" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUXCY","AAAAUXCZ","AAABEUQB","AAABMKXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXDC" (
  "AAABJSNO" varchar(3) NOT NULL default ' ',
  "AAAAWNJY" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABJSNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXDG" (
  "AAABJSNO" varchar(3) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABJSNO","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXDM" (
  "AAAAUXIL" varchar(32) NOT NULL default ' ',
  "AAABGEXH" varchar(30) NOT NULL default ' ',
  "AAAAUXDW" varchar(32) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAAAUXEG" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAAAUXIL"),
  KEY "AAAAUXDT" ("AAAAUXEG"),
  KEY "AAAAUXDV" ("AAAAUXDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXDX" (
  "AAAAUXEC" varchar(32) NOT NULL default ' ',
  "AAAAUXED" varchar(32) NOT NULL default ' ',
  "AAAAUXEE" varchar(2) NOT NULL default ' ',
  "AAAAUXEG" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAAAUXEC","AAAAUXED","AAAAUXEE"),
  KEY "AAAAUXEF" ("AAAAUXEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXEH" (
  "AAAAUXEK" varchar(128) NOT NULL default ' ',
  "AAAAUXEJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUXEK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXEL" (
  "AAAAUXON" varchar(32) NOT NULL default ' ',
  "AAAAUXEX" varchar(32) NOT NULL default ' ',
  "AAAAUXHW" decimal(15,0) NOT NULL default '0',
  "AAAAUXYA" decimal(15,0) NOT NULL default '0',
  "AAAAUXHY" varchar(12) NOT NULL default ' ',
  "AAAAUXYB" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUXON","AAAAUXEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXEU" (
  "AAAAUXON" varchar(32) NOT NULL default ' ',
  "AAAAUXHT" varchar(32) NOT NULL default ' ',
  "AAAAUXEX" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUXON","AAAAUXHT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXFA" (
  "AAAAUXON" varchar(32) NOT NULL default ' ',
  "AAAAUXHW" decimal(15,0) NOT NULL default '0',
  "AAAAUXYA" decimal(15,0) NOT NULL default '0',
  "AAAAUXHY" varchar(12) NOT NULL default ' ',
  "AAAAUXYB" varchar(12) NOT NULL default ' ',
  "AAAAUXFG" varchar(30) NOT NULL default ' ',
  "AAAAUXFH" varchar(30) NOT NULL default ' ',
  "AAAAUXOM" varchar(32) NOT NULL default ' ',
  "AAAAUXIG" varchar(32) NOT NULL default ' ',
  "AAAAUXFK" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUXON")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXFM" (
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAAAUXON" varchar(32) NOT NULL default ' ',
  "AAAAUXIE" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFZK","AAAAUXON")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXFS" (
  "AAAAUXON" varchar(32) NOT NULL default ' ',
  "AAAAUXHL" varchar(32) NOT NULL default ' ',
  "AAAAUXHM" varchar(1) NOT NULL default ' ',
  "AAAAUXFW" varchar(32) NOT NULL default ' ',
  "AAAAUXFX" varchar(1) NOT NULL default ' ',
  "AAAAUXFY" varchar(1) NOT NULL default ' ',
  "AAAAUXFZ" varchar(1) NOT NULL default ' ',
  "AAAAUXGA" varchar(1) NOT NULL default ' ',
  "AAAAUXGB" int(11) NOT NULL default '0',
  "AAAAUXGC" int(11) NOT NULL default '0',
  "AAAAUXGD" int(11) NOT NULL default '0',
  "AAAAUXGE" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUXON","AAAAUXHL","AAAAUXHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXGI" (
  "AAAAUXXM" varchar(32) NOT NULL default ' ',
  "AAAAUXGM" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUXXM","AAAAUXGM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXGN" (
  "AAAAUXXM" varchar(32) NOT NULL default ' ',
  "AAAAUXHW" decimal(15,0) NOT NULL default '0',
  "AAAAUXYA" decimal(15,0) NOT NULL default '0',
  "AAAAUXHY" varchar(12) NOT NULL default ' ',
  "AAAAUXYB" varchar(12) NOT NULL default ' ',
  "AAAAUXON" varchar(32) NOT NULL default ' ',
  "AAAAUXOM" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUXXM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXGW" (
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAAAUXXM" varchar(32) NOT NULL default ' ',
  "AAAAUXIE" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFZK","AAAAUXXM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXHC" (
  "AAAAUXXM" varchar(32) NOT NULL default ' ',
  "AAAAUXON" varchar(32) NOT NULL default ' ',
  "AAAAUXHL" varchar(32) NOT NULL default ' ',
  "AAAAUXHM" varchar(1) NOT NULL default ' ',
  "AAAAUXHN" varchar(5) NOT NULL default '00000',
  "AAAAUXHI" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUXXM","AAAAUXON","AAAAUXHL","AAAAUXHM","AAAAUXHN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXHO" (
  "AAAAUXIG" varchar(32) NOT NULL default ' ',
  "AAAAUXHT" varchar(32) NOT NULL default ' ',
  "AAAAUXHR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUXIG","AAAAUXHT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXHU" (
  "AAAAUXIG" varchar(32) NOT NULL default ' ',
  "AAAAUXHW" decimal(15,0) NOT NULL default '0',
  "AAAAUXYA" decimal(15,0) NOT NULL default '0',
  "AAAAUXHY" varchar(12) NOT NULL default ' ',
  "AAAAUXYB" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUXIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXIB" (
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAAAUXIG" varchar(32) NOT NULL default ' ',
  "AAAAUXIE" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFZK","AAAAUXIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXIH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAUXXM" varchar(64) NOT NULL default ' ',
  "AAAAUXIL" varchar(64) NOT NULL default ' ',
  "AAABIYCS" varchar(64) NOT NULL default ' ',
  "AAABFGDI" varchar(64) NOT NULL default ' ',
  "AAAAUXIO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXIR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLUSV" varchar(3) NOT NULL default ' ',
  "AAAAUXIU" varchar(32) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABGPVW" varchar(1) NOT NULL default ' ',
  "AAABGYNY" varchar(1) NOT NULL default ' ',
  "AAABLIVG" varchar(1) NOT NULL default ' ',
  "AAABKMMG" varchar(1) NOT NULL default ' ',
  "AAAAUXJC" varchar(1) NOT NULL default ' ',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXJG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  "AAABIZKU" varchar(3) NOT NULL default ' ',
  "AAAAUXJO" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLUVS","AAABIZKU","AAAAUXJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXJP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLUSV" varchar(8) NOT NULL default ' ',
  "AAAAUXJS" varchar(3) NOT NULL default ' ',
  "AAAAUXJT" varchar(2) NOT NULL default ' ',
  "AAAAUXJU" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXJX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(8) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXKF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(3) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXKR" (
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
CREATE TABLE "AAAAUXLM" (
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
  PRIMARY KEY ("AAABMRIL","AAABHZJH"),
  KEY "AAAAUXMO" ("AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXMQ" (
  "AAAAUXMT" varchar(32) NOT NULL default ' ',
  "AAABMLZS" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAAAUXMT","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXMV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABMLPO" tinyint(3) unsigned NOT NULL default '0',
  "AAABLHAH" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXNC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" decimal(15,0) NOT NULL default '0',
  "AAABHWUK" varchar(12) NOT NULL default ' ',
  "AAAAUXNI" decimal(15,0) NOT NULL default '0',
  "AAAAUXNJ" varchar(12) NOT NULL default ' ',
  "AAAAUXNK" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXNN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTVR","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXNV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLROU" varchar(32) NOT NULL default ' ',
  "AAABMLZS" tinyint(3) unsigned NOT NULL default '0',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABAHHO" varchar(80) NOT NULL default ' ',
  "AAABFGDI" varchar(32) NOT NULL default ' ',
  "AAAAUXXM" varchar(32) NOT NULL default ' ',
  "AAABLISR" varchar(25) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABLEPD" varchar(64) NOT NULL default ' ',
  "AAABLEPE" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLROU","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXOK" (
  "AAAAUXPA" varchar(12) NOT NULL default ' ',
  "AAAAUXOM" varchar(32) NOT NULL default ' ',
  "AAAAUXON" varchar(32) NOT NULL default ' ',
  "AAAAUXOO" varchar(1) NOT NULL default ' ',
  "AAAAUXOP" varchar(1) NOT NULL default ' ',
  "AAAAUXOQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUXPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXOS" (
  "AAAAUXPA" varchar(12) NOT NULL default ' ',
  "AAAAUXPB" varchar(32) NOT NULL default ' ',
  "AAAAUXPC" varchar(32) NOT NULL default ' ',
  "AAABJUQY" int(11) NOT NULL default '0',
  "AAAAUXOX" varchar(32) NOT NULL default ' ',
  "AAAAUXOY" varchar(32) NOT NULL default ' ',
  "AAAAUXOZ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUXPA","AAAAUXPB","AAAAUXPC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAAAUXPM" varchar(32) NOT NULL default ' ',
  "AAABMLPV" varchar(32) NOT NULL default ' ',
  "AAABJFIP" varchar(1) NOT NULL default ' ',
  "AAABMMJT" longtext,
  PRIMARY KEY ("AAABMRIL","AAABLZKF","AAAAUXPM","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXPO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUYCT" varchar(32) NOT NULL default ' ',
  "AAABIFRO" varchar(12) NOT NULL default ' ',
  "AAABGHUE" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUYCT","AAABIFRO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXPW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUXVO" varchar(6) NOT NULL default ' ',
  "AAABAINM" varchar(12) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLFTX" int(11) NOT NULL default '0',
  "AAABMLNA" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAUXVO","AAABAINM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXQH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAUXVO" varchar(6) NOT NULL default ' ',
  "AAABAINM" varchar(12) NOT NULL default ' ',
  "AAABAMIH" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAUXVO","AAABAINM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXQR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUXVO" varchar(6) NOT NULL default ' ',
  "AAABIMHJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUXVO","AAABIMHJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUXVO" varchar(6) NOT NULL default ' ',
  "AAABIMHJ" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAUXRD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUXVO","AAABIMHJ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXRI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUXVO" varchar(6) NOT NULL default ' ',
  "AAAAUXVP" varchar(12) NOT NULL default '000000000000',
  "AAAAUXRM" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAAZOEQ" varchar(12) NOT NULL default ' ',
  "AAABGJNL" varchar(8) NOT NULL default '00000000',
  "AAABHCDU" varchar(6) NOT NULL default '000000',
  "AAAAUXRR" varchar(12) NOT NULL default '000000000000',
  "AAAAUXRS" varchar(2) NOT NULL default ' ',
  "AAAAUXRT" varchar(1) NOT NULL default ' ',
  "AAAAUXRU" varchar(1) NOT NULL default ' ',
  "AAAAUXRV" varchar(1) NOT NULL default ' ',
  "AAAAUXRW" varchar(2) NOT NULL default ' ',
  "AAAAUXRX" varchar(2) NOT NULL default ' ',
  "AAAAUXRY" varchar(2) NOT NULL default ' ',
  "AAAAUXRZ" varchar(2) NOT NULL default ' ',
  "AAAAUXSA" varchar(2) NOT NULL default ' ',
  "AAAAUXSB" varchar(2) NOT NULL default ' ',
  "AAAAUXSC" varchar(2) NOT NULL default ' ',
  "AAAAUXSD" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUXVO","AAAAUXVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXSH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUXVO" varchar(6) NOT NULL default ' ',
  "AAAAUXVP" varchar(12) NOT NULL default '000000000000',
  "AAAAUXSQ" varchar(3) NOT NULL default ' ',
  "AAAAUXSR" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUXVO","AAAAUXVP","AAAAUXSQ","AAAAUXSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXSS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUXVO" varchar(6) NOT NULL default ' ',
  "AAAAUXVP" varchar(12) NOT NULL default '000000000000',
  "AAABIMHJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUXVO","AAAAUXVP","AAABIMHJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXTB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUXVO" varchar(6) NOT NULL default ' ',
  "AAAAUXVP" varchar(12) NOT NULL default '000000000000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAUXTG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUXVO","AAAAUXVP","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXTL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUXVO" varchar(6) NOT NULL default ' ',
  "AAAAUXVP" varchar(12) NOT NULL default '000000000000',
  "AAABKRJY" varchar(5) NOT NULL default '00000',
  "AAAAUXTQ" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUXVO","AAAAUXVP","AAABKRJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUXVO" varchar(6) NOT NULL default ' ',
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUXVO","AAABLPJD","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXUE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUXVO" varchar(6) NOT NULL default ' ',
  "AAAAUXVP" varchar(12) NOT NULL default '000000000000',
  "AAABKRJY" varchar(5) NOT NULL default '00000',
  "AAAAUXUJ" varchar(12) NOT NULL default '000000000000',
  "AAABLVJJ" varchar(14) NOT NULL default ' ',
  "AAABIYCS" varchar(8) NOT NULL default ' ',
  "AAAAUXUM" varchar(8) NOT NULL default '00000000',
  "AAAAUXUN" int(11) NOT NULL default '0',
  "AAAAUXUO" varchar(6) NOT NULL default '000000',
  "AAAAZOEQ" varchar(12) NOT NULL default ' ',
  "AAABGJNL" varchar(8) NOT NULL default '00000000',
  "AAABHCDU" varchar(6) NOT NULL default '000000',
  "AAAAUXUS" varchar(12) NOT NULL default ' ',
  "AAAAUXUT" varchar(8) NOT NULL default '00000000',
  "AAAAUXUU" varchar(6) NOT NULL default '000000',
  "AAABHCYD" varchar(1) NOT NULL default ' ',
  "AAAAUXUW" varchar(12) NOT NULL default ' ',
  "AAABLEQN" varchar(8) NOT NULL default '00000000',
  "AAABLEQO" varchar(6) NOT NULL default '000000',
  "AAAAUXUZ" varchar(1) NOT NULL default ' ',
  "AAAAUXVA" varchar(3) NOT NULL default ' ',
  "AAAAUXVB" varchar(1) NOT NULL default ' ',
  "AAAAUXVC" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAUXVO","AAAAUXVP","AAABKRJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXVH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUXVO" varchar(6) NOT NULL default ' ',
  "AAAAUXVP" varchar(12) NOT NULL default '000000000000',
  "AAAAUXVQ" varchar(5) NOT NULL default '00000',
  "AAAAUXVR" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAAAUXVO","AAAAUXVP","AAAAUXVQ","AAAAUXVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXVS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLUNJ" varchar(32) NOT NULL default ' ',
  "AAABFGDI" varchar(32) NOT NULL default ' ',
  "AAAAUXXM" varchar(32) NOT NULL default ' ',
  "AAABMDXM" varchar(64) NOT NULL default ' ',
  "AAAAUXXQ" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLOZE","AAABLUNJ"),
  KEY "AAAAUXWD" ("AAAAUXXM"),
  KEY "AAAAUXWF" ("AAABMDXM"),
  KEY "AAAAUXWH" ("AAAAUXXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXWJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLUNJ" varchar(32) NOT NULL default ' ',
  "AAABFGDI" varchar(32) NOT NULL default ' ',
  "AAAAUXXM" varchar(32) NOT NULL default ' ',
  "AAABMDXM" varchar(64) NOT NULL default ' ',
  "AAAAUXXQ" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLOZE","AAABLUNJ"),
  KEY "AAAAUXWU" ("AAAAUXXM"),
  KEY "AAAAUXWW" ("AAABMDXM"),
  KEY "AAAAUXWY" ("AAAAUXXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXXA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLUNJ" varchar(32) NOT NULL default ' ',
  "AAABFGDI" varchar(32) NOT NULL default ' ',
  "AAAAUXXM" varchar(32) NOT NULL default ' ',
  "AAABMDXM" varchar(64) NOT NULL default ' ',
  "AAAAUXXQ" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLOZE","AAABLUNJ"),
  KEY "AAAAUXXL" ("AAAAUXXM"),
  KEY "AAAAUXXN" ("AAABMDXM"),
  KEY "AAAAUXXP" ("AAAAUXXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXXR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUXYD" varchar(32) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABFODK" smallint(6) NOT NULL default '0',
  "AAABMNJW" varchar(2) NOT NULL default ' ',
  "AAAAUYBY" varchar(32) NOT NULL default ' ',
  "AAABMMJT" varchar(255) NOT NULL default ' ',
  "AAAAUXYK" varchar(80) NOT NULL default ' ',
  "AAAAUXYA" decimal(15,0) NOT NULL default '0',
  "AAAAUXYB" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUXYD","AAABLUSV","AAABFODK","AAABMNJW"),
  KEY "AAAAUXYH" ("AAABMRIL","AAABLUSV","AAAAUXYK","AAAAUYBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXYM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUYBY" varchar(32) NOT NULL default ' ',
  "AAAAUXYP" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUYBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXYS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUYBY" varchar(32) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAAAUXYW" varchar(128) NOT NULL default ' ',
  "AAAAUXYX" varchar(128) NOT NULL default ' ',
  "AAABMOHM" varchar(32) NOT NULL default ' ',
  "AAAAUYCH" varchar(1) NOT NULL default ' ',
  "AAAAUYCI" varchar(1) NOT NULL default ' ',
  "AAAAUYCJ" varchar(1) NOT NULL default ' ',
  "AAAAUXZC" varchar(1) NOT NULL default ' ',
  "AAAAUYCG" varchar(1) NOT NULL default ' ',
  "AAAAUYCK" varchar(1) NOT NULL default ' ',
  "AAAAUXZF" varchar(1) NOT NULL default ' ',
  "AAAAUXZG" varchar(1) NOT NULL default ' ',
  "AAABJAAH" varchar(1) NOT NULL default ' ',
  "AAAAUXZI" varchar(1) NOT NULL default ' ',
  "AAAAUXZJ" varchar(1) NOT NULL default ' ',
  "AAAAUXZK" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(128) NOT NULL default ' ',
  "AAABMKAI" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUYBY","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXZQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUYBY" varchar(32) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABHIEG" int(11) NOT NULL default '0',
  "AAAAUXZV" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAUYBY","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUXZZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUYBY" varchar(32) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAAAUYAD" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAUYBY","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYAH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUYBY" varchar(32) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUYBY","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYAQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUYBY" varchar(32) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABLROC" int(11) NOT NULL default '0',
  "AAAAUYBR" varchar(128) NOT NULL default ' ',
  "AAAAUYBS" int(11) NOT NULL default '0',
  "AAAAUYBT" int(11) NOT NULL default '0',
  "AAABLEKJ" varchar(32) NOT NULL default ' ',
  "AAAAUYBW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUYBY","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYBD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUYBY" varchar(32) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAAAUYBT" int(11) NOT NULL default '0',
  "AAAAUYBW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUYBY","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYBM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUYBY" varchar(32) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABLROC" int(11) NOT NULL default '0',
  "AAAAUYBR" varchar(128) NOT NULL default ' ',
  "AAAAUYBS" int(11) NOT NULL default '0',
  "AAAAUYBT" int(11) NOT NULL default '0',
  "AAABLEKJ" varchar(32) NOT NULL default ' ',
  "AAAAUYBV" varchar(1) NOT NULL default ' ',
  "AAAAUYBW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUYBY","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYCA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUYCT" varchar(32) NOT NULL default ' ',
  "AAABMLPV" varchar(32) NOT NULL default ' ',
  "AAABMNJW" varchar(2) NOT NULL default ' ',
  "AAAAUYCW" varchar(1) NOT NULL default ' ',
  "AAAAUYCG" varchar(1) NOT NULL default ' ',
  "AAAAUYCH" varchar(1) NOT NULL default ' ',
  "AAAAUYCI" varchar(1) NOT NULL default ' ',
  "AAAAUYCJ" varchar(1) NOT NULL default ' ',
  "AAAAUYCK" varchar(1) NOT NULL default ' ',
  "AAABJAAH" varchar(1) NOT NULL default ' ',
  "AAAAUYCM" int(11) NOT NULL default '0',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAAUYCP" varchar(1) NOT NULL default ' ',
  "AAAAUYCQ" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUYCT","AAABMLPV","AAABMNJW","AAAAUYCW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYCX" (
  "AAAAUYEO" varchar(3) NOT NULL default ' ',
  "AAAAUYDE" varchar(3) NOT NULL default ' ',
  "AAAAUYDF" varchar(3) NOT NULL default ' ',
  "AAAAVCGO" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAAAUYEO"),
  UNIQUE KEY "AAAAUYDD" ("AAAAUYDE","AAAAUYDF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYDG" (
  "AAABLLIO" varchar(4) NOT NULL default ' ',
  "AAABLUNJ" varchar(40) NOT NULL default ' ',
  "AAABMMJT" varchar(3) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLIO","AAABLUNJ","AAABMMJT"),
  KEY "AAAAUYDO" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYDQ" (
  "AAABLLIO" varchar(4) NOT NULL default ' ',
  "AAABLUNJ" varchar(40) NOT NULL default ' ',
  "AAABMMJT" varchar(3) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLIO","AAABLUNJ","AAABMMJT"),
  KEY "AAAAUYDY" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYEA" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUYEO" varchar(3) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAUYEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYEG" (
  "AAAAUYEO" varchar(3) NOT NULL default ' ',
  "AAABLCCC" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUYEO","AAABLCCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYEL" (
  "AAAAUYEO" varchar(3) NOT NULL default ' ',
  "AAABENKB" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUYEO","AAABENKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYEQ" (
  "AAAAUYFA" varchar(3) NOT NULL default ' ',
  "AAAAVCGO" tinyint(3) unsigned NOT NULL default '0',
  "AAAAUYET" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUYFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYEV" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUYFA" varchar(3) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAUYFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYFB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIMHJ" varchar(22) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJFIE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIMHJ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYFJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJTNV" varchar(70) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABIMHJ" varchar(22) NOT NULL default ' ',
  "AAABDODY" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABJTNV","AAABLPJD","AAABMQDU","AAABIMHJ"),
  KEY "AAAAUYFV" ("AAABIMHJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYFX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUYGO" VARBINARY(255) NOT NULL,
  "AAABEDQH" varchar(10) NOT NULL default ' ',
  "AAAAUYGY" varchar(70) NOT NULL default ' ',
  "AAAAUYHA" varchar(32) NOT NULL default ' ',
  "AAAAUYHC" varchar(2) NOT NULL default ' ',
  "AAAAUYGZ" varchar(70) NOT NULL default ' ',
  "AAAAUYHB" varchar(32) NOT NULL default ' ',
  "AAAAUYHD" varchar(2) NOT NULL default ' ',
  "AAAAUYGH" varchar(10) NOT NULL default ' ',
  "AAAAUYGI" varchar(1) NOT NULL default ' ',
  "AAAAUYGJ" varchar(10) NOT NULL default ' ',
  "AAAAUYGK" varchar(1) NOT NULL default ' ',
  "AAABDODY" decimal(15,0) NOT NULL default '0',
  "AAAAUYGM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUYGO"(255)),
  KEY "AAAAUYGP" ("AAAAUYGY","AAAAUYHA","AAAAUYHC"),
  KEY "AAAAUYGT" ("AAAAUYGZ","AAAAUYHB","AAAAUYHD"),
  KEY "AAAAUYGX" ("AAAAUYGY","AAAAUYGZ","AAAAUYHA","AAAAUYHB","AAAAUYHC","AAAAUYHD","AAABEDQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYHF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJTNV" varchar(70) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABKWUH" varchar(10) NOT NULL default ' ',
  "AAAAUYIM" VARBINARY(255) NOT NULL,
  "AAABDODY" decimal(15,0) NOT NULL default '0',
  "AAAAUYIE" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABJTNV","AAABLPJD","AAABMQDU","AAABKWUH"),
  UNIQUE KEY "AAAAUYHT" ("AAABLUXQ","AAAAUYIM"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYHW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAYKRP" varchar(32) NOT NULL default ' ',
  "AAAAVNSK" varchar(32) NOT NULL default ' ',
  "AAABHZDJ" varchar(2) NOT NULL default ' ',
  "AAABKWUH" varchar(10) NOT NULL default ' ',
  "AAAAUYIM" VARBINARY(255) NOT NULL,
  "AAABDODY" decimal(15,0) NOT NULL default '0',
  "AAAAUYIE" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAAAYKRP","AAAAVNSK","AAABHZDJ","AAABKWUH"),
  UNIQUE KEY "AAAAUYIK" ("AAABLUXQ","AAAAUYIM"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYIN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABDOEA" varchar(22) NOT NULL default ' ',
  "AAABDOED" varchar(22) NOT NULL default ' ',
  "AAABJJLO" varchar(22) NOT NULL default ' ',
  "AAABJJLF" varchar(4) NOT NULL default ' ',
  "AAABDODY" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABDOEA","AAABDOED"),
  KEY "AAAAUYIX" ("AAABJJLO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYIZ" (
  "AAAAUYJI" varchar(8) NOT NULL default ' ',
  "AAAAUYJB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUYJI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYJD" (
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAAAUYJI" varchar(8) NOT NULL default ' ',
  "AAAAUYJG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABINAW","AAAAUYJI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYJJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJTNV" varchar(70) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABKWUH" varchar(10) NOT NULL default ' ',
  "AAABIYDW" varchar(22) NOT NULL default ' ',
  "AAABDODY" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABJTNV","AAABLPJD","AAABMQDU","AAABKWUH"),
  KEY "AAAAUYJW" ("AAABIYDW"),
  KEY "AAAAUYJY" ("AAABLUXQ","AAABKWUH","AAABLPJD","AAABJTNV"),
  KEY "AAAAUYKD" ("AAABLUXQ","AAABDODY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYKG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUYOP" varchar(32) NOT NULL default ' ',
  "AAAAUYKQ" varchar(32) NOT NULL default ' ',
  "AAABCCPB" varchar(32) NOT NULL default ' ',
  "AAABCWRL" varchar(32) NOT NULL default ' ',
  "AAABCWRM" varchar(32) NOT NULL default ' ',
  "AAAAUYQZ" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAAAUYOP","AAAAUYKQ","AAABCCPB","AAABCWRL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYKT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABLUFA" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMKFU","AAABLUFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYLA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUYLG" varchar(32) NOT NULL default ' ',
  "AAAAUYLO" varchar(15) NOT NULL default ' ',
  "AAAAUYTK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUYLG","AAAAUYLO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYLI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUYLN" varchar(32) NOT NULL default ' ',
  "AAAAUYLO" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUYLN","AAAAUYLO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYLP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIOBO" varchar(32) NOT NULL default ' ',
  "AAABHKCO" varchar(32) NOT NULL default ' ',
  "AAAAUYTB" varchar(32) NOT NULL default ' ',
  "AAAAUYTK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIOBO","AAABHKCO","AAAAUYTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYLZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIOBO" varchar(32) NOT NULL default ' ',
  "AAABHKCO" varchar(32) NOT NULL default ' ',
  "AAAAUYTB" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIOBO","AAABHKCO","AAAAUYTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYMI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUYSI" varchar(32) NOT NULL default ' ',
  "AAABCCPB" varchar(32) NOT NULL default ' ',
  "AAABMALF" varchar(2) NOT NULL default '00',
  "AAABLLLP" varchar(50) NOT NULL default ' ',
  "AAAAUYTK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUYSI","AAABCCPB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYMS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUYSI" varchar(32) NOT NULL default ' ',
  "AAABCCPB" varchar(32) NOT NULL default ' ',
  "AAABMALF" varchar(2) NOT NULL default '00',
  "AAABLLLP" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUYSI","AAABCCPB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYNB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUYOP" varchar(32) NOT NULL default ' ',
  "AAABLLLP" varchar(50) NOT NULL default ' ',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  "AAABJDPL" varchar(8) NOT NULL default '00000000',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAAAUYOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYNK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUYOP" varchar(32) NOT NULL default ' ',
  "AAABCCPB" varchar(32) NOT NULL default ' ',
  "AAABLLLP" varchar(50) NOT NULL default ' ',
  "AAABMNRG" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLUXQ","AAAAUYOP","AAABCCPB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYNT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUYOP" varchar(32) NOT NULL default ' ',
  "AAABIJSB" varchar(2) NOT NULL default '00',
  "AAABGXFD" varchar(3) NOT NULL default ' ',
  "AAABIYJF" varchar(241) NOT NULL default ' ',
  "AAABLLLP" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUYOP","AAABIJSB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYOD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUYOP" varchar(32) NOT NULL default ' ',
  "AAABHWXN" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUYOP","AAABHWXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYOK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUYOP" varchar(32) NOT NULL default ' ',
  "AAABMOHM" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUYOP","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYOR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUYOT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYOV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUYSI" varchar(32) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default '000',
  "AAABMOHM" varchar(3) NOT NULL default ' ',
  "AAABLLLP" varchar(50) NOT NULL default ' ',
  "AAABIYJF" varchar(241) NOT NULL default ' ',
  "AAAAUYTK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUYSI","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYPG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUYSI" varchar(32) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default '000',
  "AAABMOHM" varchar(3) NOT NULL default ' ',
  "AAABLLLP" varchar(50) NOT NULL default ' ',
  "AAABLLJS" varchar(241) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUYSI","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYPQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLITM" varchar(32) NOT NULL default ' ',
  "AAABLLLP" varchar(50) NOT NULL default ' ',
  "AAABGFCP" varchar(32) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABLTXI" varchar(8) NOT NULL default ' ',
  "AAAAVQZW" varchar(1) NOT NULL default ' ',
  "AAAAVQZU" varchar(12) NOT NULL default ' ',
  "AAAAVQZV" varchar(8) NOT NULL default '00000000',
  "AAAAUYQA" varchar(1) NOT NULL default ' ',
  "AAAAUYQB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLITM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYQE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUYSI" varchar(32) NOT NULL default ' ',
  "AAAAUYRC" varchar(32) NOT NULL default ' ',
  "AAABCCPB" varchar(32) NOT NULL default ' ',
  "AAABCWRL" varchar(32) NOT NULL default ' ',
  "AAABCWRM" varchar(32) NOT NULL default ' ',
  "AAAAUYQZ" varchar(1) NOT NULL default '0',
  "AAAAUYTK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUYSI","AAAAUYRC","AAABCCPB","AAABCWRL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYQS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUYSI" varchar(32) NOT NULL default ' ',
  "AAAAUYRC" varchar(32) NOT NULL default ' ',
  "AAABCCPB" varchar(32) NOT NULL default ' ',
  "AAABCWRL" varchar(32) NOT NULL default ' ',
  "AAABCWRM" varchar(32) NOT NULL default ' ',
  "AAAAUYQZ" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAAAUYSI","AAAAUYRC","AAABCCPB","AAABCWRL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYRF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLUER" varchar(3) NOT NULL default ' ',
  "AAABLITM" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLUER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYRL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLITM" varchar(32) NOT NULL default ' ',
  "AAAAUYRR" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLITM","AAAAUYRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYRS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUYSI" varchar(32) NOT NULL default ' ',
  "AAABLLLP" varchar(50) NOT NULL default ' ',
  "AAAAUYSF" varchar(8) NOT NULL default '00000000',
  "AAAAUYSG" varchar(8) NOT NULL default '00000000',
  "AAAAUYTK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUYSI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYSB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUYSI" varchar(32) NOT NULL default ' ',
  "AAABLLLP" varchar(50) NOT NULL default ' ',
  "AAAAUYSF" varchar(8) NOT NULL default '00000000',
  "AAAAUYSG" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLUXQ","AAAAUYSI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYSJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUYTD" varchar(32) NOT NULL default ' ',
  "AAABIOBO" varchar(32) NOT NULL default ' ',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAAAUYTA" varchar(241) NOT NULL default ' ',
  "AAAAUYTB" varchar(32) NOT NULL default ' ',
  "AAAAUYTK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUYTD","AAABIOBO","AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYSV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUYTD" varchar(32) NOT NULL default ' ',
  "AAABIOBO" varchar(32) NOT NULL default ' ',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAAAUYTA" varchar(241) NOT NULL default ' ',
  "AAAAUYTB" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUYTD","AAABIOBO","AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYTG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUYTS" varchar(65) NOT NULL default ' ',
  "AAABFMRK" decimal(15,0) NOT NULL default '0',
  "AAAAUYTK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUYTS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYTN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUYTS" varchar(65) NOT NULL default ' ',
  "AAABFMRK" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAAAUYTS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYTT" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUYTY" varchar(2) NOT NULL default ' ',
  "AAAAUYTW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAUYTY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYTZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDSAH" varchar(4) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABMRJZ" varchar(60) NOT NULL default ' ',
  "AAABMRJM" varchar(20) NOT NULL default ' ',
  "AAAAUYUF" varchar(1) NOT NULL default ' ',
  "AAAAUYUG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDSAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYUJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUZEU" varchar(4) NOT NULL default '0000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUZFN" varchar(30) NOT NULL default ' ',
  "AAAAUZFL" varchar(30) NOT NULL default ' ',
  "AAAAUZCU" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAUZEU","AAABMLQA","AAAAUZFN","AAAAUZFL"),
  UNIQUE KEY "AAAAUYUV" ("AAABMRIL","AAAAUZEU","AAABMLQA","AAAAUZFL","AAAAUZFN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYVB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUZEU" varchar(4) NOT NULL default '0000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUZET" varchar(7) NOT NULL default '0000000',
  "AAAAUZDI" varchar(6) NOT NULL default '000000',
  "AAAAUZDJ" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUZEU","AAABMLQA","AAAAUZET")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYVM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUZEU" varchar(4) NOT NULL default '0000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJZCI" varchar(30) NOT NULL default ' ',
  "AAABEOSM" int(11) NOT NULL default '0',
  "AAAAUZEN" varchar(1) NOT NULL default ' ',
  "AAAAUZDT" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAAAUZEU","AAABMLQA","AAABJZCI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYVY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUZEU" varchar(4) NOT NULL default '0000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLP" varchar(40) NOT NULL default ' ',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  "AAAAUZEE" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAUZEU","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYWK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUZEU" varchar(4) NOT NULL default '0000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJZCI" varchar(30) NOT NULL default ' ',
  "AAAAUZET" varchar(7) NOT NULL default '0000000',
  "AAABKKBF" int(11) NOT NULL default '0',
  "AAAAUZEN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUZEU","AAABMLQA","AAABJZCI","AAAAUZET"),
  KEY "AAAAUYWX" ("AAAAUZET","AAABMRIL","AAAAUZEU","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYXC" (
  "AAABLITM" varchar(40) NOT NULL default ' ',
  "AAAAUZBF" varchar(1) NOT NULL default ' ',
  "AAAAUZBG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLITM","AAAAUZBF","AAAAUZBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYXJ" (
  "AAAAUZBF" varchar(1) NOT NULL default ' ',
  "AAAAUZBG" varchar(30) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAAAUYXN" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAAUZBF","AAAAUZBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYXQ" (
  "AAABDBPX" int(11) NOT NULL default '0',
  "AAAAUYZA" int(11) NOT NULL default '0',
  "AAABMMVO" int(11) NOT NULL default '0',
  "AAABLMKI" varchar(8) NOT NULL default '00000000',
  "AAABLMFS" varchar(6) NOT NULL default '000000',
  "AAABMRCK" longtext,
  "AAABMLPO" int(11) NOT NULL default '0',
  "AAABJQTW" longblob,
  PRIMARY KEY ("AAABDBPX","AAAAUYZA","AAABMMVO"),
  KEY "AAAAUYYC" ("AAABMMVO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYYE" (
  "AAABDBPX" int(11) NOT NULL default '0',
  "AAAAUYZA" int(11) NOT NULL default '0',
  "AAABMMVO" int(11) NOT NULL default '0',
  "AAABLMKI" varchar(8) NOT NULL default '00000000',
  "AAABLMFS" varchar(6) NOT NULL default '000000',
  "AAABMRCK" varchar(255) NOT NULL default ' ',
  "AAABMLPO" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABDBPX","AAAAUYZA","AAABMMVO"),
  KEY "AAAAUYYP" ("AAABMMVO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYYR" (
  "AAABDBPX" int(11) NOT NULL default '0',
  "AAAAUYZA" int(11) NOT NULL default '0',
  "AAABMMVO" int(11) NOT NULL default '0',
  "AAABLMKI" varchar(8) NOT NULL default '00000000',
  "AAABLMFS" varchar(6) NOT NULL default '000000',
  "AAABMLPO" int(11) NOT NULL default '0',
  "AAABJQTW" longblob,
  PRIMARY KEY ("AAABDBPX","AAAAUYZA","AAABMMVO"),
  KEY "AAAAUYZC" ("AAABMMVO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYZE" (
  "AAABIGIZ" int(11) NOT NULL default '0',
  "AAABIGLE" varchar(8) NOT NULL default '00000000',
  "AAABIGLF" varchar(6) NOT NULL default '000000',
  "AAABIGJE" varchar(40) NOT NULL default ' ',
  "AAAAUYZJ" varchar(1) NOT NULL default ' ',
  "AAABIGLG" longtext,
  PRIMARY KEY ("AAABIGIZ"),
  KEY "AAAAUYZM" ("AAABIGLE","AAABIGLF","AAABIGJE"),
  KEY "AAAAUYZQ" ("AAABIGJE","AAABIGLE","AAABIGLF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUYZU" (
  "AAABDBPX" int(11) NOT NULL default '0',
  "AAABLEJI" int(11) NOT NULL default '0',
  "AAAAUZBO" int(11) NOT NULL default '0',
  "AAAAVQUG" varchar(95) NOT NULL default ' ',
  PRIMARY KEY ("AAABDBPX"),
  UNIQUE KEY "AAAAUZAA" ("AAABLEJI","AAAAUZBO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZAD" (
  "AAABLEJI" int(11) NOT NULL default '0',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAAAUZAL" varchar(8) NOT NULL default '00000000',
  "AAAAUZAM" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLEJI"),
  UNIQUE KEY "AAAAUZAJ" ("AAABLESS","AAAAUZAL","AAAAUZAM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZAN" (
  "AAABMMVO" int(11) NOT NULL default '0',
  "AAABHMSE" int(11) NOT NULL default '0',
  "AAABDTAQ" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMVO"),
  UNIQUE KEY "AAAAUZAS" ("AAABHMSE","AAABDTAQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZAV" (
  "AAABHMSE" int(11) NOT NULL default '0',
  "AAAAUZBF" varchar(1) NOT NULL default ' ',
  "AAAAUZBG" varchar(30) NOT NULL default ' ',
  "AAABLLLP" varchar(80) NOT NULL default ' ',
  "AAAAUZBA" tinyint(3) unsigned NOT NULL default '0',
  "AAAAUZBB" varchar(1) NOT NULL default ' ',
  "AAAAVQPQ" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABHMSE"),
  UNIQUE KEY "AAAAUZBE" ("AAAAUZBF","AAAAUZBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZBH" (
  "AAABDBPX" int(11) NOT NULL default '0',
  "AAABLEJI" int(11) NOT NULL default '0',
  "AAAAUZBO" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABDBPX"),
  UNIQUE KEY "AAAAUZBM" ("AAABLEJI","AAAAUZBO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZBP" (
  "AAAAUZCN" varchar(4) NOT NULL default '0000',
  "AAAAUZBR" varchar(30) NOT NULL default ' ',
  "AAAAUZBS" varchar(30) NOT NULL default ' ',
  "AAAAUZBT" varchar(30) NOT NULL default ' ',
  "AAAAUZBU" varchar(30) NOT NULL default ' ',
  "AAAAUZBV" varchar(1) NOT NULL default ' ',
  "AAAAUZBW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUZCN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZBY" (
  "AAAAUZEU" varchar(4) NOT NULL default '0000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUZCB" varchar(3) NOT NULL default '000',
  "AAAAUZCC" varchar(1) NOT NULL default '0',
  "AAAAUZCD" varchar(100) NOT NULL default ' ',
  "AAABIGQP" varchar(1) NOT NULL default '0',
  "AAAAUZCF" varchar(1) NOT NULL default '0',
  "AAAAUZCG" varchar(2) NOT NULL default '00',
  "AAAAUZCH" varchar(1) NOT NULL default '0',
  "AAAAUZCI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUZEU","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZCL" (
  "AAAAUZEU" varchar(4) NOT NULL default '0000',
  "AAAAUZCN" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAAAUZEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZCP" (
  "AAAAUZEU" varchar(4) NOT NULL default '0000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUZFN" varchar(30) NOT NULL default ' ',
  "AAAAUZFL" varchar(30) NOT NULL default ' ',
  "AAAAUZCU" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAAUZEU","AAABMLQA","AAAAUZFN","AAAAUZFL"),
  UNIQUE KEY "AAAAUZCZ" ("AAAAUZEU","AAABMLQA","AAAAUZFL","AAAAUZFN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZDE" (
  "AAAAUZEU" varchar(4) NOT NULL default '0000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUZET" varchar(7) NOT NULL default '0000000',
  "AAAAUZDI" varchar(6) NOT NULL default '000000',
  "AAAAUZDJ" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUZEU","AAABMLQA","AAAAUZET")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZDN" (
  "AAAAUZEU" varchar(4) NOT NULL default '0000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJZCI" varchar(30) NOT NULL default ' ',
  "AAABEOSM" int(11) NOT NULL default '0',
  "AAAAUZEN" varchar(1) NOT NULL default ' ',
  "AAAAUZDT" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAAAUZEU","AAABMLQA","AAABJZCI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZDX" (
  "AAAAUZEU" varchar(4) NOT NULL default '0000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLP" varchar(40) NOT NULL default ' ',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  "AAAAUZEE" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAAUZEU","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZEH" (
  "AAAAUZEU" varchar(4) NOT NULL default '0000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJZCI" varchar(30) NOT NULL default ' ',
  "AAAAUZET" varchar(7) NOT NULL default '0000000',
  "AAABKKBF" int(11) NOT NULL default '0',
  "AAAAUZEN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUZEU","AAABMLQA","AAABJZCI","AAAAUZET"),
  KEY "AAAAUZES" ("AAAAUZET","AAAAUZEU","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZEW" (
  "AAAAUZFS" varchar(8) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUZFN" varchar(30) NOT NULL default ' ',
  "AAAAUZFM" varchar(1) NOT NULL default ' ',
  "AAAAUZFL" varchar(64) NOT NULL default ' ',
  "AAAAUZFC" decimal(10,9) NOT NULL default '0.000000000',
  PRIMARY KEY ("AAAAUZFS","AAABMLQA","AAAAUZFN","AAAAUZFM","AAAAUZFL"),
  UNIQUE KEY "AAAAUZFI" ("AAAAUZFS","AAABMLQA","AAAAUZFL","AAAAUZFM","AAAAUZFN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZFO" (
  "AAAAUZFS" varchar(8) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJZCI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUZFS","AAABMLQA","AAABJZCI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZFV" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAAAUZGC" varchar(10) NOT NULL default ' ',
  "AAAAUZGD" varchar(128) NOT NULL default ' ',
  "AAABJSLF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV"),
  UNIQUE KEY "AAAAUZGB" ("AAAAUZGC","AAAAUZGD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZGE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBIWJ" varchar(30) NOT NULL default ' ',
  "AAAAUZIU" varchar(60) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAAUZGJ" varchar(1) NOT NULL default ' ',
  "AAAAUZGK" varchar(128) NOT NULL default ' ',
  "AAAAUZGL" varchar(12) NOT NULL default ' ',
  "AAAAUZGM" varchar(8) NOT NULL default '00000000',
  "AAAAUZGN" varchar(12) NOT NULL default ' ',
  "AAAAUZGO" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABBIWJ","AAAAUZIU","AAABLHGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZGT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBIWJ" varchar(30) NOT NULL default ' ',
  "AAAAUZIU" varchar(60) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAAUZHX" int(11) NOT NULL default '0',
  "AAABLSAX" varchar(128) NOT NULL default ' ',
  "AAABGZSJ" varchar(128) NOT NULL default ' ',
  "AAABMMJT" longtext,
  PRIMARY KEY ("AAABMRIL","AAABBIWJ","AAAAUZIU","AAABLHGN","AAAAUZHX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZHH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBIWJ" varchar(30) NOT NULL default ' ',
  "AAAAUZIU" varchar(60) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAAUZHX" int(11) NOT NULL default '0',
  "AAAAUZHN" varchar(128) NOT NULL default ' ',
  "AAAAUZHO" varchar(128) NOT NULL default ' ',
  "AAAAUZHP" varchar(30) NOT NULL default ' ',
  "AAABLSAX" varchar(128) NOT NULL default ' ',
  "AAABGZSJ" varchar(128) NOT NULL default ' ',
  "AAABMMJT" longtext,
  PRIMARY KEY ("AAABMRIL","AAABBIWJ","AAAAUZIU","AAABLHGN","AAAAUZHX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZHY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBIWJ" varchar(30) NOT NULL default ' ',
  "AAAAUZIU" varchar(60) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAAVQCF" varchar(120) NOT NULL default ' ',
  "AAAAVQCG" varchar(120) NOT NULL default ' ',
  "AAABGKWP" varchar(60) NOT NULL default ' ',
  "AAABLGEF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBIWJ","AAAAUZIU","AAABLHGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZIL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBIWJ" varchar(30) NOT NULL default ' ',
  "AAAAUZIU" varchar(60) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBIWJ","AAAAUZIU","AAABLHGN","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZIX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUZMA" varchar(32) NOT NULL default ' ',
  "AAAAUZKD" varchar(30) NOT NULL default ' ',
  "AAAAUZJB" varchar(128) NOT NULL default ' ',
  "AAAAUZMB" varchar(16) NOT NULL default ' ',
  "AAABLEBN" varchar(12) NOT NULL default ' ',
  "AAABKNCM" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLUXQ","AAAAUZMA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZJH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUZMA" varchar(32) NOT NULL default ' ',
  "AAABEDRA" varchar(1) NOT NULL default ' ',
  "AAABJFIE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUZMA","AAABEDRA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZJP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUZJX" int(11) NOT NULL default '0',
  "AAABGZSJ" varchar(32) NOT NULL default ' ',
  "AAABLUVJ" varchar(128) NOT NULL default ' ',
  "AAAAUZJU" varchar(32) NOT NULL default ' ',
  "AAAAUZJV" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUZJX","AAABGZSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZJZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUZMA" varchar(32) NOT NULL default ' ',
  "AAAAUZKL" varchar(32) NOT NULL default ' ',
  "AAAAUZKD" varchar(30) NOT NULL default ' ',
  "AAABLTYY" varchar(32) NOT NULL default ' ',
  "AAAAUZKF" varchar(20) NOT NULL default ' ',
  "AAABKWET" decimal(15,0) NOT NULL default '0',
  "AAAAUZKH" varchar(12) NOT NULL default ' ',
  "AAAAUZKI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUZMA","AAAAUZKL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZKM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUZMA" varchar(32) NOT NULL default ' ',
  "AAAAUZLB" varchar(10) NOT NULL default ' ',
  "AAAAUZMG" varchar(40) NOT NULL default ' ',
  "AAAAUZLD" int(11) NOT NULL default '0',
  "AAAAUZKS" longtext,
  "AAAAUZMB" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUZMA","AAAAUZLB","AAAAUZMG","AAAAUZLD"),
  KEY "AAAAUZKZ" ("AAABLUXQ","AAAAUZLB","AAAAUZMG","AAAAUZLD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZLE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUZMA" varchar(32) NOT NULL default ' ',
  "AAAAUZMB" varchar(16) NOT NULL default ' ',
  "AAABJUQY" varchar(10) NOT NULL default ' ',
  "AAABLSAX" varchar(128) NOT NULL default ' ',
  "AAABGZSJ" varchar(128) NOT NULL default ' ',
  "AAABMMJT" longtext,
  "AAAAUZLM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUZMA","AAAAUZMB","AAABJUQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZLR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUZMA" varchar(32) NOT NULL default ' ',
  "AAAAUZMB" varchar(16) NOT NULL default ' ',
  "AAABLJJO" varchar(30) NOT NULL default ' ',
  "AAABJSPH" varchar(128) NOT NULL default ' ',
  "AAABGDAB" varchar(128) NOT NULL default ' ',
  "AAAAUZLY" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUZMA","AAAAUZMB","AAABLJJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZMD" (
  "AAAAUZMG" varchar(32) NOT NULL default ' ',
  "AAABLRQQ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUZMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZMH" (
  "AAABIWGU" varchar(6) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWGU"),
  KEY "AAAAUZMO" ("AAABLETY","AAABIWGU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZMR" (
  "AAABIWIE" varchar(6) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWIE"),
  KEY "AAAAUZMY" ("AAABLETY","AAABIWIE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZNB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUZOH" varchar(10) NOT NULL default ' ',
  "AAAAUZOI" varchar(4) NOT NULL default '0000',
  "AAAAUZOJ" varchar(2) NOT NULL default '00',
  "AAABBQTF" decimal(13,2) NOT NULL default '0.00',
  "AAABFZVY" decimal(13,2) NOT NULL default '0.00',
  "AAAAUZNI" varchar(5) NOT NULL default '00000',
  "AAAAUZNJ" decimal(13,2) NOT NULL default '0.00',
  "AAAAUZNK" double NOT NULL default '0',
  "AAAAUZNL" double NOT NULL default '0',
  "AAAAUZNM" double NOT NULL default '0',
  "AAAAUZNN" double NOT NULL default '0',
  "AAAAUZNO" double NOT NULL default '0',
  "AAAAUZNP" double NOT NULL default '0',
  "AAAAUZNQ" varchar(5) NOT NULL default '00000',
  "AAAAUZNR" varchar(5) NOT NULL default '00000',
  "AAAAUZNS" varchar(5) NOT NULL default '00000',
  "AAAAUZNT" varchar(5) NOT NULL default '00000',
  "AAAAUZNU" varchar(5) NOT NULL default '00000',
  "AAAAUZNV" decimal(13,2) NOT NULL default '0.00',
  "AAAAUZNW" double NOT NULL default '0',
  "AAAAUZNX" double NOT NULL default '0',
  "AAAAUZNY" varchar(5) NOT NULL default '00000',
  "AAAAUZNZ" varchar(5) NOT NULL default '00000',
  "AAAAUZOA" decimal(13,2) NOT NULL default '0.00',
  "AAAAUZOB" decimal(13,2) NOT NULL default '0.00',
  "AAAAUZOC" double NOT NULL default '0',
  "AAAAUZOD" double NOT NULL default '0',
  "AAAAUZOE" decimal(4,2) NOT NULL default '0.00',
  "AAAAUZOF" decimal(5,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAUZOH","AAAAUZOI","AAAAUZOJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZOK" (
  "AAABIWIW" varchar(14) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWIW"),
  KEY "AAAAUZOR" ("AAABLETY","AAABIWIW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABLECD" varchar(30) NOT NULL default ' ',
  "AAABGBTL" varchar(64) NOT NULL default ' ',
  "AAABLETY" varchar(96) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAAAUZPC" varchar(8) NOT NULL default '00000000',
  "AAAAUZPD" varchar(6) NOT NULL default '000000',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABMLPV","AAABLECD","AAABGBTL","AAABLETY","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZPN" (
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEHK"),
  UNIQUE KEY "AAAAUZPU" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZPW" (
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPQY"),
  KEY "AAAAUZQD" ("AAABLETY","AAABLPQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZQG" (
  "AAABMKFG" varchar(10) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFG"),
  UNIQUE KEY "AAAAUZQN" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZQP" (
  "AAABIWJO" varchar(8) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWJO"),
  KEY "AAAAUZQW" ("AAABLETY","AAABIWJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZQZ" (
  "AAABHUYZ" varchar(2) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHUYZ"),
  UNIQUE KEY "AAAAUZRG" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZRI" (
  "AAABIYVH" varchar(2) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIYVH"),
  UNIQUE KEY "AAAAUZRP" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZRR" (
  "AAABHJRW" varchar(6) NOT NULL default ' ',
  "AAAAUZRT" varchar(1) NOT NULL default ' ',
  "AAAAUZRU" varchar(1) NOT NULL default ' ',
  "AAAAUZRV" varchar(1) NOT NULL default ' ',
  "AAAAUZRW" varchar(1) NOT NULL default ' ',
  "AAAAUZRX" varchar(1) NOT NULL default ' ',
  "AAAAUZRY" varchar(1) NOT NULL default ' ',
  "AAAAUZRZ" varchar(1) NOT NULL default ' ',
  "AAAAUZSA" varchar(1) NOT NULL default ' ',
  "AAAAUZSB" varchar(1) NOT NULL default ' ',
  "AAAAUZSC" varchar(1) NOT NULL default ' ',
  "AAAAUZSD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHJRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZSF" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHJRW" varchar(6) NOT NULL default ' ',
  "AAABLLLP" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABHJRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZSL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHJRW" varchar(6) NOT NULL default ' ',
  "AAAAUZSO" varchar(10) NOT NULL default ' ',
  "AAAAUZSP" varchar(20) NOT NULL default ' ',
  "AAAAUZSQ" varchar(132) NOT NULL default ' ',
  "AAAAUZSR" varchar(132) NOT NULL default ' ',
  "AAAAUZSS" varchar(255) NOT NULL default ' ',
  "AAABLUVS" varchar(132) NOT NULL default ' ',
  "AAAAUZSU" varchar(132) NOT NULL default ' ',
  "AAAAUZSV" varchar(20) NOT NULL default ' ',
  "AAAAUZSW" varchar(20) NOT NULL default ' ',
  "AAAAUZSX" varchar(1) NOT NULL default ' ',
  "AAAAUZSY" varchar(1) NOT NULL default ' ',
  "AAAAUZSZ" varchar(1) NOT NULL default ' ',
  "AAAAUZTA" varchar(1) NOT NULL default ' ',
  "AAAAUZTB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHJRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZTE" (
  "AAABIWLB" varchar(2) NOT NULL default ' ',
  "AAABIWJZ" varchar(7) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWLB","AAABIWJZ"),
  KEY "AAAAUZTN" ("AAABLETY","AAABIWLB","AAABIWJZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZTR" (
  "AAABIWLB" varchar(2) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWLB"),
  KEY "AAAAUZTY" ("AAABLETY","AAABIWLB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZUB" (
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABLGCM" varchar(30) NOT NULL default ' ',
  "AAAAUZUM" varchar(30) NOT NULL default ' ',
  "AAABKMQV" varchar(132) NOT NULL default ' ',
  "AAAAUZUG" varchar(30) NOT NULL default ' ',
  "AAAAUZUH" varchar(30) NOT NULL default ' ',
  "AAAAUZUI" varchar(12) NOT NULL default ' ',
  "AAABIASL" longtext,
  PRIMARY KEY ("AAABLPJD","AAABLGCM","AAAAUZUM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZUN" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLEUZ" varchar(12) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLEUZ","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZUW" (
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABMLPV" varchar(70) NOT NULL default ' ',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAABMCXR" varchar(4) NOT NULL default ' ',
  "AAABLTXI" int(11) NOT NULL default '0',
  "AAABHIYE" varchar(8) NOT NULL default '00000000',
  "AAABHIYF" varchar(6) NOT NULL default '000000',
  "AAABDOVM" longblob,
  PRIMARY KEY ("AAABMKFH","AAABMLPV","AAABLUSV","AAABMCXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZVJ" (
  "AAAAVFFM" varchar(30) NOT NULL default ' ',
  "AAABLTXI" int(11) NOT NULL default '0',
  "AAABHIYE" varchar(8) NOT NULL default '00000000',
  "AAABHIYF" varchar(6) NOT NULL default '000000',
  "AAAAUZVO" longblob,
  PRIMARY KEY ("AAAAVFFM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZVQ" (
  "AAABMLPV" varchar(6) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default '000',
  "AAAAVEFV" VARBINARY(255),
  PRIMARY KEY ("AAABMLPV","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZVW" (
  "AAABMLPV" varchar(6) NOT NULL default ' ',
  "AAABLUVJ" varchar(32) NOT NULL default ' ',
  "AAAAUZWP" varchar(2) NOT NULL default '00',
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABGGED" varchar(1) NOT NULL default ' ',
  "AAABIMEG" varchar(12) NOT NULL default ' ',
  "AAABLMKH" varchar(8) NOT NULL default '00000000',
  "AAABLIRO" varchar(6) NOT NULL default '000000',
  "AAABIMEJ" varchar(12) NOT NULL default ' ',
  "AAABIMEK" varchar(8) NOT NULL default '00000000',
  "AAABIMEL" varchar(6) NOT NULL default '000000',
  "AAABLEJW" varchar(100) NOT NULL default ' ',
  "AAABLUSV" varchar(255) NOT NULL default ' ',
  "AAABJSDH" varchar(1) NOT NULL default ' ',
  "AAAAUZWL" varchar(8) NOT NULL default ' ',
  "AAAAUZWM" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMLPV","AAABLUVJ","AAAAUZWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZWQ" (
  "AAABJZWV" varchar(4) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJZWV"),
  UNIQUE KEY "AAAAUZWX" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZWZ" (
  "AAABLOBS" varchar(18) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOBS"),
  UNIQUE KEY "AAAAUZXG" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZXI" (
  "AAABLROO" varchar(9) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLROO"),
  UNIQUE KEY "AAAAUZXP" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZXR" (
  "AAAAUZXX" varchar(4) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUZXX"),
  UNIQUE KEY "AAAAUZXY" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZYA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(32) NOT NULL default ' ',
  "AAAAUZYK" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA","AAAAUZYK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZYL" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLUSV" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZYQ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLUSV" varchar(20) NOT NULL default ' ',
  "AAABLPWW" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMREL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZYY" (
  "AAABLUSV" varchar(20) NOT NULL default ' ',
  "AAABMMJT" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZZC" (
  "AAABLUSV" varchar(20) NOT NULL default ' ',
  "AAABLQHQ" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZZG" (
  "AAABLUSV" varchar(20) NOT NULL default ' ',
  "AAABLQHQ" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZZK" (
  "AAAAUZZN" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUZZN","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZZP" (
  "AAAAUZZV" varchar(1) NOT NULL default ' ',
  "AAABMBXY" varchar(5) NOT NULL default '00000',
  "AAABMJXI" varchar(1) NOT NULL default ' ',
  "AAABLECQ" varchar(20) NOT NULL default ' ',
  "AAABLHAH" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUZZV","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUZZX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAVAAA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAAD" (
  "AAABIQPI" varchar(3) NOT NULL default '000',
  "AAABILHK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABIQPI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAAH" (
  "AAAAVADL" varchar(20) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAVADL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAAL" (
  "AAABMCMG" varchar(1) NOT NULL default '0',
  "AAAAVAAQ" varchar(20) NOT NULL default ' ',
  "AAAAVAAR" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMCMG","AAAAVAAQ","AAAAVAAR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAAS" (
  "AAABIQPI" varchar(3) NOT NULL default '000',
  "AAABMRFO" tinyint(3) unsigned NOT NULL default '0',
  "AAABMJUY" varchar(8) NOT NULL default '00000000',
  "AAABMJUZ" varchar(8) NOT NULL default '00000000',
  "AAABMJVA" varchar(8) NOT NULL default '00000000',
  "AAABMJVB" varchar(8) NOT NULL default '00000000',
  "AAABMJVC" varchar(8) NOT NULL default '00000000',
  "AAAAVABA" varchar(8) NOT NULL default '00000000',
  "AAABEIAQ" varchar(6) NOT NULL default '000000',
  "AAAAVABC" varchar(6) NOT NULL default '000000',
  "AAAAVABD" varchar(6) NOT NULL default '000000',
  "AAAAVABE" varchar(6) NOT NULL default '000000',
  "AAAAVABF" varchar(6) NOT NULL default '000000',
  "AAAAVABG" varchar(6) NOT NULL default '000000',
  "AAABKOUG" varchar(12) NOT NULL default ' ',
  "AAABKOUH" varchar(12) NOT NULL default ' ',
  "AAABKOUI" varchar(12) NOT NULL default ' ',
  "AAABKOUJ" varchar(12) NOT NULL default ' ',
  "AAABKOUK" varchar(12) NOT NULL default ' ',
  "AAABKOUL" varchar(12) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAVABO" varchar(1) NOT NULL default ' ',
  "AAAAVABP" varchar(20) NOT NULL default ' ',
  "AAAAVABQ" varchar(16) NOT NULL default ' ',
  "AAABILHK" varchar(4) NOT NULL default ' ',
  "AAAAVRVK" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABIQPI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVABU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAVABY" varchar(1) NOT NULL default ' ',
  "AAAAVABZ" varchar(3) NOT NULL default '000',
  "AAABMEGE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLENH","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVACE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAVACH" varchar(3) NOT NULL default '000',
  "AAAAVACI" varchar(1) NOT NULL default '0',
  "AAAAVACJ" varchar(1) NOT NULL default '0',
  "AAAAVACK" varchar(1) NOT NULL default '0',
  "AAAAVACL" varchar(1) NOT NULL default '0',
  "AAAAVACM" varchar(1) NOT NULL default '0',
  "AAAAVACN" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVACQ" (
  "AAABLUSV" varchar(20) NOT NULL default ' ',
  "AAABLQHQ" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVACU" (
  "AAABLEKS" varchar(12) NOT NULL default ' ',
  "AAAAVADC" varchar(3) NOT NULL default ' ',
  "AAABIKSF" varchar(1) NOT NULL default ' ',
  "AAABIKSG" varchar(1) NOT NULL default ' ',
  "AAABIKSH" varchar(1) NOT NULL default ' ',
  "AAABIKSI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEKS","AAAAVADC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVADD" (
  "AAAAVADL" varchar(20) NOT NULL default ' ',
  "AAABMMJT" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAAAVADL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVADH" (
  "AAAAVADL" varchar(20) NOT NULL default ' ',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAAAVADL","AAABINAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVADN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVBOG" varchar(32) NOT NULL default ' ',
  "AAABCLQM" varchar(32) NOT NULL default ' ',
  "AAABHMPC" decimal(21,7) NOT NULL default '0.0000000',
  "AAABAKKR" decimal(21,7) NOT NULL default '0.0000000',
  "AAABDCRX" decimal(21,7) NOT NULL default '0.0000000',
  "AAAAVADU" decimal(15,0) NOT NULL default '0',
  "AAAAVADV" int(11) NOT NULL default '0',
  "AAAAVAQL" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAAAVBOG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVADZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVBOG" varchar(32) NOT NULL default ' ',
  "AAABKRHS" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVAUW" varchar(40) NOT NULL default ' ',
  "AAABMLPV" varchar(32) NOT NULL default ' ',
  "AAABLJJN" varchar(8) NOT NULL default ' ',
  "AAABIZMY" varchar(1) NOT NULL default ' ',
  "AAAAVAEH" int(11) NOT NULL default '0',
  "AAAAVAEI" varchar(1) NOT NULL default ' ',
  "AAABCLQM" varchar(32) NOT NULL default ' ',
  "AAAAVAEK" varchar(32) NOT NULL default ' ',
  "AAABHMPC" decimal(15,0) NOT NULL default '0',
  "AAABAKKR" decimal(21,7) NOT NULL default '0.0000000',
  "AAABDCRX" decimal(21,7) NOT NULL default '0.0000000',
  "AAAAVAEO" varchar(40) NOT NULL default ' ',
  "AAAAVAEP" varchar(132) NOT NULL default ' ',
  "AAAAVAEQ" decimal(15,0) NOT NULL default '0',
  "AAAAVAER" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAAAVBOG","AAABKRHS","AAAAVAUW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAEW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVBOG" varchar(32) NOT NULL default ' ',
  "AAABCLQM" varchar(32) NOT NULL default ' ',
  "AAABHMPC" decimal(21,7) NOT NULL default '0.0000000',
  "AAABAKKR" decimal(21,7) NOT NULL default '0.0000000',
  "AAABDCRX" decimal(21,7) NOT NULL default '0.0000000',
  "AAABMAJL" smallint(6) NOT NULL default '0',
  "AAAAVAFE" varchar(32) NOT NULL default ' ',
  "AAAAVAFF" decimal(21,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABLUXQ","AAAAVBOG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAFI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVAZF" varchar(32) NOT NULL default ' ',
  "AAAAVAGG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAVAZF","AAAAVAGG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAFP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVAGG" varchar(40) NOT NULL default ' ',
  "AAABCLQM" varchar(32) NOT NULL default ' ',
  "AAABHMPC" decimal(21,7) NOT NULL default '0.0000000',
  "AAAAVAGD" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAAAVAGG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAFX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVAZF" varchar(32) NOT NULL default ' ',
  "AAAAVAGG" varchar(40) NOT NULL default ' ',
  "AAABCLQM" varchar(32) NOT NULL default ' ',
  "AAABHMPC" decimal(21,7) NOT NULL default '0.0000000',
  "AAAAVAGD" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAAAVAZF","AAAAVAGG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAGH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAWYIL" varchar(32) NOT NULL default ' ',
  "AAABJEMU" varchar(30) NOT NULL default ' ',
  "AAAAVBIK" tinyint(3) unsigned NOT NULL default '0',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(70) NOT NULL default ' ',
  "AAABMNNA" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAWYIL","AAABJEMU","AAAAVBIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAGU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVBJW" varchar(22) NOT NULL default ' ',
  "AAABIJQE" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAVBJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAHA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVBJW" varchar(22) NOT NULL default ' ',
  "AAAAWYIL" varchar(32) NOT NULL default ' ',
  "AAABJEMU" varchar(30) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAVBJW","AAAAWYIL","AAABJEMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAHK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAVBJW" varchar(22) NOT NULL default ' ',
  "AAAAVAHO" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAAAVBJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAHS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVBJW" varchar(22) NOT NULL default ' ',
  "AAABJEMU" varchar(30) NOT NULL default ' ',
  "AAAAVAHW" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVAHX" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVAHY" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAAAVBJW","AAABJEMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAIC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAVBJW" varchar(22) NOT NULL default ' ',
  "AAABJEMU" varchar(30) NOT NULL default ' ',
  "AAABGZYP" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAAAVBJW","AAABJEMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAIM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVAIR" varchar(30) NOT NULL default ' ',
  "AAABCTYC" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAAAVAIR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAIS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVAJL" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAVAJL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAIX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVBOG" varchar(32) NOT NULL default ' ',
  "AAAAVAJL" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAVBOG","AAAAVAJL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAJE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAVAJL" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAAAVAJL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAJM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIJQE" varchar(8) NOT NULL default ' ',
  "AAAAVAJP" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVAJQ" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVAJR" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVAJS" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVAJT" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVAJU" tinyint(3) unsigned NOT NULL default '0',
  "AAABEOWS" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABIJQE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAJY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVPJH" varchar(12) NOT NULL default ' ',
  "AAAAVPJI" varchar(8) NOT NULL default ' ',
  "AAABMLPV" varchar(32) NOT NULL default ' ',
  "AAABLJJN" varchar(8) NOT NULL default ' ',
  "AAABHFAP" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVBNG" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVBOD" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVBNK" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAAAVPJH","AAAAVPJI","AAABMLPV","AAABLJJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAKN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJDNG" VARBINARY(255) NOT NULL,
  "AAAAVBOG" varchar(32) NOT NULL default ' ',
  "AAABHCDU" decimal(21,7) NOT NULL default '0.0000000',
  "AAAAVALN" VARBINARY(255),
  "AAABKWQO" VARBINARY(255),
  "AAAAVALL" decimal(21,7) NOT NULL default '0.0000000',
  "AAAAVAKV" decimal(21,7) NOT NULL default '0.0000000',
  "AAAAVALP" decimal(21,7) NOT NULL default '0.0000000',
  "AAABLOCB" decimal(15,0) NOT NULL default '0',
  "AAAAVAKY" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVALM" decimal(21,7) NOT NULL default '0.0000000',
  "AAAAVBDI" varchar(10) NOT NULL default ' ',
  "AAAAVBCF" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLUXQ","AAABJDNG"(255)),
  KEY "AAAAVALE" ("AAABLUXQ","AAABHCDU","AAAAVBOG"),
  KEY "AAAAVALI" ("AAABLUXQ","AAAAVBOG","AAAAVALL","AAAAVALM","AAAAVALN"(255),"AAABKWQO"(255),"AAAAVALP","AAABJDNG"(255)),
  KEY "AAAAVALR" ("AAABLUXQ","AAABLOCB","AAAAVBOG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVALV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJDNG" VARBINARY(255) NOT NULL,
  "AAAAVAPU" VARBINARY(255) NOT NULL,
  "AAAAVANC" VARBINARY(255) NOT NULL,
  "AAAAVBNO" varchar(32) NOT NULL default ' ',
  "AAAAVANR" varchar(32) NOT NULL default ' ',
  "AAAAVANS" varchar(32) NOT NULL default ' ',
  "AAAAVAMD" VARBINARY(255),
  "AAABHCDU" decimal(21,7) NOT NULL default '0.0000000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAVAOF" varchar(8) NOT NULL default ' ',
  "AAAAVAOG" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABECXR" varchar(4) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABILXJ" varchar(20) NOT NULL default ' ',
  "AAAAVAPA" VARBINARY(255),
  "AAABHCRQ" smallint(6) NOT NULL default '0',
  "AAAAVAPB" VARBINARY(255),
  "AAABMKFU" decimal(21,7) NOT NULL default '0.0000000',
  "AAABKWQO" VARBINARY(255),
  "AAABJLNZ" VARBINARY(255),
  "AAABDCAQ" VARBINARY(255),
  "AAAAVAOT" VARBINARY(255),
  PRIMARY KEY ("AAABLUXQ","AAABJDNG"(255),"AAAAVAPU"(255),"AAAAVANC"(255)),
  UNIQUE KEY "AAAAVAMZ" ("AAABLUXQ","AAABMKFU","AAAAVANC"(255),"AAABJDNG"(255),"AAAAVAPB"(255)),
  KEY "AAAAVANF" ("AAABLUXQ","AAABJDNG"(255),"AAAAVBNO","AAABKWQO"(255),"AAAAVAPU"(255)),
  KEY "AAAAVANL" ("AAABLUXQ","AAAAVAPU"(255)),
  KEY "AAAAVANO" ("AAABLUXQ","AAABJDNG"(255),"AAAAVANR","AAAAVANS","AAABKWQO"(255)),
  KEY "AAAAVANU" ("AAABLUXQ","AAAAVBNO","AAABJDNG"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVANY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVAPU" VARBINARY(255) NOT NULL,
  "AAAAVAPF" decimal(21,7) NOT NULL default '0.0000000',
  "AAAAVBJW" varchar(22) NOT NULL default ' ',
  "AAAAWYIL" varchar(32) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAVAOF" varchar(8) NOT NULL default ' ',
  "AAAAVAOG" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABECXR" varchar(4) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABILXJ" varchar(20) NOT NULL default ' ',
  "AAAAVAPA" VARBINARY(255),
  "AAABHCRQ" smallint(6) NOT NULL default '0',
  "AAAAVAPB" VARBINARY(255),
  "AAABMKFU" decimal(21,7) NOT NULL default '0.0000000',
  "AAABKWQO" VARBINARY(255),
  "AAABJLNZ" VARBINARY(255),
  "AAABDCAQ" VARBINARY(255),
  "AAAAVAOT" VARBINARY(255),
  "AAABHCYD" VARBINARY(255),
  PRIMARY KEY ("AAABLUXQ","AAAAVAPU"(255)),
  UNIQUE KEY "AAAAVAOX" ("AAABLUXQ","AAABMKFU","AAAAVAPA"(255),"AAAAVAPB"(255)),
  KEY "AAAAVAPC" ("AAABLUXQ","AAAAWYIL","AAAAVAPF","AAABMKFU","AAAAVAPU"(255)),
  KEY "AAAAVAPI" ("AAABLUXQ","AAABHCYD"(255),"AAAAWYIL","AAABMKFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAPN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVAPU" VARBINARY(255) NOT NULL,
  "AAABJEMU" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(70) NOT NULL default ' ',
  "AAAAVAPY" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAAAVAPU"(255),"AAABJEMU"),
  KEY "AAAAVAPW" ("AAABLUXQ","AAAAVAPY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAPZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCLQM" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAABJHCT" varchar(40) NOT NULL default ' ',
  "AAABHMPC" decimal(21,7) NOT NULL default '0.0000000',
  "AAABAKKR" decimal(21,7) NOT NULL default '0.0000000',
  "AAABDCRX" decimal(21,7) NOT NULL default '0.0000000',
  "AAABCRXB" varchar(2) NOT NULL default '00',
  "AAABCRXC" varchar(2) NOT NULL default '00',
  "AAABCRXD" varchar(3) NOT NULL default '000',
  "AAABMAJL" varchar(2) NOT NULL default '00',
  "AAAAVAQL" int(11) NOT NULL default '0',
  "AAAAVAQM" int(11) NOT NULL default '0',
  "AAABMLFD" varchar(14) NOT NULL default ' ',
  "AAABHMPB" decimal(21,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABLUXQ","AAABCLQM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAQR" (
  "AAAAVASH" varchar(32) NOT NULL default ' ',
  "AAAAVASI" varchar(8) NOT NULL default ' ',
  "AAAAVASJ" varchar(40) NOT NULL default ' ',
  "AAAAVASK" varchar(10) NOT NULL default ' ',
  "AAAAVASL" varchar(10) NOT NULL default ' ',
  "AAAAVASM" varchar(8) NOT NULL default '00000000',
  "AAAAVASN" varchar(6) NOT NULL default '000000',
  "AAAAVASO" varchar(2) NOT NULL default ' ',
  "AAAAVARA" int(11) NOT NULL default '0',
  "AAAAVARB" int(11) NOT NULL default '0',
  "AAAAVARC" varchar(8) NOT NULL default ' ',
  "AAAAVASQ" varchar(40) NOT NULL default ' ',
  "AAAAVARE" varchar(3) NOT NULL default ' ',
  "AAAAVARF" varchar(10) NOT NULL default ' ',
  "AAAAVARG" varchar(3) NOT NULL default ' ',
  "AAAAVARH" varchar(10) NOT NULL default ' ',
  "AAAAVARI" varchar(10) NOT NULL default ' ',
  "AAAAVQSH" varchar(40) NOT NULL default ' ',
  "AAABMIRX" varchar(40) NOT NULL default ' ',
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEUZ" varchar(12) NOT NULL default ' ',
  "AAAAVARN" int(11) NOT NULL default '0',
  "AAAAVARO" varchar(8) NOT NULL default '00000000',
  "AAAAVARP" varchar(6) NOT NULL default '000000',
  "AAAAVARQ" varchar(16) NOT NULL default ' ',
  "AAAAVARR" varchar(8) NOT NULL default '00000000',
  "AAAAVARS" varchar(6) NOT NULL default '000000',
  "AAAAVART" varchar(16) NOT NULL default ' ',
  "AAABHZBX" varchar(16) NOT NULL default ' ',
  "AAABMLDW" varchar(30) NOT NULL default ' ',
  "AAABHZBC" varchar(128) NOT NULL default ' ',
  "AAABHZBD" varchar(1) NOT NULL default ' ',
  "AAABHZBE" varchar(128) NOT NULL default ' ',
  "AAABHZBF" varchar(1) NOT NULL default ' ',
  "AAABHZBG" varchar(128) NOT NULL default ' ',
  "AAABHZBH" varchar(1) NOT NULL default ' ',
  "AAABHZBI" varchar(128) NOT NULL default ' ',
  "AAABHZBJ" varchar(1) NOT NULL default ' ',
  "AAABJVWR" varchar(128) NOT NULL default ' ',
  "AAAAVASF" varchar(255) NOT NULL default ' ',
  "AAAAVASG" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAAAVASH","AAAAVASI","AAAAVASJ","AAAAVASK","AAAAVASL","AAAAVASM","AAAAVASN","AAAAVASO"),
  KEY "AAAAVASP" ("AAAAVASQ","AAAAVQSH","AAABMIRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAST" (
  "AAABLMNC" varchar(15) NOT NULL default ' ',
  "AAAAVAUW" varchar(40) NOT NULL default ' ',
  "AAAAVAUX" varchar(40) NOT NULL default ' ',
  "AAAAVATW" varchar(40) NOT NULL default ' ',
  "AAAAVATX" varchar(40) NOT NULL default ' ',
  "AAAAVATY" varchar(40) NOT NULL default ' ',
  "AAAAVATZ" varchar(40) NOT NULL default ' ',
  "AAAAVAUA" varchar(40) NOT NULL default ' ',
  "AAAAVAUB" varchar(40) NOT NULL default ' ',
  "AAAAVAUC" varchar(40) NOT NULL default ' ',
  "AAAAVAUD" varchar(40) NOT NULL default ' ',
  "AAAAVAUE" varchar(20) NOT NULL default ' ',
  "AAAAVAUF" varchar(3) NOT NULL default '000',
  "AAAAVAUG" int(11) NOT NULL default '0',
  "AAAAVAUH" int(11) NOT NULL default '0',
  "AAAAVAUI" int(11) NOT NULL default '0',
  "AAAAVAUJ" int(11) NOT NULL default '0',
  "AAAAVAUK" int(11) NOT NULL default '0',
  "AAAAVAUL" int(11) NOT NULL default '0',
  "AAAAVATN" int(11) NOT NULL default '0',
  "AAAAVATO" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLMNC","AAAAVAUW","AAAAVAUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVATS" (
  "AAABLMNC" varchar(15) NOT NULL default ' ',
  "AAAAVAUW" varchar(40) NOT NULL default ' ',
  "AAAAVAUX" varchar(40) NOT NULL default ' ',
  "AAAAVATW" varchar(40) NOT NULL default ' ',
  "AAAAVATX" varchar(40) NOT NULL default ' ',
  "AAAAVATY" varchar(40) NOT NULL default ' ',
  "AAAAVATZ" varchar(40) NOT NULL default ' ',
  "AAAAVAUA" varchar(40) NOT NULL default ' ',
  "AAAAVAUB" varchar(40) NOT NULL default ' ',
  "AAAAVAUC" varchar(40) NOT NULL default ' ',
  "AAAAVAUD" varchar(40) NOT NULL default ' ',
  "AAAAVAUE" varchar(20) NOT NULL default ' ',
  "AAAAVAUF" varchar(3) NOT NULL default '000',
  "AAAAVAUG" int(11) NOT NULL default '0',
  "AAAAVAUH" int(11) NOT NULL default '0',
  "AAAAVAUI" int(11) NOT NULL default '0',
  "AAAAVAUJ" int(11) NOT NULL default '0',
  "AAAAVAUK" int(11) NOT NULL default '0',
  "AAAAVAUL" int(11) NOT NULL default '0',
  "AAAAVAUM" varchar(4) NOT NULL default ' ',
  "AAAAVAUN" int(11) NOT NULL default '0',
  "AAAAVAUO" varchar(20) NOT NULL default ' ',
  "AAAAVAUP" varchar(3) NOT NULL default '000',
  "AAAAVAUQ" int(11) NOT NULL default '0',
  "AAAAVAUR" int(11) NOT NULL default '0',
  "AAAAVAUS" int(11) NOT NULL default '0',
  "AAAAVAUT" int(11) NOT NULL default '0',
  "AAAAVAUU" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLMNC","AAAAVAUW","AAAAVAUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAUY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVAVH" varchar(32) NOT NULL default ' ',
  "AAAAVAVB" varchar(30) NOT NULL default ' ',
  "AAABETFJ" decimal(15,0) NOT NULL default '0',
  "AAABHHSC" varchar(12) NOT NULL default ' ',
  "AAABKRYU" decimal(15,0) NOT NULL default '0',
  "AAABLRQM" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAVAVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAVI" (
  "AAABMREL" varchar(16) NOT NULL default ' ',
  "AAAAVAVK" int(11) NOT NULL default '0',
  "AAABILXZ" varchar(8) NOT NULL default '00000000',
  "AAABILYA" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAVO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVBOG" varchar(32) NOT NULL default ' ',
  "AAABLHGN" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVAVS" decimal(15,0) NOT NULL default '0',
  "AAAAVAVT" varchar(12) NOT NULL default ' ',
  "AAAAVAVU" decimal(15,0) NOT NULL default '0',
  "AAAAVAVV" varchar(12) NOT NULL default ' ',
  "AAAAVAVW" tinyint(3) unsigned NOT NULL default '0',
  "AAABMRFO" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVBDI" varchar(10) NOT NULL default ' ',
  "AAAAVBCF" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLUXQ","AAAAVBOG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAWC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVBOG" varchar(32) NOT NULL default ' ',
  "AAAAVAWF" int(11) NOT NULL default '0',
  "AAAAVAWG" varchar(1) NOT NULL default ' ',
  "AAAAVAWH" int(11) NOT NULL default '0',
  "AAAAVAWI" varchar(1) NOT NULL default ' ',
  "AAAAVAWJ" int(11) NOT NULL default '0',
  "AAAAVAWK" varchar(1) NOT NULL default ' ',
  "AAAAVAWL" varchar(20) NOT NULL default ' ',
  "AAAAVAWM" varchar(5) NOT NULL default '00000',
  "AAAAVAWN" int(11) NOT NULL default '0',
  "AAAAVAWO" varchar(1) NOT NULL default ' ',
  "AAAAVAWP" int(11) NOT NULL default '0',
  "AAAAVAWQ" varchar(1) NOT NULL default ' ',
  "AAAAVAWR" int(11) NOT NULL default '0',
  "AAAAVAWS" varchar(1) NOT NULL default ' ',
  "AAAAVAWT" tinyint(3) unsigned NOT NULL default '0',
  "AAABCRXD" varchar(3) NOT NULL default '000',
  "AAABCRXC" varchar(2) NOT NULL default '00',
  "AAABCRXB" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLUXQ","AAAAVBOG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAWZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVBOG" varchar(32) NOT NULL default ' ',
  "AAAAVBGZ" varchar(32) NOT NULL default ' ',
  "AAAAVAXD" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVAXE" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVBJH" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAVBOG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAXI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVBOG" varchar(32) NOT NULL default ' ',
  "AAABLAUG" varchar(2) NOT NULL default ' ',
  "AAABLJJN" varchar(8) NOT NULL default ' ',
  "AAABMLPV" varchar(32) NOT NULL default ' ',
  "AAAAVBNO" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAVBOG","AAABLAUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAXS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAVBOG" varchar(32) NOT NULL default ' ',
  "AAAAVAXW" varchar(128) NOT NULL default ' ',
  "AAAAVAXX" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAAAVBOG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAYB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVBGZ" varchar(32) NOT NULL default ' ',
  "AAAAVAYE" varchar(32) NOT NULL default ' ',
  "AAAAVAYF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAVBGZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAYI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAVBGZ" varchar(32) NOT NULL default ' ',
  "AAABBIQE" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAAAVBGZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAYQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVBOG" varchar(32) NOT NULL default ' ',
  "AAAAVBNO" varchar(32) NOT NULL default ' ',
  "AAAAWYIL" varchar(32) NOT NULL default ' ',
  "AAABMLPV" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAVBOG","AAAAVBNO","AAAAWYIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAZA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVBOG" varchar(32) NOT NULL default ' ',
  "AAAAVBNO" varchar(32) NOT NULL default ' ',
  "AAABHIXN" varchar(32) NOT NULL default ' ',
  "AAAAVAZF" varchar(255) NOT NULL default ' ',
  "AAAAVAZG" varchar(255) NOT NULL default ' ',
  "AAAAVAZH" varchar(1) NOT NULL default ' ',
  "AAAAVAZI" varchar(1) NOT NULL default ' ',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAAAVAZK" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAAAVBOG","AAAAVBNO","AAABHIXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVAZP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVBOG" varchar(32) NOT NULL default ' ',
  "AAAAXARU" varchar(32) NOT NULL default ' ',
  "AAAAVBBW" varchar(32) NOT NULL default ' ',
  "AAAAVBBX" varchar(32) NOT NULL default ' ',
  "AAABEDQH" varchar(32) NOT NULL default ' ',
  "AAAAVAZW" VARBINARY(255),
  "AAABKRHS" VARBINARY(255),
  PRIMARY KEY ("AAABLUXQ","AAAAVBOG","AAAAXARU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBAB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVBOG" varchar(32) NOT NULL default ' ',
  "AAAAXARU" varchar(32) NOT NULL default ' ',
  "AAAAVBAM" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVBAG" varchar(30) NOT NULL default ' ',
  "AAAAVBAH" varchar(30) NOT NULL default ' ',
  "AAAAVBAS" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAVBOG","AAAAXARU","AAAAVBAM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBAN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVBOG" varchar(32) NOT NULL default ' ',
  "AAAAXARU" varchar(32) NOT NULL default ' ',
  "AAAAVBAW" varchar(30) NOT NULL default ' ',
  "AAAAVBAS" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAVBOG","AAAAXARU","AAAAVBAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBAX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVBOG" varchar(32) NOT NULL default ' ',
  "AAAAVBBW" varchar(32) NOT NULL default ' ',
  "AAAAVBBX" varchar(32) NOT NULL default ' ',
  "AAAAVBBC" varchar(1) NOT NULL default ' ',
  "AAABMJWI" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVBBE" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVLSN" tinyint(3) unsigned NOT NULL default '0',
  "AAABDROT" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVBBH" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAAAVBOG","AAAAVBBW","AAAAVBBX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBBM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAVBOG" varchar(32) NOT NULL default ' ',
  "AAAAVBBW" varchar(32) NOT NULL default ' ',
  "AAAAVBBX" varchar(32) NOT NULL default ' ',
  "AAABFNBV" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAAAVBOG","AAAAVBBW","AAAAVBBX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBBY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVBOG" varchar(32) NOT NULL default ' ',
  "AAAAVBDI" varchar(10) NOT NULL default ' ',
  "AAAAWYIL" varchar(32) NOT NULL default ' ',
  "AAABJEMU" varchar(30) NOT NULL default ' ',
  "AAAAVBCP" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVBCF" varchar(2) NOT NULL default '00',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(70) NOT NULL default ' ',
  "AAABMNNA" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAVBOG","AAAAVBDI","AAAAWYIL","AAABJEMU","AAAAVBCP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBCQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVBOG" varchar(32) NOT NULL default ' ',
  "AAAAVBDI" varchar(10) NOT NULL default ' ',
  "AAAAVBCU" varchar(1) NOT NULL default ' ',
  "AAAAVBJH" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAVBOG","AAAAVBDI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBCZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAVBOG" varchar(32) NOT NULL default ' ',
  "AAAAVBDI" varchar(10) NOT NULL default ' ',
  "AAABHTUP" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAAAVBOG","AAAAVBDI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBDJ" (
  "AAABEDQH" varchar(32) NOT NULL default ' ',
  "AAABJSLF" varchar(30) NOT NULL default ' ',
  "AAAAVBDM" varchar(1) NOT NULL default ' ',
  "AAAAVBDN" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABEDQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBDP" (
  "AAABEDQH" varchar(32) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAVBDS" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABEDQH","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBDV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVBGZ" varchar(32) NOT NULL default ' ',
  "AAAAVBOG" varchar(32) NOT NULL default ' ',
  "AAAAVBEQ" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVBEA" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAAAVBGZ","AAAAVBOG","AAAAVBEQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBEF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVBOG" varchar(32) NOT NULL default ' ',
  "AAAAVBEQ" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVBHU" decimal(15,0) NOT NULL default '0',
  "AAAAVBHV" decimal(15,0) NOT NULL default '0',
  "AAAAVBEL" decimal(21,7) NOT NULL default '0.0000000',
  "AAAAVBEM" decimal(21,7) NOT NULL default '0.0000000',
  "AAABMKFY" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAAAVBOG","AAAAVBEQ","AAAAVBHU","AAAAVBHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBET" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVBOG" varchar(32) NOT NULL default ' ',
  "AAAAVBHU" decimal(15,0) NOT NULL default '0',
  "AAAAVBHV" decimal(15,0) NOT NULL default '0',
  "AAAAVBEY" varchar(70) NOT NULL default ' ',
  "AAAAVBEZ" varchar(70) NOT NULL default ' ',
  "AAAAVBFA" varchar(70) NOT NULL default ' ',
  "AAAAVBFB" varchar(70) NOT NULL default ' ',
  "AAAAVBFC" varchar(70) NOT NULL default ' ',
  "AAAAVBFD" varchar(70) NOT NULL default ' ',
  "AAAAVBFE" varchar(70) NOT NULL default ' ',
  "AAAAVBFF" varchar(70) NOT NULL default ' ',
  "AAAAVBFG" varchar(70) NOT NULL default ' ',
  "AAAAVBFH" varchar(70) NOT NULL default ' ',
  "AAAAVBFI" varchar(70) NOT NULL default ' ',
  "AAAAVBFJ" varchar(70) NOT NULL default ' ',
  "AAAAVBFK" varchar(70) NOT NULL default ' ',
  "AAAAVBFL" varchar(70) NOT NULL default ' ',
  "AAAAVBFM" varchar(70) NOT NULL default ' ',
  "AAAAVBHK" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAAAVBOG","AAAAVBHU","AAAAVBHV"),
  KEY "AAAAVBFS" ("AAABLUXQ","AAAAVBHU","AAAAVBHV","AAAAVBOG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBFX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVBOG" varchar(32) NOT NULL default ' ',
  "AAAAVBNO" varchar(32) NOT NULL default ' ',
  "AAAAVBHU" decimal(15,0) NOT NULL default '0',
  "AAAAVBHV" decimal(15,0) NOT NULL default '0',
  "AAAAVBGD" varchar(70) NOT NULL default ' ',
  "AAAAVBGE" varchar(70) NOT NULL default ' ',
  "AAAAVBGF" varchar(70) NOT NULL default ' ',
  "AAAAVBGG" varchar(70) NOT NULL default ' ',
  "AAAAVBGH" varchar(70) NOT NULL default ' ',
  "AAAAVBHK" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAAAVBOG","AAAAVBNO","AAAAVBHU","AAAAVBHV"),
  KEY "AAAAVBGO" ("AAABLUXQ","AAAAVBOG","AAAAVBHU","AAAAVBHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBGT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVBGZ" varchar(32) NOT NULL default ' ',
  "AAAAVBOG" varchar(32) NOT NULL default ' ',
  "AAAAVBGX" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAVBGZ","AAAAVBOG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBHB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVBOG" varchar(32) NOT NULL default ' ',
  "AAAAVBOH" varchar(32) NOT NULL default ' ',
  "AAAAVBOI" varchar(32) NOT NULL default ' ',
  "AAAAVBHU" decimal(15,0) NOT NULL default '0',
  "AAAAVBHV" decimal(15,0) NOT NULL default '0',
  "AAAAVBHI" varchar(70) NOT NULL default ' ',
  "AAAAVBHJ" varchar(70) NOT NULL default ' ',
  "AAAAVBHK" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAAAVBOG","AAAAVBOH","AAAAVBOI","AAAAVBHU","AAAAVBHV"),
  KEY "AAAAVBHR" ("AAABLUXQ","AAAAVBOG","AAAAVBHU","AAAAVBHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBHW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAWYIL" varchar(32) NOT NULL default ' ',
  "AAAAVBJW" varchar(22) NOT NULL default ' ',
  "AAABJEMU" varchar(30) NOT NULL default ' ',
  "AAAAVBIK" tinyint(3) unsigned NOT NULL default '0',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(70) NOT NULL default ' ',
  "AAABMNNA" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAWYIL","AAAAVBJW","AAABJEMU","AAAAVBIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBIL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAWYIL" varchar(32) NOT NULL default ' ',
  "AAAAVBJW" varchar(22) NOT NULL default ' ',
  "AAABLOAK" varchar(1) NOT NULL default ' ',
  "AAAAVBJH" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAWYIL","AAAAVBJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBIU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAWYIL" varchar(32) NOT NULL default ' ',
  "AAAAVBIY" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAAAWYIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBJC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAWYIL" varchar(32) NOT NULL default ' ',
  "AAAAVBJW" varchar(22) NOT NULL default ' ',
  "AAABJEMU" varchar(30) NOT NULL default ' ',
  "AAAAVBJH" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAWYIL","AAAAVBJW","AAABJEMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBJM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAWYIL" varchar(32) NOT NULL default ' ',
  "AAAAVBJW" varchar(22) NOT NULL default ' ',
  "AAABJEMU" varchar(30) NOT NULL default ' ',
  "AAABGZYP" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAAAWYIL","AAAAVBJW","AAABJEMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBJY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHFSP" varchar(8) NOT NULL default ' ',
  "AAAAVBOG" varchar(32) NOT NULL default ' ',
  "AAAAVBNO" varchar(32) NOT NULL default ' ',
  "AAABLNAW" varchar(1) NOT NULL default ' ',
  "AAABARRT" varchar(1) NOT NULL default ' ',
  "AAABJSLF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABHFSP","AAAAVBOG","AAAAVBNO","AAABLNAW","AAABARRT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBKM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHFSP" varchar(8) NOT NULL default ' ',
  "AAAAVBKQ" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABHFSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBKU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGKTX" varchar(32) NOT NULL default ' ',
  "AAAAVBLW" varchar(32) NOT NULL default ' ',
  "AAAAVBKY" varchar(255) NOT NULL default ' ',
  "AAABBJUB" tinyint(3) unsigned NOT NULL default '0',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGKTX","AAAAVBLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBLE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGKTX" varchar(32) NOT NULL default ' ',
  "AAAAVBLW" varchar(32) NOT NULL default ' ',
  "AAAAVBLJ" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABGKTX","AAAAVBLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBLO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVBLW" varchar(32) NOT NULL default ' ',
  "AAAAVBLX" varchar(32) NOT NULL default ' ',
  "AAAAVBLS" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVBLT" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVBLU" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAAAVBLW","AAAAVBLX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBLY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVBMD" varchar(32) NOT NULL default ' ',
  "AAAAVBME" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAVBMD","AAAAVBME")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBMF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVBOG" varchar(32) NOT NULL default ' ',
  "AAABMLPV" varchar(32) NOT NULL default ' ',
  "AAABLJJN" varchar(8) NOT NULL default ' ',
  "AAAAVBNO" varchar(32) NOT NULL default ' ',
  "AAABGKTL" tinyint(3) unsigned NOT NULL default '0',
  "AAABKWJY" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVBMN" tinyint(3) unsigned NOT NULL default '0',
  "AAABHFAP" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVBOB" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVBOC" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVBOD" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVBMS" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAAAVBOG","AAABMLPV","AAABLJJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBMX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVBOG" varchar(32) NOT NULL default ' ',
  "AAAAVBNO" varchar(32) NOT NULL default ' ',
  "AAABMLPV" varchar(32) NOT NULL default ' ',
  "AAABLJJN" varchar(8) NOT NULL default ' ',
  "AAABGKTL" tinyint(3) unsigned NOT NULL default '0',
  "AAABKWJY" tinyint(3) unsigned NOT NULL default '0',
  "AAABHFAP" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVBNG" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVBOB" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVBOC" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVBOD" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVBNK" tinyint(3) unsigned NOT NULL default '0',
  "AAABEDFZ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAVBOG","AAAAVBNO","AAABMLPV","AAABLJJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBNR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVBOG" varchar(32) NOT NULL default ' ',
  "AAAAVBOH" varchar(32) NOT NULL default ' ',
  "AAAAVBOI" varchar(32) NOT NULL default ' ',
  "AAABMLPV" varchar(32) NOT NULL default ' ',
  "AAABLJJN" varchar(8) NOT NULL default ' ',
  "AAABGKTL" tinyint(3) unsigned NOT NULL default '0',
  "AAABKWJY" tinyint(3) unsigned NOT NULL default '0',
  "AAABHFAP" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVBOB" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVBOC" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVBOD" tinyint(3) unsigned NOT NULL default '0',
  "AAABEDFZ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAVBOG","AAAAVBOH","AAAAVBOI","AAABMLPV","AAABLJJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBOL" (
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAAAVBON" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBOP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(22) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBPA" (
  "AAABBJZR" varchar(8) NOT NULL default ' ',
  "AAAAVCBX" varchar(3) NOT NULL default '000',
  "AAAAVBPD" varchar(1) NOT NULL default ' ',
  "AAAAVBPE" varchar(70) NOT NULL default ' ',
  "AAAAVBPF" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABIMEK" varchar(8) NOT NULL default '00000000',
  "AAABIMEL" varchar(6) NOT NULL default ' ',
  "AAAAVBPJ" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default ' ',
  "AAAAVBPM" varchar(1) NOT NULL default ' ',
  "AAAAVBPN" varchar(1) NOT NULL default ' ',
  "AAAAVBPO" varchar(1) NOT NULL default ' ',
  "AAAAVBPP" varchar(1) NOT NULL default ' ',
  "AAAAVBPQ" varchar(1) NOT NULL default ' ',
  "AAAAVBPR" varchar(1) NOT NULL default ' ',
  "AAAAVBPS" varchar(1) NOT NULL default ' ',
  "AAAAVBPT" varchar(4) NOT NULL default ' ',
  "AAAAVBPU" varchar(4) NOT NULL default ' ',
  "AAAAVBPV" varchar(3) NOT NULL default ' ',
  "AAAAVBPW" varchar(3) NOT NULL default ' ',
  "AAAAVBPX" varchar(1) NOT NULL default ' ',
  "AAAAVBPY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABBJZR","AAAAVCBX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBQB" (
  "AAABBJZR" varchar(8) NOT NULL default ' ',
  "AAAAVCBX" varchar(3) NOT NULL default '000',
  "AAABMJWN" varchar(10) NOT NULL default ' ',
  "AAABMJCM" varchar(2) NOT NULL default ' ',
  "AAABMMJT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABBJZR","AAAAVCBX","AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBQK" (
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABIMEK" varchar(8) NOT NULL default '00000000',
  "AAABIMEL" varchar(6) NOT NULL default ' ',
  "AAABLTLR" varchar(3) NOT NULL default ' ',
  "AAAAVBYN" varchar(16) NOT NULL default '0000000000000000',
  "AAAAVBYO" varchar(16) NOT NULL default '0000000000000000',
  "AAAAVBYP" varchar(16) NOT NULL default '0000000000000000',
  "AAAAVBRJ" varchar(1) NOT NULL default ' ',
  "AAAAVBRK" varchar(16) NOT NULL default '0000000000000000',
  "AAAAVBRL" varchar(16) NOT NULL default '0000000000000000',
  "AAAAVBRM" varchar(16) NOT NULL default '0000000000000000',
  "AAAAVBRN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLENH","AAABIMEK","AAABIMEL","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBRB" (
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABIMEK" varchar(8) NOT NULL default '00000000',
  "AAABIMEL" varchar(6) NOT NULL default ' ',
  "AAABLTLR" varchar(3) NOT NULL default ' ',
  "AAAAVBYN" varchar(16) NOT NULL default '0000000000000000',
  "AAAAVBYO" varchar(16) NOT NULL default '0000000000000000',
  "AAAAVBYP" varchar(16) NOT NULL default '0000000000000000',
  "AAAAVBRJ" varchar(1) NOT NULL default ' ',
  "AAAAVBRK" varchar(16) NOT NULL default '0000000000000000',
  "AAAAVBRL" varchar(16) NOT NULL default '0000000000000000',
  "AAAAVBRM" varchar(16) NOT NULL default '0000000000000000',
  "AAAAVBRN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLENH","AAABIMEK","AAABIMEL","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBRS" (
  "AAABLTLR" varchar(3) NOT NULL default ' ',
  "AAABLGEL" varchar(40) NOT NULL default ' ',
  "AAABFTXC" varchar(30) NOT NULL default ' ',
  "AAAAVBRW" varchar(16) NOT NULL default '0000000000000000',
  PRIMARY KEY ("AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBRY" (
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABIMEK" varchar(8) NOT NULL default '00000000',
  "AAABIMEL" varchar(6) NOT NULL default ' ',
  "AAAAVCBX" varchar(3) NOT NULL default '000',
  "AAAAVBSD" int(11) NOT NULL default '0',
  "AAAAVBSE" int(11) NOT NULL default '0',
  "AAAAVBSF" int(11) NOT NULL default '0',
  "AAAAVBSG" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLENH","AAABIMEK","AAABIMEL","AAAAVCBX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBSL" (
  "AAAAVBSU" varchar(10) NOT NULL default ' ',
  "AAABMOIA" varchar(8) NOT NULL default ' ',
  "AAAAVBSO" varchar(3) NOT NULL default '000',
  "AAAAVBSP" varchar(1) NOT NULL default ' ',
  "AAABIHMM" varchar(1) NOT NULL default ' ',
  "AAAAVBSR" varchar(8) NOT NULL default '00000000',
  "AAAAVPZF" varchar(6) NOT NULL default '000000',
  "AAABLESQ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAAAVBSU","AAABMOIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBSW" (
  "AAAAVCBV" varchar(3) NOT NULL default ' ',
  "AAABHYMV" varchar(20) NOT NULL default ' ',
  "AAAAVBSZ" varchar(50) NOT NULL default ' ',
  "AAAAVBTA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAVCBV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBTC" (
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABIMEK" varchar(8) NOT NULL default '00000000',
  "AAABIMEL" varchar(6) NOT NULL default ' ',
  "AAABLOSZ" varchar(10) NOT NULL default ' ',
  "AAAAVBTH" varchar(20) NOT NULL default ' ',
  "AAABHMQN" varchar(10) NOT NULL default ' ',
  "AAAAVBTJ" varchar(20) NOT NULL default ' ',
  "AAABLUVJ" varchar(20) NOT NULL default ' ',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  "AAAAVBTM" varchar(4) NOT NULL default ' ',
  "AAABDTWE" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLENH","AAABIMEK","AAABIMEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBTR" (
  "AAABANNN" varchar(3) NOT NULL default ' ',
  "AAAAVBTT" varchar(4) NOT NULL default ' ',
  "AAAAVBTU" varchar(15) NOT NULL default ' ',
  "AAAAVBTV" varchar(40) NOT NULL default ' ',
  "AAAAVBTW" varchar(8) NOT NULL default ' ',
  "AAAAVBTX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABANNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBTZ" (
  "AAAAVCBX" varchar(3) NOT NULL default '000',
  "AAABANNN" varchar(3) NOT NULL default ' ',
  "AAABFOVQ" varchar(20) NOT NULL default ' ',
  "AAAAVBUD" varchar(4) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABIMEK" varchar(8) NOT NULL default '00000000',
  "AAABIMEL" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAAAVCBX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBUI" (
  "AAAAVCBX" varchar(3) NOT NULL default '000',
  "AAAAVBUK" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAAVCBX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBUM" (
  "AAABMREL" varchar(10) NOT NULL default ' ',
  "AAABLOSZ" varchar(10) NOT NULL default ' ',
  "AAABMOIA" varchar(8) NOT NULL default ' ',
  "AAAAVBUQ" int(11) NOT NULL default '0',
  "AAAAVBUR" varchar(1) NOT NULL default ' ',
  "AAAAVBUS" varchar(10) NOT NULL default ' ',
  "AAAAVBUT" varchar(50) NOT NULL default ' ',
  "AAAAVBUU" varchar(1) NOT NULL default ' ',
  "AAAAVBUV" varchar(10) NOT NULL default ' ',
  "AAABKBZZ" varchar(4) NOT NULL default ' ',
  "AAAAVBUX" varchar(1) NOT NULL default ' ',
  "AAABIEUW" varchar(2) NOT NULL default '00',
  "AAABFPKA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABLOSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBVC" (
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABKBZZ" varchar(4) NOT NULL default ' ',
  "AAABBUSK" varchar(1) NOT NULL default ' ',
  "AAAAVBVG" varchar(1) NOT NULL default ' ',
  "AAAAVBVH" varchar(1) NOT NULL default ' ',
  "AAABHUMB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLENH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBVK" (
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABMOIA" varchar(8) NOT NULL default ' ',
  "AAABIMEK" varchar(8) NOT NULL default '00000000',
  "AAABIMEL" varchar(6) NOT NULL default ' ',
  "AAABGHXN" varchar(20) NOT NULL default ' ',
  "AAAAVBYN" int(11) NOT NULL default '0',
  "AAAAVFDW" smallint(6) NOT NULL default '0',
  "AAAAVBVS" smallint(6) NOT NULL default '0',
  "AAAAVFDV" smallint(6) NOT NULL default '0',
  "AAAAVFDX" smallint(6) NOT NULL default '0',
  "AAAAXFUI" varchar(40) NOT NULL default ' ',
  "AAAAVBVW" varchar(1) NOT NULL default ' ',
  "AAABLKMO" varchar(8) NOT NULL default '00000000',
  "AAABLKMP" varchar(6) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAVBWA" int(11) NOT NULL default '0',
  "AAAAVBWB" varchar(16) NOT NULL default '0000000000000000',
  "AAAAVBWC" int(11) NOT NULL default '0',
  "AAAAVBWD" int(11) NOT NULL default '0',
  "AAAAVBWE" int(11) NOT NULL default '0',
  "AAAAVBWF" int(11) NOT NULL default '0',
  "AAAAVBYR" varchar(1) NOT NULL default ' ',
  "AAAAVBYS" varchar(1) NOT NULL default ' ',
  "AAAAVBYT" varchar(1) NOT NULL default ' ',
  "AAAAVBYU" varchar(1) NOT NULL default ' ',
  "AAAAVBYV" varchar(1) NOT NULL default ' ',
  "AAABJZAF" varchar(8) NOT NULL default '00000000',
  "AAAAVBWM" varchar(6) NOT NULL default '000000',
  "AAABKUGB" smallint(6) NOT NULL default '0',
  "AAAAVBWO" varchar(20) NOT NULL default ' ',
  "AAAAVBWP" varchar(20) NOT NULL default ' ',
  "AAAAVBWQ" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLENH","AAABMOIA","AAABIMEK","AAABIMEL"),
  KEY "AAAAVBWV" ("AAABLENH","AAABIMEK","AAABIMEL"),
  KEY "AAAAVBWZ" ("AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBXB" (
  "AAABMJWN" varchar(10) NOT NULL default ' ',
  "AAABMMJT" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBXG" (
  "AAABMJWN" varchar(10) NOT NULL default ' ',
  "AAABMMJT" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBXL" (
  "AAAAVBYD" varchar(2) NOT NULL default ' ',
  "AAAAVBXN" varchar(35) NOT NULL default ' ',
  "AAAAVBXO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAAVBYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBXQ" (
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABJSAN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBXT" (
  "AAABHCQK" varchar(32) NOT NULL default ' ',
  "AAABDPCG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHCQK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBXX" (
  "AAABMOIA" varchar(8) NOT NULL default ' ',
  "AAABLTLR" varchar(3) NOT NULL default ' ',
  "AAAAVBYD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOIA","AAABLTLR","AAAAVBYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBYE" (
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABMOIA" varchar(8) NOT NULL default ' ',
  "AAABIMEK" varchar(8) NOT NULL default '00000000',
  "AAABIMEL" varchar(6) NOT NULL default ' ',
  "AAAAXDRL" varchar(3) NOT NULL default ' ',
  "AAABLELX" varchar(10) NOT NULL default ' ',
  "AAAAVBYL" varchar(3) NOT NULL default '000',
  "AAABCOLW" varchar(3) NOT NULL default '000',
  "AAAAVBYN" varchar(16) NOT NULL default '0000000000000000',
  "AAAAVBYO" varchar(16) NOT NULL default '0000000000000000',
  "AAAAVBYP" varchar(16) NOT NULL default '0000000000000000',
  "AAAAVBYQ" varchar(1) NOT NULL default ' ',
  "AAAAVBYR" varchar(1) NOT NULL default ' ',
  "AAAAVBYS" varchar(1) NOT NULL default ' ',
  "AAAAVBYT" varchar(1) NOT NULL default ' ',
  "AAAAVBYU" varchar(1) NOT NULL default ' ',
  "AAAAVBYV" varchar(1) NOT NULL default ' ',
  "AAAAVBYW" int(11) NOT NULL default '0',
  "AAAAVBYX" int(11) NOT NULL default '0',
  "AAAAVBYY" int(11) NOT NULL default '0',
  "AAAAVBYZ" int(11) NOT NULL default '0',
  "AAAAVBZA" int(11) NOT NULL default '0',
  "AAABCPET" varchar(8) NOT NULL default ' ',
  "AAAAVBZC" int(11) NOT NULL default '0',
  "AAAAVBZD" varchar(16) NOT NULL default '0000000000000000',
  "AAAAVBZE" varchar(16) NOT NULL default '0000000000000000',
  "AAAAVBZF" varchar(1) NOT NULL default ' ',
  "AAAAVBZG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLENH","AAABMOIA","AAABIMEK","AAABIMEL","AAAAXDRL"),
  KEY "AAAAVBZM" ("AAABLENH","AAABIMEK","AAABIMEL","AAABMOIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVBZR" (
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABIMEK" varchar(8) NOT NULL default '00000000',
  "AAABIMEL" varchar(6) NOT NULL default '000000',
  "AAABETEZ" varchar(20) NOT NULL default ' ',
  "AAAAVBZW" varchar(3) NOT NULL default '000',
  "AAAAVBZX" varchar(3) NOT NULL default '000',
  "AAAAVBZY" varchar(3) NOT NULL default '000',
  "AAAAVBZZ" varchar(3) NOT NULL default '000',
  "AAAAVCAA" varchar(4) NOT NULL default '0000',
  "AAAAVCAB" varchar(4) NOT NULL default '0000',
  "AAAAVCAC" varchar(8) NOT NULL default '00000000',
  "AAAAVCAD" varchar(4) NOT NULL default '0000',
  "AAAAVCAE" varchar(9) NOT NULL default '000000000',
  "AAAAVCAF" varchar(36) NOT NULL default ' ',
  "AAAAVCAG" varchar(8) NOT NULL default '00000000',
  "AAAAVCAH" varchar(4) NOT NULL default '0000',
  "AAAAVCAI" varchar(9) NOT NULL default '000000000',
  "AAAAVCAJ" varchar(36) NOT NULL default ' ',
  "AAAAVCAK" varchar(8) NOT NULL default '00000000',
  "AAAAVCAL" varchar(4) NOT NULL default '0000',
  "AAAAVCAM" varchar(9) NOT NULL default '000000000',
  "AAAAVCAN" varchar(36) NOT NULL default ' ',
  "AAAAVCAO" varchar(8) NOT NULL default '00000000',
  "AAAAVCAP" varchar(4) NOT NULL default '0000',
  "AAAAVCAQ" varchar(9) NOT NULL default '000000000',
  "AAAAVCAR" varchar(36) NOT NULL default ' ',
  "AAAAVCAS" varchar(8) NOT NULL default '00000000',
  "AAAAVCAT" varchar(4) NOT NULL default '0000',
  "AAAAVCAU" varchar(9) NOT NULL default '000000000',
  "AAAAVCAV" varchar(36) NOT NULL default ' ',
  "AAAAVCAW" varchar(8) NOT NULL default '00000000',
  "AAAAVCAX" varchar(4) NOT NULL default '0000',
  "AAAAVCAY" varchar(9) NOT NULL default '000000000',
  "AAAAVCAZ" varchar(12) NOT NULL default '000000000000',
  "AAAAVCBA" varchar(12) NOT NULL default '000000000000',
  "AAAAVCBB" varchar(12) NOT NULL default '000000000000',
  "AAAAVCBC" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLENH","AAABIMEK","AAABIMEL","AAABETEZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVCBH" (
  "AAABMJWN" varchar(16) NOT NULL default ' ',
  "AAABMMJT" varchar(50) NOT NULL default ' ',
  "AAABJEBV" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVCBM" (
  "AAABLUZL" varchar(10) NOT NULL default ' ',
  "AAAAVCBQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUZL","AAAAVCBQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVCBR" (
  "AAAAVCBV" varchar(3) NOT NULL default ' ',
  "AAABANNN" varchar(3) NOT NULL default ' ',
  "AAAAVCBX" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAAAVCBV","AAABANNN","AAAAVCBX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVCBY" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(32) NOT NULL default ' ',
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
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVCCO" (
  "AAABIWPY" varchar(1) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWPY"),
  UNIQUE KEY "AAAAVCCV" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVCCX" (
  "AAABIWQH" varchar(30) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWQH"),
  KEY "AAAAVCDE" ("AAABLETY","AAABIWQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVCDH" (
  "AAABMBXY" VARBINARY(255) NOT NULL,
  "AAABMREF" varchar(32) NOT NULL default ' ',
  "AAABGRPF" VARBINARY(255),
  PRIMARY KEY ("AAABMBXY"(255),"AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVCDN" (
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAABGRPQ" varchar(60) NOT NULL default ' ',
  "AAABGRPR" varchar(60) NOT NULL default ' ',
  "AAABGRPS" varchar(60) NOT NULL default ' ',
  "AAABGRPT" varchar(60) NOT NULL default ' ',
  "AAABGRPU" varchar(60) NOT NULL default ' ',
  "AAABGRPV" varchar(120) NOT NULL default ' ',
  "AAABLKZT" varchar(1) NOT NULL default ' ',
  "AAAAVCDW" VARBINARY(255),
  PRIMARY KEY ("AAABLTVR"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVCDY" (
  "AAABMBXY" VARBINARY(255) NOT NULL,
  "AAABLMUD" varchar(120) NOT NULL default ' ',
  "AAAAVCFB" longtext,
  PRIMARY KEY ("AAABMBXY"(255),"AAABLMUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVCEE" (
  "AAABMBXY" VARBINARY(255) NOT NULL,
  "AAABGKWP" varchar(60) NOT NULL default ' ',
  "AAABLGEF" varchar(60) NOT NULL default ' ',
  "AAABGXFD" varchar(60) NOT NULL default ' ',
  "AAAAVCEJ" varchar(2) NOT NULL default ' ',
  "AAAAVCEK" longtext,
  "AAAAVCEL" varchar(120) NOT NULL default ' ',
  "AAAAVCEM" varchar(60) NOT NULL default ' ',
  "AAAAVCEN" VARBINARY(255),
  "AAAAVCEO" varchar(10) NOT NULL default ' ',
  "AAAAVCEP" varchar(10) NOT NULL default ' ',
  "AAAAVCEQ" varchar(10) NOT NULL default ' ',
  "AAAAVCER" varchar(10) NOT NULL default ' ',
  "AAABLKZT" varchar(1) NOT NULL default ' ',
  "AAAAVCET" varchar(120) NOT NULL default ' ',
  "AAAAVCEU" varchar(120) NOT NULL default ' ',
  "AAAAVCEV" varchar(120) NOT NULL default ' ',
  "AAAAVCEW" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVCEY" (
  "AAABMBXY" VARBINARY(255) NOT NULL,
  "AAABLMUD" varchar(120) NOT NULL default ' ',
  "AAAAVCFB" longtext,
  PRIMARY KEY ("AAABMBXY"(255),"AAABLMUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVCFE" (
  "AAABMBXY" VARBINARY(255) NOT NULL,
  "AAABGKWP" varchar(60) NOT NULL default ' ',
  "AAAAVCFM" int(11) NOT NULL default '0',
  "AAAAVCFI" varchar(120) NOT NULL default ' ',
  "AAAAVCFJ" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY"(255),"AAABGKWP","AAAAVCFM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVCFN" (
  "AAABMBXY" VARBINARY(255) NOT NULL,
  "AAAAVCFU" varchar(20) NOT NULL default ' ',
  "AAABLGEF" varchar(60) NOT NULL default ' ',
  "AAABJCNM" varchar(2) NOT NULL default ' ',
  "AAABGKWP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY"(255),"AAAAVCFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--enable_query_log
