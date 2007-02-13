# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+
# |                             tbl35.inc                              |
# |                        Copyright 2006 by MySQL AB                  |
# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+

--disable_query_log
CREATE TABLE "AAABDKMY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUZL" varchar(10) NOT NULL default ' ',
  "AAABDKNH" varchar(10) NOT NULL default '0000000000',
  "AAABDKNP" varchar(10) NOT NULL default '0000000000',
  "AAABKMJV" varchar(30) NOT NULL default ' ',
  "AAABDKNT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUZL","AAABDKNH"),
  KEY "AAABDKNI" ("AAABMRIL","AAABLUZL","AAABKMJV"),
  KEY "AAABDKNM" ("AAABMRIL","AAABLUZL","AAABDKNP"),
  KEY "AAABDKNQ" ("AAABMRIL","AAABLUZL","AAABDKNT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKNU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUZL" varchar(10) NOT NULL default ' ',
  "AAABDKOE" varchar(10) NOT NULL default '0000000000',
  "AAABDKON" varchar(10) NOT NULL default '0000000000',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABLOZE" varchar(18) NOT NULL default ' ',
  "AAABDKOS" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUZL","AAABDKOE"),
  KEY "AAABDKOF" ("AAABMRIL","AAABLUZL","AAABLTUX","AAABLOZE"),
  KEY "AAABDKOK" ("AAABMRIL","AAABLUZL","AAABDKON"),
  KEY "AAABDKOO" ("AAABMRIL","AAABLUZL","AAABLTUX","AAABDKOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKOT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUZL" varchar(10) NOT NULL default ' ',
  "AAABDKPD" varchar(18) NOT NULL default '000000000000000000',
  "AAABDKPN" varchar(18) NOT NULL default '000000000000000000',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABGIHK" varchar(30) NOT NULL default ' ',
  "AAABDRQC" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUZL","AAABDKPD"),
  KEY "AAABDKPE" ("AAABMRIL","AAABLUZL","AAABLTUX","AAABGIHK","AAABDRQC"),
  KEY "AAABDKPK" ("AAABMRIL","AAABLUZL","AAABDKPN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKPO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUZL" varchar(10) NOT NULL default ' ',
  "AAABDKPV" varchar(3) NOT NULL default ' ',
  "AAABDKPS" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUZL","AAABDKPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKPW" (
  "AAABLEJI" varchar(40) NOT NULL default ' ',
  "AAABDKQM" varchar(30) NOT NULL default ' ',
  "AAABDKQN" varchar(30) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABDKQP" varchar(3) NOT NULL default '000',
  "AAABDKQC" varchar(1) NOT NULL default ' ',
  "AAABKMVW" varchar(1) NOT NULL default ' ',
  "AAABDKQE" varchar(1) NOT NULL default ' ',
  "AAABMNLW" varchar(70) NOT NULL default ' ',
  "AAABDKQG" varchar(3) NOT NULL default '000',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  "AAABDKQI" varchar(1) NOT NULL default ' ',
  "AAABDKQJ" int(11) NOT NULL default '0',
  "AAABIFON" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEJI","AAABDKQM","AAABDKQN","AAABMOHM","AAABDKQP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDKQX" varchar(30) NOT NULL default ' ',
  "AAABLUZL" varchar(10) NOT NULL default ' ',
  "AAABDKQU" varchar(30) NOT NULL default ' ',
  "AAABDKQV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDKQX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDKRA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKRC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDVMT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKRG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSC","AAABLTUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKRN" (
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKRS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAABDKSE" varchar(1) NOT NULL default ' ',
  "AAABDKRX" varchar(1) NOT NULL default ' ',
  "AAABDKSS" varchar(2) NOT NULL default '00',
  "AAABDKRZ" varchar(10) NOT NULL default ' ',
  "AAABDKSA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTKC","AAABLXDR","AAABDKSE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKSF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDKSS" varchar(2) NOT NULL default '00',
  "AAABDKSI" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABDKSS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKSL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDKSS" varchar(2) NOT NULL default '00',
  "AAABDKSP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDKSS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKST" (
  "AAABMBXY" varchar(4) NOT NULL default ' ',
  "AAABDKSV" varchar(1) NOT NULL default ' ',
  "AAABDKSW" varchar(1) NOT NULL default ' ',
  "AAABDKSX" varchar(1) NOT NULL default ' ',
  "AAABDKSY" varchar(1) NOT NULL default ' ',
  "AAABDKSZ" varchar(1) NOT NULL default ' ',
  "AAABDKTA" varchar(1) NOT NULL default ' ',
  "AAABDKTB" varchar(1) NOT NULL default ' ',
  "AAABDKTC" varchar(1) NOT NULL default ' ',
  "AAABDKTD" varchar(1) NOT NULL default ' ',
  "AAABDKTE" varchar(1) NOT NULL default ' ',
  "AAABDKTF" varchar(1) NOT NULL default ' ',
  "AAABDKTZ" varchar(1) NOT NULL default ' ',
  "AAABDKUB" varchar(1) NOT NULL default ' ',
  "AAABDKUA" varchar(1) NOT NULL default ' ',
  "AAABDKUC" varchar(1) NOT NULL default ' ',
  "AAABDKUF" varchar(1) NOT NULL default ' ',
  "AAABDKUG" varchar(1) NOT NULL default ' ',
  "AAABDKUH" varchar(1) NOT NULL default ' ',
  "AAABDKUI" varchar(1) NOT NULL default ' ',
  "AAABDKTO" varchar(1) NOT NULL default ' ',
  "AAABHXBD" varchar(1) NOT NULL default ' ',
  "AAABDKUE" varchar(1) NOT NULL default ' ',
  "AAABDKTR" varchar(30) NOT NULL default ' ',
  "AAABDKTS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKTU" (
  "AAABDKVR" varchar(4) NOT NULL default ' ',
  "AAABLUNJ" varchar(4) NOT NULL default ' ',
  "AAABDKTX" varchar(1) NOT NULL default ' ',
  "AAABIEWH" varchar(2) NOT NULL default ' ',
  "AAABDKTZ" varchar(1) NOT NULL default ' ',
  "AAABDKUA" varchar(1) NOT NULL default ' ',
  "AAABDKUB" varchar(1) NOT NULL default ' ',
  "AAABDKUC" varchar(1) NOT NULL default ' ',
  "AAABDKUD" varchar(1) NOT NULL default ' ',
  "AAABDKUE" varchar(1) NOT NULL default ' ',
  "AAABDKUF" varchar(1) NOT NULL default ' ',
  "AAABDKUG" varchar(1) NOT NULL default ' ',
  "AAABDKUH" varchar(1) NOT NULL default ' ',
  "AAABDKUI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDKVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKUK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABLTKC","AAABLXDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKUT" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDKVR" varchar(4) NOT NULL default ' ',
  "AAABDKUW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABDKVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKUZ" (
  "AAABDKVR" varchar(4) NOT NULL default ' ',
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAABDKVD" varchar(1) NOT NULL default ' ',
  "AAABDKVE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDKVR","AAABLTKC","AAABLXDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKVI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDKVR" varchar(4) NOT NULL default ' ',
  "AAABDKVS" varchar(30) NOT NULL default ' ',
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABDKVP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDKVR","AAABDKVS","AAABLTKC","AAABLXDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKVV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDKZB" varchar(22) NOT NULL default ' ',
  "AAABEDJP" varchar(12) NOT NULL default ' ',
  "AAABLEQP" varchar(8) NOT NULL default '00000000',
  "AAABIJIU" varchar(8) NOT NULL default '00000000',
  "AAABENKB" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDKZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKWE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFFG" varchar(8) NOT NULL default ' ',
  "AAABDKWH" varchar(2) NOT NULL default '00',
  "AAABDKWI" varchar(2) NOT NULL default '00',
  "AAABDKWJ" varchar(2) NOT NULL default '00',
  "AAABDKWK" varchar(2) NOT NULL default '00',
  "AAABDKWL" varchar(2) NOT NULL default '00',
  "AAABDKWM" varchar(2) NOT NULL default '00',
  "AAABDKWN" varchar(2) NOT NULL default '00',
  "AAABDKWO" varchar(2) NOT NULL default '00',
  "AAABDKWP" varchar(2) NOT NULL default '00',
  "AAABDKWQ" varchar(2) NOT NULL default '00',
  "AAABDNYG" varchar(2) NOT NULL default '00',
  "AAABDKWS" varchar(1) NOT NULL default ' ',
  "AAABDKWT" varchar(1) NOT NULL default ' ',
  "AAABDKWU" varchar(1) NOT NULL default ' ',
  "AAABDKWV" varchar(1) NOT NULL default ' ',
  "AAABDKWW" varchar(1) NOT NULL default ' ',
  "AAABDKWX" varchar(1) NOT NULL default ' ',
  "AAABDKWY" varchar(1) NOT NULL default ' ',
  "AAABDKWZ" varchar(1) NOT NULL default ' ',
  "AAABDKXA" varchar(1) NOT NULL default ' ',
  "AAABDKXB" varchar(1) NOT NULL default ' ',
  "AAABDKXC" varchar(1) NOT NULL default ' ',
  "AAABMECK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKFFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKXG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDKXS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDKXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKXL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDKXS" varchar(10) NOT NULL default ' ',
  "AAABDKXP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDKXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKXT" (
  "AAABHHCQ" varchar(32) NOT NULL default ' ',
  "AAABDKXV" varchar(5) NOT NULL default ' ',
  "AAABDKXW" varchar(40) NOT NULL default ' ',
  "AAABDKXX" varchar(40) NOT NULL default ' ',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABDKXZ" decimal(15,0) NOT NULL default '0',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABDKYB" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABHHCQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKYD" (
  "AAABHHCQ" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(40) NOT NULL default ' ',
  "AAABJYHA" varchar(6) NOT NULL default ' ',
  "AAABDKYN" varchar(70) NOT NULL default ' ',
  "AAABDKYI" varchar(1) NOT NULL default ' ',
  "AAABDKYJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHHCQ","AAABMBXY","AAABJYHA","AAABDKYN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKYO" (
  "AAABJYHA" varchar(6) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABJYHA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKYS" (
  "AAABHGWF" varchar(5) NOT NULL default ' ',
  "AAABDKYU" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGWF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKYW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDKZB" varchar(22) NOT NULL default ' ',
  "AAABLZLD" varchar(28) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDKZB","AAABLZLD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKZD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJSBU" varchar(1) NOT NULL default ' ',
  "AAABDKZG" varchar(1) NOT NULL default ' ',
  "AAABDKZH" varchar(1) NOT NULL default ' ',
  "AAABHJJK" varchar(1) NOT NULL default ' ',
  "AAABGKPS" varchar(10) NOT NULL default ' ',
  "AAABDKZK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJSBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKZN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJSBU" varchar(1) NOT NULL default ' ',
  "AAABDKZR" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJSBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKZV" (
  "AAABLUER" varchar(6) NOT NULL default ' ',
  "AAABDKZX" varchar(10) NOT NULL default ' ',
  "AAABDKZY" varchar(15) NOT NULL default ' ',
  "AAABDKZZ" varchar(3) NOT NULL default ' ',
  "AAABDLAA" varchar(10) NOT NULL default ' ',
  "AAABDLAB" varchar(6) NOT NULL default ' ',
  "AAABDLWH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLAE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNNV" varchar(20) NOT NULL default ' ',
  "AAABLUER" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNNV","AAABLUER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLAL" (
  "AAABDNLD" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNLD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLAO" (
  "AAABDNLD" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNLD","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLAU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNLZ" varchar(6) NOT NULL default ' ',
  "AAABDLBJ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNLZ","AAABDLBJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLBB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNLZ" varchar(6) NOT NULL default ' ',
  "AAABDLBJ" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNLZ","AAABDLBJ","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLBL" (
  "AAABDLBO" varchar(6) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABDLBO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLBP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLCA" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLCA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLBU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLCA" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLCA","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLCC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNLZ" varchar(6) NOT NULL default ' ',
  "AAABDLNT" varchar(12) NOT NULL default ' ',
  "AAABDLNF" varchar(2) NOT NULL default ' ',
  "AAABDLNG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNLZ","AAABDLNT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLCL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABDNLZ" varchar(6) NOT NULL default ' ',
  "AAABDLNT" varchar(12) NOT NULL default ' ',
  "AAABDLNP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLFZK","AAABDNLZ","AAABDLNT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLCV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNLZ" varchar(6) NOT NULL default ' ',
  "AAABDNLD" varchar(6) NOT NULL default ' ',
  "AAABDLZN" varchar(6) NOT NULL default ' ',
  "AAABDNLE" varchar(2) NOT NULL default ' ',
  "AAABDNLF" varchar(2) NOT NULL default ' ',
  "AAABDNLG" varchar(4) NOT NULL default '0000',
  "AAABDNLH" decimal(10,6) NOT NULL default '0.000000',
  "AAABDNLB" varchar(6) NOT NULL default ' ',
  "AAABDNLA" varchar(6) NOT NULL default ' ',
  "AAABDNMU" varchar(10) NOT NULL default ' ',
  "AAABDNMT" varchar(10) NOT NULL default ' ',
  "AAABDLDI" varchar(1) NOT NULL default ' ',
  "AAABDNLI" decimal(10,6) NOT NULL default '0.000000',
  "AAABDNLJ" decimal(10,6) NOT NULL default '0.000000',
  "AAABDNLK" decimal(10,6) NOT NULL default '0.000000',
  "AAABDNLL" decimal(10,6) NOT NULL default '0.000000',
  "AAABDNLM" decimal(10,6) NOT NULL default '0.000000',
  "AAABDNIC" varchar(2) NOT NULL default ' ',
  "AAABDNID" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNLZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLDS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNLZ" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNLZ","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLEA" (
  "AAABDMBE" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABDMBE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLED" (
  "AAABDMBE" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABDMBE","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLEJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMBE" varchar(6) NOT NULL default ' ',
  "AAABDMBF" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMBE","AAABDMBF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMBE" varchar(6) NOT NULL default ' ',
  "AAABDMBF" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMBE","AAABDMBF","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLFA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLFN" varchar(4) NOT NULL default ' ',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABDLQQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLFN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLFH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLFN" varchar(4) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLFN","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLFP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMOI" varchar(6) NOT NULL default ' ',
  "AAABDMNN" varchar(6) NOT NULL default ' ',
  "AAABDLHZ" varchar(4) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMOI","AAABDMNN","AAABDLHZ","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLGA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNMU" varchar(10) NOT NULL default ' ',
  "AAABDLGD" varchar(10) NOT NULL default ' ',
  "AAABDLGE" varchar(250) NOT NULL default ' ',
  "AAABDLGF" varchar(250) NOT NULL default ' ',
  "AAABDLGG" varchar(250) NOT NULL default ' ',
  "AAABDLGH" varchar(250) NOT NULL default ' ',
  "AAABDLGI" varchar(250) NOT NULL default ' ',
  "AAABDLGJ" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLGM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNMU" varchar(10) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNMU","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLGU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLHO" varchar(10) NOT NULL default ' ',
  "AAABDLHZ" varchar(4) NOT NULL default ' ',
  "AAABDLGY" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLHO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLHB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLHO" varchar(10) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLHO","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLHJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLHO" varchar(10) NOT NULL default ' ',
  "AAABDLHP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLHO","AAABDLHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLHQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMOI" varchar(6) NOT NULL default ' ',
  "AAABDMNN" varchar(6) NOT NULL default ' ',
  "AAABDLHZ" varchar(4) NOT NULL default ' ',
  "AAABHSKS" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMOI","AAABDMNN","AAABDLHZ","AAABHSKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLIB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLIW" varchar(4) NOT NULL default ' ',
  "AAABDLIP" varchar(2) NOT NULL default '00',
  "AAABDLIF" varchar(30) NOT NULL default ' ',
  "AAABLEIH" varchar(10) NOT NULL default ' ',
  "AAABDLIH" varchar(2) NOT NULL default ' ',
  "AAABDLII" varchar(10) NOT NULL default ' ',
  "AAABDLIJ" varchar(20) NOT NULL default ' ',
  "AAABDLIK" varchar(20) NOT NULL default ' ',
  "AAABDLIL" int(11) NOT NULL default '0',
  "AAABDLIM" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABDLIW","AAABDLIP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLIQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLIW" varchar(4) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLIW","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLIY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMZH" varchar(6) NOT NULL default ' ',
  "AAABDNLD" varchar(6) NOT NULL default ' ',
  "AAABLCCC" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMZH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLJF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNLB" varchar(6) NOT NULL default ' ',
  "AAABDMZH" varchar(6) NOT NULL default ' ',
  "AAABDMCL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNLB","AAABDMZH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLJN" (
  "AAABDNLD" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNLD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLJQ" (
  "AAABDNLD" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNLD","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLJW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNLB" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNLB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLKB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNLB" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNLB","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLKJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMZH" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMZH","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLKR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLLK" varchar(6) NOT NULL default ' ',
  "AAABDLKU" varchar(2) NOT NULL default ' ',
  "AAABDLKV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLKY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLLK" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLLK","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLLG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLLK" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLLL" (
  "AAABDNLD" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNLD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLLO" (
  "AAABDNLD" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNLD","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLLU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNNS" varchar(6) NOT NULL default ' ',
  "AAABGNHH" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNNS","AAABGNHH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLMB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLMM" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLMG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLMM" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLMM","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLMO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLMZ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLMZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLMT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLMZ" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLMZ","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLNB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNNS" varchar(6) NOT NULL default ' ',
  "AAABDLNT" varchar(12) NOT NULL default ' ',
  "AAABDLNF" varchar(2) NOT NULL default ' ',
  "AAABDLNG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNNS","AAABDLNT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLNK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABDNNS" varchar(6) NOT NULL default ' ',
  "AAABDLNT" varchar(12) NOT NULL default ' ',
  "AAABDLNP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLFZK","AAABDNNS","AAABDLNT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLNU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLOF" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLOF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLNZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLOF" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLOF","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLOH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLOS" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLOM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLOS" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLOS","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNNS" varchar(6) NOT NULL default ' ',
  "AAABDNLD" varchar(6) NOT NULL default ' ',
  "AAABDLOY" varchar(1) NOT NULL default ' ',
  "AAABDMOU" varchar(2) NOT NULL default ' ',
  "AAABDLPA" varchar(1) NOT NULL default ' ',
  "AAABDNLB" varchar(6) NOT NULL default ' ',
  "AAABDNLA" varchar(6) NOT NULL default ' ',
  "AAABDNMU" varchar(10) NOT NULL default ' ',
  "AAABDNMT" varchar(10) NOT NULL default ' ',
  "AAABDLPF" varchar(2) NOT NULL default ' ',
  "AAABDNIC" varchar(2) NOT NULL default ' ',
  "AAABDNID" varchar(2) NOT NULL default ' ',
  "AAABDLRM" varchar(6) NOT NULL default ' ',
  "AAABDLPJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLPM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNNS" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNNS","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLPU" (
  "AAABDMUH" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABDMUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLPX" (
  "AAABDMUH" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABDMUH","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLQD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNNS" varchar(6) NOT NULL default ' ',
  "AAABDNLZ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNNS","AAABDNLZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLQK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABIMRZ" varchar(2) NOT NULL default ' ',
  "AAABMBXY" varchar(8) NOT NULL default '00000000',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABDLQQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLQS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLRM" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLRM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLQX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLRM" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLRM","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLRF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLRM" varchar(6) NOT NULL default ' ',
  "AAABDLRN" varchar(6) NOT NULL default ' ',
  "AAABEPQQ" varchar(4) NOT NULL default ' ',
  "AAABDNLB" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLRM","AAABDLRN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLRO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLRZ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLRT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLRZ" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLRZ","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLSB" (
  "AAABLUER" varchar(6) NOT NULL default ' ',
  "AAABDLSD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLSF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNLA" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNLA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLSK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNLA" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNLA","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLSS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPNX" varchar(6) NOT NULL default ' ',
  "AAABDLSV" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLSY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNLA" varchar(6) NOT NULL default ' ',
  "AAABLPNX" varchar(6) NOT NULL default ' ',
  "AAABDMCL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNLA","AAABLPNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLTG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPNX" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPNX","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLTO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNLZ" varchar(6) NOT NULL default ' ',
  "AAABDNNR" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNLZ","AAABDNNR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLUC" varchar(2) NOT NULL default ' ',
  "AAABDLUD" varchar(2) NOT NULL default ' ',
  "AAABDLUE" varchar(4) NOT NULL default ' ',
  "AAABDMZH" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLUC","AAABDLUD","AAABDLUE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLUF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGVMC" varchar(3) NOT NULL default '000',
  "AAABDNNR" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGVMC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLUL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMSB" varchar(10) NOT NULL default ' ',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMSB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLUR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAABDMSB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLUX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHRVE" varchar(12) NOT NULL default ' ',
  "AAABLUVE" tinyint(3) unsigned NOT NULL default '0',
  "AAABDLVB" varchar(3) NOT NULL default ' ',
  "AAABDLVC" varchar(3) NOT NULL default '000',
  "AAABDLVD" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABHRVE","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLVH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLVS" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLVM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLVS" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLVS","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLWH" varchar(30) NOT NULL default ' ',
  "AAABDLVX" varchar(10) NOT NULL default ' ',
  "AAABDLVY" varchar(30) NOT NULL default ' ',
  "AAABDLVZ" varchar(30) NOT NULL default ' ',
  "AAABDLWA" varchar(30) NOT NULL default ' ',
  "AAABDLWB" varchar(30) NOT NULL default ' ',
  "AAABDLWC" varchar(30) NOT NULL default ' ',
  "AAABDLWD" varchar(30) NOT NULL default ' ',
  "AAABDLWE" varchar(30) NOT NULL default ' ',
  "AAABDLWF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLWH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLWI" (
  "AAABDLXM" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABDLXM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLWL" (
  "AAABDLXM" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABDLXM","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLWR" (
  "AAABDLYC" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABDLYC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLWU" (
  "AAABDLYC" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABDLYC","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLXA" (
  "AAABDLYR" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABDLYR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLXD" (
  "AAABDLYR" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABDLYR","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLXJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLXX" varchar(6) NOT NULL default ' ',
  "AAABDLXM" varchar(6) NOT NULL default ' ',
  "AAABDLXN" varchar(2) NOT NULL default ' ',
  "AAABDNNS" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLXR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLXX" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLXX","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLXZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLYM" varchar(6) NOT NULL default ' ',
  "AAABDLYC" varchar(6) NOT NULL default ' ',
  "AAABDNNS" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLYM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLYG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLYM" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLYM","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLYO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLZA" varchar(6) NOT NULL default ' ',
  "AAABDLYR" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLZA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLYU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLZA" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLZA","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLZC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLZN" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLZN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLZH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLZN" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDLZN","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLZP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNLD" varchar(6) NOT NULL default ' ',
  "AAABDMAE" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNLD","AAABDMAE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDLZW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNLD" varchar(6) NOT NULL default ' ',
  "AAABDMAE" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNLD","AAABDMAE","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMAG" (
  "AAABDMBE" varchar(6) NOT NULL default ' ',
  "AAABDMAI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABDMBE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMAK" (
  "AAABDMBE" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABDMBE","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMAQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMBE" varchar(6) NOT NULL default ' ',
  "AAABDMBF" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMBE","AAABDMBF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMAX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMBE" varchar(6) NOT NULL default ' ',
  "AAABDMBF" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMBE","AAABDMBF","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMBH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMCN" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMCN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMBM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMCN" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMCN","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMBU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMCO" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMBZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMCO" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMCO","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMCH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMCN" varchar(6) NOT NULL default ' ',
  "AAABDMCO" varchar(6) NOT NULL default ' ',
  "AAABDMCL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMCN","AAABDMCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMCP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMCU" varchar(12) NOT NULL default ' ',
  "AAABDMOK" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMCV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMOI" varchar(6) NOT NULL default ' ',
  "AAABDNMT" varchar(10) NOT NULL default ' ',
  "AAABDMOK" varchar(6) NOT NULL default ' ',
  "AAABDMOL" varchar(6) NOT NULL default ' ',
  "AAABDMOM" varchar(10) NOT NULL default ' ',
  "AAABDNLD" varchar(2) NOT NULL default ' ',
  "AAABDNMU" varchar(10) NOT NULL default ' ',
  "AAABDMDE" varchar(2) NOT NULL default ' ',
  "AAABDMNF" varchar(10) NOT NULL default ' ',
  "AAABDMGB" varchar(2) NOT NULL default '00',
  "AAABDMDH" varchar(2) NOT NULL default '00',
  "AAABGYJW" varchar(1) NOT NULL default ' ',
  "AAABDMDJ" varchar(1) NOT NULL default ' ',
  "AAABDMDK" varchar(1) NOT NULL default ' ',
  "AAABDMDL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMOI","AAABDNMT","AAABDMOK","AAABDMOL","AAABDMOM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMDS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMOI" varchar(6) NOT NULL default ' ',
  "AAABDNMT" varchar(10) NOT NULL default ' ',
  "AAABDMOK" varchar(6) NOT NULL default ' ',
  "AAABDMOL" varchar(6) NOT NULL default ' ',
  "AAABDMOM" varchar(10) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  "AAABMECK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMOI","AAABDNMT","AAABDMOK","AAABDMOL","AAABDMOM","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMEJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMOK" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMOK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMEO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMOK" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMOK","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMEW" (
  "AAABDNLD" varchar(2) NOT NULL default ' ',
  "AAABDMEY" varchar(1) NOT NULL default ' ',
  "AAABDMEZ" varchar(1) NOT NULL default ' ',
  "AAABDMFA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNLD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMFC" (
  "AAABDNLD" varchar(2) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNLD","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMFI" (
  "AAABDNLD" varchar(2) NOT NULL default ' ',
  "AAABDMNW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNLD","AAABDMNW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMFN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMOI" varchar(6) NOT NULL default ' ',
  "AAABDNMT" varchar(10) NOT NULL default ' ',
  "AAABDMOK" varchar(6) NOT NULL default ' ',
  "AAABDMOL" varchar(6) NOT NULL default ' ',
  "AAABDMGB" varchar(2) NOT NULL default '00',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMECK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMOI","AAABDNMT","AAABDMOK","AAABDMOL","AAABDMGB","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMGD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMOI" varchar(6) NOT NULL default ' ',
  "AAABDNMT" varchar(10) NOT NULL default ' ',
  "AAABDMOK" varchar(6) NOT NULL default ' ',
  "AAABDMOL" varchar(6) NOT NULL default ' ',
  "AAABDMOM" varchar(10) NOT NULL default ' ',
  "AAABLFHC" varchar(20) NOT NULL default ' ',
  "AAABDMLS" varchar(3) NOT NULL default ' ',
  "AAABLNPY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMOI","AAABDNMT","AAABDMOK","AAABDMOL","AAABDMOM","AAABLFHC","AAABDMLS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMGV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMOI" varchar(6) NOT NULL default ' ',
  "AAABDNMT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMOI","AAABDNMT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMHC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMOI" varchar(6) NOT NULL default ' ',
  "AAABDNMT" varchar(10) NOT NULL default ' ',
  "AAABDMOK" varchar(6) NOT NULL default ' ',
  "AAABDMOL" varchar(6) NOT NULL default ' ',
  "AAABDMHI" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMOI","AAABDNMT","AAABDMOK","AAABDMOL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMHO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMOI" varchar(6) NOT NULL default ' ',
  "AAABDNMT" varchar(10) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMOI","AAABDNMT","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMHY" (
  "AAABDMOI" varchar(6) NOT NULL default ' ',
  "AAABDMJC" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABDMOI","AAABDMJC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMID" (
  "AAABDMOI" varchar(6) NOT NULL default ' ',
  "AAABDMJC" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABDMOI","AAABDMJC","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMIL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMOI" varchar(6) NOT NULL default ' ',
  "AAABDNMT" varchar(10) NOT NULL default ' ',
  "AAABDMOL" varchar(6) NOT NULL default ' ',
  "AAABDMOK" varchar(6) NOT NULL default ' ',
  "AAABDMOM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMOI","AAABDNMT","AAABDMOL","AAABDMOK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMIX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMOI" varchar(6) NOT NULL default ' ',
  "AAABDNMT" varchar(10) NOT NULL default ' ',
  "AAABDMOL" varchar(6) NOT NULL default ' ',
  "AAABDMJC" varchar(6) NOT NULL default ' ',
  "AAABDNMU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMOI","AAABDNMT","AAABDMOL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMJI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMOI" varchar(6) NOT NULL default ' ',
  "AAABDNMT" varchar(10) NOT NULL default ' ',
  "AAABDMOL" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  "AAABMECK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMOI","AAABDNMT","AAABDMOL","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMJV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMOI" varchar(6) NOT NULL default ' ',
  "AAABDNMT" varchar(10) NOT NULL default ' ',
  "AAABDMOK" varchar(6) NOT NULL default ' ',
  "AAABDMOL" varchar(6) NOT NULL default ' ',
  "AAABDMOM" varchar(10) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMOI","AAABDNMT","AAABDMOK","AAABDMOL","AAABDMOM","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMKK" (
  "AAABDMOI" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABDMOI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMKN" (
  "AAABDMOI" varchar(6) NOT NULL default ' ',
  "AAABDMNW" varchar(1) NOT NULL default ' ',
  "AAABDMTZ" varchar(10) NOT NULL default ' ',
  "AAABDMKR" varchar(3) NOT NULL default ' ',
  "AAABDMKS" varchar(4) NOT NULL default ' ',
  "AAABDMKT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDMOI","AAABDMNW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMKW" (
  "AAABDMOI" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABDMOI","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMLC" (
  "AAABDMOI" varchar(6) NOT NULL default ' ',
  "AAABDMNN" varchar(6) NOT NULL default ' ',
  "AAABDMLF" varchar(1) NOT NULL default ' ',
  "AAABDMME" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABDMOI","AAABDMNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMLJ" (
  "AAABDMOI" varchar(6) NOT NULL default ' ',
  "AAABDMNN" varchar(6) NOT NULL default ' ',
  "AAABLFHC" varchar(20) NOT NULL default ' ',
  "AAABDMLS" varchar(3) NOT NULL default ' ',
  "AAABLNPY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDMOI","AAABDMNN","AAABLFHC","AAABDMLS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMLT" (
  "AAABDMOI" varchar(6) NOT NULL default ' ',
  "AAABDMME" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABDMOI","AAABDMME")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMLY" (
  "AAABDMOI" varchar(6) NOT NULL default ' ',
  "AAABDMME" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABDMOI","AAABDMME","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMMG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMOI" varchar(6) NOT NULL default ' ',
  "AAABDMNF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMOI","AAABDMNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMMN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMOI" varchar(6) NOT NULL default ' ',
  "AAABDMNF" varchar(10) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMOI","AAABDMNF","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMMX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMOI" varchar(6) NOT NULL default ' ',
  "AAABDMNF" varchar(10) NOT NULL default ' ',
  "AAABGXLE" varchar(4) NOT NULL default '0000',
  "AAABDMNC" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMOI","AAABDMNF","AAABGXLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMNH" (
  "AAABDMOI" varchar(6) NOT NULL default ' ',
  "AAABDMNN" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABDMOI","AAABDMNN","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMNP" (
  "AAABDMOI" varchar(6) NOT NULL default ' ',
  "AAABDNLD" varchar(2) NOT NULL default ' ',
  "AAABDMNW" varchar(1) NOT NULL default ' ',
  "AAABDMNT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABDMOI","AAABDNLD","AAABDMNW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMNX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMOI" varchar(6) NOT NULL default ' ',
  "AAABDNMT" varchar(10) NOT NULL default ' ',
  "AAABDMOK" varchar(6) NOT NULL default ' ',
  "AAABDMOL" varchar(6) NOT NULL default ' ',
  "AAABDMOM" varchar(10) NOT NULL default ' ',
  "AAABDMON" varchar(5) NOT NULL default ' ',
  "AAABDMOF" varchar(2) NOT NULL default ' ',
  "AAABDMOG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMOI","AAABDNMT","AAABDMOK","AAABDMOL","AAABDMOM","AAABDMON")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMOO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNNR" varchar(6) NOT NULL default ' ',
  "AAABDNLD" varchar(6) NOT NULL default ' ',
  "AAABDMOS" decimal(10,6) NOT NULL default '0.000000',
  "AAABDMSV" varchar(3) NOT NULL default ' ',
  "AAABDMOU" varchar(2) NOT NULL default ' ',
  "AAABDMST" varchar(2) NOT NULL default ' ',
  "AAABDMOW" varchar(1) NOT NULL default ' ',
  "AAABDNMU" varchar(10) NOT NULL default ' ',
  "AAABDNLB" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNNR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMPB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNNS" varchar(6) NOT NULL default ' ',
  "AAABDNNR" varchar(6) NOT NULL default ' ',
  "AAABDMPF" decimal(10,6) NOT NULL default '0.000000',
  PRIMARY KEY ("AAABMRIL","AAABDNNS","AAABDNNR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMPJ" (
  "AAABDNLD" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNLD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMPM" (
  "AAABDNLD" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNLD","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMPS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNNR" varchar(6) NOT NULL default ' ',
  "AAABDMPV" varchar(250) NOT NULL default ' ',
  "AAABDMPW" varchar(250) NOT NULL default ' ',
  "AAABDMPX" varchar(250) NOT NULL default ' ',
  "AAABDMPY" varchar(250) NOT NULL default ' ',
  "AAABDMPZ" varchar(250) NOT NULL default ' ',
  "AAABDMQA" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNNR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMQD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMQX" varchar(6) NOT NULL default ' ',
  "AAABLUVE" tinyint(3) unsigned NOT NULL default '0',
  "AAABDNNR" varchar(6) NOT NULL default ' ',
  "AAABGUEY" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMQX","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMQM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMQX" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMQX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMQR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMQX" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMQX","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMQZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNNR" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNNR","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMRH" (
  "AAABDMRO" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABDMRO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMRK" (
  "AAABDMRO" varchar(3) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABDMRO","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMRQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMSB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMSB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMRV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMSB" varchar(10) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMSB","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMSD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGUEY" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGUEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMSI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGUEY" varchar(12) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGUEY","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMSV" varchar(3) NOT NULL default ' ',
  "AAABDMST" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMSW" (
  "AAABDNLD" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNLD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMSZ" (
  "AAABDNLD" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNLD","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMTF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNHH" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGNHH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMTK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNHH" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGNHH","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMTS" (
  "AAABDMTZ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABDMTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMTV" (
  "AAABDMTZ" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABDMTZ","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMUB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIFHG" varchar(6) NOT NULL default ' ',
  "AAABDNLD" varchar(6) NOT NULL default ' ',
  "AAABDMUF" varchar(2) NOT NULL default ' ',
  "AAABDMUG" varchar(2) NOT NULL default ' ',
  "AAABDMUH" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIFHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMUK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMUQ" varchar(6) NOT NULL default ' ',
  "AAABIFHG" varchar(6) NOT NULL default ' ',
  "AAABDMUS" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMUQ","AAABIFHG","AAABDMUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMUT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIFHG" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  "AAABDMUY" varchar(40) NOT NULL default ' ',
  "AAABDMUZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIFHG","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMVD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDSAH" varchar(6) NOT NULL default ' ',
  "AAABIMRZ" varchar(6) NOT NULL default ' ',
  "AAABDNDC" varchar(6) NOT NULL default ' ',
  "AAABDNLV" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDSAH","AAABIMRZ","AAABDNDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMVN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNDC" varchar(6) NOT NULL default ' ',
  "AAABDNJJ" varchar(1) NOT NULL default ' ',
  "AAABDNJK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNDC" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNDC","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMWC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDSAH" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDSAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMWH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDSAH" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDSAH","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMWP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMXB" varchar(6) NOT NULL default ' ',
  "AAABDMWS" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMWV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMXB" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMXB","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMXD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMXO" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMXI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMXO" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMXO","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMXQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMYC" varchar(6) NOT NULL default ' ',
  "AAABDMYL" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMYC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMXW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMYC" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMYC","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMYE" (
  "AAABDMYL" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABDMYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMYH" (
  "AAABDMYL" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABDMYL","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMYN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMYY" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMYY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMYS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMYY" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMYY","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDSAH" varchar(6) NOT NULL default ' ',
  "AAABDMZH" varchar(6) NOT NULL default ' ',
  "AAABDMZE" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDSAH","AAABDMZH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMZI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDSAH" varchar(6) NOT NULL default ' ',
  "AAABIWYL" varchar(6) NOT NULL default ' ',
  "AAABDMZM" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDSAH","AAABIWYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMZQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDSAH" varchar(6) NOT NULL default ' ',
  "AAABIMRZ" varchar(6) NOT NULL default ' ',
  "AAABDMZU" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDSAH","AAABIMRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDMZY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNAJ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNAJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNAD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNAJ" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNAJ","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNAL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNAW" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNAQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNAW" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNAW","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNAY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNBJ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNBJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNBD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNBJ" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNBJ","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNBL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNBW" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNBW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNBQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNBW" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNBW","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNBY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNCJ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNCJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNCD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNCJ" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNCJ","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNCL" (
  "AAABDNCS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNCO" (
  "AAABDNCS" varchar(30) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNCS","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNCU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRZ" varchar(6) NOT NULL default ' ',
  "AAABDNDC" varchar(6) NOT NULL default ' ',
  "AAABDNDH" varchar(1) NOT NULL default ' ',
  "AAABDNMU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMRZ","AAABDNDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNDD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRZ" varchar(6) NOT NULL default ' ',
  "AAABDNEI" varchar(6) NOT NULL default ' ',
  "AAABDNDH" varchar(1) NOT NULL default ' ',
  "AAABDNMU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMRZ","AAABDNEI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNDM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDSAH" varchar(6) NOT NULL default ' ',
  "AAABIMRZ" varchar(6) NOT NULL default ' ',
  "AAABDNDV" varchar(6) NOT NULL default ' ',
  "AAABDNLV" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDSAH","AAABIMRZ","AAABDNDV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNDW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNEI" varchar(6) NOT NULL default ' ',
  "AAABDNER" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNEI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNEC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNEI" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNEI","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNEK" (
  "AAABDNER" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNEN" (
  "AAABDNER" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNER","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNET" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNFE" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNFE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNEY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNFE" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNFE","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNFG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNFS" varchar(6) NOT NULL default ' ',
  "AAABDNGO" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNFS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNFM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNFS" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNFS","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNFU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNGF" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNGF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNFZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNGF" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNGF","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNGH" (
  "AAABDNGO" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNGO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNGK" (
  "AAABDNGO" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNGO","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNGQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNGX" varchar(20) NOT NULL default ' ',
  "AAABDNGT" varchar(6) NOT NULL default ' ',
  "AAABIYCS" varchar(2) NOT NULL default ' ',
  "AAABIMRZ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNGX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNGY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNHJ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNHJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNHD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNHJ" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNHJ","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNHL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRZ" varchar(6) NOT NULL default ' ',
  "AAABDNHO" varchar(6) NOT NULL default ' ',
  "AAABDNHP" varchar(1) NOT NULL default ' ',
  "AAABDNMU" varchar(10) NOT NULL default ' ',
  "AAABDNMT" varchar(10) NOT NULL default ' ',
  "AAABDNLF" varchar(2) NOT NULL default ' ',
  "AAABDNLG" varchar(4) NOT NULL default '0000',
  "AAABDNLH" decimal(10,6) NOT NULL default '0.000000',
  "AAABDNLB" varchar(6) NOT NULL default ' ',
  "AAABDNLA" varchar(6) NOT NULL default ' ',
  "AAABDNLI" decimal(10,6) NOT NULL default '0.000000',
  "AAABDNLJ" decimal(10,6) NOT NULL default '0.000000',
  "AAABDNLK" decimal(10,6) NOT NULL default '0.000000',
  "AAABDNLL" decimal(10,6) NOT NULL default '0.000000',
  "AAABDNLM" decimal(10,6) NOT NULL default '0.000000',
  "AAABDNIC" varchar(2) NOT NULL default ' ',
  "AAABDNID" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNIG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRZ" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMRZ","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNIO" (
  "AAABDNIV" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNIR" (
  "AAABDNIV" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNIV","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNIX" (
  "AAABDNJE" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNJE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNJA" (
  "AAABDNJE" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNJE","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNJG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMJJ" varchar(6) NOT NULL default ' ',
  "AAABDNJJ" varchar(1) NOT NULL default ' ',
  "AAABDNJK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMJJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMJJ" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMJJ","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNJV" (
  "AAABDNKJ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNKJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNJY" (
  "AAABDNKJ" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNKJ","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNKE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNKJ" varchar(6) NOT NULL default ' ',
  "AAABDNKH" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNKJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNKK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNKV" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNKV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNKP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNKV" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNKV","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNKX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNLV" varchar(6) NOT NULL default ' ',
  "AAABDNLA" varchar(6) NOT NULL default ' ',
  "AAABDNLB" varchar(6) NOT NULL default ' ',
  "AAABDNLZ" varchar(6) NOT NULL default ' ',
  "AAABDNLD" varchar(6) NOT NULL default ' ',
  "AAABDNLE" varchar(2) NOT NULL default ' ',
  "AAABDNLF" varchar(2) NOT NULL default ' ',
  "AAABDNLG" varchar(4) NOT NULL default '0000',
  "AAABDNLH" decimal(10,6) NOT NULL default '0.000000',
  "AAABDNLI" decimal(10,6) NOT NULL default '0.000000',
  "AAABDNLJ" decimal(10,6) NOT NULL default '0.000000',
  "AAABDNLK" decimal(10,6) NOT NULL default '0.000000',
  "AAABDNLL" decimal(10,6) NOT NULL default '0.000000',
  "AAABDNLM" decimal(10,6) NOT NULL default '0.000000',
  PRIMARY KEY ("AAABMRIL","AAABDNLV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNLP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNLV" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNLV","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNLX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNLZ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNMB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNMN" varchar(10) NOT NULL default ' ',
  "AAABDNME" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNMN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNMH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNMN" varchar(10) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNMN","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNMP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNND" varchar(6) NOT NULL default ' ',
  "AAABEVSW" varchar(6) NOT NULL default ' ',
  "AAABDNMT" varchar(10) NOT NULL default ' ',
  "AAABDNMU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNMX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNND" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABDNNB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNND","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNNF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNNK" varchar(30) NOT NULL default ' ',
  "AAABDNNI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNNK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNNL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNNV" varchar(20) NOT NULL default ' ',
  "AAABLMLR" varchar(6) NOT NULL default ' ',
  "AAABIYCS" varchar(2) NOT NULL default ' ',
  "AAABEVSW" varchar(6) NOT NULL default ' ',
  "AAABDNNR" varchar(6) NOT NULL default ' ',
  "AAABDNNS" varchar(6) NOT NULL default ' ',
  "AAABDNNT" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNNV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNNW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNOH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNOH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNOH" varchar(2) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNOH","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNOJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNOU" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNOU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNOO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNOU" varchar(2) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNOU","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNOW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNPH" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNPH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNPB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNPH" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNPH","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNPJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGDGH" varchar(10) NOT NULL default ' ',
  "AAABDNPM" varchar(32) NOT NULL default ' ',
  "AAABDNRJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGDGH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNPQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGDGH" varchar(10) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGDGH","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNPY" (
  "AAABDNQK" varchar(60) NOT NULL default ' ',
  "AAABDNQV" varchar(60) NOT NULL default ' ',
  "AAABDNQB" varchar(60) NOT NULL default ' ',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  "AAABIJQL" varchar(60) NOT NULL default ' ',
  "AAABDNQE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNQK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNQG" (
  "AAABDNQK" varchar(60) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNQK","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNQM" (
  "AAABDNQV" varchar(60) NOT NULL default ' ',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  "AAABDNQP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNQV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNQR" (
  "AAABDNQV" varchar(60) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNQV","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNQX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNRJ" varchar(10) NOT NULL default ' ',
  "AAABDNRA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNRD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNRJ" varchar(10) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNRJ","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNRL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIMRU" varchar(40) NOT NULL default ' ',
  "AAABJMEZ" varchar(40) NOT NULL default ' ',
  "AAABIMRW" varchar(5) NOT NULL default ' ',
  "AAABIMRV" varchar(30) NOT NULL default ' ',
  "AAABDNTA" VARBINARY(255),
  "AAABDOFZ" decimal(15,0) NOT NULL default '0',
  "AAABDNRT" VARBINARY(255),
  "AAABDNRU" VARBINARY(255),
  "AAABDNRV" decimal(15,0) NOT NULL default '0',
  "AAABDNRW" decimal(15,0) NOT NULL default '0',
  "AAABDNRX" varchar(12) NOT NULL default ' ',
  "AAABDNRY" varchar(12) NOT NULL default ' ',
  "AAABDNRZ" varchar(6) NOT NULL default ' ',
  "AAABDNSA" varchar(6) NOT NULL default ' ',
  "AAABDNTD" varchar(40) NOT NULL default ' ',
  "AAABDNSC" varchar(10) NOT NULL default '0000000000',
  "AAABDNSD" varchar(10) NOT NULL default ' ',
  "AAABDNSE" varchar(8) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABDNSG" varchar(22) NOT NULL default ' ',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABDNSI" varchar(1) NOT NULL default ' ',
  "AAABDOES" varchar(40) NOT NULL default ' ',
  "AAABDOFL" varchar(40) NOT NULL default ' ',
  "AAABDOFM" varchar(15) NOT NULL default ' ',
  "AAABDNSM" varchar(1) NOT NULL default ' ',
  "AAABDNSN" varchar(10) NOT NULL default ' ',
  "AAABDNSO" varchar(12) NOT NULL default ' ',
  "AAABDNSP" decimal(15,0) NOT NULL default '0',
  "AAABDNSQ" varchar(6) NOT NULL default ' ',
  "AAABDNVR" decimal(15,0) NOT NULL default '0',
  "AAABDNSS" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABIMRU","AAABJMEZ","AAABIMRW","AAABIMRV"),
  UNIQUE KEY "AAABDNSY" ("AAABLUXQ","AAABDNTA"(255)),
  KEY "AAABDNTB" ("AAABLUXQ","AAABDNTD"),
  KEY "AAABDNTE" ("AAABLUXQ","AAABLXTG"),
  KEY "AAABDNTH" ("AAABLUXQ","AAABDOFL","AAABDOFM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNTL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIMRU" varchar(40) NOT NULL default ' ',
  "AAABJMEZ" varchar(40) NOT NULL default ' ',
  "AAABIMRW" varchar(5) NOT NULL default ' ',
  "AAABIMRV" varchar(30) NOT NULL default ' ',
  "AAABDOHF" varchar(1) NOT NULL default ' ',
  "AAABDNTS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIMRU","AAABJMEZ","AAABIMRW","AAABIMRV","AAABDOHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNTZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABDNYV" varchar(32) NOT NULL default ' ',
  "AAABDNUL" varchar(32) NOT NULL default ' ',
  "AAABDVPT" varchar(1) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABDNUF" varchar(1) NOT NULL default ' ',
  "AAABDNUG" varchar(1) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABDNYT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABDNYV","AAABDNUL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNUM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTKC" varchar(3) NOT NULL default ' ',
  "AAABDNUP" varchar(1) NOT NULL default ' ',
  "AAABDNUQ" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLTKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNUT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTKC" varchar(3) NOT NULL default ' ',
  "AAABDNUW" varchar(8) NOT NULL default ' ',
  "AAABDNUX" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABLTKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNVA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTKC" varchar(3) NOT NULL default ' ',
  "AAABDNVD" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNVG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTKC" varchar(3) NOT NULL default ' ',
  "AAABDNVJ" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNVM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABDNZC" varchar(1) NOT NULL default ' ',
  "AAABDNZA" varchar(1) NOT NULL default ' ',
  "AAABDNVR" varchar(8) NOT NULL default '00000000',
  "AAABDNZD" varchar(4) NOT NULL default ' ',
  "AAABDNZB" varchar(1) NOT NULL default ' ',
  "AAABDNZE" varchar(1) NOT NULL default ' ',
  "AAABDPJQ" varchar(1) NOT NULL default ' ',
  "AAABDPJR" varchar(1) NOT NULL default ' ',
  "AAABDPJS" varchar(1) NOT NULL default ' ',
  "AAABDPJT" varchar(1) NOT NULL default ' ',
  "AAABDNZJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNWC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABLFYC" varchar(4) NOT NULL default ' ',
  "AAABDNZV" varchar(10) NOT NULL default ' ',
  "AAABDNZW" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABLFYC","AAABDNZV","AAABDNZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNWN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABLFYC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABLFYC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNWU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNXC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABLSOG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNXJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABLTUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNXQ" (
  "AAABDNXV" varchar(10) NOT NULL default ' ',
  "AAABDNXS" varchar(4) NOT NULL default ' ',
  "AAABDNXT" varchar(4) NOT NULL default ' ',
  "AAABDNXU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNXW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTKC" varchar(3) NOT NULL default ' ',
  "AAABDNXZ" int(11) NOT NULL default '0',
  "AAABDNYA" int(11) NOT NULL default '0',
  "AAABDNYB" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLTKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNYE" (
  "AAABIYCS" varchar(8) NOT NULL default ' ',
  "AAABDNYG" varchar(1) NOT NULL default ' ',
  "AAABDNYH" varchar(1) NOT NULL default ' ',
  "AAABMJWH" varchar(1) NOT NULL default ' ',
  "AAABDNYJ" varchar(1) NOT NULL default ' ',
  "AAABIJJA" varchar(1) NOT NULL default ' ',
  "AAABDNYL" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABIYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNYN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABDNYV" varchar(32) NOT NULL default ' ',
  "AAABDNYW" varchar(12) NOT NULL default ' ',
  "AAABDNYR" varchar(5) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default '00',
  "AAABDNYT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABDNYV","AAABDNYW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNYX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAABDNZA" varchar(1) NOT NULL default ' ',
  "AAABDNZB" varchar(1) NOT NULL default ' ',
  "AAABDNZC" varchar(1) NOT NULL default ' ',
  "AAABDNZD" varchar(4) NOT NULL default ' ',
  "AAABDNZE" varchar(1) NOT NULL default ' ',
  "AAABDPJQ" varchar(1) NOT NULL default ' ',
  "AAABDPJR" varchar(1) NOT NULL default ' ',
  "AAABDPJS" varchar(1) NOT NULL default ' ',
  "AAABDPJT" varchar(1) NOT NULL default ' ',
  "AAABDNZJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTVR"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNZM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAABLFYC" varchar(4) NOT NULL default ' ',
  "AAABDNZV" varchar(10) NOT NULL default ' ',
  "AAABDNZW" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTVR"(255),"AAABLFYC","AAABDNZV","AAABDNZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDNZX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAABLFYC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTVR"(255),"AAABLFYC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOAE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTVR"(255),"AAABLSOG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOAL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTVR"(255),"AAABLTUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOAS" (
  "AAABLTVR" varchar(128) NOT NULL default ' ',
  "AAABMOHM" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOAW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIMRW" varchar(5) NOT NULL default ' ',
  "AAABDOAZ" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIMRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOBC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDOBI" varchar(1) NOT NULL default ' ',
  "AAABIMRW" varchar(5) NOT NULL default ' ',
  "AAABDOBG" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDOBI","AAABIMRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOBK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABDOGF" varchar(5) NOT NULL default ' ',
  "AAABDOBT" varchar(5) NOT NULL default ' ',
  "AAABDOBU" varchar(5) NOT NULL default ' ',
  "AAABDOBP" varchar(1) NOT NULL default ' ',
  "AAABDOBQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABDOGF","AAABDOBT","AAABDOBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOBV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIMRW" varchar(5) NOT NULL default ' ',
  "AAABKQVZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIMRW","AAABKQVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOCC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABDOGF" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABDOGF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOCH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDOCN" varchar(1) NOT NULL default ' ',
  "AAABDOGF" varchar(5) NOT NULL default ' ',
  "AAABDOCL" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDOCN","AAABDOGF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOCP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIMRU" varchar(40) NOT NULL default ' ',
  "AAABJMEZ" varchar(40) NOT NULL default ' ',
  "AAABIMRW" varchar(5) NOT NULL default ' ',
  "AAABDODL" VARBINARY(255),
  "AAABDOCV" VARBINARY(255),
  "AAABDOGU" VARBINARY(255),
  "AAABDODO" varchar(40) NOT NULL default ' ',
  "AAABDOCY" varchar(10) NOT NULL default ' ',
  "AAABDOCZ" varchar(12) NOT NULL default ' ',
  "AAABDODA" decimal(15,0) NOT NULL default '0',
  "AAABDODB" varchar(6) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABDODD" varchar(22) NOT NULL default ' ',
  "AAABDODE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIMRU","AAABJMEZ","AAABIMRW"),
  UNIQUE KEY "AAABDODJ" ("AAABLUXQ","AAABDODL"(255)),
  KEY "AAABDODM" ("AAABLUXQ","AAABDODO"),
  KEY "AAABDODP" ("AAABLUXQ","AAABLXTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDODS" (
  "AAABDODZ" varchar(3) NOT NULL default '000',
  "AAABDOEA" varchar(22) NOT NULL default ' ',
  "AAABDOED" varchar(22) NOT NULL default ' ',
  "AAABJJLO" varchar(22) NOT NULL default ' ',
  "AAABJJLF" varchar(4) NOT NULL default ' ',
  "AAABDODY" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABDODZ","AAABDOEA","AAABDOED"),
  KEY "AAABDOEC" ("AAABDOED"),
  KEY "AAABDOEE" ("AAABJJLO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOEG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIMRU" varchar(40) NOT NULL default ' ',
  "AAABJMEZ" varchar(40) NOT NULL default ' ',
  "AAABIMRW" varchar(5) NOT NULL default ' ',
  "AAABDOFZ" decimal(15,0) NOT NULL default '0',
  "AAABDOFI" VARBINARY(255),
  "AAABDOEN" varchar(10) NOT NULL default '0000000000',
  "AAABLSBE" varchar(1) NOT NULL default ' ',
  "AAABDOEP" varchar(12) NOT NULL default ' ',
  "AAABDOEQ" decimal(15,0) NOT NULL default '0',
  "AAABDOER" varchar(6) NOT NULL default ' ',
  "AAABDOES" varchar(40) NOT NULL default ' ',
  "AAABDOFL" varchar(40) NOT NULL default ' ',
  "AAABDOFM" varchar(15) NOT NULL default ' ',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABDOEW" varchar(1) NOT NULL default ' ',
  "AAABDOEX" varchar(10) NOT NULL default ' ',
  "AAABDOEY" varchar(12) NOT NULL default ' ',
  "AAABDOEZ" varchar(24) NOT NULL default ' ',
  "AAABDOFA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIMRU","AAABJMEZ","AAABIMRW","AAABDOFZ"),
  UNIQUE KEY "AAABDOFG" ("AAABLUXQ","AAABDOFI"(255)),
  KEY "AAABDOFJ" ("AAABLUXQ","AAABDOFL","AAABDOFM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOFN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIMRU" varchar(40) NOT NULL default ' ',
  "AAABJMEZ" varchar(40) NOT NULL default ' ',
  "AAABIMRW" varchar(5) NOT NULL default ' ',
  "AAABDOFZ" decimal(15,0) NOT NULL default '0',
  "AAABDOHF" varchar(1) NOT NULL default ' ',
  "AAABDOFU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIMRU","AAABJMEZ","AAABIMRW","AAABDOFZ","AAABDOHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOGB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJMEZ" varchar(40) NOT NULL default ' ',
  "AAABDOGU" VARBINARY(255),
  "AAABDOGF" varchar(5) NOT NULL default ' ',
  "AAABDOGG" varchar(12) NOT NULL default ' ',
  "AAABDOGH" decimal(15,0) NOT NULL default '0',
  "AAABDOGI" varchar(6) NOT NULL default ' ',
  "AAABDOGJ" varchar(10) NOT NULL default '0000000000',
  "AAABDOGK" varchar(12) NOT NULL default ' ',
  "AAABDOGL" decimal(15,0) NOT NULL default '0',
  "AAABDOGM" varchar(6) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABDOGO" varchar(22) NOT NULL default ' ',
  "AAABDOGP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJMEZ"),
  UNIQUE KEY "AAABDOGS" ("AAABLUXQ","AAABDOGU"(255)),
  KEY "AAABDOGV" ("AAABLUXQ","AAABLXTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOGY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJMEZ" varchar(40) NOT NULL default ' ',
  "AAABDOHF" varchar(1) NOT NULL default ' ',
  "AAABDOHC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJMEZ","AAABDOHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOHG" (
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABDOHS" varchar(8) NOT NULL default ' ',
  "AAABDOHT" varchar(8) NOT NULL default ' ',
  "AAABMECK" varchar(20) NOT NULL default ' ',
  "AAABLTXO" varchar(30) NOT NULL default ' ',
  "AAABIWUV" varchar(3) NOT NULL default '000',
  "AAABIWUW" varchar(3) NOT NULL default '000',
  "AAABIWUX" varchar(3) NOT NULL default '000',
  "AAABIWUY" varchar(3) NOT NULL default '000',
  "AAABIWUZ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLUVS","AAABDOHS","AAABDOHT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOHU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABDOHX" varchar(1) NOT NULL default '0',
  "AAABDOHY" varchar(1) NOT NULL default '0',
  "AAABDOHZ" varchar(1) NOT NULL default '0',
  "AAABDOIA" varchar(1) NOT NULL default '0',
  "AAABDOIB" varchar(1) NOT NULL default '0',
  "AAABDOIC" varchar(1) NOT NULL default '0',
  "AAABDOID" varchar(1) NOT NULL default '0',
  "AAABDOIE" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOIH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABIMRU" varchar(40) NOT NULL default ' ',
  "AAABJMEZ" varchar(40) NOT NULL default ' ',
  "AAABIMRW" varchar(5) NOT NULL default ' ',
  "AAABIMRV" varchar(30) NOT NULL default ' ',
  "AAABDOIO" decimal(15,0) NOT NULL default '0',
  "AAABDOIP" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMDEN","AAABIMRU","AAABJMEZ","AAABIMRW","AAABIMRV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCNC" varchar(7) NOT NULL default ' ',
  "AAABDOIZ" varchar(6) NOT NULL default ' ',
  "AAABDOJA" varchar(6) NOT NULL default ' ',
  "AAABDOJB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOJE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLCNC" varchar(7) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  "AAABDOJJ" varchar(15) NOT NULL default ' ',
  "AAABDOJK" varchar(15) NOT NULL default ' ',
  "AAABDOJL" varchar(15) NOT NULL default ' ',
  "AAABDOJM" varchar(15) NOT NULL default ' ',
  "AAABDOJN" varchar(15) NOT NULL default ' ',
  "AAABDOJO" varchar(15) NOT NULL default ' ',
  "AAABDOJP" varchar(15) NOT NULL default ' ',
  "AAABDOJQ" varchar(15) NOT NULL default ' ',
  "AAABDOJR" varchar(15) NOT NULL default ' ',
  "AAABDOJS" varchar(15) NOT NULL default ' ',
  "AAABDOJT" varchar(15) NOT NULL default ' ',
  "AAABDOJU" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLCNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOJY" (
  "AAABDOLQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDOLQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOKB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDOKP" varchar(2) NOT NULL default ' ',
  "AAABDOKE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDOKP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOKH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDOKP" varchar(2) NOT NULL default ' ',
  "AAABHIHD" varchar(8) NOT NULL default ' ',
  "AAABHIHE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDOKP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOKQ" (
  "AAABKQPE" varchar(2) NOT NULL default '00',
  "AAABJFHC" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABKQPE","AAABJFHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOKV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKOGD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKOGD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOLA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHQRM" varchar(2) NOT NULL default ' ',
  "AAABDOLD" varchar(1) NOT NULL default ' ',
  "AAABDOLE" varchar(1) NOT NULL default ' ',
  "AAABDOLF" varchar(1) NOT NULL default ' ',
  "AAABDOLG" varchar(1) NOT NULL default ' ',
  "AAABDOLH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHQRM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOLK" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDOLQ" varchar(1) NOT NULL default ' ',
  "AAABDOLN" varchar(20) NOT NULL default ' ',
  "AAABDOLO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABDOLQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOLR" (
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
  "AAABDOMD" varchar(10) NOT NULL default ' ',
  "AAABDOME" varchar(4) NOT NULL default ' ',
  "AAABKQLH" varchar(3) NOT NULL default ' ',
  "AAABKQJS" varchar(3) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLCNC" varchar(7) NOT NULL default ' ',
  "AAABDOMJ" varchar(2) NOT NULL default ' ',
  "AAABKOEW" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABGJBT" varchar(1) NOT NULL default ' ',
  "AAABDOMN" varchar(4) NOT NULL default ' ',
  "AAABKQMO" varchar(1) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLURO" varchar(3) NOT NULL default ' ',
  "AAABDOMS" varchar(6) NOT NULL default ' ',
  "AAABDOMT" varchar(6) NOT NULL default ' ',
  "AAABDOMU" varchar(6) NOT NULL default ' ',
  "AAABDOMV" varchar(6) NOT NULL default ' ',
  "AAABDOMW" varchar(6) NOT NULL default ' ',
  "AAABDOMX" varchar(6) NOT NULL default ' ',
  "AAABGJBV" varchar(4) NOT NULL default ' ',
  "AAABGJBW" varchar(12) NOT NULL default ' ',
  "AAABKNSO" varchar(3) NOT NULL default ' ',
  "AAABDONB" varchar(2) NOT NULL default '00',
  "AAABDVOC" varchar(3) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABGJBU" varchar(1) NOT NULL default ' ',
  "AAABDTRQ" varchar(1) NOT NULL default ' ',
  "AAABKRSH" varchar(1) NOT NULL default ' ',
  "AAABDTRX" varchar(11) NOT NULL default ' ',
  "AAABDTRY" varchar(11) NOT NULL default ' ',
  "AAABDTRV" varchar(11) NOT NULL default ' ',
  "AAABDTRW" varchar(11) NOT NULL default ' ',
  "AAABDTRZ" varchar(11) NOT NULL default ' ',
  "AAABDONM" varchar(7) NOT NULL default ' ',
  "AAABKRHQ" varchar(1) NOT NULL default ' ',
  "AAABLDON" varchar(10) NOT NULL default ' ',
  "AAABDONP" varchar(12) NOT NULL default ' ',
  "AAABKNVT" varchar(1) NOT NULL default ' ',
  "AAABKNVU" varchar(1) NOT NULL default ' ',
  "AAABDQTT" varchar(12) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABDONU" varchar(1) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAABDONW" varchar(9) NOT NULL default ' ',
  "AAABDONX" varchar(3) NOT NULL default ' ',
  "AAABDONY" varchar(4) NOT NULL default '0000',
  "AAABKOHS" varchar(1) NOT NULL default ' ',
  "AAABDOOA" varchar(1) NOT NULL default ' ',
  "AAABDOOB" varchar(2) NOT NULL default ' ',
  "AAABDOOC" varchar(2) NOT NULL default ' ',
  "AAABDOOD" varchar(2) NOT NULL default ' ',
  "AAABKRYF" varchar(3) NOT NULL default ' ',
  "AAABDOOF" varchar(3) NOT NULL default ' ',
  "AAABDOOG" varchar(3) NOT NULL default ' ',
  "AAABDOOH" varchar(10) NOT NULL default ' ',
  "AAABKRDF" varchar(1) NOT NULL default ' ',
  "AAABFIGO" varchar(7) NOT NULL default ' ',
  "AAABDOOK" varchar(7) NOT NULL default ' ',
  "AAABDOOL" varchar(7) NOT NULL default ' ',
  "AAABDOOM" varchar(7) NOT NULL default ' ',
  "AAABKPVV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRBW","AAABHQFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOOR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKRBW" varchar(1) NOT NULL default ' ',
  "AAABHQFG" varchar(7) NOT NULL default ' ',
  "AAABFBAZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKRBW","AAABHQFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOPB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDOSD" varchar(7) NOT NULL default ' ',
  "AAABDOQY" varchar(1) NOT NULL default ' ',
  "AAABDOQZ" varchar(1) NOT NULL default ' ',
  "AAABDORA" varchar(1) NOT NULL default ' ',
  "AAABDORB" varchar(1) NOT NULL default ' ',
  "AAABDORC" varchar(1) NOT NULL default ' ',
  "AAABDORD" varchar(1) NOT NULL default ' ',
  "AAABDORE" varchar(1) NOT NULL default ' ',
  "AAABDORF" varchar(1) NOT NULL default ' ',
  "AAABDORG" varchar(1) NOT NULL default ' ',
  "AAABDORH" varchar(1) NOT NULL default ' ',
  "AAABDORI" varchar(1) NOT NULL default ' ',
  "AAABDORJ" varchar(1) NOT NULL default ' ',
  "AAABDORK" varchar(1) NOT NULL default ' ',
  "AAABDORL" varchar(1) NOT NULL default ' ',
  "AAABDORM" varchar(1) NOT NULL default ' ',
  "AAABDORN" varchar(1) NOT NULL default ' ',
  "AAABDORO" varchar(1) NOT NULL default ' ',
  "AAABDORP" varchar(1) NOT NULL default ' ',
  "AAABDORQ" varchar(1) NOT NULL default ' ',
  "AAABDORR" varchar(1) NOT NULL default ' ',
  "AAABDORS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDOSD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOQB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDOSD" varchar(7) NOT NULL default ' ',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABFJAC" varchar(5) NOT NULL default ' ',
  "AAABKQNK" varchar(40) NOT NULL default ' ',
  "AAABKSUH" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDOSD","AAABLKXR","AAABFJAC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOQM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDOSD" varchar(7) NOT NULL default ' ',
  "AAABJFHC" varchar(5) NOT NULL default ' ',
  "AAABKQNK" varchar(40) NOT NULL default ' ',
  "AAABKSUH" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDOSD","AAABJFHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOQV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDOSD" varchar(7) NOT NULL default ' ',
  "AAABDOQY" varchar(1) NOT NULL default ' ',
  "AAABDOQZ" varchar(1) NOT NULL default ' ',
  "AAABDORA" varchar(1) NOT NULL default ' ',
  "AAABDORB" varchar(1) NOT NULL default ' ',
  "AAABDORC" varchar(1) NOT NULL default ' ',
  "AAABDORD" varchar(1) NOT NULL default ' ',
  "AAABDORE" varchar(1) NOT NULL default ' ',
  "AAABDORF" varchar(1) NOT NULL default ' ',
  "AAABDORG" varchar(1) NOT NULL default ' ',
  "AAABDORH" varchar(1) NOT NULL default ' ',
  "AAABDORI" varchar(1) NOT NULL default ' ',
  "AAABDORJ" varchar(1) NOT NULL default ' ',
  "AAABDORK" varchar(1) NOT NULL default ' ',
  "AAABDORL" varchar(1) NOT NULL default ' ',
  "AAABDORM" varchar(1) NOT NULL default ' ',
  "AAABDORN" varchar(1) NOT NULL default ' ',
  "AAABDORO" varchar(1) NOT NULL default ' ',
  "AAABDORP" varchar(1) NOT NULL default ' ',
  "AAABDORQ" varchar(1) NOT NULL default ' ',
  "AAABDORR" varchar(1) NOT NULL default ' ',
  "AAABDORS" varchar(1) NOT NULL default ' ',
  "AAABDORT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDOSD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDORW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDOSD" varchar(7) NOT NULL default ' ',
  "AAABFBAZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDOSD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOSE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABDOSI" varchar(8) NOT NULL default ' ',
  "AAABDOSJ" varchar(2) NOT NULL default ' ',
  "AAABDOSK" varchar(1) NOT NULL default ' ',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABLOZE" varchar(18) NOT NULL default ' ',
  "AAABKNSU" varchar(7) NOT NULL default ' ',
  "AAABLURO" varchar(3) NOT NULL default ' ',
  "AAABDOSP" varchar(1) NOT NULL default ' ',
  "AAABKQPE" varchar(2) NOT NULL default '00',
  "AAABKRBW" varchar(1) NOT NULL default ' ',
  "AAABKRJU" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABMLHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOSW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKOGD" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABKOGD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOTE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHQRM" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHQRM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOTM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABFYCS" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABJHFD" varchar(1) NOT NULL default ' ',
  "AAABDXHF" varchar(1) NOT NULL default ' ',
  "AAABGLYJ" varchar(1) NOT NULL default ' ',
  "AAABLSDL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAABKNSU","AAABFYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOUB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABFYCS" varchar(3) NOT NULL default '000',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREL","AAABKNSU","AAABFYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOUN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDQBD" varchar(12) NOT NULL default ' ',
  "AAABDOUQ" varchar(1) NOT NULL default ' ',
  "AAABDOUR" varchar(1) NOT NULL default '0',
  "AAABDOUS" varchar(1) NOT NULL default ' ',
  "AAABDOUT" varchar(1) NOT NULL default '0',
  "AAABDOUU" varchar(1) NOT NULL default '0',
  "AAABDOUV" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABDQBD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOUY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDQBD" varchar(12) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDQBD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOVG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(12) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABDPQW" varchar(6) NOT NULL default ' ',
  "AAABDPQX" varchar(6) NOT NULL default ' ',
  "AAABDOVM" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOVQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUVS" varchar(12) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUVS","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOWA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABJHFD" varchar(1) NOT NULL default ' ',
  "AAABDXHF" varchar(1) NOT NULL default ' ',
  "AAABDXAX" varchar(12) NOT NULL default ' ',
  "AAABDQAC" varchar(12) NOT NULL default ' ',
  "AAABDXAY" varchar(12) NOT NULL default ' ',
  "AAABDOWK" varchar(12) NOT NULL default ' ',
  "AAABDXAZ" varchar(12) NOT NULL default ' ',
  "AAABDQBD" varchar(12) NOT NULL default ' ',
  "AAABDRSI" varchar(1) NOT NULL default ' ',
  "AAABDQBI" varchar(1) NOT NULL default ' ',
  "AAABDOWP" varchar(1) NOT NULL default ' ',
  "AAABDOWQ" varchar(1) NOT NULL default ' ',
  "AAABDOWR" varchar(1) NOT NULL default ' ',
  "AAABDOWS" varchar(1) NOT NULL default ' ',
  "AAABDOWT" varchar(1) NOT NULL default ' ',
  "AAABDXBA" varchar(1) NOT NULL default ' ',
  "AAABDXBC" varchar(1) NOT NULL default ' ',
  "AAABDXBB" varchar(1) NOT NULL default ' ',
  "AAABDOWX" varchar(1) NOT NULL default ' ',
  "AAABDXBD" varchar(1) NOT NULL default ' ',
  "AAABDXBE" varchar(2) NOT NULL default '00',
  "AAABDXBF" varchar(2) NOT NULL default '00',
  "AAABDXAW" varchar(14) NOT NULL default ' ',
  "AAABDXBG" varchar(14) NOT NULL default ' ',
  "AAABDQBO" varchar(14) NOT NULL default ' ',
  "AAABFTXT" varchar(7) NOT NULL default ' ',
  "AAABDOXF" varchar(1) NOT NULL default ' ',
  "AAABDOXG" varchar(1) NOT NULL default ' ',
  "AAABDOXH" varchar(1) NOT NULL default ' ',
  "AAABDOXI" varchar(12) NOT NULL default ' ',
  "AAABDOXJ" varchar(1) NOT NULL default ' ',
  "AAABDOXK" varchar(1) NOT NULL default ' ',
  "AAABDOXL" varchar(1) NOT NULL default ' ',
  "AAABDOXM" varchar(14) NOT NULL default ' ',
  "AAABDOXN" varchar(14) NOT NULL default ' ',
  "AAABDOXO" varchar(7) NOT NULL default ' ',
  "AAABDOXP" varchar(1) NOT NULL default ' ',
  "AAABDOXQ" varchar(1) NOT NULL default ' ',
  "AAABDOXR" varchar(1) NOT NULL default ' ',
  "AAABDOXS" varchar(12) NOT NULL default ' ',
  "AAABDOXT" varchar(14) NOT NULL default ' ',
  "AAABDOXU" varchar(14) NOT NULL default ' ',
  "AAABDOXV" varchar(7) NOT NULL default ' ',
  "AAABDOXW" varchar(1) NOT NULL default ' ',
  "AAABDOXX" varchar(1) NOT NULL default ' ',
  "AAABDOXY" varchar(1) NOT NULL default ' ',
  "AAABDOXZ" varchar(12) NOT NULL default ' ',
  "AAABDOYA" varchar(1) NOT NULL default ' ',
  "AAABDOYB" varchar(1) NOT NULL default ' ',
  "AAABDOYC" varchar(1) NOT NULL default ' ',
  "AAABDOYD" varchar(1) NOT NULL default ' ',
  "AAABDQBP" varchar(1) NOT NULL default ' ',
  "AAABDQBQ" varchar(1) NOT NULL default ' ',
  "AAABDQBR" varchar(1) NOT NULL default ' ',
  "AAABDOYH" varchar(1) NOT NULL default ' ',
  "AAABDOYI" varchar(1) NOT NULL default ' ',
  "AAABDQBS" varchar(1) NOT NULL default ' ',
  "AAABDQBT" varchar(1) NOT NULL default ' ',
  "AAABDQBU" varchar(1) NOT NULL default ' ',
  "AAABDQBV" varchar(1) NOT NULL default ' ',
  "AAABDQBW" varchar(1) NOT NULL default ' ',
  "AAABDQBX" varchar(1) NOT NULL default ' ',
  "AAABDQBY" varchar(1) NOT NULL default ' ',
  "AAABDQBZ" varchar(1) NOT NULL default ' ',
  "AAABDQCA" varchar(1) NOT NULL default ' ',
  "AAABDOYS" varchar(1) NOT NULL default ' ',
  "AAABDOYT" varchar(1) NOT NULL default ' ',
  "AAABDQCB" varchar(1) NOT NULL default ' ',
  "AAABDOYV" varchar(1) NOT NULL default ' ',
  "AAABDQCC" varchar(1) NOT NULL default ' ',
  "AAABDOYX" varchar(1) NOT NULL default ' ',
  "AAABDOYY" varchar(1) NOT NULL default ' ',
  "AAABDQBE" varchar(3) NOT NULL default ' ',
  "AAABDQBF" varchar(3) NOT NULL default ' ',
  "AAABDQBG" varchar(3) NOT NULL default ' ',
  "AAABIMIT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAABKNSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDOZG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABJHFD" varchar(1) NOT NULL default ' ',
  "AAABDXHF" varchar(1) NOT NULL default ' ',
  "AAABDPBV" varchar(1) NOT NULL default ' ',
  "AAABHHLQ" varchar(1) NOT NULL default ' ',
  "AAABDPBX" varchar(1) NOT NULL default ' ',
  "AAABDPBY" varchar(1) NOT NULL default ' ',
  "AAABDPBZ" varchar(1) NOT NULL default ' ',
  "AAABKRHZ" varchar(12) NOT NULL default ' ',
  "AAABDPCE" varchar(1) NOT NULL default ' ',
  "AAABDOZT" varchar(1) NOT NULL default ' ',
  "AAABDPCF" varchar(1) NOT NULL default ' ',
  "AAABDOZV" varchar(1) NOT NULL default ' ',
  "AAABDPCG" varchar(1) NOT NULL default ' ',
  "AAABDPCH" varchar(1) NOT NULL default ' ',
  "AAABDPCI" varchar(1) NOT NULL default ' ',
  "AAABDPCJ" varchar(1) NOT NULL default ' ',
  "AAABDPCK" varchar(1) NOT NULL default ' ',
  "AAABDPCL" varchar(1) NOT NULL default ' ',
  "AAABDPCM" varchar(1) NOT NULL default ' ',
  "AAABLTQC" varchar(1) NOT NULL default ' ',
  "AAABGOAH" varchar(1) NOT NULL default ' ',
  "AAABDPCP" varchar(1) NOT NULL default ' ',
  "AAABJWIP" varchar(1) NOT NULL default ' ',
  "AAABDPCR" varchar(1) NOT NULL default ' ',
  "AAABDPCS" tinyint(3) unsigned NOT NULL default '0',
  "AAABMOIB" varchar(14) NOT NULL default ' ',
  "AAABDPDU" varchar(12) NOT NULL default ' ',
  "AAABDPCV" varchar(12) NOT NULL default ' ',
  "AAABKSAU" varchar(20) NOT NULL default ' ',
  "AAABGAMR" varchar(1) NOT NULL default ' ',
  "AAABDPCY" varchar(1) NOT NULL default ' ',
  "AAABDPCZ" varchar(1) NOT NULL default ' ',
  "AAABDPDA" varchar(1) NOT NULL default ' ',
  "AAABDPDB" varchar(1) NOT NULL default ' ',
  "AAABDPAS" varchar(1) NOT NULL default ' ',
  "AAABDPCC" varchar(1) NOT NULL default ' ',
  "AAABDPCD" varchar(1) NOT NULL default ' ',
  "AAABDPAV" varchar(1) NOT NULL default ' ',
  "AAABDPAW" varchar(1) NOT NULL default ' ',
  "AAABDPCB" varchar(1) NOT NULL default ' ',
  "AAABDPAY" varchar(1) NOT NULL default ' ',
  "AAABDPAZ" varchar(1) NOT NULL default ' ',
  "AAABDPBA" varchar(1) NOT NULL default ' ',
  "AAABDPBB" varchar(1) NOT NULL default ' ',
  "AAABDPBC" varchar(1) NOT NULL default ' ',
  "AAABDPBD" varchar(1) NOT NULL default ' ',
  "AAABDPBE" varchar(1) NOT NULL default ' ',
  "AAABJDMI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPBI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKNSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPBQ" (
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABJHFD" varchar(1) NOT NULL default ' ',
  "AAABDXHF" varchar(1) NOT NULL default ' ',
  "AAABDPBV" varchar(1) NOT NULL default ' ',
  "AAABHHLQ" varchar(1) NOT NULL default ' ',
  "AAABDPBX" varchar(1) NOT NULL default ' ',
  "AAABDPBY" varchar(1) NOT NULL default ' ',
  "AAABDPBZ" varchar(1) NOT NULL default ' ',
  "AAABKRHZ" varchar(12) NOT NULL default ' ',
  "AAABDPCB" varchar(1) NOT NULL default ' ',
  "AAABDPCC" varchar(1) NOT NULL default ' ',
  "AAABDPCD" varchar(1) NOT NULL default ' ',
  "AAABDPCE" varchar(1) NOT NULL default ' ',
  "AAABDPCF" varchar(1) NOT NULL default ' ',
  "AAABDPCG" varchar(1) NOT NULL default ' ',
  "AAABDPCH" varchar(1) NOT NULL default ' ',
  "AAABDPCI" varchar(1) NOT NULL default ' ',
  "AAABDPCJ" varchar(1) NOT NULL default ' ',
  "AAABDPCK" varchar(1) NOT NULL default ' ',
  "AAABDPCL" varchar(1) NOT NULL default ' ',
  "AAABDPCM" varchar(1) NOT NULL default ' ',
  "AAABLTQC" varchar(1) NOT NULL default ' ',
  "AAABGOAH" varchar(1) NOT NULL default ' ',
  "AAABDPCP" varchar(1) NOT NULL default ' ',
  "AAABJWIP" varchar(1) NOT NULL default ' ',
  "AAABDPCR" varchar(1) NOT NULL default ' ',
  "AAABDPCS" tinyint(3) unsigned NOT NULL default '0',
  "AAABMOIB" varchar(14) NOT NULL default ' ',
  "AAABDPDU" varchar(12) NOT NULL default ' ',
  "AAABDPCV" varchar(12) NOT NULL default ' ',
  "AAABKSAU" varchar(20) NOT NULL default ' ',
  "AAABGAMR" varchar(1) NOT NULL default ' ',
  "AAABDPCY" varchar(1) NOT NULL default ' ',
  "AAABDPCZ" varchar(1) NOT NULL default ' ',
  "AAABDPDA" varchar(1) NOT NULL default ' ',
  "AAABDPDB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKNSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPDD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDPDU" varchar(12) NOT NULL default ' ',
  "AAABDPDG" varchar(2) NOT NULL default '00',
  "AAABDPDH" varchar(2) NOT NULL default ' ',
  "AAABEPSM" varchar(8) NOT NULL default ' ',
  "AAABIWLU" varchar(15) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDPDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPDN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDPDU" varchar(12) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDPDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPDV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREL","AAABKNSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPEF" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLUNJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPEJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABFYCS" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABJHFD" varchar(1) NOT NULL default ' ',
  "AAABDXHF" varchar(1) NOT NULL default ' ',
  "AAABLSDL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAABKNSU","AAABFYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPEX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABFYCS" varchar(3) NOT NULL default '000',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREL","AAABKNSU","AAABFYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPFJ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLXIS" varchar(15) NOT NULL default ' ',
  "AAABGGHA" varchar(20) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPFR" (
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABDPFT" varchar(120) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAABEDDS" varchar(32) NOT NULL default ' ',
  "AAABDPFW" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPFY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABFYCS" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABJHFD" varchar(1) NOT NULL default ' ',
  "AAABDXHF" varchar(1) NOT NULL default ' ',
  "AAABMLMY" varchar(6) NOT NULL default '000000',
  "AAABIWAM" varchar(1) NOT NULL default ' ',
  "AAABFYCW" varchar(1) NOT NULL default ' ',
  "AAABLSDL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAABKNSU","AAABFYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPGP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABFYCS" varchar(3) NOT NULL default '000',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREL","AAABKNSU","AAABFYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPHB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDPHX" varchar(4) NOT NULL default ' ',
  "AAABDPHY" varchar(12) NOT NULL default ' ',
  "AAABDPHZ" varchar(1) NOT NULL default ' ',
  "AAABDPHG" varchar(12) NOT NULL default ' ',
  "AAABDPHH" varchar(12) NOT NULL default ' ',
  "AAABDPHI" varchar(12) NOT NULL default ' ',
  "AAABDPHJ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDPHX","AAABDPHY","AAABDPHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPHO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDPHX" varchar(4) NOT NULL default ' ',
  "AAABDPHY" varchar(12) NOT NULL default ' ',
  "AAABDPHZ" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDPHX","AAABDPHY","AAABDPHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPIA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNXB" varchar(8) NOT NULL default '00000000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDPKZ" varchar(40) NOT NULL default ' ',
  "AAABDPIG" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABKNXB","AAABMLCZ","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPIL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(4) NOT NULL default ' ',
  "AAABKROB" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDPIQ" varchar(1) NOT NULL default ' ',
  "AAABDPIR" varchar(1) NOT NULL default ' ',
  "AAABDPIS" varchar(1) NOT NULL default ' ',
  "AAABDPIT" varchar(1) NOT NULL default ' ',
  "AAABDPIU" varchar(1) NOT NULL default ' ',
  "AAABDPIV" varchar(1) NOT NULL default ' ',
  "AAABDPIW" varchar(1) NOT NULL default ' ',
  "AAABDTOD" varchar(4) NOT NULL default ' ',
  "AAABKQVZ" varchar(1) NOT NULL default ' ',
  "AAABDPIZ" varchar(1) NOT NULL default ' ',
  "AAABDPJA" varchar(1) NOT NULL default ' ',
  "AAABDPJB" varchar(1) NOT NULL default ' ',
  "AAABDPJC" varchar(1) NOT NULL default ' ',
  "AAABKRPC" varchar(1) NOT NULL default ' ',
  "AAABDPJE" varchar(1) NOT NULL default ' ',
  "AAABKRPW" varchar(1) NOT NULL default ' ',
  "AAABDPJG" varchar(1) NOT NULL default ' ',
  "AAABDPJH" varchar(1) NOT NULL default ' ',
  "AAABHPNU" varchar(1) NOT NULL default ' ',
  "AAABKRQJ" varchar(1) NOT NULL default ' ',
  "AAABDPJK" varchar(1) NOT NULL default ' ',
  "AAABDTOE" varchar(1) NOT NULL default ' ',
  "AAABKRQP" varchar(1) NOT NULL default ' ',
  "AAABDPJN" varchar(1) NOT NULL default ' ',
  "AAABKRQO" varchar(1) NOT NULL default ' ',
  "AAABDPJP" varchar(1) NOT NULL default ' ',
  "AAABDPJQ" varchar(1) NOT NULL default ' ',
  "AAABDPJR" varchar(1) NOT NULL default ' ',
  "AAABDPJS" varchar(1) NOT NULL default ' ',
  "AAABDPJT" varchar(1) NOT NULL default ' ',
  "AAABDPJU" decimal(3,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPJX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUVS" varchar(4) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPKF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDPKZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDPKZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPKK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNXB" varchar(24) NOT NULL default ' ',
  "AAABLSOG" varchar(1) NOT NULL default '0',
  "AAABJEXI" varchar(1) NOT NULL default '0',
  "AAABMKEO" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKNXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPKS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDPKZ" varchar(10) NOT NULL default ' ',
  "AAABDPKW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDPKZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPLA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(12) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABDPQW" varchar(6) NOT NULL default ' ',
  "AAABDPQX" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPLJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUVS" varchar(12) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUVS","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPLT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRYF" varchar(3) NOT NULL default ' ',
  "AAABLSOG" varchar(1) NOT NULL default '0',
  "AAABDPLX" varchar(1) NOT NULL default '0',
  "AAABMKEO" varchar(1) NOT NULL default '0',
  "AAABGHJP" varchar(1) NOT NULL default ' ',
  "AAABDPMA" varchar(1) NOT NULL default ' ',
  "AAABDPMB" varchar(1) NOT NULL default ' ',
  "AAABDPMC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRYF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPMF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKRYF" varchar(3) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKRYF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPMN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(6) NOT NULL default ' ',
  "AAABDPPI" varchar(6) NOT NULL default ' ',
  "AAABDPQR" varchar(6) NOT NULL default ' ',
  "AAABDPPP" varchar(6) NOT NULL default ' ',
  "AAABDPQK" varchar(6) NOT NULL default ' ',
  "AAABDPQD" varchar(6) NOT NULL default ' ',
  "AAABDPPW" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPMY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUVS" varchar(6) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPNG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDPPI" varchar(6) NOT NULL default ' ',
  "AAABDPNK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDPPI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPNO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDPPP" varchar(6) NOT NULL default ' ',
  "AAABDPNS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDPPP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPNW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDPPW" varchar(6) NOT NULL default ' ',
  "AAABDPOA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDPPW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPOE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDPQD" varchar(6) NOT NULL default ' ',
  "AAABDPOI" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDPQD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPOM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDPQK" varchar(6) NOT NULL default ' ',
  "AAABDPOQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDPQK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDPQR" varchar(6) NOT NULL default ' ',
  "AAABDPOY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDPQR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPPC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDPPI" varchar(6) NOT NULL default ' ',
  "AAABDPPF" varchar(11) NOT NULL default ' ',
  "AAABDPPG" varchar(11) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDPPI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPPJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDPPP" varchar(6) NOT NULL default ' ',
  "AAABDPPM" varchar(11) NOT NULL default ' ',
  "AAABDPPN" varchar(11) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDPPP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPPQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDPPW" varchar(6) NOT NULL default ' ',
  "AAABDPPT" varchar(11) NOT NULL default ' ',
  "AAABDPPU" varchar(11) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDPPW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPPX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDPQD" varchar(6) NOT NULL default ' ',
  "AAABDPQA" varchar(11) NOT NULL default ' ',
  "AAABDPQB" varchar(11) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDPQD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPQE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDPQK" varchar(6) NOT NULL default ' ',
  "AAABDPQH" varchar(11) NOT NULL default ' ',
  "AAABDPQI" varchar(11) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDPQK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPQL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDPQR" varchar(6) NOT NULL default ' ',
  "AAABDPQO" varchar(11) NOT NULL default ' ',
  "AAABDPQP" varchar(11) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDPQR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPQS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(12) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABDPQW" varchar(6) NOT NULL default ' ',
  "AAABDPQX" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPRB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUVS" varchar(12) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUVS","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPRL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(12) NOT NULL default ' ',
  "AAABDPRO" varchar(1) NOT NULL default ' ',
  "AAABDPRP" varchar(1) NOT NULL default ' ',
  "AAABDPRQ" varchar(11) NOT NULL default ' ',
  "AAABDPRR" varchar(1) NOT NULL default ' ',
  "AAABLFTW" varchar(4) NOT NULL default ' ',
  "AAABEYKB" varchar(3) NOT NULL default '000',
  "AAABDPRU" varchar(1) NOT NULL default ' ',
  "AAABDPRV" varchar(5) NOT NULL default '00000',
  "AAABDPRW" varchar(1) NOT NULL default ' ',
  "AAABDPRX" varchar(11) NOT NULL default ' ',
  "AAABDPRY" varchar(11) NOT NULL default ' ',
  "AAABDPRZ" varchar(1) NOT NULL default ' ',
  "AAABDPSA" varchar(1) NOT NULL default ' ',
  "AAABDPSB" varchar(11) NOT NULL default ' ',
  "AAABDPSC" varchar(11) NOT NULL default ' ',
  "AAABDPSD" varchar(1) NOT NULL default ' ',
  "AAABDPSE" varchar(1) NOT NULL default ' ',
  "AAABDPSF" varchar(11) NOT NULL default ' ',
  "AAABDPSG" varchar(11) NOT NULL default ' ',
  "AAABDPSH" varchar(11) NOT NULL default ' ',
  "AAABDPSI" varchar(5) NOT NULL default '00000',
  "AAABDPSJ" varchar(11) NOT NULL default ' ',
  "AAABDPSK" varchar(11) NOT NULL default ' ',
  "AAABDPSL" varchar(11) NOT NULL default ' ',
  "AAABDPSM" varchar(11) NOT NULL default ' ',
  "AAABDPSN" varchar(11) NOT NULL default ' ',
  "AAABDPSO" varchar(1) NOT NULL default ' ',
  "AAABDPSP" varchar(1) NOT NULL default ' ',
  "AAABDPSQ" varchar(11) NOT NULL default ' ',
  "AAABDPSR" varchar(11) NOT NULL default ' ',
  "AAABDPSS" varchar(11) NOT NULL default ' ',
  "AAABDPST" varchar(1) NOT NULL default ' ',
  "AAABDPSU" varchar(11) NOT NULL default ' ',
  "AAABDPSV" varchar(11) NOT NULL default ' ',
  "AAABDPSW" varchar(11) NOT NULL default ' ',
  "AAABDPSX" varchar(3) NOT NULL default '000',
  "AAABLFTX" varchar(4) NOT NULL default '0000',
  "AAABDPSZ" varchar(3) NOT NULL default '000',
  "AAABDPTA" varchar(4) NOT NULL default '0000',
  "AAABDPTB" varchar(3) NOT NULL default '000',
  "AAABDPTC" varchar(4) NOT NULL default '0000',
  "AAABDPTD" varchar(5) NOT NULL default '00000',
  "AAABDPTE" varchar(1) NOT NULL default ' ',
  "AAABDPTF" varchar(1) NOT NULL default ' ',
  "AAABDPTG" varchar(1) NOT NULL default ' ',
  "AAABDPTH" varchar(6) NOT NULL default '000000',
  "AAABDPTI" varchar(1) NOT NULL default ' ',
  "AAABDPTJ" varchar(1) NOT NULL default ' ',
  "AAABDPTK" varchar(1) NOT NULL default ' ',
  "AAABDPTL" varchar(1) NOT NULL default ' ',
  "AAABDPTM" varchar(3) NOT NULL default '000',
  "AAABDPTN" varchar(3) NOT NULL default '000',
  "AAABDPTO" varchar(3) NOT NULL default '000',
  "AAABDPTP" varchar(3) NOT NULL default '000',
  "AAABDPTQ" varchar(1) NOT NULL default ' ',
  "AAABDPTR" varchar(3) NOT NULL default '000',
  "AAABDPTS" varchar(3) NOT NULL default '000',
  "AAABDPTT" varchar(1) NOT NULL default ' ',
  "AAABDPTU" varchar(5) NOT NULL default '00000',
  "AAABDPTV" varchar(1) NOT NULL default ' ',
  "AAABDPTW" varchar(1) NOT NULL default ' ',
  "AAABDPTX" varchar(1) NOT NULL default ' ',
  "AAABDPTY" varchar(2) NOT NULL default '00',
  "AAABDPTZ" varchar(1) NOT NULL default ' ',
  "AAABDPUA" varchar(1) NOT NULL default ' ',
  "AAABDPUB" varchar(1) NOT NULL default ' ',
  "AAABDPUC" varchar(4) NOT NULL default '0000',
  "AAABDPUD" varchar(1) NOT NULL default ' ',
  "AAABLLZQ" varchar(1) NOT NULL default ' ',
  "AAABLROG" varchar(1) NOT NULL default ' ',
  "AAABDPUG" varchar(4) NOT NULL default '0000',
  "AAABDPUH" varchar(1) NOT NULL default ' ',
  "AAABDPUI" varchar(1) NOT NULL default ' ',
  "AAABDPUJ" varchar(4) NOT NULL default '0000',
  "AAABDPUK" varchar(2) NOT NULL default '00',
  "AAABDPUL" varchar(2) NOT NULL default ' ',
  "AAABDPUM" varchar(1) NOT NULL default ' ',
  "AAABDPUN" varchar(1) NOT NULL default ' ',
  "AAABDPUO" varchar(1) NOT NULL default ' ',
  "AAABDPUP" varchar(1) NOT NULL default ' ',
  "AAABDPUQ" varchar(1) NOT NULL default ' ',
  "AAABDPUR" varchar(1) NOT NULL default ' ',
  "AAABDPUS" varchar(1) NOT NULL default ' ',
  "AAABDPUT" varchar(1) NOT NULL default ' ',
  "AAABDPUU" varchar(1) NOT NULL default ' ',
  "AAABDPUV" varchar(1) NOT NULL default ' ',
  "AAABDPUW" varchar(1) NOT NULL default ' ',
  "AAABDPUX" varchar(1) NOT NULL default ' ',
  "AAABDPUY" varchar(1) NOT NULL default ' ',
  "AAABDPUZ" varchar(1) NOT NULL default ' ',
  "AAABDPVA" varchar(1) NOT NULL default ' ',
  "AAABLEEP" varchar(1) NOT NULL default ' ',
  "AAABEZGI" varchar(1) NOT NULL default ' ',
  "AAABDPVD" varchar(1) NOT NULL default ' ',
  "AAABDPVE" varchar(1) NOT NULL default ' ',
  "AAABDPVF" varchar(12) NOT NULL default ' ',
  "AAABDPVG" varchar(12) NOT NULL default ' ',
  "AAABDPVH" varchar(12) NOT NULL default ' ',
  "AAABDPVI" varchar(1) NOT NULL default '0',
  "AAABDPVJ" varchar(1) NOT NULL default ' ',
  "AAABDPVK" varchar(1) NOT NULL default ' ',
  "AAABDPVL" varchar(1) NOT NULL default ' ',
  "AAABDPVM" varchar(1) NOT NULL default ' ',
  "AAABDPVN" varchar(1) NOT NULL default ' ',
  "AAABDPVO" varchar(1) NOT NULL default ' ',
  "AAABDPVP" varchar(1) NOT NULL default ' ',
  "AAABDPVQ" varchar(1) NOT NULL default ' ',
  "AAABDPVR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUVS" varchar(12) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPWC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABFYCS" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABJHFD" varchar(1) NOT NULL default ' ',
  "AAABDXHF" varchar(1) NOT NULL default ' ',
  "AAABGLYJ" varchar(1) NOT NULL default ' ',
  "AAABLSDL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAABKNSU","AAABFYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPWR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJEXI" varchar(8) NOT NULL default '00000000',
  "AAABLCCC" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABJEXI","AAABLCCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPXA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABFYCS" varchar(3) NOT NULL default '000',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREL","AAABKNSU","AAABFYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPXM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKOIN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKOIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPXR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKOIN" varchar(20) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKOIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPXZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCCC" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPYE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABFYCS" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABJHFD" varchar(1) NOT NULL default ' ',
  "AAABDXHF" varchar(1) NOT NULL default ' ',
  "AAABGLYJ" varchar(1) NOT NULL default ' ',
  "AAABLSDL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAABKNSU","AAABFYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPYT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABFYCS" varchar(3) NOT NULL default '000',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREL","AAABKNSU","AAABFYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDPZF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABJHFD" varchar(1) NOT NULL default ' ',
  "AAABDXHF" varchar(1) NOT NULL default ' ',
  "AAABDPZL" varchar(12) NOT NULL default ' ',
  "AAABDPZM" varchar(12) NOT NULL default ' ',
  "AAABDPZN" varchar(12) NOT NULL default ' ',
  "AAABDPZO" varchar(12) NOT NULL default ' ',
  "AAABDPZP" varchar(12) NOT NULL default ' ',
  "AAABDPZQ" varchar(12) NOT NULL default ' ',
  "AAABDPZR" varchar(12) NOT NULL default ' ',
  "AAABDPZS" varchar(12) NOT NULL default ' ',
  "AAABDPZT" varchar(12) NOT NULL default ' ',
  "AAABDPZU" varchar(12) NOT NULL default ' ',
  "AAABDPZV" varchar(12) NOT NULL default ' ',
  "AAABDPZW" varchar(12) NOT NULL default ' ',
  "AAABDPZX" varchar(12) NOT NULL default ' ',
  "AAABDPZY" varchar(12) NOT NULL default ' ',
  "AAABDPZZ" varchar(12) NOT NULL default ' ',
  "AAABDQAA" varchar(12) NOT NULL default ' ',
  "AAABDQAB" varchar(12) NOT NULL default ' ',
  "AAABDQAC" varchar(12) NOT NULL default ' ',
  "AAABDQAD" varchar(10) NOT NULL default ' ',
  "AAABDQAE" varchar(10) NOT NULL default ' ',
  "AAABDQAF" varchar(10) NOT NULL default ' ',
  "AAABDQAG" varchar(10) NOT NULL default ' ',
  "AAABDQAH" varchar(10) NOT NULL default ' ',
  "AAABDQAI" varchar(10) NOT NULL default ' ',
  "AAABDQAJ" varchar(10) NOT NULL default ' ',
  "AAABDQAK" varchar(10) NOT NULL default ' ',
  "AAABDQAL" varchar(10) NOT NULL default ' ',
  "AAABDQAM" varchar(10) NOT NULL default ' ',
  "AAABDQAN" varchar(10) NOT NULL default ' ',
  "AAABDQAO" varchar(10) NOT NULL default ' ',
  "AAABDQAP" varchar(10) NOT NULL default ' ',
  "AAABDQAQ" varchar(60) NOT NULL default ' ',
  "AAABDQAR" varchar(60) NOT NULL default ' ',
  "AAABDQAS" varchar(12) NOT NULL default ' ',
  "AAABDQAT" varchar(12) NOT NULL default ' ',
  "AAABDQAU" varchar(2) NOT NULL default '00',
  "AAABDQAV" varchar(2) NOT NULL default '00',
  "AAABDQAW" varchar(1) NOT NULL default ' ',
  "AAABDQAX" varchar(12) NOT NULL default ' ',
  "AAABDQAY" varchar(4) NOT NULL default ' ',
  "AAABDQAZ" varchar(12) NOT NULL default ' ',
  "AAABDQBA" varchar(1) NOT NULL default ' ',
  "AAABDQBB" varchar(12) NOT NULL default ' ',
  "AAABDXAX" varchar(12) NOT NULL default ' ',
  "AAABDQBD" varchar(12) NOT NULL default ' ',
  "AAABDQBE" varchar(3) NOT NULL default ' ',
  "AAABDQBF" varchar(3) NOT NULL default ' ',
  "AAABDQBG" varchar(3) NOT NULL default ' ',
  "AAABDRSI" varchar(1) NOT NULL default ' ',
  "AAABDQBI" varchar(1) NOT NULL default ' ',
  "AAABDQBJ" varchar(1) NOT NULL default ' ',
  "AAABDXBE" varchar(2) NOT NULL default '00',
  "AAABDXBF" varchar(2) NOT NULL default '00',
  "AAABDXAW" varchar(14) NOT NULL default ' ',
  "AAABFTXT" varchar(7) NOT NULL default ' ',
  "AAABDQBO" varchar(14) NOT NULL default ' ',
  "AAABDQBP" varchar(1) NOT NULL default ' ',
  "AAABDQBQ" varchar(1) NOT NULL default ' ',
  "AAABDQBR" varchar(1) NOT NULL default ' ',
  "AAABDQBS" varchar(1) NOT NULL default ' ',
  "AAABDQBT" varchar(1) NOT NULL default ' ',
  "AAABDQBU" varchar(1) NOT NULL default ' ',
  "AAABDQBV" varchar(1) NOT NULL default ' ',
  "AAABDQBW" varchar(1) NOT NULL default ' ',
  "AAABDQBX" varchar(1) NOT NULL default ' ',
  "AAABDQBY" varchar(1) NOT NULL default ' ',
  "AAABDQBZ" varchar(1) NOT NULL default ' ',
  "AAABDQCA" varchar(1) NOT NULL default ' ',
  "AAABDQCB" varchar(1) NOT NULL default ' ',
  "AAABDQCC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQCF" (
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABDQCN" varchar(32) NOT NULL default ' ',
  "AAABDQCO" varchar(32) NOT NULL default ' ',
  "AAABDQCJ" varchar(120) NOT NULL default ' ',
  "AAABDQCK" varchar(32) NOT NULL default ' ',
  "AAABDQCL" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR","AAABDQCN","AAABDQCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQCP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDQHV" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDQHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQCU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDQHV" varchar(5) NOT NULL default ' ',
  "AAABDQFX" varchar(1) NOT NULL default ' ',
  "AAABDQCY" varchar(30) NOT NULL default ' ',
  "AAABDQCZ" varchar(30) NOT NULL default ' ',
  "AAABDQDA" varchar(1) NOT NULL default ' ',
  "AAABDYVV" varchar(1) NOT NULL default ' ',
  "AAABLUHY" varchar(1) NOT NULL default ' ',
  "AAABEAZS" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDQHV","AAABDQFX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQDH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDQHV" varchar(5) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDQHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQDP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDQFC" varchar(6) NOT NULL default '000000',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  "AAABLCKD" varchar(2) NOT NULL default ' ',
  "AAABDQII" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDQFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQDX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDQFC" varchar(6) NOT NULL default '000000',
  "AAABDQHV" varchar(5) NOT NULL default ' ',
  "AAABKQTE" varchar(12) NOT NULL default '000000000000',
  "AAABMLEL" varchar(8) NOT NULL default '00000000',
  "AAABKOOT" varchar(3) NOT NULL default '000',
  "AAABDQHQ" varchar(2) NOT NULL default '00',
  "AAABIHTL" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABDQFC","AAABDQHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQEJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDQGE" varchar(2) NOT NULL default ' ',
  "AAABDQEM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDQGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQEP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDQGE" varchar(2) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDQGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQEX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRZZ" varchar(10) NOT NULL default '0000000000',
  "AAABLMJL" varchar(4) NOT NULL default '0000',
  "AAABKSAB" varchar(1) NOT NULL default ' ',
  "AAABDQFC" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABKRZZ","AAABLMJL","AAABKSAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQFH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRZZ" varchar(10) NOT NULL default '0000000000',
  "AAABLMJL" varchar(4) NOT NULL default '0000',
  "AAABKSAB" varchar(1) NOT NULL default ' ',
  "AAABDQHV" varchar(5) NOT NULL default ' ',
  "AAABDQFX" varchar(1) NOT NULL default ' ',
  "AAABDQFO" varchar(8) NOT NULL default '00000000',
  "AAABDQFP" varchar(6) NOT NULL default '000000',
  "AAABDQFQ" varchar(6) NOT NULL default ' ',
  "AAABDQFR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRZZ","AAABLMJL","AAABKSAB","AAABDQHV","AAABDQFX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQFY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRZZ" varchar(10) NOT NULL default '0000000000',
  "AAABLMJL" varchar(4) NOT NULL default '0000',
  "AAABKSAB" varchar(1) NOT NULL default ' ',
  "AAABDQHV" varchar(5) NOT NULL default ' ',
  "AAABDQGE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRZZ","AAABLMJL","AAABKSAB","AAABDQHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(6) NOT NULL default ' ',
  "AAABIIWF" varchar(1) NOT NULL default ' ',
  "AAABHZVL" varchar(1) NOT NULL default ' ',
  "AAABFJZE" varchar(1) NOT NULL default ' ',
  "AAABDQGQ" varchar(1) NOT NULL default ' ',
  "AAABHZVS" varchar(1) NOT NULL default '0',
  "AAABHZVU" varchar(1) NOT NULL default '0',
  "AAABHZVW" varchar(1) NOT NULL default '0',
  "AAABDQGU" varchar(1) NOT NULL default '0',
  "AAABDQGV" varchar(1) NOT NULL default '0',
  "AAABDQGW" varchar(40) NOT NULL default ' ',
  "AAABDQGX" varchar(1) NOT NULL default ' ',
  "AAABDQGY" varchar(1) NOT NULL default ' ',
  "AAABDQGZ" varchar(1) NOT NULL default ' ',
  "AAABDQHA" varchar(1) NOT NULL default ' ',
  "AAABDQHB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUVS" varchar(6) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQHM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDQII" varchar(6) NOT NULL default ' ',
  "AAABDQHV" varchar(5) NOT NULL default ' ',
  "AAABDQHQ" varchar(2) NOT NULL default '00',
  "AAABIHTL" varchar(3) NOT NULL default '000',
  "AAABLADN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDQII","AAABDQHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQHW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDQII" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDQII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQIB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDQII" varchar(6) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDQII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQIJ" (
  "AAABMKFH" varchar(1) NOT NULL default ' ',
  "AAABKNGM" varchar(1) NOT NULL default ' ',
  "AAABHFQD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABKNGM","AAABHFQD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQIQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKNSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQIY" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLAUS" varchar(55) NOT NULL default ' ',
  "AAABKGYO" varchar(10) NOT NULL default ' ',
  "AAABKGYP" varchar(20) NOT NULL default ' ',
  "AAABKGYQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQJJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABJVTD" varchar(6) NOT NULL default ' ',
  "AAABJVUZ" varchar(3) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABJHFD" varchar(1) NOT NULL default ' ',
  "AAABDXHF" varchar(1) NOT NULL default ' ',
  "AAABDQJR" varchar(1) NOT NULL default ' ',
  "AAABDQJS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNSU","AAABJVTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQJW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKNSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQKE" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABJTCX" varchar(3) NOT NULL default ' ',
  "AAABDQKH" varchar(1) NOT NULL default ' ',
  "AAABDQKI" varchar(1) NOT NULL default ' ',
  "AAABDQKJ" varchar(3) NOT NULL default ' ',
  "AAABDQKK" varchar(30) NOT NULL default ' ',
  "AAABDQKL" varchar(1) NOT NULL default ' ',
  "AAABDQKM" varchar(1) NOT NULL default ' ',
  "AAABDQKN" varchar(1) NOT NULL default ' ',
  "AAABDQKO" varchar(1) NOT NULL default ' ',
  "AAABDQKP" varchar(1) NOT NULL default ' ',
  "AAABDQKQ" varchar(1) NOT NULL default ' ',
  "AAABLMKH" varchar(8) NOT NULL default '00000000',
  "AAABLIRO" varchar(6) NOT NULL default '000000',
  "AAABIACJ" varchar(12) NOT NULL default ' ',
  "AAABDQKU" varchar(1) NOT NULL default ' ',
  "AAABDQKV" varchar(1) NOT NULL default ' ',
  "AAABIGFH" varchar(1) NOT NULL default ' ',
  "AAABDQKX" varchar(1) NOT NULL default ' ',
  "AAABETOT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQLA" (
  "AAABHJRW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABHJRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQLD" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHJRW" varchar(4) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABHJRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQLJ" (
  "AAABHJRW" varchar(4) NOT NULL default ' ',
  "AAABDQTS" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLTTF" varchar(1) NOT NULL default ' ',
  "AAABDQLR" varchar(31) NOT NULL default ' ',
  "AAABDQLS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHJRW","AAABDQTS","AAABMKFH","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQLX" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABDQLZ" varchar(30) NOT NULL default ' ',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAABDQMB" varchar(30) NOT NULL default ' ',
  "AAABIGQI" int(11) NOT NULL default '0',
  "AAABDQMD" int(11) NOT NULL default '0',
  "AAABDQME" int(11) NOT NULL default '0',
  "AAABDQMF" varchar(1) NOT NULL default ' ',
  "AAABDQMG" varchar(1) NOT NULL default ' ',
  "AAABDQMH" varchar(1) NOT NULL default ' ',
  "AAABDQMI" varchar(18) NOT NULL default ' ',
  "AAABDQMJ" varchar(3) NOT NULL default ' ',
  "AAABDQMK" varchar(1) NOT NULL default ' ',
  "AAABDQML" varchar(1) NOT NULL default ' ',
  "AAABDQMM" varchar(1) NOT NULL default ' ',
  "AAABDQMN" varchar(18) NOT NULL default ' ',
  "AAABDQMO" varchar(3) NOT NULL default ' ',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABJTAJ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQMS" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHJRW" varchar(4) NOT NULL default ' ',
  "AAABDQTS" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABHJRW","AAABDQTS","AAABMKFH","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQNE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHJRW" varchar(4) NOT NULL default ' ',
  "AAABDQTS" varchar(4) NOT NULL default ' ',
  "AAABDQTT" varchar(12) NOT NULL default ' ',
  "AAABDQTU" varchar(1) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABMMRH" varchar(2) NOT NULL default '00',
  "AAABIHLK" varchar(3) NOT NULL default '000',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABDQNO" varchar(2) NOT NULL default '00',
  "AAABDQNP" varchar(1) NOT NULL default ' ',
  "AAABFYCW" varchar(1) NOT NULL default ' ',
  "AAABHMLY" varchar(30) NOT NULL default ' ',
  "AAABDQNS" varchar(1) NOT NULL default ' ',
  "AAABDQTJ" varchar(1) NOT NULL default ' ',
  "AAABLTQJ" varchar(1) NOT NULL default ' ',
  "AAABDQTO" varchar(1) NOT NULL default ' ',
  "AAABFPZD" varchar(1) NOT NULL default ' ',
  "AAABLTTF" varchar(1) NOT NULL default ' ',
  "AAABLTXX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHJRW","AAABDQTS","AAABDQTT","AAABDQTU","AAABMKFH","AAABMMRH","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQOH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHJRW" varchar(4) NOT NULL default ' ',
  "AAABDQTS" varchar(4) NOT NULL default ' ',
  "AAABDQTT" varchar(12) NOT NULL default ' ',
  "AAABDQTU" varchar(1) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABMMRH" varchar(2) NOT NULL default '00',
  "AAABIHLK" varchar(3) NOT NULL default '000',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHJRW","AAABDQTS","AAABDQTT","AAABDQTU","AAABMKFH","AAABMMRH","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQPB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABFYCS" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABJHFD" varchar(1) NOT NULL default ' ',
  "AAABDXHF" varchar(1) NOT NULL default ' ',
  "AAABLSDL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAABKNSU","AAABFYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQPP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABFYCS" varchar(3) NOT NULL default '000',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREL","AAABKNSU","AAABFYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQQB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHJRW" varchar(4) NOT NULL default ' ',
  "AAABDQTS" varchar(4) NOT NULL default ' ',
  "AAABDQTT" varchar(12) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABJHFD" varchar(1) NOT NULL default ' ',
  "AAABDXHF" varchar(1) NOT NULL default ' ',
  "AAABKMUE" varchar(3) NOT NULL default '000',
  "AAABDQRY" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABHJRW","AAABDQTS","AAABDQTT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQQP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHJRW" varchar(4) NOT NULL default ' ',
  "AAABDQTS" varchar(4) NOT NULL default ' ',
  "AAABDQTT" varchar(12) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHJRW","AAABDQTS","AAABDQTT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQRB" (
  "AAABHJRW" varchar(4) NOT NULL default ' ',
  "AAABDQTS" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABDQRF" varchar(1) NOT NULL default ' ',
  "AAABDQRG" varchar(1) NOT NULL default ' ',
  "AAABDQRH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHJRW","AAABDQTS","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQRL" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHJRW" varchar(4) NOT NULL default ' ',
  "AAABDQTS" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABHJRW","AAABDQTS","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQRV" (
  "AAABHJRW" varchar(4) NOT NULL default ' ',
  "AAABDQTS" varchar(4) NOT NULL default ' ',
  "AAABDQRY" varchar(3) NOT NULL default '000',
  "AAABDQRZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHJRW","AAABDQTS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQSC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKSUH" varchar(12) NOT NULL default ' ',
  "AAABKQNB" varchar(1) NOT NULL default ' ',
  "AAABDQSG" varchar(1) NOT NULL default ' ',
  "AAABKYUV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKSUH","AAABKQNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQSL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKSUH" varchar(12) NOT NULL default ' ',
  "AAABDQSP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABKSUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQST" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHJRW" varchar(4) NOT NULL default ' ',
  "AAABDQTS" varchar(4) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABHJRW","AAABDQTS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQTB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHJRW" varchar(4) NOT NULL default ' ',
  "AAABDQTS" varchar(4) NOT NULL default ' ',
  "AAABDQTT" varchar(12) NOT NULL default ' ',
  "AAABDQTU" varchar(1) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABDQTI" varchar(1) NOT NULL default ' ',
  "AAABDQTJ" varchar(1) NOT NULL default ' ',
  "AAABDQTK" varchar(1) NOT NULL default ' ',
  "AAABDQTL" varchar(1) NOT NULL default ' ',
  "AAABDQTM" varchar(1) NOT NULL default ' ',
  "AAABLTQJ" varchar(1) NOT NULL default ' ',
  "AAABDQTO" varchar(1) NOT NULL default ' ',
  "AAABFPZD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHJRW","AAABDQTS","AAABDQTT","AAABDQTU","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQTW" (
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABMMVU" varchar(2) NOT NULL default '00',
  "AAABMNLX" varchar(2) NOT NULL default ' ',
  "AAABMNLY" varchar(4) NOT NULL default ' ',
  "AAABMDER" varchar(1) NOT NULL default ' ',
  "AAABMDBL" varchar(1) NOT NULL default ' ',
  "AAABDQUN" varchar(1) NOT NULL default ' ',
  "AAABILEX" varchar(1) NOT NULL default ' ',
  "AAABDQUF" varchar(20) NOT NULL default ' ',
  "AAABDWNR" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNLV","AAABMMVU","AAABMNLX","AAABMNLY","AAABMDER","AAABMDBL","AAABDQUN","AAABILEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQUP" (
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABMNLY" varchar(4) NOT NULL default ' ',
  "AAABMDER" varchar(1) NOT NULL default ' ',
  "AAABDQUY" varchar(1) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNLV","AAABMNLY","AAABMDER","AAABDQUY","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQVA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABMQYE" varchar(1) NOT NULL default ' ',
  "AAABDQVF" varchar(1) NOT NULL default ' ',
  "AAABHOTF" varchar(4) NOT NULL default ' ',
  "AAABDQVH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABMLHQ","AAABMQYE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQVM" (
  "AAABGIKF" varchar(40) NOT NULL default ' ',
  "AAABMNLX" varchar(2) NOT NULL default ' ',
  "AAABMNLY" varchar(4) NOT NULL default ' ',
  "AAABKQPE" varchar(2) NOT NULL default '00',
  "AAABMFSV" varchar(4) NOT NULL default '0000',
  "AAABGPOF" decimal(2,0) NOT NULL default '0',
  "AAABGPOG" decimal(2,0) NOT NULL default '0',
  "AAABGPOH" decimal(2,0) NOT NULL default '0',
  "AAABGPOI" decimal(2,0) NOT NULL default '0',
  PRIMARY KEY ("AAABGIKF","AAABMNLX","AAABMNLY","AAABKQPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQWA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDQXU" varchar(6) NOT NULL default ' ',
  "AAABDQWE" varchar(1) NOT NULL default ' ',
  "AAABDQWF" varchar(1) NOT NULL default ' ',
  "AAABDQWG" varchar(1) NOT NULL default ' ',
  "AAABDQWH" varchar(1) NOT NULL default ' ',
  "AAABLDON" varchar(10) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABKQPE" varchar(2) NOT NULL default '00',
  "AAABDQWL" varchar(1) NOT NULL default ' ',
  "AAABDQWM" varchar(1) NOT NULL default ' ',
  "AAABDQWN" varchar(1) NOT NULL default ' ',
  "AAABDQWO" varchar(1) NOT NULL default ' ',
  "AAABDQWP" varchar(1) NOT NULL default ' ',
  "AAABDQWQ" varchar(1) NOT NULL default ' ',
  "AAABDQWR" varchar(1) NOT NULL default ' ',
  "AAABDQWS" varchar(1) NOT NULL default ' ',
  "AAABDQWT" varchar(1) NOT NULL default ' ',
  "AAABDQWU" varchar(1) NOT NULL default ' ',
  "AAABDQWV" varchar(1) NOT NULL default ' ',
  "AAABDQWW" varchar(1) NOT NULL default ' ',
  "AAABDQWX" varchar(4) NOT NULL default ' ',
  "AAABDQWY" varchar(4) NOT NULL default ' ',
  "AAABDQWZ" varchar(4) NOT NULL default ' ',
  "AAABDQXA" varchar(1) NOT NULL default ' ',
  "AAABDQXB" varchar(1) NOT NULL default ' ',
  "AAABDQXC" varchar(1) NOT NULL default ' ',
  "AAABDQXD" varchar(1) NOT NULL default ' ',
  "AAABDQXE" varchar(1) NOT NULL default ' ',
  "AAABDQXF" varchar(1) NOT NULL default ' ',
  "AAABDQXG" varchar(1) NOT NULL default ' ',
  "AAABDQXH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABDQXU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQXL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDQXU" varchar(6) NOT NULL default ' ',
  "AAABDQXQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCVI","AAABDQXU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDQXV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(6) NOT NULL default ' ',
  "AAABKQPE" varchar(2) NOT NULL default '00',
  "AAABDQXZ" varchar(1) NOT NULL default ' ',
  "AAABDQYA" varchar(1) NOT NULL default ' ',
  "AAABDQYB" varchar(1) NOT NULL default ' ',
  "AAABDQYC" varchar(1) NOT NULL default ' ',
  "AAABDQYD" varchar(1) NOT NULL default ' ',
  "AAABDQYE" varchar(1) NOT NULL default ' ',
  "AAABDQYF" varchar(1) NOT NULL default ' ',
  "AAABDQYG" varchar(1) NOT NULL default ' ',
  "AAABDQYH" varchar(1) NOT NULL default ' ',
  "AAABDQYI" varchar(1) NOT NULL default ' ',
  "AAABDQYJ" varchar(1) NOT NULL default ' ',
  "AAABDQYK" varchar(1) NOT NULL default ' ',
  "AAABDQYL" varchar(1) NOT NULL default ' ',
  "AAABDQYM" varchar(1) NOT NULL default ' ',
  "AAABDQYN" varchar(1) NOT NULL default ' ',
  "AAABDQYO" varchar(1) NOT NULL default ' ',
  "AAABDQYP" varchar(1) NOT NULL default ' ',
  "AAABDQYQ" varchar(1) NOT NULL default ' ',
  "AAABDQYR" varchar(1) NOT NULL default ' ',
  "AAABDQYS" varchar(1) NOT NULL default ' ',
  "AAABDQYT" varchar(1) NOT NULL default ' ',
  "AAABDQYU" varchar(1) NOT NULL default ' ',
  "AAABDQYV" varchar(1) NOT NULL default ' ',
  "AAABDQYW" varchar(1) NOT NULL default ' ',
  "AAABDQYX" varchar(1) NOT NULL default ' ',
  "AAABDQYY" varchar(1) NOT NULL default ' ',
  "AAABDQYZ" varchar(1) NOT NULL default ' ',
  "AAABDQZA" varchar(1) NOT NULL default ' ',
  "AAABDQZB" varchar(1) NOT NULL default ' ',
  "AAABDQZC" varchar(1) NOT NULL default ' ',
  "AAABDQZD" varchar(1) NOT NULL default ' ',
  "AAABDQZE" varchar(1) NOT NULL default ' ',
  "AAABDQZF" varchar(1) NOT NULL default ' ',
  "AAABDQZG" varchar(1) NOT NULL default ' ',
  "AAABDQZH" varchar(1) NOT NULL default ' ',
  "AAABDQZI" varchar(1) NOT NULL default ' ',
  "AAABDQZJ" varchar(1) NOT NULL default ' ',
  "AAABDQZK" varchar(1) NOT NULL default ' ',
  "AAABDQZL" varchar(1) NOT NULL default ' ',
  "AAABDQZM" varchar(1) NOT NULL default ' ',
  "AAABDQZN" varchar(1) NOT NULL default ' ',
  "AAABDQZO" varchar(1) NOT NULL default ' ',
  "AAABDQZP" varchar(1) NOT NULL default ' ',
  "AAABDQZQ" varchar(1) NOT NULL default ' ',
  "AAABDQZR" varchar(1) NOT NULL default ' ',
  "AAABDQZS" varchar(1) NOT NULL default ' ',
  "AAABDQZT" varchar(1) NOT NULL default ' ',
  "AAABDQZU" varchar(1) NOT NULL default ' ',
  "AAABDQZV" varchar(1) NOT NULL default ' ',
  "AAABDQZW" varchar(1) NOT NULL default ' ',
  "AAABDQZX" varchar(1) NOT NULL default ' ',
  "AAABDQZY" varchar(1) NOT NULL default ' ',
  "AAABDQZZ" varchar(1) NOT NULL default ' ',
  "AAABDRAA" varchar(1) NOT NULL default ' ',
  "AAABDRAB" varchar(1) NOT NULL default ' ',
  "AAABDRAC" varchar(1) NOT NULL default ' ',
  "AAABDRAD" varchar(1) NOT NULL default ' ',
  "AAABDRAE" varchar(1) NOT NULL default ' ',
  "AAABDRAF" varchar(1) NOT NULL default ' ',
  "AAABDRAG" varchar(1) NOT NULL default ' ',
  "AAABDRAH" varchar(1) NOT NULL default ' ',
  "AAABDRAI" varchar(1) NOT NULL default ' ',
  "AAABDRAJ" varchar(1) NOT NULL default ' ',
  "AAABDRAK" varchar(1) NOT NULL default ' ',
  "AAABDRAL" varchar(1) NOT NULL default ' ',
  "AAABDRAM" varchar(1) NOT NULL default ' ',
  "AAABDRAN" varchar(1) NOT NULL default ' ',
  "AAABDRAO" varchar(1) NOT NULL default ' ',
  "AAABDRAP" varchar(1) NOT NULL default ' ',
  "AAABDRAQ" varchar(1) NOT NULL default ' ',
  "AAABDRAR" varchar(1) NOT NULL default ' ',
  "AAABDRAS" varchar(1) NOT NULL default ' ',
  "AAABDRAT" varchar(1) NOT NULL default ' ',
  "AAABDRAU" varchar(1) NOT NULL default ' ',
  "AAABDRAV" varchar(1) NOT NULL default ' ',
  "AAABDRAW" varchar(1) NOT NULL default ' ',
  "AAABDRAX" varchar(1) NOT NULL default ' ',
  "AAABDRAY" varchar(1) NOT NULL default ' ',
  "AAABDRAZ" varchar(1) NOT NULL default ' ',
  "AAABDRBA" varchar(1) NOT NULL default ' ',
  "AAABDRBB" varchar(1) NOT NULL default ' ',
  "AAABDRBC" varchar(1) NOT NULL default ' ',
  "AAABDRBD" varchar(1) NOT NULL default ' ',
  "AAABDRBE" varchar(1) NOT NULL default ' ',
  "AAABDRBF" varchar(1) NOT NULL default ' ',
  "AAABDRBG" varchar(1) NOT NULL default ' ',
  "AAABDRBH" varchar(1) NOT NULL default ' ',
  "AAABDRBI" varchar(1) NOT NULL default ' ',
  "AAABDRBJ" varchar(1) NOT NULL default ' ',
  "AAABDRBK" varchar(1) NOT NULL default ' ',
  "AAABDRBL" varchar(1) NOT NULL default ' ',
  "AAABDRBM" varchar(1) NOT NULL default ' ',
  "AAABDRBN" varchar(1) NOT NULL default ' ',
  "AAABDRBO" varchar(1) NOT NULL default ' ',
  "AAABDRBP" varchar(1) NOT NULL default ' ',
  "AAABDRBQ" varchar(1) NOT NULL default ' ',
  "AAABDRBR" varchar(1) NOT NULL default ' ',
  "AAABDRBS" varchar(1) NOT NULL default ' ',
  "AAABDRBT" varchar(1) NOT NULL default ' ',
  "AAABDRBU" varchar(1) NOT NULL default ' ',
  "AAABDRBV" varchar(1) NOT NULL default ' ',
  "AAABDRBW" varchar(1) NOT NULL default ' ',
  "AAABDRBX" varchar(1) NOT NULL default ' ',
  "AAABDRBY" varchar(1) NOT NULL default ' ',
  "AAABDRBZ" varchar(1) NOT NULL default ' ',
  "AAABDRCA" varchar(1) NOT NULL default ' ',
  "AAABDRCB" varchar(1) NOT NULL default ' ',
  "AAABDRCC" varchar(1) NOT NULL default ' ',
  "AAABDRCD" varchar(1) NOT NULL default ' ',
  "AAABDRCE" varchar(1) NOT NULL default ' ',
  "AAABDRCF" varchar(1) NOT NULL default ' ',
  "AAABDRCG" varchar(1) NOT NULL default ' ',
  "AAABDRCH" varchar(1) NOT NULL default ' ',
  "AAABDRCI" varchar(1) NOT NULL default ' ',
  "AAABDRCJ" varchar(1) NOT NULL default ' ',
  "AAABDRCK" varchar(1) NOT NULL default ' ',
  "AAABDRCL" varchar(1) NOT NULL default ' ',
  "AAABDRCM" varchar(1) NOT NULL default ' ',
  "AAABDRCN" varchar(1) NOT NULL default ' ',
  "AAABDRCO" varchar(1) NOT NULL default ' ',
  "AAABDRCP" varchar(1) NOT NULL default ' ',
  "AAABDRCQ" varchar(1) NOT NULL default ' ',
  "AAABDRCR" varchar(1) NOT NULL default ' ',
  "AAABDRCS" varchar(1) NOT NULL default ' ',
  "AAABDRCT" varchar(1) NOT NULL default ' ',
  "AAABDRCU" varchar(1) NOT NULL default ' ',
  "AAABDRCV" varchar(1) NOT NULL default ' ',
  "AAABDRCW" varchar(1) NOT NULL default ' ',
  "AAABDRCX" varchar(1) NOT NULL default ' ',
  "AAABDRCY" varchar(1) NOT NULL default ' ',
  "AAABDRCZ" varchar(1) NOT NULL default ' ',
  "AAABDRDA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRDD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUVS" varchar(6) NOT NULL default ' ',
  "AAABDRDH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRDL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDREA" varchar(6) NOT NULL default ' ',
  "AAABDRDO" varchar(6) NOT NULL default ' ',
  "AAABDRDP" varchar(6) NOT NULL default ' ',
  "AAABDRDQ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDREA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRDT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDREA" varchar(6) NOT NULL default ' ',
  "AAABDRDX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDREA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDREB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKQPE" varchar(2) NOT NULL default '00',
  "AAABDREL" varchar(6) NOT NULL default ' ',
  "AAABJUQY" varchar(4) NOT NULL default '0000',
  "AAABGIKF" varchar(40) NOT NULL default ' ',
  "AAABMNLX" varchar(2) NOT NULL default ' ',
  "AAABMNLY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKQPE","AAABDREL","AAABJUQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDREN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABJHFD" varchar(1) NOT NULL default ' ',
  "AAABMOIB" varchar(14) NOT NULL default ' ',
  "AAABDRET" varchar(1) NOT NULL default ' ',
  "AAABDREU" varchar(1) NOT NULL default ' ',
  "AAABDREV" varchar(6) NOT NULL default ' ',
  "AAABDREW" varchar(1) NOT NULL default ' ',
  "AAABDREX" varchar(1) NOT NULL default ' ',
  "AAABDREY" varchar(1) NOT NULL default ' ',
  "AAABDREZ" varchar(6) NOT NULL default ' ',
  "AAABDRFA" varchar(1) NOT NULL default ' ',
  "AAABDRFB" varchar(1) NOT NULL default ' ',
  "AAABDRFC" varchar(1) NOT NULL default ' ',
  "AAABDRFD" varchar(6) NOT NULL default ' ',
  "AAABDRFE" varchar(1) NOT NULL default ' ',
  "AAABDRFF" varchar(1) NOT NULL default ' ',
  "AAABDRFG" varchar(1) NOT NULL default ' ',
  "AAABDRFH" varchar(6) NOT NULL default ' ',
  "AAABDRFI" varchar(1) NOT NULL default ' ',
  "AAABDRFJ" varchar(1) NOT NULL default ' ',
  "AAABDRFK" varchar(1) NOT NULL default ' ',
  "AAABDRFL" varchar(6) NOT NULL default ' ',
  "AAABDRFM" varchar(1) NOT NULL default ' ',
  "AAABDRFN" varchar(1) NOT NULL default ' ',
  "AAABDRFO" varchar(1) NOT NULL default ' ',
  "AAABDRFP" varchar(6) NOT NULL default ' ',
  "AAABDRFQ" varchar(1) NOT NULL default ' ',
  "AAABDRFR" varchar(1) NOT NULL default ' ',
  "AAABDRFS" varchar(1) NOT NULL default ' ',
  "AAABDRFT" varchar(6) NOT NULL default ' ',
  "AAABDRFU" varchar(1) NOT NULL default ' ',
  "AAABDRFV" varchar(1) NOT NULL default ' ',
  "AAABDRFW" varchar(1) NOT NULL default ' ',
  "AAABDRFX" varchar(6) NOT NULL default ' ',
  "AAABDRFY" varchar(1) NOT NULL default ' ',
  "AAABDRFZ" varchar(1) NOT NULL default ' ',
  "AAABDRGA" varchar(1) NOT NULL default ' ',
  "AAABDRGB" varchar(6) NOT NULL default ' ',
  "AAABDRGC" varchar(1) NOT NULL default ' ',
  "AAABDRGD" varchar(1) NOT NULL default ' ',
  "AAABDRGE" varchar(1) NOT NULL default ' ',
  "AAABDRGF" varchar(6) NOT NULL default ' ',
  "AAABDRGG" varchar(1) NOT NULL default ' ',
  "AAABDRGH" varchar(1) NOT NULL default ' ',
  "AAABDRGI" varchar(1) NOT NULL default ' ',
  "AAABDRGJ" varchar(6) NOT NULL default ' ',
  "AAABDRGK" varchar(1) NOT NULL default ' ',
  "AAABDRGL" varchar(1) NOT NULL default ' ',
  "AAABDRGM" varchar(1) NOT NULL default ' ',
  "AAABDRGN" varchar(6) NOT NULL default ' ',
  "AAABDRGO" varchar(1) NOT NULL default ' ',
  "AAABDRGP" varchar(1) NOT NULL default ' ',
  "AAABDRGQ" varchar(1) NOT NULL default ' ',
  "AAABDRGR" varchar(6) NOT NULL default ' ',
  "AAABDRGS" varchar(1) NOT NULL default ' ',
  "AAABDRGT" varchar(1) NOT NULL default ' ',
  "AAABDRGU" varchar(1) NOT NULL default ' ',
  "AAABDRGV" varchar(6) NOT NULL default ' ',
  "AAABDRGW" varchar(1) NOT NULL default ' ',
  "AAABDRGX" varchar(1) NOT NULL default ' ',
  "AAABDRGY" varchar(1) NOT NULL default ' ',
  "AAABDRGZ" varchar(6) NOT NULL default ' ',
  "AAABDRHA" varchar(1) NOT NULL default ' ',
  "AAABDRHB" varchar(1) NOT NULL default ' ',
  "AAABDRHC" varchar(1) NOT NULL default ' ',
  "AAABDRHD" varchar(6) NOT NULL default ' ',
  "AAABDRHE" varchar(1) NOT NULL default ' ',
  "AAABDRHF" varchar(1) NOT NULL default ' ',
  "AAABDRHG" varchar(1) NOT NULL default ' ',
  "AAABDRHH" varchar(6) NOT NULL default ' ',
  "AAABDRHI" varchar(1) NOT NULL default ' ',
  "AAABDRHJ" varchar(1) NOT NULL default ' ',
  "AAABDRHK" varchar(1) NOT NULL default ' ',
  "AAABDRHL" varchar(6) NOT NULL default ' ',
  "AAABDRHM" varchar(1) NOT NULL default ' ',
  "AAABDRHN" varchar(1) NOT NULL default ' ',
  "AAABDRHO" varchar(1) NOT NULL default ' ',
  "AAABDRHP" varchar(6) NOT NULL default ' ',
  "AAABDRHQ" varchar(1) NOT NULL default ' ',
  "AAABDRHR" varchar(1) NOT NULL default ' ',
  "AAABDRHS" varchar(1) NOT NULL default ' ',
  "AAABDRHT" varchar(6) NOT NULL default ' ',
  "AAABDRHU" varchar(1) NOT NULL default ' ',
  "AAABDRHV" varchar(1) NOT NULL default ' ',
  "AAABDRHW" varchar(1) NOT NULL default ' ',
  "AAABDRHX" varchar(6) NOT NULL default ' ',
  "AAABDRHY" varchar(1) NOT NULL default ' ',
  "AAABDRHZ" varchar(1) NOT NULL default ' ',
  "AAABDRIA" varchar(1) NOT NULL default ' ',
  "AAABDRIB" varchar(6) NOT NULL default ' ',
  "AAABDRIC" varchar(1) NOT NULL default ' ',
  "AAABDRID" varchar(1) NOT NULL default ' ',
  "AAABDRIE" varchar(1) NOT NULL default ' ',
  "AAABDRIF" varchar(6) NOT NULL default ' ',
  "AAABDRIG" varchar(1) NOT NULL default ' ',
  "AAABDRIH" varchar(1) NOT NULL default ' ',
  "AAABDRII" varchar(1) NOT NULL default ' ',
  "AAABDRIJ" varchar(6) NOT NULL default ' ',
  "AAABDRIK" varchar(1) NOT NULL default ' ',
  "AAABDRIL" varchar(1) NOT NULL default ' ',
  "AAABDRIM" varchar(1) NOT NULL default ' ',
  "AAABDRIN" varchar(6) NOT NULL default ' ',
  "AAABDRIO" varchar(1) NOT NULL default ' ',
  "AAABDRIP" varchar(1) NOT NULL default ' ',
  "AAABDRIQ" varchar(1) NOT NULL default ' ',
  "AAABDRIR" varchar(6) NOT NULL default ' ',
  "AAABDRIS" varchar(1) NOT NULL default ' ',
  "AAABDRIT" varchar(1) NOT NULL default ' ',
  "AAABDRIU" varchar(1) NOT NULL default ' ',
  "AAABDRIV" varchar(6) NOT NULL default ' ',
  "AAABDRIW" varchar(1) NOT NULL default ' ',
  "AAABDRIX" varchar(1) NOT NULL default ' ',
  "AAABDRIY" varchar(1) NOT NULL default ' ',
  "AAABDRIZ" varchar(6) NOT NULL default ' ',
  "AAABDRJA" varchar(1) NOT NULL default ' ',
  "AAABDRJB" varchar(1) NOT NULL default ' ',
  "AAABDRJC" varchar(1) NOT NULL default ' ',
  "AAABDRJD" varchar(6) NOT NULL default ' ',
  "AAABDRJE" varchar(1) NOT NULL default ' ',
  "AAABDRJF" varchar(1) NOT NULL default ' ',
  "AAABDRJG" varchar(1) NOT NULL default ' ',
  "AAABDRJH" varchar(6) NOT NULL default ' ',
  "AAABDRJI" varchar(1) NOT NULL default ' ',
  "AAABDRJJ" varchar(1) NOT NULL default ' ',
  "AAABDRJK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKNSU" varchar(6) NOT NULL default ' ',
  "AAABDRKY" varchar(1) NOT NULL default ' ',
  "AAABDRJS" varchar(6) NOT NULL default ' ',
  "AAABDRJT" varchar(6) NOT NULL default ' ',
  "AAABDRJU" varchar(6) NOT NULL default ' ',
  "AAABDRJV" varchar(6) NOT NULL default ' ',
  "AAABDRJW" varchar(6) NOT NULL default ' ',
  "AAABDRJX" varchar(6) NOT NULL default ' ',
  "AAABDRSI" varchar(1) NOT NULL default ' ',
  "AAABDXBA" varchar(1) NOT NULL default ' ',
  "AAABDXBB" varchar(1) NOT NULL default ' ',
  "AAABDXBC" varchar(1) NOT NULL default ' ',
  "AAABDXBD" varchar(1) NOT NULL default ' ',
  "AAABDXBE" varchar(2) NOT NULL default '00',
  "AAABDXBF" varchar(2) NOT NULL default '00',
  "AAABDXBG" varchar(14) NOT NULL default ' ',
  "AAABFTXT" varchar(7) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABJHFD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAABKNSU","AAABDRKY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRKN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKNSU" varchar(6) NOT NULL default ' ',
  "AAABDRKY" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREL","AAABKNSU","AAABDRKY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRKZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABDRND" varchar(12) NOT NULL default ' ',
  "AAABDRLD" varchar(12) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABJHFD" varchar(1) NOT NULL default ' ',
  "AAABDXHF" varchar(1) NOT NULL default ' ',
  "AAABDRLH" varchar(3) NOT NULL default '000',
  "AAABDRLI" varchar(1) NOT NULL default ' ',
  "AAABDRLJ" varchar(2) NOT NULL default '00',
  "AAABDRLK" varchar(1) NOT NULL default ' ',
  "AAABDRLL" decimal(5,2) NOT NULL default '0.00',
  "AAABDRLM" varchar(3) NOT NULL default '000',
  "AAABDRLN" varchar(3) NOT NULL default ' ',
  "AAABDRLO" varchar(3) NOT NULL default '000',
  "AAABDRLP" varchar(3) NOT NULL default ' ',
  "AAABDRLQ" varchar(1) NOT NULL default ' ',
  "AAABLROG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRLU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKNSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRMC" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRMF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABDRMR" varchar(12) NOT NULL default ' ',
  "AAABFYCS" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMLMY" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABJHFD" varchar(1) NOT NULL default ' ',
  "AAABDXHF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAABDRMR","AAABFYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRMT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABDRND" varchar(12) NOT NULL default ' ',
  "AAABFYCS" varchar(3) NOT NULL default '000',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREL","AAABDRND","AAABFYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRNF" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRNL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKNSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRNT" (
  "AAABDTXK" varchar(3) NOT NULL default ' ',
  "AAABDUFH" varchar(4) NOT NULL default ' ',
  "AAABDUFI" varchar(4) NOT NULL default ' ',
  "AAABDRNX" varchar(1) NOT NULL default ' ',
  "AAABDRNY" varchar(1) NOT NULL default ' ',
  "AAABDRNZ" varchar(1) NOT NULL default ' ',
  "AAABDROA" varchar(1) NOT NULL default ' ',
  "AAABDROB" varchar(1) NOT NULL default ' ',
  "AAABDROC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDTXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDROE" (
  "AAABLOZE" varchar(4) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default '0000',
  "AAABMAZB" varchar(1) NOT NULL default '0',
  "AAABLQTM" varchar(1) NOT NULL default ' ',
  "AAABDROJ" varchar(1) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAABMGHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDROQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJLIY" varchar(30) NOT NULL default ' ',
  "AAABDROT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJLIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDROW" (
  "AAABLOZE" varchar(4) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJFBB" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAABMGHE","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRPE" (
  "AAABLOZE" varchar(4) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default '0000',
  "AAABJLIY" varchar(30) NOT NULL default ' ',
  "AAABDRPI" varchar(2) NOT NULL default '00',
  "AAABLPWW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAABMGHE","AAABJLIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRPN" (
  "AAABJLIY" varchar(30) NOT NULL default ' ',
  "AAABDRPP" varchar(30) NOT NULL default ' ',
  "AAABDRPQ" varchar(30) NOT NULL default ' ',
  "AAABDRPR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABJLIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRPT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABDRQB" varchar(1) NOT NULL default ' ',
  "AAABDRQC" varchar(50) NOT NULL default ' ',
  "AAABKCWA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTUX","AAABDRQB","AAABDRQC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRQD" (
  "AAABJLIY" varchar(30) NOT NULL default ' ',
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  "AAABMRFW" varchar(3) NOT NULL default '000',
  "AAABMDCZ" varchar(3) NOT NULL default ' ',
  "AAABIVOQ" varchar(1) NOT NULL default ' ',
  "AAABLHPU" varchar(1) NOT NULL default ' ',
  "AAABJDAC" int(11) NOT NULL default '0',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABMRFO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABJLIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRQU" (
  "AAABDTXK" varchar(3) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHIOX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABDTXK","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRRA" (
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABMLCO" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABMDCZ" varchar(3) NOT NULL default ' ',
  "AAABMDDF" varchar(3) NOT NULL default ' ',
  "AAABMDDL" varchar(3) NOT NULL default ' ',
  "AAABMDDR" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKXW","AAABMLCO","AAABMGHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRRL" (
  "AAABKQPE" varchar(2) NOT NULL default '00',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABKQPE","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRRS" (
  "AAABILHK" varchar(4) NOT NULL default ' ',
  "AAABDRRY" varchar(1) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABLQJP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABILHK","AAABDRRY","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRSA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABDXAW" varchar(14) NOT NULL default ' ',
  "AAABDXAX" varchar(12) NOT NULL default ' ',
  "AAABDXAY" varchar(12) NOT NULL default ' ',
  "AAABDXAZ" varchar(12) NOT NULL default ' ',
  "AAABDRSI" varchar(1) NOT NULL default ' ',
  "AAABDXBA" varchar(1) NOT NULL default ' ',
  "AAABDXBB" varchar(1) NOT NULL default ' ',
  "AAABDXBC" varchar(1) NOT NULL default ' ',
  "AAABDXBD" varchar(1) NOT NULL default ' ',
  "AAABDXBE" varchar(2) NOT NULL default '00',
  "AAABDXBF" varchar(2) NOT NULL default '00',
  "AAABDXBG" varchar(14) NOT NULL default ' ',
  "AAABFTXT" varchar(7) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABJHFD" varchar(1) NOT NULL default ' ',
  "AAABDXHF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAABKNSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRSX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABFYCS" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMLMY" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABJHFD" varchar(1) NOT NULL default ' ',
  "AAABDXHF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAABKNSU","AAABFYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRTL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABFYCS" varchar(3) NOT NULL default '000',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREL","AAABKNSU","AAABFYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRTX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABFYCS" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABJHFD" varchar(1) NOT NULL default ' ',
  "AAABDXHF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAABKNSU","AAABFYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRUK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABFYCS" varchar(3) NOT NULL default '000',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREL","AAABKNSU","AAABFYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRUW" (
  "AAABDRVP" varchar(2) NOT NULL default '00',
  "AAABKQPE" varchar(2) NOT NULL default '00',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLENG" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABDRVP","AAABKQPE","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRVE" (
  "AAABDRVP" varchar(2) NOT NULL default '00',
  "AAABKQPE" varchar(2) NOT NULL default '00',
  "AAABDRVH" varchar(40) NOT NULL default ' ',
  "AAABDRVI" varchar(4) NOT NULL default '0000',
  "AAABDRVJ" varchar(30) NOT NULL default ' ',
  "AAABDRVK" varchar(40) NOT NULL default ' ',
  "AAABDRVL" varchar(30) NOT NULL default ' ',
  "AAABDRVM" varchar(40) NOT NULL default ' ',
  "AAABDRVN" varchar(4) NOT NULL default '0000',
  "AAABDRVO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABDRVP","AAABKQPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRVR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABFYCS" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABJHFD" varchar(1) NOT NULL default ' ',
  "AAABDXHF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAABKNSU","AAABFYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRWE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABFYCS" varchar(3) NOT NULL default '000',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREL","AAABKNSU","AAABFYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRWQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREL","AAABKNSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRXA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDRXH" varchar(12) NOT NULL default ' ',
  "AAABDRXD" varchar(1) NOT NULL default ' ',
  "AAABDRXE" varchar(1) NOT NULL default ' ',
  "AAABIJYD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDRXH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRXI" (
  "AAABLHPM" varchar(4) NOT NULL default ' ',
  "AAABDRXK" varchar(4) NOT NULL default ' ',
  "AAABDRXL" varchar(2) NOT NULL default '00',
  "AAABDRXM" varchar(30) NOT NULL default ' ',
  "AAABDRXN" varchar(30) NOT NULL default ' ',
  "AAABDRXO" varchar(30) NOT NULL default ' ',
  "AAABDRXP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRXR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKQPE" varchar(2) NOT NULL default '00',
  "AAABDRXU" varchar(4) NOT NULL default ' ',
  "AAABDRXV" varchar(1) NOT NULL default ' ',
  "AAABEDUT" varchar(1) NOT NULL default ' ',
  "AAABDRXX" varchar(1) NOT NULL default ' ',
  "AAABDRXY" varchar(1) NOT NULL default ' ',
  "AAABDRXZ" varchar(1) NOT NULL default ' ',
  "AAABDRYA" varchar(1) NOT NULL default ' ',
  "AAABDRYB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKQPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRYE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLXR" varchar(50) NOT NULL default ' ',
  "AAABHZUM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLXR","AAABHZUM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRYL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRYQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLPX" varchar(40) NOT NULL default ' ',
  "AAABLAUS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRYY" (
  "AAABMMZS" varchar(20) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMMZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRZD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABDRZK" varchar(2) NOT NULL default '00',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLHQ","AAABDRZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDRZL" (
  "AAABDRZX" varchar(30) NOT NULL default ' ',
  "AAABDRZY" varchar(30) NOT NULL default ' ',
  "AAABDRZZ" varchar(30) NOT NULL default ' ',
  "AAABDRZP" varchar(30) NOT NULL default ' ',
  "AAABMJWH" varchar(30) NOT NULL default ' ',
  "AAABDRZR" varchar(30) NOT NULL default ' ',
  "AAABDRZS" varchar(30) NOT NULL default ' ',
  "AAABDRZT" varchar(30) NOT NULL default ' ',
  "AAABDRZU" varchar(1) NOT NULL default '0',
  "AAABLOKS" varchar(1) NOT NULL default ' ',
  "AAABHDIY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDRZX","AAABDRZY","AAABDRZZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSAA" (
  "AAABDSAH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABDSAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSAD" (
  "AAABDSAH" varchar(4) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABDSAG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABDSAH","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSAJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAABDSAM" varchar(12) NOT NULL default ' ',
  "AAABGHUE" varchar(8) NOT NULL default ' ',
  "AAABDSAO" varchar(1) NOT NULL default ' ',
  "AAABDSAP" varchar(1) NOT NULL default ' ',
  "AAABDSAQ" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSOG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSAT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAABDSDB" varchar(20) NOT NULL default ' ',
  "AAABDSEY" varchar(20) NOT NULL default ' ',
  "AAABDSAY" varchar(1) NOT NULL default ' ',
  "AAABDSAZ" varchar(1) NOT NULL default ' ',
  "AAABDSBA" varchar(1) NOT NULL default ' ',
  "AAABDSBB" smallint(6) NOT NULL default '0',
  "AAABDSBC" smallint(6) NOT NULL default '0',
  "AAABDSBD" varchar(1) NOT NULL default ' ',
  "AAABDSBE" varchar(1) NOT NULL default ' ',
  "AAABDSBF" varchar(1) NOT NULL default ' ',
  "AAABDSBG" varchar(1) NOT NULL default ' ',
  "AAABDSBH" varchar(1) NOT NULL default ' ',
  "AAABDSBI" varchar(1) NOT NULL default ' ',
  "AAABDSBJ" varchar(1) NOT NULL default ' ',
  "AAABDSBK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSOG","AAABDSDB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSBO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAABDSDB" varchar(20) NOT NULL default ' ',
  "AAABLKWV" varchar(4) NOT NULL default '0000',
  "AAABDSBT" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSOG","AAABDSDB","AAABLKWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSBY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAABDSDB" varchar(20) NOT NULL default ' ',
  "AAABLKWV" varchar(4) NOT NULL default '0000',
  "AAABDSCD" VARBINARY(255),
  "AAABDSCE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSOG","AAABDSDB","AAABLKWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSCJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAABDSDB" varchar(20) NOT NULL default ' ',
  "AAABLKWV" varchar(4) NOT NULL default '0000',
  "AAABDSCO" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSOG","AAABDSDB","AAABLKWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSCT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAABDSDB" varchar(20) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABDSCY" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSOG","AAABDSDB","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSDD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAABDSDY" varchar(20) NOT NULL default ' ',
  "AAABDSEY" varchar(20) NOT NULL default ' ',
  "AAABIMHK" varchar(255) NOT NULL default ' ',
  "AAABDSEK" decimal(4,0) NOT NULL default '0',
  "AAABHVRR" varchar(5) NOT NULL default ' ',
  "AAABDSDL" tinyint(3) unsigned NOT NULL default '0',
  "AAABLUZL" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLSOG","AAABDSDY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSDQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAABDSDY" varchar(20) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABIMHG" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSOG","AAABDSDY","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSEA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAABDSEY" varchar(20) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABHWXN" varchar(128) NOT NULL default ' ',
  "AAABDSEG" varchar(1) NOT NULL default ' ',
  "AAABDSEH" varchar(1) NOT NULL default ' ',
  "AAABDSEI" varchar(1) NOT NULL default ' ',
  "AAABDSEJ" int(11) NOT NULL default '0',
  "AAABDSEK" int(11) NOT NULL default '0',
  "AAABDSEL" tinyint(3) unsigned NOT NULL default '0',
  "AAABDSEM" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLSOG","AAABDSEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAABDSEY" varchar(20) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABDSEV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSOG","AAABDSEY","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSFA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABDSHO" varchar(2) NOT NULL default ' ',
  "AAABKNSU" varchar(6) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAABDSHO","AAABKNSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSFL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABGYLC" varchar(10) NOT NULL default ' ',
  "AAABDSFP" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABGYLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSFT" (
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABMLPV" varchar(32) NOT NULL default ' ',
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  "AAABDSFX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSFZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABDSHO" varchar(2) NOT NULL default ' ',
  "AAABKNSU" varchar(6) NOT NULL default ' ',
  "AAABFYCS" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMLMY" varchar(6) NOT NULL default '000000',
  "AAABGLYJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAABDSHO","AAABKNSU","AAABFYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSGN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABDSHO" varchar(2) NOT NULL default ' ',
  "AAABKNSU" varchar(6) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMLPO" varchar(1) NOT NULL default '0',
  "AAABDSGU" varchar(1) NOT NULL default ' ',
  "AAABDSGV" varchar(2) NOT NULL default ' ',
  "AAABDSGW" varchar(45) NOT NULL default ' ',
  "AAABDSGX" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAABDSHO","AAABKNSU","AAABMREF","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABDSHO" varchar(2) NOT NULL default ' ',
  "AAABKNSU" varchar(6) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREL","AAABDSHO","AAABKNSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSHQ" (
  "AAABKQPE" varchar(2) NOT NULL default '00',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABDSHY" varchar(1) NOT NULL default '0',
  "AAABJYTM" varchar(30) NOT NULL default ' ',
  "AAABDSHV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABKQPE","AAABKAFO","AAABDSHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABDSID" varchar(1) NOT NULL default ' ',
  "AAABDSIE" varchar(1) NOT NULL default ' ',
  "AAABDSML" varchar(1) NOT NULL default ' ',
  "AAABDSMK" varchar(1) NOT NULL default ' ',
  "AAABDSIH" varchar(1) NOT NULL default ' ',
  "AAABDSII" varchar(1) NOT NULL default ' ',
  "AAABDSIJ" varchar(1) NOT NULL default ' ',
  "AAABDSIK" varchar(1) NOT NULL default ' ',
  "AAABDSIL" varchar(1) NOT NULL default ' ',
  "AAABDSIM" varchar(1) NOT NULL default ' ',
  "AAABDSIN" varchar(1) NOT NULL default ' ',
  "AAABDSIO" varchar(1) NOT NULL default ' ',
  "AAABDSIP" varchar(1) NOT NULL default ' ',
  "AAABDSIQ" varchar(1) NOT NULL default ' ',
  "AAABDSIR" varchar(1) NOT NULL default ' ',
  "AAABDSIS" varchar(1) NOT NULL default ' ',
  "AAABDSIT" varchar(1) NOT NULL default ' ',
  "AAABDSIU" varchar(1) NOT NULL default ' ',
  "AAABDSIV" varchar(1) NOT NULL default ' ',
  "AAABDSIW" varchar(1) NOT NULL default ' ',
  "AAABDSIX" varchar(1) NOT NULL default ' ',
  "AAABDSIY" varchar(1) NOT NULL default ' ',
  "AAABKNUR" varchar(3) NOT NULL default '000',
  "AAABDSJA" varchar(3) NOT NULL default '000',
  "AAABDSJB" varchar(1) NOT NULL default ' ',
  "AAABDSJC" varchar(1) NOT NULL default ' ',
  "AAABDSJD" varchar(1) NOT NULL default ' ',
  "AAABDSJE" varchar(1) NOT NULL default ' ',
  "AAABDSJF" varchar(1) NOT NULL default ' ',
  "AAABDSJG" varchar(1) NOT NULL default ' ',
  "AAABDSJH" varchar(1) NOT NULL default ' ',
  "AAABDSJI" varchar(1) NOT NULL default ' ',
  "AAABDSJJ" varchar(1) NOT NULL default ' ',
  "AAABDSJK" varchar(1) NOT NULL default ' ',
  "AAABHNLR" varchar(4) NOT NULL default ' ',
  "AAABDSJM" varchar(1) NOT NULL default ' ',
  "AAABDSJN" varchar(1) NOT NULL default ' ',
  "AAABDSJO" varchar(1) NOT NULL default ' ',
  "AAABDSJP" varchar(1) NOT NULL default ' ',
  "AAABDSJQ" varchar(1) NOT NULL default ' ',
  "AAABDSJR" varchar(1) NOT NULL default ' ',
  "AAABDSJS" varchar(1) NOT NULL default ' ',
  "AAABDSJT" varchar(1) NOT NULL default ' ',
  "AAABDSJU" varchar(1) NOT NULL default ' ',
  "AAABDSJV" varchar(1) NOT NULL default ' ',
  "AAABDSJW" varchar(8) NOT NULL default '00000000',
  "AAABDSJX" varchar(1) NOT NULL default ' ',
  "AAABDSJY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABMLHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSKC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKQPE" varchar(2) NOT NULL default '00',
  "AAABLUVS" varchar(18) NOT NULL default ' ',
  "AAABHEVN" varchar(1) NOT NULL default ' ',
  "AAABDSNQ" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  "AAABDSKK" varchar(20) NOT NULL default ' ',
  "AAABDSKL" varchar(20) NOT NULL default ' ',
  "AAABDSKM" varchar(20) NOT NULL default ' ',
  "AAABDSKN" varchar(20) NOT NULL default ' ',
  "AAABDSKO" varchar(20) NOT NULL default ' ',
  "AAABDSKP" varchar(20) NOT NULL default ' ',
  "AAABDSKQ" varchar(20) NOT NULL default ' ',
  "AAABDSKR" varchar(20) NOT NULL default ' ',
  "AAABDSKS" varchar(20) NOT NULL default ' ',
  "AAABDSKT" varchar(20) NOT NULL default ' ',
  "AAABDSKU" varchar(20) NOT NULL default ' ',
  "AAABDSKV" varchar(20) NOT NULL default ' ',
  "AAABDSKW" varchar(20) NOT NULL default ' ',
  "AAABDSKX" varchar(20) NOT NULL default ' ',
  "AAABDSKY" varchar(20) NOT NULL default ' ',
  "AAABDSKZ" varchar(20) NOT NULL default ' ',
  "AAABDSLA" varchar(20) NOT NULL default ' ',
  "AAABDSLB" varchar(20) NOT NULL default ' ',
  "AAABDSLC" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKQPE","AAABLUVS","AAABHEVN","AAABDSNQ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSLJ" (
  "AAABKQPE" varchar(2) NOT NULL default '00',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABKQPE","AAABMGHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSLO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKQPE" varchar(2) NOT NULL default '00',
  "AAABLUVS" varchar(18) NOT NULL default ' ',
  "AAABHEVN" varchar(1) NOT NULL default ' ',
  "AAABDSNQ" varchar(1) NOT NULL default ' ',
  "AAABDSLU" varchar(1) NOT NULL default ' ',
  "AAABHEVE" varchar(4) NOT NULL default ' ',
  "AAABHEVF" varchar(4) NOT NULL default ' ',
  "AAABHEVG" varchar(4) NOT NULL default ' ',
  "AAABHEVH" varchar(4) NOT NULL default ' ',
  "AAABHEVI" varchar(4) NOT NULL default ' ',
  "AAABHEVJ" varchar(4) NOT NULL default ' ',
  "AAABHEVK" varchar(4) NOT NULL default ' ',
  "AAABHEVL" varchar(4) NOT NULL default ' ',
  "AAABDSMD" varchar(1) NOT NULL default ' ',
  "AAABDSME" varchar(1) NOT NULL default ' ',
  "AAABDSMF" varchar(1) NOT NULL default ' ',
  "AAABDSMG" varchar(1) NOT NULL default ' ',
  "AAABDSMH" varchar(1) NOT NULL default ' ',
  "AAABDSMI" varchar(1) NOT NULL default ' ',
  "AAABDSMJ" varchar(1) NOT NULL default ' ',
  "AAABDSMK" varchar(1) NOT NULL default ' ',
  "AAABDSML" varchar(1) NOT NULL default ' ',
  "AAABDSMM" varchar(1) NOT NULL default ' ',
  "AAABDSMN" varchar(1) NOT NULL default ' ',
  "AAABDSMO" varchar(1) NOT NULL default ' ',
  "AAABDSMP" varchar(1) NOT NULL default ' ',
  "AAABDSMQ" varchar(1) NOT NULL default ' ',
  "AAABDSMR" varchar(1) NOT NULL default ' ',
  "AAABDSMS" varchar(1) NOT NULL default ' ',
  "AAABDSMT" varchar(1) NOT NULL default ' ',
  "AAABDSMU" varchar(1) NOT NULL default ' ',
  "AAABDSMV" varchar(1) NOT NULL default ' ',
  "AAABDSMW" varchar(1) NOT NULL default ' ',
  "AAABDSMX" varchar(1) NOT NULL default ' ',
  "AAABDSMY" varchar(1) NOT NULL default ' ',
  "AAABDSMZ" varchar(1) NOT NULL default ' ',
  "AAABDSNA" varchar(1) NOT NULL default ' ',
  "AAABDSNB" varchar(1) NOT NULL default ' ',
  "AAABDSNC" varchar(1) NOT NULL default ' ',
  "AAABDSND" varchar(1) NOT NULL default ' ',
  "AAABDSNE" varchar(1) NOT NULL default ' ',
  "AAABDSNF" varchar(1) NOT NULL default ' ',
  "AAABDSNG" varchar(1) NOT NULL default ' ',
  "AAABDSNH" varchar(1) NOT NULL default ' ',
  "AAABDSNI" varchar(1) NOT NULL default ' ',
  "AAABDSNJ" varchar(1) NOT NULL default ' ',
  "AAABDSNK" varchar(1) NOT NULL default ' ',
  "AAABDSNL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKQPE","AAABLUVS","AAABHEVN","AAABDSNQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSNR" (
  "AAABKQPE" varchar(2) NOT NULL default '00',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABKQPE","AAABMGHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSNW" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLDNU" varchar(4) NOT NULL default ' ',
  "AAABDSOA" varchar(1) NOT NULL default ' ',
  "AAABDSOB" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSOE" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLAUS" varchar(55) NOT NULL default ' ',
  "AAABKGYO" varchar(10) NOT NULL default ' ',
  "AAABKGYP" varchar(20) NOT NULL default ' ',
  "AAABKGYQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSOP" (
  "AAABDSRF" varchar(4) NOT NULL default '0000',
  "AAABDSOR" varchar(3) NOT NULL default ' ',
  "AAABDSOS" varchar(2) NOT NULL default '00',
  "AAABDSOT" varchar(2) NOT NULL default '00',
  "AAABDSOU" varchar(2) NOT NULL default '00',
  "AAABDSOV" varchar(5) NOT NULL default '00000',
  "AAABDSOW" varchar(50) NOT NULL default ' ',
  "AAABDSOX" varchar(5) NOT NULL default '00000',
  "AAABDSOY" varchar(4) NOT NULL default '0000',
  "AAABDSOZ" varchar(4) NOT NULL default '0000',
  "AAABDSPA" varchar(2) NOT NULL default '00',
  "AAABDSPB" varchar(2) NOT NULL default '00',
  "AAABDSPC" varchar(1) NOT NULL default ' ',
  "AAABDSPD" varchar(4) NOT NULL default '0000',
  "AAABDSPE" varchar(4) NOT NULL default '0000',
  "AAABDSPF" varchar(4) NOT NULL default '0000',
  "AAABDSPG" varchar(4) NOT NULL default '0000',
  "AAABDSPH" varchar(1) NOT NULL default ' ',
  "AAABDSPI" varchar(1) NOT NULL default ' ',
  "AAABDSPJ" varchar(1) NOT NULL default ' ',
  "AAABDSPK" varchar(1) NOT NULL default ' ',
  "AAABDSPL" varchar(1) NOT NULL default ' ',
  "AAABDSPM" varchar(1) NOT NULL default ' ',
  "AAABDSZW" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABDSRF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSPP" (
  "AAABDSRF" varchar(4) NOT NULL default '0000',
  "AAABDSPV" varchar(1) NOT NULL default ' ',
  "AAABDSPW" varchar(4) NOT NULL default ' ',
  "AAABDSQP" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABDSRF","AAABDSPV","AAABDSPW"),
  KEY "AAABDSPX" ("AAABDSQP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSPZ" (
  "AAABDSQQ" varchar(5) NOT NULL default '00000',
  "AAABDSQB" varchar(30) NOT NULL default ' ',
  "AAABDTBM" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABDSQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSQE" (
  "AAABDSRF" varchar(4) NOT NULL default '0000',
  "AAABDSQQ" varchar(5) NOT NULL default '00000',
  "AAABDSQH" varchar(3) NOT NULL default '000',
  "AAABDSQI" varchar(252) NOT NULL default ' ',
  PRIMARY KEY ("AAABDSRF","AAABDSQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSQL" (
  "AAABDSQQ" varchar(5) NOT NULL default '00000',
  "AAABDSQR" varchar(1) NOT NULL default ' ',
  "AAABDSQS" varchar(1) NOT NULL default ' ',
  "AAABDSQP" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABDSQQ","AAABDSQR","AAABDSQS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSQT" (
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABDSRF" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMNJW","AAABDSRF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSQY" (
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABDSRF" varchar(4) NOT NULL default '0000',
  "AAABDSRB" varchar(1) NOT NULL default ' ',
  "AAABDSRC" varchar(3) NOT NULL default '000',
  "AAABDSRD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNJW","AAABDSRF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSRG" (
  "AAABDSRO" varchar(16) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABINIL" varchar(8) NOT NULL default ' ',
  "AAABGHVW" varchar(255) NOT NULL default ' ',
  "AAABDSRM" varchar(4) NOT NULL default '0000',
  "AAABDSRN" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABDSRO","AAABMLQA","AAABLEHK","AAABINIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSRS" (
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABDSRU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSRW" (
  "AAABDSSC" varchar(4) NOT NULL default '0000',
  "AAABDSSD" varchar(3) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDSSB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABDSSC","AAABDSSD","AAABMRFO","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSSG" (
  "AAABDSSV" varchar(2) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABINIL" varchar(8) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABDSSL" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABDSSV","AAABLEHK","AAABINIL","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSSQ" (
  "AAABDSSV" varchar(2) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABDSST" varchar(4) NOT NULL default '0000',
  "AAABDSSU" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABDSSV","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSSX" (
  "AAABMLPV" varchar(20) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABLUSG" varchar(14) NOT NULL default ' ',
  "AAABMMJT" varchar(240) NOT NULL default ' ',
  "AAABDSTC" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV","AAABLHGN","AAABLUSG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSTG" (
  "AAABDSTM" varchar(6) NOT NULL default ' ',
  "AAABDSTI" varchar(90) NOT NULL default ' ',
  "AAABDSTJ" varchar(1) NOT NULL default ' ',
  "AAABDSTK" varchar(1) NOT NULL default ' ',
  "AAABDSTL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDSTM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSTN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDSUC" varchar(4) NOT NULL default '0000',
  "AAABDSTQ" varchar(30) NOT NULL default ' ',
  "AAABDSTR" varchar(4) NOT NULL default ' ',
  "AAABDSTS" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDSUC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABDSUC" varchar(4) NOT NULL default '0000',
  "AAABDSTZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABDSUC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSUD" (
  "AAABDSZA" varchar(10) NOT NULL default ' ',
  "AAABMLPV" varchar(50) NOT NULL default ' ',
  "AAABDSYN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDSZA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSUI" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABDSUV" varchar(4) NOT NULL default '0000',
  "AAABDSUW" varchar(4) NOT NULL default '0000',
  "AAABDSUX" varchar(1) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABLITM" varchar(40) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABDSUV","AAABDSUW","AAABDSUX","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSUZ" (
  "AAABDSVC" varchar(4) NOT NULL default '0000',
  "AAABDSVB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABDSVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSVD" (
  "AAABLQGL" varchar(70) NOT NULL default ' ',
  "AAABDSVF" varchar(10) NOT NULL default ' ',
  "AAABHVRQ" varchar(30) NOT NULL default ' ',
  "AAABDSVH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSVJ" (
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLQGL" varchar(70) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFZK","AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSVP" (
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABLLLP" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSVT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDSYA" varchar(10) NOT NULL default ' ',
  "AAABDSVX" varchar(14) NOT NULL default ' ',
  "AAABDSVY" varchar(14) NOT NULL default ' ',
  "AAABDSVZ" varchar(10) NOT NULL default ' ',
  "AAABDSWA" varchar(18) NOT NULL default ' ',
  "AAABDSWB" varchar(18) NOT NULL default ' ',
  "AAABDSWC" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABDSYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSWG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDSWO" varchar(10) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABDSWL" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABDSWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSWP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDSYA" varchar(10) NOT NULL default ' ',
  "AAABLTIZ" varchar(14) NOT NULL default '00000000000000',
  "AAABMEGE" varchar(1) NOT NULL default '0',
  "AAABDSWV" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABDSYA","AAABLTIZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSXA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDSYA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDSYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSXF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDSYA" varchar(10) NOT NULL default ' ',
  "AAABLUDG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDSYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSXN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDSYA" varchar(10) NOT NULL default ' ',
  "AAABDSXR" varchar(64) NOT NULL default ' ',
  "AAABDSXS" varchar(64) NOT NULL default ' ',
  "AAABDSXT" varchar(64) NOT NULL default ' ',
  "AAABHMMH" varchar(64) NOT NULL default ' ',
  "AAABMNYT" varchar(64) NOT NULL default ' ',
  "AAABMNMY" varchar(1) NOT NULL default ' ',
  "AAABDSXX" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABDSYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSYB" (
  "AAABDSZW" varchar(5) NOT NULL default ' ',
  "AAABDTBN" varchar(1) NOT NULL default ' ',
  "AAABDTBO" varchar(4) NOT NULL default ' ',
  "AAABLISR" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABDSZW","AAABDTBN","AAABDTBO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSYJ" (
  "AAABDSZW" varchar(5) NOT NULL default ' ',
  "AAABMJXW" varchar(5) NOT NULL default ' ',
  "AAABDSYM" varchar(2) NOT NULL default '00',
  "AAABDSYN" varchar(1) NOT NULL default ' ',
  "AAABDSYO" varchar(1) NOT NULL default ' ',
  "AAABDSYP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDSZW","AAABMJXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSYS" (
  "AAABDSZB" varchar(6) NOT NULL default ' ',
  "AAABMLPV" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABDSZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSYW" (
  "AAABDSZW" varchar(5) NOT NULL default ' ',
  "AAABMNLA" varchar(1) NOT NULL default ' ',
  "AAABDSYZ" varchar(50) NOT NULL default ' ',
  "AAABDSZA" varchar(10) NOT NULL default ' ',
  "AAABDSZB" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABDSZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSZD" (
  "AAABDSZW" varchar(5) NOT NULL default ' ',
  "AAABDTBM" varchar(6) NOT NULL default ' ',
  "AAABDSZN" smallint(6) NOT NULL default '0',
  "AAABLVJB" VARBINARY(255),
  PRIMARY KEY ("AAABDSZW","AAABDTBM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSZK" (
  "AAABDSZW" varchar(5) NOT NULL default ' ',
  "AAABDTBM" varchar(6) NOT NULL default ' ',
  "AAABDSZN" smallint(6) NOT NULL default '0',
  "AAABLVJB" VARBINARY(255),
  PRIMARY KEY ("AAABDSZW","AAABDTBM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSZR" (
  "AAABDSZW" varchar(5) NOT NULL default ' ',
  "AAABDSZT" varchar(2) NOT NULL default '00',
  "AAABDSZU" varchar(20) NOT NULL default ' ',
  "AAABDSZV" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABDSZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDSZX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDTAQ" varchar(10) NOT NULL default ' ',
  "AAABLSBY" varchar(60) NOT NULL default ' ',
  "AAABDTAB" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABDTAQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTAE" (
  "AAABMNDU" varchar(40) NOT NULL default ' ',
  "AAABIZCH" varchar(8) NOT NULL default '00000000',
  "AAABGBTN" varchar(6) NOT NULL default '000000',
  "AAABDTAQ" varchar(6) NOT NULL default '000000',
  "AAABLUDG" varchar(72) NOT NULL default ' ',
  "AAABDTAK" varchar(10) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABDTAM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNDU","AAABIZCH","AAABGBTN","AAABDTAQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTAR" (
  "AAABDTBM" varchar(6) NOT NULL default ' ',
  "AAABDTAT" varchar(1) NOT NULL default ' ',
  "AAABDTAU" varchar(1) NOT NULL default ' ',
  "AAABDTAV" varchar(6) NOT NULL default ' ',
  "AAABDTAW" varchar(6) NOT NULL default ' ',
  "AAABDTAX" varchar(1) NOT NULL default ' ',
  "AAABDTAY" varchar(1) NOT NULL default ' ',
  "AAABDTAZ" varchar(1) NOT NULL default ' ',
  "AAABDTBA" varchar(1) NOT NULL default ' ',
  "AAABDTBB" varchar(1) NOT NULL default ' ',
  "AAABDTBC" varchar(1) NOT NULL default ' ',
  "AAABDTBD" varchar(1) NOT NULL default ' ',
  "AAABDTBE" varchar(1) NOT NULL default ' ',
  "AAABDTBF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDTBM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTBH" (
  "AAABDTBM" varchar(6) NOT NULL default ' ',
  "AAABDTBN" varchar(1) NOT NULL default ' ',
  "AAABDTBO" varchar(4) NOT NULL default ' ',
  "AAABLISR" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABDTBM","AAABDTBN","AAABDTBO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTBP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLXR" varchar(50) NOT NULL default ' ',
  "AAABHZUM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLXR","AAABHZUM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTBW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDTRC" varchar(40) NOT NULL default ' ',
  "AAABDTBZ" varchar(8) NOT NULL default '00000000',
  "AAABJFHC" varchar(1) NOT NULL default ' ',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABJDFW" varchar(2) NOT NULL default ' ',
  "AAABDTCD" varchar(1) NOT NULL default ' ',
  "AAABDTCE" varchar(2) NOT NULL default ' ',
  "AAABDTCF" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABDTRC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTCI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABHVDW" varchar(2) NOT NULL default ' ',
  "AAABGZKU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABMPLT","AAABHVDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTCS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDTCU" varchar(30) NOT NULL default ' ',
  "AAABDTCV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTCX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDTDD" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDTDB" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDTDD","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(10) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABDTDT" varchar(10) NOT NULL default ' ',
  "AAABDTDU" varchar(10) NOT NULL default ' ',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABHWBK" varchar(1) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABMMJT" decimal(15,0) NOT NULL default '0',
  "AAABDTDP" decimal(11,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMMVH","AAABDTDT","AAABDTDU","AAABMKIQ","AAABHWBK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTDX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQPP" varchar(4) NOT NULL default ' ',
  "AAABDTEA" varchar(2) NOT NULL default ' ',
  "AAABDTEB" varchar(30) NOT NULL default ' ',
  "AAABDTEC" varchar(30) NOT NULL default ' ',
  "AAABDTED" varchar(30) NOT NULL default ' ',
  "AAABDTEE" varchar(30) NOT NULL default ' ',
  "AAABDTEF" varchar(30) NOT NULL default ' ',
  "AAABDTEG" varchar(30) NOT NULL default ' ',
  "AAABDTEH" varchar(30) NOT NULL default ' ',
  "AAABDTEI" varchar(1) NOT NULL default ' ',
  "AAABDTEJ" varchar(3) NOT NULL default '000',
  "AAABDTEK" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABLQPP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTEN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLQPP" varchar(4) NOT NULL default ' ',
  "AAABDTER" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLQPP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTEV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABGMXF" varchar(4) NOT NULL default ' ',
  "AAABDTFE" varchar(4) NOT NULL default ' ',
  "AAABDTFA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOHM","AAABGMXF","AAABDTFE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTFF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDTFR" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDTFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTFK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDTFR" varchar(20) NOT NULL default ' ',
  "AAABDTFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDTFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTFS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDTGT" varchar(2) NOT NULL default ' ',
  "AAABKSQC" varchar(3) NOT NULL default ' ',
  "AAABDTFW" varchar(3) NOT NULL default ' ',
  "AAABKSOX" decimal(13,3) NOT NULL default '0.000',
  "AAABKNSA" varchar(2) NOT NULL default '00',
  "AAABDTHA" varchar(1) NOT NULL default ' ',
  "AAABDTGA" varchar(1) NOT NULL default ' ',
  "AAABDTGB" varchar(1) NOT NULL default ' ',
  "AAABDTGC" varchar(1) NOT NULL default ' ',
  "AAABDTGD" varchar(1) NOT NULL default ' ',
  "AAABDTGE" varchar(1) NOT NULL default ' ',
  "AAABKPPV" varchar(3) NOT NULL default ' ',
  "AAABDTGG" varchar(1) NOT NULL default ' ',
  "AAABDTGH" varchar(1) NOT NULL default ' ',
  "AAABDTGI" varchar(1) NOT NULL default ' ',
  "AAABDTGJ" varchar(20) NOT NULL default ' ',
  "AAABDTGK" varchar(1) NOT NULL default ' ',
  "AAABDTGL" varchar(1) NOT NULL default ' ',
  "AAABDTGM" varchar(1) NOT NULL default ' ',
  "AAABDTGN" varchar(1) NOT NULL default ' ',
  "AAABDTGO" varchar(1) NOT NULL default ' ',
  "AAABDTGP" varchar(1) NOT NULL default ' ',
  "AAABDTGQ" varchar(1) NOT NULL default ' ',
  "AAABDTGR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDTGT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTGU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXUJ" varchar(1) NOT NULL default ' ',
  "AAABKQVZ" varchar(1) NOT NULL default ' ',
  "AAABDTGY" varchar(1) NOT NULL default ' ',
  "AAABDTGZ" varchar(1) NOT NULL default ' ',
  "AAABDTHA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXUJ","AAABKQVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKQVZ" varchar(1) NOT NULL default ' ',
  "AAABKNSA" varchar(2) NOT NULL default '00',
  "AAABDTHR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKQVZ","AAABKNSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTHM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXUJ" varchar(1) NOT NULL default ' ',
  "AAABKQVZ" varchar(1) NOT NULL default ' ',
  "AAABKNSA" varchar(2) NOT NULL default '00',
  "AAABDTHR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXUJ","AAABKQVZ","AAABKNSA"),
  KEY "AAABDTHW" ("AAABMRIL","AAABKQVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKNLE" varchar(1) NOT NULL default ' ',
  "AAABDTID" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKNLE"),
  KEY "AAABDTIH" ("AAABMRIL","AAABKNLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTIK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKNLE" varchar(1) NOT NULL default ' ',
  "AAABDTIO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKNLE"),
  KEY "AAABDTIS" ("AAABMRIL","AAABKNLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTIV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDTJB" varchar(2) NOT NULL default ' ',
  "AAABDTJC" varchar(2) NOT NULL default '00',
  "AAABKQVZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDTJB","AAABDTJC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTJD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDTJQ" varchar(2) NOT NULL default '00',
  "AAABFNHE" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABDTJQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTJJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDTJQ" varchar(2) NOT NULL default '00',
  "AAABLPSU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDTJQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTJR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDTKE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDTKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTJW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDTKE" varchar(2) NOT NULL default ' ',
  "AAABIYBO" varchar(20) NOT NULL default ' ',
  "AAABDTKB" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDTKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTKF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDTKL" varchar(1) NOT NULL default ' ',
  "AAABDTKI" varchar(1) NOT NULL default ' ',
  "AAABDTKJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDTKL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJUQY" varchar(8) NOT NULL default ' ',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABMNLW" varchar(5) NOT NULL default ' ',
  "AAABGPKR" varchar(30) NOT NULL default ' ',
  "AAABLQTM" varchar(1) NOT NULL default ' ',
  "AAABDTKT" varchar(1) NOT NULL default ' ',
  "AAABDTKU" varchar(1) NOT NULL default ' ',
  "AAABDTKV" varchar(3) NOT NULL default '000',
  "AAABDTKW" varchar(3) NOT NULL default '000',
  "AAABDTKX" varchar(3) NOT NULL default '000',
  "AAABDTKY" varchar(3) NOT NULL default '000',
  "AAABDTKZ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABJUQY","AAABLXIY","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTLE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABDTLL" varchar(40) NOT NULL default ' ',
  "AAABJUQY" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDTLL","AAABJUQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTLN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJUQY" varchar(8) NOT NULL default ' ',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABMNLW" varchar(5) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJUQY","AAABLXIY","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTLZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDTMV" varchar(10) NOT NULL default ' ',
  "AAABIMFT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDTMV","AAABIMFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTMG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDTMV" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABIMFT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDTMV","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTMO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDTMV" varchar(10) NOT NULL default ' ',
  "AAABIMFT" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDTMT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDTMV","AAABIMFT","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTMY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDTPJ" varchar(6) NOT NULL default ' ',
  "AAABGMTM" varchar(6) NOT NULL default ' ',
  "AAABDTNC" varchar(6) NOT NULL default ' ',
  "AAABDTND" varchar(6) NOT NULL default ' ',
  "AAABDTNE" varchar(1) NOT NULL default ' ',
  "AAABDTNF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDTPJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTNI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABDTNU" varchar(2) NOT NULL default ' ',
  "AAABDTNV" varchar(2) NOT NULL default ' ',
  "AAABDTPJ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABDTNU","AAABDTNV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTNW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDTPB" varchar(12) NOT NULL default ' ',
  "AAABKQVZ" varchar(1) NOT NULL default ' ',
  "AAABDTOA" varchar(4) NOT NULL default ' ',
  "AAABKRPC" varchar(1) NOT NULL default ' ',
  "AAABKRQO" varchar(1) NOT NULL default ' ',
  "AAABDTOD" varchar(4) NOT NULL default ' ',
  "AAABDTOE" varchar(1) NOT NULL default ' ',
  "AAABKQOS" varchar(12) NOT NULL default ' ',
  "AAABDTOG" varchar(1) NOT NULL default ' ',
  "AAABDTOH" varchar(1) NOT NULL default ' ',
  "AAABDTOI" varchar(1) NOT NULL default ' ',
  "AAABDTOJ" varchar(1) NOT NULL default ' ',
  "AAABDTOK" varchar(1) NOT NULL default ' ',
  "AAABDTOL" varchar(1) NOT NULL default ' ',
  "AAABDTOM" varchar(1) NOT NULL default ' ',
  "AAABDTON" varchar(1) NOT NULL default ' ',
  "AAABDTOO" varchar(1) NOT NULL default ' ',
  "AAABMOQQ" varchar(1) NOT NULL default ' ',
  "AAABDTOQ" varchar(1) NOT NULL default ' ',
  "AAABDTOR" varchar(1) NOT NULL default ' ',
  "AAABDTOS" varchar(1) NOT NULL default ' ',
  "AAABDTOT" varchar(1) NOT NULL default ' ',
  "AAABDTOU" varchar(1) NOT NULL default ' ',
  "AAABDTOV" varchar(1) NOT NULL default ' ',
  "AAABDTOW" varchar(1) NOT NULL default ' ',
  "AAABDTOX" varchar(1) NOT NULL default ' ',
  "AAABDTOY" varchar(1) NOT NULL default ' ',
  "AAABFQCJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDTPB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTPC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDTPJ" varchar(6) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDTPJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTPK" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABILRJ" varchar(36) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTPQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMRM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMRM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTPV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMRM" varchar(4) NOT NULL default ' ',
  "AAABMLZS" varchar(2) NOT NULL default '00',
  "AAABGZXX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMRM","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTQD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMMRM" varchar(4) NOT NULL default ' ',
  "AAABJFIE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMMRM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTQL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABDTQT" varchar(3) NOT NULL default '000',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDTQT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTQU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABDTRC" varchar(40) NOT NULL default ' ',
  "AAABKNRP" varchar(4) NOT NULL default ' ',
  "AAABKRBY" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABDTRC","AAABKNRP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTRE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABLGXS" varchar(3) NOT NULL default ' ',
  "AAABGZKU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABMPLT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTRN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(12) NOT NULL default ' ',
  "AAABDTRQ" varchar(1) NOT NULL default ' ',
  "AAABDTRR" varchar(1) NOT NULL default ' ',
  "AAABDTRS" varchar(1) NOT NULL default ' ',
  "AAABDTRT" varchar(11) NOT NULL default ' ',
  "AAABDTRU" varchar(11) NOT NULL default ' ',
  "AAABDTRV" varchar(11) NOT NULL default ' ',
  "AAABDTRW" varchar(11) NOT NULL default ' ',
  "AAABDTRX" varchar(11) NOT NULL default ' ',
  "AAABDTRY" varchar(11) NOT NULL default ' ',
  "AAABDTRZ" varchar(11) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTSC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUVS" varchar(12) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTSK" (
  "AAABDTVY" varchar(10) NOT NULL default ' ',
  "AAABJEDB" varchar(30) NOT NULL default ' ',
  "AAABDTSN" varchar(30) NOT NULL default ' ',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  "AAABHZPJ" varchar(4) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABHZPN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABDTVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTSX" (
  "AAABDTVY" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABDTVY","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTTD" (
  "AAABDTWV" varchar(4) NOT NULL default ' ',
  "AAABIOBO" varchar(10) NOT NULL default ' ',
  "AAABMALF" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABDTWV","AAABIOBO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTTJ" (
  "AAABDTWV" varchar(4) NOT NULL default ' ',
  "AAABIOBO" varchar(10) NOT NULL default ' ',
  "AAABDTTQ" varchar(32) NOT NULL default ' ',
  "AAABMALF" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABDTWV","AAABIOBO","AAABDTTQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTTR" (
  "AAABDTWV" varchar(4) NOT NULL default ' ',
  "AAABDTTT" varchar(1) NOT NULL default ' ',
  "AAABDTTU" varchar(1) NOT NULL default ' ',
  "AAABDTTV" varchar(1) NOT NULL default ' ',
  "AAABDTTW" varchar(1) NOT NULL default ' ',
  "AAABDTTX" varchar(1) NOT NULL default ' ',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  "AAABHZPJ" varchar(4) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABHZPN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABDTWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTUH" (
  "AAABDTWV" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABDTWV","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTUN" (
  "AAABDTWV" varchar(4) NOT NULL default ' ',
  "AAABLTWR" varchar(40) NOT NULL default ' ',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABGNEW" varchar(10) NOT NULL default ' ',
  "AAABDTWE" varchar(10) NOT NULL default ' ',
  "AAABDTUT" varchar(10) NOT NULL default ' ',
  "AAABDTUU" varchar(10) NOT NULL default ' ',
  "AAABMALF" varchar(3) NOT NULL default '000',
  "AAABDTUW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDTWV","AAABLTWR","AAABLMNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTVA" (
  "AAABDTWV" varchar(4) NOT NULL default ' ',
  "AAABLTWR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABDTWV","AAABLTWR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTVF" (
  "AAABDTWV" varchar(4) NOT NULL default ' ',
  "AAABLTWR" varchar(40) NOT NULL default ' ',
  "AAABHRVE" varchar(20) NOT NULL default ' ',
  "AAABDTWU" varchar(20) NOT NULL default ' ',
  "AAABDTVK" varchar(20) NOT NULL default ' ',
  "AAABMALF" varchar(3) NOT NULL default '000',
  "AAABDTVM" varchar(1) NOT NULL default ' ',
  "AAABHFAP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABDTWV","AAABLTWR","AAABHRVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTVR" (
  "AAABDTWV" varchar(4) NOT NULL default ' ',
  "AAABDTWW" varchar(40) NOT NULL default ' ',
  "AAABDTVY" varchar(10) NOT NULL default ' ',
  "AAABDTVV" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABDTWV","AAABDTWW","AAABDTVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTVZ" (
  "AAABDTWV" varchar(4) NOT NULL default ' ',
  "AAABDTWW" varchar(40) NOT NULL default ' ',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABGNEW" varchar(10) NOT NULL default ' ',
  "AAABDTWE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABDTWV","AAABDTWW","AAABLMNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTWI" (
  "AAABDTWV" varchar(4) NOT NULL default ' ',
  "AAABLTWR" varchar(40) NOT NULL default ' ',
  "AAABDTWP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABDTWV","AAABLTWR"),
  KEY "AAABDTWO" ("AAABDTWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTWQ" (
  "AAABDTWV" varchar(4) NOT NULL default ' ',
  "AAABDTWW" varchar(40) NOT NULL default ' ',
  "AAABHRVE" varchar(20) NOT NULL default ' ',
  "AAABDTWU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABDTWV","AAABDTWW","AAABHRVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTWY" (
  "AAABKMJV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABKMJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTXB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDTXK" varchar(3) NOT NULL default ' ',
  "AAABKAFO" varchar(10) NOT NULL default ' ',
  "AAABDTXM" varchar(4) NOT NULL default ' ',
  "AAABDTXG" varchar(4) NOT NULL default ' ',
  "AAABDTXH" varchar(30) NOT NULL default ' ',
  "AAABDTXI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDTXK","AAABKAFO","AAABDTXM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTXN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDTZJ" varchar(3) NOT NULL default ' ',
  "AAABDUEU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDTZJ","AAABDUEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTXU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABHSKS" varchar(1) NOT NULL default ' ',
  "AAABDTXY" varchar(1) NOT NULL default ' ',
  "AAABDTXZ" varchar(1) NOT NULL default ' ',
  "AAABDTYA" varchar(1) NOT NULL default ' ',
  "AAABDTYB" varchar(1) NOT NULL default ' ',
  "AAABDTYC" varchar(1) NOT NULL default ' ',
  "AAABDTYD" varchar(1) NOT NULL default ' ',
  "AAABDTYE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTUX","AAABHSKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTYI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDTZJ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDTZJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTYN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABDTYR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABMLHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTYV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDTZX" varchar(1) NOT NULL default ' ',
  "AAABDUZL" varchar(8) NOT NULL default '00000000',
  "AAABDUZM" varchar(6) NOT NULL default '000000',
  "AAABDUZN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTZC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDTZJ" varchar(3) NOT NULL default ' ',
  "AAABDTZG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDTZJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTZK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDTZM" varchar(1) NOT NULL default ' ',
  "AAABDUZL" varchar(8) NOT NULL default '00000000',
  "AAABDUZM" varchar(6) NOT NULL default '000000',
  "AAABDUZN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDTZR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDTZT" varchar(1) NOT NULL default ' ',
  "AAABDTZU" varchar(1) NOT NULL default ' ',
  "AAABDTZV" varchar(1) NOT NULL default ' ',
  "AAABDTZW" varchar(1) NOT NULL default ' ',
  "AAABDTZX" varchar(1) NOT NULL default ' ',
  "AAABDUZL" varchar(8) NOT NULL default '00000000',
  "AAABDUZM" varchar(6) NOT NULL default '000000',
  "AAABDUZN" varchar(12) NOT NULL default ' ',
  "AAABDUAB" varchar(8) NOT NULL default '00000000',
  "AAABDUAC" varchar(6) NOT NULL default '000000',
  "AAABDUAD" varchar(12) NOT NULL default ' ',
  "AAABDUAE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUAG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDUEU" varchar(10) NOT NULL default ' ',
  "AAABDUAM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDUEU","AAABDUAM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDUAP" varchar(1) NOT NULL default ' ',
  "AAABDUZL" varchar(8) NOT NULL default '00000000',
  "AAABDUZM" varchar(6) NOT NULL default '000000',
  "AAABDUZN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUAU" (
  "AAABDUAX" varchar(2) NOT NULL default ' ',
  "AAABDUBG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABDUAX","AAABDUBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUAZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDUBG" varchar(2) NOT NULL default ' ',
  "AAABDUBC" varchar(1) NOT NULL default ' ',
  "AAABDUBD" varchar(2) NOT NULL default '00',
  "AAABDUBE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDUBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUBH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCT" varchar(4) NOT NULL default ' ',
  "AAABLKIU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUBN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDUBP" varchar(1) NOT NULL default ' ',
  "AAABDUBQ" varchar(1) NOT NULL default ' ',
  "AAABDUBR" varchar(1) NOT NULL default ' ',
  "AAABDUZL" varchar(8) NOT NULL default '00000000',
  "AAABDUZM" varchar(6) NOT NULL default '000000',
  "AAABDUZN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUBW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDUCI" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDUCI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUCB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDUCI" varchar(10) NOT NULL default ' ',
  "AAABDUCF" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDUCI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUCJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABDUCN" varchar(1) NOT NULL default ' ',
  "AAABDUCO" varchar(1) NOT NULL default ' ',
  "AAABDUCP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABMLHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUCT" (
  "AAABLLIO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUCW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDVAO" varchar(30) NOT NULL default ' ',
  "AAABMCMG" varchar(2) NOT NULL default '00',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABJEYP" varchar(10) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABHAGF" varchar(3) NOT NULL default '000',
  "AAABLTTF" varchar(1) NOT NULL default ' ',
  "AAABDUDF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDVAO","AAABMCMG","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUDK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDUDM" varchar(1) NOT NULL default ' ',
  "AAABLOBS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUDP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDUDS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUDV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDUEU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDUEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUEA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGIHK" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABDUEE" varchar(1) NOT NULL default ' ',
  "AAABDUEF" varchar(1) NOT NULL default ' ',
  "AAABDUEG" varchar(1) NOT NULL default ' ',
  "AAABDUEH" varchar(1) NOT NULL default ' ',
  "AAABHJJK" varchar(1) NOT NULL default ' ',
  "AAABGKPS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGIHK","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUEN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDUEU" varchar(10) NOT NULL default ' ',
  "AAABDUER" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDUEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUEV" (
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABLUSV" varchar(10) NOT NULL default ' ',
  "AAABEBKQ" varchar(5) NOT NULL default '00000',
  "AAABEBKR" varchar(5) NOT NULL default '00000',
  "AAABLFYC" varchar(1) NOT NULL default ' ',
  "AAABDUFB" varchar(1) NOT NULL default ' ',
  "AAABDUFC" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLZKF","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUFF" (
  "AAABDUGI" varchar(3) NOT NULL default ' ',
  "AAABDUFH" varchar(4) NOT NULL default ' ',
  "AAABDUFI" varchar(4) NOT NULL default ' ',
  "AAABDUFJ" varchar(4) NOT NULL default ' ',
  "AAABDUFK" varchar(4) NOT NULL default ' ',
  "AAABDUFL" varchar(1) NOT NULL default ' ',
  "AAABDUFM" varchar(1) NOT NULL default ' ',
  "AAABDUFN" varchar(4) NOT NULL default ' ',
  "AAABDUFO" varchar(1) NOT NULL default ' ',
  "AAABDUFP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABDUGI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUFR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGIHK" varchar(30) NOT NULL default ' ',
  "AAABDUFU" varchar(1) NOT NULL default ' ',
  "AAABDUFV" varchar(1) NOT NULL default ' ',
  "AAABDUFW" varchar(1) NOT NULL default ' ',
  "AAABDUFX" varchar(1) NOT NULL default ' ',
  "AAABDUFY" varchar(1) NOT NULL default ' ',
  "AAABDUFZ" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABDUGB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGIHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUGE" (
  "AAABDUGI" varchar(3) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHIOX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABDUGI","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABDUGN" varchar(3) NOT NULL default ' ',
  "AAABDUGO" varchar(3) NOT NULL default ' ',
  "AAABIZQZ" varchar(8) NOT NULL default '00000000',
  "AAABDUGQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUGT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABDUGW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUGZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDUHF" varchar(5) NOT NULL default ' ',
  "AAABGJZC" varchar(5) NOT NULL default ' ',
  "AAABDUHD" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDUHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUHG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABKYSP" varchar(5) NOT NULL default ' ',
  "AAABKYSQ" varchar(5) NOT NULL default ' ',
  "AAABIZQZ" varchar(8) NOT NULL default ' ',
  "AAABKWQY" decimal(9,0) NOT NULL default '0',
  "AAABKWQZ" decimal(9,0) NOT NULL default '0',
  "AAABIZQC" varchar(4) NOT NULL default ' ',
  "AAABIZQD" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLXJP","AAABKYSP","AAABKYSQ","AAABIZQZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUHV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABDUHY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUIB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYSP" varchar(5) NOT NULL default ' ',
  "AAABKYSQ" varchar(5) NOT NULL default ' ',
  "AAABIZQZ" varchar(8) NOT NULL default ' ',
  "AAABDUIG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYSP","AAABKYSQ","AAABIZQZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUIL" (
  "AAABMKFH" varchar(20) NOT NULL default ' ',
  "AAABDUIN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUIP" (
  "AAABMKFH" varchar(20) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUIV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDUIX" varchar(1) NOT NULL default ' ',
  "AAABDUIY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABKYSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABKYSP" varchar(5) NOT NULL default ' ',
  "AAABKYSQ" varchar(5) NOT NULL default ' ',
  "AAABIZQZ" varchar(8) NOT NULL default ' ',
  "AAABMPSW" decimal(9,5) NOT NULL default '0.00000',
  "AAABKWQY" decimal(9,0) NOT NULL default '0',
  "AAABKWQZ" decimal(9,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLXJP","AAABKYSP","AAABKYSQ","AAABIZQZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUJO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABKYSP" varchar(5) NOT NULL default ' ',
  "AAABKYSQ" varchar(5) NOT NULL default ' ',
  "AAABIZQZ" varchar(8) NOT NULL default ' ',
  "AAABFPVV" decimal(9,5) NOT NULL default '0.00000',
  PRIMARY KEY ("AAABMRIL","AAABLXJP","AAABKYSP","AAABKYSQ","AAABIZQZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUKA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  "AAABLXIS" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUKJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGJZC" varchar(5) NOT NULL default ' ',
  "AAABMKFH" varchar(20) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABJDPL" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABGJZC","AAABMKFH","AAABMREX","AAABLXIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUKV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABDUKY" varchar(1) NOT NULL default ' ',
  "AAABJFCZ" varchar(5) NOT NULL default ' ',
  "AAABDULA" varchar(1) NOT NULL default ' ',
  "AAABDULB" varchar(4) NOT NULL default ' ',
  "AAABDULC" varchar(4) NOT NULL default ' ',
  "AAABDULD" varchar(1) NOT NULL default ' ',
  "AAABDULE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXJP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDULH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABDULL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXJP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDULP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDUMW" varchar(10) NOT NULL default ' ',
  "AAABDULS" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABDUMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDULV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDUMD" varchar(4) NOT NULL default ' ',
  "AAABKYSP" varchar(5) NOT NULL default ' ',
  "AAABKYSQ" varchar(5) NOT NULL default ' ',
  "AAABDUMW" varchar(10) NOT NULL default ' ',
  "AAABDUMB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDUMD","AAABKYSP","AAABKYSQ"),
  KEY "AAABDUMG" ("AAABDUMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUMI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDUMW" varchar(10) NOT NULL default ' ',
  "AAABDUML" varchar(1) NOT NULL default ' ',
  "AAABKJMI" decimal(4,1) NOT NULL default '0.0',
  PRIMARY KEY ("AAABMRIL","AAABDUMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUMP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDUMW" varchar(10) NOT NULL default ' ',
  "AAABMDXM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDUMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUMX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGJZC" varchar(5) NOT NULL default ' ',
  "AAABMKFH" varchar(20) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABDUNC" varchar(8) NOT NULL default '00000000',
  "AAABDUND" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABGJZC","AAABMKFH","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUNI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGJZC" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGJZC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUNN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGJZC" varchar(5) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGJZC","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUNV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYTE" varchar(3) NOT NULL default ' ',
  "AAABDUQW" varchar(30) NOT NULL default ' ',
  "AAABMMCV" varchar(30) NOT NULL default ' ',
  "AAABDUPP" varchar(1) NOT NULL default ' ',
  "AAABDUQU" varchar(30) NOT NULL default ' ',
  "AAABLTYE" varchar(30) NOT NULL default ' ',
  "AAABLTYF" varchar(30) NOT NULL default ' ',
  "AAABLTYG" varchar(30) NOT NULL default ' ',
  "AAABIABP" varchar(30) NOT NULL default ' ',
  "AAABIABQ" varchar(30) NOT NULL default ' ',
  "AAABDUOH" varchar(30) NOT NULL default ' ',
  "AAABDUOI" varchar(30) NOT NULL default ' ',
  "AAABDUOJ" varchar(30) NOT NULL default ' ',
  "AAABDUOK" varchar(30) NOT NULL default ' ',
  "AAABDUOL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYTE","AAABDUQW","AAABMMCV","AAABDUPP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUOR" (
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMMVU" varchar(3) NOT NULL default ' ',
  "AAABMMJT" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRFQ","AAABMMVU","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUOY" (
  "AAABLYTE" varchar(3) NOT NULL default ' ',
  "AAABDUQW" varchar(30) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABMNYU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLYTE","AAABDUQW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUPF" (
  "AAABHTWG" varchar(30) NOT NULL default ' ',
  "AAABIWUT" varchar(3) NOT NULL default ' ',
  "AAABLQTM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHTWG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUPK" (
  "AAABDUQW" varchar(30) NOT NULL default ' ',
  "AAABMMCV" varchar(30) NOT NULL default ' ',
  "AAABDUPN" varchar(2) NOT NULL default '00',
  "AAABGEZT" varchar(30) NOT NULL default ' ',
  "AAABDUPP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDUQW","AAABMMCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUPS" (
  "AAABLUSV" varchar(10) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUPW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDUPY" varchar(32) NOT NULL default ' ',
  "AAABGAQV" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUQB" (
  "AAABDUQD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDUQD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUQE" (
  "AAABMMVU" varchar(3) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABDUQO" varchar(8) NOT NULL default '00000000',
  "AAABLITM" varchar(32) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABDUQK" varchar(1) NOT NULL default ' ',
  "AAABHCEC" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMMVU","AAABMRFQ","AAABDUQO","AAABLITM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUQQ" (
  "AAABLYTE" varchar(3) NOT NULL default ' ',
  "AAABDUQW" varchar(30) NOT NULL default ' ',
  "AAABMMCV" varchar(30) NOT NULL default ' ',
  "AAABDUQU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLYTE","AAABDUQW","AAABMMCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUQY" (
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  "AAABMKFH" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDURC" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  "AAABMECK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDURI" (
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABLZKF","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDURN" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDURQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDURW" varchar(26) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABJSDF" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABDURW","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDURY" (
  "AAABDUSF" varchar(4) NOT NULL default ' ',
  "AAABDUSG" varchar(60) NOT NULL default ' ',
  "AAABDUSH" varchar(4) NOT NULL default ' ',
  "AAABDUSC" varchar(4) NOT NULL default ' ',
  "AAABDUSD" varchar(60) NOT NULL default ' ',
  "AAABDUSE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABDUSF","AAABDUSG","AAABDUSH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUSI" (
  "AAABKNXB" varchar(3) NOT NULL default '000',
  "AAABGKLG" varchar(1) NOT NULL default ' ',
  "AAABGKLM" varchar(1) NOT NULL default ' ',
  "AAABDUTK" varchar(3) NOT NULL default ' ',
  "AAABDUTL" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHEIH" varchar(12) NOT NULL default ' ',
  "AAABDUSQ" varchar(12) NOT NULL default ' ',
  "AAABMKKB" varchar(8) NOT NULL default '00000000',
  "AAABGKLD" varchar(8) NOT NULL default '00000000',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABLELN" varchar(12) NOT NULL default ' ',
  "AAABDUTW" varchar(1) NOT NULL default ' ',
  "AAABDUTX" varchar(1) NOT NULL default ' ',
  "AAABDUTY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKNXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUTE" (
  "AAABKNXB" varchar(8) NOT NULL default ' ',
  "AAABDUTG" varchar(2) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABGKLG" varchar(1) NOT NULL default ' ',
  "AAABGKLM" varchar(1) NOT NULL default ' ',
  "AAABDUTK" varchar(3) NOT NULL default ' ',
  "AAABDUTL" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMKKB" varchar(8) NOT NULL default '00000000',
  "AAABGKLD" varchar(8) NOT NULL default '00000000',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABLELN" varchar(12) NOT NULL default ' ',
  "AAABDUTW" varchar(1) NOT NULL default ' ',
  "AAABDUTX" varchar(1) NOT NULL default ' ',
  "AAABDUTY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKNXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUUA" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKNXB" varchar(8) NOT NULL default ' ',
  "AAABLUDG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABKNXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUUG" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKNXB" varchar(3) NOT NULL default '000',
  "AAABLUDG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABKNXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUUM" (
  "AAABJEXI" varchar(3) NOT NULL default '000',
  "AAABGKVA" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABJEXI","AAABGKVA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUUR" (
  "AAABJEXI" varchar(8) NOT NULL default ' ',
  "AAABGKVA" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEXI","AAABGKVA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUUW" (
  "AAABJEXI" varchar(3) NOT NULL default '000',
  "AAABGEHW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEXI","AAABGEHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUVB" (
  "AAABLNOS" varchar(12) NOT NULL default ' ',
  "AAABJEXI" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLNOS","AAABJEXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUVG" (
  "AAABLNOS" varchar(32) NOT NULL default ' ',
  "AAABJEXI" varchar(8) NOT NULL default ' ',
  "AAABLNJO" varchar(2) NOT NULL default '00',
  "AAABDUVK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNOS","AAABJEXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUVN" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKNXB" varchar(3) NOT NULL default '000',
  "AAABMJVD" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABKNXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUVT" (
  "AAABKGBL" varchar(2) NOT NULL default ' ',
  "AAABMMVO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKGBL","AAABMMVO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUVY" (
  "AAABGGBA" varchar(20) NOT NULL default ' ',
  "AAABIISW" varchar(4) NOT NULL default ' ',
  "AAABHZQZ" varchar(15) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABDUWI" varchar(4) NOT NULL default ' ',
  "AAABDUWJ" varchar(4) NOT NULL default ' ',
  "AAABMNLA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGGBA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUWM" (
  "AAABGGBA" varchar(20) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABGGBA","AAABLEHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUWT" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGGBA" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABGGBA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUWZ" (
  "AAABMMVU" varchar(3) NOT NULL default ' ',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABLUDG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMVU","AAABMNJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUXF" (
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABDUXH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLZKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUXJ" (
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAABMIRY" varchar(1) NOT NULL default ' ',
  "AAABGKUN" varchar(10) NOT NULL default ' ',
  "AAABJEXI" varchar(3) NOT NULL default '000',
  "AAABHEHZ" varchar(4) NOT NULL default ' ',
  "AAABHEIC" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMIRX","AAABMIRY","AAABGKUN","AAABJEXI"),
  KEY "AAABDUXU" ("AAABHEIC","AAABHEHZ"),
  KEY "AAABDUXX" ("AAABJEXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUXZ" (
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAABMIRY" varchar(1) NOT NULL default ' ',
  "AAABGKUN" varchar(10) NOT NULL default ' ',
  "AAABJEXI" varchar(8) NOT NULL default ' ',
  "AAABHEHZ" varchar(4) NOT NULL default ' ',
  "AAABHEIC" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMIRX","AAABMIRY","AAABGKUN","AAABJEXI"),
  KEY "AAABDUYK" ("AAABHEIC","AAABHEHZ"),
  KEY "AAABDUYN" ("AAABJEXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUYP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDUYR" varchar(1) NOT NULL default ' ',
  "AAABDUZL" varchar(8) NOT NULL default '00000000',
  "AAABDUZM" varchar(6) NOT NULL default '000000',
  "AAABDUZN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUYW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDUYY" varchar(1) NOT NULL default ' ',
  "AAABDUZL" varchar(8) NOT NULL default '00000000',
  "AAABDUZM" varchar(6) NOT NULL default '000000',
  "AAABDUZN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUZD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABDUZH" varchar(3) NOT NULL default ' ',
  "AAABDUZI" varchar(3) NOT NULL default ' ',
  "AAABDUZJ" varchar(3) NOT NULL default ' ',
  "AAABDUZK" varchar(1) NOT NULL default ' ',
  "AAABDUZL" varchar(8) NOT NULL default '00000000',
  "AAABDUZM" varchar(6) NOT NULL default '000000',
  "AAABDUZN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDUZQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDVAH" varchar(1) NOT NULL default ' ',
  "AAABDUZT" varchar(1) NOT NULL default ' ',
  "AAABDUZU" varchar(1) NOT NULL default ' ',
  "AAABDUZV" varchar(1) NOT NULL default ' ',
  "AAABHJJK" varchar(1) NOT NULL default ' ',
  "AAABGKPS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDVAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVAA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDVAH" varchar(1) NOT NULL default ' ',
  "AAABDVAE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDVAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVAI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDVAO" varchar(30) NOT NULL default ' ',
  "AAABMCMG" varchar(2) NOT NULL default '00',
  "AAABLTQK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDVAO","AAABMCMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVAQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDVAX" varchar(10) NOT NULL default '0000000000',
  "AAABDVAY" varchar(10) NOT NULL default '0000000000',
  "AAABDVAU" varchar(16) NOT NULL default ' ',
  "AAABDVAV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDVAX","AAABDVAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVAZ" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABDVBU" varchar(40) NOT NULL default ' ',
  "AAABDVBV" varchar(1) NOT NULL default ' ',
  "AAABDVBW" varchar(12) NOT NULL default ' ',
  "AAABDVBX" varchar(30) NOT NULL default ' ',
  "AAABDVBY" varchar(10) NOT NULL default ' ',
  "AAABDVBZ" varchar(20) NOT NULL default ' ',
  "AAABGLYO" varchar(3) NOT NULL default '000',
  "AAABDVBI" varchar(132) NOT NULL default ' ',
  "AAABDVBJ" varchar(3) NOT NULL default '000',
  "AAABDVBK" varchar(2) NOT NULL default ' ',
  "AAABDVBL" varchar(2) NOT NULL default ' ',
  "AAABDVBM" int(11) NOT NULL default '0',
  "AAABDVBN" int(11) NOT NULL default '0',
  "AAABDVBO" int(11) NOT NULL default '0',
  "AAABDVBP" varchar(1) NOT NULL default ' ',
  "AAABDVBQ" varchar(1) NOT NULL default ' ',
  "AAABDVBR" varchar(1) NOT NULL default ' ',
  "AAABDVBS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABDVBU","AAABDVBV","AAABDVBW","AAABDVBX","AAABDVBY","AAABDVBZ","AAABGLYO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVCB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFJUY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFJUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVCG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFJUY" varchar(4) NOT NULL default ' ',
  "AAABDVCO" varchar(3) NOT NULL default ' ',
  "AAABIEIT" varchar(2) NOT NULL default ' ',
  "AAABIEIX" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABFJUY","AAABDVCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVCP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFJUY" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABFJUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVCX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIVQ" varchar(10) NOT NULL default ' ',
  "AAABDVDA" varchar(1) NOT NULL default ' ',
  "AAABDVDB" varchar(4) NOT NULL default ' ',
  "AAABDVDC" varchar(1) NOT NULL default ' ',
  "AAABHYJM" varchar(2) NOT NULL default ' ',
  "AAABLYCG" varchar(2) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAABGAAJ" varchar(1) NOT NULL default ' ',
  "AAABDVDH" varchar(4) NOT NULL default ' ',
  "AAABDVDI" varchar(4) NOT NULL default ' ',
  "AAABDVDJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLIVQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVDM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIVQ" varchar(10) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMFSJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLIVQ","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVDW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLIVQ" varchar(10) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLIVQ","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVEI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLIVQ" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLIVQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIVT" varchar(4) NOT NULL default ' ',
  "AAABDVET" varchar(1) NOT NULL default ' ',
  "AAABLBET" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLIVT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVEX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLIVT" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLIVT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVFF" (
  "AAABDVFN" varchar(8) NOT NULL default '00000000',
  "AAABDVFH" varchar(60) NOT NULL default ' ',
  "AAABIHDR" varchar(1) NOT NULL default ' ',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABDVFN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVFL" (
  "AAABEUWV" varchar(30) NOT NULL default ' ',
  "AAABDVFN" varchar(8) NOT NULL default '00000000',
  "AAABLXIS" varchar(75) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABEUWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVFR" (
  "AAABEUWV" varchar(30) NOT NULL default ' ',
  "AAABMKXW" varchar(13) NOT NULL default ' ',
  "AAABLXIS" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABEUWV","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVFX" (
  "AAABDVKL" varchar(8) NOT NULL default '00000000',
  "AAABKWJY" varchar(30) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABLXIS" varchar(60) NOT NULL default ' ',
  "AAABHAZS" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABDVKL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVGE" (
  "AAABGCUH" varchar(15) NOT NULL default ' ',
  "AAABHAZI" varchar(10) NOT NULL default ' ',
  "AAABHAZS" varchar(12) NOT NULL default ' ',
  "AAABGCUL" varchar(132) NOT NULL default ' ',
  "AAABLRQQ" varchar(1) NOT NULL default ' ',
  "AAABIKRP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGCUH","AAABHAZI","AAABHAZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVGO" (
  "AAABHAZL" varchar(8) NOT NULL default '00000000',
  "AAABEUWV" varchar(30) NOT NULL default ' ',
  "AAABLXIS" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABHAZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVGT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDVIN" varchar(20) NOT NULL default '00000000000000000000',
  "AAABHAZI" varchar(10) NOT NULL default ' ',
  "AAABKWJY" varchar(30) NOT NULL default ' ',
  "AAABMOIB" varchar(10) NOT NULL default ' ',
  "AAABKOUC" varchar(4) NOT NULL default ' ',
  "AAABEUWV" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABHAZS" varchar(12) NOT NULL default ' ',
  "AAABHJAF" varchar(8) NOT NULL default '00000000',
  "AAABDVHE" varchar(6) NOT NULL default '000000',
  "AAABDVHF" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDVIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVHI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDVIN" varchar(20) NOT NULL default '00000000000000000000',
  "AAABDVHV" varchar(8) NOT NULL default '00000000',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default ' ',
  "AAABDVHO" varchar(50) NOT NULL default ' ',
  "AAABDVHP" varchar(50) NOT NULL default ' ',
  "AAABDVHQ" varchar(50) NOT NULL default ' ',
  "AAABDVHR" varchar(50) NOT NULL default ' ',
  "AAABMRCK" varchar(73) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDVIN","AAABDVHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVHW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDVIN" varchar(20) NOT NULL default '00000000000000000000',
  "AAABMRCU" varchar(8) NOT NULL default '00000000',
  "AAABHAZL" varchar(8) NOT NULL default '00000000',
  "AAABEUWV" varchar(30) NOT NULL default ' ',
  "AAABMOIB" varchar(10) NOT NULL default ' ',
  "AAABKOUC" varchar(4) NOT NULL default ' ',
  "AAABDVKL" varchar(8) NOT NULL default '00000000',
  "AAABGMZS" varchar(8) NOT NULL default '00000000',
  "AAABDVJA" varchar(8) NOT NULL default '00000000',
  "AAABHAZS" varchar(12) NOT NULL default ' ',
  "AAABLRTV" varchar(8) NOT NULL default '00000000',
  "AAABLRTW" varchar(6) NOT NULL default '000000',
  "AAABLEQN" varchar(8) NOT NULL default '00000000',
  "AAABLEQO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABDVIN","AAABMRCU","AAABHAZL","AAABEUWV","AAABMOIB","AAABKOUC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVIT" (
  "AAABLUSV" varchar(8) NOT NULL default '00000000',
  "AAABKWJY" varchar(30) NOT NULL default ' ',
  "AAABMOIB" varchar(10) NOT NULL default ' ',
  "AAABHAZL" varchar(8) NOT NULL default '00000000',
  "AAABDVKL" varchar(8) NOT NULL default '00000000',
  "AAABGMZS" varchar(8) NOT NULL default '00000000',
  "AAABDVJA" varchar(8) NOT NULL default '00000000',
  "AAABDVJB" varchar(8) NOT NULL default '00000000',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  "AAABHAZS" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVJF" (
  "AAABKWJY" varchar(30) NOT NULL default ' ',
  "AAABIHDR" varchar(1) NOT NULL default ' ',
  "AAABDVJI" longtext,
  "AAABDVJJ" longtext,
  "AAABDVJK" longtext,
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABKWJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVJN" (
  "AAABHAZS" varchar(12) NOT NULL default ' ',
  "AAABDVJT" varchar(8) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABFHMV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHAZS","AAABDVJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVJU" (
  "AAABMKXW" varchar(13) NOT NULL default ' ',
  "AAABEUWV" varchar(30) NOT NULL default ' ',
  "AAABDVKL" varchar(8) NOT NULL default '00000000',
  "AAABDYBQ" varchar(8) NOT NULL default '00000000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  "AAABDYBE" varchar(1) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMKXW","AAABEUWV","AAABDVKL","AAABDYBQ","AAABLSCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVKO" (
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABMLPV" varchar(32) NOT NULL default ' ',
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  "AAABDVKS" varchar(2) NOT NULL default ' ',
  "AAABDVKT" varchar(8) NOT NULL default '00000000',
  "AAABDVKU" varchar(2) NOT NULL default '00',
  "AAABDVKV" varchar(2) NOT NULL default '00',
  "AAABLNZX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVKY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDVOA" varchar(2) NOT NULL default ' ',
  "AAABDVOB" varchar(4) NOT NULL default ' ',
  "AAABDVOC" varchar(3) NOT NULL default ' ',
  "AAABDVLE" varchar(3) NOT NULL default '000',
  "AAABDVLF" varchar(2) NOT NULL default ' ',
  "AAABDVLG" varchar(1) NOT NULL default ' ',
  "AAABDVLH" varchar(1) NOT NULL default ' ',
  "AAABDVLI" varchar(3) NOT NULL default '000',
  "AAABDVLJ" varchar(2) NOT NULL default ' ',
  "AAABDVLK" varchar(1) NOT NULL default ' ',
  "AAABDVLL" varchar(1) NOT NULL default ' ',
  "AAABDVLM" varchar(3) NOT NULL default '000',
  "AAABDVLN" varchar(2) NOT NULL default ' ',
  "AAABDVLO" varchar(1) NOT NULL default ' ',
  "AAABDVLP" varchar(1) NOT NULL default ' ',
  "AAABDVLQ" varchar(1) NOT NULL default ' ',
  "AAABDVLR" varchar(3) NOT NULL default '000',
  "AAABDVLS" varchar(2) NOT NULL default ' ',
  "AAABEEDC" varchar(1) NOT NULL default ' ',
  "AAABDVLU" varchar(1) NOT NULL default ' ',
  "AAABDVLV" varchar(3) NOT NULL default '000',
  "AAABDVLW" varchar(2) NOT NULL default ' ',
  "AAABEEDJ" varchar(1) NOT NULL default ' ',
  "AAABDVLY" varchar(1) NOT NULL default ' ',
  "AAABDVLZ" varchar(1) NOT NULL default ' ',
  "AAABDVNK" varchar(1) NOT NULL default ' ',
  "AAABDVNL" varchar(1) NOT NULL default ' ',
  "AAABDVOH" varchar(1) NOT NULL default ' ',
  "AAABDVOI" varchar(1) NOT NULL default ' ',
  "AAABDVOJ" varchar(1) NOT NULL default ' ',
  "AAABDVOK" varchar(3) NOT NULL default '000',
  "AAABDVON" varchar(1) NOT NULL default ' ',
  "AAABKQMP" varchar(1) NOT NULL default ' ',
  "AAABDVNQ" varchar(1) NOT NULL default ' ',
  "AAABDVOP" varchar(1) NOT NULL default ' ',
  "AAABDVOM" varchar(1) NOT NULL default '0',
  "AAABDVOQ" varchar(1) NOT NULL default ' ',
  "AAABDVNS" decimal(3,0) NOT NULL default '0',
  "AAABDVNT" decimal(3,0) NOT NULL default '0',
  "AAABDVNU" decimal(3,0) NOT NULL default '0',
  "AAABDVNV" decimal(3,0) NOT NULL default '0',
  "AAABDVNW" decimal(3,0) NOT NULL default '0',
  "AAABDVNX" decimal(3,0) NOT NULL default '0',
  "AAABDVMS" varchar(1) NOT NULL default ' ',
  "AAABDVMT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABDVOA","AAABDVOB","AAABDVOC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVMZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDVOA" varchar(2) NOT NULL default ' ',
  "AAABDVOB" varchar(4) NOT NULL default ' ',
  "AAABDVOC" varchar(3) NOT NULL default ' ',
  "AAABKQXG" varchar(3) NOT NULL default '000',
  "AAABDVNG" varchar(1) NOT NULL default ' ',
  "AAABDVNH" varchar(2) NOT NULL default ' ',
  "AAABDVNI" varchar(1) NOT NULL default ' ',
  "AAABDVNJ" varchar(1) NOT NULL default ' ',
  "AAABDVNK" varchar(1) NOT NULL default ' ',
  "AAABDVNL" varchar(1) NOT NULL default ' ',
  "AAABDVOH" varchar(1) NOT NULL default ' ',
  "AAABDVOK" varchar(3) NOT NULL default '000',
  "AAABDVON" varchar(1) NOT NULL default ' ',
  "AAABKQMP" varchar(1) NOT NULL default ' ',
  "AAABDVNQ" varchar(1) NOT NULL default ' ',
  "AAABDVOM" varchar(1) NOT NULL default '0',
  "AAABDVNS" decimal(3,0) NOT NULL default '0',
  "AAABDVNT" decimal(3,0) NOT NULL default '0',
  "AAABDVNU" decimal(3,0) NOT NULL default '0',
  "AAABDVNV" decimal(3,0) NOT NULL default '0',
  "AAABDVNW" decimal(3,0) NOT NULL default '0',
  "AAABDVNX" decimal(3,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABDVOA","AAABDVOB","AAABDVOC","AAABKQXG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVOE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFFG" varchar(12) NOT NULL default ' ',
  "AAABDVOH" varchar(1) NOT NULL default ' ',
  "AAABDVOI" varchar(1) NOT NULL default ' ',
  "AAABDVOJ" varchar(1) NOT NULL default ' ',
  "AAABDVOK" varchar(3) NOT NULL default '000',
  "AAABDVOL" varchar(1) NOT NULL default ' ',
  "AAABDVOM" varchar(1) NOT NULL default '0',
  "AAABDVON" varchar(1) NOT NULL default ' ',
  "AAABDVOO" varchar(1) NOT NULL default ' ',
  "AAABDVOP" varchar(1) NOT NULL default ' ',
  "AAABDVOQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKFFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVOT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUVS" varchar(12) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVPB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKWZ" varchar(30) NOT NULL default ' ',
  "AAABFLEJ" varchar(24) NOT NULL default ' ',
  "AAABDVXU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKWZ","AAABFLEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVPJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDVQC" varchar(10) NOT NULL default ' ',
  "AAABDVXU" varchar(30) NOT NULL default ' ',
  "AAABDVPN" varchar(24) NOT NULL default ' ',
  "AAABDVPO" varchar(24) NOT NULL default ' ',
  "AAABDVPP" varchar(24) NOT NULL default ' ',
  "AAABDVPQ" varchar(24) NOT NULL default ' ',
  "AAABDVPR" decimal(5,0) NOT NULL default '0',
  "AAABDVPS" decimal(5,0) NOT NULL default '0',
  "AAABDVPT" varchar(1) NOT NULL default ' ',
  "AAABDVPU" varchar(10) NOT NULL default ' ',
  "AAABDVPV" varchar(10) NOT NULL default ' ',
  "AAABDVXY" varchar(1) NOT NULL default ' ',
  "AAABDVXZ" varchar(1) NOT NULL default ' ',
  "AAABDVYA" varchar(1) NOT NULL default ' ',
  "AAABDVYB" varchar(1) NOT NULL default ' ',
  "AAABDVYE" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABDVQC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVQD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDVRL" varchar(10) NOT NULL default ' ',
  "AAABIJWI" varchar(1) NOT NULL default ' ',
  "AAABDVZV" varchar(1) NOT NULL default ' ',
  "AAABDWBA" varchar(1) NOT NULL default ' ',
  "AAABDWAB" decimal(3,0) NOT NULL default '0',
  "AAABDWAC" decimal(3,0) NOT NULL default '0',
  "AAABDVQL" decimal(5,0) NOT NULL default '0',
  "AAABDWAX" varchar(1) NOT NULL default ' ',
  "AAABDWAY" decimal(3,0) NOT NULL default '0',
  "AAABDVQO" varchar(1) NOT NULL default ' ',
  "AAABDVQP" varchar(1) NOT NULL default ' ',
  "AAABDVQQ" decimal(5,0) NOT NULL default '0',
  "AAABDVQR" decimal(5,0) NOT NULL default '0',
  "AAABDVQS" varchar(3) NOT NULL default ' ',
  "AAABDWAE" varchar(1) NOT NULL default ' ',
  "AAABILZL" varchar(10) NOT NULL default ' ',
  "AAABDVZZ" varchar(4) NOT NULL default ' ',
  "AAABDWAF" varchar(8) NOT NULL default ' ',
  "AAABDWAG" varchar(8) NOT NULL default ' ',
  "AAABDWAA" varchar(1) NOT NULL default ' ',
  "AAABDVQZ" varchar(1) NOT NULL default ' ',
  "AAABDVRA" varchar(1) NOT NULL default ' ',
  "AAABDVRB" varchar(1) NOT NULL default ' ',
  "AAABDVZX" varchar(1) NOT NULL default ' ',
  "AAABLRXN" varchar(10) NOT NULL default ' ',
  "AAABLRXO" varchar(10) NOT NULL default ' ',
  "AAABDVZC" varchar(1) NOT NULL default ' ',
  "AAABFSUI" varchar(10) NOT NULL default ' ',
  "AAABDVRH" varchar(3) NOT NULL default ' ',
  "AAABDWAH" varchar(1) NOT NULL default ' ',
  "AAABDWBG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDVRL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVRM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLDON" varchar(10) NOT NULL default ' ',
  "AAABDVYS" varchar(10) NOT NULL default ' ',
  "AAABDVZR" varchar(10) NOT NULL default ' ',
  "AAABDWAT" varchar(10) NOT NULL default ' ',
  "AAABDWBQ" varchar(10) NOT NULL default ' ',
  "AAABDWLE" varchar(10) NOT NULL default ' ',
  "AAABDWRF" varchar(10) NOT NULL default ' ',
  "AAABDWMC" varchar(10) NOT NULL default ' ',
  "AAABDWQY" varchar(10) NOT NULL default ' ',
  "AAABDWUV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLDON")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVSA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLDON" varchar(10) NOT NULL default ' ',
  "AAABHYGA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLDON")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVSI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDVXX" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDVXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVSN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLROG" varchar(10) NOT NULL default ' ',
  "AAABDVWX" varchar(2) NOT NULL default '00',
  "AAABDVSS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLROG","AAABDVWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVSX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLROG" varchar(10) NOT NULL default ' ',
  "AAABDVWX" varchar(2) NOT NULL default '00',
  "AAABIYDX" varchar(10) NOT NULL default ' ',
  "AAABEOSE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLROG","AAABDVWX","AAABIYDX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVTJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLROG" varchar(10) NOT NULL default ' ',
  "AAABDVWX" varchar(2) NOT NULL default '00',
  "AAABDVWP" varchar(10) NOT NULL default ' ',
  "AAABDVTP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLROG","AAABDVWX","AAABDVWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLROG" varchar(10) NOT NULL default ' ',
  "AAABDVTZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLROG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVUD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDVXX" varchar(10) NOT NULL default ' ',
  "AAABDVUH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDVXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVUL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLROG" varchar(10) NOT NULL default ' ',
  "AAABDVWX" varchar(2) NOT NULL default '00',
  "AAABDVWP" varchar(10) NOT NULL default ' ',
  "AAABIYDX" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLROG","AAABDVWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVUU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLROG" varchar(10) NOT NULL default ' ',
  "AAABDVWX" varchar(2) NOT NULL default '00',
  "AAABIYDX" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLROG","AAABDVWX","AAABIYDX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVVD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLROG" varchar(10) NOT NULL default ' ',
  "AAABDVWX" varchar(2) NOT NULL default '00',
  "AAABIYDX" varchar(10) NOT NULL default ' ',
  "AAABMMJW" varchar(2) NOT NULL default '00',
  "AAABDWFT" varchar(10) NOT NULL default ' ',
  "AAABDVVK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLROG","AAABDVWX","AAABIYDX","AAABMMJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVVQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLROG" varchar(10) NOT NULL default ' ',
  "AAABDVWX" varchar(2) NOT NULL default '00',
  "AAABDVWP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLROG","AAABDVWX","AAABDVWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVVZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLROG" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLROG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVWE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLROG" varchar(10) NOT NULL default ' ',
  "AAABDVWX" varchar(2) NOT NULL default '00',
  "AAABDVWP" varchar(10) NOT NULL default ' ',
  "AAABMMJW" varchar(2) NOT NULL default '00',
  "AAABMNMP" varchar(10) NOT NULL default ' ',
  "AAABHECM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLROG","AAABDVWX","AAABDVWP","AAABMMJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVWR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDVXX" varchar(10) NOT NULL default ' ',
  "AAABMMJW" varchar(2) NOT NULL default '00',
  "AAABDVXU" varchar(30) NOT NULL default ' ',
  "AAABDVWW" varchar(24) NOT NULL default ' ',
  "AAABDVWX" tinyint(3) unsigned NOT NULL default '0',
  "AAABDVWY" varchar(1) NOT NULL default ' ',
  "AAABDVWZ" varchar(2) NOT NULL default '00',
  "AAABDVXA" varchar(2) NOT NULL default '00',
  "AAABDVXB" varchar(2) NOT NULL default '00',
  "AAABDVXC" varchar(2) NOT NULL default '00',
  "AAABDVXD" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABDVXX","AAABMMJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVXH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDVXX" varchar(10) NOT NULL default ' ',
  "AAABMMJW" varchar(2) NOT NULL default '00',
  "AAABHACY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDVXX","AAABMMJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVXR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDVYS" varchar(10) NOT NULL default ' ',
  "AAABDVXU" varchar(30) NOT NULL default ' ',
  "AAABDVXV" varchar(24) NOT NULL default ' ',
  "AAABDVXW" varchar(24) NOT NULL default ' ',
  "AAABDVXX" varchar(10) NOT NULL default ' ',
  "AAABDVXY" varchar(1) NOT NULL default ' ',
  "AAABDVXZ" varchar(1) NOT NULL default ' ',
  "AAABDVYA" varchar(1) NOT NULL default ' ',
  "AAABDVYB" varchar(1) NOT NULL default ' ',
  "AAABDVYC" varchar(1) NOT NULL default ' ',
  "AAABDVYD" varchar(1) NOT NULL default ' ',
  "AAABDVYE" varchar(2) NOT NULL default '00',
  "AAABDVYF" varchar(10) NOT NULL default ' ',
  "AAABDVYG" varchar(1) NOT NULL default ' ',
  "AAABDVYH" varchar(1) NOT NULL default ' ',
  "AAABDVYI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDVYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVYL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDVYS" varchar(10) NOT NULL default ' ',
  "AAABDVYP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDVYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVYT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDVZR" varchar(10) NOT NULL default ' ',
  "AAABDVYW" varchar(1) NOT NULL default ' ',
  "AAABDVYX" varchar(10) NOT NULL default ' ',
  "AAABDVYY" varchar(10) NOT NULL default ' ',
  "AAABDVYZ" varchar(1) NOT NULL default ' ',
  "AAABDVZA" varchar(10) NOT NULL default ' ',
  "AAABDVZB" varchar(10) NOT NULL default ' ',
  "AAABDVZC" varchar(1) NOT NULL default ' ',
  "AAABFSUI" varchar(10) NOT NULL default ' ',
  "AAABDVZE" varchar(1) NOT NULL default ' ',
  "AAABDVZF" varchar(10) NOT NULL default ' ',
  "AAABDVZG" varchar(10) NOT NULL default ' ',
  "AAABDVZH" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABDVZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVZK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDVZR" varchar(10) NOT NULL default ' ',
  "AAABDVZO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDVZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVZS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDWAT" varchar(10) NOT NULL default ' ',
  "AAABDVZV" varchar(1) NOT NULL default ' ',
  "AAABIJWI" varchar(1) NOT NULL default ' ',
  "AAABDVZX" varchar(1) NOT NULL default ' ',
  "AAABILZL" varchar(10) NOT NULL default ' ',
  "AAABDVZZ" varchar(4) NOT NULL default ' ',
  "AAABDWAA" varchar(1) NOT NULL default ' ',
  "AAABDWAB" decimal(3,0) NOT NULL default '0',
  "AAABDWAC" decimal(3,0) NOT NULL default '0',
  "AAABDWAD" varchar(3) NOT NULL default ' ',
  "AAABDWAE" varchar(1) NOT NULL default ' ',
  "AAABDWAF" varchar(8) NOT NULL default ' ',
  "AAABDWAG" varchar(8) NOT NULL default ' ',
  "AAABDWAH" varchar(1) NOT NULL default ' ',
  "AAABDWAI" varchar(1) NOT NULL default ' ',
  "AAABDWAJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDWAT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWAM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDWAT" varchar(10) NOT NULL default ' ',
  "AAABDWAQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDWAT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWAU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDWBQ" varchar(10) NOT NULL default ' ',
  "AAABDWAX" varchar(1) NOT NULL default ' ',
  "AAABDWAY" decimal(3,0) NOT NULL default '0',
  "AAABDWAZ" varchar(10) NOT NULL default ' ',
  "AAABDWBA" varchar(1) NOT NULL default ' ',
  "AAABDWQL" varchar(1) NOT NULL default ' ',
  "AAABDWQM" varchar(1) NOT NULL default '0',
  "AAABDWQN" varchar(6) NOT NULL default '000000',
  "AAABDWQO" varchar(1) NOT NULL default '0',
  "AAABDWBF" varchar(3) NOT NULL default ' ',
  "AAABDWBG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDWBQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWBJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDWBQ" varchar(10) NOT NULL default ' ',
  "AAABDWBN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDWBQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWBR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDWRF" varchar(10) NOT NULL default ' ',
  "AAABKWVT" varchar(1) NOT NULL default ' ',
  "AAABDWBV" varchar(1) NOT NULL default ' ',
  "AAABDWBW" varchar(1) NOT NULL default ' ',
  "AAABDWBX" varchar(1) NOT NULL default ' ',
  "AAABDWBY" varchar(1) NOT NULL default ' ',
  "AAABDWBZ" varchar(1) NOT NULL default '0',
  "AAABDWCA" varchar(1) NOT NULL default ' ',
  "AAABDWCB" varchar(1) NOT NULL default ' ',
  "AAABDWCC" varchar(1) NOT NULL default '0',
  "AAABDWCD" varchar(1) NOT NULL default '0',
  "AAABDWCE" varchar(10) NOT NULL default ' ',
  "AAABDWCF" varchar(1) NOT NULL default ' ',
  "AAABDWCG" varchar(1) NOT NULL default ' ',
  "AAABDWCH" varchar(1) NOT NULL default ' ',
  "AAABDWCI" varchar(1) NOT NULL default ' ',
  "AAABDWCJ" varchar(1) NOT NULL default ' ',
  "AAABDWCK" varchar(1) NOT NULL default '0',
  "AAABDWCL" varchar(1) NOT NULL default ' ',
  "AAABIWGL" varchar(1) NOT NULL default '0',
  "AAABDWCN" varchar(1) NOT NULL default ' ',
  "AAABDWCO" varchar(1) NOT NULL default ' ',
  "AAABDWCP" varchar(1) NOT NULL default ' ',
  "AAABDWCQ" varchar(1) NOT NULL default ' ',
  "AAABDWCR" varchar(1) NOT NULL default ' ',
  "AAABDWCS" varchar(1) NOT NULL default ' ',
  "AAABDWCT" varchar(1) NOT NULL default ' ',
  "AAABDWCU" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDWRF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWCX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDWRF" varchar(10) NOT NULL default ' ',
  "AAABDWDB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDWRF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDWFT" varchar(10) NOT NULL default ' ',
  "AAABDWDI" varchar(1) NOT NULL default ' ',
  "AAABDWDJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDWFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWDM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHECM" varchar(10) NOT NULL default ' ',
  "AAABILRT" varchar(3) NOT NULL default '000',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHECM","AAABILRT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWDW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDWFT" varchar(10) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDWFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWEE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDWFT" varchar(10) NOT NULL default ' ',
  "AAABDWEK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDWFT","AAABDWEK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWEL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHECM" varchar(10) NOT NULL default ' ',
  "AAABILRT" varchar(3) NOT NULL default '000',
  "AAABDWFT" varchar(10) NOT NULL default ' ',
  "AAABDWFU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHECM","AAABILRT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWEU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDWFT" varchar(10) NOT NULL default ' ',
  "AAABDWFU" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABDWFB" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDWFT","AAABDWFU","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWFI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDWFT" varchar(10) NOT NULL default ' ',
  "AAABDWFU" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABILRT" varchar(3) NOT NULL default '000',
  "AAABHAGF" smallint(6) NOT NULL default '0',
  "AAABMLMY" varchar(6) NOT NULL default '000000',
  "AAABDWFR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDWFT","AAABDWFU","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWFX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMUZ" varchar(6) NOT NULL default ' ',
  "AAABJHCL" varchar(2) NOT NULL default '00',
  "AAABMLQP" varchar(10) NOT NULL default ' ',
  "AAABMMCV" varchar(30) NOT NULL default ' ',
  "AAABDWGD" varchar(30) NOT NULL default ' ',
  "AAABEPJZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMUZ","AAABJHCL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWGI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMUZ" varchar(6) NOT NULL default ' ',
  "AAABDWJA" varchar(6) NOT NULL default ' ',
  "AAABDWGQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMUZ","AAABDWJA","AAABDWGQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWGR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMUZ" varchar(6) NOT NULL default ' ',
  "AAABDWJA" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMUZ","AAABDWJA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWGY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLMUZ" varchar(6) NOT NULL default ' ',
  "AAABDWJA" varchar(6) NOT NULL default ' ',
  "AAABDWHD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLMUZ","AAABDWJA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWHI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDWWZ" varchar(10) NOT NULL default ' ',
  "AAABMRFO" varchar(5) NOT NULL default '00000',
  "AAABMALF" varchar(2) NOT NULL default '00',
  "AAABMLQP" varchar(10) NOT NULL default ' ',
  "AAABMMCV" varchar(30) NOT NULL default ' ',
  "AAABDWZE" varchar(3) NOT NULL default ' ',
  "AAABMMJT" varchar(30) NOT NULL default ' ',
  "AAABDWZG" varchar(5) NOT NULL default '00000',
  "AAABDWZH" varchar(10) NOT NULL default ' ',
  "AAABLWIV" varchar(30) NOT NULL default ' ',
  "AAABLMUZ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDWWZ","AAABMRFO","AAABMALF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDWWZ" varchar(10) NOT NULL default ' ',
  "AAABDWKS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDWWZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWIF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDWWZ" varchar(10) NOT NULL default ' ',
  "AAABDWIJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDWWZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWIN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMUZ" varchar(6) NOT NULL default ' ',
  "AAABDWWF" varchar(10) NOT NULL default ' ',
  "AAABMRFO" varchar(5) NOT NULL default '00000',
  "AAABMALF" varchar(2) NOT NULL default '00',
  "AAABMLQP" varchar(10) NOT NULL default ' ',
  "AAABMMCV" varchar(30) NOT NULL default ' ',
  "AAABDWZE" varchar(3) NOT NULL default ' ',
  "AAABMMJT" varchar(30) NOT NULL default ' ',
  "AAABDWZG" varchar(5) NOT NULL default '00000',
  "AAABDWZH" varchar(10) NOT NULL default ' ',
  "AAABLWIV" varchar(30) NOT NULL default ' ',
  "AAABDWJA" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMUZ","AAABDWWF","AAABMRFO","AAABMALF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWJG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMUZ" varchar(6) NOT NULL default ' ',
  "AAABDWWF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMUZ","AAABDWWF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMUZ" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDWWF" varchar(10) NOT NULL default ' ',
  "AAABDWJS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMUZ","AAABMRHS","AAABDWWF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWJX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDWLE" varchar(10) NOT NULL default ' ',
  "AAABDWKA" varchar(4) NOT NULL default ' ',
  "AAABDWKB" varchar(12) NOT NULL default ' ',
  "AAABDWKC" varchar(1) NOT NULL default ' ',
  "AAABDWKD" varchar(10) NOT NULL default ' ',
  "AAABDWKE" varchar(10) NOT NULL default ' ',
  "AAABDWKF" varchar(10) NOT NULL default ' ',
  "AAABDWKG" varchar(10) NOT NULL default ' ',
  "AAABDWKH" varchar(10) NOT NULL default ' ',
  "AAABDWKI" decimal(10,2) NOT NULL default '0.00',
  "AAABDWKJ" varchar(3) NOT NULL default ' ',
  "AAABDWKK" decimal(10,2) NOT NULL default '0.00',
  "AAABDWKL" varchar(3) NOT NULL default ' ',
  "AAABDWKM" decimal(10,2) NOT NULL default '0.00',
  "AAABDWKN" varchar(3) NOT NULL default ' ',
  "AAABDWKO" varchar(10) NOT NULL default ' ',
  "AAABLROG" varchar(10) NOT NULL default ' ',
  "AAABDWUB" varchar(10) NOT NULL default ' ',
  "AAABFYMZ" varchar(1) NOT NULL default ' ',
  "AAABDWKS" varchar(10) NOT NULL default ' ',
  "AAABDWKT" varchar(10) NOT NULL default ' ',
  "AAABDWKU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDWLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWKX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDWLE" varchar(10) NOT NULL default ' ',
  "AAABDWLB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDWLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWLF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDWMC" varchar(10) NOT NULL default ' ',
  "AAABDWLI" varchar(1) NOT NULL default ' ',
  "AAABIXQA" varchar(1) NOT NULL default ' ',
  "AAABDWLK" varchar(1) NOT NULL default ' ',
  "AAABDWLL" varchar(1) NOT NULL default ' ',
  "AAABDWLM" varchar(20) NOT NULL default ' ',
  "AAABDWLN" varchar(20) NOT NULL default ' ',
  "AAABDWLO" varchar(1) NOT NULL default ' ',
  "AAABDWLP" varchar(1) NOT NULL default ' ',
  "AAABDWLQ" varchar(1) NOT NULL default ' ',
  "AAABDWLR" varchar(20) NOT NULL default ' ',
  "AAABDWLS" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABDWMC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWLV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDWMC" varchar(10) NOT NULL default ' ',
  "AAABDWLZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDWMC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWMD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDWOT" varchar(10) NOT NULL default ' ',
  "AAABDWNV" varchar(2) NOT NULL default '00',
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  "AAABDWMI" decimal(10,2) NOT NULL default '0.00',
  "AAABDWMJ" varchar(2) NOT NULL default ' ',
  "AAABDWMK" varchar(2) NOT NULL default ' ',
  "AAABDWML" varchar(2) NOT NULL default ' ',
  "AAABDWMM" varchar(2) NOT NULL default ' ',
  "AAABDWMN" varchar(2) NOT NULL default ' ',
  "AAABDWMO" decimal(10,2) NOT NULL default '0.00',
  "AAABDWUB" varchar(10) NOT NULL default ' ',
  "AAABDWMQ" varchar(1) NOT NULL default ' ',
  "AAABDWMR" varchar(2) NOT NULL default ' ',
  "AAABDWMS" varchar(2) NOT NULL default ' ',
  "AAABDWMT" varchar(1) NOT NULL default ' ',
  "AAABDWMU" varchar(2) NOT NULL default ' ',
  "AAABDWMV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDWOT","AAABDWNV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWMZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDWOT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDWOT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWNE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDWOT" varchar(10) NOT NULL default ' ',
  "AAABDWNI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDWOT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWNM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDWOT" varchar(10) NOT NULL default ' ',
  "AAABDWNV" varchar(2) NOT NULL default '00',
  "AAABDWNR" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDWOT","AAABDWNV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWNW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDWVI" varchar(6) NOT NULL default ' ',
  "AAABDWNZ" varchar(2) NOT NULL default ' ',
  "AAABDWOA" varchar(2) NOT NULL default ' ',
  "AAABDWOB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDWVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWOE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDWVI" varchar(6) NOT NULL default ' ',
  "AAABDWOI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDWVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWOM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDWOT" varchar(10) NOT NULL default ' ',
  "AAABDWOU" varchar(2) NOT NULL default '00',
  "AAABDWOV" varchar(2) NOT NULL default '00',
  "AAABMNLW" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDWOT","AAABDWOU","AAABDWOV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWOW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDWPR" varchar(10) NOT NULL default ' ',
  "AAABDWOZ" decimal(10,2) NOT NULL default '0.00',
  "AAABDWPA" varchar(3) NOT NULL default ' ',
  "AAABDWPB" decimal(10,2) NOT NULL default '0.00',
  "AAABDWPC" decimal(10,2) NOT NULL default '0.00',
  "AAABDWPD" varchar(3) NOT NULL default ' ',
  "AAABDWPE" decimal(10,2) NOT NULL default '0.00',
  "AAABDWPF" varchar(3) NOT NULL default ' ',
  "AAABDWPG" decimal(10,2) NOT NULL default '0.00',
  "AAABDWPH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDWPR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWPK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDWPR" varchar(10) NOT NULL default ' ',
  "AAABDWPO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDWPR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWPS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMUZ" varchar(6) NOT NULL default ' ',
  "AAABDWPV" varchar(2) NOT NULL default ' ',
  "AAABDWPW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWPZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLMUZ" varchar(6) NOT NULL default ' ',
  "AAABEZGK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLMUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWQH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDWQY" varchar(10) NOT NULL default ' ',
  "AAABLROG" varchar(10) NOT NULL default ' ',
  "AAABDWQL" varchar(1) NOT NULL default ' ',
  "AAABDWQM" varchar(1) NOT NULL default '0',
  "AAABDWQN" varchar(6) NOT NULL default '000000',
  "AAABDWQO" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABDWQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWQR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDWQY" varchar(10) NOT NULL default ' ',
  "AAABDWQV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDWQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWQZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDWRF" varchar(10) NOT NULL default ' ',
  "AAABDWRG" varchar(8) NOT NULL default '00000000',
  "AAABDWRD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDWRF","AAABDWRG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWRH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDWUB" varchar(10) NOT NULL default ' ',
  "AAABDWRK" decimal(10,2) NOT NULL default '0.00',
  "AAABDWRL" decimal(10,2) NOT NULL default '0.00',
  "AAABDWRM" decimal(10,2) NOT NULL default '0.00',
  "AAABDWRN" decimal(10,2) NOT NULL default '0.00',
  "AAABDWRO" decimal(10,2) NOT NULL default '0.00',
  "AAABDWRP" decimal(10,2) NOT NULL default '0.00',
  "AAABDWRQ" decimal(10,2) NOT NULL default '0.00',
  "AAABDWRR" decimal(10,2) NOT NULL default '0.00',
  "AAABDWRS" decimal(10,2) NOT NULL default '0.00',
  "AAABDWRT" decimal(10,2) NOT NULL default '0.00',
  "AAABDWRU" decimal(10,2) NOT NULL default '0.00',
  "AAABDWRV" decimal(10,2) NOT NULL default '0.00',
  "AAABDWRW" decimal(10,2) NOT NULL default '0.00',
  "AAABDWRX" decimal(10,2) NOT NULL default '0.00',
  "AAABDWRY" varchar(2) NOT NULL default ' ',
  "AAABDWRZ" varchar(2) NOT NULL default ' ',
  "AAABDWSA" varchar(2) NOT NULL default ' ',
  "AAABDWSB" varchar(2) NOT NULL default ' ',
  "AAABDWSC" varchar(2) NOT NULL default ' ',
  "AAABDWSD" varchar(2) NOT NULL default ' ',
  "AAABDWSE" varchar(2) NOT NULL default ' ',
  "AAABDWSF" varchar(3) NOT NULL default '000',
  "AAABDWSG" varchar(3) NOT NULL default '000',
  "AAABDWSH" varchar(2) NOT NULL default '00',
  "AAABDWSI" varchar(2) NOT NULL default '00',
  "AAABDWSJ" varchar(2) NOT NULL default '00',
  "AAABDWSK" varchar(1) NOT NULL default ' ',
  "AAABDWSL" varchar(2) NOT NULL default ' ',
  "AAABDWSM" varchar(2) NOT NULL default ' ',
  "AAABDWSN" varchar(2) NOT NULL default ' ',
  "AAABDWSO" varchar(2) NOT NULL default ' ',
  "AAABDWSP" varchar(2) NOT NULL default ' ',
  "AAABDWSQ" varchar(2) NOT NULL default ' ',
  "AAABDWSR" varchar(2) NOT NULL default ' ',
  "AAABDWSS" int(11) NOT NULL default '0',
  "AAABDWST" int(11) NOT NULL default '0',
  "AAABDWSU" int(11) NOT NULL default '0',
  "AAABDWSV" int(11) NOT NULL default '0',
  "AAABDWSW" int(11) NOT NULL default '0',
  "AAABDWSX" int(11) NOT NULL default '0',
  "AAABDWSY" int(11) NOT NULL default '0',
  "AAABDWSZ" varchar(2) NOT NULL default ' ',
  "AAABDWTA" varchar(2) NOT NULL default ' ',
  "AAABDWTB" varchar(2) NOT NULL default ' ',
  "AAABDWTC" varchar(2) NOT NULL default ' ',
  "AAABDWTD" varchar(2) NOT NULL default ' ',
  "AAABDWTE" varchar(2) NOT NULL default ' ',
  "AAABDWTF" varchar(2) NOT NULL default ' ',
  "AAABDWTG" varchar(1) NOT NULL default ' ',
  "AAABDWTH" varchar(2) NOT NULL default ' ',
  "AAABDWTI" varchar(2) NOT NULL default ' ',
  "AAABDWTJ" varchar(2) NOT NULL default ' ',
  "AAABDWTK" varchar(1) NOT NULL default ' ',
  "AAABDWTL" varchar(2) NOT NULL default ' ',
  "AAABDWTM" varchar(2) NOT NULL default ' ',
  "AAABDWTN" varchar(1) NOT NULL default ' ',
  "AAABDWTO" varchar(2) NOT NULL default ' ',
  "AAABDWTP" varchar(6) NOT NULL default '000000',
  "AAABDWTQ" varchar(2) NOT NULL default ' ',
  "AAABDWTR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDWUB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWTU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDWUB" varchar(10) NOT NULL default ' ',
  "AAABDWTY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDWUB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWUC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDWUV" varchar(10) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABDXBW" varchar(12) NOT NULL default ' ',
  "AAABDWUH" varchar(30) NOT NULL default ' ',
  "AAABDWUI" varchar(30) NOT NULL default ' ',
  "AAABDWUJ" varchar(12) NOT NULL default ' ',
  "AAABDWUK" varchar(12) NOT NULL default ' ',
  "AAABDWUL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDWUV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWUO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDWUV" varchar(10) NOT NULL default ' ',
  "AAABDWUS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDWUV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWUW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDWVZ" varchar(10) NOT NULL default ' ',
  "AAABMRFO" varchar(5) NOT NULL default '00000',
  "AAABMALF" varchar(2) NOT NULL default '00',
  "AAABMLQP" varchar(10) NOT NULL default ' ',
  "AAABMMCV" varchar(30) NOT NULL default ' ',
  "AAABDWZE" varchar(3) NOT NULL default ' ',
  "AAABMMJT" varchar(30) NOT NULL default ' ',
  "AAABDWZG" varchar(5) NOT NULL default '00000',
  "AAABDWZH" varchar(10) NOT NULL default ' ',
  "AAABLWIV" varchar(30) NOT NULL default ' ',
  "AAABDWVI" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDWVZ","AAABMRFO","AAABMALF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWVN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDWVZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDWVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWVS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDWVZ" varchar(10) NOT NULL default ' ',
  "AAABDWVW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDWVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWWA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDWWZ" varchar(10) NOT NULL default ' ',
  "AAABDWXA" varchar(10) NOT NULL default ' ',
  "AAABLMUZ" varchar(6) NOT NULL default ' ',
  "AAABDWWF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDWWZ","AAABDWXA","AAABLMUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWWK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDWWZ" varchar(10) NOT NULL default ' ',
  "AAABDWXA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDWWZ","AAABDWXA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWWR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDWWZ" varchar(10) NOT NULL default ' ',
  "AAABDWXA" varchar(10) NOT NULL default ' ',
  "AAABDWWW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDWWZ","AAABDWXA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWXB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDWYE" varchar(10) NOT NULL default ' ',
  "AAABMRFO" varchar(5) NOT NULL default '00000',
  "AAABMALF" varchar(2) NOT NULL default '00',
  "AAABMLQP" varchar(10) NOT NULL default ' ',
  "AAABMMCV" varchar(30) NOT NULL default ' ',
  "AAABDWZE" varchar(3) NOT NULL default ' ',
  "AAABMMJT" varchar(30) NOT NULL default ' ',
  "AAABDWZG" varchar(5) NOT NULL default '00000',
  "AAABDWZH" varchar(10) NOT NULL default ' ',
  "AAABLWIV" varchar(30) NOT NULL default ' ',
  "AAABDWYW" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDWYE","AAABMRFO","AAABMALF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWXS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDWYE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDWYE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWXX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDWYE" varchar(10) NOT NULL default ' ',
  "AAABDWYB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDWYE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDWYW" varchar(6) NOT NULL default ' ',
  "AAABDWYI" decimal(10,2) NOT NULL default '0.00',
  "AAABDWYJ" varchar(2) NOT NULL default ' ',
  "AAABDWYK" varchar(2) NOT NULL default ' ',
  "AAABDWYL" varchar(2) NOT NULL default '00',
  "AAABDWYM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDWYW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWYP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDWYW" varchar(6) NOT NULL default ' ',
  "AAABDWYT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDWYW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWYX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDXAA" varchar(10) NOT NULL default ' ',
  "AAABMRFO" varchar(5) NOT NULL default '00000',
  "AAABMALF" varchar(2) NOT NULL default '00',
  "AAABMLQP" varchar(10) NOT NULL default ' ',
  "AAABMMCV" varchar(30) NOT NULL default ' ',
  "AAABDWZE" varchar(3) NOT NULL default ' ',
  "AAABMMJT" varchar(30) NOT NULL default ' ',
  "AAABDWZG" varchar(5) NOT NULL default '00000',
  "AAABDWZH" varchar(10) NOT NULL default ' ',
  "AAABLWIV" varchar(30) NOT NULL default ' ',
  "AAABDXAR" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDXAA","AAABMRFO","AAABMALF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWZO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDWZS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDWZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDWZT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDXAA" varchar(10) NOT NULL default ' ',
  "AAABDWZX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDXAA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXAB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDXAR" varchar(6) NOT NULL default ' ',
  "AAABLTQJ" varchar(2) NOT NULL default ' ',
  "AAABDXAF" varchar(1) NOT NULL default ' ',
  "AAABDXAG" varchar(1) NOT NULL default ' ',
  "AAABMNRG" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABDXAR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXAK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDXAR" varchar(6) NOT NULL default ' ',
  "AAABDXAO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDXAR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXAS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABDXBW" varchar(12) NOT NULL default ' ',
  "AAABDXAW" varchar(14) NOT NULL default ' ',
  "AAABDXAX" varchar(12) NOT NULL default ' ',
  "AAABDXAY" varchar(12) NOT NULL default ' ',
  "AAABDXAZ" varchar(12) NOT NULL default ' ',
  "AAABDXBA" varchar(1) NOT NULL default ' ',
  "AAABDXBB" varchar(1) NOT NULL default ' ',
  "AAABDXBC" varchar(1) NOT NULL default ' ',
  "AAABDXBD" varchar(1) NOT NULL default ' ',
  "AAABDXBE" varchar(2) NOT NULL default '00',
  "AAABDXBF" varchar(2) NOT NULL default '00',
  "AAABDXBG" varchar(14) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABJHFD" varchar(1) NOT NULL default ' ',
  "AAABDXHF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAABDXBW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXBN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABDXBW" varchar(12) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREL","AAABDXBW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXBX" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXCA" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLXIS" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXCG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABFYCS" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMLMY" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABJHFD" varchar(1) NOT NULL default ' ',
  "AAABDXHF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAABKNSU","AAABFYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXCU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABFYCS" varchar(3) NOT NULL default '000',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREL","AAABKNSU","AAABFYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXDG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDXIW" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABDXIW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXDN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDXIW" varchar(8) NOT NULL default ' ',
  "AAABDXDS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCVI","AAABDXIW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXDX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDXFV" varchar(10) NOT NULL default ' ',
  "AAABDXEA" varchar(1) NOT NULL default ' ',
  "AAABDXEB" varchar(1) NOT NULL default ' ',
  "AAABDXEC" varchar(1) NOT NULL default ' ',
  "AAABKQOF" varchar(1) NOT NULL default ' ',
  "AAABDXEE" varchar(1) NOT NULL default ' ',
  "AAABDXEF" varchar(1) NOT NULL default ' ',
  "AAABDXEG" varchar(10) NOT NULL default ' ',
  "AAABDXEH" varchar(10) NOT NULL default ' ',
  "AAABDXEI" varchar(1) NOT NULL default ' ',
  "AAABMLMY" varchar(6) NOT NULL default '000000',
  "AAABDXEK" varchar(1) NOT NULL default ' ',
  "AAABDXEL" varchar(2) NOT NULL default ' ',
  "AAABDXEM" varchar(2) NOT NULL default ' ',
  "AAABDXEN" varchar(1) NOT NULL default ' ',
  "AAABDXEO" decimal(5,2) NOT NULL default '0.00',
  "AAABDXEP" varchar(1) NOT NULL default '0',
  "AAABDXEQ" decimal(5,2) NOT NULL default '0.00',
  "AAABDXER" varchar(1) NOT NULL default '0',
  "AAABDXES" varchar(1) NOT NULL default '0',
  "AAABDXET" varchar(1) NOT NULL default ' ',
  "AAABDXEU" varchar(1) NOT NULL default ' ',
  "AAABDXEV" varchar(1) NOT NULL default ' ',
  "AAABDXEW" decimal(5,2) NOT NULL default '0.00',
  "AAABDXEX" varchar(1) NOT NULL default '0',
  "AAABDXEY" decimal(5,2) NOT NULL default '0.00',
  "AAABDXEZ" varchar(1) NOT NULL default '0',
  "AAABDXFA" varchar(1) NOT NULL default '0',
  "AAABDXFB" varchar(1) NOT NULL default ' ',
  "AAABDXFC" varchar(1) NOT NULL default ' ',
  "AAABDXFD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDXFV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXFG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDXFV" varchar(10) NOT NULL default ' ',
  "AAABDXFK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDXFV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXFO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDXFV" varchar(10) NOT NULL default ' ',
  "AAABJUQY" varchar(2) NOT NULL default '00',
  "AAABKQOF" varchar(1) NOT NULL default ' ',
  "AAABDXFT" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABDXFV","AAABJUQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXFX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABFYCS" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABJHFD" varchar(1) NOT NULL default ' ',
  "AAABDXHF" varchar(1) NOT NULL default ' ',
  "AAABGLYJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAABKNSU","AAABFYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXGL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABFYCS" varchar(3) NOT NULL default '000',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREL","AAABKNSU","AAABFYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXGX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABFYCS" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABJHFD" varchar(1) NOT NULL default ' ',
  "AAABDXHF" varchar(1) NOT NULL default ' ',
  "AAABGLYJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAABKNSU","AAABFYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXHL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKNSU" varchar(12) NOT NULL default ' ',
  "AAABFYCS" varchar(3) NOT NULL default '000',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREL","AAABKNSU","AAABFYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXHX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDXIW" varchar(8) NOT NULL default ' ',
  "AAABDXIX" varchar(20) NOT NULL default ' ',
  "AAABDXIY" varchar(20) NOT NULL default ' ',
  "AAABDXIZ" varchar(20) NOT NULL default ' ',
  "AAABDXJA" varchar(20) NOT NULL default ' ',
  "AAABKOPL" decimal(9,3) NOT NULL default '0.000',
  "AAABKQJO" varchar(3) NOT NULL default ' ',
  "AAABDXIH" decimal(5,2) NOT NULL default '0.00',
  "AAABKQIS" decimal(9,3) NOT NULL default '0.000',
  "AAABKQIT" varchar(3) NOT NULL default ' ',
  "AAABDXIK" decimal(5,2) NOT NULL default '0.00',
  "AAABDXIL" varchar(1) NOT NULL default '0',
  "AAABKQWU" varchar(1) NOT NULL default ' ',
  "AAABKQWS" varchar(8) NOT NULL default ' ',
  "AAABKQWT" varchar(2) NOT NULL default ' ',
  "AAABDXIP" varchar(1) NOT NULL default ' ',
  "AAABDXIQ" varchar(2) NOT NULL default '00',
  "AAABDXIR" varchar(1) NOT NULL default '0',
  "AAABLECD" varchar(2) NOT NULL default ' ',
  "AAABDXIT" decimal(9,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABDXIW","AAABDXIX","AAABDXIY","AAABDXIZ","AAABDXJA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXJB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABHVMV" varchar(3) NOT NULL default ' ',
  "AAABDXJT" varchar(40) NOT NULL default ' ',
  "AAABDXJU" varchar(3) NOT NULL default ' ',
  "AAABDXJV" varchar(25) NOT NULL default ' ',
  "AAABDXJW" varchar(2) NOT NULL default ' ',
  "AAABDXJX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMRD","AAABHVMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABHVMV" varchar(3) NOT NULL default ' ',
  "AAABDXKO" varchar(2) NOT NULL default ' ',
  "AAABMLPO" varchar(10) NOT NULL default '0000000000',
  "AAABDXJT" varchar(255) NOT NULL default ' ',
  "AAABDXJU" varchar(3) NOT NULL default ' ',
  "AAABDXJV" varchar(25) NOT NULL default ' ',
  "AAABDXJW" varchar(2) NOT NULL default ' ',
  "AAABDXJX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMRD","AAABHVMV","AAABDXKO","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXKD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABHVMV" varchar(3) NOT NULL default ' ',
  "AAABDXKO" varchar(2) NOT NULL default ' ',
  "AAABMLPO" varchar(10) NOT NULL default '0000000000',
  "AAABEDBR" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMRD","AAABHVMV","AAABDXKO","AAABMLPO","AAABEDBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXKR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDXKT" varchar(1) NOT NULL default ' ',
  "AAABDXKU" varchar(1) NOT NULL default ' ',
  "AAABDXKV" varchar(255) NOT NULL default ' ',
  "AAABDXKW" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXKY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJQZV" varchar(4) NOT NULL default ' ',
  "AAABJQZW" varchar(2) NOT NULL default '00',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABJMAG" varchar(3) NOT NULL default '000',
  "AAABDXLE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJQZV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXLH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJQZV" varchar(4) NOT NULL default ' ',
  "AAABKTYZ" varchar(15) NOT NULL default ' ',
  "AAABKTZA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJQZV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXLQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJMBX" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABJMBX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXLV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJMBX" varchar(3) NOT NULL default '000',
  "AAABKTYZ" varchar(15) NOT NULL default ' ',
  "AAABKTZA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJMBX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXME" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABJMAG" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABLCAK","AAABJMAG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXML" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJMAG" varchar(3) NOT NULL default '000',
  "AAABKTYZ" varchar(15) NOT NULL default ' ',
  "AAABKTZA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJMAG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXMU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCQZ" varchar(3) NOT NULL default '000',
  "AAABDXMX" varchar(1) NOT NULL default ' ',
  "AAABDXMY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCQZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXNB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLCQZ" varchar(3) NOT NULL default '000',
  "AAABJMOB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLCQZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXNJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLCKF" varchar(2) NOT NULL default ' ',
  "AAABKTYZ" varchar(15) NOT NULL default ' ',
  "AAABKTZA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLCKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXNS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLCKF" varchar(2) NOT NULL default ' ',
  "AAABKTYZ" varchar(15) NOT NULL default ' ',
  "AAABKTZA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLCKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCKF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXOG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCKF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXOL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJLSL" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABJLSL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXOQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJLSL" varchar(2) NOT NULL default '00',
  "AAABKTYZ" varchar(15) NOT NULL default ' ',
  "AAABKTZA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJLSL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXOZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABJPQT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCAK","AAABJPQT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXPG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABJPQT" varchar(2) NOT NULL default ' ',
  "AAABJQTW" varchar(30) NOT NULL default ' ',
  "AAABKTYZ" varchar(15) NOT NULL default ' ',
  "AAABKTZA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLCAK","AAABJPQT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXPS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJMAV" varchar(2) NOT NULL default '00',
  "AAABDXPV" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABJMAV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJMAV" varchar(2) NOT NULL default '00',
  "AAABDXQQ" varchar(15) NOT NULL default ' ',
  "AAABJMOB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJMAV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXQH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJMBI" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABJMBI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXQM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJMBI" varchar(2) NOT NULL default '00',
  "AAABDXQQ" varchar(15) NOT NULL default ' ',
  "AAABJMOB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJMBI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXQV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJMCO" varchar(2) NOT NULL default '00',
  "AAABDXQY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJMCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXRB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJMCO" varchar(2) NOT NULL default '00',
  "AAABKTYZ" varchar(15) NOT NULL default ' ',
  "AAABKHNG" varchar(30) NOT NULL default ' ',
  "AAABKTZA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJMCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXRL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJMCX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJMCX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXRQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJMCX" varchar(4) NOT NULL default ' ',
  "AAABDXVI" varchar(15) NOT NULL default ' ',
  "AAABDXVJ" varchar(30) NOT NULL default ' ',
  "AAABFCOF" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJMCX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXSA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJMCW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJMCW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXSF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJMCW" varchar(4) NOT NULL default ' ',
  "AAABDXVI" varchar(15) NOT NULL default ' ',
  "AAABDXVJ" varchar(30) NOT NULL default ' ',
  "AAABFCOF" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJMCW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXSP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJOTS" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJOTS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXSU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJOTS" varchar(12) NOT NULL default ' ',
  "AAABDXVI" varchar(15) NOT NULL default ' ',
  "AAABDXVJ" varchar(30) NOT NULL default ' ',
  "AAABFCOF" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJOTS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXTE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABLCWJ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABLCPX","AAABLCWJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXTL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCSF" varchar(3) NOT NULL default '000',
  "AAABDXTO" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABLCSF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXTR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLCSF" varchar(3) NOT NULL default '000',
  "AAABJMOB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLCSF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXTZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCRR" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABLCRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXUE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLCRR" varchar(3) NOT NULL default '000',
  "AAABJMOB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLCRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXUM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCSE" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABLCSE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXUR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLCSE" varchar(3) NOT NULL default '000',
  "AAABJMOB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLCSE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXUZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJOTT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJOTT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXVE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDXVM" varchar(1) NOT NULL default ' ',
  "AAABJOTT" varchar(10) NOT NULL default ' ',
  "AAABDXVI" varchar(15) NOT NULL default ' ',
  "AAABDXVJ" varchar(30) NOT NULL default ' ',
  "AAABFCOF" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDXVM","AAABJOTT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXVO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDXWK" varchar(4) NOT NULL default ' ',
  "AAABDXWL" varchar(4) NOT NULL default ' ',
  "AAABDXWF" varchar(16) NOT NULL default ' ',
  "AAABFWHC" varchar(6) NOT NULL default ' ',
  "AAABDXWH" varchar(16) NOT NULL default ' ',
  "AAABDXWI" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDXWK","AAABDXWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXVZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDXWK" varchar(4) NOT NULL default ' ',
  "AAABDXWL" varchar(4) NOT NULL default ' ',
  "AAABMCUX" varchar(2) NOT NULL default ' ',
  "AAABJPMO" varchar(1) NOT NULL default '0',
  "AAABDXWF" varchar(16) NOT NULL default ' ',
  "AAABFWHC" varchar(6) NOT NULL default ' ',
  "AAABDXWH" varchar(16) NOT NULL default ' ',
  "AAABDXWI" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDXWK","AAABDXWL","AAABMCUX","AAABJPMO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXWO" (
  "AAABFZRW" varchar(21) NOT NULL default ' ',
  "AAABGAMR" varchar(3) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXWT" (
  "AAABJPGE" varchar(1) NOT NULL default ' ',
  "AAABFZRW" varchar(21) NOT NULL default ' ',
  "AAABDXWW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJPGE","AAABFZRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXWZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABJPRA" varchar(2) NOT NULL default '00',
  "AAABLCVA" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLCAK","AAABJPRA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXXJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABJPRA" varchar(2) NOT NULL default '00',
  "AAABJQTW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABLCAK","AAABJPRA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXXV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDXYT" varchar(4) NOT NULL default ' ',
  "AAABDXYU" varchar(2) NOT NULL default ' ',
  "AAABIYQF" varchar(2) NOT NULL default ' ',
  "AAABDXYN" varchar(2) NOT NULL default ' ',
  "AAABDXYO" varchar(2) NOT NULL default ' ',
  "AAABDXYP" varchar(2) NOT NULL default ' ',
  "AAABDXYQ" varchar(2) NOT NULL default ' ',
  "AAABKGMN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDXYT","AAABDXYU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXYI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDXYT" varchar(4) NOT NULL default ' ',
  "AAABDXYU" varchar(2) NOT NULL default ' ',
  "AAABIYQF" varchar(2) NOT NULL default ' ',
  "AAABDXYN" varchar(2) NOT NULL default ' ',
  "AAABDXYO" varchar(2) NOT NULL default ' ',
  "AAABDXYP" varchar(2) NOT NULL default ' ',
  "AAABDXYQ" varchar(2) NOT NULL default ' ',
  "AAABKGMN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDXYT","AAABDXYU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXYV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABDYAB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXZB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJOQW" varchar(2) NOT NULL default '00',
  "AAABDXZE" varchar(1) NOT NULL default ' ',
  "AAABDXZF" varchar(1) NOT NULL default ' ',
  "AAABDXZG" varchar(1) NOT NULL default ' ',
  "AAABDXZH" varchar(1) NOT NULL default ' ',
  "AAABDXZI" varchar(1) NOT NULL default ' ',
  "AAABDXZJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJOQW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXZM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJOQW" varchar(2) NOT NULL default '00',
  "AAABDXZQ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJOQW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDXZU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABLCVA" varchar(2) NOT NULL default '00',
  "AAABJODP" varchar(5) NOT NULL default ' ',
  "AAABJMAG" varchar(3) NOT NULL default '000',
  "AAABDYAB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLCAK","AAABLCVA","AAABJODP","AAABJMAG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYAI" (
  "AAABLUER" varchar(2) NOT NULL default '00',
  "AAABECLM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER","AAABECLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZYP" varchar(8) NOT NULL default '00000000',
  "AAABKWJY" varchar(30) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  "AAABHAZS" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZYP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYAW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKXW" varchar(13) NOT NULL default ' ',
  "AAABEUWV" varchar(30) NOT NULL default ' ',
  "AAABEZYP" varchar(8) NOT NULL default '00000000',
  "AAABDYBQ" varchar(8) NOT NULL default '00000000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  "AAABDYBE" varchar(1) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMKXW","AAABEUWV","AAABEZYP","AAABDYBQ","AAABLSCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYBS" (
  "AAABDYCE" varchar(1) NOT NULL default ' ',
  "AAABLWWP" varchar(2) NOT NULL default '00',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABDYBW" varchar(2) NOT NULL default '00',
  "AAABDYBX" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLTYE" varchar(30) NOT NULL default ' ',
  "AAABLTYF" varchar(30) NOT NULL default ' ',
  "AAABDYCB" varchar(30) NOT NULL default ' ',
  "AAABHUHN" varchar(30) NOT NULL default ' ',
  "AAABDYCD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABDYCE","AAABLWWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYCG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLDCD" varchar(4) NOT NULL default ' ',
  "AAABDYCL" varchar(1) NOT NULL default ' ',
  "AAABDYCM" varchar(1) NOT NULL default ' ',
  "AAABDYCN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCPX","AAABMREX","AAABLDCD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYCS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABMAAS" varchar(1) NOT NULL default ' ',
  "AAABLISJ" varchar(1) NOT NULL default ' ',
  "AAABMJWI" varchar(1) NOT NULL default ' ',
  "AAABLTST" varchar(1) NOT NULL default ' ',
  "AAABIJYD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLCAK","AAABMAAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYDF" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABHFJC" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABHFJC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYDK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJPBG" varchar(3) NOT NULL default '000',
  "AAABLAXH" varchar(4) NOT NULL default ' ',
  "AAABIEJO" varchar(4) NOT NULL default ' ',
  "AAABIEJP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJPBG","AAABLAXH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYDT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABDYEA" varchar(2) NOT NULL default ' ',
  "AAABDYDX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABDYEA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYEB" (
  "AAABDYFL" varchar(10) NOT NULL default ' ',
  "AAABDYLJ" varchar(1) NOT NULL default '0',
  "AAABDYFG" varchar(3) NOT NULL default '000',
  "AAABDYFH" varchar(4) NOT NULL default '0000',
  "AAABDYEY" varchar(1) NOT NULL default ' ',
  "AAABMEAG" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABDYFL","AAABDYLJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYEK" (
  "AAABDYFL" varchar(10) NOT NULL default ' ',
  "AAABDZKE" varchar(1) NOT NULL default '0',
  "AAABDYFG" varchar(3) NOT NULL default '000',
  "AAABDYFH" varchar(4) NOT NULL default '0000',
  "AAABDYFI" varchar(1) NOT NULL default ' ',
  "AAABMEAG" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABDYFL","AAABDZKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYET" (
  "AAABDYFL" varchar(10) NOT NULL default ' ',
  "AAABDYLJ" varchar(1) NOT NULL default '0',
  "AAABDYFG" varchar(3) NOT NULL default '000',
  "AAABDYFH" varchar(4) NOT NULL default '0000',
  "AAABDYEY" varchar(1) NOT NULL default ' ',
  "AAABMEAG" varchar(1) NOT NULL default '0',
  "AAABMEAM" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABDYFL","AAABDYLJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYFD" (
  "AAABDYFL" varchar(10) NOT NULL default ' ',
  "AAABDZKE" varchar(1) NOT NULL default '0',
  "AAABDYFG" varchar(3) NOT NULL default '000',
  "AAABDYFH" varchar(4) NOT NULL default '0000',
  "AAABDYFI" varchar(1) NOT NULL default ' ',
  "AAABMEAG" varchar(1) NOT NULL default '0',
  "AAABMEAM" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABDYFL","AAABDZKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYFN" (
  "AAABLTYE" varchar(10) NOT NULL default ' ',
  "AAABDYFP" varchar(10) NOT NULL default ' ',
  "AAABDYFQ" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLTYE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYFS" (
  "AAABECLM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABECLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYFV" (
  "AAABLCOG" varchar(1) NOT NULL default ' ',
  "AAABECLM" varchar(2) NOT NULL default ' ',
  "AAABDYFY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLCOG","AAABECLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYGB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABJOHA" varchar(1) NOT NULL default ' ',
  "AAABDYGG" varchar(1) NOT NULL default ' ',
  "AAABDYGH" varchar(2) NOT NULL default '00',
  "AAABDYGI" varchar(2) NOT NULL default '00',
  "AAABDYJG" varchar(4) NOT NULL default '0000',
  "AAABDYGK" varchar(1) NOT NULL default ' ',
  "AAABDYGL" varchar(1) NOT NULL default ' ',
  "AAABECGJ" varchar(1) NOT NULL default ' ',
  "AAABDYGN" varchar(1) NOT NULL default ' ',
  "AAABDYGO" varchar(1) NOT NULL default ' ',
  "AAABDYGP" varchar(1) NOT NULL default ' ',
  "AAABDYGQ" varchar(1) NOT NULL default ' ',
  "AAABECGM" varchar(1) NOT NULL default ' ',
  "AAABECGN" varchar(1) NOT NULL default ' ',
  "AAABLDQW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLCAK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYGX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJPJW" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABJPJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYHC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJPJW" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJQTW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJPJW","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYHK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJOKV" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABJOKV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYHP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJOKV" varchar(3) NOT NULL default '000',
  "AAABJQTW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJOKV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYHX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDYIU" varchar(1) NOT NULL default ' ',
  "AAABJOIK" varchar(1) NOT NULL default ' ',
  "AAABJOKF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDYIU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYIE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDYIU" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJQTW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDYIU","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYIM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABJPJO" varchar(2) NOT NULL default ' ',
  "AAABDZFH" varchar(1) NOT NULL default ' ',
  "AAABJOHR" varchar(1) NOT NULL default ' ',
  "AAABDYIT" varchar(1) NOT NULL default ' ',
  "AAABDYIU" varchar(1) NOT NULL default ' ',
  "AAABDYLS" varchar(2) NOT NULL default '00',
  "AAABJOIT" varchar(1) NOT NULL default ' ',
  "AAABJOKL" varchar(1) NOT NULL default ' ',
  "AAABJOIE" varchar(4) NOT NULL default ' ',
  "AAABJOJW" varchar(4) NOT NULL default ' ',
  "AAABJOKA" varchar(4) NOT NULL default '0000',
  "AAABDYJB" varchar(4) NOT NULL default '0000',
  "AAABDYJC" varchar(4) NOT NULL default '0000',
  "AAABDYJD" varchar(4) NOT NULL default '0000',
  "AAABDYJE" varchar(4) NOT NULL default '0000',
  "AAABJOIJ" varchar(4) NOT NULL default '0000',
  "AAABDYJG" varchar(4) NOT NULL default '0000',
  "AAABJOIH" varchar(4) NOT NULL default '0000',
  "AAABJOIF" varchar(4) NOT NULL default ' ',
  "AAABJOJY" varchar(4) NOT NULL default ' ',
  "AAABJOIG" varchar(4) NOT NULL default ' ',
  "AAABJOJZ" varchar(4) NOT NULL default ' ',
  "AAABDYJM" varchar(4) NOT NULL default ' ',
  "AAABDYJN" varchar(4) NOT NULL default ' ',
  "AAABDYJO" varchar(4) NOT NULL default ' ',
  "AAABDYJP" varchar(4) NOT NULL default ' ',
  "AAABJOHD" varchar(1) NOT NULL default ' ',
  "AAABJOKV" varchar(3) NOT NULL default '000',
  "AAABJPGZ" varchar(2) NOT NULL default '00',
  "AAABJOKX" varchar(3) NOT NULL default '000',
  "AAABJOHQ" decimal(10,7) NOT NULL default '0.0000000',
  "AAABJOJK" varchar(1) NOT NULL default ' ',
  "AAABDYJW" varchar(1) NOT NULL default ' ',
  "AAABDYJX" varchar(1) NOT NULL default ' ',
  "AAABDYJY" varchar(3) NOT NULL default '000',
  "AAABJOHI" varchar(2) NOT NULL default '00',
  "AAABJOHJ" varchar(2) NOT NULL default '00',
  "AAABJOHK" varchar(2) NOT NULL default '00',
  "AAABJOHL" varchar(2) NOT NULL default '00',
  "AAABJOHM" varchar(2) NOT NULL default '00',
  "AAABJOHN" varchar(2) NOT NULL default '00',
  "AAABDZFR" varchar(2) NOT NULL default ' ',
  "AAABJOHO" varchar(2) NOT NULL default '00',
  "AAABJOHP" varchar(2) NOT NULL default '00',
  "AAABJOJF" varchar(4) NOT NULL default ' ',
  "AAABJOJG" varchar(4) NOT NULL default ' ',
  "AAABJOJH" varchar(4) NOT NULL default ' ',
  "AAABJOKS" varchar(4) NOT NULL default ' ',
  "AAABJOKT" varchar(4) NOT NULL default ' ',
  "AAABJOKU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLCAK","AAABJPJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYKS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABJPJO" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJQTW" varchar(40) NOT NULL default ' ',
  "AAABECJY" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLCAK","AAABJPJO","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYLF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDYLS" varchar(2) NOT NULL default '00',
  "AAABDZKE" varchar(2) NOT NULL default '00',
  "AAABDYLJ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABDYLS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYLM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDYLS" varchar(2) NOT NULL default '00',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJQTW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDYLS","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYLU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJPGR" varchar(1) NOT NULL default ' ',
  "AAABDYZG" varchar(1) NOT NULL default ' ',
  "AAABJPGW" varchar(1) NOT NULL default ' ',
  "AAABJOFO" varchar(1) NOT NULL default '0',
  "AAABJPGZ" varchar(2) NOT NULL default '00',
  "AAABJOFH" varchar(3) NOT NULL default '000',
  "AAABJPHA" varchar(1) NOT NULL default ' ',
  "AAABJPHB" varchar(2) NOT NULL default ' ',
  "AAABDYZN" varchar(1) NOT NULL default ' ',
  "AAABDYZO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCAK","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYML" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABECGJ" varchar(1) NOT NULL default ' ',
  "AAABECGM" varchar(1) NOT NULL default ' ',
  "AAABECGL" varchar(1) NOT NULL default ' ',
  "AAABECGN" varchar(1) NOT NULL default ' ',
  "AAABLDQW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLCAK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYMX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJPJW" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABJPJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYNC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJPJW" varchar(3) NOT NULL default '000',
  "AAABDZAU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJPJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYNK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDYNW" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABDYNW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYNP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDYNW" varchar(3) NOT NULL default '000',
  "AAABDYNT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDYNW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYNX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJQDC" varchar(3) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABDYOC" varchar(30) NOT NULL default ' ',
  "AAABDYOM" varchar(30) NOT NULL default ' ',
  "AAABDYON" varchar(1) NOT NULL default ' ',
  "AAABDYOO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJQDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYOI" (
  "AAABJQDC" varchar(3) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABDYOM" varchar(30) NOT NULL default ' ',
  "AAABDYON" varchar(1) NOT NULL default ' ',
  "AAABDYOO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJQDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYOQ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJQDC" varchar(3) NOT NULL default ' ',
  "AAABDYQE" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABJQDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYOW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJQDC" varchar(3) NOT NULL default ' ',
  "AAABJQCJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJQDC","AAABJQCJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYPD" (
  "AAABJQDC" varchar(3) NOT NULL default ' ',
  "AAABJQCJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJQDC","AAABJQCJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYPI" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJQDC" varchar(3) NOT NULL default ' ',
  "AAABJQCJ" varchar(1) NOT NULL default ' ',
  "AAABDYPV" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABJQDC","AAABJQCJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYPQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJQDC" varchar(3) NOT NULL default ' ',
  "AAABJQCJ" varchar(2) NOT NULL default ' ',
  "AAABDYPV" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJQDC","AAABJQCJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYQA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJQDC" varchar(3) NOT NULL default ' ',
  "AAABDYQE" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJQDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYQI" (
  "AAABJOQG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABJOQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYQL" (
  "AAABJOQG" varchar(4) NOT NULL default ' ',
  "AAABJOQH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABJOQG","AAABJOQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYQQ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJOQG" varchar(4) NOT NULL default ' ',
  "AAABJOQH" varchar(4) NOT NULL default ' ',
  "AAABDYQU" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABJOQG","AAABJOQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYQY" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJOQG" varchar(4) NOT NULL default ' ',
  "AAABDYRB" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABJOQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYRE" (
  "AAABDYST" varchar(8) NOT NULL default ' ',
  "AAABKWVJ" varchar(10) NOT NULL default ' ',
  "AAABKWVR" varchar(10) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAABLPSU" varchar(30) NOT NULL default ' ',
  "AAABMKKY" varchar(8) NOT NULL default '00000000',
  "AAABMKLH" varchar(8) NOT NULL default '00000000',
  "AAABDYSH" varchar(8) NOT NULL default ' ',
  "AAABDYSI" varchar(30) NOT NULL default ' ',
  "AAABDYSJ" decimal(15,2) NOT NULL default '0.00',
  "AAABJABF" varchar(5) NOT NULL default ' ',
  "AAABDYSL" varchar(3) NOT NULL default '000',
  "AAABDYSM" varchar(2) NOT NULL default '00',
  "AAABDYSN" varchar(2) NOT NULL default '00',
  "AAABDYSO" varchar(2) NOT NULL default '00',
  "AAABDYSP" varchar(1) NOT NULL default '0',
  "AAABDYSQ" varchar(1) NOT NULL default '0',
  "AAABDYSR" varchar(1) NOT NULL default ' ',
  "AAABDYSS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDYST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYRZ" (
  "AAABDYST" varchar(8) NOT NULL default ' ',
  "AAABKWVJ" varchar(10) NOT NULL default ' ',
  "AAABKWVR" varchar(10) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAABLPSU" varchar(30) NOT NULL default ' ',
  "AAABMKKY" varchar(8) NOT NULL default '00000000',
  "AAABMKLH" varchar(8) NOT NULL default '00000000',
  "AAABDYSH" varchar(8) NOT NULL default ' ',
  "AAABDYSI" varchar(30) NOT NULL default ' ',
  "AAABDYSJ" decimal(15,2) NOT NULL default '0.00',
  "AAABJABF" varchar(5) NOT NULL default ' ',
  "AAABDYSL" varchar(3) NOT NULL default '000',
  "AAABDYSM" varchar(2) NOT NULL default '00',
  "AAABDYSN" varchar(2) NOT NULL default '00',
  "AAABDYSO" varchar(2) NOT NULL default '00',
  "AAABDYSP" varchar(1) NOT NULL default '0',
  "AAABDYSQ" varchar(1) NOT NULL default '0',
  "AAABDYSR" varchar(1) NOT NULL default ' ',
  "AAABDYSS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDYST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYSU" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLIMF" varchar(8) NOT NULL default ' ',
  "AAABDYSX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLIMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYTA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABDYTC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYTE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJPBG" varchar(3) NOT NULL default '000',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLCYD" varchar(1) NOT NULL default ' ',
  "AAABLCYP" decimal(13,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAABJPBG","AAABLDBT","AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYTP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDYUF" varchar(4) NOT NULL default '0000',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABIFMC" varchar(6) NOT NULL default ' ',
  "AAABIFJK" varchar(4) NOT NULL default ' ',
  "AAABDYTV" varchar(6) NOT NULL default ' ',
  "AAABDYTW" varchar(4) NOT NULL default ' ',
  "AAABEPQQ" varchar(4) NOT NULL default ' ',
  "AAABJMEB" varchar(1) NOT NULL default ' ',
  "AAABGUBO" varchar(3) NOT NULL default '000',
  "AAABDYUA" varchar(6) NOT NULL default ' ',
  "AAABIFJS" varchar(1) NOT NULL default ' ',
  "AAABEPIN" varchar(4) NOT NULL default ' ',
  "AAABECAI" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDYUF","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYUH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABEPQQ" varchar(4) NOT NULL default ' ',
  "AAABEPKH" varchar(30) NOT NULL default ' ',
  "AAABDYUS" varchar(10) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAABHSZC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABEPQQ","AAABEPKH","AAABDYUS","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYUU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABEPQQ" varchar(4) NOT NULL default ' ',
  "AAABEPKH" varchar(30) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  "AAABHHTF" varchar(2) NOT NULL default ' ',
  "AAABHHTG" varchar(1) NOT NULL default ' ',
  "AAABLPMM" varchar(2) NOT NULL default ' ',
  "AAABHHTI" varchar(20) NOT NULL default ' ',
  "AAABHHTJ" varchar(30) NOT NULL default ' ',
  "AAABHHTK" varchar(30) NOT NULL default ' ',
  "AAABHHTL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABEPQQ","AAABEPKH","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYVN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFPKA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYVR" (
  "AAABDYVX" varchar(30) NOT NULL default ' ',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMGHA" varchar(4) NOT NULL default '0000',
  "AAABDYVV" varchar(1) NOT NULL default ' ',
  "AAABLUVJ" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABDYVX"),
  KEY "AAABDYVY" ("AAABMNYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYWA" (
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABDYWH" varchar(8) NOT NULL default ' ',
  "AAABHZYF" varchar(8) NOT NULL default ' ',
  "AAABDYWJ" varchar(3) NOT NULL default '000',
  "AAABDYWF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLCPX","AAABDYWH","AAABHZYF","AAABDYWJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYWK" (
  "AAABDYWX" varchar(1) NOT NULL default ' ',
  "AAABDYWY" varchar(50) NOT NULL default ' ',
  "AAABDYWN" varchar(4) NOT NULL default ' ',
  "AAABDYWO" varchar(1) NOT NULL default ' ',
  "AAABDYWP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDYWX","AAABDYWY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYWS" (
  "AAABDYWX" varchar(1) NOT NULL default ' ',
  "AAABDYWY" varchar(50) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDYWW" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABDYWX","AAABDYWY","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYXA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABECGJ" varchar(1) NOT NULL default ' ',
  "AAABECGL" varchar(1) NOT NULL default ' ',
  "AAABECGM" varchar(1) NOT NULL default ' ',
  "AAABECGN" varchar(1) NOT NULL default ' ',
  "AAABLDQW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLCAK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYXM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABDYXP" varchar(1) NOT NULL default ' ',
  "AAABDYXQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYXT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDYYF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDYYF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYXY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDYYF" varchar(3) NOT NULL default ' ',
  "AAABJQTW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDYYF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYYG" (
  "AAABJPGE" varchar(1) NOT NULL default ' ',
  "AAABDYYS" varchar(20) NOT NULL default ' ',
  "AAABDYYT" varchar(1) NOT NULL default ' ',
  "AAABDYYU" varchar(1) NOT NULL default ' ',
  "AAABDYYV" varchar(1) NOT NULL default ' ',
  "AAABJQTP" varchar(1) NOT NULL default ' ',
  "AAABDYYX" varchar(2) NOT NULL default ' ',
  "AAABDYYY" varchar(1) NOT NULL default ' ',
  "AAABJPGG" varchar(1) NOT NULL default ' ',
  "AAABDYZA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJPGE","AAABDYYS","AAABDYYT","AAABDYYU","AAABDYYV","AAABJQTP","AAABDYYX","AAABDYYY","AAABJPGG","AAABDYZA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYZB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJPGR" varchar(1) NOT NULL default ' ',
  "AAABDYZG" varchar(1) NOT NULL default ' ',
  "AAABJPGW" varchar(1) NOT NULL default ' ',
  "AAABJPIB" varchar(1) NOT NULL default '0',
  "AAABJPGZ" varchar(2) NOT NULL default '00',
  "AAABJPHI" varchar(3) NOT NULL default '000',
  "AAABJPHA" varchar(1) NOT NULL default ' ',
  "AAABJPHB" varchar(2) NOT NULL default ' ',
  "AAABDYZN" varchar(1) NOT NULL default ' ',
  "AAABDYZO" varchar(1) NOT NULL default ' ',
  "AAABDYZP" varchar(1) NOT NULL default ' ',
  "AAABJPHN" varchar(1) NOT NULL default ' ',
  "AAABJPHT" varchar(1) NOT NULL default '0',
  "AAABJPHU" varchar(2) NOT NULL default ' ',
  "AAABJPHC" varchar(1) NOT NULL default ' ',
  "AAABJPHD" varchar(2) NOT NULL default ' ',
  "AAABJPHE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCAK","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDYZZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABECGJ" varchar(1) NOT NULL default ' ',
  "AAABECGM" varchar(1) NOT NULL default ' ',
  "AAABECGL" varchar(1) NOT NULL default ' ',
  "AAABECGN" varchar(1) NOT NULL default ' ',
  "AAABLDQW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLCAK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZAL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJPJW" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABJPJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZAQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJPJW" varchar(3) NOT NULL default '000',
  "AAABDZAU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJPJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZAY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDZBK" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABDZBK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZBD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDZBK" varchar(3) NOT NULL default '000',
  "AAABDZBH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDZBK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZBL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCJO" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZBQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCJP" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCJP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZBV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCJQ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCJQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZCA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJMCK" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJMCK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZCF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLCJO" varchar(6) NOT NULL default ' ',
  "AAABKTYZ" varchar(15) NOT NULL default ' ',
  "AAABKTZA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLCJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZCO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLCJP" varchar(6) NOT NULL default ' ',
  "AAABKTYZ" varchar(15) NOT NULL default ' ',
  "AAABKTZA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLCJP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZCX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLCJQ" varchar(6) NOT NULL default ' ',
  "AAABKTYZ" varchar(15) NOT NULL default ' ',
  "AAABKTZA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLCJQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZDG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJMCK" varchar(6) NOT NULL default ' ',
  "AAABKTYZ" varchar(15) NOT NULL default ' ',
  "AAABKTZA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJMCK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZDP" (
  "AAABDZDY" varchar(30) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDZDY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZDT" (
  "AAABDZDY" varchar(30) NOT NULL default ' ',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABFQBN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDZDY","AAABMNYT","AAABMGHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZEB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABECGJ" varchar(1) NOT NULL default ' ',
  "AAABDZEG" varchar(1) NOT NULL default ' ',
  "AAABDZEH" varchar(1) NOT NULL default ' ',
  "AAABECGO" varchar(1) NOT NULL default ' ',
  "AAABDZEJ" varchar(1) NOT NULL default ' ',
  "AAABDZEK" varchar(1) NOT NULL default ' ',
  "AAABECGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLCAK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZEP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJPJW" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABJPJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZEU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJPJW" varchar(3) NOT NULL default '000',
  "AAABJQTW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJPJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZFC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABJPJO" varchar(2) NOT NULL default ' ',
  "AAABDZFH" varchar(1) NOT NULL default ' ',
  "AAABJPJB" varchar(4) NOT NULL default ' ',
  "AAABDZFJ" varchar(1) NOT NULL default ' ',
  "AAABJPJF" decimal(12,8) NOT NULL default '0.00000000',
  "AAABDZFL" varchar(1) NOT NULL default ' ',
  "AAABJPIZ" varchar(1) NOT NULL default ' ',
  "AAABJPIY" varchar(1) NOT NULL default ' ',
  "AAABJPJA" varchar(2) NOT NULL default ' ',
  "AAABJPIW" varchar(1) NOT NULL default ' ',
  "AAABJPJV" varchar(2) NOT NULL default '00',
  "AAABDZFR" varchar(2) NOT NULL default ' ',
  "AAABJPJW" varchar(3) NOT NULL default '000',
  "AAABLDQW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLCAK","AAABJPJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZFY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABJPJO" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJQTW" varchar(40) NOT NULL default ' ',
  "AAABECJY" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLCAK","AAABJPJO","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZGL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDZHB" varchar(4) NOT NULL default ' ',
  "AAABDZHC" decimal(5,2) NOT NULL default '0.00',
  "AAABDZHD" decimal(5,2) NOT NULL default '0.00',
  "AAABDZGQ" varchar(1) NOT NULL default '0',
  "AAABDZGR" varchar(1) NOT NULL default '0',
  "AAABDZGS" varchar(1) NOT NULL default '0',
  "AAABDZGT" varchar(1) NOT NULL default '0',
  "AAABDZGU" varchar(1) NOT NULL default '0',
  "AAABDZGV" varchar(1) NOT NULL default '0',
  "AAABDZGW" varchar(1) NOT NULL default '0',
  "AAABDZGX" varchar(1) NOT NULL default '0',
  "AAABDZGY" varchar(1) NOT NULL default '0',
  "AAABDZGZ" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABDZHB","AAABDZHC","AAABDZHD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDZHK" varchar(8) NOT NULL default '00000000',
  "AAABEUWV" varchar(30) NOT NULL default ' ',
  "AAABDZHI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDZHK","AAABEUWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZHM" (
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABDZHO" varchar(10) NOT NULL default ' ',
  "AAABHYAG" varchar(10) NOT NULL default ' ',
  "AAABKZAW" varchar(60) NOT NULL default ' ',
  "AAABDZHR" varchar(1) NOT NULL default ' ',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABDZHT" varchar(4) NOT NULL default ' ',
  "AAABFVDJ" varchar(30) NOT NULL default ' ',
  "AAABLMNC" varchar(20) NOT NULL default ' ',
  "AAABJSPH" varchar(10) NOT NULL default ' ',
  "AAABDZHX" varchar(1) NOT NULL default ' ',
  "AAABIZKD" varchar(8) NOT NULL default ' ',
  "AAABDZIQ" varchar(30) NOT NULL default ' ',
  "AAABDZIA" varchar(2) NOT NULL default ' ',
  "AAABDZIB" varchar(4) NOT NULL default ' ',
  "AAABDZIC" varchar(1) NOT NULL default ' ',
  "AAABDZID" varchar(6) NOT NULL default ' ',
  "AAABDZIE" varchar(1) NOT NULL default ' ',
  "AAABGYNY" varchar(1) NOT NULL default ' ',
  "AAABDZIG" varchar(1) NOT NULL default ' ',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAABKNAR" varchar(8) NOT NULL default '00000000',
  "AAABLEBN" varchar(12) NOT NULL default ' ',
  "AAABKNCM" varchar(8) NOT NULL default '00000000',
  "AAABDZIL" varchar(1) NOT NULL default ' ',
  "AAABDZIM" varchar(1) NOT NULL default ' ',
  "AAABDZIN" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABIRUN"),
  KEY "AAABDZIP" ("AAABDZIQ"),
  KEY "AAABDZIR" ("AAABLMNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZIT" (
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKZAW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABIRUN","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZIZ" (
  "AAABLOZE" varchar(6) NOT NULL default ' ',
  "AAABLFYC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAABLFYC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZJE" (
  "AAABHEGP" varchar(10) NOT NULL default ' ',
  "AAABLFYC" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEGP","AAABLFYC","AAABLFYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZJL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDZKE" varchar(2) NOT NULL default '00',
  "AAABJPBG" varchar(3) NOT NULL default '000',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  "AAABDZKA" varchar(1) NOT NULL default ' ',
  "AAABDZKB" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABDZKE","AAABJPBG","AAABLDBT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZJW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDZKE" varchar(2) NOT NULL default '00',
  "AAABJPBG" varchar(3) NOT NULL default '000',
  "AAABDZKA" varchar(1) NOT NULL default ' ',
  "AAABDZKB" varchar(1) NOT NULL default '0',
  "AAABDZKC" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABDZKE","AAABJPBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESGI" varchar(2) NOT NULL default '00',
  "AAABERTG" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABESGI","AAABERTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZKN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABESGI" varchar(2) NOT NULL default '00',
  "AAABERTG" varchar(3) NOT NULL default '000',
  "AAABERTB" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABESGI","AAABERTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZKX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESGI" varchar(2) NOT NULL default '00',
  "AAABERYU" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESGI","AAABERYU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZLE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABESGI" varchar(2) NOT NULL default '00',
  "AAABERYU" varchar(3) NOT NULL default ' ',
  "AAABERTU" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABESGI","AAABERYU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZLO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESGI" varchar(2) NOT NULL default '00',
  "AAABERYU" varchar(3) NOT NULL default ' ',
  "AAABERYD" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESGI","AAABERYU","AAABERYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZLX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABESGI" varchar(2) NOT NULL default '00',
  "AAABERYU" varchar(3) NOT NULL default ' ',
  "AAABERYD" varchar(5) NOT NULL default ' ',
  "AAABERUO" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABESGI","AAABERYU","AAABERYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZMJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDZNC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDZNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZMO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDZMT" varchar(2) NOT NULL default '00',
  "AAABESGI" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABDZMT","AAABESGI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZMV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABDZNC" varchar(1) NOT NULL default ' ',
  "AAABDZMZ" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABDZNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZND" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESGI" varchar(2) NOT NULL default '00',
  "AAABERVO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESGI","AAABERVO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZNK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABESGI" varchar(2) NOT NULL default '00',
  "AAABERVO" varchar(4) NOT NULL default ' ',
  "AAABERVK" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABESGI","AAABERVO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZNU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZOA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLAUS" varchar(55) NOT NULL default ' ',
  "AAABKGYO" varchar(10) NOT NULL default ' ',
  "AAABKGYP" varchar(20) NOT NULL default ' ',
  "AAABKGYQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZOL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDZOX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDZOX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZOQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABDZOX" varchar(4) NOT NULL default ' ',
  "AAABDZOU" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABDZOX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZOY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDZPK" varchar(7) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDZPK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABDZPK" varchar(7) NOT NULL default ' ',
  "AAABDZPH" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABDZPK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZPL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABERWS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABERWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZPQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABERWS" varchar(4) NOT NULL default ' ',
  "AAABERWO" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABERWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDZQK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDZQK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZQD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABDZQK" varchar(12) NOT NULL default ' ',
  "AAABDZQH" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABDZQK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZQL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDZQX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDZQX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABDZQX" varchar(4) NOT NULL default ' ',
  "AAABDZQU" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABDZQX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDZRK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDZRK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZRD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABDZRK" varchar(4) NOT NULL default ' ',
  "AAABDZRH" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABDZRK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZRL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABERXG" varchar(2) NOT NULL default ' ',
  "AAABERWW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABERXG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZRR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABERXG" varchar(2) NOT NULL default ' ',
  "AAABERXD" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABERXG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZRZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRBY" varchar(3) NOT NULL default ' ',
  "AAABDZSC" varchar(1) NOT NULL default ' ',
  "AAABDZSD" varchar(1) NOT NULL default ' ',
  "AAABDZSE" varchar(1) NOT NULL default ' ',
  "AAABDZSF" varchar(1) NOT NULL default ' ',
  "AAABDZSG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZSJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABKRBY" varchar(3) NOT NULL default ' ',
  "AAABDZSN" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABKRBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--enable_query_log
