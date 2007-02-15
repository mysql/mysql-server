# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+
# |                             tbl29.inc                              |
# |                        Copyright 2006 by MySQL AB                  |
# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+

--disable_query_log
CREATE TABLE "AAAAZOXM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZPBQ" varchar(6) NOT NULL default ' ',
  "AAAAZOXQ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAAAZPBQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZOXU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZPBQ" varchar(6) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAAAZPBQ","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZOYD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZPBQ" varchar(6) NOT NULL default ' ',
  "AAAAZOYI" varchar(60) NOT NULL default ' ',
  "AAAAZOYJ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMP","AAAAZPBQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZOYO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHYAO" varchar(5) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHYAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZOYW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZOZV" varchar(5) NOT NULL default ' ',
  "AAAAZOZW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAAAZOZV","AAAAZOZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZOZF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZOZV" varchar(5) NOT NULL default ' ',
  "AAAAZOZW" varchar(2) NOT NULL default ' ',
  "AAABMJVD" varchar(40) NOT NULL default ' ',
  "AAABMJVE" varchar(40) NOT NULL default ' ',
  "AAABMJVF" varchar(40) NOT NULL default ' ',
  "AAABMJVG" varchar(40) NOT NULL default ' ',
  "AAABMJVH" varchar(40) NOT NULL default ' ',
  "AAABLTRV" varchar(40) NOT NULL default ' ',
  "AAABLTSB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMP","AAAAZOZV","AAAAZOZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZOZX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZPBQ" varchar(6) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAAAZPBS" varchar(1) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAAAZPAE" varchar(1) NOT NULL default ' ',
  "AAAAZPAF" varchar(2) NOT NULL default ' ',
  "AAAAZPAG" varchar(4) NOT NULL default ' ',
  "AAAAZPAH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAAAZPBQ","AAABMCVI","AAAAZPBS","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPAO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZPBQ" varchar(6) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAAAZPBS" varchar(1) NOT NULL default ' ',
  "AAABKQVZ" varchar(1) NOT NULL default ' ',
  "AAABDTOD" varchar(4) NOT NULL default ' ',
  "AAABKRTQ" varchar(2) NOT NULL default ' ',
  "AAABDJGM" varchar(1) NOT NULL default ' ',
  "AAABDPJE" varchar(1) NOT NULL default ' ',
  "AAABKRPW" varchar(1) NOT NULL default ' ',
  "AAABDPJG" varchar(1) NOT NULL default ' ',
  "AAABDPJC" varchar(1) NOT NULL default ' ',
  "AAABHPNU" varchar(1) NOT NULL default ' ',
  "AAAAZPBD" varchar(2) NOT NULL default '00',
  "AAABMCVU" decimal(5,0) NOT NULL default '0',
  "AAAAZPBF" varchar(1) NOT NULL default ' ',
  "AAAAZPBG" decimal(5,2) NOT NULL default '0.00',
  "AAAAZPBH" varchar(1) NOT NULL default ' ',
  "AAAAZPBI" decimal(3,0) NOT NULL default '0',
  "AAAAZPBJ" varchar(1) NOT NULL default ' ',
  "AAAAZPBK" varchar(1) NOT NULL default ' ',
  "AAAAZPBL" varchar(1) NOT NULL default ' ',
  "AAAAZPBM" varchar(1) NOT NULL default ' ',
  "AAAAZPBN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAAAZPBQ","AAABMCVI","AAAAZPBS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPBT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLXUE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAABLXUE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPCA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLXUE" varchar(2) NOT NULL default ' ',
  "AAAAZROJ" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAABLXUE","AAAAZROJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPCJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLXUE" varchar(2) NOT NULL default ' ',
  "AAABMAHQ" varchar(30) NOT NULL default ' ',
  "AAABMJVE" varchar(60) NOT NULL default ' ',
  "AAABMJVF" varchar(60) NOT NULL default ' ',
  "AAABMJVG" varchar(60) NOT NULL default ' ',
  "AAABMJVH" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMP","AAABLXUE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPCX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXHD" varchar(3) NOT NULL default ' ',
  "AAAAZPDG" varchar(2) NOT NULL default ' ',
  "AAABKMCD" varchar(3) NOT NULL default ' ',
  "AAAAZPDI" varchar(2) NOT NULL default ' ',
  "AAAAZRVO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXHD","AAAAZPDG","AAABKMCD","AAAAZPDI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPDJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJIKB" varchar(2) NOT NULL default ' ',
  "AAABJIJR" varchar(4) NOT NULL default ' ',
  "AAAAZPDN" varchar(1) NOT NULL default ' ',
  "AAAAZPDO" varchar(1) NOT NULL default ' ',
  "AAAAZPDP" varchar(1) NOT NULL default ' ',
  "AAAAZPDQ" varchar(4) NOT NULL default ' ',
  "AAAAZPDR" varchar(1) NOT NULL default ' ',
  "AAAAZPDS" varchar(1) NOT NULL default ' ',
  "AAAAZPDT" varchar(1) NOT NULL default ' ',
  "AAAAZQLA" varchar(1) NOT NULL default ' ',
  "AAAAZQLB" varchar(1) NOT NULL default ' ',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAAAZPDX" varchar(1) NOT NULL default ' ',
  "AAABLYCG" varchar(2) NOT NULL default ' ',
  "AAAAZPDZ" varchar(1) NOT NULL default ' ',
  "AAAAZPEA" varchar(1) NOT NULL default ' ',
  "AAAAZPEB" varchar(1) NOT NULL default ' ',
  "AAAAZPEC" varchar(1) NOT NULL default ' ',
  "AAAAZPED" varchar(1) NOT NULL default ' ',
  "AAAAZPEE" varchar(1) NOT NULL default ' ',
  "AAAAZPEF" varchar(1) NOT NULL default ' ',
  "AAAAZPEG" varchar(1) NOT NULL default ' ',
  "AAAAZPEH" varchar(1) NOT NULL default ' ',
  "AAAAZPHX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJIKB","AAABJIJR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPEM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXHD" varchar(3) NOT NULL default ' ',
  "AAABJIKB" varchar(2) NOT NULL default ' ',
  "AAAAZPEQ" varchar(1) NOT NULL default ' ',
  "AAAAZPER" varchar(1) NOT NULL default ' ',
  "AAAAZPES" varchar(1) NOT NULL default ' ',
  "AAAAZPET" varchar(2) NOT NULL default ' ',
  "AAAAZPEU" varchar(2) NOT NULL default ' ',
  "AAAAZPEV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXHD","AAABJIKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPEZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJIKB" varchar(2) NOT NULL default ' ',
  "AAABJIJP" varchar(14) NOT NULL default ' ',
  "AAAAZRSO" varchar(17) NOT NULL default ' ',
  "AAAAZPFE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJIKB","AAABJIJP","AAAAZRSO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPFJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJIKB" varchar(2) NOT NULL default ' ',
  "AAABJIJQ" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJIKB","AAABJIJQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPFQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJIKB" varchar(2) NOT NULL default ' ',
  "AAABJIJR" varchar(4) NOT NULL default ' ',
  "AAAAZPFZ" varchar(17) NOT NULL default ' ',
  "AAABJIKE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJIKB","AAABJIJR","AAAAZPFZ","AAABJIKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPGB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJIKB" varchar(2) NOT NULL default ' ',
  "AAABJIJP" varchar(14) NOT NULL default ' ',
  "AAABJIJX" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJIKB","AAABJIJP","AAABJIJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJIKB" varchar(2) NOT NULL default ' ',
  "AAABJIJP" varchar(14) NOT NULL default ' ',
  "AAABJIJX" varchar(5) NOT NULL default ' ',
  "AAAAZPPR" varchar(60) NOT NULL default ' ',
  "AAAAZPPS" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJIKB","AAABJIJP","AAABJIJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPGX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJIKB" varchar(2) NOT NULL default ' ',
  "AAAAZPOQ" varchar(2) NOT NULL default ' ',
  "AAAAZPHB" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABJIKB","AAAAZPOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPHF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJIKB" varchar(2) NOT NULL default ' ',
  "AAAAZPHI" varchar(1) NOT NULL default ' ',
  "AAAAZPHJ" varchar(1) NOT NULL default ' ',
  "AAAAZPHK" varchar(1) NOT NULL default ' ',
  "AAAAZPHL" varchar(1) NOT NULL default ' ',
  "AAAAZPHM" varchar(1) NOT NULL default ' ',
  "AAAAZPHN" varchar(1) NOT NULL default ' ',
  "AAAAZPHO" varchar(3) NOT NULL default ' ',
  "AAAAZPHP" varchar(1) NOT NULL default ' ',
  "AAAAZPHQ" varchar(1) NOT NULL default ' ',
  "AAAAZPHR" varchar(1) NOT NULL default ' ',
  "AAAAZPHS" varchar(1) NOT NULL default ' ',
  "AAAAZQHX" varchar(2) NOT NULL default ' ',
  "AAAAZPHU" varchar(1) NOT NULL default ' ',
  "AAAAZPHV" varchar(1) NOT NULL default ' ',
  "AAAAZPHW" varchar(2) NOT NULL default ' ',
  "AAAAZPHX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJIKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPIA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJIKB" varchar(2) NOT NULL default ' ',
  "AAABJIJP" varchar(14) NOT NULL default ' ',
  "AAAAZPIE" varchar(1) NOT NULL default ' ',
  "AAAAZPKK" varchar(1) NOT NULL default ' ',
  "AAAAZPKL" varchar(1) NOT NULL default ' ',
  "AAAAZPKM" varchar(1) NOT NULL default ' ',
  "AAAAZPKN" varchar(1) NOT NULL default ' ',
  "AAAAZPKO" varchar(1) NOT NULL default ' ',
  "AAAAZPKP" varchar(1) NOT NULL default ' ',
  "AAAAZPKQ" varchar(1) NOT NULL default ' ',
  "AAAAZPKR" varchar(1) NOT NULL default ' ',
  "AAAAZPKS" varchar(1) NOT NULL default ' ',
  "AAAAZPKT" varchar(1) NOT NULL default ' ',
  "AAAAZPKU" varchar(1) NOT NULL default ' ',
  "AAAAZPKV" varchar(1) NOT NULL default ' ',
  "AAAAZPKW" varchar(1) NOT NULL default ' ',
  "AAAAZPKX" varchar(1) NOT NULL default ' ',
  "AAAAZPKY" varchar(1) NOT NULL default ' ',
  "AAAAZPKZ" varchar(1) NOT NULL default ' ',
  "AAAAZPLA" varchar(1) NOT NULL default ' ',
  "AAAAZPLB" varchar(1) NOT NULL default ' ',
  "AAAAZPLC" varchar(1) NOT NULL default ' ',
  "AAAAZPLD" varchar(1) NOT NULL default ' ',
  "AAAAZPLE" varchar(1) NOT NULL default ' ',
  "AAAAZPLF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJIKB","AAABJIJP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPJE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJIKB" varchar(2) NOT NULL default ' ',
  "AAABJIJP" varchar(14) NOT NULL default ' ',
  "AAABJIJQ" varchar(14) NOT NULL default ' ',
  "AAAAZPJJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJIKB","AAABJIJP","AAABJIJQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPJO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZPNB" varchar(3) NOT NULL default ' ',
  "AAAAZPND" varchar(8) NOT NULL default '00000000',
  "AAAAZPMZ" varchar(1) NOT NULL default ' ',
  "AAAAZPJT" varchar(1) NOT NULL default ' ',
  "AAAAZPJU" varchar(1) NOT NULL default ' ',
  "AAAAZPJV" varchar(1) NOT NULL default ' ',
  "AAAAZPJW" varchar(1) NOT NULL default ' ',
  "AAAAZPJX" varchar(1) NOT NULL default ' ',
  "AAAAZPJY" varchar(5) NOT NULL default ' ',
  "AAAAZPJZ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZPNB","AAAAZPND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPKD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZPNB" varchar(3) NOT NULL default ' ',
  "AAABJIKB" varchar(2) NOT NULL default ' ',
  "AAAAZPOQ" varchar(2) NOT NULL default ' ',
  "AAAAZPND" varchar(8) NOT NULL default '00000000',
  "AAABHKSE" varchar(3) NOT NULL default '000',
  "AAAAZPKK" varchar(1) NOT NULL default ' ',
  "AAAAZPKL" varchar(1) NOT NULL default ' ',
  "AAAAZPKM" varchar(1) NOT NULL default ' ',
  "AAAAZPKN" varchar(1) NOT NULL default ' ',
  "AAAAZPKO" varchar(1) NOT NULL default ' ',
  "AAAAZPKP" varchar(1) NOT NULL default ' ',
  "AAAAZPKQ" varchar(1) NOT NULL default ' ',
  "AAAAZPKR" varchar(1) NOT NULL default ' ',
  "AAAAZPKS" varchar(1) NOT NULL default ' ',
  "AAAAZPKT" varchar(1) NOT NULL default ' ',
  "AAAAZPKU" varchar(1) NOT NULL default ' ',
  "AAAAZPKV" varchar(1) NOT NULL default ' ',
  "AAAAZPKW" varchar(1) NOT NULL default ' ',
  "AAAAZPKX" varchar(1) NOT NULL default ' ',
  "AAAAZPKY" varchar(1) NOT NULL default ' ',
  "AAAAZPKZ" varchar(1) NOT NULL default ' ',
  "AAAAZPLA" varchar(1) NOT NULL default ' ',
  "AAAAZPLB" varchar(1) NOT NULL default ' ',
  "AAAAZPLC" varchar(1) NOT NULL default ' ',
  "AAAAZPLD" varchar(1) NOT NULL default ' ',
  "AAAAZPLE" varchar(1) NOT NULL default ' ',
  "AAAAZPLF" varchar(1) NOT NULL default ' ',
  "AAAAZPLG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZPNB","AAABJIKB","AAAAZPOQ","AAAAZPND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPLM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZPNB" varchar(3) NOT NULL default ' ',
  "AAABJIKB" varchar(2) NOT NULL default ' ',
  "AAABJIJQ" varchar(14) NOT NULL default ' ',
  "AAAAZPND" varchar(8) NOT NULL default '00000000',
  "AAAAZPNQ" varchar(5) NOT NULL default '00000',
  "AAABJIJR" varchar(4) NOT NULL default ' ',
  "AAABHKSE" varchar(3) NOT NULL default '000',
  "AAAAZPMM" decimal(15,2) NOT NULL default '0.00',
  "AAAAZPMN" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZPNB","AAABJIKB","AAABJIJQ","AAAAZPND","AAAAZPNQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPMD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJIKB" varchar(2) NOT NULL default ' ',
  "AAAAZPOQ" varchar(2) NOT NULL default ' ',
  "AAABJIJQ" varchar(14) NOT NULL default ' ',
  "AAAAZPND" varchar(8) NOT NULL default '00000000',
  "AAAAZPNQ" varchar(5) NOT NULL default '00000',
  "AAABJIJR" varchar(4) NOT NULL default ' ',
  "AAABHKSE" varchar(3) NOT NULL default '000',
  "AAAAZPMM" decimal(15,2) NOT NULL default '0.00',
  "AAAAZPMN" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJIKB","AAAAZPOQ","AAABJIJQ","AAAAZPND","AAAAZPNQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPMU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZPNB" varchar(3) NOT NULL default ' ',
  "AAABLXHD" varchar(3) NOT NULL default ' ',
  "AAAAZPND" varchar(8) NOT NULL default '00000000',
  "AAAAZPMZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZPNB","AAABLXHD","AAAAZPND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPNE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZPNY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZPNY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPNJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZPNY" varchar(2) NOT NULL default ' ',
  "AAAAZPNQ" varchar(5) NOT NULL default '00000',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZPNY","AAAAZPNQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPNR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZPNY" varchar(2) NOT NULL default ' ',
  "AAABMAHQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZPNY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPNZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJIKB" varchar(2) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJIKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPOH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJIKB" varchar(2) NOT NULL default ' ',
  "AAAAZPOQ" varchar(2) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJIKB","AAAAZPOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPOR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJIKB" varchar(2) NOT NULL default ' ',
  "AAABJIJR" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJIKB","AAABJIJR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPPB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJIKB" varchar(2) NOT NULL default ' ',
  "AAABJIJQ" varchar(14) NOT NULL default ' ',
  "AAABMAHQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJIKB","AAABJIJQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPPL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJIKB" varchar(2) NOT NULL default ' ',
  "AAABJIJP" varchar(14) NOT NULL default ' ',
  "AAABMAHQ" varchar(30) NOT NULL default ' ',
  "AAAAZPPR" varchar(60) NOT NULL default ' ',
  "AAAAZPPS" varchar(60) NOT NULL default ' ',
  "AAAAZPPT" varchar(60) NOT NULL default ' ',
  "AAAAZPPU" varchar(60) NOT NULL default ' ',
  "AAAAZPPV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJIKB","AAABJIJP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPQA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJIKB" varchar(2) NOT NULL default ' ',
  "AAABJIJR" varchar(4) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABJIKE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJIKB","AAABJIJR","AAABMLCZ","AAABJIKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPQL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJIKB" varchar(2) NOT NULL default ' ',
  "AAABJIJR" varchar(4) NOT NULL default ' ',
  "AAAAZPQU" varchar(3) NOT NULL default ' ',
  "AAABJIKE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJIKB","AAABJIJR","AAAAZPQU","AAABJIKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPQW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJIKB" varchar(2) NOT NULL default ' ',
  "AAABJIJR" varchar(4) NOT NULL default ' ',
  "AAABJIJP" varchar(14) NOT NULL default ' ',
  "AAABJIKE" varchar(10) NOT NULL default ' ',
  "AAABJIJX" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJIKB","AAABJIJR","AAABJIJP","AAABJIKE","AAABJIJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPRJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJIKB" varchar(2) NOT NULL default ' ',
  "AAABJIJR" varchar(4) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABJIKE" varchar(10) NOT NULL default ' ',
  "AAAAZPRP" varchar(1) NOT NULL default ' ',
  "AAAAZPRQ" varchar(1) NOT NULL default ' ',
  "AAAAZPRR" varchar(1) NOT NULL default ' ',
  "AAAAZPRS" varchar(1) NOT NULL default ' ',
  "AAAAZPRT" varchar(1) NOT NULL default ' ',
  "AAAAZPRU" varchar(1) NOT NULL default ' ',
  "AAAAZPRV" varchar(1) NOT NULL default ' ',
  "AAAAZPRW" varchar(1) NOT NULL default ' ',
  "AAAAZPSK" varchar(6) NOT NULL default ' ',
  "AAAAZPSL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJIKB","AAABJIJR","AAABMPLT","AAABJIKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPSE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJIKB" varchar(2) NOT NULL default ' ',
  "AAABJIJR" varchar(4) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABJIKE" varchar(10) NOT NULL default ' ',
  "AAAAZPSK" varchar(6) NOT NULL default ' ',
  "AAAAZPSL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJIKB","AAABJIJR","AAABMCSR","AAABJIKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPSR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLGJS" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLGJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPSW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLGJS" varchar(2) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLGJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPTE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZPYT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZPYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPTJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZPYT" varchar(1) NOT NULL default ' ',
  "AAAAZPTN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZPYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPTR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZPUN" varchar(2) NOT NULL default ' ',
  "AAAAZPTU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZPUN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPTX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZPUN" varchar(2) NOT NULL default ' ',
  "AAAAZPUB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZPUN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPUF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZPXO" varchar(2) NOT NULL default ' ',
  "AAAAZPUN" varchar(2) NOT NULL default ' ',
  "AAAAZPUJ" decimal(3,0) NOT NULL default '0',
  "AAAAZPUK" decimal(3,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAZPXO","AAAAZPUN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPUO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZPXO" varchar(2) NOT NULL default ' ',
  "AAAAZPUR" varchar(1) NOT NULL default ' ',
  "AAAAZPUS" varchar(1) NOT NULL default ' ',
  "AAAAZPUT" varchar(1) NOT NULL default ' ',
  "AAAAZPUU" varchar(1) NOT NULL default ' ',
  "AAAAZPUV" varchar(1) NOT NULL default ' ',
  "AAAAZPUW" varchar(1) NOT NULL default ' ',
  "AAAAZPUX" varchar(1) NOT NULL default ' ',
  "AAAAZPUY" varchar(1) NOT NULL default ' ',
  "AAAAZPUZ" varchar(1) NOT NULL default ' ',
  "AAAAZPVA" varchar(1) NOT NULL default ' ',
  "AAAAZPVB" varchar(1) NOT NULL default ' ',
  "AAAAZPVC" varchar(1) NOT NULL default ' ',
  "AAAAZPVD" varchar(1) NOT NULL default ' ',
  "AAAAZPVE" varchar(1) NOT NULL default ' ',
  "AAAAZPVF" varchar(1) NOT NULL default ' ',
  "AAAAZPVG" varchar(1) NOT NULL default ' ',
  "AAAAZPVH" varchar(1) NOT NULL default ' ',
  "AAAAZPVI" varchar(1) NOT NULL default ' ',
  "AAAAZPVJ" varchar(1) NOT NULL default ' ',
  "AAAAZPVK" varchar(1) NOT NULL default ' ',
  "AAAAZPVL" varchar(1) NOT NULL default ' ',
  "AAAAZPVM" varchar(1) NOT NULL default ' ',
  "AAAAZPVN" varchar(1) NOT NULL default ' ',
  "AAAAZPVO" varchar(1) NOT NULL default ' ',
  "AAAAZPVP" varchar(1) NOT NULL default ' ',
  "AAAAZPVQ" varchar(1) NOT NULL default ' ',
  "AAAAZPVR" varchar(1) NOT NULL default ' ',
  "AAAAZPVS" varchar(1) NOT NULL default ' ',
  "AAAAZPVT" varchar(1) NOT NULL default ' ',
  "AAAAZPVU" varchar(1) NOT NULL default ' ',
  "AAAAZPVV" varchar(1) NOT NULL default ' ',
  "AAAAZPVW" varchar(1) NOT NULL default ' ',
  "AAAAZPVX" varchar(1) NOT NULL default ' ',
  "AAAAZPVY" varchar(1) NOT NULL default ' ',
  "AAABLYCG" varchar(2) NOT NULL default ' ',
  "AAAAZPWA" varchar(1) NOT NULL default ' ',
  "AAAAZPWB" varchar(1) NOT NULL default ' ',
  "AAAAZPXI" varchar(1) NOT NULL default ' ',
  "AAAAZPWD" varchar(1) NOT NULL default ' ',
  "AAAAZPWE" varchar(1) NOT NULL default ' ',
  "AAAAZPWF" varchar(1) NOT NULL default ' ',
  "AAAAZPWG" varchar(1) NOT NULL default ' ',
  "AAAAZPWH" varchar(1) NOT NULL default ' ',
  "AAAAZPWI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZPXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPWL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZPXO" varchar(2) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZPXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPWT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZPXI" varchar(1) NOT NULL default ' ',
  "AAAAZPXJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZPXI","AAAAZPXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPXA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZPXI" varchar(1) NOT NULL default ' ',
  "AAAAZPXJ" varchar(1) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZPXI","AAAAZPXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPXK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLGJS" varchar(2) NOT NULL default ' ',
  "AAAAZTYJ" varchar(1) NOT NULL default ' ',
  "AAAAZPXO" varchar(2) NOT NULL default ' ',
  "AAAAZPYS" varchar(1) NOT NULL default ' ',
  "AAAAZPXQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLGJS","AAAAZTYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPXU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZPYS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZPYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPXZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZPYS" varchar(1) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZPYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPYH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZPYS" varchar(1) NOT NULL default ' ',
  "AAAAZPYT" varchar(1) NOT NULL default ' ',
  "AAAAZPYL" tinyint(3) unsigned NOT NULL default '0',
  "AAAAZPYM" varchar(1) NOT NULL default ' ',
  "AAAAZPYN" varchar(1) NOT NULL default ' ',
  "AAAAZPYO" varchar(1) NOT NULL default ' ',
  "AAAAZPYP" varchar(1) NOT NULL default ' ',
  "AAAAZPYQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZPYS","AAAAZPYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZPYU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZQHX" varchar(2) NOT NULL default ' ',
  "AAAAZPYX" varchar(1) NOT NULL default ' ',
  "AAAAZPYY" varchar(3) NOT NULL default ' ',
  "AAAAZPYZ" varchar(1) NOT NULL default ' ',
  "AAAAZPZA" varchar(3) NOT NULL default ' ',
  "AAAAZPZB" varchar(1) NOT NULL default ' ',
  "AAAAZPZC" varchar(3) NOT NULL default ' ',
  "AAAAZPZD" varchar(1) NOT NULL default ' ',
  "AAAAZPZE" varchar(3) NOT NULL default ' ',
  "AAAAZPZF" varchar(1) NOT NULL default ' ',
  "AAAAZPZG" varchar(3) NOT NULL default ' ',
  "AAAAZPZH" varchar(1) NOT NULL default ' ',
  "AAAAZPZI" varchar(3) NOT NULL default ' ',
  "AAAAZPZJ" varchar(1) NOT NULL default ' ',
  "AAAAZPZK" varchar(1) NOT NULL default ' ',
  "AAAAZPZL" varchar(1) NOT NULL default ' ',
  "AAAAZPZM" varchar(1) NOT NULL default ' ',
  "AAAAZPZN" varchar(1) NOT NULL default ' ',
  "AAAAZPZO" varchar(1) NOT NULL default ' ',
  "AAAAZPZP" varchar(1) NOT NULL default ' ',
  "AAAAZPZQ" varchar(1) NOT NULL default ' ',
  "AAAAZPZR" varchar(3) NOT NULL default '000',
  "AAAAZPZS" varchar(1) NOT NULL default ' ',
  "AAAAZPZT" varchar(1) NOT NULL default ' ',
  "AAAAZPZU" varchar(1) NOT NULL default ' ',
  "AAAAZPZV" varchar(1) NOT NULL default ' ',
  "AAAAZPZW" varchar(1) NOT NULL default ' ',
  "AAAAZPZX" varchar(1) NOT NULL default ' ',
  "AAAAZPZY" varchar(1) NOT NULL default ' ',
  "AAAAZPZZ" varchar(1) NOT NULL default ' ',
  "AAAAZQAA" varchar(1) NOT NULL default ' ',
  "AAAAZQAB" varchar(1) NOT NULL default ' ',
  "AAAAZQAC" varchar(1) NOT NULL default ' ',
  "AAAAZQAD" varchar(1) NOT NULL default ' ',
  "AAAAZQAE" varchar(1) NOT NULL default ' ',
  "AAAAZQAF" varchar(1) NOT NULL default ' ',
  "AAAAZQAG" varchar(1) NOT NULL default ' ',
  "AAAAZQAH" varchar(1) NOT NULL default ' ',
  "AAAAZQAI" varchar(1) NOT NULL default ' ',
  "AAAAZQAJ" varchar(1) NOT NULL default ' ',
  "AAAAZQAK" varchar(1) NOT NULL default ' ',
  "AAAAZQAL" varchar(1) NOT NULL default ' ',
  "AAAAZQAM" varchar(1) NOT NULL default ' ',
  "AAAAZQAN" varchar(1) NOT NULL default ' ',
  "AAAAZQAO" varchar(1) NOT NULL default ' ',
  "AAAAZQAP" varchar(1) NOT NULL default ' ',
  "AAAAZQAQ" varchar(1) NOT NULL default ' ',
  "AAAAZQAR" varchar(3) NOT NULL default ' ',
  "AAAAZQAS" varchar(3) NOT NULL default ' ',
  "AAAAZQAT" varchar(3) NOT NULL default ' ',
  "AAAAZQAU" varchar(3) NOT NULL default ' ',
  "AAAAZQAV" varchar(3) NOT NULL default ' ',
  "AAAAZQAW" varchar(3) NOT NULL default ' ',
  "AAABHRJN" decimal(5,2) NOT NULL default '0.00',
  "AAAAZQAY" decimal(5,2) NOT NULL default '0.00',
  "AAAAZQAZ" decimal(5,2) NOT NULL default '0.00',
  "AAAAZQBA" decimal(5,2) NOT NULL default '0.00',
  "AAAAZQBB" decimal(5,2) NOT NULL default '0.00',
  "AAAAZQBC" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAZQHX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQBF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZQHA" varchar(5) NOT NULL default ' ',
  "AAAAZQBN" varchar(40) NOT NULL default ' ',
  "AAAAZQBO" varchar(40) NOT NULL default ' ',
  "AAAAZQBK" varchar(7) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZQHA","AAAAZQBN","AAAAZQBO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQBP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZQHA" varchar(5) NOT NULL default ' ',
  "AAAAZQBS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZQHA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQBV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZQHA" varchar(5) NOT NULL default ' ',
  "AAAAZQCB" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZQHA","AAAAZQCB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQCC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZQHA" varchar(5) NOT NULL default ' ',
  "AAABJIKB" varchar(2) NOT NULL default ' ',
  "AAAAZQCG" varchar(10) NOT NULL default ' ',
  "AAAAZQCH" varchar(60) NOT NULL default ' ',
  "AAAAZQCI" varchar(60) NOT NULL default ' ',
  "AAAAZQCJ" varchar(60) NOT NULL default ' ',
  "AAAAZQCK" decimal(4,0) NOT NULL default '0',
  "AAAAZQCL" decimal(4,0) NOT NULL default '0',
  "AAAAZQCM" decimal(2,0) NOT NULL default '0',
  "AAAAZQCN" decimal(2,0) NOT NULL default '0',
  "AAAAZQCO" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAZQHA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQCR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZQHA" varchar(5) NOT NULL default ' ',
  "AAABMAHQ" varchar(60) NOT NULL default ' ',
  "AAAAZQCW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZQHA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQDA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZQHA" varchar(5) NOT NULL default ' ',
  "AAAAZQHB" varchar(40) NOT NULL default ' ',
  "AAAAZQHC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZQHA","AAAAZQHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQDI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZQHA" varchar(5) NOT NULL default ' ',
  "AAAAZQHB" varchar(40) NOT NULL default ' ',
  "AAAAZQHC" varchar(40) NOT NULL default ' ',
  "AAAAZQDN" varchar(2) NOT NULL default ' ',
  "AAAAZQDO" varchar(2) NOT NULL default ' ',
  "AAAAZQDP" varchar(2) NOT NULL default ' ',
  "AAAAZQDQ" varchar(2) NOT NULL default ' ',
  "AAAAZQDR" varchar(2) NOT NULL default ' ',
  "AAAAZQDS" varchar(2) NOT NULL default ' ',
  "AAAAZQDT" varchar(2) NOT NULL default ' ',
  "AAAAZQDU" varchar(2) NOT NULL default ' ',
  "AAAAZQDV" varchar(2) NOT NULL default ' ',
  "AAAAZQDW" varchar(2) NOT NULL default ' ',
  "AAAAZQDX" varchar(10) NOT NULL default ' ',
  "AAAAZQDY" varchar(10) NOT NULL default ' ',
  "AAAAZQDZ" varchar(10) NOT NULL default ' ',
  "AAAAZQEA" varchar(10) NOT NULL default ' ',
  "AAAAZQEB" varchar(10) NOT NULL default ' ',
  "AAAAZQEC" varchar(10) NOT NULL default ' ',
  "AAAAZQED" varchar(10) NOT NULL default ' ',
  "AAAAZQEE" varchar(10) NOT NULL default ' ',
  "AAAAZQEF" varchar(10) NOT NULL default ' ',
  "AAAAZQEG" varchar(10) NOT NULL default ' ',
  "AAAAZQEH" varchar(10) NOT NULL default ' ',
  "AAAAZQEI" varchar(10) NOT NULL default ' ',
  "AAAAZQEJ" varchar(10) NOT NULL default ' ',
  "AAAAZQEK" varchar(10) NOT NULL default ' ',
  "AAAAZQEL" varchar(10) NOT NULL default ' ',
  "AAAAZQEM" varchar(10) NOT NULL default ' ',
  "AAAAZQEN" varchar(10) NOT NULL default ' ',
  "AAAAZQEO" varchar(10) NOT NULL default ' ',
  "AAAAZQEP" varchar(10) NOT NULL default ' ',
  "AAAAZQEQ" varchar(10) NOT NULL default ' ',
  "AAAAZQER" varchar(10) NOT NULL default ' ',
  "AAAAZQES" varchar(10) NOT NULL default ' ',
  "AAAAZQET" varchar(10) NOT NULL default ' ',
  "AAAAZQEU" varchar(10) NOT NULL default ' ',
  "AAAAZQEV" varchar(10) NOT NULL default ' ',
  "AAAAZQEW" varchar(10) NOT NULL default ' ',
  "AAAAZQEX" varchar(10) NOT NULL default ' ',
  "AAAAZQEY" varchar(10) NOT NULL default ' ',
  "AAAAZQEZ" varchar(10) NOT NULL default ' ',
  "AAAAZQFA" varchar(10) NOT NULL default ' ',
  "AAAAZQFB" varchar(10) NOT NULL default ' ',
  "AAAAZQFC" varchar(10) NOT NULL default ' ',
  "AAAAZQFD" varchar(10) NOT NULL default ' ',
  "AAAAZQFE" varchar(10) NOT NULL default ' ',
  "AAAAZQFF" varchar(10) NOT NULL default ' ',
  "AAAAZQFG" varchar(10) NOT NULL default ' ',
  "AAAAZQFH" varchar(10) NOT NULL default ' ',
  "AAAAZQFI" varchar(10) NOT NULL default ' ',
  "AAAAZQFJ" varchar(10) NOT NULL default ' ',
  "AAAAZQFK" varchar(10) NOT NULL default ' ',
  "AAAAZQFL" varchar(10) NOT NULL default ' ',
  "AAAAZQFM" varchar(10) NOT NULL default ' ',
  "AAAAZQFN" varchar(10) NOT NULL default ' ',
  "AAAAZQFO" varchar(10) NOT NULL default ' ',
  "AAAAZQFP" varchar(10) NOT NULL default ' ',
  "AAAAZQFQ" varchar(10) NOT NULL default ' ',
  "AAAAZQFR" varchar(10) NOT NULL default ' ',
  "AAAAZQFS" varchar(10) NOT NULL default ' ',
  "AAAAZQFT" varchar(10) NOT NULL default ' ',
  "AAAAZQFU" varchar(10) NOT NULL default ' ',
  "AAAAZQFV" varchar(10) NOT NULL default ' ',
  "AAAAZQFW" varchar(10) NOT NULL default ' ',
  "AAAAZQFX" varchar(10) NOT NULL default ' ',
  "AAAAZQFY" varchar(10) NOT NULL default ' ',
  "AAAAZQFZ" varchar(10) NOT NULL default ' ',
  "AAAAZQGA" varchar(10) NOT NULL default ' ',
  "AAAAZQGB" varchar(10) NOT NULL default ' ',
  "AAAAZQGC" varchar(10) NOT NULL default ' ',
  "AAAAZQGD" varchar(10) NOT NULL default ' ',
  "AAAAZQGE" varchar(10) NOT NULL default ' ',
  "AAAAZQGF" varchar(10) NOT NULL default ' ',
  "AAAAZQGG" varchar(10) NOT NULL default ' ',
  "AAAAZQGH" varchar(10) NOT NULL default ' ',
  "AAAAZQGI" varchar(10) NOT NULL default ' ',
  "AAAAZQGJ" varchar(10) NOT NULL default ' ',
  "AAAAZQGK" varchar(10) NOT NULL default ' ',
  "AAAAZQGL" varchar(10) NOT NULL default ' ',
  "AAAAZQGM" varchar(10) NOT NULL default ' ',
  "AAAAZQGN" varchar(10) NOT NULL default ' ',
  "AAAAZQGO" varchar(10) NOT NULL default ' ',
  "AAAAZQGP" varchar(10) NOT NULL default ' ',
  "AAAAZQGQ" varchar(10) NOT NULL default ' ',
  "AAAAZQGR" varchar(10) NOT NULL default ' ',
  "AAAAZQGS" varchar(10) NOT NULL default ' ',
  "AAAAZQGT" varchar(10) NOT NULL default ' ',
  "AAAAZQGU" varchar(10) NOT NULL default ' ',
  "AAAAZQGV" varchar(10) NOT NULL default ' ',
  "AAAAZQGW" varchar(10) NOT NULL default ' ',
  "AAAAZQGX" varchar(10) NOT NULL default ' ',
  "AAAAZQGY" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZQHA","AAAAZQHB","AAAAZQHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQHD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZQHP" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZQHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQHI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZQHP" varchar(6) NOT NULL default ' ',
  "AAABMAHQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZQHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQHQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZQHX" varchar(2) NOT NULL default ' ',
  "AAABMAHQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZQHX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQHY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHOTE" varchar(2) NOT NULL default ' ',
  "AAAAZQKZ" varchar(1) NOT NULL default ' ',
  "AAAAZQLA" varchar(1) NOT NULL default ' ',
  "AAAAZQLB" varchar(1) NOT NULL default ' ',
  "AAAAZQLC" varchar(1) NOT NULL default ' ',
  "AAAAZQIF" varchar(1) NOT NULL default ' ',
  "AAAAZQIG" varchar(1) NOT NULL default ' ',
  "AAAAZQIH" varchar(1) NOT NULL default ' ',
  "AAAAZQII" varchar(1) NOT NULL default ' ',
  "AAAAZQIJ" varchar(1) NOT NULL default ' ',
  "AAAAZQIK" varchar(1) NOT NULL default ' ',
  "AAAAZQIL" varchar(1) NOT NULL default ' ',
  "AAAAZQIM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHOTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQIP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHOTE" varchar(2) NOT NULL default ' ',
  "AAABMJVD" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHOTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQIX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZQRR" varchar(1) NOT NULL default ' ',
  "AAABHOTI" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZQRR","AAABHOTI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQJE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZQRR" varchar(1) NOT NULL default ' ',
  "AAABHOTI" varchar(3) NOT NULL default ' ',
  "AAABMAHQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZQRR","AAABHOTI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQJO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZQSC" varchar(2) NOT NULL default ' ',
  "AAABHNOT" varchar(30) NOT NULL default ' ',
  "AAABHNOU" varchar(30) NOT NULL default ' ',
  "AAAAZQJT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZQSC","AAABHNOT","AAABHNOU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQJY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZQKT" varchar(9) NOT NULL default ' ',
  "AAAAZQKC" varchar(8) NOT NULL default ' ',
  "AAAAZQKD" varchar(8) NOT NULL default ' ',
  "AAABELVQ" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAAAZQKT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQKI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZQKT" varchar(9) NOT NULL default ' ',
  "AAABMJVD" varchar(60) NOT NULL default ' ',
  "AAABMJVE" varchar(60) NOT NULL default ' ',
  "AAABMJVF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMP","AAAAZQKT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQKU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABHNOT" varchar(30) NOT NULL default ' ',
  "AAABHNOU" varchar(30) NOT NULL default ' ',
  "AAAAZQKZ" varchar(1) NOT NULL default ' ',
  "AAAAZQLA" varchar(1) NOT NULL default ' ',
  "AAAAZQLB" varchar(1) NOT NULL default ' ',
  "AAAAZQLC" varchar(1) NOT NULL default ' ',
  "AAAAZRQA" varchar(1) NOT NULL default ' ',
  "AAAAZQLE" varchar(1) NOT NULL default ' ',
  "AAAAZQLF" varchar(1) NOT NULL default ' ',
  "AAAAZQLG" varchar(1) NOT NULL default ' ',
  "AAABFXCI" varchar(1) NOT NULL default ' ',
  "AAAAZQLI" varchar(1) NOT NULL default ' ',
  "AAAAZQLJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAABHNOT","AAABHNOU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQLO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFZVT" varchar(1) NOT NULL default ' ',
  "AAAAZQMU" varchar(3) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  "AAAAZQLU" varchar(1) NOT NULL default ' ',
  "AAAAZQLV" varchar(1) NOT NULL default ' ',
  "AAAAZQLW" varchar(1) NOT NULL default ' ',
  "AAABENTQ" varchar(2) NOT NULL default ' ',
  "AAABEQXE" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFZVT","AAAAZQMU","AAABMAMP","AAABMQNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQME" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFZVT" varchar(1) NOT NULL default ' ',
  "AAAAZQMU" varchar(3) NOT NULL default ' ',
  "AAAAZRQA" varchar(1) NOT NULL default '0',
  "AAAAZQMJ" varchar(1) NOT NULL default ' ',
  "AAAAZQMK" varchar(1) NOT NULL default ' ',
  "AAAAZQML" varchar(1) NOT NULL default ' ',
  "AAAAZQMM" varchar(1) NOT NULL default ' ',
  "AAAAZQMN" varchar(241) NOT NULL default ' ',
  "AAABLPWW" varchar(70) NOT NULL default ' ',
  "AAABLOXS" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHWIL" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABFZVT","AAAAZQMU","AAAAZRQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQMW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZQRK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZQRK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQNB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZQRK" varchar(3) NOT NULL default ' ',
  "AAABMAHQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZQRK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQNJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAAAZQNO" varchar(1) NOT NULL default ' ',
  "AAAAZQNP" varchar(1) NOT NULL default ' ',
  "AAAAZQNQ" varchar(2) NOT NULL default ' ',
  "AAABGENP" varchar(10) NOT NULL default ' ',
  "AAAAZQNS" varchar(1) NOT NULL default ' ',
  "AAAAZQNT" varchar(1) NOT NULL default ' ',
  "AAAAZQNU" varchar(1) NOT NULL default ' ',
  "AAAAZRCR" varchar(10) NOT NULL default ' ',
  "AAAAZREB" varchar(10) NOT NULL default ' ',
  "AAAAZRFN" varchar(10) NOT NULL default ' ',
  "AAAAZQNY" varchar(1) NOT NULL default ' ',
  "AAAAZQNZ" varchar(1) NOT NULL default ' ',
  "AAAAZRFS" varchar(10) NOT NULL default ' ',
  "AAAAZREY" decimal(3,0) NOT NULL default '0',
  "AAAAZQOC" varchar(1) NOT NULL default ' ',
  "AAAAZQOD" varchar(1) NOT NULL default ' ',
  "AAAAZQOE" varchar(1) NOT NULL default ' ',
  "AAAAZQOF" varchar(10) NOT NULL default ' ',
  "AAABJIKB" varchar(2) NOT NULL default ' ',
  "AAABJIJR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQOM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABFNHW" varchar(40) NOT NULL default ' ',
  "AAABMRCU" varchar(2) NOT NULL default '00',
  "AAABHNOU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQIJ","AAABMAMY","AAABFNHW","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQOY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABFNHW" varchar(40) NOT NULL default ' ',
  "AAABMBTQ" varchar(16) NOT NULL default ' ',
  "AAABHNOT" varchar(30) NOT NULL default ' ',
  "AAAAZQPF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQIJ","AAABMAMY","AAABFNHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQPK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABFNHW" varchar(40) NOT NULL default ' ',
  "AAAAZQPQ" varchar(90) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMQIJ","AAABMAMY","AAABFNHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQPW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZQQE" varchar(1) NOT NULL default ' ',
  "AAAAZRQA" varchar(1) NOT NULL default ' ',
  "AAAAZREG" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAAAZQQE","AAAAZRQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQQG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZRQA" varchar(1) NOT NULL default '0',
  "AAAAZQQK" varchar(1) NOT NULL default ' ',
  "AAAAZQRD" varchar(2) NOT NULL default ' ',
  "AAAAZQRE" varchar(2) NOT NULL default ' ',
  "AAAAZQRF" varchar(1) NOT NULL default ' ',
  "AAAAZQRG" varchar(1) NOT NULL default ' ',
  "AAAAZQQP" varchar(2) NOT NULL default '00',
  "AAAAZQRH" varchar(1) NOT NULL default ' ',
  "AAAAZQRK" varchar(3) NOT NULL default ' ',
  "AAAAZQQS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAAAZRQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQQW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZRQA" varchar(1) NOT NULL default '0',
  "AAAAZQRK" varchar(3) NOT NULL default ' ',
  "AAAAZQRL" varchar(3) NOT NULL default ' ',
  "AAAAZQRR" varchar(1) NOT NULL default ' ',
  "AAABHOTI" varchar(3) NOT NULL default ' ',
  "AAAAZQRD" varchar(2) NOT NULL default ' ',
  "AAAAZQRE" varchar(2) NOT NULL default ' ',
  "AAAAZQRF" varchar(1) NOT NULL default ' ',
  "AAAAZQRG" varchar(1) NOT NULL default ' ',
  "AAAAZQRH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZRQA","AAAAZQRK","AAAAZQRL","AAAAZQRR","AAABHOTI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQRO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZQSC" varchar(2) NOT NULL default ' ',
  "AAAAZQRR" varchar(1) NOT NULL default ' ',
  "AAAAZRQA" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAZQSC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQRV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZQSC" varchar(2) NOT NULL default ' ',
  "AAABMJVD" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZQSC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQSD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZQVG" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZRLZ","AAAAZQVG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQSK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZQVG" varchar(6) NOT NULL default ' ',
  "AAABMAHQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZRLZ","AAAAZQVG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQSU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZRFV" varchar(20) NOT NULL default ' ',
  "AAAAZRQA" varchar(1) NOT NULL default '0',
  "AAAAZQSZ" varchar(2) NOT NULL default ' ',
  "AAAAZQTA" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAAAZRFV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQTE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZRFV" varchar(20) NOT NULL default ' ',
  "AAABMJVD" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMP","AAAAZRFV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQTO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZQWD" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAAAZQWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZQWD" varchar(15) NOT NULL default ' ',
  "AAABMJVD" varchar(60) NOT NULL default ' ',
  "AAABMJVE" varchar(60) NOT NULL default ' ',
  "AAABMJVF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMP","AAAAZQWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQUH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZQVW" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZRLZ","AAAAZQVW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQUO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZQVW" varchar(6) NOT NULL default ' ',
  "AAABMAHQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZRLZ","AAAAZQVW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQUY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZRSO" varchar(17) NOT NULL default ' ',
  "AAAAZQYQ" varchar(3) NOT NULL default ' ',
  "AAAAZQVN" varchar(10) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAAAZQVG" varchar(6) NOT NULL default ' ',
  "AAABJTPP" varchar(3) NOT NULL default ' ',
  "AAABHKSE" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAAAZRLZ","AAAAZRSO","AAAAZQYQ","AAAAZQVN","AAABMCSR","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQVQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZRSO" varchar(17) NOT NULL default ' ',
  "AAAAZQWD" varchar(15) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAAAZQVW" varchar(6) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAAAZQYL" varchar(1) NOT NULL default ' ',
  "AAABJTPP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZRLZ","AAAAZRSO","AAAAZQWD","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQWF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZRSO" varchar(17) NOT NULL default ' ',
  "AAAAZQYP" varchar(3) NOT NULL default ' ',
  "AAAAZQYQ" varchar(3) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAAAZRCR" varchar(10) NOT NULL default ' ',
  "AAAAZQWN" varchar(10) NOT NULL default ' ',
  "AAAAZQWO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZRLZ","AAAAZRSO","AAAAZQYP","AAAAZQYQ","AAABMLCZ","AAAAZRCR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQWW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZRSO" varchar(17) NOT NULL default ' ',
  "AAAAZQYQ" varchar(3) NOT NULL default ' ',
  "AAAAZROJ" varchar(8) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAAAZRAS" varchar(6) NOT NULL default ' ',
  "AAAAZQYL" varchar(1) NOT NULL default ' ',
  "AAABJTPP" varchar(3) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABHKSE" varchar(3) NOT NULL default '000',
  "AAAAZRAI" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZRLZ","AAAAZRSO","AAAAZQYQ","AAAAZROJ","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQXP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZRSO" varchar(17) NOT NULL default ' ',
  "AAAAZQYP" varchar(3) NOT NULL default ' ',
  "AAAAZQYQ" varchar(3) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABJTPP" varchar(3) NOT NULL default ' ',
  "AAAAZQYL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZRLZ","AAAAZRSO","AAAAZQYP","AAAAZQYQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQYD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZRSO" varchar(17) NOT NULL default ' ',
  "AAAAZQYP" varchar(3) NOT NULL default ' ',
  "AAAAZQYQ" varchar(3) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABJTPP" varchar(3) NOT NULL default ' ',
  "AAAAZQYL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZRLZ","AAAAZRSO","AAAAZQYP","AAAAZQYQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQYR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZRSO" varchar(17) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABJTPP" varchar(3) NOT NULL default ' ',
  "AAABFZWB" decimal(5,2) NOT NULL default '0.00',
  "AAAAZQYZ" varchar(8) NOT NULL default '00000000',
  "AAAAZQZA" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAZRLZ","AAAAZRSO","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQZF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZRAS" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZRLZ","AAAAZRAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQZM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZRAH" varchar(2) NOT NULL default ' ',
  "AAAAZRAI" varchar(10) NOT NULL default ' ',
  "AAAAZQZR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZRLZ","AAAAZRAH","AAAAZRAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZQZW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZRAH" varchar(2) NOT NULL default ' ',
  "AAAAZRAI" varchar(10) NOT NULL default ' ',
  "AAAAZRBF" varchar(60) NOT NULL default ' ',
  "AAAAZRBG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZRLZ","AAAAZRAH","AAAAZRAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRAJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZRAS" varchar(6) NOT NULL default ' ',
  "AAABMAHQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZRLZ","AAAAZRAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRAT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZRBU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZRLZ","AAAAZRBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRBA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZRBU" varchar(10) NOT NULL default ' ',
  "AAAAZRBF" varchar(60) NOT NULL default ' ',
  "AAAAZRBG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZRLZ","AAAAZRBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRBL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZRBU" varchar(10) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZRBW" varchar(8) NOT NULL default '00000000',
  "AAAAZRBX" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAZRLZ","AAAAZRBU","AAABMAMP","AAAAZRBW","AAAAZRBX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRBY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZRCR" varchar(10) NOT NULL default ' ',
  "AAAAZREY" decimal(3,0) NOT NULL default '0',
  "AAAAZREZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAAAZRCR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRCH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZRCR" varchar(10) NOT NULL default ' ',
  "AAABMJVD" varchar(60) NOT NULL default ' ',
  "AAABMJVE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMP","AAAAZRCR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRCS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZRFS" varchar(10) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZRFS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRCZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZRFS" varchar(10) NOT NULL default ' ',
  "AAAAZRDD" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZRFS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRDH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZREB" varchar(10) NOT NULL default ' ',
  "AAAAZREY" decimal(3,0) NOT NULL default '0',
  "AAAAZREZ" varchar(4) NOT NULL default ' ',
  "AAABJTPP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZRLZ","AAAAZREB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRDR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZREB" varchar(10) NOT NULL default ' ',
  "AAABMJVD" varchar(60) NOT NULL default ' ',
  "AAABMJVE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZRLZ","AAAAZREB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZREC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZREJ" varchar(1) NOT NULL default ' ',
  "AAAAZREG" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZRLZ","AAAAZREJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZREK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZRES" varchar(4) NOT NULL default ' ',
  "AAABLXZF" varchar(1) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZRLZ","AAAAZRES","AAABLXZF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZREU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZRFN" varchar(10) NOT NULL default ' ',
  "AAAAZREY" decimal(3,0) NOT NULL default '0',
  "AAAAZREZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAAAZRFN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRFD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZRFN" varchar(10) NOT NULL default ' ',
  "AAABMJVD" varchar(60) NOT NULL default ' ',
  "AAABMJVE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMP","AAAAZRFN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRFO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAAAZRFV" varchar(20) NOT NULL default ' ',
  "AAAAZRFS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAAAZRFV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRFW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAAAZRQA" varchar(1) NOT NULL default '0',
  "AAAAZRGK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCSR","AAABMPLT","AAABMAMP","AAABMALG","AAAAZRQA","AAAAZRGK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRGL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZRMA" varchar(8) NOT NULL default ' ',
  "AAAAZRJE" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZRLZ","AAAAZRMA","AAAAZRJE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRGU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZRMA" varchar(8) NOT NULL default ' ',
  "AAAAZRJE" varchar(6) NOT NULL default ' ',
  "AAABMAHQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZRLZ","AAAAZRMA","AAAAZRJE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRHG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZRHW" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZRLZ","AAAAZRHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRHN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZRHW" varchar(6) NOT NULL default ' ',
  "AAABMAHQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZRLZ","AAAAZRHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRHX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZRMA" varchar(8) NOT NULL default ' ',
  "AAAAZRJD" varchar(4) NOT NULL default ' ',
  "AAAAZRIC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZRLZ","AAAAZRMA","AAAAZRJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRIH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZRMA" varchar(8) NOT NULL default ' ',
  "AAAAZRJD" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZRLZ","AAAAZRMA","AAAAZRJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRIT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZRMA" varchar(8) NOT NULL default ' ',
  "AAAAZRJD" varchar(4) NOT NULL default ' ',
  "AAAAZRJE" varchar(6) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZRLZ","AAAAZRMA","AAAAZRJD","AAAAZRJE","AAABMQIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRJG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZRJW" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZRLZ","AAAAZRJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZRJW" varchar(5) NOT NULL default ' ',
  "AAABMAHQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZRLZ","AAAAZRJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRJX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZRKN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZRLZ","AAAAZRKN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRKE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZRKN" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZRLZ","AAAAZRKN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZRMA" varchar(8) NOT NULL default ' ',
  "AAAAZRLI" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZRLZ","AAAAZRMA","AAAAZRLI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRKX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZRMA" varchar(8) NOT NULL default ' ',
  "AAAAZRLI" varchar(3) NOT NULL default ' ',
  "AAABMAHQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZRLZ","AAAAZRMA","AAAAZRLI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRLJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZRMA" varchar(8) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZRLZ","AAAAZRMA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRLR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZRLZ" varchar(3) NOT NULL default ' ',
  "AAAAZRMA" varchar(8) NOT NULL default ' ',
  "AAABMAHQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZRLZ","AAAAZRMA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRMB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZRRY" varchar(6) NOT NULL default ' ',
  "AAAAZRMF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAAAZRRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRMJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZRRY" varchar(6) NOT NULL default ' ',
  "AAABMAHQ" varchar(30) NOT NULL default ' ',
  "AAABMJVE" varchar(60) NOT NULL default ' ',
  "AAABMJVF" varchar(60) NOT NULL default ' ',
  "AAABMJVG" varchar(60) NOT NULL default ' ',
  "AAABMJVH" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMP","AAAAZRRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRMX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXHD" varchar(3) NOT NULL default ' ',
  "AAAAZRNG" varchar(6) NOT NULL default ' ',
  "AAABKMCD" varchar(3) NOT NULL default ' ',
  "AAAAZRNI" varchar(6) NOT NULL default ' ',
  "AAAAZRVO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXHD","AAAAZRNG","AAABKMCD","AAAAZRNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRNJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZROJ" varchar(8) NOT NULL default ' ',
  "AAAAZRNN" varchar(1) NOT NULL default ' ',
  "AAAAZRNO" varchar(1) NOT NULL default ' ',
  "AAABAZAS" varchar(2) NOT NULL default ' ',
  "AAAAZRNQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAAAZROJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRNU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZROJ" varchar(8) NOT NULL default ' ',
  "AAABMAHQ" varchar(30) NOT NULL default ' ',
  "AAABMJVE" varchar(60) NOT NULL default ' ',
  "AAABMJVF" varchar(60) NOT NULL default ' ',
  "AAABMJVG" varchar(60) NOT NULL default ' ',
  "AAABMJVH" varchar(60) NOT NULL default ' ',
  "AAABLTRV" varchar(60) NOT NULL default ' ',
  "AAABLTSB" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMP","AAAAZROJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZROK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXHD" varchar(3) NOT NULL default ' ',
  "AAAAZROT" varchar(8) NOT NULL default ' ',
  "AAABKMCD" varchar(3) NOT NULL default ' ',
  "AAAAZROV" varchar(8) NOT NULL default ' ',
  "AAAAZRVO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXHD","AAAAZROT","AAABKMCD","AAAAZROV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZROW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZRQA" varchar(1) NOT NULL default '0',
  "AAABLXHD" varchar(3) NOT NULL default ' ',
  "AAAAZRQC" varchar(3) NOT NULL default ' ',
  "AAABKMCD" varchar(3) NOT NULL default ' ',
  "AAAAZRQE" varchar(3) NOT NULL default ' ',
  "AAABHPPT" varchar(1) NOT NULL default ' ',
  "AAAAZRPE" varchar(1) NOT NULL default ' ',
  "AAAAZRRY" varchar(6) NOT NULL default ' ',
  "AAAAZRPG" varchar(6) NOT NULL default ' ',
  "AAAAZRPH" varchar(6) NOT NULL default ' ',
  "AAAAZRPI" varchar(3) NOT NULL default ' ',
  "AAAAZRPJ" varchar(6) NOT NULL default ' ',
  "AAAAZRPK" varchar(3) NOT NULL default ' ',
  "AAAAZRPL" varchar(6) NOT NULL default ' ',
  "AAAAZRPM" varchar(3) NOT NULL default ' ',
  "AAAAZRPN" varchar(6) NOT NULL default ' ',
  "AAAAZRPO" varchar(3) NOT NULL default ' ',
  "AAAAZRPP" varchar(6) NOT NULL default ' ',
  "AAAAZRPQ" varchar(3) NOT NULL default ' ',
  "AAAAZRPR" varchar(6) NOT NULL default ' ',
  "AAAAZRPS" varchar(3) NOT NULL default ' ',
  "AAAAZRPT" varchar(6) NOT NULL default ' ',
  "AAAAZRPU" varchar(30) NOT NULL default ' ',
  "AAAAZRPV" varchar(30) NOT NULL default ' ',
  "AAAAZRPW" varchar(3) NOT NULL default ' ',
  "AAABDJEU" varchar(3) NOT NULL default ' ',
  "AAAAZRPY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZRQA","AAABLXHD","AAAAZRQC","AAABKMCD","AAAAZRQE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRQF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABHPPT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAABHPPT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRQM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZRTM" varchar(6) NOT NULL default ' ',
  "AAABMRCU" varchar(5) NOT NULL default '00000',
  "AAAAZRQR" varchar(6) NOT NULL default ' ',
  "AAABLOQL" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAAAZRTM","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRQX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZRTQ" varchar(6) NOT NULL default ' ',
  "AAAAZRSP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAAAZRTQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRRF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZRTQ" varchar(6) NOT NULL default ' ',
  "AAABMJVD" varchar(40) NOT NULL default ' ',
  "AAABMJVE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMP","AAAAZRTQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRRQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABHPPT" varchar(1) NOT NULL default ' ',
  "AAAAZRRY" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAABHPPT","AAAAZRRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRRZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZRUF" varchar(12) NOT NULL default ' ',
  "AAAAZRWO" varchar(4) NOT NULL default ' ',
  "AAAAZRWP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAAAZRUF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRSI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZRTM" varchar(6) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAAAZRTQ" varchar(6) NOT NULL default ' ',
  "AAAAZRSO" varchar(17) NOT NULL default ' ',
  "AAAAZRSP" varchar(20) NOT NULL default ' ',
  "AAAAZRSQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAZRSR" varchar(5) NOT NULL default ' ',
  "AAAAZRSS" decimal(13,3) NOT NULL default '0.000',
  "AAAAZRST" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAAAZRTM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRSX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZRTM" varchar(6) NOT NULL default ' ',
  "AAABMAHQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMP","AAAAZRTM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRTH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZRTQ" varchar(6) NOT NULL default ' ',
  "AAABMRCU" varchar(5) NOT NULL default '00000',
  "AAAAZRTM" varchar(6) NOT NULL default ' ',
  "AAABLOQL" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAAAZRTQ","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRTS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZRUF" varchar(12) NOT NULL default ' ',
  "AAABMJVD" varchar(60) NOT NULL default ' ',
  "AAABMJVE" varchar(60) NOT NULL default ' ',
  "AAABMJVF" varchar(60) NOT NULL default ' ',
  "AAABMJVG" varchar(60) NOT NULL default ' ',
  "AAABMJVH" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMP","AAAAZRUF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRUG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZRWO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAAAZRWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRUN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZRWO" varchar(4) NOT NULL default ' ',
  "AAABMJVD" varchar(40) NOT NULL default ' ',
  "AAABMJVE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMP","AAAAZRWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRUY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABHPPT" varchar(1) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMP","AAABHPPT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRVI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXHD" varchar(3) NOT NULL default ' ',
  "AAAAZRVR" varchar(1) NOT NULL default ' ',
  "AAABKMCD" varchar(3) NOT NULL default ' ',
  "AAAAZRVT" varchar(1) NOT NULL default ' ',
  "AAAAZRVO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXHD","AAAAZRVR","AAABKMCD","AAAAZRVT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZRWO" varchar(4) NOT NULL default ' ',
  "AAAAZRWP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAAAZRWO","AAAAZRWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRWD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZRWO" varchar(4) NOT NULL default ' ',
  "AAAAZRWP" varchar(4) NOT NULL default ' ',
  "AAABMJVD" varchar(40) NOT NULL default ' ',
  "AAABMJVE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMP","AAAAZRWO","AAAAZRWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRWQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSHU" varchar(4) NOT NULL default ' ',
  "AAAAZRWT" varchar(5) NOT NULL default '00000',
  "AAABHYJM" varchar(2) NOT NULL default ' ',
  "AAABHYJN" varchar(2) NOT NULL default ' ',
  "AAABIDJL" varchar(4) NOT NULL default ' ',
  "AAABLMDT" varchar(4) NOT NULL default ' ',
  "AAAAZRWY" varchar(4) NOT NULL default ' ',
  "AAAAZRWZ" varchar(4) NOT NULL default ' ',
  "AAABLMDY" varchar(1) NOT NULL default ' ',
  "AAABLMDZ" varchar(1) NOT NULL default ' ',
  "AAABLMEA" varchar(1) NOT NULL default ' ',
  "AAABJRCJ" varchar(1) NOT NULL default ' ',
  "AAABLMDV" varchar(4) NOT NULL default ' ',
  "AAAAZRXF" varchar(20) NOT NULL default ' ',
  "AAAAZRXG" varchar(20) NOT NULL default ' ',
  "AAABLMEB" varchar(4) NOT NULL default ' ',
  "AAAAZRXI" varchar(6) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAAAZRXL" varchar(2) NOT NULL default ' ',
  "AAAAZRXM" varchar(20) NOT NULL default ' ',
  "AAAAZRXN" varchar(1) NOT NULL default ' ',
  "AAAAZRXO" varchar(1) NOT NULL default ' ',
  "AAABLUHE" varchar(1) NOT NULL default ' ',
  "AAABLMEC" varchar(1) NOT NULL default ' ',
  "AAAAZRXR" varchar(1) NOT NULL default ' ',
  "AAAAZRXS" varchar(1) NOT NULL default ' ',
  "AAAAZRXT" varchar(1) NOT NULL default ' ',
  "AAAAZRXU" varchar(1) NOT NULL default ' ',
  "AAAAZRXV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSHU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRXY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLSHU" varchar(4) NOT NULL default ' ',
  "AAAAZRYC" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLSHU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRYG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMDT" varchar(4) NOT NULL default ' ',
  "AAABLMED" varchar(1) NOT NULL default ' ',
  "AAAAZRYK" varchar(1) NOT NULL default ' ',
  "AAAAZRYL" varchar(1) NOT NULL default ' ',
  "AAAAZRYM" varchar(1) NOT NULL default ' ',
  "AAABLZZR" varchar(4) NOT NULL default ' ',
  "AAABLGSX" varchar(1) NOT NULL default ' ',
  "AAABMDBO" varchar(4) NOT NULL default ' ',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  "AAAAZRYR" varchar(4) NOT NULL default ' ',
  "AAAAZRYS" varchar(4) NOT NULL default ' ',
  "AAAAZRYT" varchar(1) NOT NULL default ' ',
  "AAAAZRYU" varchar(1) NOT NULL default ' ',
  "AAAAZRYV" varchar(1) NOT NULL default ' ',
  "AAAAZRYW" varchar(1) NOT NULL default ' ',
  "AAAAZRYX" varchar(1) NOT NULL default ' ',
  "AAAAZRYY" varchar(1) NOT NULL default ' ',
  "AAAAZRYZ" varchar(1) NOT NULL default ' ',
  "AAAAZRZA" varchar(4) NOT NULL default ' ',
  "AAAAZRZB" varchar(1) NOT NULL default ' ',
  "AAAAZRZC" varchar(1) NOT NULL default ' ',
  "AAAAZRZD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMDT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRZG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLMDT" varchar(4) NOT NULL default ' ',
  "AAAAZRZK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLMDT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZRZO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZSAM" varchar(4) NOT NULL default ' ',
  "AAABHYJM" varchar(2) NOT NULL default ' ',
  "AAABHYJN" varchar(2) NOT NULL default ' ',
  "AAAAZRZT" varchar(3) NOT NULL default ' ',
  "AAABLMDZ" varchar(1) NOT NULL default ' ',
  "AAABLMEA" varchar(1) NOT NULL default ' ',
  "AAAAZRZW" varchar(1) NOT NULL default ' ',
  "AAAAZRZX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZSAM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSAA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZSAM" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZSAE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZSAM","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSAI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMDER" varchar(1) NOT NULL default ' ',
  "AAAAZSAM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSAP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZSAX" varchar(10) NOT NULL default '0000000000',
  "AAAAZSAY" varchar(10) NOT NULL default '0000000000',
  "AAABKLUK" varchar(3) NOT NULL default '000',
  "AAAAZSAU" varchar(1) NOT NULL default ' ',
  "AAABLSFX" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZSAX","AAAAZSAY","AAABKLUK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSBA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZSBV" varchar(1) NOT NULL default ' ',
  "AAABGBAU" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZSBV","AAABGBAU","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSBJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZSBV" varchar(1) NOT NULL default ' ',
  "AAAAZSBM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZSBV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSBP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZSBV" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZSBT" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZSBV","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSBX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMEB" varchar(4) NOT NULL default ' ',
  "AAAAZSCA" varchar(1) NOT NULL default ' ',
  "AAAAZSCB" varchar(2) NOT NULL default '00',
  "AAAAZSCC" varchar(1) NOT NULL default ' ',
  "AAAAZSCD" varchar(1) NOT NULL default ' ',
  "AAAAZSCE" varchar(1) NOT NULL default ' ',
  "AAAAZSCF" varchar(2) NOT NULL default '00',
  "AAAAZSCG" tinyint(3) unsigned NOT NULL default '0',
  "AAAAZSCH" tinyint(3) unsigned NOT NULL default '0',
  "AAAAZSCI" varchar(1) NOT NULL default ' ',
  "AAAAZSCJ" varchar(1) NOT NULL default ' ',
  "AAAAZSCK" varchar(1) NOT NULL default ' ',
  "AAAAZSCL" varchar(1) NOT NULL default ' ',
  "AAABJBQX" varchar(1) NOT NULL default ' ',
  "AAAAZSCN" varchar(1) NOT NULL default ' ',
  "AAAAZSCO" varchar(1) NOT NULL default ' ',
  "AAAAZSCP" varchar(1) NOT NULL default ' ',
  "AAAAZSCQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMEB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSCT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLMEB" varchar(4) NOT NULL default ' ',
  "AAAAZSCX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLMEB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSDB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMDV" varchar(4) NOT NULL default ' ',
  "AAABHVJV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMDV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSDH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLMDV" varchar(4) NOT NULL default ' ',
  "AAAAZSDL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLMDV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSDP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZSEI" varchar(2) NOT NULL default ' ',
  "AAAAZSEJ" varchar(1) NOT NULL default ' ',
  "AAAAZSDT" varchar(1) NOT NULL default ' ',
  "AAAAZSDU" varchar(1) NOT NULL default ' ',
  "AAAAZSDV" varchar(1) NOT NULL default ' ',
  "AAAAZSDW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZSEI","AAAAZSEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSEA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZSEI" varchar(2) NOT NULL default ' ',
  "AAAAZSEJ" varchar(1) NOT NULL default ' ',
  "AAAAZSEF" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZSEI","AAAAZSEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSEK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAAAZSEN" varchar(1) NOT NULL default ' ',
  "AAAAZSEO" varchar(1) NOT NULL default ' ',
  "AAAAZSEP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRSQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSES" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLRSQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSFA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRVS" varchar(1) NOT NULL default ' ',
  "AAAAZSFU" varchar(2) NOT NULL default ' ',
  "AAAAZSFE" varchar(1) NOT NULL default ' ',
  "AAAAZSFF" varchar(1) NOT NULL default ' ',
  "AAAAZSFG" varchar(1) NOT NULL default ' ',
  "AAAAZSFH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRVS","AAAAZSFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSFL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKRVS" varchar(1) NOT NULL default ' ',
  "AAAAZSFU" varchar(2) NOT NULL default ' ',
  "AAAAZSFQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKRVS","AAAAZSFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSFV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZSGC" varchar(10) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAAAZSGC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSGD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIFOV" varchar(3) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABAXPP" varchar(3) NOT NULL default ' ',
  "AAAAZSGI" varchar(3) NOT NULL default ' ',
  "AAAAZSGJ" varchar(3) NOT NULL default ' ',
  "AAAAZSGK" varchar(3) NOT NULL default ' ',
  "AAAAZSGL" varchar(3) NOT NULL default ' ',
  "AAAAZSGM" varchar(3) NOT NULL default ' ',
  "AAAAZSGN" varchar(3) NOT NULL default ' ',
  "AAAAZSGO" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIFOV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSGR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZSHD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZSHD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSGW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZSHD" varchar(1) NOT NULL default ' ',
  "AAAAZSHA" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZSHD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZSHQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZSHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSHJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZSHQ" varchar(2) NOT NULL default ' ',
  "AAABBANY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZSHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSHR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZSID" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZSID")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSHW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZSID" varchar(5) NOT NULL default ' ',
  "AAAAZSIA" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZSID")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSIE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZSIQ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZSIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSIJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZSIQ" varchar(3) NOT NULL default ' ',
  "AAAAZSIN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZSIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSIR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZSJD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZSJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZSJD" varchar(2) NOT NULL default ' ',
  "AAABBANY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZSJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSJE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMNJF" varchar(1) NOT NULL default ' ',
  "AAAAZSJO" varchar(4) NOT NULL default ' ',
  "AAAAZSJJ" varchar(4) NOT NULL default ' ',
  "AAAAZSJK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPLT","AAABMNJF","AAAAZSJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSJP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABKPBA" varchar(5) NOT NULL default ' ',
  "AAAAZTKE" varchar(1) NOT NULL default ' ',
  "AAAAZSJU" varchar(1) NOT NULL default ' ',
  "AAAAZSJV" decimal(3,1) NOT NULL default '0.0',
  "AAAAZSJW" varchar(1) NOT NULL default ' ',
  "AAAAZSJX" decimal(3,1) NOT NULL default '0.0',
  "AAAAZSJY" varchar(1) NOT NULL default ' ',
  "AAAAZSJZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAZSKA" varchar(1) NOT NULL default ' ',
  "AAAAZSKB" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMPLT","AAABKPBA","AAAAZTKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCSR" varchar(17) NOT NULL default ' ',
  "AAABLXWT" varchar(30) NOT NULL default ' ',
  "AAABLXWL" varchar(25) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCSR","AAABLXWT","AAABLXWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSKQ" (
  "AAAAZSLA" varchar(2) NOT NULL default ' ',
  "AAAAZSKS" varchar(1) NOT NULL default ' ',
  "AAAAZSKT" varchar(1) NOT NULL default ' ',
  "AAAAZSKU" varchar(1) NOT NULL default ' ',
  "AAAAZSKV" varchar(1) NOT NULL default ' ',
  "AAAAZSKW" varchar(1) NOT NULL default ' ',
  "AAAAZSKX" varchar(1) NOT NULL default ' ',
  "AAAAZSKY" varchar(1) NOT NULL default ' ',
  "AAAAZSKZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAZSLA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSLB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABLXWL" varchar(25) NOT NULL default ' ',
  "AAAAZSLF" varchar(8) NOT NULL default ' ',
  "AAAAZSMM" varchar(2) NOT NULL default ' ',
  "AAAAZSML" varchar(1) NOT NULL default ' ',
  "AAAAZSLI" varchar(4) NOT NULL default ' ',
  "AAAAZSLJ" varchar(1) NOT NULL default ' ',
  "AAAAZSLK" varchar(1) NOT NULL default ' ',
  "AAAAZSLL" varchar(1) NOT NULL default ' ',
  "AAAAZSLM" varchar(1) NOT NULL default ' ',
  "AAAAZSLN" varchar(1) NOT NULL default ' ',
  "AAAAZSLO" varchar(1) NOT NULL default ' ',
  "AAAAZSMB" varchar(1) NOT NULL default ' ',
  "AAAAZSMN" varchar(1) NOT NULL default ' ',
  "AAAAZSLR" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPLT","AAABLXWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSLW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABLXWL" varchar(25) NOT NULL default ' ',
  "AAAAZSML" varchar(1) NOT NULL default ' ',
  "AAAAZSMB" varchar(1) NOT NULL default ' ',
  "AAAAZSMN" varchar(1) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPLT","AAABLXWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSMH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABLXWL" varchar(25) NOT NULL default ' ',
  "AAAAZSML" varchar(1) NOT NULL default ' ',
  "AAAAZSMM" varchar(2) NOT NULL default ' ',
  "AAAAZSMN" varchar(1) NOT NULL default ' ',
  "AAAAZSMO" varchar(4) NOT NULL default ' ',
  "AAAAZSMP" varchar(4) NOT NULL default ' ',
  "AAAAZSMQ" varchar(4) NOT NULL default ' ',
  "AAAAZSMR" varchar(4) NOT NULL default ' ',
  "AAAAZSMS" varchar(4) NOT NULL default ' ',
  "AAAAZSMT" varchar(4) NOT NULL default ' ',
  "AAAAZSMU" varchar(1) NOT NULL default ' ',
  "AAAAZSMV" varchar(4) NOT NULL default ' ',
  "AAAAZSMW" varchar(4) NOT NULL default ' ',
  "AAAAZSMX" varchar(4) NOT NULL default ' ',
  "AAAAZSMY" varchar(4) NOT NULL default ' ',
  "AAAAZSMZ" varchar(2) NOT NULL default ' ',
  "AAAAZSNA" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPLT","AAABLXWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSNE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZSNG" varchar(6) NOT NULL default '000000',
  "AAAAZSNH" varchar(6) NOT NULL default '000000',
  "AAAAZSNI" varchar(6) NOT NULL default '000000',
  "AAAAZSNJ" varchar(1) NOT NULL default ' ',
  "AAAAZSNK" varchar(3) NOT NULL default ' ',
  "AAAAZSNL" varchar(3) NOT NULL default ' ',
  "AAAAZSNM" varchar(3) NOT NULL default ' ',
  "AAAAZSNN" varchar(3) NOT NULL default ' ',
  "AAAAZSNO" varchar(4) NOT NULL default ' ',
  "AAAAZSNP" varchar(4) NOT NULL default ' ',
  "AAAAZSNQ" varchar(4) NOT NULL default ' ',
  "AAAAZSNR" varchar(4) NOT NULL default ' ',
  "AAAAZSNS" varchar(4) NOT NULL default ' ',
  "AAAAZSNT" varchar(4) NOT NULL default ' ',
  "AAAAZSNU" varchar(4) NOT NULL default ' ',
  "AAAAZSNV" varchar(4) NOT NULL default ' ',
  "AAAAZSNW" varchar(4) NOT NULL default ' ',
  "AAAAZSNX" varchar(4) NOT NULL default ' ',
  "AAAAZSNY" varchar(4) NOT NULL default ' ',
  "AAAAZSNZ" varchar(4) NOT NULL default ' ',
  "AAAAZSOA" varchar(4) NOT NULL default ' ',
  "AAAAZSOB" varchar(4) NOT NULL default ' ',
  "AAAAZSOC" varchar(4) NOT NULL default ' ',
  "AAAAZSOD" varchar(4) NOT NULL default ' ',
  "AAAAZSOE" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSOG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDEKF" varchar(10) NOT NULL default ' ',
  "AAABDEKG" varchar(2) NOT NULL default ' ',
  "AAABDEKH" varchar(2) NOT NULL default ' ',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABIJDD" varchar(3) NOT NULL default ' ',
  "AAABLQTX" varchar(3) NOT NULL default ' ',
  "AAABLQSF" varchar(1) NOT NULL default ' ',
  "AAABKPJP" varchar(1) NOT NULL default ' ',
  "AAABKPJQ" varchar(1) NOT NULL default ' ',
  "AAABKPJR" varchar(1) NOT NULL default ' ',
  "AAABKPJS" varchar(1) NOT NULL default ' ',
  "AAAAZSOT" varchar(6) NOT NULL default '000000',
  "AAAAZSOU" varchar(8) NOT NULL default '00000000',
  "AAAAZSOV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDEKF","AAABDEKG","AAABDEKH","AAABLMHB","AAABIJDD","AAABLQTX","AAABLQSF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSPE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWHG" varchar(10) NOT NULL default ' ',
  "AAABKPAN" varchar(1) NOT NULL default ' ',
  "AAABKPAQ" varchar(1) NOT NULL default ' ',
  "AAABKPAR" varchar(1) NOT NULL default ' ',
  "AAABKPAS" varchar(4) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABKPAU" varchar(1) NOT NULL default ' ',
  "AAABKPAW" varchar(1) NOT NULL default ' ',
  "AAABKPAV" varchar(1) NOT NULL default ' ',
  "AAABKPAX" varchar(1) NOT NULL default ' ',
  "AAABKPAY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLWHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSPT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKPAS" varchar(4) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAAAZSPX" decimal(4,2) NOT NULL default '0.00',
  "AAAAZSPY" decimal(4,2) NOT NULL default '0.00',
  "AAAAZSPZ" varchar(1) NOT NULL default ' ',
  "AAAAZSQA" varchar(1) NOT NULL default ' ',
  "AAAAZSQB" decimal(4,2) NOT NULL default '0.00',
  "AAAAZSQC" decimal(4,2) NOT NULL default '0.00',
  "AAAAZSQD" varchar(1) NOT NULL default ' ',
  "AAAAZSQE" varchar(1) NOT NULL default ' ',
  "AAAAZSQF" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABKPAS","AAABMQII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSQJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKPAS" varchar(4) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAZSQO" decimal(13,2) NOT NULL default '0.00',
  "AAAAZSQP" decimal(13,2) NOT NULL default '0.00',
  "AAAAZSQQ" varchar(1) NOT NULL default ' ',
  "AAAAZSQR" varchar(1) NOT NULL default ' ',
  "AAAAZSQS" decimal(13,2) NOT NULL default '0.00',
  "AAAAZSQT" decimal(13,2) NOT NULL default '0.00',
  "AAAAZSQU" varchar(1) NOT NULL default ' ',
  "AAAAZSQV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKPAS","AAABMQII","AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSRA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKPAS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKPAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSRF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKPAS" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZSRJ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKPAS","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSRN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAAAZSSE" varchar(4) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMAMX","AAAAZSSE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSRV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAAAZSSE" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMX","AAAAZSSE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSSF" (
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABDUMD" varchar(3) NOT NULL default '000',
  "AAABHVRQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMAMX","AAABMAMY","AAABDUMD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSSN" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABDUMD" varchar(3) NOT NULL default '000',
  "AAABMOHX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMAMX","AAABMAMY","AAABDUMD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSSX" (
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAAAZSUJ" varchar(3) NOT NULL default '000',
  "AAAAZSVK" varchar(30) NOT NULL default ' ',
  "AAABDUMD" varchar(3) NOT NULL default '000',
  "AAAAZSTD" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMAMX","AAABMAMY","AAAAZSUJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSTH" (
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAAAZSUJ" varchar(3) NOT NULL default '000',
  "AAAAZSTU" varchar(2) NOT NULL default '00',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABFGSE" varchar(30) NOT NULL default ' ',
  "AAABHVRQ" varchar(4) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMAMX","AAABMAMY","AAAAZSUJ","AAAAZSTU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSTV" (
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAAAZSVC" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMAMX","AAABMAMY","AAAAZSVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSUC" (
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAAAZSVC" varchar(12) NOT NULL default ' ',
  "AAABHSHJ" varchar(2) NOT NULL default '00',
  "AAAAZSUH" varchar(3) NOT NULL default '000',
  "AAAAZSUI" varchar(3) NOT NULL default '000',
  "AAAAZSUJ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMAMX","AAABMAMY","AAAAZSVC","AAABHSHJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSUO" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAAAZSVC" varchar(12) NOT NULL default ' ',
  "AAABMOHX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMAMX","AAABMAMY","AAAAZSVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSUY" (
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAZSVC" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMAMX","AAABMAMY","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSVG" (
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABLVBH" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAAAZSVK" varchar(30) NOT NULL default ' ',
  "AAAAZSZD" varchar(4) NOT NULL default ' ',
  "AAAAZSVM" varchar(1) NOT NULL default ' ',
  "AAAAZSVN" varchar(1) NOT NULL default ' ',
  "AAAAZSVO" varchar(1) NOT NULL default ' ',
  "AAAAZSVP" varchar(3) NOT NULL default '000',
  "AAABEEXB" varchar(8) NOT NULL default ' ',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  "AAAAZSVT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMAMX","AAABLVBH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSVW" (
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMAMY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSVZ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMAMY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSWF" (
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABEPTB" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMAMX","AAABMAMY","AAABEPTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSWM" (
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAAZSWR" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAABMAMX","AAABMAMY","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSWW" (
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABLVBH" varchar(3) NOT NULL default '000',
  "AAAAZSZD" varchar(4) NOT NULL default ' ',
  "AAAAZSXH" varchar(1) NOT NULL default ' ',
  "AAAAZSXI" varchar(2) NOT NULL default '00',
  "AAAAZSXC" varchar(30) NOT NULL default ' ',
  "AAAAZSXD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMAMX","AAABLVBH","AAAAZSZD","AAAAZSXH","AAAAZSXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSXJ" (
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAAAZSXQ" varchar(3) NOT NULL default ' ',
  "AAAAZSXR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMAMX","AAABMAMY","AAAAZSXQ","AAAAZSXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSXS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABHKSD" varchar(4) NOT NULL default '0000',
  "AAAAZSXX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMX","AAABMAMY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSYB" (
  "AAABLCGT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLCGT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSYE" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLCGT" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLCGT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSYK" (
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABLVBH" varchar(3) NOT NULL default '000',
  "AAAAZSZD" varchar(4) NOT NULL default ' ',
  "AAABECXR" varchar(4) NOT NULL default ' ',
  "AAAAZSYP" varchar(1) NOT NULL default ' ',
  "AAAAZSYQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMAMX","AAABLVBH","AAAAZSZD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSYU" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABLVBH" varchar(3) NOT NULL default '000',
  "AAAAZSZD" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMAMX","AAABLVBH","AAAAZSZD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSZE" (
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAAAZSZG" varchar(30) NOT NULL default ' ',
  "AAABFXJE" varchar(30) NOT NULL default ' ',
  "AAAAZSZI" varchar(40) NOT NULL default ' ',
  "AAAAZSZJ" varchar(40) NOT NULL default ' ',
  "AAAAZSZK" varchar(1) NOT NULL default '0',
  "AAAAZSZL" varchar(1) NOT NULL default '0',
  "AAAAZSZM" varchar(40) NOT NULL default ' ',
  "AAAAZSZN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMAMX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSZP" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMAMX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZSZV" (
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAAAZTSY" varchar(2) NOT NULL default ' ',
  "AAAAZSZY" varchar(1) NOT NULL default ' ',
  "AAAAZSZZ" varchar(1) NOT NULL default ' ',
  "AAAAZTAA" varchar(2) NOT NULL default ' ',
  "AAAAZTAB" varchar(8) NOT NULL default '00000000',
  "AAAAZTAC" varchar(6) NOT NULL default '000000',
  "AAABEEXB" varchar(8) NOT NULL default ' ',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  "AAAAZTAG" varchar(1) NOT NULL default ' ',
  "AAAAZTAH" varchar(1) NOT NULL default ' ',
  "AAAAZTAI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMAMX","AAAAZTSY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTAL" (
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMIZS" varchar(30) NOT NULL default ' ',
  "AAAAZTAQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMAMX","AAABMAMY","AAABMREL","AAABMIZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTAV" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAAAZTSY" varchar(2) NOT NULL default ' ',
  "AAAAZTAZ" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMAMX","AAAAZTSY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTBD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAZTBL" varchar(3) NOT NULL default ' ',
  "AAAAZTBM" varchar(30) NOT NULL default ' ',
  "AAABMLZS" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAAAZTBL","AAAAZTBM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTBN" (
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAAAZTJB" varchar(4) NOT NULL default ' ',
  "AAAAZTBR" varchar(1) NOT NULL default ' ',
  "AAABLWBE" varchar(1) NOT NULL default ' ',
  "AAAAZTCK" varchar(1) NOT NULL default ' ',
  "AAAAZTBU" varchar(3) NOT NULL default ' ',
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZTBW" varchar(8) NOT NULL default ' ',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMAMX","AAABMAMY","AAAAZTJB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTCC" (
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAAAZTJB" varchar(4) NOT NULL default ' ',
  "AAABHSHJ" varchar(2) NOT NULL default '00',
  "AAABLVBH" varchar(3) NOT NULL default '000',
  "AAAAZTCI" decimal(3,0) NOT NULL default '0',
  "AAAAZTCJ" varchar(1) NOT NULL default ' ',
  "AAAAZTCK" varchar(1) NOT NULL default ' ',
  "AAABLXXL" varchar(3) NOT NULL default '000',
  "AAAAZTKF" varchar(1) NOT NULL default ' ',
  "AAAAZTCN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMAMX","AAABMAMY","AAAAZTJB","AAABHSHJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTCS" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAAAZTJB" varchar(4) NOT NULL default ' ',
  "AAAAZTCX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMAMX","AAABMAMY","AAAAZTJB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTDC" (
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAAAZTJB" varchar(4) NOT NULL default ' ',
  "AAABHSHJ" varchar(2) NOT NULL default '00',
  "AAABLTXF" varchar(2) NOT NULL default '00',
  "AAAAZTDI" varchar(30) NOT NULL default ' ',
  "AAABLXYE" varchar(30) NOT NULL default ' ',
  "AAABFXYZ" varchar(30) NOT NULL default ' ',
  "AAAAZTDL" varchar(20) NOT NULL default ' ',
  "AAAAZTDM" varchar(1) NOT NULL default ' ',
  "AAAAZTDN" varchar(1) NOT NULL default ' ',
  "AAAAZTDO" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMAMX","AAABMAMY","AAAAZTJB","AAABHSHJ","AAABLTXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTDU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAAAZTDZ" varchar(1) NOT NULL default ' ',
  "AAAAZTEA" varchar(1) NOT NULL default ' ',
  "AAAAZTEB" varchar(1) NOT NULL default ' ',
  "AAAAZTEC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMX","AAABMAMY","AAABMEKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTEH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABBMSB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABMLHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTEU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABLTXE" varchar(3) NOT NULL default '000',
  "AAABLTXF" varchar(2) NOT NULL default '00',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAAAZTFC" varchar(3) NOT NULL default '000',
  "AAAAZTFD" varchar(3) NOT NULL default '000',
  "AAAAZTFE" varchar(1) NOT NULL default ' ',
  "AAABLXXL" varchar(3) NOT NULL default '000',
  "AAAAZTFG" varchar(1) NOT NULL default ' ',
  "AAABJJOH" varchar(1) NOT NULL default ' ',
  "AAABCFJM" varchar(3) NOT NULL default '000',
  "AAAAZTFJ" varchar(3) NOT NULL default '000',
  "AAAAZTFK" varchar(3) NOT NULL default ' ',
  "AAAAZTFL" varchar(3) NOT NULL default ' ',
  "AAAAZTFM" varchar(1) NOT NULL default ' ',
  "AAAAZTFN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMX","AAABMAMY","AAABMEKQ","AAABLTXE","AAABLTXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTFU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABLTXE" varchar(3) NOT NULL default '000',
  "AAABLTXF" varchar(2) NOT NULL default '00',
  "AAABMOHX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMX","AAABMAMY","AAABMEKQ","AAABLTXE","AAABLTXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABMOHX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMX","AAABMAMY","AAABMEKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTGW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABLUKA" varchar(1) NOT NULL default ' ',
  "AAABHOAX" varchar(1) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAAAZTHE" varchar(4) NOT NULL default ' ',
  "AAABGAHV" varchar(6) NOT NULL default ' ',
  "AAAAZTHG" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABLUKA","AAABHOAX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTHN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABLUKA" varchar(1) NOT NULL default ' ',
  "AAABHOAX" varchar(1) NOT NULL default ' ',
  "AAABJHRA" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABLUKA","AAABHOAX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTIB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAAAZTIV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMX","AAABMAMY","AAAAZTIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTIK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAAAZTIV" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMX","AAABMAMY","AAAAZTIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAAAZTJB" varchar(4) NOT NULL default ' ',
  "AAABHOCQ" varchar(1) NOT NULL default ' ',
  "AAAAZUBF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMX","AAABMAMY","AAABMQIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTJI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLXZF" varchar(1) NOT NULL default ' ',
  "AAABLXZJ" varchar(1) NOT NULL default ' ',
  "AAABCHHL" varchar(1) NOT NULL default ' ',
  "AAAAZTJP" varchar(1) NOT NULL default ' ',
  "AAAAZTJQ" varchar(1) NOT NULL default ' ',
  "AAAAZTJR" varchar(4) NOT NULL default ' ',
  "AAAAZTJS" varchar(2) NOT NULL default '00',
  "AAABJTWP" varchar(3) NOT NULL default ' ',
  "AAAAZTJU" varchar(1) NOT NULL default ' ',
  "AAAAZTJV" varchar(1) NOT NULL default ' ',
  "AAAAZTJW" varchar(1) NOT NULL default ' ',
  "AAAAZTJX" varchar(1) NOT NULL default ' ',
  "AAAAZTJY" varchar(1) NOT NULL default ' ',
  "AAABLXTQ" varchar(1) NOT NULL default ' ',
  "AAAAZTKA" varchar(1) NOT NULL default ' ',
  "AAAAZTKB" varchar(3) NOT NULL default '000',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABLWYG" varchar(1) NOT NULL default ' ',
  "AAAAZTKE" varchar(1) NOT NULL default ' ',
  "AAAAZTKF" varchar(1) NOT NULL default ' ',
  "AAAAZTKG" varchar(1) NOT NULL default ' ',
  "AAAAZTKH" varchar(1) NOT NULL default ' ',
  "AAAAZTKI" varchar(1) NOT NULL default ' ',
  "AAAAZTKJ" varchar(1) NOT NULL default ' ',
  "AAAAZTKK" varchar(1) NOT NULL default ' ',
  "AAAAZTKL" varchar(1) NOT NULL default ' ',
  "AAAAZTKM" varchar(1) NOT NULL default ' ',
  "AAAAZTKN" varchar(1) NOT NULL default ' ',
  "AAAAZTKO" varchar(1) NOT NULL default ' ',
  "AAAAZTKP" varchar(1) NOT NULL default ' ',
  "AAAAZTOM" varchar(1) NOT NULL default ' ',
  "AAAAZTKR" varchar(1) NOT NULL default ' ',
  "AAAAZTKS" varchar(1) NOT NULL default ' ',
  "AAAAZTKT" varchar(1) NOT NULL default ' ',
  "AAABLXZI" varchar(1) NOT NULL default ' ',
  "AAAAZTKV" varchar(1) NOT NULL default ' ',
  "AAAAZTKW" varchar(1) NOT NULL default ' ',
  "AAAAZTKX" varchar(1) NOT NULL default ' ',
  "AAABLWZC" varchar(1) NOT NULL default ' ',
  "AAAAZTKZ" varchar(1) NOT NULL default ' ',
  "AAAAZTLA" varchar(2) NOT NULL default ' ',
  "AAAAZTLB" varchar(4) NOT NULL default ' ',
  "AAABJUPV" varchar(1) NOT NULL default ' ',
  "AAABJUPW" varchar(4) NOT NULL default ' ',
  "AAAAZTLE" varchar(1) NOT NULL default ' ',
  "AAAAZTLF" varchar(1) NOT NULL default ' ',
  "AAAAZTLG" varchar(1) NOT NULL default ' ',
  "AAABLUZV" varchar(1) NOT NULL default ' ',
  "AAAAZTLI" varchar(1) NOT NULL default ' ',
  "AAAAZTLJ" varchar(1) NOT NULL default ' ',
  "AAAAZTLK" varchar(1) NOT NULL default ' ',
  "AAAAZUCN" varchar(1) NOT NULL default ' ',
  "AAABLYCG" varchar(2) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAAAZTLO" varchar(1) NOT NULL default ' ',
  "AAAAZTLP" VARBINARY(255),
  "AAAAZTLQ" varchar(2) NOT NULL default ' ',
  "AAAAZTLR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTLV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAAAZTLZ" varchar(1) NOT NULL default ' ',
  "AAABKZKJ" varchar(1) NOT NULL default ' ',
  "AAAAZTMB" varchar(70) NOT NULL default ' ',
  "AAABHPXO" varchar(1) NOT NULL default ' ',
  "AAABMEKY" varchar(1) NOT NULL default ' ',
  "AAABDKJZ" varchar(1) NOT NULL default ' ',
  "AAABGCHK" varchar(40) NOT NULL default ' ',
  "AAAAZTMG" varchar(30) NOT NULL default ' ',
  "AAAAZTMH" varchar(40) NOT NULL default ' ',
  "AAAAZTMI" varchar(30) NOT NULL default ' ',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAAAZTMK" varchar(1) NOT NULL default ' ',
  "AAAAZTML" varchar(1) NOT NULL default ' ',
  "AAAAZTMM" varchar(1) NOT NULL default ' ',
  "AAAAZTMN" varchar(1) NOT NULL default ' ',
  "AAAAZTMO" varchar(1) NOT NULL default ' ',
  "AAAAZTMP" varchar(1) NOT NULL default ' ',
  "AAAAZTMQ" varchar(1) NOT NULL default ' ',
  "AAABLKWD" varchar(1) NOT NULL default ' ',
  "AAAAZTMS" varchar(10) NOT NULL default ' ',
  "AAAAZTMT" varchar(40) NOT NULL default ' ',
  "AAAAZTMU" varchar(30) NOT NULL default ' ',
  "AAAAZTMV" varchar(12) NOT NULL default ' ',
  "AAAAZTMW" varchar(1) NOT NULL default ' ',
  "AAABDDFD" varchar(1) NOT NULL default ' ',
  "AAAAZTMY" varchar(40) NOT NULL default ' ',
  "AAAAZTMZ" varchar(30) NOT NULL default ' ',
  "AAAAZTNA" varchar(1) NOT NULL default ' ',
  "AAAAZTNB" varchar(40) NOT NULL default ' ',
  "AAAAZTNC" varchar(30) NOT NULL default ' ',
  "AAAAZTND" varchar(1) NOT NULL default ' ',
  "AAABKRIX" varchar(61) NOT NULL default ' ',
  "AAABKRIY" varchar(61) NOT NULL default ' ',
  "AAABKRIZ" varchar(61) NOT NULL default ' ',
  "AAABMMRM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTNL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLXPP" varchar(1) NOT NULL default ' ',
  "AAAAZTNQ" varchar(4) NOT NULL default '0000',
  "AAAAZTNR" varchar(3) NOT NULL default '000',
  "AAAAZTNS" varchar(1) NOT NULL default ' ',
  "AAABLXUI" varchar(3) NOT NULL default '000',
  "AAAAZTNU" varchar(1) NOT NULL default ' ',
  "AAAAZTNV" varchar(1) NOT NULL default ' ',
  "AAAAZTNW" varchar(4) NOT NULL default ' ',
  "AAAAZTNX" varchar(20) NOT NULL default ' ',
  "AAAAZTNY" varchar(10) NOT NULL default '0000000000',
  "AAAAZTNZ" varchar(10) NOT NULL default '0000000000',
  "AAAAZTOA" varchar(1) NOT NULL default ' ',
  "AAAAZTOB" varchar(1) NOT NULL default ' ',
  "AAAAZTOC" varchar(18) NOT NULL default ' ',
  "AAAAZTOD" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTOH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAAAZTOL" varchar(1) NOT NULL default ' ',
  "AAAAZTOM" varchar(1) NOT NULL default ' ',
  "AAAAZTON" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTOR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAAAZTOV" varchar(1) NOT NULL default ' ',
  "AAABLXZJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTPA" (
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABKZKJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMAMY","AAABKZKJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTPF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTPM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAAAZTQA" varchar(1) NOT NULL default '0',
  "AAABJUMT" varchar(10) NOT NULL default ' ',
  "AAABJUHN" varchar(2) NOT NULL default ' ',
  "AAABJTWM" varchar(3) NOT NULL default '000',
  "AAABJTWN" varchar(3) NOT NULL default '000',
  "AAABJTWO" varchar(1) NOT NULL default ' ',
  "AAABJTWP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAAAZTQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTQB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMX","AAABMAMY","AAABMQIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTQN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLXZF" varchar(1) NOT NULL default ' ',
  "AAABLXYZ" varchar(5) NOT NULL default ' ',
  "AAABLWXZ" decimal(11,2) NOT NULL default '0.00',
  "AAABLWYA" decimal(11,2) NOT NULL default '0.00',
  "AAABLXZA" decimal(5,0) NOT NULL default '0',
  "AAABLXZB" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABLXZF","AAABLXYZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTRC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABLWYG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABLWYG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTRJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABLWYG" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMY","AAABLWYG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTRT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZTSF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZTSF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTRY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZTSF" varchar(3) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZTSF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTSG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZTST" varchar(2) NOT NULL default ' ',
  "AAAAZUCN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZTST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTSM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZTST" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZTST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTSU" (
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAAAZTTW" varchar(4) NOT NULL default ' ',
  "AAAAZTSY" varchar(2) NOT NULL default ' ',
  "AAAAZTSZ" varchar(1) NOT NULL default ' ',
  "AAAAZTTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMAMX","AAABMAMY","AAAAZTTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTTE" (
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAAAZTTW" varchar(4) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMAMX","AAABMAMY","AAAAZTTW","AAABMEKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTTN" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAAAZTTW" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMAMX","AAABMAMY","AAAAZTTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTTX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZTUJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZTUJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTUC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZTUJ" varchar(2) NOT NULL default ' ',
  "AAAAZTUG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZTUJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTUK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCZV" varchar(4) NOT NULL default ' ',
  "AAABLXJU" varchar(3) NOT NULL default ' ',
  "AAAAZTXW" varchar(2) NOT NULL default ' ',
  "AAAAZTUP" varchar(1) NOT NULL default ' ',
  "AAAAZTUQ" varchar(3) NOT NULL default '000',
  "AAAAZTUR" varchar(1) NOT NULL default ' ',
  "AAAAZTUS" decimal(15,2) NOT NULL default '0.00',
  "AAAAZTUT" varchar(3) NOT NULL default '000',
  "AAAAZTUU" varchar(3) NOT NULL default '000',
  "AAAAZTUV" varchar(1) NOT NULL default ' ',
  "AAAAZTUW" varchar(1) NOT NULL default ' ',
  "AAAAZTUX" varchar(1) NOT NULL default ' ',
  "AAABAEQS" varchar(1) NOT NULL default ' ',
  "AAAAZTUZ" varchar(1) NOT NULL default ' ',
  "AAAAZTVA" varchar(1) NOT NULL default ' ',
  "AAAAZTVB" varchar(1) NOT NULL default ' ',
  "AAAAZTVC" varchar(3) NOT NULL default '000',
  "AAAAZTVD" varchar(1) NOT NULL default ' ',
  "AAAAZTVE" varchar(1) NOT NULL default ' ',
  "AAAAZTVF" varchar(1) NOT NULL default ' ',
  "AAAAZTVG" decimal(15,2) NOT NULL default '0.00',
  "AAABIHRW" varchar(1) NOT NULL default ' ',
  "AAAAZTVI" varchar(1) NOT NULL default ' ',
  "AAAAZTVJ" varchar(1) NOT NULL default ' ',
  "AAAAZTVK" varchar(1) NOT NULL default ' ',
  "AAAAZTVL" varchar(1) NOT NULL default ' ',
  "AAAAZTVM" varchar(1) NOT NULL default ' ',
  "AAAAZTVN" varchar(3) NOT NULL default '000',
  "AAAAZTVO" varchar(1) NOT NULL default ' ',
  "AAAAZTVP" varchar(1) NOT NULL default ' ',
  "AAAAZTVQ" varchar(1) NOT NULL default ' ',
  "AAAAZTVR" varchar(3) NOT NULL default '000',
  "AAAAZTVS" varchar(3) NOT NULL default '000',
  "AAAAZTVT" varchar(1) NOT NULL default ' ',
  "AAAAZTVU" varchar(1) NOT NULL default ' ',
  "AAAAZTVV" varchar(1) NOT NULL default ' ',
  "AAAAZTVW" varchar(3) NOT NULL default '000',
  "AAAAZTVX" varchar(1) NOT NULL default ' ',
  "AAAAZTVY" varchar(1) NOT NULL default ' ',
  "AAAAZTVZ" varchar(1) NOT NULL default ' ',
  "AAAAZTWA" varchar(1) NOT NULL default '0',
  "AAAAZTWB" varchar(1) NOT NULL default ' ',
  "AAAAZTWC" varchar(1) NOT NULL default ' ',
  "AAAAZTWD" varchar(1) NOT NULL default ' ',
  "AAAAZTWE" decimal(15,2) NOT NULL default '0.00',
  "AAAAZTWF" varchar(1) NOT NULL default ' ',
  "AAAAZTWG" varchar(1) NOT NULL default ' ',
  "AAAAZTWH" varchar(1) NOT NULL default ' ',
  "AAAAZTWI" varchar(1) NOT NULL default ' ',
  "AAAAZTWJ" varchar(1) NOT NULL default ' ',
  "AAAAZTWK" varchar(1) NOT NULL default ' ',
  "AAAAZTWL" varchar(1) NOT NULL default ' ',
  "AAAAZTWM" varchar(1) NOT NULL default ' ',
  "AAAAZTWN" varchar(1) NOT NULL default ' ',
  "AAAAZTWO" varchar(1) NOT NULL default ' ',
  "AAAAZTWP" varchar(1) NOT NULL default ' ',
  "AAAAZTWQ" varchar(1) NOT NULL default ' ',
  "AAAAZTWR" varchar(1) NOT NULL default ' ',
  "AAAAZTWS" varchar(1) NOT NULL default ' ',
  "AAAAZTWT" varchar(1) NOT NULL default ' ',
  "AAAAZTWU" varchar(1) NOT NULL default ' ',
  "AAAAZTWV" varchar(1) NOT NULL default ' ',
  "AAAAZTWW" varchar(1) NOT NULL default ' ',
  "AAAAZTWX" varchar(1) NOT NULL default ' ',
  "AAAAZTWY" varchar(1) NOT NULL default ' ',
  "AAAAZTWZ" varchar(1) NOT NULL default ' ',
  "AAABJWQZ" varchar(1) NOT NULL default ' ',
  "AAABHKSE" varchar(3) NOT NULL default '000',
  "AAAAZTXC" varchar(8) NOT NULL default '00000000',
  "AAAAZTXD" varchar(8) NOT NULL default '00000000',
  "AAAAZTXE" varchar(2) NOT NULL default '00',
  "AAAAZTXF" varchar(2) NOT NULL default '00',
  "AAAAZTXG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCZV","AAABLXJU","AAAAZTXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTXL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCZV" varchar(4) NOT NULL default ' ',
  "AAABLXJU" varchar(3) NOT NULL default ' ',
  "AAAAZTXW" varchar(2) NOT NULL default ' ',
  "AAABMLZY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCZV","AAABLXJU","AAAAZTXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTXX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCZV" varchar(4) NOT NULL default ' ',
  "AAAAZTYE" varchar(15) NOT NULL default '000000000000000',
  "AAAAZTYB" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCZV","AAAAZTYE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZTZN" varchar(2) NOT NULL default ' ',
  "AAAAZTYI" varchar(1) NOT NULL default ' ',
  "AAAAZTYJ" varchar(1) NOT NULL default ' ',
  "AAABLGJS" varchar(2) NOT NULL default ' ',
  "AAAAZTYL" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAAAZTZN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTYO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZTZN" varchar(2) NOT NULL default ' ',
  "AAABMAHQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZTZN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTYW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIGP" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLIGP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTZB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLIGP" varchar(6) NOT NULL default ' ',
  "AAABMAHQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLIGP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTZJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIGP" varchar(6) NOT NULL default ' ',
  "AAAAZTZR" varchar(3) NOT NULL default '000',
  "AAAAZTZN" varchar(2) NOT NULL default ' ',
  "AAABHKSE" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABLIGP","AAAAZTZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTZS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZUAY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZUAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZTZX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZUAY" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZUAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHKMM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHKMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUAK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHKMM" varchar(2) NOT NULL default ' ',
  "AAABMAHQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHKMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUAS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZUAY" varchar(4) NOT NULL default ' ',
  "AAABHKMM" varchar(2) NOT NULL default ' ',
  "AAABLIGP" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZUAY","AAABHKMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUBA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXYW" varchar(4) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAAAZUBE" varchar(1) NOT NULL default ' ',
  "AAAAZUBF" varchar(1) NOT NULL default ' ',
  "AAABHOCQ" varchar(1) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAAAZUBI" varchar(1) NOT NULL default ' ',
  "AAABLGLC" varchar(4) NOT NULL default ' ',
  "AAAAZUBK" varchar(1) NOT NULL default ' ',
  "AAABHYJM" varchar(2) NOT NULL default ' ',
  "AAABHYJN" varchar(2) NOT NULL default ' ',
  "AAAAZUBN" varchar(4) NOT NULL default ' ',
  "AAAAZUBO" varchar(4) NOT NULL default ' ',
  "AAAAZUBP" varchar(4) NOT NULL default ' ',
  "AAAAZUBQ" varchar(1) NOT NULL default ' ',
  "AAABLWZZ" varchar(1) NOT NULL default ' ',
  "AAAAZUBS" varchar(1) NOT NULL default ' ',
  "AAAAZUBT" varchar(1) NOT NULL default ' ',
  "AAAAZUBU" varchar(1) NOT NULL default ' ',
  "AAAAZUBV" varchar(1) NOT NULL default ' ',
  "AAAAZUBW" varchar(1) NOT NULL default ' ',
  "AAAAZUBX" varchar(4) NOT NULL default ' ',
  "AAAAZUBY" varchar(1) NOT NULL default ' ',
  "AAABGCQJ" varchar(2) NOT NULL default ' ',
  "AAAAZUCA" varchar(2) NOT NULL default ' ',
  "AAAAZUCB" varchar(1) NOT NULL default ' ',
  "AAAAZUCC" varchar(1) NOT NULL default ' ',
  "AAAAZUCD" varchar(1) NOT NULL default ' ',
  "AAAAZUCE" varchar(4) NOT NULL default ' ',
  "AAAAZUCF" varchar(1) NOT NULL default ' ',
  "AAABLKXQ" varchar(4) NOT NULL default ' ',
  "AAAAZUCH" varchar(1) NOT NULL default ' ',
  "AAABHUCF" varchar(1) NOT NULL default ' ',
  "AAAAZUCJ" varchar(4) NOT NULL default ' ',
  "AAAAZUCK" varchar(4) NOT NULL default ' ',
  "AAAAZUCL" varchar(2) NOT NULL default ' ',
  "AAAAZUCM" varchar(1) NOT NULL default ' ',
  "AAAAZUCN" varchar(1) NOT NULL default ' ',
  "AAABLYCG" varchar(2) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAABGAHT" varchar(6) NOT NULL default ' ',
  "AAAAZUFI" varchar(4) NOT NULL default ' ',
  "AAABFZYX" varchar(2) NOT NULL default ' ',
  "AAABFZYY" varchar(6) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAAAZUCV" varchar(2) NOT NULL default ' ',
  "AAAAZUCW" varchar(6) NOT NULL default ' ',
  "AAAAZUCX" varchar(4) NOT NULL default ' ',
  "AAABFXEI" varchar(2) NOT NULL default ' ',
  "AAABFXEJ" varchar(4) NOT NULL default ' ',
  "AAAAZUDA" varchar(2) NOT NULL default ' ',
  "AAAAZUDB" varchar(2) NOT NULL default ' ',
  "AAAAZUDC" varchar(2) NOT NULL default ' ',
  "AAAAZUDD" varchar(1) NOT NULL default ' ',
  "AAAAZUDE" varchar(6) NOT NULL default ' ',
  "AAAAZUDF" varchar(1) NOT NULL default ' ',
  "AAAAZUDG" varchar(1) NOT NULL default ' ',
  "AAAAZUDH" varchar(1) NOT NULL default ' ',
  "AAAAZUDI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXYW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUDL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXYW" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXYW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUDT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABLGLC" varchar(4) NOT NULL default ' ',
  "AAAAZUDX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABLGLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUEB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABLGLC" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMY","AAABLGLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUEL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAAAZUFI" varchar(4) NOT NULL default ' ',
  "AAABIBEE" varchar(1) NOT NULL default ' ',
  "AAABLXZO" varchar(1) NOT NULL default ' ',
  "AAAAZUER" varchar(1) NOT NULL default ' ',
  "AAAAZUES" varchar(1) NOT NULL default ' ',
  "AAAAZUGS" varchar(4) NOT NULL default ' ',
  "AAABAPYY" varchar(1) NOT NULL default ' ',
  "AAAAZUEV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAAAZUFI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUEZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAAAZUFI" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMY","AAAAZUFI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUFJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAAAZUGS" varchar(4) NOT NULL default ' ',
  "AAAAZUFN" decimal(7,2) NOT NULL default '0.00',
  "AAAAZUFO" decimal(7,2) NOT NULL default '0.00',
  "AAAAZUFP" decimal(7,2) NOT NULL default '0.00',
  "AAAAZUFQ" decimal(7,2) NOT NULL default '0.00',
  "AAAAZUFR" decimal(7,2) NOT NULL default '0.00',
  "AAAAZUFS" decimal(7,2) NOT NULL default '0.00',
  "AAAAZUFT" decimal(7,2) NOT NULL default '0.00',
  "AAAAZUFU" decimal(7,2) NOT NULL default '0.00',
  "AAAAZUFV" decimal(7,2) NOT NULL default '0.00',
  "AAAAZUFW" decimal(7,2) NOT NULL default '0.00',
  "AAAAZUFX" decimal(7,2) NOT NULL default '0.00',
  "AAAAZUFY" decimal(7,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAAAZUGS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUGC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAAAZUGS" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMY","AAAAZUGS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUGM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABGCCV" varchar(2) NOT NULL default ' ',
  "AAABLXYW" varchar(4) NOT NULL default ' ',
  "AAAAZUGS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMAMX","AAABGCCV","AAABLXYW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUGY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLODQ" varchar(2) NOT NULL default ' ',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAAAZUHL" varchar(4) NOT NULL default '0000',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLVBH" varchar(3) NOT NULL default '000',
  "AAAAZUHG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLODQ","AAABMAMX","AAABMAMY","AAAAZUHL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUHM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLODQ" varchar(2) NOT NULL default ' ',
  "AAAAZUHP" varchar(1) NOT NULL default ' ',
  "AAAAZUHQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLODQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUHT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLODQ" varchar(2) NOT NULL default ' ',
  "AAAAZUIA" varchar(30) NOT NULL default ' ',
  "AAAAZUIG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLODQ","AAAAZUIA","AAAAZUIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUIC" (
  "AAABLODQ" varchar(2) NOT NULL default ' ',
  "AAAAZUII" varchar(1) NOT NULL default '0',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAAAZUIG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLODQ","AAAAZUII","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUIK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZUIV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZUIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUIP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZUIV" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZUIV","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUIX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZULF" varchar(2) NOT NULL default ' ',
  "AAAAZUJJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZULF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUJF" (
  "AAABMMKX" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZULA" varchar(2) NOT NULL default ' ',
  "AAAAZUJJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMKX","AAABMRHS","AAAAZULA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZUKE" varchar(1) NOT NULL default ' ',
  "AAAAZUJQ" varchar(1) NOT NULL default ' ',
  "AAAAZUJR" varchar(1) NOT NULL default ' ',
  "AAAAZUJS" varchar(1) NOT NULL default ' ',
  "AAAAZUJT" varchar(1) NOT NULL default ' ',
  "AAAAZUJU" varchar(1) NOT NULL default ' ',
  "AAAAZUJV" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZUKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUJY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZUKE" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZUKE","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFXIX" varchar(1) NOT NULL default ' ',
  "AAABHPWS" varchar(1) NOT NULL default ' ',
  "AAAAZUKK" varchar(1) NOT NULL default ' ',
  "AAAAZUKL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFXIX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFXIX" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFXIX","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZULA" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZULA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZULB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZULF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZULF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZULG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSKT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSKT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZULL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSKT" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSKT","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZULT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSKS" varchar(4) NOT NULL default ' ',
  "AAAAZULW" varchar(5) NOT NULL default ' ',
  "AAAAZULX" varchar(5) NOT NULL default ' ',
  "AAAAZULY" varchar(2) NOT NULL default ' ',
  "AAAAZULZ" varchar(3) NOT NULL default '000',
  "AAAAZUMA" varchar(3) NOT NULL default '000',
  "AAAAZUMB" varchar(3) NOT NULL default '000',
  "AAAAZUMC" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABLSKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUMF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSKS" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSKS","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUMN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZUOB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZUOB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUMU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAABBOPZ" varchar(2) NOT NULL default ' ',
  "AAAAZUNP" varchar(10) NOT NULL default ' ',
  "AAABCAKN" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABCADO" varchar(4) NOT NULL default ' ',
  "AAAAZUND" varchar(2) NOT NULL default '00',
  "AAAAZWJA" varchar(2) NOT NULL default ' ',
  "AAABLFDD" varchar(1) NOT NULL default ' ',
  "AAAAZUNG" varchar(4) NOT NULL default ' ',
  "AAAAZUNH" varchar(4) NOT NULL default ' ',
  "AAAAZUNI" varchar(4) NOT NULL default ' ',
  "AAAAZUNJ" varchar(4) NOT NULL default ' ',
  "AAAAZUNK" varchar(4) NOT NULL default ' ',
  "AAAAZUNL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAABBOPZ","AAAAZUNP","AAABCAKN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUNS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZUOB" varchar(1) NOT NULL default ' ',
  "AAAAZXGL" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAEC","AAAAZUOB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUOC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZXKZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZXKZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUOJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZXIZ" varchar(1) NOT NULL default ' ',
  "AAAAZUON" varchar(1) NOT NULL default ' ',
  "AAAAZUOO" varchar(1) NOT NULL default ' ',
  "AAAAZUOP" varchar(1) NOT NULL default ' ',
  "AAAAZUOQ" varchar(1) NOT NULL default ' ',
  "AAAAZUOR" varchar(1) NOT NULL default ' ',
  "AAAAZXAE" varchar(1) NOT NULL default ' ',
  "AAAAZXBF" varchar(1) NOT NULL default ' ',
  "AAAAZUOU" varchar(1) NOT NULL default ' ',
  "AAAAZUOV" varchar(1) NOT NULL default ' ',
  "AAAAZUOW" varchar(1) NOT NULL default ' ',
  "AAAAZUOX" varchar(1) NOT NULL default ' ',
  "AAAAZUOY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZXIZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUPC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZXEF" varchar(1) NOT NULL default ' ',
  "AAAAZXIZ" varchar(1) NOT NULL default ' ',
  "AAABCAEE" varchar(1) NOT NULL default ' ',
  "AAAAZXEI" varchar(3) NOT NULL default ' ',
  "AAAAZXEJ" varchar(5) NOT NULL default ' ',
  "AAAAZXGF" varchar(1) NOT NULL default ' ',
  "AAAAZXEL" varchar(1) NOT NULL default ' ',
  "AAAAZXEM" varchar(2) NOT NULL default ' ',
  "AAAAZXEN" varchar(3) NOT NULL default '000',
  "AAABGCHM" varchar(6) NOT NULL default '000000',
  "AAABGCHN" varchar(6) NOT NULL default '000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZVAQ" varchar(3) NOT NULL default '000',
  "AAAAZVAR" varchar(3) NOT NULL default '000',
  "AAAAZVAS" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZXEF","AAAAZXIZ","AAABCAEE","AAAAZXEI","AAAAZXEJ","AAAAZXGF","AAAAZXEL","AAAAZXEM","AAAAZXEN","AAABGCHM","AAABGCHN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUQJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZXEL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZXEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZXEM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZXEM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUQX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZWJA" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZWJA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZURH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZWJA" varchar(2) NOT NULL default ' ',
  "AAABHXDG" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAEC","AAAAZWJA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZURR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZURU" varchar(1) NOT NULL default ' ',
  "AAAAZURV" varchar(1) NOT NULL default ' ',
  "AAAAZURW" varchar(1) NOT NULL default ' ',
  "AAAAZURX" varchar(1) NOT NULL default ' ',
  "AAAAZURY" varchar(1) NOT NULL default ' ',
  "AAAAZURZ" varchar(1) NOT NULL default ' ',
  "AAAAZUSA" varchar(1) NOT NULL default ' ',
  "AAAAZUSB" varchar(1) NOT NULL default ' ',
  "AAAAZUSC" varchar(1) NOT NULL default ' ',
  "AAAAZUSD" varchar(1) NOT NULL default ' ',
  "AAAAZUSE" varchar(1) NOT NULL default ' ',
  "AAAAZUSF" varchar(1) NOT NULL default ' ',
  "AAAAZUSG" varchar(1) NOT NULL default ' ',
  "AAAAZUSH" varchar(1) NOT NULL default ' ',
  "AAAAZUSI" varchar(1) NOT NULL default ' ',
  "AAAAZUSJ" varchar(1) NOT NULL default ' ',
  "AAAAZUSK" varchar(1) NOT NULL default ' ',
  "AAAAZUSL" varchar(1) NOT NULL default ' ',
  "AAAAZUSM" varchar(1) NOT NULL default ' ',
  "AAAAZUSN" varchar(1) NOT NULL default ' ',
  "AAAAZUSO" varchar(1) NOT NULL default ' ',
  "AAAAZUSP" varchar(1) NOT NULL default ' ',
  "AAAAZUSQ" varchar(1) NOT NULL default ' ',
  "AAAAZUSR" varchar(1) NOT NULL default ' ',
  "AAAAZUSS" varchar(1) NOT NULL default ' ',
  "AAAAZUST" varchar(1) NOT NULL default ' ',
  "AAAAZUSU" varchar(1) NOT NULL default ' ',
  "AAAAZUSV" varchar(1) NOT NULL default ' ',
  "AAAAZUSW" varchar(1) NOT NULL default ' ',
  "AAAAZUSX" varchar(1) NOT NULL default ' ',
  "AAABLWFN" varchar(1) NOT NULL default ' ',
  "AAABLWFP" varchar(1) NOT NULL default ' ',
  "AAABLWFR" varchar(1) NOT NULL default ' ',
  "AAABLWFT" varchar(1) NOT NULL default ' ',
  "AAABLWFV" varchar(1) NOT NULL default ' ',
  "AAABLWFX" varchar(1) NOT NULL default ' ',
  "AAABLWFZ" varchar(1) NOT NULL default ' ',
  "AAABLWGB" varchar(1) NOT NULL default ' ',
  "AAABLWGD" varchar(1) NOT NULL default ' ',
  "AAABLWGF" varchar(1) NOT NULL default ' ',
  "AAABLWGH" varchar(1) NOT NULL default ' ',
  "AAABLWGJ" varchar(1) NOT NULL default ' ',
  "AAABLWGL" varchar(1) NOT NULL default ' ',
  "AAABLWGN" varchar(1) NOT NULL default ' ',
  "AAABLWGP" varchar(1) NOT NULL default ' ',
  "AAABLWGR" varchar(1) NOT NULL default ' ',
  "AAABLWGT" varchar(1) NOT NULL default ' ',
  "AAABLWGV" varchar(1) NOT NULL default ' ',
  "AAABLWGX" varchar(1) NOT NULL default ' ',
  "AAABLWGZ" varchar(1) NOT NULL default ' ',
  "AAAAZUTS" varchar(1) NOT NULL default ' ',
  "AAAAZUTT" varchar(1) NOT NULL default ' ',
  "AAAAZUTU" varchar(1) NOT NULL default ' ',
  "AAAAZUTV" varchar(1) NOT NULL default ' ',
  "AAAAZUTW" varchar(1) NOT NULL default ' ',
  "AAAAZUTX" varchar(1) NOT NULL default ' ',
  "AAAAZUTY" varchar(1) NOT NULL default ' ',
  "AAAAZUTZ" varchar(1) NOT NULL default ' ',
  "AAAAZUUA" varchar(1) NOT NULL default ' ',
  "AAAAZUUB" varchar(1) NOT NULL default ' ',
  "AAAAZUUC" varchar(1) NOT NULL default ' ',
  "AAAAZUUD" varchar(1) NOT NULL default ' ',
  "AAAAZUUE" varchar(1) NOT NULL default ' ',
  "AAAAZUUF" varchar(1) NOT NULL default ' ',
  "AAAAZUUG" varchar(1) NOT NULL default ' ',
  "AAAAZUUH" varchar(1) NOT NULL default ' ',
  "AAAAZUUI" varchar(1) NOT NULL default ' ',
  "AAAAZUUJ" varchar(1) NOT NULL default ' ',
  "AAAAZUUK" varchar(1) NOT NULL default ' ',
  "AAAAZUUL" varchar(1) NOT NULL default ' ',
  "AAAAZUUM" varchar(1) NOT NULL default ' ',
  "AAAAZUUN" varchar(1) NOT NULL default ' ',
  "AAAAZUUO" varchar(1) NOT NULL default ' ',
  "AAAAZUUP" varchar(1) NOT NULL default ' ',
  "AAAAZUUQ" varchar(1) NOT NULL default ' ',
  "AAAAZUUR" varchar(1) NOT NULL default ' ',
  "AAAAZUUS" varchar(1) NOT NULL default ' ',
  "AAAAZUUT" varchar(1) NOT NULL default ' ',
  "AAAAZUUU" varchar(1) NOT NULL default ' ',
  "AAAAZUUV" varchar(1) NOT NULL default ' ',
  "AAAAZUUW" varchar(1) NOT NULL default ' ',
  "AAAAZUUX" varchar(1) NOT NULL default ' ',
  "AAAAZUUY" varchar(1) NOT NULL default ' ',
  "AAAAZUUZ" varchar(1) NOT NULL default ' ',
  "AAAAZUVA" varchar(1) NOT NULL default ' ',
  "AAAAZUVB" varchar(1) NOT NULL default ' ',
  "AAAAZUVC" varchar(1) NOT NULL default ' ',
  "AAAAZUVD" varchar(1) NOT NULL default ' ',
  "AAAAZUVE" varchar(1) NOT NULL default ' ',
  "AAAAZUVF" varchar(1) NOT NULL default ' ',
  "AAAAZUVG" varchar(1) NOT NULL default ' ',
  "AAAAZUVH" varchar(1) NOT NULL default ' ',
  "AAAAZUVI" varchar(1) NOT NULL default ' ',
  "AAAAZUVJ" varchar(1) NOT NULL default ' ',
  "AAAAZUVK" varchar(1) NOT NULL default ' ',
  "AAAAZUVL" varchar(1) NOT NULL default ' ',
  "AAAAZUVM" varchar(1) NOT NULL default ' ',
  "AAAAZUVN" varchar(1) NOT NULL default ' ',
  "AAAAZUVO" varchar(1) NOT NULL default ' ',
  "AAABHDPB" varchar(1) NOT NULL default ' ',
  "AAABHDPE" varchar(1) NOT NULL default ' ',
  "AAABHDPH" varchar(1) NOT NULL default ' ',
  "AAABHDPK" varchar(1) NOT NULL default ' ',
  "AAABHDPN" varchar(1) NOT NULL default ' ',
  "AAABHDPQ" varchar(1) NOT NULL default ' ',
  "AAABHDPT" varchar(1) NOT NULL default ' ',
  "AAABHDPW" varchar(1) NOT NULL default ' ',
  "AAABHDPZ" varchar(1) NOT NULL default ' ',
  "AAABHDQC" varchar(1) NOT NULL default ' ',
  "AAABHDQF" varchar(1) NOT NULL default ' ',
  "AAAAZUWA" varchar(1) NOT NULL default ' ',
  "AAAAZUWB" varchar(1) NOT NULL default ' ',
  "AAAAZUWC" varchar(1) NOT NULL default ' ',
  "AAAAZUWD" varchar(1) NOT NULL default ' ',
  "AAAAZUWE" varchar(1) NOT NULL default ' ',
  "AAAAZUWF" varchar(1) NOT NULL default ' ',
  "AAAAZUWG" varchar(1) NOT NULL default ' ',
  "AAAAZUWH" varchar(1) NOT NULL default ' ',
  "AAAAZUWI" varchar(1) NOT NULL default ' ',
  "AAAAZUWJ" varchar(1) NOT NULL default ' ',
  "AAAAZUWK" varchar(1) NOT NULL default ' ',
  "AAAAZUWL" varchar(1) NOT NULL default ' ',
  "AAAAZUWM" varchar(1) NOT NULL default ' ',
  "AAAAZUWN" varchar(1) NOT NULL default ' ',
  "AAAAZUWO" varchar(1) NOT NULL default ' ',
  "AAAAZUWP" varchar(1) NOT NULL default ' ',
  "AAAAZUWQ" varchar(1) NOT NULL default ' ',
  "AAAAZUWR" varchar(1) NOT NULL default ' ',
  "AAAAZUWS" varchar(1) NOT NULL default ' ',
  "AAAAZUWT" varchar(1) NOT NULL default ' ',
  "AAAAZUWU" varchar(1) NOT NULL default ' ',
  "AAAAZUWV" varchar(1) NOT NULL default ' ',
  "AAAAZUWW" varchar(1) NOT NULL default ' ',
  "AAAAZUWX" varchar(1) NOT NULL default ' ',
  "AAAAZUWY" varchar(1) NOT NULL default ' ',
  "AAAAZUWZ" varchar(1) NOT NULL default ' ',
  "AAAAZUXA" varchar(1) NOT NULL default ' ',
  "AAAAZUXB" varchar(1) NOT NULL default ' ',
  "AAAAZUXC" varchar(1) NOT NULL default ' ',
  "AAAAZUXD" varchar(1) NOT NULL default ' ',
  "AAAAZUXE" varchar(1) NOT NULL default ' ',
  "AAAAZUXF" varchar(1) NOT NULL default ' ',
  "AAAAZUXG" varchar(1) NOT NULL default ' ',
  "AAAAZUXH" varchar(1) NOT NULL default ' ',
  "AAAAZUXI" varchar(1) NOT NULL default ' ',
  "AAAAZUXJ" varchar(1) NOT NULL default ' ',
  "AAAAZUXK" varchar(1) NOT NULL default ' ',
  "AAAAZUXL" varchar(1) NOT NULL default ' ',
  "AAAAZUXM" varchar(1) NOT NULL default ' ',
  "AAAAZUXN" varchar(1) NOT NULL default ' ',
  "AAAAZUXO" varchar(1) NOT NULL default ' ',
  "AAAAZUXP" varchar(1) NOT NULL default ' ',
  "AAAAZUXQ" varchar(1) NOT NULL default ' ',
  "AAAAZUXR" varchar(1) NOT NULL default ' ',
  "AAAAZUXS" varchar(1) NOT NULL default ' ',
  "AAAAZUXT" varchar(1) NOT NULL default ' ',
  "AAAAZUXU" varchar(1) NOT NULL default ' ',
  "AAAAZUXV" varchar(1) NOT NULL default ' ',
  "AAAAZUXW" varchar(1) NOT NULL default ' ',
  "AAAAZUXX" varchar(1) NOT NULL default ' ',
  "AAAAZUXY" varchar(1) NOT NULL default ' ',
  "AAAAZUXZ" varchar(1) NOT NULL default ' ',
  "AAAAZUYA" varchar(1) NOT NULL default ' ',
  "AAAAZUYB" varchar(1) NOT NULL default ' ',
  "AAAAZUYC" varchar(1) NOT NULL default ' ',
  "AAAAZUYD" varchar(1) NOT NULL default ' ',
  "AAAAZUYE" varchar(1) NOT NULL default ' ',
  "AAAAZUYF" varchar(1) NOT NULL default ' ',
  "AAAAZUYG" varchar(1) NOT NULL default ' ',
  "AAAAZUYH" varchar(1) NOT NULL default ' ',
  "AAAAZUYI" varchar(1) NOT NULL default ' ',
  "AAAAZUYJ" varchar(1) NOT NULL default ' ',
  "AAAAZUYK" varchar(1) NOT NULL default ' ',
  "AAAAZUYL" varchar(1) NOT NULL default ' ',
  "AAAAZUYM" varchar(1) NOT NULL default ' ',
  "AAAAZUYN" varchar(1) NOT NULL default ' ',
  "AAAAZUYO" varchar(1) NOT NULL default ' ',
  "AAAAZUYP" varchar(1) NOT NULL default ' ',
  "AAAAZUYQ" varchar(1) NOT NULL default ' ',
  "AAAAZUYR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUYU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZXEJ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAABMAMP","AAAAZXEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUZD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZWNJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZWNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUZK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZWNJ" varchar(1) NOT NULL default ' ',
  "AAAAZXGL" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAEC","AAAAZWNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZUZU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAABCAEE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAABCAEE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVAB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZXEF" varchar(1) NOT NULL default ' ',
  "AAAAZXIZ" varchar(1) NOT NULL default ' ',
  "AAABCAEE" varchar(1) NOT NULL default ' ',
  "AAAAZXEI" varchar(3) NOT NULL default ' ',
  "AAAAZXEJ" varchar(5) NOT NULL default ' ',
  "AAAAZXGF" varchar(1) NOT NULL default ' ',
  "AAAAZXEL" varchar(1) NOT NULL default ' ',
  "AAAAZXEM" varchar(2) NOT NULL default ' ',
  "AAAAZXEN" varchar(3) NOT NULL default '000',
  "AAAAZXEO" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZVAQ" varchar(3) NOT NULL default '000',
  "AAAAZVAR" varchar(3) NOT NULL default '000',
  "AAAAZVAS" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZXEF","AAAAZXIZ","AAABCAEE","AAAAZXEI","AAAAZXEJ","AAAAZXGF","AAAAZXEL","AAAAZXEM","AAAAZXEN","AAAAZXEO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVBG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZXEZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZXEZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVBN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZXGF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZXGF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVBU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZXGP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZXGP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVCB" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAZVCK" varchar(1) NOT NULL default ' ',
  "AAAAZVCL" varchar(1) NOT NULL default ' ',
  "AAAAZVCM" varchar(1) NOT NULL default ' ',
  "AAABMOGI" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABHVYP","AAAAZVCK","AAAAZVCL","AAAAZVCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVCN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZVCP" varchar(1) NOT NULL default ' ',
  "AAAAZVCQ" varchar(1) NOT NULL default ' ',
  "AAAAZVCR" varchar(1) NOT NULL default ' ',
  "AAAAZVCS" varchar(1) NOT NULL default ' ',
  "AAAAZVCT" varchar(1) NOT NULL default ' ',
  "AAAAZVCU" varchar(1) NOT NULL default ' ',
  "AAAAZVCV" varchar(1) NOT NULL default ' ',
  "AAAAZVCW" varchar(1) NOT NULL default ' ',
  "AAAAZVCX" varchar(1) NOT NULL default ' ',
  "AAAAZVCY" varchar(1) NOT NULL default ' ',
  "AAAAZVCZ" varchar(1) NOT NULL default ' ',
  "AAAAZVDA" varchar(1) NOT NULL default ' ',
  "AAAAZVDB" varchar(1) NOT NULL default ' ',
  "AAAAZVDC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVDE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAAZVDI" varchar(4) NOT NULL default ' ',
  "AAAAZVDJ" varchar(1) NOT NULL default ' ',
  "AAAAZVDK" varchar(1) NOT NULL default ' ',
  "AAAAZVDL" varchar(1) NOT NULL default ' ',
  "AAAAZVDM" varchar(1) NOT NULL default ' ',
  "AAAAZVDN" varchar(1) NOT NULL default ' ',
  "AAAAZVDO" varchar(1) NOT NULL default ' ',
  "AAAAZVDP" varchar(2) NOT NULL default ' ',
  "AAAAZVDQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVDT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABDPUA" varchar(1) NOT NULL default ' ',
  "AAAAZVDY" varchar(1) NOT NULL default ' ',
  "AAAAZVDZ" varchar(1) NOT NULL default ' ',
  "AAAAZVEA" varchar(1) NOT NULL default ' ',
  "AAABIVOR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVEF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZVEH" varchar(1) NOT NULL default ' ',
  "AAAAZVEI" varchar(1) NOT NULL default ' ',
  "AAAAZVEJ" varchar(1) NOT NULL default ' ',
  "AAAAZVEK" varchar(1) NOT NULL default ' ',
  "AAAAZVEL" varchar(1) NOT NULL default ' ',
  "AAAAZVEM" varchar(1) NOT NULL default ' ',
  "AAAAZVEN" varchar(1) NOT NULL default ' ',
  "AAAAZVEO" varchar(1) NOT NULL default ' ',
  "AAAAZVEP" varchar(1) NOT NULL default ' ',
  "AAAAZVEQ" varchar(1) NOT NULL default ' ',
  "AAAAZVER" varchar(1) NOT NULL default ' ',
  "AAAAZVES" varchar(1) NOT NULL default ' ',
  "AAAAZVET" varchar(1) NOT NULL default ' ',
  "AAAAZVEU" varchar(1) NOT NULL default ' ',
  "AAAAZVEV" varchar(1) NOT NULL default ' ',
  "AAAAZVEW" varchar(1) NOT NULL default ' ',
  "AAAAZVEX" varchar(1) NOT NULL default ' ',
  "AAAAZVEY" varchar(1) NOT NULL default ' ',
  "AAAAZVEZ" varchar(1) NOT NULL default ' ',
  "AAAAZVFA" varchar(1) NOT NULL default ' ',
  "AAAAZVFB" varchar(1) NOT NULL default ' ',
  "AAAAZVFC" varchar(1) NOT NULL default ' ',
  "AAAAZVFD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVFF" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABGLFH" varchar(2) NOT NULL default ' ',
  "AAAAZVQQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABGLFH","AAAAZVQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVFM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABMJWN" varchar(5) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  "AAABLXIS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVFX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABMJWN" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVGE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAAAZVGH" varchar(4) NOT NULL default ' ',
  "AAAAZVGI" varchar(1) NOT NULL default ' ',
  "AAAAZVGJ" varchar(1) NOT NULL default ' ',
  "AAAAZVQQ" varchar(1) NOT NULL default ' ',
  "AAABGLFH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMGHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVGO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGLFI" varchar(3) NOT NULL default '000',
  "AAABKPUA" varchar(6) NOT NULL default ' ',
  "AAAAZVGT" varchar(1) NOT NULL default ' ',
  "AAAAZVGU" varchar(1) NOT NULL default ' ',
  "AAAAZVNW" varchar(5) NOT NULL default ' ',
  "AAAAZVNV" varchar(1) NOT NULL default ' ',
  "AAAAZVGX" varchar(1) NOT NULL default ' ',
  "AAAAZVGY" varchar(1) NOT NULL default ' ',
  "AAAAZVNY" varchar(5) NOT NULL default ' ',
  "AAAAZVNX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGLFI","AAABKPUA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVHF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGLFI" varchar(3) NOT NULL default '000',
  "AAABMRCU" varchar(1) NOT NULL default ' ',
  "AAABEIIS" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGLFI","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVHP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLFE" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABGLFI" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGLFE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGLFI" varchar(3) NOT NULL default '000',
  "AAAAZVRG" varchar(5) NOT NULL default ' ',
  "AAABBHDK" decimal(13,3) NOT NULL default '0.000',
  "AAAAZVIF" varchar(3) NOT NULL default ' ',
  "AAABBAEF" varchar(5) NOT NULL default ' ',
  "AAAAZVIH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGLFI","AAAAZVRG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVIM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGLFH" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABGLFI" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZVIU" varchar(1) NOT NULL default ' ',
  "AAAAZVIV" varchar(1) NOT NULL default ' ',
  "AAAAZVIW" varchar(1) NOT NULL default ' ',
  "AAAAZVIX" varchar(5) NOT NULL default ' ',
  "AAAAZVIY" varchar(1) NOT NULL default ' ',
  "AAAAZVIZ" varchar(5) NOT NULL default ' ',
  "AAAAZVJA" varchar(1) NOT NULL default ' ',
  "AAAAZVJB" varchar(1) NOT NULL default ' ',
  "AAAAZVJC" varchar(1) NOT NULL default ' ',
  "AAAAZVJD" varchar(5) NOT NULL default ' ',
  "AAAAZVJE" varchar(1) NOT NULL default ' ',
  "AAAAZVJF" varchar(5) NOT NULL default ' ',
  "AAAAZVJG" varchar(1) NOT NULL default ' ',
  "AAAAZVJH" varchar(1) NOT NULL default ' ',
  "AAAAZVJI" varchar(1) NOT NULL default ' ',
  "AAAAZVJJ" varchar(5) NOT NULL default ' ',
  "AAAAZVJK" varchar(1) NOT NULL default ' ',
  "AAAAZVJL" varchar(5) NOT NULL default ' ',
  "AAAAZVJM" varchar(1) NOT NULL default ' ',
  "AAAAZVJN" varchar(1) NOT NULL default ' ',
  "AAAAZVJO" varchar(1) NOT NULL default ' ',
  "AAAAZVJP" varchar(5) NOT NULL default ' ',
  "AAAAZVJQ" varchar(1) NOT NULL default ' ',
  "AAAAZVJR" varchar(5) NOT NULL default ' ',
  "AAAAZVJS" varchar(1) NOT NULL default ' ',
  "AAAAZVJT" varchar(1) NOT NULL default ' ',
  "AAAAZVJU" varchar(1) NOT NULL default ' ',
  "AAAAZVJV" varchar(5) NOT NULL default ' ',
  "AAAAZVJW" varchar(1) NOT NULL default ' ',
  "AAAAZVJX" varchar(5) NOT NULL default ' ',
  "AAAAZVJY" varchar(1) NOT NULL default ' ',
  "AAAAZVJZ" varchar(1) NOT NULL default ' ',
  "AAAAZVKA" varchar(1) NOT NULL default ' ',
  "AAAAZVKB" varchar(5) NOT NULL default ' ',
  "AAAAZVKC" varchar(1) NOT NULL default ' ',
  "AAAAZVKD" varchar(5) NOT NULL default ' ',
  "AAAAZVKE" varchar(1) NOT NULL default ' ',
  "AAAAZVKF" varchar(1) NOT NULL default ' ',
  "AAAAZVKG" varchar(1) NOT NULL default ' ',
  "AAAAZVKH" varchar(5) NOT NULL default ' ',
  "AAAAZVKI" varchar(1) NOT NULL default ' ',
  "AAAAZVKJ" varchar(5) NOT NULL default ' ',
  "AAAAZVKK" varchar(1) NOT NULL default ' ',
  "AAAAZVKL" varchar(1) NOT NULL default ' ',
  "AAAAZVKM" varchar(1) NOT NULL default ' ',
  "AAAAZVKN" varchar(5) NOT NULL default ' ',
  "AAAAZVKO" varchar(1) NOT NULL default ' ',
  "AAAAZVKP" varchar(5) NOT NULL default ' ',
  "AAAAZVKQ" varchar(1) NOT NULL default ' ',
  "AAAAZVKR" varchar(1) NOT NULL default ' ',
  "AAAAZVKS" varchar(1) NOT NULL default ' ',
  "AAAAZVKT" varchar(5) NOT NULL default ' ',
  "AAAAZVKU" varchar(1) NOT NULL default ' ',
  "AAAAZVKV" varchar(5) NOT NULL default ' ',
  "AAAAZVKW" varchar(1) NOT NULL default ' ',
  "AAAAZVKX" varchar(1) NOT NULL default ' ',
  "AAAAZVKY" varchar(1) NOT NULL default ' ',
  "AAAAZVKZ" varchar(5) NOT NULL default ' ',
  "AAAAZVLA" varchar(1) NOT NULL default ' ',
  "AAAAZVLB" varchar(5) NOT NULL default ' ',
  "AAAAZVLC" varchar(1) NOT NULL default ' ',
  "AAAAZVLD" varchar(1) NOT NULL default ' ',
  "AAAAZVLE" varchar(1) NOT NULL default ' ',
  "AAAAZVLF" varchar(5) NOT NULL default ' ',
  "AAAAZVLG" varchar(1) NOT NULL default ' ',
  "AAAAZVLH" varchar(5) NOT NULL default ' ',
  "AAAAZVLI" varchar(1) NOT NULL default ' ',
  "AAAAZVLJ" varchar(1) NOT NULL default ' ',
  "AAAAZVLK" varchar(1) NOT NULL default ' ',
  "AAAAZVLL" varchar(5) NOT NULL default ' ',
  "AAAAZVLM" varchar(1) NOT NULL default ' ',
  "AAAAZVLN" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGLFH","AAABHVYL","AAABGLFI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVLU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGLFH" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZVMB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGLFH","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVMH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLFH" varchar(2) NOT NULL default ' ',
  "AAAAZVMK" varchar(4) NOT NULL default ' ',
  "AAAAZVML" varchar(4) NOT NULL default ' ',
  "AAAAZVMM" varchar(4) NOT NULL default ' ',
  "AAAAZVMN" varchar(4) NOT NULL default ' ',
  "AAAAZVMO" varchar(4) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAZVMQ" varchar(1) NOT NULL default ' ',
  "AAAAZVMR" varchar(1) NOT NULL default ' ',
  "AAAAZVMS" varchar(1) NOT NULL default ' ',
  "AAAAZVMT" varchar(1) NOT NULL default ' ',
  "AAAAZVMU" varchar(1) NOT NULL default ' ',
  "AAAAZVMV" varchar(1) NOT NULL default ' ',
  "AAAAZVMW" varchar(1) NOT NULL default ' ',
  "AAAAZVMX" varchar(1) NOT NULL default ' ',
  "AAAAZVMY" varchar(1) NOT NULL default ' ',
  "AAAAZVMZ" varchar(1) NOT NULL default ' ',
  "AAAAZVNA" varchar(1) NOT NULL default ' ',
  "AAAAZVNB" varchar(1) NOT NULL default ' ',
  "AAAAZVNC" varchar(1) NOT NULL default ' ',
  "AAAAZVND" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGLFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVNG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABGLFH" varchar(2) NOT NULL default ' ',
  "AAABBHNP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABGLFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVNO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGLFI" varchar(3) NOT NULL default '000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAZVNT" varchar(1) NOT NULL default ' ',
  "AAAAZVNU" varchar(1) NOT NULL default ' ',
  "AAAAZVNV" varchar(1) NOT NULL default ' ',
  "AAAAZVNW" varchar(5) NOT NULL default ' ',
  "AAAAZVNX" varchar(1) NOT NULL default ' ',
  "AAAAZVNY" varchar(5) NOT NULL default ' ',
  "AAAAZVNZ" varchar(1) NOT NULL default ' ',
  "AAAAZVOA" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGLFI","AAABHVYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVOF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZVON" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAZVOP" varchar(9) NOT NULL default '000000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLENG" varchar(9) NOT NULL default '000000000',
  PRIMARY KEY ("AAABMRIL","AAAAZVON","AAABKFCZ","AAAAZVOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVOQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGLFI" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZVOW" decimal(9,2) NOT NULL default '0.00',
  "AAAAZVOX" decimal(9,2) NOT NULL default '0.00',
  "AAAAZVOY" decimal(9,2) NOT NULL default '0.00',
  "AAAAZVOZ" decimal(9,2) NOT NULL default '0.00',
  "AAAAZVPA" decimal(9,2) NOT NULL default '0.00',
  "AAABKOEQ" varchar(1) NOT NULL default ' ',
  "AAAAZVPC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGLFI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGLFI" varchar(3) NOT NULL default '000',
  "AAABBHNP" varchar(30) NOT NULL default ' ',
  "AAAAZWTG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABGLFI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVPS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLFI" varchar(3) NOT NULL default '000',
  "AAAAZVQV" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGLFI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABGLFI" varchar(3) NOT NULL default '000',
  "AAABBHNP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABGLFI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVQG" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABGLFH" varchar(2) NOT NULL default ' ',
  "AAAAZVQQ" varchar(1) NOT NULL default ' ',
  "AAABGLYO" varchar(3) NOT NULL default '000',
  "AAABDVBI" varchar(30) NOT NULL default ' ',
  "AAABDVBJ" varchar(3) NOT NULL default '000',
  "AAABGLYC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABGLFH","AAAAZVQQ","AAABGLYO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVQS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZVRG" varchar(5) NOT NULL default ' ',
  "AAAAZVQV" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZVRG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAZVRG" varchar(5) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  "AAABLXIS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAZVRG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVRH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAZVRX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAZVRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVRO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAZVRX" varchar(2) NOT NULL default ' ',
  "AAAAZVRT" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAAAZVRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVRY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZWBK" varchar(1) NOT NULL default ' ',
  "AAABIBIY" varchar(10) NOT NULL default ' ',
  "AAABLRWA" varchar(1) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABHANX" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZWBK","AAABIBIY","AAABLRWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVSK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZVXI" varchar(3) NOT NULL default ' ',
  "AAABGCHO" varchar(1) NOT NULL default ' ',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAAAZVWA" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABGCHE" varchar(4) NOT NULL default ' ',
  "AAAAZVST" varchar(1) NOT NULL default ' ',
  "AAAAZVSU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZVXI","AAABGCHO","AAABGCHD","AAAAZVWA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVTB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHZ" varchar(2) NOT NULL default '00',
  "AAABGKXE" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZVTH" varchar(10) NOT NULL default ' ',
  "AAAAZVTI" varchar(1) NOT NULL default ' ',
  "AAABKWQO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHZ","AAABGKXE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVTO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZVZM" varchar(2) NOT NULL default ' ',
  "AAAAZVZU" varchar(2) NOT NULL default ' ',
  "AAAAZVTS" varchar(1) NOT NULL default ' ',
  "AAAAZVTT" varchar(1) NOT NULL default ' ',
  "AAAAZVTU" varchar(1) NOT NULL default ' ',
  "AAAAZVTV" varchar(1) NOT NULL default ' ',
  "AAAAZVTW" varchar(1) NOT NULL default ' ',
  "AAAAZVTX" varchar(1) NOT NULL default ' ',
  "AAAAZVTY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZVZM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVUB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZVZU" varchar(2) NOT NULL default ' ',
  "AAAAZVUE" varchar(1) NOT NULL default ' ',
  "AAAAZVUF" varchar(1) NOT NULL default ' ',
  "AAAAZVUG" varchar(1) NOT NULL default ' ',
  "AAAAZVUH" varchar(1) NOT NULL default ' ',
  "AAAAZVUI" varchar(1) NOT NULL default ' ',
  "AAAAZVUJ" varchar(1) NOT NULL default ' ',
  "AAAAZVUK" varchar(1) NOT NULL default ' ',
  "AAAAZVUL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZVZU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVUO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHZ" varchar(2) NOT NULL default '00',
  "AAABBFEB" varchar(1) NOT NULL default ' ',
  "AAABKWQO" varchar(2) NOT NULL default ' ',
  "AAABLUVE" varchar(2) NOT NULL default '00',
  "AAAAZVUU" varchar(55) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHZ","AAABBFEB","AAABKWQO","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVVA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZVYD" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAZVYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVVF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAZVYD" varchar(2) NOT NULL default '00',
  "AAABILTS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAZVYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVVN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZVXI" varchar(3) NOT NULL default ' ',
  "AAABGCHO" varchar(1) NOT NULL default ' ',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAAAZVWA" varchar(4) NOT NULL default ' ',
  "AAABHPIX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZVXI","AAABGCHO","AAABGCHD","AAAAZVWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVWB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZVXI" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZVXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVWG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZVYO" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZVYO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVWL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZVYO" varchar(3) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABEFZK" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABEFZN" decimal(9,2) NOT NULL default '0.00',
  "AAABEFZM" decimal(7,2) NOT NULL default '0.00',
  "AAABKRZV" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZVYO","AAABHVYP","AAABEFZK","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVXB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAZVXI" varchar(3) NOT NULL default ' ',
  "AAABHPIX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAZVXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVXJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAZVYO" varchar(3) NOT NULL default ' ',
  "AAABHPIX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAZVYO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVXR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZVYD" varchar(2) NOT NULL default '00',
  "AAAAZVZM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZVYD","AAAAZVZM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVXY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZVYD" varchar(2) NOT NULL default '00',
  "AAABGLCX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZVYD","AAABGLCX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAZVYO" varchar(3) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABEFZK" varchar(4) NOT NULL default ' ',
  "AAABBHNP" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAZVYO","AAABHVYP","AAABEFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVYR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNLE" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAAAZWBB" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNLE","AAABLUNJ","AAAAZWBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZWBB" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZWBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVZF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAZVZM" varchar(2) NOT NULL default ' ',
  "AAABLPUZ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAZVZM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVZN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAZVZU" varchar(2) NOT NULL default ' ',
  "AAABLWJT" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAZVZU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZVZV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZWBK" varchar(1) NOT NULL default ' ',
  "AAAAZVZY" varchar(1) NOT NULL default '0',
  "AAAAZVZZ" varchar(1) NOT NULL default ' ',
  "AAAAZWAA" varchar(4) NOT NULL default '0000',
  "AAAAZWAB" varchar(1) NOT NULL default ' ',
  "AAAAZWAC" varchar(1) NOT NULL default ' ',
  "AAAAZWAD" varchar(1) NOT NULL default ' ',
  "AAAAZWAE" varchar(1) NOT NULL default ' ',
  "AAAAZWAF" varchar(1) NOT NULL default ' ',
  "AAAAZWAG" varchar(1) NOT NULL default ' ',
  "AAAAZWAH" varchar(1) NOT NULL default ' ',
  "AAAAZWAI" varchar(1) NOT NULL default ' ',
  "AAAAZWAJ" varchar(1) NOT NULL default ' ',
  "AAAAZWAK" varchar(1) NOT NULL default ' ',
  "AAAAZWAL" varchar(1) NOT NULL default ' ',
  "AAAAZWAM" varchar(1) NOT NULL default ' ',
  "AAAAZWAN" varchar(1) NOT NULL default ' ',
  "AAAAZWAO" varchar(1) NOT NULL default ' ',
  "AAAAZWAP" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAZWAR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZWBK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWAU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAZWBB" varchar(3) NOT NULL default ' ',
  "AAABHPIX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAZWBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWBC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZWBK" varchar(1) NOT NULL default ' ',
  "AAABLRWA" varchar(1) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABHANX" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZWBK","AAABLRWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWBM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZWZW" varchar(1) NOT NULL default ' ',
  "AAAAZWCG" varchar(1) NOT NULL default ' ',
  "AAAAZXIZ" varchar(1) NOT NULL default ' ',
  "AAABCAEE" varchar(1) NOT NULL default ' ',
  "AAAAZXEL" varchar(1) NOT NULL default ' ',
  "AAAAZXEM" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZWBX" varchar(1) NOT NULL default ' ',
  "AAAAZWBY" decimal(10,2) NOT NULL default '0.00',
  "AAAAZWBZ" varchar(1) NOT NULL default ' ',
  "AAAAZWCA" decimal(10,2) NOT NULL default '0.00',
  "AAAAZWCB" varchar(5) NOT NULL default ' ',
  "AAAAZWCC" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZWZW","AAAAZWCG","AAAAZXIZ","AAABCAEE","AAAAZXEL","AAAAZXEM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWCM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZXIZ" varchar(1) NOT NULL default ' ',
  "AAABCAEE" varchar(1) NOT NULL default ' ',
  "AAAAZXEI" varchar(3) NOT NULL default ' ',
  "AAAAZXEJ" varchar(5) NOT NULL default ' ',
  "AAAAZXGF" varchar(1) NOT NULL default ' ',
  "AAAAZXEL" varchar(1) NOT NULL default ' ',
  "AAAAZXEM" varchar(2) NOT NULL default ' ',
  "AAABCADO" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAAAZWEP" varchar(1) NOT NULL default ' ',
  "AAAAZWEQ" varchar(1) NOT NULL default ' ',
  "AAAAZWDC" decimal(10,2) NOT NULL default '0.00',
  "AAAAZWER" varchar(9) NOT NULL default ' ',
  "AAAAZWIS" varchar(4) NOT NULL default ' ',
  "AAAAZWIT" varchar(4) NOT NULL default ' ',
  "AAABCAKN" varchar(1) NOT NULL default ' ',
  "AAAAZWDH" decimal(10,2) NOT NULL default '0.00',
  "AAAAZWHM" varchar(1) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAZWEW" varchar(1) NOT NULL default ' ',
  "AAAAZWEX" varchar(1) NOT NULL default ' ',
  "AAAAZWEY" varchar(1) NOT NULL default ' ',
  "AAAAZWEZ" varchar(1) NOT NULL default ' ',
  "AAAAZWFA" varchar(1) NOT NULL default ' ',
  "AAAAZWFB" varchar(1) NOT NULL default ' ',
  "AAAAZWFC" varchar(1) NOT NULL default ' ',
  "AAAAZWFD" varchar(1) NOT NULL default ' ',
  "AAAAZWFE" varchar(1) NOT NULL default ' ',
  "AAAAZWFF" varchar(1) NOT NULL default ' ',
  "AAAAZWFG" varchar(1) NOT NULL default ' ',
  "AAAAZWFH" varchar(1) NOT NULL default ' ',
  "AAAAZWEV" varchar(1) NOT NULL default ' ',
  "AAAAZWIU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZXIZ","AAABCAEE","AAAAZXEI","AAAAZXEJ","AAAAZXGF","AAAAZXEL","AAAAZXEM","AAABCADO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWEJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAABCADO" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZWEP" varchar(1) NOT NULL default ' ',
  "AAAAZWEQ" varchar(1) NOT NULL default ' ',
  "AAAAZWER" varchar(9) NOT NULL default ' ',
  "AAABCAKN" varchar(1) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAAAZWEU" varchar(1) NOT NULL default ' ',
  "AAAAZWEV" varchar(1) NOT NULL default ' ',
  "AAAAZWEW" varchar(1) NOT NULL default ' ',
  "AAAAZWEX" varchar(1) NOT NULL default ' ',
  "AAAAZWEY" varchar(1) NOT NULL default ' ',
  "AAAAZWEZ" varchar(1) NOT NULL default ' ',
  "AAAAZWFA" varchar(1) NOT NULL default ' ',
  "AAAAZWFB" varchar(1) NOT NULL default ' ',
  "AAAAZWFC" varchar(1) NOT NULL default ' ',
  "AAAAZWFD" varchar(1) NOT NULL default ' ',
  "AAAAZWFE" varchar(1) NOT NULL default ' ',
  "AAAAZWFF" varchar(1) NOT NULL default ' ',
  "AAAAZWFG" varchar(1) NOT NULL default ' ',
  "AAAAZWFH" varchar(1) NOT NULL default ' ',
  "AAAAZWHS" varchar(1) NOT NULL default ' ',
  "AAAAZWFJ" varchar(1) NOT NULL default ' ',
  "AAAAZWFK" varchar(1) NOT NULL default ' ',
  "AAAAZWFL" varchar(1) NOT NULL default ' ',
  "AAAAZWFM" varchar(1) NOT NULL default ' ',
  "AAAAZWFN" varchar(1) NOT NULL default ' ',
  "AAAAZWFO" varchar(1) NOT NULL default ' ',
  "AAAAZWFP" varchar(1) NOT NULL default ' ',
  "AAAAZWFQ" varchar(4) NOT NULL default ' ',
  "AAAAZWFR" varchar(4) NOT NULL default ' ',
  "AAAAZWFS" varchar(1) NOT NULL default ' ',
  "AAAAZWFT" varchar(1) NOT NULL default ' ',
  "AAAAZWFU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAABCADO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWFZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZXIZ" varchar(1) NOT NULL default ' ',
  "AAAAZXGF" varchar(1) NOT NULL default ' ',
  "AAABCAEE" varchar(1) NOT NULL default ' ',
  "AAAAZXEI" varchar(3) NOT NULL default ' ',
  "AAAAZXEJ" varchar(5) NOT NULL default ' ',
  "AAAAZXEL" varchar(1) NOT NULL default ' ',
  "AAAAZXEM" varchar(2) NOT NULL default ' ',
  "AAABCADO" varchar(4) NOT NULL default ' ',
  "AAABFLGW" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABBHDK" decimal(10,2) NOT NULL default '0.00',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default ' ',
  "AAAAZWGR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZXIZ","AAAAZXGF","AAABCAEE","AAAAZXEI","AAAAZXEJ","AAAAZXEL","AAAAZXEM","AAABCADO","AAABFLGW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZWHS" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABADPG" decimal(10,2) NOT NULL default '0.00',
  "AAAAZWHM" varchar(1) NOT NULL default ' ',
  "AAABBPGR" varchar(1) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZWHS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWHU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAABCADO" varchar(4) NOT NULL default ' ',
  "AAAAZWJA" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZWIS" varchar(4) NOT NULL default ' ',
  "AAAAZWIT" varchar(4) NOT NULL default ' ',
  "AAAAZWIU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAABCADO","AAAAZWJA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWIJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZWIX" varchar(2) NOT NULL default ' ',
  "AAABKWRL" varchar(1) NOT NULL default ' ',
  "AAABCADO" varchar(4) NOT NULL default ' ',
  "AAAAZWJA" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZWIS" varchar(4) NOT NULL default ' ',
  "AAAAZWIT" varchar(4) NOT NULL default ' ',
  "AAAAZWIU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZWIX","AAABKWRL","AAABCADO","AAAAZWJA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWJC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAABCADO" varchar(4) NOT NULL default ' ',
  "AAAAZWJR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAEC","AAABCADO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWJM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAABCADO" varchar(4) NOT NULL default ' ',
  "AAAAZWJR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAEC","AAABCADO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWJW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZXIZ" varchar(1) NOT NULL default ' ',
  "AAABCAEE" varchar(1) NOT NULL default ' ',
  "AAAAZXKZ" varchar(1) NOT NULL default ' ',
  "AAAAZWKC" varchar(1) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAAAZWKE" varchar(2) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZWKI" varchar(2) NOT NULL default ' ',
  "AAAAZWKJ" varchar(2) NOT NULL default ' ',
  "AAAAZWKK" varchar(2) NOT NULL default ' ',
  "AAAAZWKL" varchar(2) NOT NULL default ' ',
  "AAAAZWKM" varchar(1) NOT NULL default '0',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAZWKO" varchar(1) NOT NULL default ' ',
  "AAAAZWKP" varchar(1) NOT NULL default ' ',
  "AAAAZWKQ" varchar(1) NOT NULL default ' ',
  "AAAAZWKR" varchar(1) NOT NULL default ' ',
  "AAAAZWKS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWKV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZXKZ" varchar(1) NOT NULL default ' ',
  "AAAAZWLA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAEC","AAAAZXKZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWLF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZWLM" varchar(32) NOT NULL default ' ',
  "AAABLPSU" varchar(255) NOT NULL default ' ',
  "AAAAZWLJ" varchar(8) NOT NULL default '00000000',
  "AAAAZWLK" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAAAZWLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWLN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZWLQ" varchar(128) NOT NULL default ' ',
  "AAABLSBE" varchar(128) NOT NULL default ' ',
  "AAAAZWLS" varchar(255) NOT NULL default ' ',
  "AAAAZWLT" varchar(255) NOT NULL default ' ',
  "AAAAZWLU" varchar(1) NOT NULL default ' ',
  "AAAAZWLV" varchar(1) NOT NULL default ' ',
  "AAAAZWLW" varchar(1) NOT NULL default ' ',
  "AAAAZWLX" varchar(1) NOT NULL default ' ',
  "AAAAZWLY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWMB" (
  "AAABHHRZ" varchar(8) NOT NULL default ' ',
  "AAAAZWMD" varchar(1) NOT NULL default '0',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAAAZWMF" varchar(8) NOT NULL default ' ',
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHHRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWMI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZXIZ" varchar(1) NOT NULL default ' ',
  "AAABCAEE" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZXEJ" varchar(5) NOT NULL default ' ',
  "AAAAZXGF" varchar(1) NOT NULL default ' ',
  "AAAAZXKZ" varchar(1) NOT NULL default ' ',
  "AAAAZWNJ" varchar(1) NOT NULL default ' ',
  "AAAAZXEZ" varchar(3) NOT NULL default ' ',
  "AAAAZWZH" varchar(1) NOT NULL default ' ',
  "AAAAZWNM" varchar(6) NOT NULL default '000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAZXEA" decimal(10,2) NOT NULL default '0.00',
  "AAAAZXEB" decimal(10,2) NOT NULL default '0.00',
  "AAAAZXEC" decimal(10,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZXIZ","AAABCAEE","AAABMAMP","AAAAZXEJ","AAAAZXGF","AAAAZXKZ","AAAAZWNJ","AAAAZXEZ","AAAAZWZH","AAAAZWNM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWNO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZXKZ" varchar(1) NOT NULL default ' ',
  "AAAAZXEZ" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZWNV" varchar(1) NOT NULL default ' ',
  "AAAAZWZH" varchar(1) NOT NULL default ' ',
  "AAAAZWNX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZXKZ","AAAAZXEZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWOD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZWTA" varchar(4) NOT NULL default ' ',
  "AAABMAYY" varchar(2) NOT NULL default ' ',
  "AAAAZWOS" varchar(1) NOT NULL default ' ',
  "AAABKMWI" varchar(3) NOT NULL default '000',
  "AAAAZWRP" varchar(3) NOT NULL default ' ',
  "AAAAZWRR" varchar(1) NOT NULL default '0',
  "AAAAZWRS" varchar(1) NOT NULL default ' ',
  "AAABAMRA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZWTA","AAABMAYY","AAAAZWOS","AAABKMWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZWTA" varchar(4) NOT NULL default ' ',
  "AAAAZWQO" varchar(3) NOT NULL default ' ',
  "AAAAZWOZ" varchar(1) NOT NULL default ' ',
  "AAABIDCA" varchar(2) NOT NULL default '00',
  "AAAAZWPB" varchar(2) NOT NULL default '00',
  "AAAAZWPC" varchar(2) NOT NULL default '00',
  "AAAAZWPD" varchar(2) NOT NULL default '00',
  "AAAAZWPE" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZWTA","AAAAZWQO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWPJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZWTA" varchar(4) NOT NULL default ' ',
  "AAAAZWQO" varchar(3) NOT NULL default ' ',
  "AAAAZWPP" varchar(78) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAEC","AAAAZWTA","AAAAZWQO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWPV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZWTA" varchar(4) NOT NULL default ' ',
  "AAAAZWQO" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZWTA","AAAAZWQO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWQE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZWTA" varchar(4) NOT NULL default ' ',
  "AAAAZWQO" varchar(3) NOT NULL default ' ',
  "AAAAZWQP" varchar(10) NOT NULL default ' ',
  "AAAAZWQK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZWTA","AAAAZWQO","AAAAZWQP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZWRP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZWRP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWQX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZWRP" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(78) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAEC","AAAAZWRP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWRH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZWTA" varchar(4) NOT NULL default ' ',
  "AAABMAYY" varchar(2) NOT NULL default ' ',
  "AAABKMWI" varchar(3) NOT NULL default '000',
  "AAAAZWRZ" varchar(3) NOT NULL default '000',
  "AAABJEMU" varchar(40) NOT NULL default ' ',
  "AAAAZWRP" varchar(3) NOT NULL default ' ',
  "AAABFRJL" varchar(3) NOT NULL default '000',
  "AAAAZWRR" varchar(1) NOT NULL default '0',
  "AAAAZWRS" varchar(1) NOT NULL default ' ',
  "AAABAMRA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZWTA","AAABMAYY","AAABKMWI","AAAAZWRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWSA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZWTA" varchar(4) NOT NULL default ' ',
  "AAABLOPB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZWTA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWSI" (
  "AAABJEMU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWSL" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJEMU" varchar(40) NOT NULL default ' ',
  "AAAAZWSO" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABJEMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWSR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZWTA" varchar(4) NOT NULL default ' ',
  "AAAAZWSW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAEC","AAAAZWTA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWTB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZXIZ" varchar(1) NOT NULL default ' ',
  "AAAAZWTG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAEC","AAAAZXIZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWTL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZXEF" varchar(1) NOT NULL default ' ',
  "AAAAZXIZ" varchar(1) NOT NULL default ' ',
  "AAABCAEE" varchar(1) NOT NULL default ' ',
  "AAAAZXEI" varchar(3) NOT NULL default ' ',
  "AAAAZXEJ" varchar(5) NOT NULL default ' ',
  "AAAAZXGF" varchar(1) NOT NULL default ' ',
  "AAAAZXEL" varchar(1) NOT NULL default ' ',
  "AAAAZXEM" varchar(2) NOT NULL default ' ',
  "AAAAZXEN" varchar(3) NOT NULL default '000',
  "AAABGCHM" varchar(6) NOT NULL default '000000',
  "AAABGCHN" varchar(6) NOT NULL default '000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZXEA" decimal(10,2) NOT NULL default '0.00',
  "AAAAZXEB" decimal(10,2) NOT NULL default '0.00',
  "AAAAZXEC" decimal(10,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZXEF","AAAAZXIZ","AAABCAEE","AAAAZXEI","AAAAZXEJ","AAAAZXGF","AAAAZXEL","AAAAZXEM","AAAAZXEN","AAABGCHM","AAABGCHN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWUT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZXEL" varchar(1) NOT NULL default ' ',
  "AAAAZXGL" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAEC","AAAAZXEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWVD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZXEM" varchar(2) NOT NULL default ' ',
  "AAAAZXGL" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAEC","AAAAZXEM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWVN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAZWWH" varchar(5) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZWVU" varchar(3) NOT NULL default ' ',
  "AAAAZWVV" varchar(3) NOT NULL default ' ',
  "AAAAZWVW" varchar(2) NOT NULL default ' ',
  "AAAAZWVX" varchar(1) NOT NULL default ' ',
  "AAAAZWVY" varchar(1) NOT NULL default ' ',
  "AAAAZWVZ" varchar(1) NOT NULL default ' ',
  "AAAAZWWA" varchar(1) NOT NULL default ' ',
  "AAAAZWWB" varchar(1) NOT NULL default ' ',
  "AAAAZWWC" varchar(1) NOT NULL default ' ',
  "AAAAZWWD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAABHVYL","AAAAZWWH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWWJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZXEJ" varchar(5) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZWWQ" varchar(3) NOT NULL default ' ',
  "AAAAZWWR" varchar(3) NOT NULL default ' ',
  "AAAAZWWS" varchar(3) NOT NULL default ' ',
  "AAAAZWWT" varchar(5) NOT NULL default ' ',
  "AAAAZWWU" varchar(5) NOT NULL default ' ',
  "AAAAZWWV" varchar(5) NOT NULL default ' ',
  "AAAAZWWW" varchar(3) NOT NULL default ' ',
  "AAAAZWWX" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAABMAMP","AAAAZXEJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWXD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZXEJ" varchar(5) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZWXK" varchar(2) NOT NULL default ' ',
  "AAAAZWXL" varchar(2) NOT NULL default ' ',
  "AAAAZWXM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAABMAMP","AAAAZXEJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWXS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZXGL" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAEC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWYA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAZXEJ" varchar(5) NOT NULL default ' ',
  "AAAAZXGL" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAEC","AAABMAMP","AAAAZXEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWYM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZWZH" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZWYS" varchar(6) NOT NULL default ' ',
  "AAAAZWZD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZWZH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWYY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZWZH" varchar(1) NOT NULL default ' ',
  "AAAAZWZD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAEC","AAAAZWZH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWZI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAABCAEE" varchar(1) NOT NULL default ' ',
  "AAABHWSF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAEC","AAABCAEE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZWZS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAABCADZ" varchar(2) NOT NULL default ' ',
  "AAAAZWZW" varchar(1) NOT NULL default ' ',
  "AAAAZWZX" varchar(3) NOT NULL default ' ',
  "AAAAZWZY" varchar(1) NOT NULL default ' ',
  "AAAAZWZZ" varchar(1) NOT NULL default ' ',
  "AAAAZXAA" varchar(1) NOT NULL default ' ',
  "AAAAZXAB" varchar(1) NOT NULL default ' ',
  "AAAAZXAC" varchar(1) NOT NULL default ' ',
  "AAABKQHD" varchar(1) NOT NULL default ' ',
  "AAAAZXAE" varchar(1) NOT NULL default ' ',
  "AAAAZXGF" varchar(1) NOT NULL default ' ',
  "AAAAZXAG" varchar(1) NOT NULL default ' ',
  "AAAAZXEJ" varchar(5) NOT NULL default ' ',
  "AAAAZXIZ" varchar(1) NOT NULL default ' ',
  "AAABCAEE" varchar(1) NOT NULL default ' ',
  "AAAAZXAK" varchar(1) NOT NULL default ' ',
  "AAAAZXAL" varchar(1) NOT NULL default ' ',
  "AAAAZXAM" varchar(1) NOT NULL default ' ',
  "AAAAZXAN" varchar(1) NOT NULL default ' ',
  "AAAAZXAO" varchar(1) NOT NULL default ' ',
  "AAAAZXAP" varchar(1) NOT NULL default ' ',
  "AAAAZXAQ" varchar(1) NOT NULL default ' ',
  "AAAAZXAR" varchar(1) NOT NULL default ' ',
  "AAAAZXAS" varchar(1) NOT NULL default ' ',
  "AAAAZXAT" varchar(1) NOT NULL default ' ',
  "AAAAZXAU" varchar(1) NOT NULL default ' ',
  "AAAAZXAV" varchar(1) NOT NULL default ' ',
  "AAAAZXAW" varchar(1) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default ' ',
  "AAAAZXAZ" varchar(1) NOT NULL default ' ',
  "AAAAZXBA" varchar(1) NOT NULL default ' ',
  "AAAAZXBB" varchar(1) NOT NULL default ' ',
  "AAAAZXBC" varchar(1) NOT NULL default ' ',
  "AAAAZXBD" varchar(30) NOT NULL default ' ',
  "AAABJDPL" varchar(8) NOT NULL default '00000000',
  "AAAAZXBF" varchar(1) NOT NULL default ' ',
  "AAAAZXBG" varchar(2) NOT NULL default ' ',
  "AAAAZXBH" varchar(2) NOT NULL default ' ',
  "AAAAZXBI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAABCADZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXBM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAABCADZ" varchar(2) NOT NULL default ' ',
  "AAABLPWW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAEC","AAABCADZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXBW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZXIY" varchar(1) NOT NULL default ' ',
  "AAAAZXIZ" varchar(1) NOT NULL default ' ',
  "AAAAZXCC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAEC","AAAAZXIY","AAAAZXIZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXCI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZXEF" varchar(1) NOT NULL default ' ',
  "AAAAZXIZ" varchar(1) NOT NULL default ' ',
  "AAABCAEE" varchar(1) NOT NULL default ' ',
  "AAAAZXEI" varchar(3) NOT NULL default ' ',
  "AAAAZXEJ" varchar(5) NOT NULL default ' ',
  "AAAAZXGF" varchar(1) NOT NULL default ' ',
  "AAAAZXEL" varchar(1) NOT NULL default ' ',
  "AAAAZXEM" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAZXEA" decimal(10,2) NOT NULL default '0.00',
  "AAAAZXEB" decimal(10,2) NOT NULL default '0.00',
  "AAAAZXEC" decimal(10,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZXEF","AAAAZXIZ","AAABCAEE","AAAAZXEI","AAAAZXEJ","AAAAZXGF","AAAAZXEL","AAAAZXEM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXDK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZXEF" varchar(1) NOT NULL default ' ',
  "AAAAZXIZ" varchar(1) NOT NULL default ' ',
  "AAABCAEE" varchar(1) NOT NULL default ' ',
  "AAAAZXEI" varchar(3) NOT NULL default ' ',
  "AAAAZXEJ" varchar(5) NOT NULL default ' ',
  "AAAAZXGF" varchar(1) NOT NULL default ' ',
  "AAAAZXEL" varchar(1) NOT NULL default ' ',
  "AAAAZXEM" varchar(2) NOT NULL default ' ',
  "AAAAZXEN" varchar(3) NOT NULL default '000',
  "AAAAZXEO" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZXEA" decimal(10,2) NOT NULL default '0.00',
  "AAAAZXEB" decimal(10,2) NOT NULL default '0.00',
  "AAAAZXEC" decimal(10,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZXEF","AAAAZXIZ","AAABCAEE","AAAAZXEI","AAAAZXEJ","AAAAZXGF","AAAAZXEL","AAAAZXEM","AAAAZXEN","AAAAZXEO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZXEZ" varchar(3) NOT NULL default ' ',
  "AAAAZXGL" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAEC","AAAAZXEZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXFA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLENL" varchar(1) NOT NULL default '0',
  "AAAAZXFF" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAABMOHM","AAABLENL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXFK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLENL" varchar(1) NOT NULL default '0',
  "AAABKYOM" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAEC","AAABMOHM","AAABLENL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXFW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZXGF" varchar(1) NOT NULL default ' ',
  "AAAAZXGL" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAEC","AAAAZXGF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXGG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZXGP" varchar(3) NOT NULL default ' ',
  "AAAAZXGL" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAEC","AAAAZXGP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXGQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZXIM" varchar(15) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAAAZXHM" varchar(1) NOT NULL default ' ',
  "AAABHTLA" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZXIM","AAABMGHE","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXHD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABCADZ" varchar(2) NOT NULL default ' ',
  "AAAAZXIM" varchar(15) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAAAZXIP" varchar(4) NOT NULL default ' ',
  "AAAAZXHM" varchar(1) NOT NULL default ' ',
  "AAABHTLA" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAABMNYT","AAABCADZ","AAAAZXIM","AAABMGHE","AAABKAFO","AAAAZXIP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXHW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABCADZ" varchar(2) NOT NULL default ' ',
  "AAAAZXIM" varchar(15) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAAAZXIP" varchar(4) NOT NULL default ' ',
  "AAAAZXIG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAEC","AAABMNYT","AAABCADZ","AAAAZXIM","AAABMGHE","AAABKAFO","AAAAZXIP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXIQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZXIY" varchar(1) NOT NULL default ' ',
  "AAAAZXIZ" varchar(1) NOT NULL default ' ',
  "AAABLTQJ" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZXIY","AAAAZXIZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXJA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBOPZ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBOPZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXJF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABBOPZ" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABBOPZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAABBAEF" varchar(5) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLXZQ" decimal(9,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABFNEH" decimal(4,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAABBAEF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXKC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAABBAEF" varchar(5) NOT NULL default ' ',
  "AAABBAED" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAEC","AAABBAEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZXLR" varchar(1) NOT NULL default ' ',
  "AAABCADO" varchar(4) NOT NULL default ' ',
  "AAAAZXKZ" varchar(1) NOT NULL default ' ',
  "AAAAZXLA" varchar(5) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAABLBDX" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZXLR","AAABCADO","AAAAZXKZ","AAAAZXLA","AAABLZNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXLC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAZXLG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAABHVYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXLK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZXLR" varchar(1) NOT NULL default ' ',
  "AAABLBDX" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZXLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXLS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZXMI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAZXMI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXLZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAZXMI" varchar(20) NOT NULL default ' ',
  "AAABMRCK" longtext,
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAEC","AAAAZXMI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXMJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABJATT" varchar(2) NOT NULL default ' ',
  "AAABJQRH" varchar(2) NOT NULL default ' ',
  "AAAAZZVX" varchar(8) NOT NULL default ' ',
  "AAAAZZVY" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZXMS" decimal(13,2) NOT NULL default '0.00',
  "AAAAZXMT" decimal(13,2) NOT NULL default '0.00',
  "AAAAZXMU" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABJATT","AAABJQRH","AAAAZZVX","AAAAZZVY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXNC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABJATT" varchar(2) NOT NULL default ' ',
  "AAABJQRH" varchar(2) NOT NULL default ' ',
  "AAAAZZVX" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZXNK" decimal(13,2) NOT NULL default '0.00',
  "AAAAZXNL" decimal(13,2) NOT NULL default '0.00',
  "AAAAZXNM" varchar(3) NOT NULL default '000',
  "AAAAZXNN" varchar(5) NOT NULL default '00000',
  "AAAAZXNO" varchar(5) NOT NULL default '00000',
  "AAAAZZVO" varchar(1) NOT NULL default ' ',
  "AAABAROT" varchar(1) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABBGQC" varchar(4) NOT NULL default ' ',
  "AAABAOLK" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABJATT","AAABJQRH","AAAAZZVX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXOA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZZVO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZZVO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXOF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAZZVO" varchar(1) NOT NULL default ' ',
  "AAAAZXOJ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAZZVO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXON" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABABEG" varchar(4) NOT NULL default ' ',
  "AAAAZXPD" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABABEG","AAAAZXPD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABABEG" varchar(4) NOT NULL default ' ',
  "AAAAZXPD" varchar(8) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABABEG","AAAAZXPD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXPE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHXKL" varchar(4) NOT NULL default ' ',
  "AAAAZXQC" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZXPK" decimal(13,2) NOT NULL default '0.00',
  "AAAAZXPL" decimal(13,2) NOT NULL default '0.00',
  "AAAAZXPM" decimal(5,2) NOT NULL default '0.00',
  "AAABKPBA" varchar(5) NOT NULL default ' ',
  "AAABKPXV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHXKL","AAAAZXQC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXPT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHXKL" varchar(4) NOT NULL default ' ',
  "AAAAZXQC" varchar(8) NOT NULL default ' ',
  "AAABMOQQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHXKL","AAAAZXQC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXQD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAMI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXQI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAAMI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAKV" varchar(4) NOT NULL default ' ',
  "AAAAZXQT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAKV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXQW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAAKV" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAAKV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXRE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAABAAKV" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZXYW" varchar(4) NOT NULL default ' ',
  "AAAAZYCG" varchar(4) NOT NULL default ' ',
  "AAABBBDA" varchar(1) NOT NULL default ' ',
  "AAABAAIB" decimal(13,2) NOT NULL default '0.00',
  "AAAAZXRO" varchar(1) NOT NULL default ' ',
  "AAAAZXRP" decimal(13,2) NOT NULL default '0.00',
  "AAABAYCQ" varchar(5) NOT NULL default ' ',
  "AAAAZXRR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAMI","AAABAAKV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXRW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAAKV" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZXSD" varchar(4) NOT NULL default ' ',
  "AAAAZXSE" varchar(4) NOT NULL default ' ',
  "AAABBBFS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAMI","AAABHVYP","AAABAAKV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXSL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAAAZXUD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAMI","AAAAZXUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXSS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAAAZXUD" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAAMI","AAAAZXUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXTC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAAAZXUE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAMI","AAAAZXUE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXTJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAAAZXUE" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAAMI","AAAAZXUE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXTT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAAAZXUD" varchar(4) NOT NULL default ' ',
  "AAAAZXUE" varchar(4) NOT NULL default ' ',
  "AAABAAKV" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAAMI","AAAAZXUD","AAAAZXUE","AAABAAKV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXUH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZXVD" varchar(4) NOT NULL default ' ',
  "AAAAZXUK" varchar(8) NOT NULL default '00000000',
  "AAAAZXUL" varchar(8) NOT NULL default '00000000',
  "AAAAZXUM" varchar(1) NOT NULL default ' ',
  "AAABADUQ" varchar(8) NOT NULL default '00000000',
  "AAABADUR" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAZXVD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXUR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAZXVD" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAZXVD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXUZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEIQX" varchar(4) NOT NULL default ' ',
  "AAABAAKV" varchar(4) NOT NULL default ' ',
  "AAAAZXVD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEIQX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXVG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEIQX" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEIQX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXVO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAABEIQX" varchar(4) NOT NULL default ' ',
  "AAAAZXVS" varchar(8) NOT NULL default '00000000',
  "AAABACUG" varchar(8) NOT NULL default '00000000',
  "AAABACUI" varchar(8) NOT NULL default '00000000',
  "AAAAZXVV" decimal(4,0) NOT NULL default '0',
  "AAAAZXVW" varchar(3) NOT NULL default ' ',
  "AAAAZYSF" varchar(1) NOT NULL default ' ',
  "AAAAZXVY" varchar(8) NOT NULL default '00000000',
  "AAAAZXVZ" varchar(2) NOT NULL default ' ',
  "AAAAZXWA" varchar(8) NOT NULL default '00000000',
  "AAAAZXWB" varchar(8) NOT NULL default '00000000',
  "AAAAZXWC" varchar(1) NOT NULL default ' ',
  "AAAAZXWD" varchar(1) NOT NULL default ' ',
  "AAAAZXWE" varchar(8) NOT NULL default '00000000',
  "AAAAZXWF" varchar(8) NOT NULL default '00000000',
  "AAAAZXWG" varchar(8) NOT NULL default '00000000',
  "AAABLYAX" varchar(4) NOT NULL default ' ',
  "AAAAZXWI" varchar(1) NOT NULL default ' ',
  "AAAAZXWJ" varchar(1) NOT NULL default ' ',
  "AAAAZXWK" varchar(1) NOT NULL default ' ',
  "AAAAZXWL" varchar(2) NOT NULL default ' ',
  "AAAAZZVO" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABAAMI","AAABEIQX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXWQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAAAZXYW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAMI","AAAAZXYW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXWX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAAAZXYW" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAAMI","AAAAZXYW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXXH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAAAZYOD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAMI","AAAAZYOD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXXO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAAAZYOD" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAAMI","AAAAZYOD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXXY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAAAZXYW" varchar(4) NOT NULL default ' ',
  "AAAAZYOD" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZXYF" varchar(1) NOT NULL default ' ',
  "AAAAZYMF" decimal(4,0) NOT NULL default '0',
  "AAAAZXYH" varchar(3) NOT NULL default ' ',
  "AAAAZXYI" varchar(4) NOT NULL default ' ',
  "AAAAZYMK" decimal(7,2) NOT NULL default '0.00',
  "AAAAZYML" varchar(3) NOT NULL default ' ',
  "AAAAZXYL" decimal(3,0) NOT NULL default '0',
  "AAAAZXYM" decimal(3,0) NOT NULL default '0',
  "AAAAZXYN" decimal(3,2) NOT NULL default '0.00',
  "AAAAZXYO" decimal(3,2) NOT NULL default '0.00',
  "AAABBHUT" varchar(4) NOT NULL default ' ',
  "AAAAZXYQ" decimal(4,0) NOT NULL default '0',
  "AAABABZF" varchar(3) NOT NULL default ' ',
  "AAAAZXYS" varchar(1) NOT NULL default ' ',
  "AAAAZXYT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAMI","AAAAZXYW","AAAAZYOD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXYZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAABBHUT" varchar(4) NOT NULL default ' ',
  "AAAAZXZD" decimal(4,0) NOT NULL default '0',
  "AAAAZXZE" varchar(3) NOT NULL default ' ',
  "AAAAZXZF" varchar(1) NOT NULL default ' ',
  "AAAAZXZG" varchar(8) NOT NULL default '00000000',
  "AAAAZYXE" varchar(8) NOT NULL default '00000000',
  "AAAAZXZI" varchar(2) NOT NULL default ' ',
  "AAAAZXZJ" varchar(8) NOT NULL default '00000000',
  "AAAAZXZK" varchar(2) NOT NULL default ' ',
  "AAAAZXZL" varchar(8) NOT NULL default '00000000',
  "AAAAZXZM" decimal(15,3) NOT NULL default '0.000',
  "AAAAZXZN" decimal(15,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABAAMI","AAABBHUT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZXZR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAABBHUT" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAAMI","AAABBHUT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYAB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAAAZYCG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAMI","AAAAZYCG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYAI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAAAZYCG" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAAMI","AAAAZYCG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYAS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAAAZYCH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAMI","AAAAZYCH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYAZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAAAZYCH" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAAMI","AAAAZYCH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYBJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAAAZYCG" varchar(4) NOT NULL default ' ',
  "AAAAZYCH" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZYHR" varchar(4) NOT NULL default ' ',
  "AAAAZYDL" varchar(4) NOT NULL default ' ',
  "AAAAZZVO" varchar(2) NOT NULL default '00',
  "AAAAZYHE" decimal(13,2) NOT NULL default '0.00',
  "AAAAZYHF" decimal(13,2) NOT NULL default '0.00',
  "AAAAZYHG" decimal(13,2) NOT NULL default '0.00',
  "AAAAZYHH" decimal(7,2) NOT NULL default '0.00',
  "AAAAZYHI" decimal(7,2) NOT NULL default '0.00',
  "AAAAZYHJ" decimal(7,2) NOT NULL default '0.00',
  "AAAAZYHK" decimal(10,0) NOT NULL default '0',
  "AAABMGWU" decimal(10,0) NOT NULL default '0',
  "AAAAZYHM" decimal(10,0) NOT NULL default '0',
  "AAABAYCQ" varchar(5) NOT NULL default ' ',
  "AAABAAHT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAMI","AAAAZYCG","AAAAZYCH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYCJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAAAZYDL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZYCP" varchar(1) NOT NULL default ' ',
  "AAAAZYCQ" varchar(2) NOT NULL default '00',
  "AAAAZYCR" varchar(2) NOT NULL default '00',
  "AAABBBDA" varchar(1) NOT NULL default ' ',
  "AAABAAIB" decimal(13,2) NOT NULL default '0.00',
  "AAAAZYCU" decimal(7,2) NOT NULL default '0.00',
  "AAABAAIE" decimal(10,0) NOT NULL default '0',
  "AAABAYCQ" varchar(5) NOT NULL default ' ',
  "AAABAAHT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAMI","AAAAZYDL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYDC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAAAZYDL" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAAMI","AAAAZYDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYDM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAAAZYHR" varchar(4) NOT NULL default ' ',
  "AAAAZZBQ" varchar(4) NOT NULL default ' ',
  "AAAAZZBS" varchar(4) NOT NULL default ' ',
  "AAAAZZBU" varchar(4) NOT NULL default ' ',
  "AAAAZYDT" varchar(1) NOT NULL default ' ',
  "AAAAZYDU" varchar(1) NOT NULL default ' ',
  "AAAAZYDV" varchar(1) NOT NULL default ' ',
  "AAAAZYDW" varchar(1) NOT NULL default ' ',
  "AAAAZYDX" varchar(1) NOT NULL default ' ',
  "AAAAZYDY" varchar(1) NOT NULL default ' ',
  "AAAAZYDZ" varchar(1) NOT NULL default ' ',
  "AAAAZYEA" varchar(1) NOT NULL default ' ',
  "AAAAZYEB" varchar(1) NOT NULL default ' ',
  "AAAAZZVO" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABAAMI","AAAAZYHR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYEG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAAAZYHR" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAAMI","AAAAZYHR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAABELUC" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZYEW" decimal(4,0) NOT NULL default '0',
  "AAAAZYEX" varchar(3) NOT NULL default ' ',
  "AAAAZYFX" varchar(4) NOT NULL default ' ',
  "AAAAZYEZ" varchar(60) NOT NULL default ' ',
  "AAAAZYFA" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAMI","AAABELUC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYFF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAABELUC" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAAMI","AAABELUC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYFP" (
  "AAAAZYFX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAAZYFX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYFS" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAZYFX" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAZYFX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYFY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAABELUC" varchar(4) NOT NULL default ' ',
  "AAAAZZFR" varchar(4) NOT NULL default ' ',
  "AAAAZYGD" decimal(15,3) NOT NULL default '0.000',
  "AAAAZYGE" decimal(15,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABAAMI","AAABELUC","AAAAZZFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYGJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAABELUC" varchar(4) NOT NULL default ' ',
  "AAAAZZFR" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAAMI","AAABELUC","AAAAZZFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYGV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAAAZYHR" varchar(4) NOT NULL default ' ',
  "AAAAZZHB" varchar(4) NOT NULL default ' ',
  "AAAAZZHC" varchar(4) NOT NULL default ' ',
  "AAAAZZHD" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZYHE" decimal(13,2) NOT NULL default '0.00',
  "AAAAZYHF" decimal(13,2) NOT NULL default '0.00',
  "AAAAZYHG" decimal(13,2) NOT NULL default '0.00',
  "AAAAZYHH" decimal(7,2) NOT NULL default '0.00',
  "AAAAZYHI" decimal(7,2) NOT NULL default '0.00',
  "AAAAZYHJ" decimal(7,2) NOT NULL default '0.00',
  "AAAAZYHK" decimal(10,0) NOT NULL default '0',
  "AAABMGWU" decimal(10,0) NOT NULL default '0',
  "AAAAZYHM" decimal(10,0) NOT NULL default '0',
  "AAABAYCQ" varchar(5) NOT NULL default ' ',
  "AAABAAHT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAMI","AAAAZYHR","AAAAZZHB","AAAAZZHC","AAAAZZHD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYHW" (
  "AAABLUZL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYHZ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUZL" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLUZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYIF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHYMV" varchar(4) NOT NULL default ' ',
  "AAABJGBU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHYMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYIL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHYMV" varchar(4) NOT NULL default ' ',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMAYY" varchar(4) NOT NULL default ' ',
  "AAABBGQC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHYMV","AAABAAMI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYIX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHYMV" varchar(4) NOT NULL default ' ',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZYJD" varchar(1) NOT NULL default ' ',
  "AAABBGQC" varchar(4) NOT NULL default ' ',
  "AAABMAYY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHYMV","AAABAAMI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYJK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHYMV" varchar(4) NOT NULL default ' ',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBGQC" varchar(4) NOT NULL default ' ',
  "AAABFYUL" decimal(13,2) NOT NULL default '0.00',
  "AAABKPBA" varchar(5) NOT NULL default ' ',
  "AAABMAYY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHYMV","AAABAAMI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYJY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHYMV" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHYMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZYKY" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZYKY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYKL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAZYKY" varchar(6) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAZYKY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZYKY" varchar(6) NOT NULL default ' ',
  "AAABHYMV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZYKY","AAABHYMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYLA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAMI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYLG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAAMI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYLO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHEHA" varchar(4) NOT NULL default ' ',
  "AAABEUQF" varchar(8) NOT NULL default '00000000',
  "AAABEUQG" varchar(8) NOT NULL default '00000000',
  "AAAAZYLT" varchar(8) NOT NULL default '00000000',
  "AAAAZYLU" varchar(8) NOT NULL default '00000000',
  "AAABBEEM" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABHEHA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYLY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZYQU" varchar(8) NOT NULL default ' ',
  "AAAAZYOD" varchar(6) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZYME" varchar(1) NOT NULL default ' ',
  "AAAAZYMF" decimal(4,0) NOT NULL default '0',
  "AAAAZYMG" varchar(3) NOT NULL default ' ',
  "AAABBHTA" varchar(2) NOT NULL default '00',
  "AAABBHTB" varchar(2) NOT NULL default '00',
  "AAABJRCJ" varchar(2) NOT NULL default ' ',
  "AAAAZYMK" decimal(4,0) NOT NULL default '0',
  "AAAAZYML" varchar(3) NOT NULL default ' ',
  "AAAAZYMM" varchar(1) NOT NULL default ' ',
  "AAAAZYTS" varchar(6) NOT NULL default ' ',
  "AAAAZYMO" varchar(1) NOT NULL default ' ',
  "AAAAZYUU" varchar(8) NOT NULL default ' ',
  "AAAAZYMQ" varchar(1) NOT NULL default ' ',
  "AAAAZYVQ" varchar(6) NOT NULL default ' ',
  "AAABBHSP" varchar(1) NOT NULL default ' ',
  "AAAAZYWM" varchar(6) NOT NULL default ' ',
  "AAABBOZM" varchar(1) NOT NULL default ' ',
  "AAAAZYXS" varchar(6) NOT NULL default ' ',
  "AAAAZYMW" varchar(1) NOT NULL default ' ',
  "AAAAZYYO" varchar(6) NOT NULL default ' ',
  "AAAAZYMY" varchar(12) NOT NULL default ' ',
  "AAABFZAB" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZYQU","AAAAZYOD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYNE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZYQU" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZYQU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYNJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAZYQU" varchar(8) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAZYQU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYNR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZYOD" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZYOD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYNW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZYOC" varchar(1) NOT NULL default ' ',
  "AAAAZYOD" varchar(6) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZYOC","AAAAZYOD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYOE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBMQM" varchar(2) NOT NULL default ' ',
  "AAABAAKV" varchar(4) NOT NULL default ' ',
  "AAABLYAX" varchar(4) NOT NULL default ' ',
  "AAABEHKP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBMQM","AAABAAKV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYON" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBMQM" varchar(2) NOT NULL default ' ',
  "AAAAZZVO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBMQM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYOT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBMQM" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBMQM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYPB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBMQM" varchar(2) NOT NULL default ' ',
  "AAABAAKV" varchar(4) NOT NULL default ' ',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAAAZYQU" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBMQM","AAABAAKV","AAABAAMI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYPL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAKV" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHYMV" varchar(4) NOT NULL default ' ',
  "AAAAZYPR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAKV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYPV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAAKV" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAAKV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYQD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAKV" varchar(4) NOT NULL default ' ',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZYSE" varchar(1) NOT NULL default ' ',
  "AAAAZYSF" varchar(1) NOT NULL default ' ',
  "AAAAZYSG" varchar(2) NOT NULL default '00',
  "AAAAZYSH" varchar(2) NOT NULL default '00',
  "AAABEQNK" varchar(2) NOT NULL default ' ',
  "AAAAZYRN" varchar(4) NOT NULL default ' ',
  "AAAAZYRO" varchar(2) NOT NULL default '00',
  "AAAAZYRP" varchar(2) NOT NULL default '00',
  "AAAAZYRQ" varchar(1) NOT NULL default ' ',
  "AAABAZEO" varchar(8) NOT NULL default ' ',
  "AAABABEW" varchar(4) NOT NULL default ' ',
  "AAAAZYQU" varchar(8) NOT NULL default ' ',
  "AAAAZYQV" varchar(1) NOT NULL default ' ',
  "AAAAZYQW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAKV","AAABAAMI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYRB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAKV" varchar(4) NOT NULL default ' ',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABJGBU" varchar(1) NOT NULL default ' ',
  "AAAAZYSE" varchar(1) NOT NULL default ' ',
  "AAAAZYSF" varchar(1) NOT NULL default ' ',
  "AAAAZYSG" varchar(2) NOT NULL default '00',
  "AAAAZYSH" varchar(2) NOT NULL default '00',
  "AAABEQNK" varchar(2) NOT NULL default ' ',
  "AAAAZYRN" varchar(4) NOT NULL default ' ',
  "AAAAZYRO" varchar(2) NOT NULL default '00',
  "AAAAZYRP" varchar(2) NOT NULL default '00',
  "AAAAZYRQ" varchar(1) NOT NULL default ' ',
  "AAABBGQC" varchar(4) NOT NULL default ' ',
  "AAAAZZZY" varchar(8) NOT NULL default ' ',
  "AAABABEW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAKV","AAABAAMI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYRY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAKV" varchar(4) NOT NULL default ' ',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZYSE" varchar(1) NOT NULL default ' ',
  "AAAAZYSF" varchar(1) NOT NULL default ' ',
  "AAAAZYSG" varchar(2) NOT NULL default '00',
  "AAAAZYSH" varchar(2) NOT NULL default '00',
  "AAABEQNK" varchar(2) NOT NULL default ' ',
  "AAABBGQC" varchar(4) NOT NULL default ' ',
  "AAABFYUL" decimal(13,2) NOT NULL default '0.00',
  "AAABKPBA" varchar(5) NOT NULL default ' ',
  "AAAAZZZY" varchar(8) NOT NULL default ' ',
  "AAABABEW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAKV","AAABAAMI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYSS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZYTS" varchar(6) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABJATT" varchar(2) NOT NULL default ' ',
  "AAABJQRH" varchar(2) NOT NULL default ' ',
  "AAAAZYUB" varchar(8) NOT NULL default ' ',
  "AAAAZYUC" varchar(2) NOT NULL default ' ',
  "AAAAZYUD" varchar(8) NOT NULL default ' ',
  "AAAAZYUE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZYTS","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYTG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZYTS" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZYTS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYTL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAZYTS" varchar(6) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAZYTS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYTT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZYUU" varchar(8) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAAAZYUB" varchar(8) NOT NULL default ' ',
  "AAAAZYUC" varchar(2) NOT NULL default ' ',
  "AAAAZYUD" varchar(8) NOT NULL default ' ',
  "AAAAZYUE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZYUU","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYUI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZYUU" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZYUU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYUN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAZYUU" varchar(8) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAZYUU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYUV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZYVQ" varchar(6) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAAAZYUZ" varchar(12) NOT NULL default ' ',
  "AAAAZYVA" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZYVQ","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYVE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZYVQ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZYVQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYVJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAZYVQ" varchar(6) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAZYVQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYVR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZYWM" varchar(6) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAAAZYVV" decimal(7,4) NOT NULL default '0.0000',
  "AAAAZYVW" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAAAZYWM","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYWA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZYWM" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZYWM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYWF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAZYWM" varchar(6) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAZYWM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYWN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZYXS" varchar(6) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAAAZYWR" decimal(15,3) NOT NULL default '0.000',
  "AAAAZYWS" decimal(15,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAAAZYXS","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYWW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZYXS" varchar(6) NOT NULL default ' ',
  "AAAAZYWZ" varchar(8) NOT NULL default '00000000',
  "AAAAZYXA" varchar(2) NOT NULL default '00',
  "AAAAZYXB" varchar(2) NOT NULL default '00',
  "AAAAZYXC" varchar(1) NOT NULL default ' ',
  "AAAAZYXD" varchar(1) NOT NULL default ' ',
  "AAAAZYXE" varchar(8) NOT NULL default '00000000',
  "AAAAZYXF" varchar(2) NOT NULL default ' ',
  "AAAAZYXG" varchar(8) NOT NULL default '00000000',
  "AAAAZYXH" varchar(2) NOT NULL default ' ',
  "AAAAZYXI" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAZYXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYXL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAZYXS" varchar(6) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAZYXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYXT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZYYO" varchar(6) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAAAZYXX" varchar(10) NOT NULL default ' ',
  "AAAAZYXY" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZYYO","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYYC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZYYO" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZYYO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYYH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAZYYO" varchar(6) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAZYYO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYYP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAAAZZZY" varchar(8) NOT NULL default ' ',
  "AAAAZZAL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBJZZ" varchar(1) NOT NULL default '0',
  "AAAAZYYX" decimal(9,5) NOT NULL default '0.00000',
  "AAAAZYYY" decimal(13,2) NOT NULL default '0.00',
  "AAABEGKI" decimal(15,5) NOT NULL default '0.00000',
  "AAABKPBA" varchar(5) NOT NULL default ' ',
  "AAABJFIP" varchar(1) NOT NULL default ' ',
  "AAAAZZHA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAMI","AAAAZZZY","AAAAZZAL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYZI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAAAZZZY" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAMI","AAAAZZZY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYZP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAAAZZZY" varchar(8) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAAMI","AAAAZZZY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZYZZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZZAL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZZAL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZAE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAZZAL" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAZZAL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZAM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAABAZEO" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABABDL" varchar(2) NOT NULL default '00',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAZZZY" varchar(8) NOT NULL default ' ',
  "AAABBBDA" varchar(1) NOT NULL default ' ',
  "AAAAZZAW" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABAAMI","AAABAZEO","AAABKFCZ","AAABABDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZBC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAABAZEO" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAMI","AAABAZEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZBJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAAAZZHA" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZZBP" varchar(1) NOT NULL default ' ',
  "AAAAZZBQ" varchar(4) NOT NULL default ' ',
  "AAAAZZBR" varchar(1) NOT NULL default ' ',
  "AAAAZZBS" varchar(4) NOT NULL default ' ',
  "AAAAZZBT" varchar(1) NOT NULL default ' ',
  "AAAAZZBU" varchar(4) NOT NULL default ' ',
  "AAABJFIP" varchar(1) NOT NULL default ' ',
  "AAABKPBA" varchar(5) NOT NULL default ' ',
  "AAABMKAI" varchar(3) NOT NULL default ' ',
  "AAAAZZBY" varchar(1) NOT NULL default ' ',
  "AAAAZZBZ" varchar(1) NOT NULL default ' ',
  "AAAAZZCA" varchar(1) NOT NULL default ' ',
  "AAAAZZCB" varchar(1) NOT NULL default ' ',
  "AAAAZZCC" varchar(1) NOT NULL default ' ',
  "AAAAZZCD" varchar(1) NOT NULL default ' ',
  "AAAAZZCE" varchar(1) NOT NULL default ' ',
  "AAAAZZCF" varchar(1) NOT NULL default ' ',
  "AAAAZZCG" varchar(1) NOT NULL default ' ',
  "AAAAZZCH" varchar(1) NOT NULL default ' ',
  "AAAAZZCI" varchar(1) NOT NULL default ' ',
  "AAAAZZCJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAMI","AAAAZZHA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZCO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAAAZZHA" varchar(4) NOT NULL default ' ',
  "AAABLPWW" varchar(10) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAAMI","AAAAZZHA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZCZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBACP" varchar(1) NOT NULL default ' ',
  "AAABMJWN" varchar(30) NOT NULL default ' ',
  "AAABMKAI" varchar(3) NOT NULL default ' ',
  "AAAAZZDH" varchar(2) NOT NULL default '00',
  "AAAAZZDI" varchar(2) NOT NULL default '00',
  "AAAAZZDJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZDN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEPVW" varchar(4) NOT NULL default ' ',
  "AAABLPWW" varchar(10) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEPVW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZDW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBACP" varchar(1) NOT NULL default ' ',
  "AAABMLCO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBACP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZEC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBACP" varchar(1) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  "AAABLPWW" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBACP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZEL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAAAZZHA" varchar(4) NOT NULL default ' ',
  "AAABEPVW" varchar(4) NOT NULL default ' ',
  "AAAAZZFR" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAXTI" decimal(15,3) NOT NULL default '0.000',
  "AAABAXTJ" decimal(15,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABAAMI","AAAAZZHA","AAABEPVW","AAAAZZFR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZFB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAAAZZHA" varchar(4) NOT NULL default ' ',
  "AAABEPVW" varchar(4) NOT NULL default ' ',
  "AAAAZZFR" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLPWW" varchar(10) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAAMI","AAAAZZHA","AAABEPVW","AAAAZZFR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZFT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFIP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFIP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZFY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJFIP" varchar(1) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABJFIP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZGG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAAAZZHA" varchar(4) NOT NULL default ' ',
  "AAAAZZHB" varchar(2) NOT NULL default '00',
  "AAAAZZHC" varchar(2) NOT NULL default '00',
  "AAAAZZHD" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABFJUF" decimal(7,2) NOT NULL default '0.00',
  "AAAAZZGQ" decimal(7,2) NOT NULL default '0.00',
  "AAAAZZGR" decimal(7,2) NOT NULL default '0.00',
  "AAAAZZGS" decimal(13,2) NOT NULL default '0.00',
  "AAABCPBD" decimal(13,2) NOT NULL default '0.00',
  "AAAAZZGU" decimal(13,2) NOT NULL default '0.00',
  "AAAAZZGV" decimal(15,5) NOT NULL default '0.00000',
  "AAAAZZGW" decimal(15,5) NOT NULL default '0.00000',
  "AAAAZZGX" decimal(15,5) NOT NULL default '0.00000',
  PRIMARY KEY ("AAABMRIL","AAABAAMI","AAAAZZHA","AAAAZZHB","AAAAZZHC","AAAAZZHD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZHF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATV" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(60) NOT NULL default ' ',
  "AAABLHAH" varchar(255) NOT NULL default ' ',
  "AAAAZZHK" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABCATV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZHN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATV" varchar(4) NOT NULL default ' ',
  "AAAAZZMZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCATV","AAAAZZMZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZHU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCATV" varchar(4) NOT NULL default ' ',
  "AAAAZZMZ" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABCATV","AAAAZZMZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZIE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATV" varchar(4) NOT NULL default ' ',
  "AAAAZZNU" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCATV","AAAAZZNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZIL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCATV" varchar(4) NOT NULL default ' ',
  "AAAAZZNU" varchar(8) NOT NULL default ' ',
  "AAABMOQQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABCATV","AAAAZZNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZIV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATV" varchar(4) NOT NULL default ' ',
  "AAAAZZJR" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCATV","AAAAZZJR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZJC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCATV" varchar(4) NOT NULL default ' ',
  "AAAAZZJR" varchar(8) NOT NULL default ' ',
  "AAABMOQQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABCATV","AAAAZZJR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZJM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATV" varchar(4) NOT NULL default ' ',
  "AAAAZZNU" varchar(8) NOT NULL default ' ',
  "AAAAZZNV" varchar(2) NOT NULL default ' ',
  "AAAAZZJR" varchar(8) NOT NULL default ' ',
  "AAAAZZJS" varchar(8) NOT NULL default ' ',
  "AAAAZZJT" varchar(8) NOT NULL default ' ',
  "AAAAZZJU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCATV","AAAAZZNU","AAAAZZNV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZJZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATV" varchar(4) NOT NULL default ' ',
  "AAAAZZMZ" varchar(4) NOT NULL default ' ',
  "AAAAZZNA" varchar(8) NOT NULL default '00000000',
  "AAAAZZNB" varchar(8) NOT NULL default '00000000',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAAAZZKG" decimal(5,2) NOT NULL default '0.00',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABLEJW" varchar(60) NOT NULL default ' ',
  "AAAAZZKK" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABCATV","AAAAZZMZ","AAAAZZNA","AAAAZZNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZKQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATV" varchar(4) NOT NULL default ' ',
  "AAAAZZMZ" varchar(4) NOT NULL default ' ',
  "AAAAZZNA" varchar(8) NOT NULL default '00000000',
  "AAAAZZNB" varchar(8) NOT NULL default '00000000',
  "AAAAZZNU" varchar(8) NOT NULL default ' ',
  "AAAAZZNV" varchar(2) NOT NULL default ' ',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAAAZZVO" varchar(2) NOT NULL default '00',
  "AAAAZZLA" varchar(1) NOT NULL default ' ',
  "AAAAZZLB" varchar(1) NOT NULL default ' ',
  "AAAAZZNW" decimal(5,2) NOT NULL default '0.00',
  "AAAAZZVP" decimal(15,2) NOT NULL default '0.00',
  "AAAAZZVQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAZZVR" decimal(15,2) NOT NULL default '0.00',
  "AAAAZZLG" varchar(1) NOT NULL default ' ',
  "AAAAZZLH" decimal(5,2) NOT NULL default '0.00',
  "AAAAZZLI" decimal(5,2) NOT NULL default '0.00',
  "AAAAZZLJ" decimal(5,2) NOT NULL default '0.00',
  "AAAAZZLK" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABCATV","AAAAZZMZ","AAAAZZNA","AAAAZZNB","AAAAZZNU","AAAAZZNV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZLS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATV" varchar(4) NOT NULL default ' ',
  "AAAAZZMZ" varchar(4) NOT NULL default ' ',
  "AAAAZZNA" varchar(8) NOT NULL default '00000000',
  "AAAAZZNB" varchar(8) NOT NULL default '00000000',
  "AAAAZZNU" varchar(8) NOT NULL default ' ',
  "AAAAZZNV" varchar(2) NOT NULL default ' ',
  "AAABLEHK" varchar(2) NOT NULL default ' ',
  "AAABJZWV" varchar(4) NOT NULL default ' ',
  "AAABIYJJ" varchar(4) NOT NULL default ' ',
  "AAAAZZRH" varchar(4) NOT NULL default ' ',
  "AAAAZZSG" varchar(4) NOT NULL default ' ',
  "AAAAZZTF" varchar(4) NOT NULL default ' ',
  "AAABLGEF" varchar(4) NOT NULL default ' ',
  "AAAAZZXW" varchar(2) NOT NULL default ' ',
  "AAAAZZMI" decimal(7,0) NOT NULL default '0',
  "AAAAZZMJ" decimal(7,0) NOT NULL default '0',
  "AAAAZZMK" decimal(7,0) NOT NULL default '0',
  "AAABBLDD" decimal(15,2) NOT NULL default '0.00',
  "AAAAZZMM" decimal(15,2) NOT NULL default '0.00',
  "AAAAZZMN" decimal(15,2) NOT NULL default '0.00',
  "AAAAZZMO" decimal(5,2) NOT NULL default '0.00',
  "AAAAZZMP" decimal(5,2) NOT NULL default '0.00',
  "AAAAZZMQ" decimal(5,2) NOT NULL default '0.00',
  "AAAAZZMR" decimal(15,2) NOT NULL default '0.00',
  "AAAAZZMS" decimal(15,2) NOT NULL default '0.00',
  "AAAAZZMT" decimal(15,2) NOT NULL default '0.00',
  "AAAAZZMU" decimal(15,2) NOT NULL default '0.00',
  "AAAAZZMV" decimal(15,2) NOT NULL default '0.00',
  "AAAAZZMW" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABCATV","AAAAZZMZ","AAAAZZNA","AAAAZZNB","AAAAZZNU","AAAAZZNV","AAABLEHK","AAABJZWV","AAABIYJJ","AAAAZZRH","AAAAZZSG","AAAAZZTF","AAABLGEF","AAAAZZXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZNM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABCATV" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZZNU" varchar(8) NOT NULL default ' ',
  "AAAAZZNV" varchar(2) NOT NULL default ' ',
  "AAAAZZNW" decimal(5,2) NOT NULL default '0.00',
  "AAAAZZNX" decimal(4,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABJFHJ","AAABLHPM","AAABLUNJ","AAABCATV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZOE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATV" varchar(4) NOT NULL default ' ',
  "AAABLEHK" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCATV","AAABLEHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZOL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATV" varchar(4) NOT NULL default ' ',
  "AAAAZZOS" varchar(60) NOT NULL default ' ',
  "AAABJZWV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCATV","AAAAZZOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZOT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATV" varchar(4) NOT NULL default ' ',
  "AAABJZWV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCATV","AAABJZWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZPA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCATV" varchar(4) NOT NULL default ' ',
  "AAABJZWV" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABCATV","AAABJZWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZPK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATV" varchar(4) NOT NULL default ' ',
  "AAAAZZPR" varchar(60) NOT NULL default ' ',
  "AAABIYJJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCATV","AAAAZZPR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZPS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATV" varchar(4) NOT NULL default ' ',
  "AAABIYJJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCATV","AAABIYJJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZPZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCATV" varchar(4) NOT NULL default ' ',
  "AAABIYJJ" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABCATV","AAABIYJJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZQJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATV" varchar(4) NOT NULL default ' ',
  "AAAAZZQQ" varchar(60) NOT NULL default ' ',
  "AAAAZZRH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCATV","AAAAZZQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZQR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATV" varchar(4) NOT NULL default ' ',
  "AAAAZZRH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCATV","AAAAZZRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCATV" varchar(4) NOT NULL default ' ',
  "AAAAZZRH" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABCATV","AAAAZZRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZRI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATV" varchar(4) NOT NULL default ' ',
  "AAAAZZRP" varchar(60) NOT NULL default ' ',
  "AAAAZZSG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCATV","AAAAZZRP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZRQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATV" varchar(4) NOT NULL default ' ',
  "AAAAZZSG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCATV","AAAAZZSG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZRX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCATV" varchar(4) NOT NULL default ' ',
  "AAAAZZSG" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABCATV","AAAAZZSG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZSH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATV" varchar(4) NOT NULL default ' ',
  "AAAAZZSO" varchar(60) NOT NULL default ' ',
  "AAAAZZTF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCATV","AAAAZZSO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZSP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATV" varchar(4) NOT NULL default ' ',
  "AAAAZZTF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCATV","AAAAZZTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZSW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCATV" varchar(4) NOT NULL default ' ',
  "AAAAZZTF" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABCATV","AAAAZZTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZTG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATV" varchar(4) NOT NULL default ' ',
  "AAAAZZTN" varchar(60) NOT NULL default ' ',
  "AAABLGEF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCATV","AAAAZZTN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZTO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATV" varchar(4) NOT NULL default ' ',
  "AAABLGEF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCATV","AAABLGEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCATV" varchar(4) NOT NULL default ' ',
  "AAABLGEF" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABCATV","AAABLGEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZUF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZZXW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZZXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZUK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAZZXW" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAZZXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZUS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATV" varchar(4) NOT NULL default ' ',
  "AAABCZFZ" varchar(2) NOT NULL default ' ',
  "AAAAZZVD" varchar(3) NOT NULL default '000',
  "AAAAZZXW" varchar(2) NOT NULL default ' ',
  "AAAAZZUY" varchar(30) NOT NULL default ' ',
  "AAAAZZUZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCATV","AAABCZFZ","AAAAZZVD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZVE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZZXA" varchar(4) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABJATT" varchar(2) NOT NULL default ' ',
  "AAABJQRH" varchar(2) NOT NULL default ' ',
  "AAAAZZVX" varchar(8) NOT NULL default ' ',
  "AAAAZZVY" varchar(2) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAAAZZVO" varchar(2) NOT NULL default '00',
  "AAAAZZVP" decimal(15,2) NOT NULL default '0.00',
  "AAAAZZVQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAZZVR" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAZZXA","AAABHVYP","AAABJATT","AAABJQRH","AAAAZZVX","AAAAZZVY","AAABKFCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZWA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZZWN" varchar(4) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZZWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZWG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAZZWN" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAZZWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZWO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZZXA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZZXA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZWT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAZZXA" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAZZXA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZXB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZZXN" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZZXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZXG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAZZXN" varchar(8) NOT NULL default ' ',
  "AAABMOQQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAZZXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZXO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZZXW" varchar(2) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAZZXS" varchar(4) NOT NULL default ' ',
  "AAAAZZXT" varchar(2) NOT NULL default ' ',
  "AAAAZZXU" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZZXW","AAABHVYP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZXY" (
  "AAABFLGW" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABFLGW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZYB" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFLGW" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABFLGW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZYH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFLGW" varchar(3) NOT NULL default '000',
  "AAAAZZYK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFLGW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZYN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAANO" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABIJTI" varchar(8) NOT NULL default '00000000',
  "AAABFLGW" varchar(3) NOT NULL default '000',
  "AAABAAMD" varchar(2) NOT NULL default '00',
  "AAABBGOP" varchar(3) NOT NULL default ' ',
  "AAABAAMA" varchar(4) NOT NULL default ' ',
  "AAAAZZYW" varchar(8) NOT NULL default ' ',
  "AAAAZZYX" varchar(1) NOT NULL default ' ',
  "AAABLMKH" varchar(8) NOT NULL default '00000000',
  "AAAAZZYZ" varchar(2) NOT NULL default '00',
  "AAAAZZZA" varchar(3) NOT NULL default ' ',
  "AAAAZZZB" varchar(2) NOT NULL default '00',
  "AAAAZZZC" varchar(3) NOT NULL default ' ',
  "AAAAZZZD" varchar(2) NOT NULL default '00',
  "AAAAZZZE" varchar(3) NOT NULL default ' ',
  "AAABAAHJ" varchar(13) NOT NULL default ' ',
  "AAABAAGA" varchar(10) NOT NULL default ' ',
  "AAABLMLH" varchar(2) NOT NULL default ' ',
  "AAAAZZZI" decimal(15,5) NOT NULL default '0.00000',
  "AAAAZZZJ" varchar(1) NOT NULL default ' ',
  "AAABAAJV" varchar(8) NOT NULL default ' ',
  "AAABEJQX" varchar(1) NOT NULL default ' ',
  "AAAAZZZM" varchar(4) NOT NULL default '0000',
  "AAAAZZZN" varchar(4) NOT NULL default '0000',
  "AAAAZZZO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAANO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAZZZR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAABAANO" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABIJTI" varchar(8) NOT NULL default '00000000',
  "AAABBLIL" varchar(4) NOT NULL default ' ',
  "AAAAZZZY" varchar(8) NOT NULL default ' ',
  "AAAAZZZZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAMI","AAABAANO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAAD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAANO" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAABB" varchar(2) NOT NULL default '00',
  "AAABAAAJ" decimal(15,6) NOT NULL default '0.000000',
  "AAABAAAK" decimal(15,6) NOT NULL default '0.000000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAANO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAAP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAABB" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABAABB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAAU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAABB" varchar(2) NOT NULL default '00',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAABB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAABC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAANP" varchar(4) NOT NULL default ' ',
  "AAABAABF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAANP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAABI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAANP" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAANP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAABQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAANO" varchar(4) NOT NULL default ' ',
  "AAABAANP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAANO","AAABAANP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAABX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAJV" varchar(8) NOT NULL default ' ',
  "AAABAOJE" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAAJV","AAABAOJE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAACH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAJV" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAACM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAAJV" varchar(8) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAAJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAACU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAOJE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAOJE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAACZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAOJE" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAOJE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAADH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAADM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAAMP" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAAMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAADU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMP" varchar(4) NOT NULL default ' ',
  "AAABLMET" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAEA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAEP" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAAEF" decimal(15,5) NOT NULL default '0.00000',
  "AAABAAEN" decimal(15,5) NOT NULL default '0.00000',
  "AAABAAEH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAEK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAEP" varchar(4) NOT NULL default ' ',
  "AAABAAEN" decimal(15,5) NOT NULL default '0.00000',
  PRIMARY KEY ("AAABMRIL","AAABAAEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAANO" varchar(4) NOT NULL default ' ',
  "AAABLGNX" varchar(8) NOT NULL default '00000000',
  "AAABAAHY" varchar(3) NOT NULL default '000',
  "AAABAAHZ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABAANO","AAABLGNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAEZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAANO" varchar(4) NOT NULL default ' ',
  "AAABLGNX" varchar(8) NOT NULL default '00000000',
  "AAABAAHY" varchar(3) NOT NULL default '000',
  "AAABAAHZ" varchar(3) NOT NULL default '000',
  "AAABAAFF" varchar(1) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAANO","AAABLGNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAFK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAKV" varchar(4) NOT NULL default ' ',
  "AAABAAJV" varchar(4) NOT NULL default ' ',
  "AAABAAFO" varchar(1) NOT NULL default ' ',
  "AAABAAFP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAKV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAFS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAKV" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAAMA" varchar(4) NOT NULL default ' ',
  "AAABAAFY" decimal(4,0) NOT NULL default '0',
  "AAABAAFZ" varchar(3) NOT NULL default ' ',
  "AAABAAGA" varchar(10) NOT NULL default ' ',
  "AAABLMLH" varchar(2) NOT NULL default ' ',
  "AAABAAHT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAKV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAGG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAKV" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAAGL" decimal(13,2) NOT NULL default '0.00',
  "AAABAYCQ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAKV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAGQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAAKV" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAAGW" varchar(4) NOT NULL default ' ',
  "AAABAAGX" varchar(4) NOT NULL default ' ',
  "AAABAAGY" varchar(4) NOT NULL default ' ',
  "AAABAAGZ" varchar(4) NOT NULL default ' ',
  "AAABAAHA" varchar(1) NOT NULL default ' ',
  "AAABBBFS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAAKV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAHG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAHT" varchar(4) NOT NULL default ' ',
  "AAABAAHJ" varchar(13) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAHT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAHM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAAHT" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAAHT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAHU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAIG" varchar(4) NOT NULL default ' ',
  "AAABAAHX" varchar(4) NOT NULL default ' ',
  "AAABAAHY" decimal(3,0) NOT NULL default '0',
  "AAABAAHZ" decimal(3,0) NOT NULL default '0',
  "AAABBBDA" varchar(1) NOT NULL default ' ',
  "AAABAAIB" decimal(13,2) NOT NULL default '0.00',
  "AAABAYCQ" varchar(5) NOT NULL default ' ',
  "AAABAAID" varchar(1) NOT NULL default ' ',
  "AAABAAIE" decimal(10,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABAAIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAIH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAMA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAIM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAAMA" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAAMA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAIU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMA" varchar(4) NOT NULL default ' ',
  "AAABAALU" varchar(3) NOT NULL default '000',
  "AAABAAIY" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABAAMA","AAABAALU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAJC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAJV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAJH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAAJV" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAAJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAJP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAJV" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAAJV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAJX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAKW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAKW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAKC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAAKW" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAAKW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAKK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAKV" varchar(4) NOT NULL default ' ',
  "AAABAAKW" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAAKQ" decimal(4,0) NOT NULL default '0',
  "AAABAAKR" varchar(3) NOT NULL default ' ',
  "AAABAAKS" varchar(1) NOT NULL default ' ',
  "AAABAAKT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAKV","AAABAAKW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAKY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAMA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAALD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAAMA" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAAMA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAALL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMA" varchar(4) NOT NULL default ' ',
  "AAABAALT" decimal(15,3) NOT NULL default '0.000',
  "AAABAALU" varchar(3) NOT NULL default '000',
  "AAABKQPU" decimal(7,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABAAMA","AAABAALT","AAABAALU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAALV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAMI" varchar(2) NOT NULL default ' ',
  "AAABAANO" varchar(4) NOT NULL default ' ',
  "AAABAAMP" varchar(4) NOT NULL default ' ',
  "AAABAAMA" varchar(4) NOT NULL default ' ',
  "AAABBNKX" varchar(1) NOT NULL default ' ',
  "AAABAAMC" varchar(1) NOT NULL default ' ',
  "AAABAAMD" varchar(2) NOT NULL default '00',
  "AAABBGOP" varchar(3) NOT NULL default ' ',
  "AAABAAMF" varchar(1) NOT NULL default ' ',
  "AAABKYBY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAMI","AAABAANO","AAABAAMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAML" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABACWF" varchar(10) NOT NULL default ' ',
  "AAABACWG" varchar(32) NOT NULL default ' ',
  "AAABAAMP" varchar(4) NOT NULL default ' ',
  "AAABAAMQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABACWF","AAABACWG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAMU" (
  "AAABAANQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABAANQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAMX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAANQ" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABAANQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAND" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAANO" varchar(4) NOT NULL default ' ',
  "AAABAANP" varchar(4) NOT NULL default ' ',
  "AAABAANQ" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBGQC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAANO","AAABAANP","AAABAANQ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAANS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAARI" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAARI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAANY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGBU" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAOD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGBU" varchar(3) NOT NULL default ' ',
  "AAABAAOG" varchar(2) NOT NULL default '00',
  "AAABAAOH" varchar(1) NOT NULL default ' ',
  "AAABAAOI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAOL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJGBU" varchar(3) NOT NULL default ' ',
  "AAABAAOP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABJGBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAOT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGBU" varchar(3) NOT NULL default ' ',
  "AAABAAWF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGBU","AAABAAWF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAPA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGBU" varchar(3) NOT NULL default ' ',
  "AAABAAWF" varchar(3) NOT NULL default ' ',
  "AAABAAPE" varchar(2) NOT NULL default '00',
  "AAABAAPF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGBU","AAABAAWF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAPJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJGBU" varchar(3) NOT NULL default ' ',
  "AAABAAWF" varchar(3) NOT NULL default ' ',
  "AAABAAPO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABJGBU","AAABAAWF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAPT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAAWF" varchar(3) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAAQA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAAWF","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAQG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHA" varchar(4) NOT NULL default ' ',
  "AAABAARI" varchar(4) NOT NULL default ' ',
  "AAABAAQK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHA","AAABAARI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAQO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEZHA" varchar(4) NOT NULL default ' ',
  "AAABAARI" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEZHA","AAABAARI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHA" varchar(4) NOT NULL default ' ',
  "AAABAARI" varchar(4) NOT NULL default ' ',
  "AAABAARJ" varchar(2) NOT NULL default '00',
  "AAABLGEA" varchar(20) NOT NULL default ' ',
  "AAABAARE" varchar(50) NOT NULL default ' ',
  "AAABAARF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHA","AAABAARI","AAABAARJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAARK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGJO" varchar(2) NOT NULL default '00',
  "AAABAARN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBGJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAARQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBGJO" varchar(2) NOT NULL default '00',
  "AAABANPX" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBGJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAARY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAABBGJO" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABABPL","AAABBGJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAASF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABAATT" varchar(4) NOT NULL default ' ',
  "AAABAASR" varchar(4) NOT NULL default ' ',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABAATT","AAABAASR","AAABFMBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAST" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABAATT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABAATT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAATC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABAATT" varchar(4) NOT NULL default ' ',
  "AAABAATI" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABGYLC","AAABAATT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAATO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABABEW" varchar(2) NOT NULL default ' ',
  "AAABAATT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABABEW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAATY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAABAAXJ" varchar(8) NOT NULL default '00000000',
  "AAABAAWZ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABABPL","AAABAAXJ","AAABAAWZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAUJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAABAAXJ" varchar(8) NOT NULL default '00000000',
  "AAABAAWZ" varchar(2) NOT NULL default ' ',
  "AAABAAUX" varchar(2) NOT NULL default '00',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAABMMJT" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABABPL","AAABAAXJ","AAABAAWZ","AAABAAUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAUY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABCADZ" varchar(2) NOT NULL default ' ',
  "AAABAAVE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAVH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABAAVQ" varchar(4) NOT NULL default ' ',
  "AAABMMJT" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABAAVQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAVR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAABJGBU" varchar(3) NOT NULL default ' ',
  "AAABAAWF" varchar(3) NOT NULL default ' ',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAABABJU" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABABPL","AAABJGBU","AAABAAWF","AAABFMBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAWH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAABAAWZ" varchar(2) NOT NULL default ' ',
  "AAABAAWU" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABABPL","AAABAAWZ","AAABAAWU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAWQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAABAAWZ" varchar(2) NOT NULL default ' ',
  "AAABAAWU" varchar(2) NOT NULL default ' ',
  "AAABAAWV" varchar(2) NOT NULL default ' ',
  "AAABAAWW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABABPL","AAABAAWZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAXA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHL" varchar(14) NOT NULL default ' ',
  "AAABMOQQ" varchar(60) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABAAXJ" varchar(8) NOT NULL default '00000000',
  "AAABAAXK" varchar(1) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABABJU" varchar(2) NOT NULL default ' ',
  "AAABABEW" varchar(2) NOT NULL default ' ',
  "AAABAAXO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAXR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHL" varchar(14) NOT NULL default ' ',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAABMMJT" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHL","AAABFMBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAXZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABABJU" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABABJU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAYI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABABJU" varchar(2) NOT NULL default ' ',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABABJU","AAABFMBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAYT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABABCS" varchar(4) NOT NULL default ' ',
  "AAABABCU" varchar(4) NOT NULL default ' ',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAABABDL" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABAJAK" varchar(4) NOT NULL default ' ',
  "AAABABDC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABABCS","AAABABCU","AAABFMBR","AAABABDL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAAZO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABAAZZ" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLADO" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABAAZZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABAB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABABCS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABABCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABAK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABABCS" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABGYLC","AAABABCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABAW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABABCU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABABCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABBF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABABCU" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABGYLC","AAABABCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABBR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAJAK" varchar(4) NOT NULL default ' ',
  "AAABLXES" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAJAK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABBX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABABEG" varchar(2) NOT NULL default ' ',
  "AAABHMRN" varchar(10) NOT NULL default '0000000000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABLUFO" varchar(10) NOT NULL default '0000000000',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABLQVJ" varchar(5) NOT NULL default '00000',
  "AAABABDL" varchar(2) NOT NULL default '00',
  "AAABABCG" varchar(10) NOT NULL default '0000000000',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAABMMJT" varchar(18) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABABCK" varchar(8) NOT NULL default '00000000',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABAJAK" varchar(4) NOT NULL default ' ',
  "AAABJBPT" varchar(1) NOT NULL default ' ',
  "AAABLXES" varchar(1) NOT NULL default ' ',
  "AAABDQKV" varchar(14) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABHUEA" varchar(5) NOT NULL default ' ',
  "AAABABCS" varchar(4) NOT NULL default ' ',
  "AAABHUEB" varchar(5) NOT NULL default ' ',
  "AAABABCU" varchar(4) NOT NULL default ' ',
  "AAABHUEC" varchar(5) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABAWGV" varchar(2) NOT NULL default '00',
  "AAABBGCX" varchar(4) NOT NULL default '0000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABABDC" varchar(1) NOT NULL default ' ',
  "AAABABDD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABABEG","AAABHMRN","AAABMCRV","AAABLUFO","AAABMQBT","AAABLQVJ","AAABABDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABDM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABABEG" varchar(2) NOT NULL default ' ',
  "AAABHMRN" varchar(10) NOT NULL default '0000000000',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABABDW" varchar(8) NOT NULL default '00000000',
  "AAABABDX" varchar(8) NOT NULL default '00000000',
  "AAABABDY" varchar(1) NOT NULL default ' ',
  "AAABABDZ" varchar(8) NOT NULL default '00000000',
  "AAABABEA" varchar(8) NOT NULL default '00000000',
  "AAABLUZZ" varchar(1) NOT NULL default ' ',
  "AAABABEC" varchar(10) NOT NULL default '0000000000',
  "AAABABED" varchar(4) NOT NULL default ' ',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABABEG","AAABHMRN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABEI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABABER" varchar(30) NOT NULL default ' ',
  "AAABABES" varchar(30) NOT NULL default ' ',
  "AAABBGJO" varchar(2) NOT NULL default '00',
  "AAABABFV" varchar(1) NOT NULL default ' ',
  "AAABABEV" varchar(1) NOT NULL default ' ',
  "AAABABEW" varchar(2) NOT NULL default ' ',
  "AAABABEX" varchar(4) NOT NULL default ' ',
  "AAABABEY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABFMBR","AAABABPL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABFF" (
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABABFV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHVYP","AAABGYLC","AAABABFV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABFM" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABABFV" varchar(1) NOT NULL default ' ',
  "AAABMOQQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABHVYP","AAABGYLC","AAABABFV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABFW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMJWN" varchar(10) NOT NULL default ' ',
  "AAABMMJT" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABGC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABMJWN" varchar(10) NOT NULL default ' ',
  "AAABMOQQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABKKSG" varchar(4) NOT NULL default ' ',
  "AAABLADO" varchar(5) NOT NULL default ' ',
  "AAABABGQ" varchar(30) NOT NULL default ' ',
  "AAABABGR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABKKSG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABGW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABKKSG" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABGYLC","AAABKKSG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABHI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABKKSG" varchar(4) NOT NULL default ' ',
  "AAABLFJP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABKKSG","AAABLFJP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABHT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABKKSG" varchar(4) NOT NULL default ' ',
  "AAABLFJP" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABGYLC","AAABKKSG","AAABLFJP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABIH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABKKSG" varchar(4) NOT NULL default ' ',
  "AAABABKA" varchar(4) NOT NULL default ' ',
  "AAABABIN" varchar(30) NOT NULL default ' ',
  "AAABDTRC" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABABIQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABKKSG","AAABABKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABKKSG" varchar(4) NOT NULL default ' ',
  "AAABABKA" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABGYLC","AAABKKSG","AAABABKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABJK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABKKSG" varchar(4) NOT NULL default ' ',
  "AAABLFJP" varchar(2) NOT NULL default ' ',
  "AAABABKA" varchar(4) NOT NULL default ' ',
  "AAABCTZC" varchar(1) NOT NULL default ' ',
  "AAABLQVJ" varchar(2) NOT NULL default '00',
  "AAABABJT" varchar(1) NOT NULL default ' ',
  "AAABABJU" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABKKSG","AAABLFJP","AAABABKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABKB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABKKSG" varchar(4) NOT NULL default ' ',
  "AAABLFJP" varchar(2) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABBFVS" varchar(2) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABABPE" varchar(1) NOT NULL default ' ',
  "AAABABKM" varchar(1) NOT NULL default ' ',
  "AAABLQVJ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABKKSG","AAABLFJP","AAABJFGL","AAABJFHB","AAABBFVS","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABKX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABKKSG" varchar(4) NOT NULL default ' ',
  "AAABLFJP" varchar(2) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABBFVS" varchar(2) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABLWJT" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABGYLC","AAABKKSG","AAABLFJP","AAABJFGL","AAABJFHB","AAABBFVS","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABLT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABKKSG" varchar(4) NOT NULL default ' ',
  "AAABLFJP" varchar(2) NOT NULL default ' ',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAABABNO" varchar(1) NOT NULL default ' ',
  "AAABABNP" varchar(1) NOT NULL default ' ',
  "AAABABPE" varchar(1) NOT NULL default ' ',
  "AAABABPD" varchar(1) NOT NULL default ' ',
  "AAABLQVJ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABKKSG","AAABLFJP","AAABABPL","AAABFMBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABMN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABKKSG" varchar(4) NOT NULL default ' ',
  "AAABLFJP" varchar(2) NOT NULL default ' ',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAABABNE" varchar(4) NOT NULL default ' ',
  "AAABABNF" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABKKSG","AAABLFJP","AAABABPL","AAABFMBR","AAABABNE","AAABABNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABNG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABKKSG" varchar(4) NOT NULL default ' ',
  "AAABLFJP" varchar(2) NOT NULL default ' ',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABABNO" varchar(1) NOT NULL default ' ',
  "AAABABNP" varchar(1) NOT NULL default ' ',
  "AAABABPE" varchar(1) NOT NULL default ' ',
  "AAABLQVJ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABKKSG","AAABLFJP","AAABABPL","AAABHVYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABNZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABKKSG" varchar(4) NOT NULL default ' ',
  "AAABLFJP" varchar(2) NOT NULL default ' ',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABABOR" varchar(4) NOT NULL default ' ',
  "AAABABOS" varchar(2) NOT NULL default '00',
  "AAABABOT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABKKSG","AAABLFJP","AAABABPL","AAABHVYL","AAABABOR","AAABABOS","AAABABOT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABKKSG" varchar(4) NOT NULL default ' ',
  "AAABLFJP" varchar(2) NOT NULL default ' ',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAABLGYD" varchar(1) NOT NULL default ' ',
  "AAABABPD" varchar(1) NOT NULL default ' ',
  "AAABABPE" varchar(1) NOT NULL default ' ',
  "AAABLQVJ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABKKSG","AAABLFJP","AAABABPL","AAABFMBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABPN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABKKSG" varchar(4) NOT NULL default ' ',
  "AAABABPS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABKKSG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABPX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABABQX" varchar(4) NOT NULL default ' ',
  "AAABABQY" varchar(2) NOT NULL default ' ',
  "AAABLQAJ" varchar(4) NOT NULL default ' ',
  "AAABABQC" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABABQX","AAABABQY","AAABLQAJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABQH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABABQX" varchar(4) NOT NULL default ' ',
  "AAABABQY" varchar(2) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABBFVS" varchar(2) NOT NULL default ' ',
  "AAABABQO" varchar(40) NOT NULL default ' ',
  "AAABABQP" varchar(40) NOT NULL default ' ',
  "AAABABQQ" varchar(1) NOT NULL default ' ',
  "AAABABQR" varchar(8) NOT NULL default '00000000',
  "AAABEPWA" varchar(20) NOT NULL default ' ',
  "AAABABQT" varchar(1) NOT NULL default ' ',
  "AAABABQU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABABQX","AAABABQY","AAABJFHB","AAABBFVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABRB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABACVV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABACVV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABRI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABACPJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABACPJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABRP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABACPT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABACPT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABRW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABACQD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABACQD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABSD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABAEFF" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAEIV" varchar(4) NOT NULL default ' ',
  "AAABACRB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABAEFF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABSR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABACRB" varchar(4) NOT NULL default ' ',
  "AAABABSW" varchar(1) NOT NULL default ' ',
  "AAABABSX" varchar(1) NOT NULL default ' ',
  "AAABBHSP" varchar(1) NOT NULL default ' ',
  "AAABBHSQ" varchar(1) NOT NULL default ' ',
  "AAABBHSR" varchar(1) NOT NULL default ' ',
  "AAABBHSV" varchar(4) NOT NULL default ' ',
  "AAABBHSW" varchar(2) NOT NULL default '00',
  "AAABBHSX" varchar(2) NOT NULL default '00',
  "AAABBHSY" varchar(2) NOT NULL default '00',
  "AAABBHSZ" varchar(2) NOT NULL default '00',
  "AAABBHTA" varchar(2) NOT NULL default '00',
  "AAABBHTB" varchar(2) NOT NULL default '00',
  "AAABBHTC" varchar(2) NOT NULL default ' ',
  "AAABBHTD" varchar(2) NOT NULL default ' ',
  "AAABBHTF" varchar(2) NOT NULL default '00',
  "AAABBHTG" varchar(2) NOT NULL default '00',
  "AAABBHTE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABACRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABTR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABACRB" varchar(4) NOT NULL default ' ',
  "AAABBHVT" varchar(4) NOT NULL default ' ',
  "AAABBHVU" varchar(4) NOT NULL default ' ',
  "AAABBHVV" varchar(4) NOT NULL default ' ',
  "AAABACPJ" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABABUC" decimal(15,2) NOT NULL default '0.00',
  "AAABABUD" decimal(8,5) NOT NULL default '0.00000',
  "AAABABUE" varchar(1) NOT NULL default ' ',
  "AAABABUF" decimal(13,2) NOT NULL default '0.00',
  "AAABABUG" decimal(15,2) NOT NULL default '0.00',
  "AAABABUH" decimal(8,5) NOT NULL default '0.00000',
  "AAABABUI" varchar(1) NOT NULL default ' ',
  "AAABABUJ" decimal(13,2) NOT NULL default '0.00',
  "AAABABUK" varchar(1) NOT NULL default ' ',
  "AAABADPG" decimal(15,2) NOT NULL default '0.00',
  "AAABABUM" decimal(8,5) NOT NULL default '0.00000',
  "AAABABUN" varchar(1) NOT NULL default ' ',
  "AAABABUO" decimal(13,2) NOT NULL default '0.00',
  "AAABABUP" varchar(1) NOT NULL default ' ',
  "AAABABUQ" decimal(13,2) NOT NULL default '0.00',
  "AAABABUR" decimal(9,0) NOT NULL default '0',
  "AAABABUS" decimal(9,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABACRB","AAABBHVT","AAABBHVU","AAABBHVV","AAABACPJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABVC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABACEO" varchar(4) NOT NULL default ' ',
  "AAABACER" varchar(4) NOT NULL default ' ',
  "AAABBABG" varchar(4) NOT NULL default ' ',
  "AAABARON" varchar(4) NOT NULL default ' ',
  "AAABABVM" varchar(1) NOT NULL default ' ',
  "AAABACWP" decimal(13,2) NOT NULL default '0.00',
  "AAABABVO" varchar(4) NOT NULL default ' ',
  "AAABABVP" varchar(4) NOT NULL default ' ',
  "AAABABVQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABVV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABBABG" varchar(4) NOT NULL default ' ',
  "AAABKPXV" varchar(2) NOT NULL default '00',
  "AAABABWB" varchar(1) NOT NULL default ' ',
  "AAABBHSP" varchar(1) NOT NULL default ' ',
  "AAABBHSQ" varchar(1) NOT NULL default ' ',
  "AAABBHSR" varchar(1) NOT NULL default ' ',
  "AAABBHSV" varchar(4) NOT NULL default ' ',
  "AAABBHSW" varchar(2) NOT NULL default '00',
  "AAABBHSX" varchar(2) NOT NULL default '00',
  "AAABBHSY" varchar(2) NOT NULL default '00',
  "AAABBHSZ" varchar(2) NOT NULL default '00',
  "AAABBHTA" varchar(2) NOT NULL default '00',
  "AAABBHTB" varchar(2) NOT NULL default '00',
  "AAABBHTC" varchar(2) NOT NULL default ' ',
  "AAABBHTD" varchar(2) NOT NULL default ' ',
  "AAABBHTF" varchar(2) NOT NULL default '00',
  "AAABBHTG" varchar(2) NOT NULL default '00',
  "AAABABZE" decimal(4,0) NOT NULL default '0',
  "AAABABZF" varchar(3) NOT NULL default ' ',
  "AAABABZG" varchar(3) NOT NULL default '000',
  "AAABABZH" varchar(1) NOT NULL default ' ',
  "AAABABZI" varchar(2) NOT NULL default ' ',
  "AAABBHTE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABBABG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABXA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABBABG" varchar(4) NOT NULL default ' ',
  "AAABBHVT" varchar(4) NOT NULL default ' ',
  "AAABBHVU" varchar(4) NOT NULL default ' ',
  "AAABBHVV" varchar(4) NOT NULL default ' ',
  "AAABACPT" varchar(4) NOT NULL default ' ',
  "AAABLUVE" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABABXM" varchar(1) NOT NULL default ' ',
  "AAABABXN" decimal(13,2) NOT NULL default '0.00',
  "AAABABXO" varchar(1) NOT NULL default ' ',
  "AAABABXP" decimal(5,2) NOT NULL default '0.00',
  "AAABABXQ" varchar(1) NOT NULL default ' ',
  "AAABABXR" varchar(1) NOT NULL default ' ',
  "AAABABXS" decimal(13,2) NOT NULL default '0.00',
  "AAABABXT" varchar(1) NOT NULL default ' ',
  "AAABABXU" decimal(5,2) NOT NULL default '0.00',
  "AAABABXV" varchar(1) NOT NULL default ' ',
  "AAABABXW" varchar(1) NOT NULL default ' ',
  "AAABABXX" decimal(13,2) NOT NULL default '0.00',
  "AAABABXY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABBABG","AAABBHVT","AAABBHVU","AAABBHVV","AAABACPT","AAABLUVE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABYJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABARON" varchar(4) NOT NULL default ' ',
  "AAABKPXV" varchar(2) NOT NULL default '00',
  "AAABABYP" varchar(1) NOT NULL default ' ',
  "AAABBHSP" varchar(1) NOT NULL default ' ',
  "AAABBHSQ" varchar(1) NOT NULL default ' ',
  "AAABBHSR" varchar(1) NOT NULL default ' ',
  "AAABBHSV" varchar(4) NOT NULL default ' ',
  "AAABBHSW" varchar(2) NOT NULL default '00',
  "AAABBHSX" varchar(2) NOT NULL default '00',
  "AAABBHSY" varchar(2) NOT NULL default '00',
  "AAABBHSZ" varchar(2) NOT NULL default '00',
  "AAABBHTA" varchar(2) NOT NULL default '00',
  "AAABBHTB" varchar(2) NOT NULL default '00',
  "AAABBHTC" varchar(2) NOT NULL default ' ',
  "AAABBHTD" varchar(2) NOT NULL default ' ',
  "AAABBHTF" varchar(2) NOT NULL default '00',
  "AAABBHTG" varchar(2) NOT NULL default '00',
  "AAABABZE" decimal(4,0) NOT NULL default '0',
  "AAABABZF" varchar(3) NOT NULL default ' ',
  "AAABABZG" varchar(3) NOT NULL default '000',
  "AAABABZH" varchar(1) NOT NULL default ' ',
  "AAABABZI" varchar(2) NOT NULL default ' ',
  "AAABBHTE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABARON")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABABZO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABARON" varchar(4) NOT NULL default ' ',
  "AAABBHVT" varchar(4) NOT NULL default ' ',
  "AAABBHVU" varchar(4) NOT NULL default ' ',
  "AAABBHVV" varchar(4) NOT NULL default ' ',
  "AAABACQD" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABABZZ" varchar(1) NOT NULL default ' ',
  "AAABACAA" varchar(1) NOT NULL default ' ',
  "AAABACAB" varchar(1) NOT NULL default ' ',
  "AAABACAC" varchar(1) NOT NULL default ' ',
  "AAABACAD" varchar(1) NOT NULL default ' ',
  "AAABACAE" varchar(1) NOT NULL default ' ',
  "AAABACAF" varchar(1) NOT NULL default ' ',
  "AAABACAG" decimal(13,2) NOT NULL default '0.00',
  "AAABACAH" decimal(13,2) NOT NULL default '0.00',
  "AAABACAI" decimal(13,2) NOT NULL default '0.00',
  "AAABACAJ" decimal(13,2) NOT NULL default '0.00',
  "AAABACAK" decimal(13,2) NOT NULL default '0.00',
  "AAABACAL" decimal(13,2) NOT NULL default '0.00',
  "AAABACAM" decimal(13,2) NOT NULL default '0.00',
  "AAABACAN" decimal(13,2) NOT NULL default '0.00',
  "AAABACAO" decimal(5,2) NOT NULL default '0.00',
  "AAABACAP" decimal(5,2) NOT NULL default '0.00',
  "AAABACAQ" decimal(5,2) NOT NULL default '0.00',
  "AAABACAR" decimal(5,2) NOT NULL default '0.00',
  "AAABACAS" decimal(5,2) NOT NULL default '0.00',
  "AAABACAT" decimal(5,2) NOT NULL default '0.00',
  "AAABACAU" decimal(5,2) NOT NULL default '0.00',
  "AAABACAV" decimal(5,2) NOT NULL default '0.00',
  "AAABACAW" decimal(9,0) NOT NULL default '0',
  "AAABACAX" decimal(9,0) NOT NULL default '0',
  "AAABACAY" decimal(9,0) NOT NULL default '0',
  "AAABACAZ" decimal(9,0) NOT NULL default '0',
  "AAABACBA" decimal(9,0) NOT NULL default '0',
  "AAABACBB" decimal(9,0) NOT NULL default '0',
  "AAABACBC" decimal(9,0) NOT NULL default '0',
  "AAABACBD" decimal(9,0) NOT NULL default '0',
  "AAABKWRL" decimal(13,2) NOT NULL default '0.00',
  "AAABACBF" decimal(13,2) NOT NULL default '0.00',
  "AAABACBG" decimal(13,2) NOT NULL default '0.00',
  "AAABACBH" varchar(1) NOT NULL default ' ',
  "AAABACBI" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABARON","AAABBHVT","AAABBHVU","AAABBHVV","AAABACQD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACBS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABACSL" varchar(4) NOT NULL default ' ',
  "AAABACBZ" decimal(3,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACCE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABACSL" varchar(4) NOT NULL default ' ',
  "AAABKPXV" varchar(2) NOT NULL default '00',
  "AAABACCK" varchar(1) NOT NULL default ' ',
  "AAABBHSP" varchar(1) NOT NULL default ' ',
  "AAABBHSQ" varchar(1) NOT NULL default ' ',
  "AAABBHSR" varchar(1) NOT NULL default ' ',
  "AAABBHSV" varchar(4) NOT NULL default ' ',
  "AAABBHSW" varchar(2) NOT NULL default '00',
  "AAABBHSX" varchar(2) NOT NULL default '00',
  "AAABBHSY" varchar(2) NOT NULL default '00',
  "AAABBHSZ" varchar(2) NOT NULL default '00',
  "AAABBHTA" varchar(2) NOT NULL default '00',
  "AAABBHTB" varchar(2) NOT NULL default '00',
  "AAABBHTC" varchar(2) NOT NULL default ' ',
  "AAABBHTD" varchar(2) NOT NULL default ' ',
  "AAABBHTF" varchar(2) NOT NULL default '00',
  "AAABBHTG" varchar(2) NOT NULL default '00',
  "AAABBHTE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABACSL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACDE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABACSL" varchar(4) NOT NULL default ' ',
  "AAABBHVT" varchar(4) NOT NULL default ' ',
  "AAABBHVU" varchar(4) NOT NULL default ' ',
  "AAABBHVV" varchar(4) NOT NULL default ' ',
  "AAABACEC" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABACDP" decimal(13,2) NOT NULL default '0.00',
  "AAABACDQ" decimal(7,4) NOT NULL default '0.0000',
  "AAABACDR" varchar(1) NOT NULL default ' ',
  "AAABACDS" decimal(13,2) NOT NULL default '0.00',
  "AAABACDT" decimal(13,2) NOT NULL default '0.00',
  "AAABACDU" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABACSL","AAABBHVT","AAABBHVU","AAABBHVV","AAABACEC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACEE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABBHWL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAEIV" varchar(4) NOT NULL default ' ',
  "AAABACSL" varchar(4) NOT NULL default ' ',
  "AAABACRB" varchar(4) NOT NULL default ' ',
  "AAABACEO" varchar(4) NOT NULL default ' ',
  "AAABARON" varchar(4) NOT NULL default ' ',
  "AAABBABG" varchar(4) NOT NULL default ' ',
  "AAABACER" varchar(4) NOT NULL default ' ',
  "AAABACWP" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABBHWL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACEY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBHPT" varchar(4) NOT NULL default ' ',
  "AAABMCNJ" varchar(4) NOT NULL default ' ',
  "AAABLQAJ" varchar(4) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABACIU" decimal(5,2) NOT NULL default '0.00',
  "AAABBICD" decimal(13,2) NOT NULL default '0.00',
  "AAABACIX" decimal(5,2) NOT NULL default '0.00',
  "AAABAZHC" decimal(13,2) NOT NULL default '0.00',
  "AAABACNQ" varchar(1) NOT NULL default ' ',
  "AAABAYCQ" varchar(5) NOT NULL default ' ',
  "AAABACIP" varchar(1) NOT NULL default ' ',
  "AAABACIV" decimal(9,0) NOT NULL default '0',
  "AAABACIY" decimal(9,0) NOT NULL default '0',
  "AAABACJA" decimal(13,2) NOT NULL default '0.00',
  "AAABACJB" decimal(5,2) NOT NULL default '0.00',
  "AAABACJC" decimal(9,0) NOT NULL default '0',
  "AAABACJD" decimal(13,2) NOT NULL default '0.00',
  "AAABACJE" decimal(5,2) NOT NULL default '0.00',
  "AAABACJF" decimal(9,0) NOT NULL default '0',
  "AAABACJG" varchar(1) NOT NULL default ' ',
  "AAABKPXV" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBHPT","AAABMCNJ","AAABLQAJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACGB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBHPT" varchar(4) NOT NULL default ' ',
  "AAABMCNJ" varchar(4) NOT NULL default ' ',
  "AAABLQAJ" varchar(4) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABACGI" decimal(13,2) NOT NULL default '0.00',
  "AAABAYCQ" varchar(5) NOT NULL default ' ',
  "AAABACIP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBHPT","AAABMCNJ","AAABLQAJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACGQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBHPT" varchar(4) NOT NULL default ' ',
  "AAABMCNJ" varchar(4) NOT NULL default ' ',
  "AAABLQAJ" varchar(4) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABACIP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBHPT","AAABMCNJ","AAABLQAJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACHD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBHPT" varchar(4) NOT NULL default ' ',
  "AAABMCNJ" varchar(4) NOT NULL default ' ',
  "AAABLQAJ" varchar(4) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABBHWL" varchar(4) NOT NULL default ' ',
  "AAABACHL" decimal(9,0) NOT NULL default '0',
  "AAABACIU" decimal(5,2) NOT NULL default '0.00',
  "AAABBICD" decimal(13,2) NOT NULL default '0.00',
  "AAABACIX" decimal(5,2) NOT NULL default '0.00',
  "AAABAZHC" decimal(13,2) NOT NULL default '0.00',
  "AAABACNQ" varchar(1) NOT NULL default ' ',
  "AAABAYCQ" varchar(5) NOT NULL default ' ',
  "AAABACIP" varchar(1) NOT NULL default ' ',
  "AAABACIV" decimal(9,0) NOT NULL default '0',
  "AAABACIY" decimal(9,0) NOT NULL default '0',
  "AAABACJA" decimal(13,2) NOT NULL default '0.00',
  "AAABACJB" decimal(5,2) NOT NULL default '0.00',
  "AAABACJC" decimal(9,0) NOT NULL default '0',
  "AAABACJD" decimal(13,2) NOT NULL default '0.00',
  "AAABACJE" decimal(5,2) NOT NULL default '0.00',
  "AAABACJF" decimal(9,0) NOT NULL default '0',
  "AAABACJG" varchar(1) NOT NULL default ' ',
  "AAABKPXV" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBHPT","AAABMCNJ","AAABLQAJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACII" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBHPT" varchar(4) NOT NULL default ' ',
  "AAABMCNJ" varchar(4) NOT NULL default ' ',
  "AAABLQAJ" varchar(4) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABACIP" varchar(1) NOT NULL default ' ',
  "AAABACIQ" varchar(1) NOT NULL default ' ',
  "AAABKPXV" varchar(2) NOT NULL default '00',
  "AAABAYCQ" varchar(5) NOT NULL default ' ',
  "AAABBICD" decimal(13,2) NOT NULL default '0.00',
  "AAABACIU" decimal(5,2) NOT NULL default '0.00',
  "AAABACIV" decimal(9,0) NOT NULL default '0',
  "AAABAZHC" decimal(13,2) NOT NULL default '0.00',
  "AAABACIX" decimal(5,2) NOT NULL default '0.00',
  "AAABACIY" decimal(9,0) NOT NULL default '0',
  "AAABACNQ" varchar(1) NOT NULL default ' ',
  "AAABACJA" decimal(13,2) NOT NULL default '0.00',
  "AAABACJB" decimal(5,2) NOT NULL default '0.00',
  "AAABACJC" decimal(9,0) NOT NULL default '0',
  "AAABACJD" decimal(13,2) NOT NULL default '0.00',
  "AAABACJE" decimal(5,2) NOT NULL default '0.00',
  "AAABACJF" decimal(9,0) NOT NULL default '0',
  "AAABACJG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBHPT","AAABMCNJ","AAABLQAJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACJM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABACJS" varchar(1) NOT NULL default ' ',
  "AAABACJT" decimal(4,0) NOT NULL default '0',
  "AAABACJU" varchar(3) NOT NULL default ' ',
  "AAABACJV" varchar(1) NOT NULL default ' ',
  "AAABACJW" decimal(15,2) NOT NULL default '0.00',
  "AAABBGKB" decimal(8,5) NOT NULL default '0.00000',
  "AAABACJY" decimal(15,2) NOT NULL default '0.00',
  "AAABACJZ" decimal(8,5) NOT NULL default '0.00000',
  "AAABACKA" decimal(15,2) NOT NULL default '0.00',
  "AAABACKB" decimal(8,5) NOT NULL default '0.00000',
  "AAABBHTF" varchar(2) NOT NULL default '00',
  "AAABBHTG" varchar(2) NOT NULL default '00',
  "AAABACKE" varchar(1) NOT NULL default ' ',
  "AAABACKF" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACKK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABACKU" varchar(4) NOT NULL default ' ',
  "AAABADCP" varchar(4) NOT NULL default ' ',
  "AAABACKQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABACKU","AAABADCP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABACLH" varchar(4) NOT NULL default ' ',
  "AAABACLI" varchar(4) NOT NULL default ' ',
  "AAABACLJ" varchar(4) NOT NULL default ' ',
  "AAABACLK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABACLH","AAABACLI","AAABACLJ","AAABACLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACLL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABACTJ" varchar(4) NOT NULL default ' ',
  "AAABACLQ" decimal(15,2) NOT NULL default '0.00',
  "AAABAYCQ" varchar(5) NOT NULL default ' ',
  "AAABACLS" decimal(8,5) NOT NULL default '0.00000',
  "AAABACLT" varchar(4) NOT NULL default ' ',
  "AAABACLU" varchar(1) NOT NULL default ' ',
  "AAABACLV" decimal(13,2) NOT NULL default '0.00',
  "AAABBHTF" varchar(2) NOT NULL default '00',
  "AAABBHTG" varchar(2) NOT NULL default '00',
  "AAABACNC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABACTJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACMD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABACTJ" varchar(4) NOT NULL default ' ',
  "AAABACNV" varchar(4) NOT NULL default ' ',
  "AAABACNO" decimal(4,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABACTJ","AAABACNV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACMP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABACTV" varchar(4) NOT NULL default ' ',
  "AAABACMU" decimal(13,2) NOT NULL default '0.00',
  "AAABAYCQ" varchar(5) NOT NULL default ' ',
  "AAABACMW" decimal(8,5) NOT NULL default '0.00000',
  "AAABACMX" varchar(4) NOT NULL default ' ',
  "AAABACMY" varchar(1) NOT NULL default ' ',
  "AAABACMZ" decimal(13,2) NOT NULL default '0.00',
  "AAABBHTF" varchar(2) NOT NULL default '00',
  "AAABBHTG" varchar(2) NOT NULL default '00',
  "AAABACNC" varchar(2) NOT NULL default ' ',
  "AAABKPXV" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABACTV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACNI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABACTV" varchar(4) NOT NULL default ' ',
  "AAABACNV" varchar(4) NOT NULL default ' ',
  "AAABACNO" decimal(4,2) NOT NULL default '0.00',
  "AAABACNP" varchar(1) NOT NULL default ' ',
  "AAABACNQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABACTV","AAABACNV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACNW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABACOF" varchar(4) NOT NULL default ' ',
  "AAABACOL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABACOF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACOG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABACOP" varchar(4) NOT NULL default ' ',
  "AAABACOL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABACOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACOQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABACVV" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABACVV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACPA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABACPJ" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABACPJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACPK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABACPT" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABACPT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACPU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABACQD" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABACQD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACQE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABAEFF" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABBICN","AAABAEFF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABACRB" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABBICN","AAABACRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACRC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABBABG" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABBICN","AAABBABG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACRO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABARON" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABBICN","AAABARON")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACSA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABACSL" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABBICN","AAABACSL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACSM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABBHWL" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABBICN","AAABBHWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACSY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABACTJ" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABBICN","AAABACTJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACTK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABACTV" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABBICN","AAABACTV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACTW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABMPMH" varchar(4) NOT NULL default ' ',
  "AAABACUA" varchar(1) NOT NULL default ' ',
  "AAABACUB" decimal(4,0) NOT NULL default '0',
  "AAABACUC" varchar(3) NOT NULL default ' ',
  "AAABACUD" decimal(4,0) NOT NULL default '0',
  "AAABACUE" varchar(3) NOT NULL default ' ',
  "AAABACUF" varchar(1) NOT NULL default ' ',
  "AAABACUG" varchar(8) NOT NULL default '00000000',
  "AAABACUH" varchar(1) NOT NULL default ' ',
  "AAABACUI" varchar(8) NOT NULL default '00000000',
  "AAABACUJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACUN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABMPMH" varchar(4) NOT NULL default ' ',
  "AAABACVV" varchar(4) NOT NULL default ' ',
  "AAABLQAJ" varchar(4) NOT NULL default ' ',
  "AAABACUT" varchar(1) NOT NULL default ' ',
  "AAABACUU" varchar(1) NOT NULL default ' ',
  "AAABACUV" varchar(1) NOT NULL default ' ',
  "AAABACUW" varchar(1) NOT NULL default ' ',
  "AAABACUX" varchar(1) NOT NULL default ' ',
  "AAABACUY" varchar(1) NOT NULL default ' ',
  "AAABACUZ" varchar(1) NOT NULL default ' ',
  "AAABACVA" varchar(1) NOT NULL default ' ',
  "AAABACVB" varchar(1) NOT NULL default ' ',
  "AAABACVC" varchar(1) NOT NULL default ' ',
  "AAABACVD" varchar(1) NOT NULL default ' ',
  "AAABACVE" varchar(1) NOT NULL default ' ',
  "AAABACVF" varchar(1) NOT NULL default ' ',
  "AAABACVG" varchar(1) NOT NULL default ' ',
  "AAABACVH" varchar(1) NOT NULL default ' ',
  "AAABACVI" varchar(1) NOT NULL default ' ',
  "AAABACVJ" varchar(1) NOT NULL default ' ',
  "AAABACVK" varchar(1) NOT NULL default ' ',
  "AAABACVL" varchar(1) NOT NULL default ' ',
  "AAABACVM" varchar(1) NOT NULL default ' ',
  "AAABACVN" varchar(1) NOT NULL default ' ',
  "AAABACVO" varchar(1) NOT NULL default ' ',
  "AAABACVP" varchar(1) NOT NULL default ' ',
  "AAABACVQ" varchar(1) NOT NULL default ' ',
  "AAABACVR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABMPMH","AAABACVV","AAABLQAJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACVX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABACWF" varchar(10) NOT NULL default ' ',
  "AAABACWG" varchar(32) NOT NULL default ' ',
  "AAABMPMH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABACWF","AAABACWG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACWH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABARON" varchar(4) NOT NULL default ' ',
  "AAABBABG" varchar(4) NOT NULL default ' ',
  "AAABACWP" decimal(13,2) NOT NULL default '0.00',
  "AAABACWQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACWV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABAXLA" varchar(4) NOT NULL default ' ',
  "AAABINFJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABAXLA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACXF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABEQNK" varchar(2) NOT NULL default ' ',
  "AAABACXJ" decimal(4,0) NOT NULL default '0',
  "AAABACXK" decimal(4,0) NOT NULL default '0',
  "AAABACXL" varchar(3) NOT NULL default ' ',
  "AAABACXM" varchar(4) NOT NULL default ' ',
  "AAABACXN" varchar(1) NOT NULL default ' ',
  "AAABACXO" varchar(1) NOT NULL default ' ',
  "AAABACXP" varchar(2) NOT NULL default ' ',
  "AAABACXQ" varchar(3) NOT NULL default ' ',
  "AAABACXR" decimal(4,0) NOT NULL default '0',
  "AAABACXS" varchar(3) NOT NULL default ' ',
  "AAABBHTE" varchar(4) NOT NULL default ' ',
  "AAABMPMH" varchar(4) NOT NULL default ' ',
  "AAABACXV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABEQNK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACXZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADFD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABADFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACYG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABADFD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABADFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACYP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABBBFS" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABBBFS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACZB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADFN" varchar(4) NOT NULL default ' ',
  "AAABACZF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABADFN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACZJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADFN" varchar(4) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABACZU" varchar(10) NOT NULL default ' ',
  "AAABACZP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABADFN","AAABMAMP","AAABACZU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABACZV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADAF" varchar(2) NOT NULL default ' ',
  "AAABACZZ" varchar(1) NOT NULL default ' ',
  "AAABADAA" varchar(16) NOT NULL default ' ',
  "AAABMBXO" varchar(42) NOT NULL default ' ',
  "AAABADAC" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABADAF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADAG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBGQB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBGQB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBGQB" varchar(4) NOT NULL default ' ',
  "AAABADAX" varchar(4) NOT NULL default ' ',
  "AAABBGPY" varchar(1) NOT NULL default ' ',
  "AAABADAT" decimal(4,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBGQB","AAABADAX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADAY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABADBK" varchar(2) NOT NULL default '00',
  "AAABADBD" decimal(15,2) NOT NULL default '0.00',
  "AAABADBE" decimal(7,4) NOT NULL default '0.0000',
  "AAABADBF" decimal(9,4) NOT NULL default '0.0000',
  "AAABADBG" decimal(11,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABADBK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADBL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABLCGL" varchar(4) NOT NULL default ' ',
  "AAABADBP" varchar(255) NOT NULL default ' ',
  "AAABADBQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABLCGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADBU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBGPC" varchar(8) NOT NULL default '00000000',
  "AAABADBY" varchar(4) NOT NULL default ' ',
  "AAABADBZ" varchar(1) NOT NULL default ' ',
  "AAABADCA" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBGPC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADCE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADCP" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABADCJ" varchar(2) NOT NULL default '00',
  "AAABADCK" varchar(2) NOT NULL default '00',
  "AAABADCL" varchar(2) NOT NULL default '00',
  "AAABADCM" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABADCP","AAABJFHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADCR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADGR" varchar(4) NOT NULL default ' ',
  "AAABADCV" varchar(1) NOT NULL default ' ',
  "AAABADCW" varchar(1) NOT NULL default ' ',
  "AAABADCX" varchar(1) NOT NULL default ' ',
  "AAABADCY" varchar(1) NOT NULL default ' ',
  "AAABADCZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABADGR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADDD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADGR" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABADFD" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABADDL" varchar(2) NOT NULL default '00',
  "AAABADDM" varchar(2) NOT NULL default '00',
  "AAABADDN" varchar(1) NOT NULL default ' ',
  "AAABADDO" varchar(1) NOT NULL default ' ',
  "AAABADDP" varchar(1) NOT NULL default ' ',
  "AAABADDQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABADGR","AAABJFHB","AAABADFD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADDX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABADEA" varchar(1) NOT NULL default ' ',
  "AAABAOLK" varchar(1) NOT NULL default ' ',
  "AAABADEC" varchar(1) NOT NULL default ' ',
  "AAABADED" varchar(1) NOT NULL default ' ',
  "AAABBCJJ" varchar(1) NOT NULL default ' ',
  "AAABADEF" varchar(1) NOT NULL default ' ',
  "AAABADEG" varchar(1) NOT NULL default ' ',
  "AAABADEH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADEK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABMPMH" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADEU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADFD" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABADFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADFE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADFN" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABADFN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADFO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBGQB" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABBGQB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADFY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABLCGL" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABLCGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADGI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADGR" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABADGR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADGS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABADMG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABADMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADGX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABADMG" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABADMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADHF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBGJO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBGJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADHM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBGJO" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABBGJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADHW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADMA" varchar(4) NOT NULL default ' ',
  "AAABBGJO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABADMA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADIE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADMA" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABADMA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADIO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADMA" varchar(4) NOT NULL default ' ',
  "AAABADLR" varchar(4) NOT NULL default ' ',
  "AAABADIT" varchar(1) NOT NULL default ' ',
  "AAABBHSP" varchar(1) NOT NULL default ' ',
  "AAABBHSQ" varchar(1) NOT NULL default ' ',
  "AAABBHSR" varchar(1) NOT NULL default ' ',
  "AAABBHSV" varchar(4) NOT NULL default ' ',
  "AAABBHSW" varchar(2) NOT NULL default ' ',
  "AAABBHSX" varchar(2) NOT NULL default ' ',
  "AAABBHSY" varchar(2) NOT NULL default ' ',
  "AAABBHSZ" varchar(2) NOT NULL default ' ',
  "AAABBHTA" varchar(2) NOT NULL default ' ',
  "AAABBHTB" varchar(2) NOT NULL default ' ',
  "AAABBHTC" varchar(2) NOT NULL default ' ',
  "AAABBHTD" varchar(2) NOT NULL default ' ',
  "AAABBHTE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABADMA","AAABADLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADJL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADMA" varchar(4) NOT NULL default ' ',
  "AAABADLR" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABADMA","AAABADLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADJX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADLH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABADLH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADKE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADLH" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABADLH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADMA" varchar(4) NOT NULL default ' ',
  "AAABADLR" varchar(4) NOT NULL default ' ',
  "AAABBHVT" varchar(4) NOT NULL default ' ',
  "AAABBHVU" varchar(4) NOT NULL default ' ',
  "AAABBHVV" varchar(4) NOT NULL default ' ',
  "AAABADLH" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABADKZ" decimal(10,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABADMA","AAABADLR","AAABBHVT","AAABBHVU","AAABBHVV","AAABADLH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADLJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADMA" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABADLP" decimal(10,0) NOT NULL default '0',
  "AAABADLQ" varchar(1) NOT NULL default ' ',
  "AAABADLR" varchar(4) NOT NULL default ' ',
  "AAABADLS" decimal(10,0) NOT NULL default '0',
  "AAABADLT" decimal(3,0) NOT NULL default '0',
  "AAABADLU" decimal(10,0) NOT NULL default '0',
  "AAABADLV" decimal(3,0) NOT NULL default '0',
  "AAABADLW" varchar(1) NOT NULL default ' ',
  "AAABADQC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABADMA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADMC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADWZ" varchar(4) NOT NULL default ' ',
  "AAABADMG" varchar(2) NOT NULL default ' ',
  "AAABADMH" varchar(1) NOT NULL default ' ',
  "AAABBGJO" varchar(4) NOT NULL default ' ',
  "AAABADMJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABADWZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADMN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADWZ" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABADWZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADMX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADPX" varchar(4) NOT NULL default ' ',
  "AAABADNB" varchar(1) NOT NULL default ' ',
  "AAABBHSP" varchar(1) NOT NULL default ' ',
  "AAABBHSQ" varchar(1) NOT NULL default ' ',
  "AAABBHSR" varchar(1) NOT NULL default ' ',
  "AAABBHSV" varchar(4) NOT NULL default ' ',
  "AAABBHSW" varchar(2) NOT NULL default ' ',
  "AAABBHSX" varchar(2) NOT NULL default ' ',
  "AAABBHSY" varchar(2) NOT NULL default ' ',
  "AAABBHSZ" varchar(2) NOT NULL default ' ',
  "AAABBHTA" varchar(2) NOT NULL default ' ',
  "AAABBHTB" varchar(2) NOT NULL default ' ',
  "AAABBHTC" varchar(2) NOT NULL default ' ',
  "AAABBHTD" varchar(2) NOT NULL default ' ',
  "AAABBHTE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABADPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADNS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADPX" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABADPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADOC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADPO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABADPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADOJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADPO" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABADPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADOT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADPX" varchar(4) NOT NULL default ' ',
  "AAABBHVT" varchar(4) NOT NULL default ' ',
  "AAABBHVU" varchar(4) NOT NULL default ' ',
  "AAABBHVV" varchar(4) NOT NULL default ' ',
  "AAABADPO" varchar(4) NOT NULL default ' ',
  "AAABADPP" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABADPE" decimal(2,0) NOT NULL default '0',
  "AAABADPF" decimal(10,0) NOT NULL default '0',
  "AAABADPG" decimal(13,2) NOT NULL default '0.00',
  "AAABADPH" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABADPX","AAABBHVT","AAABBHVU","AAABBHVV","AAABADPO","AAABADPP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADPR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADWZ" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABADPX" varchar(4) NOT NULL default ' ',
  "AAABADPY" varchar(1) NOT NULL default ' ',
  "AAABADPZ" varchar(1) NOT NULL default ' ',
  "AAABADQA" varchar(1) NOT NULL default ' ',
  "AAABADQB" varchar(1) NOT NULL default ' ',
  "AAABADQC" varchar(4) NOT NULL default ' ',
  "AAABADQD" varchar(4) NOT NULL default ' ',
  "AAABADQE" varchar(4) NOT NULL default ' ',
  "AAABADQF" varchar(4) NOT NULL default ' ',
  "AAABADQG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABADWZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADQL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADWZ" varchar(4) NOT NULL default ' ',
  "AAABADRO" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABADQS" decimal(10,0) NOT NULL default '0',
  "AAABADQT" decimal(3,0) NOT NULL default '0',
  "AAABADQU" decimal(13,2) NOT NULL default '0.00',
  "AAABADQV" varchar(5) NOT NULL default ' ',
  "AAABADQW" varchar(4) NOT NULL default ' ',
  "AAABADQX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABADWZ","AAABADRO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADRD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADWZ" varchar(4) NOT NULL default ' ',
  "AAABADRO" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABADWZ","AAABADRO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADRP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADSK" varchar(4) NOT NULL default ' ',
  "AAABADRT" decimal(13,2) NOT NULL default '0.00',
  "AAABADRU" varchar(1) NOT NULL default ' ',
  "AAABADRV" decimal(13,2) NOT NULL default '0.00',
  "AAABADRW" decimal(10,0) NOT NULL default '0',
  "AAABADRX" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABADSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADSB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADSK" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABADSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADSL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADXJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABADXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADSS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADXJ" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABADXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADTC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADXK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABADXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADTJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADXK" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABADXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADTT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADXN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABADXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADUA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADXN" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABADXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADUK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBHPT" varchar(4) NOT NULL default ' ',
  "AAABMCNJ" varchar(4) NOT NULL default ' ',
  "AAABADWZ" varchar(4) NOT NULL default ' ',
  "AAABADUQ" varchar(8) NOT NULL default '00000000',
  "AAABADUR" varchar(8) NOT NULL default '00000000',
  "AAABADUS" varchar(2) NOT NULL default ' ',
  "AAABADXN" varchar(4) NOT NULL default ' ',
  "AAABADUU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBHPT","AAABMCNJ","AAABADWZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADVA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADWZ" varchar(4) NOT NULL default ' ',
  "AAABADVU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABADWZ","AAABADVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADVJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADWZ" varchar(4) NOT NULL default ' ',
  "AAABADVU" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABADWZ","AAABADVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADVV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADWZ" varchar(4) NOT NULL default ' ',
  "AAABADWP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABADWZ","AAABADWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADWE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADWZ" varchar(4) NOT NULL default ' ',
  "AAABADWP" varchar(1) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABADWZ","AAABADWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADWQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADWZ" varchar(4) NOT NULL default ' ',
  "AAABADXA" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABADWZ","AAABADXA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADXC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADXN" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHXM" varchar(1) NOT NULL default ' ',
  "AAABADXJ" varchar(4) NOT NULL default ' ',
  "AAABADXK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABADXN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADXP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABADYJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABADYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADXU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABADYJ" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABADYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADYC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABADYJ" varchar(4) NOT NULL default ' ',
  "AAABMPMH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABADYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABLQAJ" varchar(4) NOT NULL default ' ',
  "AAABADYO" varchar(2) NOT NULL default ' ',
  "AAABADYP" varchar(1) NOT NULL default ' ',
  "AAABADYQ" varchar(2) NOT NULL default ' ',
  "AAABADYR" varchar(1) NOT NULL default ' ',
  "AAABADYS" decimal(4,0) NOT NULL default '0',
  "AAABADYT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABLQAJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADYX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABADZJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABADZJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADZC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABADZJ" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABADZJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADZK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABENHV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADZQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABMPMH" varchar(4) NOT NULL default ' ',
  "AAABADZU" decimal(4,0) NOT NULL default '0',
  "AAABADZV" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABADZZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABBHWL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAEAG" decimal(10,5) NOT NULL default '0.00000',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABBHWL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEAM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABAEBS" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAEAT" decimal(10,5) NOT NULL default '0.00000',
  "AAABAEAU" decimal(10,5) NOT NULL default '0.00000',
  "AAABAEAV" decimal(10,5) NOT NULL default '0.00000',
  "AAABAEAW" decimal(10,5) NOT NULL default '0.00000',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABAEBS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEBC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABAEBS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABAEBS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEBJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABAEBS" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABAEBS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEBT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHTKH" varchar(2) NOT NULL default '00',
  "AAABAEKQ" varchar(4) NOT NULL default ' ',
  "AAABAEKP" varchar(4) NOT NULL default ' ',
  "AAABAECC" varchar(4) NOT NULL default ' ',
  "AAABAECD" varchar(4) NOT NULL default ' ',
  "AAABAECE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAECJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAECP" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAECU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABAEDF" varchar(4) NOT NULL default ' ',
  "AAABAEDG" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABAEDF","AAABAEDG","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEDI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABMPMH" varchar(4) NOT NULL default ' ',
  "AAABAEDM" varchar(1) NOT NULL default ' ',
  "AAABAEDN" varchar(1) NOT NULL default ' ',
  "AAABAEDO" varchar(2) NOT NULL default ' ',
  "AAABAEDP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEDT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABEKQV" varchar(4) NOT NULL default '0000',
  "AAABAEED" varchar(2) NOT NULL default ' ',
  "AAABAEDY" decimal(2,0) NOT NULL default '0',
  "AAABAEDZ" decimal(2,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABEKQV","AAABAEED")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEEE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAEEJ" decimal(3,0) NOT NULL default '0',
  "AAABAEEK" decimal(3,0) NOT NULL default '0',
  "AAABAEEL" varchar(1) NOT NULL default ' ',
  "AAABAEEM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABAEFF" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAEEX" varchar(1) NOT NULL default ' ',
  "AAABAEEY" varchar(4) NOT NULL default ' ',
  "AAABAEEZ" varchar(4) NOT NULL default ' ',
  "AAABAEFA" varchar(4) NOT NULL default ' ',
  "AAABAEFB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABAEFF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEFH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABAEGB" varchar(4) NOT NULL default ' ',
  "AAABKUES" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABAEGB","AAABKUES")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEFQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABAEGB" varchar(4) NOT NULL default ' ',
  "AAABAEGC" varchar(4) NOT NULL default ' ',
  "AAABKUES" varchar(4) NOT NULL default ' ',
  "AAABAEFW" varchar(4) NOT NULL default ' ',
  "AAABAEFX" varchar(1) NOT NULL default ' ',
  "AAABAEFY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABAEGB","AAABAEGC","AAABKUES")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEGE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABAEGH" varchar(1) NOT NULL default ' ',
  "AAABAEGI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEGL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAEGR" varchar(2) NOT NULL default ' ',
  "AAABAEKE" varchar(1) NOT NULL default ' ',
  "AAABAEGT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEGY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABBHWL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAEHF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABBHWL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEHL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABDSYA" varchar(3) NOT NULL default ' ',
  "AAABAEHS" varchar(1) NOT NULL default ' ',
  "AAABAEKE" varchar(1) NOT NULL default ' ',
  "AAABAEKD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABAEIV" varchar(4) NOT NULL default ' ',
  "AAABBHVT" varchar(4) NOT NULL default ' ',
  "AAABBHVU" varchar(4) NOT NULL default ' ',
  "AAABBHVV" varchar(4) NOT NULL default ' ',
  "AAABAEIZ" varchar(4) NOT NULL default ' ',
  "AAABBHVX" varchar(1) NOT NULL default ' ',
  "AAABAEJB" varchar(1) NOT NULL default ' ',
  "AAABKWRL" varchar(13) NOT NULL default '0000000000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAZSL" decimal(13,2) NOT NULL default '0.00',
  "AAABAEIO" decimal(7,4) NOT NULL default '0.0000',
  "AAABAEIP" varchar(1) NOT NULL default ' ',
  "AAABAEIQ" decimal(13,2) NOT NULL default '0.00',
  "AAABAEIR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABAEIV","AAABBHVT","AAABBHVU","AAABBHVV","AAABAEIZ","AAABBHVX","AAABAEJB","AAABKWRL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEJE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAEKP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKPXV" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEJW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABBHWL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAEKD" varchar(1) NOT NULL default ' ',
  "AAABAEKE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABBHWL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEKK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAEKP" varchar(4) NOT NULL default ' ',
  "AAABAEKQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEKU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAELO" varchar(8) NOT NULL default '00000000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAELO","AAABLHPM","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAELD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAELO" varchar(8) NOT NULL default '00000000',
  "AAABAELX" varchar(8) NOT NULL default '00000000',
  "AAABAELH" varchar(8) NOT NULL default '00000000',
  "AAABAELI" varchar(8) NOT NULL default '00000000',
  "AAABAELJ" varchar(70) NOT NULL default ' ',
  "AAABAELK" decimal(12,2) NOT NULL default '0.00',
  "AAABANOK" varchar(5) NOT NULL default ' ',
  "AAABAELM" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAELO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAELP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAELX" varchar(8) NOT NULL default '00000000',
  "AAABAENI" varchar(2) NOT NULL default '00',
  "AAABAELT" varchar(20) NOT NULL default ' ',
  "AAABCUWM" varchar(8) NOT NULL default ' ',
  "AAABAEOZ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAELX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAELY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAENI" varchar(2) NOT NULL default '00',
  "AAABAEMB" varchar(2) NOT NULL default ' ',
  "AAABAEOB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAENI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEMF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKNPJ" varchar(1) NOT NULL default ' ',
  "AAABAEMJ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABKNPJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEMN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAEPB" varchar(2) NOT NULL default ' ',
  "AAABAEMR" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAEPB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEMV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAENQ" varchar(3) NOT NULL default '000',
  "AAABAEVX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAENQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAENB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAENI" varchar(2) NOT NULL default '00',
  "AAABAENF" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAENI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAENJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAENQ" varchar(3) NOT NULL default '000',
  "AAABAENN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAENQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAENR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAEOM" varchar(3) NOT NULL default '000',
  "AAABAENU" varchar(2) NOT NULL default ' ',
  "AAABAENV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAEOM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAENY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNPJ" varchar(1) NOT NULL default ' ',
  "AAABAEOB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNPJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEOE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAEOM" varchar(3) NOT NULL default '000',
  "AAABAEOI" varchar(15) NOT NULL default ' ',
  "AAABAEOJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAEOM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEON" (
  "AAABLPNX" varchar(20) NOT NULL default ' ',
  "AAABAEOR" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPNX","AAABAEOR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEOS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAEOY" varchar(3) NOT NULL default ' ',
  "AAABAEOZ" varchar(8) NOT NULL default '00000000',
  "AAABKNPJ" varchar(1) NOT NULL default ' ',
  "AAABAEPB" varchar(2) NOT NULL default ' ',
  "AAABBGEH" varchar(8) NOT NULL default '00000000',
  "AAABAEPD" decimal(5,2) NOT NULL default '0.00',
  "AAABAEPE" decimal(5,2) NOT NULL default '0.00',
  "AAABAEPF" decimal(5,2) NOT NULL default '0.00',
  "AAABAEPG" varchar(3) NOT NULL default '000',
  "AAABAEPH" varchar(3) NOT NULL default '000',
  "AAABAEPI" varchar(3) NOT NULL default '000',
  "AAABAMFH" varchar(1) NOT NULL default ' ',
  "AAABLRBI" varchar(1) NOT NULL default ' ',
  "AAABIWAM" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLHPM","AAABLUNJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEPQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAEQR" varchar(1) NOT NULL default ' ',
  "AAABAEQW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAEQR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEPW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAEQR" varchar(1) NOT NULL default ' ',
  "AAABAEQA" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAEQR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEQE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAEQS" varchar(2) NOT NULL default '00',
  "AAABAEQI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAEQS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEQM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAEQR" varchar(1) NOT NULL default ' ',
  "AAABAEQS" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABAEQR","AAABAEQS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEQT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVXC" varchar(2) NOT NULL default ' ',
  "AAABAEQW" varchar(2) NOT NULL default ' ',
  "AAABHWLM" varchar(2) NOT NULL default ' ',
  "AAABAEQY" varchar(1) NOT NULL default ' ',
  "AAABLRVC" varchar(1) NOT NULL default ' ',
  "AAABAERA" varchar(5) NOT NULL default ' ',
  "AAABAERB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVXC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAERE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVXC" varchar(2) NOT NULL default ' ',
  "AAABAERI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVXC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAERM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMJWN" varchar(8) NOT NULL default ' ',
  "AAABHTLA" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAERS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAESL" varchar(6) NOT NULL default ' ',
  "AAABAESM" varchar(3) NOT NULL default '000',
  "AAABKRXK" varchar(6) NOT NULL default '000000',
  "AAABAESI" varchar(132) NOT NULL default ' ',
  "AAABLENG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAESL","AAABAESM","AAABKRXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAESD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAESL" varchar(6) NOT NULL default ' ',
  "AAABAESM" varchar(32) NOT NULL default ' ',
  "AAABKRXK" varchar(6) NOT NULL default '000000',
  "AAABAESI" varchar(132) NOT NULL default ' ',
  "AAABLENG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAESL","AAABAESM","AAABKRXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAESO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAEST" varchar(11) NOT NULL default '00000000000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAESU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABKAFO" varchar(132) NOT NULL default ' ',
  "AAABLQVJ" varchar(2) NOT NULL default '00',
  "AAABJECR" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABJFGL","AAABKAFO","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAETG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAETL" varchar(9) NOT NULL default '000000000',
  "AAABAETJ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAETL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAETM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAEUC" varchar(5) NOT NULL default ' ',
  "AAABAEUD" varchar(5) NOT NULL default ' ',
  "AAABLHAH" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAEUC","AAABAEUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAETU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAEUC" varchar(5) NOT NULL default ' ',
  "AAABAEUD" varchar(5) NOT NULL default ' ',
  "AAABAETZ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAEUC","AAABAEUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEUE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABLFYC" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(10) NOT NULL default ' ',
  "AAABGFTG" varchar(1) NOT NULL default ' ',
  "AAABAEUK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHJ","AAABLFYC","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEUP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABAEUS" varchar(8) NOT NULL default '00000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEUW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAFKC" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAFEW" decimal(15,2) NOT NULL default '0.00',
  "AAABAEVC" varchar(4) NOT NULL default ' ',
  "AAABAEVD" decimal(6,3) NOT NULL default '0.000',
  "AAABAFMS" varchar(8) NOT NULL default ' ',
  "AAABDCNN" decimal(15,2) NOT NULL default '0.00',
  "AAABDCNO" decimal(15,2) NOT NULL default '0.00',
  "AAABBKTQ" varchar(2) NOT NULL default '00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABAEVJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAFKC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEVN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAEVW" varchar(4) NOT NULL default ' ',
  "AAABAEVX" varchar(3) NOT NULL default ' ',
  "AAABAEVR" varchar(20) NOT NULL default ' ',
  "AAABAEVS" varchar(1) NOT NULL default ' ',
  "AAABLYNE" varchar(14) NOT NULL default ' ',
  "AAABAEVU" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAEVW","AAABAEVX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEVY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJSEA" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAEWD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJSEA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEWH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJSEA" varchar(2) NOT NULL default ' ',
  "AAABAFIN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJSEA","AAABAFIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEWO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJSEA" varchar(2) NOT NULL default ' ',
  "AAABAEWR" varchar(3) NOT NULL default '000',
  "AAABAEWS" varchar(3) NOT NULL default '000',
  "AAABAEWT" varchar(3) NOT NULL default ' ',
  "AAABAEWU" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJSEA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEWX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJSEA" varchar(2) NOT NULL default ' ',
  "AAABAEXB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABJSEA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEXF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAFKC" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAFMS" varchar(8) NOT NULL default ' ',
  "AAABBKTQ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABAFKC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEXP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAEYX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAEYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEXU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAEYX" varchar(2) NOT NULL default ' ',
  "AAABAEXY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAEYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEYC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAFNX" varchar(4) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAFNY" varchar(4) NOT NULL default ' ',
  "AAABAFFD" varchar(1) NOT NULL default ' ',
  "AAABIWAM" varchar(2) NOT NULL default ' ',
  "AAABIKEM" decimal(6,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABAFNX","AAABLQVJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEYQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAEYX" varchar(2) NOT NULL default ' ',
  "AAABAEYU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAEYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEYY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJSEA" varchar(2) NOT NULL default ' ',
  "AAABLEHK" varchar(2) NOT NULL default ' ',
  "AAABAXID" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJSEA","AAABLEHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEZG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAEZP" varchar(2) NOT NULL default ' ',
  "AAABAFGY" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAEZM" decimal(6,3) NOT NULL default '0.000',
  "AAABAEZN" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAEZP","AAABAFGY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEZS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAFAL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAFAL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAEZX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAFAL" varchar(2) NOT NULL default ' ',
  "AAABLEHK" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAFAL","AAABLEHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFAE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAFAL" varchar(2) NOT NULL default ' ',
  "AAABAFAI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAFAL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFAM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAFCF" varchar(4) NOT NULL default ' ',
  "AAABAFAP" varchar(4) NOT NULL default ' ',
  "AAABKWFM" varchar(2) NOT NULL default ' ',
  "AAABAFAR" varchar(4) NOT NULL default '0000',
  "AAABAFAS" varchar(3) NOT NULL default ' ',
  "AAABAFAT" varchar(1) NOT NULL default ' ',
  "AAABAFAU" varchar(1) NOT NULL default ' ',
  "AAABAFAV" decimal(4,0) NOT NULL default '0',
  "AAABAFAW" varchar(3) NOT NULL default ' ',
  "AAABMKKB" varchar(2) NOT NULL default ' ',
  "AAABAFAY" decimal(4,0) NOT NULL default '0',
  "AAABAFAZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAFCF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFBC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAFCF" varchar(4) NOT NULL default ' ',
  "AAABJSEA" varchar(2) NOT NULL default ' ',
  "AAABAFIN" varchar(2) NOT NULL default ' ',
  "AAABAFGU" varchar(1) NOT NULL default ' ',
  "AAABAFGV" varchar(2) NOT NULL default ' ',
  "AAABAFGW" varchar(2) NOT NULL default ' ',
  "AAABAFGX" varchar(2) NOT NULL default ' ',
  "AAABAFGY" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAFCF","AAABJSEA","AAABAFIN","AAABAFGU","AAABAFGV","AAABAFGW","AAABAFGX","AAABAFGY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFBY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAFCF" varchar(4) NOT NULL default ' ',
  "AAABAFCC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAFCF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFCG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAFDJ" varchar(2) NOT NULL default ' ',
  "AAABAFGW" varchar(2) NOT NULL default ' ',
  "AAABAFCT" varchar(2) NOT NULL default ' ',
  "AAABAFCU" varchar(2) NOT NULL default ' ',
  "AAABAFCV" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAFCP" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABAFDJ","AAABAFGW","AAABAFCT","AAABAFCU","AAABAFCV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFCX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAFDJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAFDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFDC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAFDJ" varchar(2) NOT NULL default ' ',
  "AAABAFDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAFDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFDK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGZXX" varchar(2) NOT NULL default ' ',
  "AAABAFDO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABGZXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFDS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZXX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFDX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAFKC" varchar(4) NOT NULL default ' ',
  "AAABAFMI" varchar(1) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABAFET" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAFKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFEG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAFKC" varchar(4) NOT NULL default ' ',
  "AAABAFEK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAFKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFEO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAFKC" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAFET" varchar(1) NOT NULL default ' ',
  "AAABBKTQ" varchar(2) NOT NULL default '00',
  "AAABAFEV" varchar(8) NOT NULL default ' ',
  "AAABAFEW" decimal(15,2) NOT NULL default '0.00',
  "AAABAFEX" varchar(5) NOT NULL default ' ',
  "AAABDCNN" decimal(15,2) NOT NULL default '0.00',
  "AAABDCNO" decimal(15,2) NOT NULL default '0.00',
  "AAABAFFA" varchar(5) NOT NULL default ' ',
  "AAABAFFB" varchar(1) NOT NULL default ' ',
  "AAABAFFC" varchar(1) NOT NULL default ' ',
  "AAABAFFD" varchar(1) NOT NULL default ' ',
  "AAABAFFE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAFKC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFFI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEHK" varchar(2) NOT NULL default ' ',
  "AAABLPNX" varchar(4) NOT NULL default ' ',
  "AAABFZUL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLEHK","AAABLPNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFFQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGFQS" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGFQS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFFV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGFQS" varchar(2) NOT NULL default ' ',
  "AAABAFFZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABGFQS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFGD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAFKC" varchar(4) NOT NULL default ' ',
  "AAABJSEA" varchar(2) NOT NULL default ' ',
  "AAABAFIN" varchar(2) NOT NULL default ' ',
  "AAABAFGU" varchar(1) NOT NULL default ' ',
  "AAABAFGV" varchar(2) NOT NULL default ' ',
  "AAABAFGW" varchar(2) NOT NULL default ' ',
  "AAABAFGX" varchar(2) NOT NULL default ' ',
  "AAABAFGY" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAFGP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAFKC","AAABJSEA","AAABAFIN","AAABAFGU","AAABAFGV","AAABAFGW","AAABAFGX","AAABAFGY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFHA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIWAM" varchar(2) NOT NULL default ' ',
  "AAABFZUL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIWAM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFHI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIWAM" varchar(2) NOT NULL default ' ',
  "AAABAFHM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABIWAM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFHQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJSEA" varchar(2) NOT NULL default ' ',
  "AAABAFIA" varchar(2) NOT NULL default ' ',
  "AAABAFHU" varchar(1) NOT NULL default ' ',
  "AAABAFHV" varchar(30) NOT NULL default ' ',
  "AAABENDM" varchar(42) NOT NULL default ' ',
  "AAABAFHX" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABJSEA","AAABAFIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFIB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAFIN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAFIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFIG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAFIN" varchar(2) NOT NULL default ' ',
  "AAABAFIK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAFIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFIO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAFKC" varchar(4) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAFKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAFKC" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAFJC" varchar(4) NOT NULL default ' ',
  "AAABFZUL" varchar(4) NOT NULL default ' ',
  "AAABAFJE" decimal(2,0) NOT NULL default '0',
  "AAABAFJF" decimal(3,0) NOT NULL default '0',
  "AAABAFJG" varchar(1) NOT NULL default ' ',
  "AAABAFJH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAFKC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFJM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAFNX" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAFNY" varchar(4) NOT NULL default ' ',
  "AAABIWAM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAFNX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFJW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJSEA" varchar(2) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAFKC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJSEA","AAABLUVE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFKH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAFKT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAFKT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAFKT" varchar(2) NOT NULL default ' ',
  "AAABAFKQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAFKT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFKU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEHK" varchar(2) NOT NULL default ' ',
  "AAABIYJJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLEHK","AAABIYJJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFLB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIYJJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIYJJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFLG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAFLR" varchar(12) NOT NULL default ' ',
  "AAABAFLS" varchar(8) NOT NULL default '00000000',
  "AAABAFLT" varchar(6) NOT NULL default '000000',
  "AAABAFLU" varchar(8) NOT NULL default '00000000',
  "AAABAFLM" varchar(8) NOT NULL default '00000000',
  "AAABAFLN" varchar(8) NOT NULL default '00000000',
  "AAABAFLO" varchar(3) NOT NULL default '000',
  "AAABAFLP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAFLR","AAABAFLS","AAABAFLT","AAABAFLU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFLV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLEHK" varchar(2) NOT NULL default ' ',
  "AAABIYJJ" varchar(2) NOT NULL default ' ',
  "AAABAFMA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLEHK","AAABIYJJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFMF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAFMS" varchar(8) NOT NULL default ' ',
  "AAABAFMI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAFMS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFML" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAFMS" varchar(8) NOT NULL default ' ',
  "AAABHIWY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAFMS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFMT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABAFMW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFMZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJSEA" varchar(2) NOT NULL default ' ',
  "AAABAFNG" varchar(2) NOT NULL default ' ',
  "AAABAFNH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJSEA","AAABAFNG","AAABAFNH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFNI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABAFNM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFNQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAFNX" varchar(4) NOT NULL default ' ',
  "AAABAFNY" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAFNX","AAABAFNY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFOA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAFOY" varchar(8) NOT NULL default ' ',
  "AAABAMIV" varchar(6) NOT NULL default '000000',
  "AAABAMIW" varchar(6) NOT NULL default '000000',
  "AAABAFOF" varchar(6) NOT NULL default '000000',
  "AAABAFOG" varchar(6) NOT NULL default '000000',
  "AAABAFOH" varchar(6) NOT NULL default '000000',
  "AAABAFOI" varchar(6) NOT NULL default '000000',
  "AAABKRZW" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABAFOY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFOM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAFOS" varchar(30) NOT NULL default ' ',
  "AAABAFOT" varchar(30) NOT NULL default ' ',
  "AAABCITX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAFOS","AAABAFOT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAGVB" varchar(8) NOT NULL default ' ',
  "AAABAFPB" varchar(3) NOT NULL default '000',
  "AAABAFOY" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAGVB","AAABAFPB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFPC" (
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABAFPJ" varchar(30) NOT NULL default ' ',
  "AAABAFPF" varchar(30) NOT NULL default ' ',
  "AAABAFPG" int(11) NOT NULL default '0',
  "AAABAFPH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJFGL","AAABAFPJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFPK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABAFQM" varchar(132) NOT NULL default ' ',
  "AAABAFPO" varchar(1) NOT NULL default ' ',
  "AAABGYLC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFGL","AAABAFQM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFPT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAABAFQA" varchar(30) NOT NULL default ' ',
  "AAABAFPX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUAV","AAABAFQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFQB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAFQM" varchar(30) NOT NULL default ' ',
  "AAABAFQE" varchar(30) NOT NULL default ' ',
  "AAABGYKY" varchar(30) NOT NULL default ' ',
  "AAABAFQG" varchar(30) NOT NULL default ' ',
  "AAABAFQH" varchar(30) NOT NULL default ' ',
  "AAABAFQI" varchar(30) NOT NULL default ' ',
  "AAABAFQJ" varchar(30) NOT NULL default ' ',
  "AAABAFQK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAFQM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFQN" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABCITX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFQT" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLAUS" varchar(55) NOT NULL default ' ',
  "AAABKGYO" varchar(10) NOT NULL default ' ',
  "AAABKGYP" varchar(20) NOT NULL default ' ',
  "AAABKGYQ" varchar(40) NOT NULL default ' ',
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  "AAABJEDB" varchar(30) NOT NULL default ' ',
  "AAABEFYT" varchar(30) NOT NULL default ' ',
  "AAABAFRD" varchar(1) NOT NULL default ' ',
  "AAABLSDL" varchar(10) NOT NULL default ' ',
  "AAABAFRF" varchar(30) NOT NULL default ' ',
  "AAABAFRG" varchar(30) NOT NULL default ' ',
  "AAABAFRH" varchar(6) NOT NULL default '000000',
  "AAABAFRI" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFRL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFRQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAFSF" varchar(8) NOT NULL default ' ',
  "AAABLPWW" varchar(40) NOT NULL default ' ',
  "AAABIMEG" varchar(25) NOT NULL default ' ',
  "AAABLVHQ" varchar(30) NOT NULL default ' ',
  "AAABLVHI" varchar(40) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLYNE" varchar(25) NOT NULL default ' ',
  "AAABAXPS" varchar(25) NOT NULL default ' ',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  "AAABAFSC" varchar(35) NOT NULL default ' ',
  "AAABAFSD" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAFSF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFSG" (
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJDFT" varchar(1) NOT NULL default ' ',
  "AAABJDFU" varchar(3) NOT NULL default ' ',
  "AAABJDGB" varchar(2) NOT NULL default ' ',
  "AAABAFSL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHPM","AAABJDFT","AAABJDFU","AAABJDGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAHAQ" varchar(4) NOT NULL default '0000',
  "AAABAFSU" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAHAQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFSY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJDFT" varchar(1) NOT NULL default ' ',
  "AAABJDFU" varchar(3) NOT NULL default ' ',
  "AAABJDGB" varchar(2) NOT NULL default ' ',
  "AAABAFTE" varchar(1) NOT NULL default ' ',
  "AAABAFTF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHPM","AAABJDFT","AAABJDFU","AAABJDGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFTL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBQFF" varchar(4) NOT NULL default ' ',
  "AAABKZAW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBQFF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFTT" (
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABAFTV" varchar(30) NOT NULL default ' ',
  "AAABAFTW" varchar(30) NOT NULL default ' ',
  "AAABAFTX" varchar(30) NOT NULL default ' ',
  "AAABAHCX" varchar(4) NOT NULL default ' ',
  "AAABAHCY" varchar(4) NOT NULL default ' ',
  "AAABAFUA" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLDEV" varchar(30) NOT NULL default ' ',
  "AAABAFUD" varchar(30) NOT NULL default ' ',
  "AAABAGRQ" varchar(30) NOT NULL default ' ',
  "AAABAFUF" varchar(30) NOT NULL default ' ',
  "AAABAFUG" varchar(30) NOT NULL default ' ',
  "AAABAFUH" varchar(30) NOT NULL default ' ',
  "AAABAFUI" varchar(30) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABAFUK" varchar(40) NOT NULL default ' ',
  "AAABAFUL" varchar(1) NOT NULL default ' ',
  "AAABAFUM" varchar(30) NOT NULL default ' ',
  "AAABAFUN" varchar(1) NOT NULL default ' ',
  "AAABAFUO" varchar(30) NOT NULL default ' ',
  "AAABAFUP" varchar(30) NOT NULL default ' ',
  "AAABAFUQ" varchar(30) NOT NULL default ' ',
  "AAABAFUR" varchar(30) NOT NULL default ' ',
  "AAABAFUS" varchar(1) NOT NULL default ' ',
  "AAABAFUT" varchar(1) NOT NULL default ' ',
  "AAABAFUU" varchar(1) NOT NULL default ' ',
  "AAABAFUV" varchar(1) NOT NULL default ' ',
  "AAABAFUW" varchar(1) NOT NULL default ' ',
  "AAABAFUX" varchar(1) NOT NULL default ' ',
  "AAABAFUY" varchar(1) NOT NULL default ' ',
  "AAABAFUZ" varchar(1) NOT NULL default ' ',
  "AAABAFVA" varchar(1) NOT NULL default ' ',
  "AAABAFVB" varchar(1) NOT NULL default ' ',
  "AAABAFVC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJFGL"),
  KEY "AAABAFVE" ("AAABAGRQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFVG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABAFVK" varchar(30) NOT NULL default ' ',
  "AAABAFVL" varchar(30) NOT NULL default ' ',
  "AAABAFVM" varchar(30) NOT NULL default ' ',
  "AAABAFVN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFGL","AAABJFHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFVR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAGZO" varchar(2) NOT NULL default ' ',
  "AAABJDFU" varchar(3) NOT NULL default ' ',
  "AAABAGZP" varchar(2) NOT NULL default ' ',
  "AAABJDFT" varchar(1) NOT NULL default ' ',
  "AAABAMFH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAGZO","AAABJDFU","AAABAGZP","AAABJDFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFWD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHC" varchar(1) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHC","AAABJFGL","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFWM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABAGKJ" varchar(4) NOT NULL default ' ',
  "AAABHPIX" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABJFHB","AAABAGKJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFWW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTVR"),
  KEY "AAABAFXI" ("AAABJFHJ","AAABLHPM","AAABLUNJ","AAABMPMT","AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFXO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAFXT" varchar(4) NOT NULL default ' ',
  "AAABMJGC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAFXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFXU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABAMFH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHPM","AAABJFGL","AAABJFHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFYF" (
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABLLIO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHPM","AAABJFGL","AAABJFHB","AAABLLIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFYO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABJFHC" varchar(1) NOT NULL default ' ',
  "AAABHFFQ" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABLBEK" varchar(1) NOT NULL default ' ',
  "AAABIJZM" varchar(1) NOT NULL default ' ',
  "AAABAFZS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFGL","AAABJFHB","AAABLHPM","AAABJFHJ","AAABJFHC","AAABHFFQ","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAFZI" (
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABJFHC" varchar(1) NOT NULL default ' ',
  "AAABHFFQ" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABLBEK" varchar(1) NOT NULL default ' ',
  "AAABIJZM" varchar(1) NOT NULL default ' ',
  "AAABAFZS" varchar(1) NOT NULL default ' ',
  "AAABECCJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJFGL","AAABJFHB","AAABLHPM","AAABJFHJ","AAABJFHC","AAABHFFQ","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGAB" (
  "AAABAGAF" varchar(1) NOT NULL default ' ',
  "AAABAGAG" varchar(1) NOT NULL default ' ',
  "AAABAGAE" decimal(9,5) NOT NULL default '0.00000',
  PRIMARY KEY ("AAABAGAF","AAABAGAG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGAH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVXC" varchar(4) NOT NULL default ' ',
  "AAABAGAV" varchar(3) NOT NULL default '000',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABJFHC" varchar(1) NOT NULL default ' ',
  "AAABJFHF" varchar(10) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABAGAS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVXC","AAABAGAV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGAW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHALW" varchar(3) NOT NULL default ' ',
  "AAABAGBI" varchar(3) NOT NULL default '000',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABLWIT" varchar(30) NOT NULL default ' ',
  "AAABLWIV" varchar(30) NOT NULL default ' ',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMNYU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHALW","AAABAGBI","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGBK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABAGEX" varchar(3) NOT NULL default ' ',
  "AAABAGCO" varchar(12) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMLPN" varchar(50) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABLENH","AAABAGEX","AAABAGCO","AAABMREL","AAABMLPN"),
  KEY "AAABAGCD" ("AAABLMNC","AAABMREL","AAABMLPN","AAABAGCO","AAABMRIL"),
  KEY "AAABAGCJ" ("AAABLENH","AAABAGEX","AAABMREL","AAABMLPN","AAABAGCO","AAABMRIL"),
  KEY "AAABAGCQ" ("AAABMRIL","AAABMREL","AAABMLPN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGCU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABAGEX" varchar(3) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMLPN" varchar(50) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLENH","AAABAGEX","AAABMREL","AAABMLPN"),
  KEY "AAABAGDG" ("AAABMRIL","AAABMREL","AAABMLPN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGDK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABAGEX" varchar(3) NOT NULL default ' ',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABLEMJ" varchar(10) NOT NULL default ' ',
  "AAABJZVA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLENH","AAABAGEX","AAABLMNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGDV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABAGEX" varchar(3) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMLPN" varchar(50) NOT NULL default ' ',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLENH","AAABAGEX","AAABMREL","AAABMLPN"),
  KEY "AAABAGEJ" ("AAABMRIL","AAABMREL","AAABMLPN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGEN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABAGEX" varchar(3) NOT NULL default ' ',
  "AAABEDJP" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABEIPO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLENH","AAABAGEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGEY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABCCLT" varchar(10) NOT NULL default ' ',
  "AAABAGFD" varchar(10) NOT NULL default ' ',
  "AAABHGWF" varchar(1) NOT NULL default ' ',
  "AAABAGFF" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLENH","AAABLMNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGFJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGFO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABHIOX" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGFW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABIMUR" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABJFHJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGGE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHDCQ" varchar(4) NOT NULL default '0000',
  "AAABAGGI" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHDCQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGGM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEBIH" varchar(4) NOT NULL default ' ',
  "AAABLHPM" varchar(4) NOT NULL default ' ',
  "AAABHZCZ" varchar(4) NOT NULL default ' ',
  "AAABLGEL" varchar(1) NOT NULL default ' ',
  "AAABHFFQ" varchar(30) NOT NULL default ' ',
  "AAABAGGT" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEBIH","AAABLHPM","AAABHZCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGGY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJFHC" varchar(1) NOT NULL default ' ',
  "AAABLPWW" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABJFHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGHG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABAZVX" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABJFGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGHO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABAGHT" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABJFGL","AAABJFHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGHY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJDFU" varchar(3) NOT NULL default ' ',
  "AAABLPUZ" varchar(25) NOT NULL default ' ',
  "AAABANRW" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABJDFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGIH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABAGMZ" varchar(3) NOT NULL default ' ',
  "AAABFLOA" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABJFHB","AAABAGMZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGIR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAGNE" varchar(4) NOT NULL default ' ',
  "AAABJQTW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAGNE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGIZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABANSD" varchar(1) NOT NULL default ' ',
  "AAABEJQB" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHPM","AAABJFGL","AAABJFHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGJK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCDT" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJCDT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGJP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAHAQ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABAHAQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGJU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBQFF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBQFF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGJZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABAGKJ" varchar(4) NOT NULL default ' ',
  "AAABAGKD" varchar(8) NOT NULL default ' ',
  "AAABAGKE" varchar(3) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJVUZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHB","AAABAGKJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGKK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAGKP" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAGKP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGKQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVXC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVXC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGKV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLLIO" varchar(10) NOT NULL default ' ',
  "AAABLTXO" varchar(30) NOT NULL default ' ',
  "AAABAKZJ" varchar(30) NOT NULL default ' ',
  "AAABJSLF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHPM"),
  KEY "AAABAGLE" ("AAABMRIL","AAABLLIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGLH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGLM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGLR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHDCQ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABHDCQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGLW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGMB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGMG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFGL","AAABJFHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGMN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDFU" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGMS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABAGMZ" varchar(3) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHB","AAABAGMZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGNA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAGNE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAGNE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGNF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIO" varchar(10) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJCDT" varchar(8) NOT NULL default ' ',
  "AAABAGNK" varchar(8) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABAMFH" varchar(1) NOT NULL default ' ',
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIO","AAABLHPM"),
  KEY "AAABAGNT" ("AAABMRIL","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGNW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAGOU" varchar(10) NOT NULL default ' ',
  "AAABAGNZ" varchar(2) NOT NULL default ' ',
  "AAABAGOA" varchar(8) NOT NULL default ' ',
  "AAABAGOB" varchar(2) NOT NULL default ' ',
  "AAABAGOC" varchar(8) NOT NULL default ' ',
  "AAABAGOD" varchar(1) NOT NULL default ' ',
  "AAABAGOE" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAGOU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGOH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGOM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAGOU" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAGOQ" varchar(25) NOT NULL default ' ',
  "AAABAGOR" varchar(25) NOT NULL default ' ',
  "AAABAGOS" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAGOU","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGOW" (
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABAGPZ" varchar(4) NOT NULL default ' ',
  "AAABANFV" varchar(4) NOT NULL default ' ',
  "AAABAGQB" varchar(3) NOT NULL default '000',
  "AAABAGPT" varchar(10) NOT NULL default ' ',
  "AAABMPMH" varchar(32) NOT NULL default ' ',
  "AAABHOTF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHPM","AAABJFGL","AAABJFHB","AAABAGPZ","AAABANFV","AAABAGQB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGPM" (
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABAGPZ" varchar(4) NOT NULL default ' ',
  "AAABANFV" varchar(4) NOT NULL default ' ',
  "AAABAGQB" varchar(3) NOT NULL default '000',
  "AAABAGPT" varchar(10) NOT NULL default ' ',
  "AAABMPMH" varchar(32) NOT NULL default ' ',
  "AAABHOTF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHPM","AAABJFGL","AAABJFHB","AAABAGPZ","AAABANFV","AAABAGQB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGQC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAGVB" varchar(8) NOT NULL default ' ',
  "AAABIIVG" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAGVB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGQK" (
  "AAABHWBH" varchar(15) NOT NULL default ' ',
  "AAABHVTW" varchar(12) NOT NULL default ' ',
  "AAABAGQW" varchar(1) NOT NULL default ' ',
  "AAABHVQO" varchar(2) NOT NULL default '00',
  "AAABHVUJ" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABHWBH","AAABHVTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGQS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHWBH" varchar(15) NOT NULL default ' ',
  "AAABHVTW" varchar(12) NOT NULL default ' ',
  "AAABAGQW" varchar(1) NOT NULL default ' ',
  "AAABHVQO" varchar(2) NOT NULL default '00',
  "AAABHVUJ" varchar(15) NOT NULL default ' ',
  "AAABHVQQ" varchar(1) NOT NULL default ' ',
  "AAABJDQT" varchar(1) NOT NULL default ' ',
  "AAABHVQP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHWBH","AAABHVTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGRF" (
  "AAABHWBH" varchar(15) NOT NULL default ' ',
  "AAABHVTW" varchar(12) NOT NULL default ' ',
  "AAABHVQQ" varchar(1) NOT NULL default ' ',
  "AAABJDQT" varchar(1) NOT NULL default ' ',
  "AAABHVQP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHWBH","AAABHVTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGRN" (
  "AAABAIIS" varchar(30) NOT NULL default ' ',
  "AAABLDEV" varchar(30) NOT NULL default ' ',
  "AAABAGRQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABAIIS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGRS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDFU" varchar(3) NOT NULL default ' ',
  "AAABAGRZ" varchar(2) NOT NULL default ' ',
  "AAABAGSA" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDFU","AAABAGRZ","AAABAGSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGSB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDFU" varchar(3) NOT NULL default ' ',
  "AAABAGSE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGSH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCJUZ" varchar(10) NOT NULL default ' ',
  "AAABCKEB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCJUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGSN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMJO" varchar(10) NOT NULL default ' ',
  "AAABCKEB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGST" (
  "AAABAGUV" varchar(4) NOT NULL default '0000',
  "AAABLKXV" varchar(10) NOT NULL default ' ',
  "AAABAGTJ" varchar(32) NOT NULL default ' ',
  "AAABAGTK" varchar(30) NOT NULL default ' ',
  "AAABAGSY" varchar(1) NOT NULL default ' ',
  "AAABAGSZ" varchar(1) NOT NULL default ' ',
  "AAABAGTA" varchar(1) NOT NULL default ' ',
  "AAABAGTB" varchar(1) NOT NULL default ' ',
  "AAABAGTC" varchar(1) NOT NULL default ' ',
  "AAABAGTD" varchar(1) NOT NULL default ' ',
  "AAABAGTE" varchar(1) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABAGUV","AAABLKXV","AAABAGTJ","AAABAGTK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--enable_query_log
