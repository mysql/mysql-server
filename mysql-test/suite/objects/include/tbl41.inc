# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+
# |                             tbl41.inc                              |
# |                        Copyright 2006 by MySQL AB                  |
# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+

--disable_query_log
CREATE TABLE "AAABHGJE" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABHGXL" varchar(32) NOT NULL default ' ',
  "AAABHGXH" varchar(32) NOT NULL default ' ',
  "AAABHGJI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABHGXL","AAABHGXH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGJM" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABHGJR" varchar(32) NOT NULL default ' ',
  "AAABHGJS" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABHGJR","AAABHGJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGJT" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMLPO" int(11) NOT NULL default '0',
  "AAABINZS" varchar(10) NOT NULL default ' ',
  "AAABHGVY" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABJAAI","AAABMLPO"),
  KEY "AAABHGKC" ("AAABINZS","AAABHGVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGKF" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABHGKI" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGKK" (
  "AAABLTXI" varchar(32) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABMOHM" varchar(5) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABKMZH" varchar(1) NOT NULL default ' ',
  "AAABLELN" varchar(12) NOT NULL default ' ',
  "AAABHGKU" varchar(1) NOT NULL default ' ',
  "AAABHGKV" varchar(10) NOT NULL default ' ',
  "AAABHGKW" varchar(1) NOT NULL default ' ',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  "AAABHZPJ" varchar(4) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABHZPN" varchar(4) NOT NULL default ' ',
  "AAABLQSF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI"),
  KEY "AAABHGLH" ("AAABLUSV","AAABHZPG","AAABHZPH","AAABHZPI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGLM" (
  "AAABLTXI" varchar(32) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABHGTR" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(15) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABLGEY","AAABJAAI","AAABMRHK","AAABHZRB","AAABHGTR","AAABMMJT"),
  KEY "AAABHGMC" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGME" (
  "AAABLTXI" varchar(32) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABHGTR" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(15) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABLGEY","AAABJAAI","AAABMRHK","AAABHZRB","AAABHGTR","AAABMMJT"),
  KEY "AAABHGMU" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGMW" (
  "AAABLTXI" varchar(32) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABHGTR" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(3) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABLGEY","AAABJAAI","AAABMRHK","AAABHZRB","AAABHGTR","AAABMMJT"),
  KEY "AAABHGNM" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGNO" (
  "AAABLTXI" varchar(32) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABHGTR" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(3) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABLGEY","AAABJAAI","AAABMRHK","AAABHZRB","AAABHGTR","AAABMMJT"),
  KEY "AAABHGOE" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGOG" (
  "AAABLTXI" varchar(32) NOT NULL default ' ',
  "AAABHHAY" varchar(8) NOT NULL default '00000000',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABLILS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABHHAY","AAABMRHK"),
  KEY "AAABHGOO" ("AAABMRHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGOQ" (
  "AAABLTXI" varchar(32) NOT NULL default ' ',
  "AAABLMUZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABLMUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGOV" (
  "AAABLTXI" varchar(32) NOT NULL default ' ',
  "AAABHHAY" varchar(8) NOT NULL default '00000000',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMLPO" int(11) NOT NULL default '0',
  "AAABINZS" varchar(10) NOT NULL default ' ',
  "AAABHGVY" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABHHAY","AAABJAAI","AAABMLPO"),
  KEY "AAABHGPG" ("AAABINZS","AAABHGVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGPJ" (
  "AAABLTXI" varchar(32) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABHGTR" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(3) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABLGEY","AAABJAAI","AAABMRHK","AAABHZRB","AAABHGTR","AAABMMJT"),
  KEY "AAABHGPZ" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGQB" (
  "AAABLTXI" varchar(32) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABHGTR" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(3) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABLGEY","AAABJAAI","AAABMRHK","AAABHZRB","AAABHGTR","AAABMMJT"),
  KEY "AAABHGQR" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGQT" (
  "AAABLTXI" varchar(32) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABHGTR" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(3) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABLGEY","AAABJAAI","AAABMRHK","AAABHZRB","AAABHGTR","AAABMMJT"),
  KEY "AAABHGRJ" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGRL" (
  "AAABLTXI" varchar(32) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABHGTR" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(3) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABLGEY","AAABJAAI","AAABMRHK","AAABHZRB","AAABHGTR","AAABMMJT"),
  KEY "AAABHGSB" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGSD" (
  "AAABLTXI" varchar(32) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABLMLR" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABLTXI","AAABLUSV","AAABJAAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGSL" (
  "AAABLTXI" varchar(32) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABHGTR" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(3) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABLGEY","AAABJAAI","AAABMRHK","AAABHZRB","AAABHGTR","AAABMMJT"),
  KEY "AAABHGTB" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGTD" (
  "AAABLTXI" varchar(32) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABHGTR" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(3) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABLGEY","AAABJAAI","AAABMRHK","AAABHZRB","AAABHGTR","AAABMMJT"),
  KEY "AAABHGTT" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGTV" (
  "AAABLTXI" varchar(32) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  "AAABHGVD" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABMRHS"),
  KEY "AAABHGUD" ("AAABMRHS","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGUG" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLTXI" varchar(32) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  "AAABHGVD" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLTXI"),
  KEY "AAABHGUN" ("AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGUP" (
  "AAABHHCQ" varchar(32) NOT NULL default ' ',
  "AAABHHCR" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMBXY" varchar(8) NOT NULL default '00000000',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABHHCQ","AAABHHCR","AAABMRHS","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGUZ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHHCQ" varchar(32) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  "AAABHGVD" varchar(75) NOT NULL default ' ',
  "AAABHGVE" varchar(75) NOT NULL default ' ',
  "AAABHGVF" varchar(75) NOT NULL default ' ',
  "AAABHGVG" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABHHCQ"),
  KEY "AAABHGVJ" ("AAABHHCQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGVL" (
  "AAABLTXI" varchar(32) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMLPO" int(11) NOT NULL default '0',
  "AAABINZS" varchar(10) NOT NULL default ' ',
  "AAABHGVY" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABLUSV","AAABJAAI","AAABMLPO"),
  KEY "AAABHGVW" ("AAABINZS","AAABHGVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGVZ" (
  "AAABHHCQ" varchar(32) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABHGYT" varchar(12) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABHGYG" varchar(6) NOT NULL default '000000',
  "AAABHGWF" varchar(1) NOT NULL default ' ',
  "AAABHGWG" varchar(4) NOT NULL default ' ',
  "AAABKMZH" varchar(1) NOT NULL default ' ',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAABHGYI" varchar(12) NOT NULL default ' ',
  "AAABHGYJ" varchar(8) NOT NULL default '00000000',
  "AAABHGYK" varchar(6) NOT NULL default '000000',
  "AAABHGWM" varchar(32) NOT NULL default ' ',
  "AAABHGWN" varchar(12) NOT NULL default ' ',
  "AAABHGWO" varchar(8) NOT NULL default '00000000',
  "AAABHGWP" varchar(6) NOT NULL default '000000',
  "AAABHGWQ" varchar(32) NOT NULL default ' ',
  "AAABHGWR" varchar(12) NOT NULL default ' ',
  "AAABHGWS" varchar(8) NOT NULL default '00000000',
  "AAABHGWT" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABHHCQ"),
  KEY "AAABHGWV" ("AAABLUSV","AAABHGYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGWY" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABHGXA" varchar(6) NOT NULL default '000000',
  "AAABHGXB" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGXD" (
  "AAABLTXI" varchar(32) NOT NULL default ' ',
  "AAABHGXK" varchar(8) NOT NULL default '00000000',
  "AAABHGXL" varchar(32) NOT NULL default ' ',
  "AAABHGXH" varchar(32) NOT NULL default ' ',
  "AAABHGXI" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLTXI","AAABHGXK","AAABHGXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGXM" (
  "AAABLTXI" varchar(32) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABHGYA" varchar(32) NOT NULL default ' ',
  "AAABHGXQ" varchar(32) NOT NULL default ' ',
  "AAABHGYT" varchar(12) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABHGYG" varchar(6) NOT NULL default '000000',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAABHGYI" varchar(12) NOT NULL default ' ',
  "AAABHGYJ" varchar(8) NOT NULL default '00000000',
  "AAABHGYK" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLTXI","AAABLUSV","AAABHGYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGYB" (
  "AAABLTXI" varchar(32) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABHGYT" varchar(12) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABHGYG" varchar(6) NOT NULL default '000000',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAABHGYI" varchar(12) NOT NULL default ' ',
  "AAABHGYJ" varchar(8) NOT NULL default '00000000',
  "AAABHGYK" varchar(6) NOT NULL default '000000',
  "AAABHGYL" varchar(8) NOT NULL default '00000000',
  "AAABKMZH" varchar(1) NOT NULL default ' ',
  "AAABMQXO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI"),
  KEY "AAABHGYP" ("AAABLUSV","AAABMPMT"),
  KEY "AAABHGYS" ("AAABHGYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGYU" (
  "AAABLTXI" varchar(32) NOT NULL default ' ',
  "AAABHHAY" varchar(8) NOT NULL default '00000000',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABLMUZ" varchar(4) NOT NULL default ' ',
  "AAABHRWW" varchar(2) NOT NULL default '00',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABLFYJ" varchar(32) NOT NULL default ' ',
  "AAABHZOI" varchar(40) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABHZOJ" varchar(32) NOT NULL default ' ',
  "AAABHZOK" varchar(32) NOT NULL default ' ',
  "AAABHGZG" varchar(1) NOT NULL default ' ',
  "AAABHZOM" varchar(1) NOT NULL default ' ',
  "AAABHZON" varchar(4) NOT NULL default ' ',
  "AAABHZOO" varchar(4) NOT NULL default ' ',
  "AAABHZOP" varchar(8) NOT NULL default '00000000',
  "AAABHZOQ" varchar(8) NOT NULL default '00000000',
  "AAABLTSU" varchar(1) NOT NULL default ' ',
  "AAABHZOR" varchar(1) NOT NULL default ' ',
  "AAABHGZO" varchar(32) NOT NULL default ' ',
  "AAABHGZP" varchar(1) NOT NULL default ' ',
  "AAABHGZQ" varchar(1) NOT NULL default ' ',
  "AAABHZQH" varchar(1) NOT NULL default ' ',
  "AAABHZQI" varchar(1) NOT NULL default ' ',
  "AAABHZQJ" varchar(1) NOT NULL default ' ',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  "AAABHZPJ" varchar(4) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABHZPN" varchar(4) NOT NULL default ' ',
  "AAABMDXP" varchar(12) NOT NULL default ' ',
  "AAABHHAD" varchar(12) NOT NULL default ' ',
  "AAABHHAE" varchar(1) NOT NULL default ' ',
  "AAABHHAF" varchar(1) NOT NULL default ' ',
  "AAABHHAG" varchar(1) NOT NULL default ' ',
  "AAABHHAH" varchar(1) NOT NULL default ' ',
  "AAABKGFE" varchar(1) NOT NULL default ' ',
  "AAABHHAJ" varchar(1) NOT NULL default ' ',
  "AAABHHAK" varchar(1) NOT NULL default ' ',
  "AAABHZPD" varchar(1) NOT NULL default ' ',
  "AAABHZOV" varchar(32) NOT NULL default ' ',
  "AAABHZOW" varchar(32) NOT NULL default ' ',
  "AAABHZOX" varchar(32) NOT NULL default ' ',
  "AAABHZOY" varchar(32) NOT NULL default ' ',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABHZQA" varchar(20) NOT NULL default ' ',
  "AAABHZQB" varchar(10) NOT NULL default ' ',
  "AAABHZQC" varchar(10) NOT NULL default ' ',
  "AAABHHAU" varchar(1) NOT NULL default ' ',
  "AAABHZQF" varchar(20) NOT NULL default ' ',
  "AAABHZQG" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABHHAY"),
  KEY "AAABHHAZ" ("AAABHZOJ"),
  KEY "AAABHHBB" ("AAABHZOK","AAABHZOR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHBE" (
  "AAABLTXI" varchar(32) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABJCXS" varchar(4) NOT NULL default ' ',
  "AAABHZQN" varchar(70) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABHHBK" varchar(10) NOT NULL default '0000000000',
  "AAABLILU" varchar(1) NOT NULL default ' ',
  "AAABHHBM" varchar(1) NOT NULL default ' ',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABHZQA" varchar(20) NOT NULL default ' ',
  "AAABHZQB" varchar(10) NOT NULL default ' ',
  "AAABHZQC" varchar(10) NOT NULL default ' ',
  "AAABHZQD" varchar(1) NOT NULL default ' ',
  "AAABHZQE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABMRHK","AAABJCXS","AAABHZQN","AAABJAAI"),
  KEY "AAABHHBY" ("AAABJCXS","AAABHZQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHCB" (
  "AAABHHCQ" varchar(32) NOT NULL default ' ',
  "AAABHHCR" varchar(1) NOT NULL default ' ',
  "AAABMBXY" varchar(8) NOT NULL default '00000000',
  "AAABLFYJ" varchar(8) NOT NULL default '00000000',
  "AAABHHCG" varchar(1) NOT NULL default ' ',
  "AAABLFYL" varchar(2) NOT NULL default '00',
  "AAABLOSB" varchar(8) NOT NULL default '00000000',
  "AAABHHCJ" varchar(255) NOT NULL default ' ',
  "AAABHHCK" varchar(255) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  "AAABHHCN" varchar(1) NOT NULL default ' ',
  "AAABHHCO" varchar(32) NOT NULL default ' ',
  "AAABHHCP" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABHHCQ","AAABHHCR","AAABMBXY","AAABLFYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHCU" (
  "AAABLTXI" varchar(32) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABHZQZ" varchar(15) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  "AAABHHDB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABMRHS","AAABMRHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHDF" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(32) NOT NULL default ' ',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  "AAABHZPJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHDO" (
  "AAABJATL" varchar(10) NOT NULL default ' ',
  "AAABHHDQ" varchar(20) NOT NULL default ' ',
  "AAABHHLQ" varchar(5) NOT NULL default ' ',
  "AAABHHNN" varchar(1) NOT NULL default ' ',
  "AAABHHNO" varchar(12) NOT NULL default ' ',
  "AAABHHNP" decimal(15,0) NOT NULL default '0',
  "AAABHHNQ" varchar(12) NOT NULL default ' ',
  "AAABHHNR" decimal(15,0) NOT NULL default '0',
  "AAABHHNS" varchar(12) NOT NULL default ' ',
  "AAABHHNT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABJATL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHEA" (
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABJATL" varchar(10) NOT NULL default ' ',
  "AAABHHNZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNJW","AAABJATL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHEG" (
  "AAABHHKJ" varchar(10) NOT NULL default ' ',
  "AAABHHEI" varchar(20) NOT NULL default ' ',
  "AAABHHEJ" varchar(10) NOT NULL default ' ',
  "AAABHHEK" varchar(10) NOT NULL default ' ',
  "AAABHHEL" varchar(10) NOT NULL default ' ',
  "AAABHHEM" varchar(30) NOT NULL default ' ',
  "AAABHHEN" varchar(1) NOT NULL default ' ',
  "AAABHHEO" varchar(1) NOT NULL default ' ',
  "AAABHHLQ" varchar(5) NOT NULL default ' ',
  "AAABHHNN" varchar(1) NOT NULL default ' ',
  "AAABHHNO" varchar(12) NOT NULL default ' ',
  "AAABHHNP" decimal(15,0) NOT NULL default '0',
  "AAABHHNQ" varchar(12) NOT NULL default ' ',
  "AAABHHNR" decimal(15,0) NOT NULL default '0',
  "AAABHHNS" varchar(12) NOT NULL default ' ',
  "AAABHHNT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABHHKJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHEY" (
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABHHKJ" varchar(10) NOT NULL default ' ',
  "AAABHHNZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNJW","AAABHHKJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHFE" (
  "AAABHHFV" varchar(10) NOT NULL default ' ',
  "AAABHHFG" varchar(20) NOT NULL default ' ',
  "AAABHHLQ" varchar(5) NOT NULL default ' ',
  "AAABHHNN" varchar(1) NOT NULL default ' ',
  "AAABHHNO" varchar(12) NOT NULL default ' ',
  "AAABHHNP" decimal(15,0) NOT NULL default '0',
  "AAABHHNQ" varchar(12) NOT NULL default ' ',
  "AAABHHNR" decimal(15,0) NOT NULL default '0',
  "AAABHHNS" varchar(12) NOT NULL default ' ',
  "AAABHHNT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABHHFV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHFQ" (
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABHHFV" varchar(10) NOT NULL default ' ',
  "AAABHHNZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNJW","AAABHHFV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHFW" (
  "AAABLTIZ" varchar(10) NOT NULL default ' ',
  "AAABHHGN" varchar(1) NOT NULL default ' ',
  "AAABHHLP" varchar(10) NOT NULL default ' ',
  "AAABHHGA" varchar(30) NOT NULL default ' ',
  "AAABHHGB" varchar(30) NOT NULL default ' ',
  "AAABHHGC" varchar(30) NOT NULL default ' ',
  "AAABHHGD" varchar(30) NOT NULL default ' ',
  "AAABHHLQ" varchar(5) NOT NULL default ' ',
  "AAABHHNN" varchar(1) NOT NULL default ' ',
  "AAABHHNO" varchar(12) NOT NULL default ' ',
  "AAABHHNP" decimal(15,0) NOT NULL default '0',
  "AAABHHNQ" varchar(12) NOT NULL default ' ',
  "AAABHHNR" decimal(15,0) NOT NULL default '0',
  "AAABHHNS" varchar(12) NOT NULL default ' ',
  "AAABHHNT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLTIZ","AAABHHGN","AAABHHLP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHGP" (
  "AAABHHLP" varchar(10) NOT NULL default ' ',
  "AAABHHGR" varchar(20) NOT NULL default ' ',
  "AAABHHGS" varchar(30) NOT NULL default ' ',
  "AAABHHGT" tinyint(3) unsigned NOT NULL default '0',
  "AAABHHGU" varchar(30) NOT NULL default ' ',
  "AAABHHGV" varchar(30) NOT NULL default ' ',
  "AAABHHGW" varchar(30) NOT NULL default ' ',
  "AAABHHGX" varchar(30) NOT NULL default ' ',
  "AAABHHGY" varchar(30) NOT NULL default ' ',
  "AAABHHGZ" varchar(30) NOT NULL default ' ',
  "AAABHHHA" varchar(30) NOT NULL default ' ',
  "AAABHHLQ" varchar(5) NOT NULL default ' ',
  "AAABHHNN" varchar(1) NOT NULL default ' ',
  "AAABHHNO" varchar(12) NOT NULL default ' ',
  "AAABHHNP" decimal(15,0) NOT NULL default '0',
  "AAABHHNQ" varchar(12) NOT NULL default ' ',
  "AAABHHNR" decimal(15,0) NOT NULL default '0',
  "AAABHHNS" varchar(12) NOT NULL default ' ',
  "AAABHHNT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABHHLP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHHK" (
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABHHLP" varchar(10) NOT NULL default ' ',
  "AAABHHNZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNJW","AAABHHLP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHHQ" (
  "AAABHHLQ" varchar(5) NOT NULL default ' ',
  "AAABHHHS" varchar(20) NOT NULL default ' ',
  "AAABHHNN" varchar(1) NOT NULL default ' ',
  "AAABHHNO" varchar(12) NOT NULL default ' ',
  "AAABHHNP" decimal(15,0) NOT NULL default '0',
  "AAABHHNQ" varchar(12) NOT NULL default ' ',
  "AAABHHNR" decimal(15,0) NOT NULL default '0',
  "AAABHHNS" varchar(12) NOT NULL default ' ',
  "AAABHHNT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABHHLQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHIB" (
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABHHLQ" varchar(5) NOT NULL default ' ',
  "AAABHHNZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNJW","AAABHHLQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHIH" (
  "AAABLTIZ" varchar(10) NOT NULL default ' ',
  "AAABHHIJ" varchar(20) NOT NULL default ' ',
  "AAABHHIK" varchar(10) NOT NULL default ' ',
  "AAABHHIL" varchar(1) NOT NULL default ' ',
  "AAABHHIM" varchar(1) NOT NULL default ' ',
  "AAABHHIN" varchar(1) NOT NULL default ' ',
  "AAABHHIO" varchar(1) NOT NULL default ' ',
  "AAABHHIP" varchar(30) NOT NULL default ' ',
  "AAABHHLQ" varchar(5) NOT NULL default ' ',
  "AAABHHNN" varchar(1) NOT NULL default ' ',
  "AAABHHNO" varchar(12) NOT NULL default ' ',
  "AAABHHNP" decimal(15,0) NOT NULL default '0',
  "AAABHHNQ" varchar(12) NOT NULL default ' ',
  "AAABHHNR" decimal(15,0) NOT NULL default '0',
  "AAABHHNS" varchar(12) NOT NULL default ' ',
  "AAABHHNT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLTIZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHIZ" (
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABLTIZ" varchar(10) NOT NULL default ' ',
  "AAABHHNZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNJW","AAABLTIZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHJF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHHLP" varchar(10) NOT NULL default ' ',
  "AAABHHLZ" varchar(10) NOT NULL default ' ',
  "AAABHHLQ" varchar(5) NOT NULL default ' ',
  "AAABHHNN" varchar(1) NOT NULL default ' ',
  "AAABHHNO" varchar(12) NOT NULL default ' ',
  "AAABHHNP" decimal(15,0) NOT NULL default '0',
  "AAABHHNQ" varchar(12) NOT NULL default ' ',
  "AAABHHNR" decimal(15,0) NOT NULL default '0',
  "AAABHHNS" varchar(12) NOT NULL default ' ',
  "AAABHHNT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABHHLP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHJT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHHLZ" varchar(10) NOT NULL default ' ',
  "AAABHHKJ" varchar(10) NOT NULL default ' ',
  "AAABHHJX" int(11) NOT NULL default '0',
  "AAABHHJY" varchar(1) NOT NULL default ' ',
  "AAABHHLQ" varchar(5) NOT NULL default ' ',
  "AAABHHNN" varchar(1) NOT NULL default ' ',
  "AAABHHNO" varchar(12) NOT NULL default ' ',
  "AAABHHNP" decimal(15,0) NOT NULL default '0',
  "AAABHHNQ" varchar(12) NOT NULL default ' ',
  "AAABHHNR" decimal(15,0) NOT NULL default '0',
  "AAABHHNS" varchar(12) NOT NULL default ' ',
  "AAABHHNT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABHHLZ","AAABHHKJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHKK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHHLZ" varchar(10) NOT NULL default ' ',
  "AAABHHKN" varchar(20) NOT NULL default ' ',
  "AAABHHKO" varchar(10) NOT NULL default ' ',
  "AAABHHLP" varchar(10) NOT NULL default ' ',
  "AAABHHLQ" varchar(5) NOT NULL default ' ',
  "AAABHHNN" varchar(1) NOT NULL default ' ',
  "AAABHHNO" varchar(12) NOT NULL default ' ',
  "AAABHHNP" decimal(15,0) NOT NULL default '0',
  "AAABHHNQ" varchar(12) NOT NULL default ' ',
  "AAABHHNR" decimal(15,0) NOT NULL default '0',
  "AAABHHNS" varchar(12) NOT NULL default ' ',
  "AAABHHNT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABHHLZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHLA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABHHLZ" varchar(10) NOT NULL default ' ',
  "AAABHHNZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMNJW","AAABHHLZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHLI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHHLZ" varchar(10) NOT NULL default ' ',
  "AAABHHMA" varchar(10) NOT NULL default ' ',
  "AAABHHMB" varchar(10) NOT NULL default ' ',
  "AAABHHLN" varchar(2) NOT NULL default ' ',
  "AAABHHLO" varchar(10) NOT NULL default ' ',
  "AAABHHLP" varchar(10) NOT NULL default ' ',
  "AAABHHLQ" varchar(5) NOT NULL default ' ',
  "AAABHHNN" varchar(1) NOT NULL default ' ',
  "AAABHHNO" varchar(12) NOT NULL default ' ',
  "AAABHHNP" decimal(15,0) NOT NULL default '0',
  "AAABHHNQ" varchar(12) NOT NULL default ' ',
  "AAABHHNR" decimal(15,0) NOT NULL default '0',
  "AAABHHNS" varchar(12) NOT NULL default ' ',
  "AAABHHNT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABHHLZ","AAABHHMA","AAABHHMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHMC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHHNK" varchar(10) NOT NULL default ' ',
  "AAABHHMF" varchar(10) NOT NULL default ' ',
  "AAABHHMG" varchar(10) NOT NULL default ' ',
  "AAABHHMH" varchar(10) NOT NULL default ' ',
  "AAABHHMI" varchar(10) NOT NULL default ' ',
  "AAABHHMJ" varchar(10) NOT NULL default ' ',
  "AAABHHMK" varchar(10) NOT NULL default ' ',
  "AAABHHML" varchar(1) NOT NULL default '0',
  "AAABHHMM" varchar(1) NOT NULL default ' ',
  "AAABHHMN" varchar(1) NOT NULL default ' ',
  "AAABHHMO" varchar(1) NOT NULL default ' ',
  "AAABHHMP" varchar(1) NOT NULL default ' ',
  "AAABHHMQ" varchar(1) NOT NULL default ' ',
  "AAABHHMR" varchar(1) NOT NULL default ' ',
  "AAABHHMS" varchar(1) NOT NULL default ' ',
  "AAABHHMT" varchar(1) NOT NULL default ' ',
  "AAABHHNN" varchar(1) NOT NULL default ' ',
  "AAABHHNO" varchar(12) NOT NULL default ' ',
  "AAABHHNP" decimal(15,0) NOT NULL default '0',
  "AAABHHNQ" varchar(12) NOT NULL default ' ',
  "AAABHHNR" decimal(15,0) NOT NULL default '0',
  "AAABHHNS" varchar(12) NOT NULL default ' ',
  "AAABHHNT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABHHNK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHND" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABHHNK" varchar(10) NOT NULL default ' ',
  "AAABHHNZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMNJW","AAABHHNK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHNL" (
  "AAABHHOB" varchar(1) NOT NULL default ' ',
  "AAABHHNN" varchar(1) NOT NULL default ' ',
  "AAABHHNO" varchar(12) NOT NULL default ' ',
  "AAABHHNP" decimal(15,0) NOT NULL default '0',
  "AAABHHNQ" varchar(12) NOT NULL default ' ',
  "AAABHHNR" decimal(15,0) NOT NULL default '0',
  "AAABHHNS" varchar(12) NOT NULL default ' ',
  "AAABHHNT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABHHOB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHNV" (
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABHHOB" varchar(1) NOT NULL default ' ',
  "AAABHHNY" varchar(10) NOT NULL default ' ',
  "AAABHHNZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNJW","AAABHHOB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHOC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKXWQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKXWQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHOH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKXWQ" varchar(2) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKXWQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHOP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHHSH" varchar(2) NOT NULL default ' ',
  "AAABJAMQ" varchar(10) NOT NULL default ' ',
  "AAABHHSJ" varchar(2) NOT NULL default ' ',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLSTE" varchar(32) NOT NULL default ' ',
  "AAABHHOW" varchar(1) NOT NULL default ' ',
  "AAABHHOX" varchar(1) NOT NULL default ' ',
  "AAABHHOY" varchar(1) NOT NULL default ' ',
  "AAABHHOZ" varchar(1) NOT NULL default ' ',
  "AAABLYTF" varchar(6) NOT NULL default ' ',
  "AAABHHPB" varchar(2) NOT NULL default '00',
  "AAABHHPC" varchar(2) NOT NULL default '00',
  "AAABHHPD" varchar(2) NOT NULL default ' ',
  "AAABHHPE" varchar(30) NOT NULL default ' ',
  "AAABHHPF" varchar(30) NOT NULL default ' ',
  "AAABHHPG" varchar(30) NOT NULL default ' ',
  "AAABHHPH" varchar(4) NOT NULL default ' ',
  "AAABHHPI" varchar(1) NOT NULL default ' ',
  "AAABHHSB" decimal(15,0) NOT NULL default '0',
  "AAABHHSC" varchar(12) NOT NULL default ' ',
  "AAABJQWM" decimal(15,0) NOT NULL default '0',
  "AAABLRQM" varchar(12) NOT NULL default ' ',
  "AAABHHRK" decimal(15,0) NOT NULL default '0',
  "AAABHHRL" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHHSH","AAABJAMQ","AAABHHSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHPT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLSTE" varchar(32) NOT NULL default ' ',
  "AAABHHQZ" varchar(4) NOT NULL default '0000',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHHSH" varchar(2) NOT NULL default ' ',
  "AAABJAMQ" varchar(10) NOT NULL default ' ',
  "AAABHHSJ" varchar(2) NOT NULL default ' ',
  "AAABHHRK" decimal(15,0) NOT NULL default '0',
  "AAABHHRL" varchar(12) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLSTE","AAABHHQZ","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHQL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLSTE" varchar(32) NOT NULL default ' ',
  "AAABHHQZ" varchar(4) NOT NULL default '0000',
  "AAABHHSH" varchar(2) NOT NULL default ' ',
  "AAABJAMQ" varchar(10) NOT NULL default ' ',
  "AAABHHSJ" varchar(2) NOT NULL default ' ',
  "AAABHHQT" int(11) NOT NULL default '0',
  "AAABHHRK" decimal(15,0) NOT NULL default '0',
  "AAABHHRL" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLSTE","AAABHHQZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHRA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHHSH" varchar(2) NOT NULL default ' ',
  "AAABJAMQ" varchar(10) NOT NULL default ' ',
  "AAABHHSJ" varchar(2) NOT NULL default ' ',
  "AAABHHRF" varchar(4) NOT NULL default ' ',
  "AAABHHSB" decimal(15,0) NOT NULL default '0',
  "AAABHHSC" varchar(12) NOT NULL default ' ',
  "AAABJQWM" decimal(15,0) NOT NULL default '0',
  "AAABLRQM" varchar(12) NOT NULL default ' ',
  "AAABHHRK" decimal(15,0) NOT NULL default '0',
  "AAABHHRL" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHHSH","AAABJAMQ","AAABHHSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHRQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIGKR" varchar(1) NOT NULL default ' ',
  "AAABHHSH" varchar(2) NOT NULL default ' ',
  "AAABJAMQ" varchar(10) NOT NULL default ' ',
  "AAABHHSJ" varchar(2) NOT NULL default ' ',
  "AAABIUHL" varchar(10) NOT NULL default ' ',
  "AAABLOFJ" varchar(40) NOT NULL default ' ',
  "AAABHHRY" varchar(10) NOT NULL default ' ',
  "AAABHHRZ" varchar(4) NOT NULL default ' ',
  "AAABHHSA" varchar(70) NOT NULL default ' ',
  "AAABHHSB" decimal(15,0) NOT NULL default '0',
  "AAABHHSC" varchar(12) NOT NULL default ' ',
  "AAABJQWM" decimal(15,0) NOT NULL default '0',
  "AAABLRQM" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIGKR","AAABHHSH","AAABJAMQ","AAABHHSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHSK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABHHST" varchar(8) NOT NULL default ' ',
  "AAABHHSU" varchar(10) NOT NULL default ' ',
  "AAABHHSV" varchar(4) NOT NULL default '0000',
  "AAABHXLS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABHHST","AAABHHSU","AAABHHSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHSW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABIFJO" varchar(4) NOT NULL default ' ',
  "AAABIFJP" varchar(15) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
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
  PRIMARY KEY ("AAABMRIL","AAABLCPX","AAABIFJO","AAABIFJP","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHTR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABHIEF" varchar(30) NOT NULL default ' ',
  "AAABHHTV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMEKQ","AAABHIEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHTZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABIFJO" varchar(4) NOT NULL default ' ',
  "AAABIFJP" varchar(15) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABHHUF" varchar(40) NOT NULL default ' ',
  "AAABHHUG" varchar(50) NOT NULL default ' ',
  "AAABMBTQ" varchar(16) NOT NULL default ' ',
  "AAABIFJV" varchar(1) NOT NULL default ' ',
  "AAABIFJU" varchar(1) NOT NULL default ' ',
  "AAABHHUK" varchar(1) NOT NULL default ' ',
  "AAABHHUL" varchar(30) NOT NULL default ' ',
  "AAABHHUM" varchar(8) NOT NULL default ' ',
  "AAABIFJS" varchar(1) NOT NULL default ' ',
  "AAABMEEK" varchar(4) NOT NULL default ' ',
  "AAABHHUP" varchar(1) NOT NULL default ' ',
  "AAABHHUQ" varchar(1) NOT NULL default ' ',
  "AAABHHUR" varchar(1) NOT NULL default ' ',
  "AAABHHUS" varchar(1) NOT NULL default ' ',
  "AAABHHUT" varchar(1) NOT NULL default ' ',
  "AAABHHUU" varchar(1) NOT NULL default ' ',
  "AAABIFAF" varchar(1) NOT NULL default ' ',
  "AAABHHUW" varchar(10) NOT NULL default '0000000000',
  "AAABHHUX" varchar(30) NOT NULL default ' ',
  "AAABHHUY" varchar(10) NOT NULL default '0000000000',
  "AAABHHUZ" varchar(1) NOT NULL default ' ',
  "AAABHHVA" varchar(1) NOT NULL default ' ',
  "AAABHHVB" varchar(1) NOT NULL default ' ',
  "AAABHHVC" varchar(1) NOT NULL default ' ',
  "AAABIFJW" varchar(1) NOT NULL default ' ',
  "AAABHHVE" varchar(1) NOT NULL default ' ',
  "AAABHHVF" varchar(1) NOT NULL default ' ',
  "AAABJRDA" varchar(4) NOT NULL default ' ',
  "AAABIGRE" varchar(2) NOT NULL default ' ',
  "AAABJRGC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCPX","AAABIFJO","AAABIFJP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHVN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABIFJO" varchar(4) NOT NULL default ' ',
  "AAABIFJP" varchar(15) NOT NULL default ' ',
  "AAABHHVT" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLCPX","AAABIFJO","AAABIFJP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHVZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHHWH" varchar(16) NOT NULL default ' ',
  "AAABHHWI" varchar(8) NOT NULL default '00000000',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABIFJO" varchar(4) NOT NULL default ' ',
  "AAABIFJP" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHHWH","AAABHHWI","AAABLCPX","AAABIFJO","AAABIFJP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHWM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABIFJO" varchar(4) NOT NULL default ' ',
  "AAABIFJP" varchar(15) NOT NULL default ' ',
  "AAABHHXI" varchar(3) NOT NULL default '000',
  "AAABHHWS" varchar(10) NOT NULL default ' ',
  "AAABHHWT" varchar(70) NOT NULL default ' ',
  "AAABHHWU" varchar(1) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABHHWW" varchar(30) NOT NULL default ' ',
  "AAABIFJS" varchar(1) NOT NULL default ' ',
  "AAABHHWY" varchar(1) NOT NULL default ' ',
  "AAABHHWZ" varchar(1) NOT NULL default ' ',
  "AAABHHXA" varchar(1) NOT NULL default ' ',
  "AAABHHXB" varchar(15) NOT NULL default ' ',
  "AAABHHXC" varchar(30) NOT NULL default ' ',
  "AAABHHXD" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCPX","AAABIFJO","AAABIFJP","AAABHHXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHXJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABHIEF" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMEKQ","AAABHIEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHXT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHIHQ" varchar(1) NOT NULL default ' ',
  "AAABMPMH" varchar(3) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHIHQ","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHYC" (
  "AAABLZNN" varchar(10) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLZNN","AAABLZNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHYH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZNN" varchar(10) NOT NULL default ' ',
  "AAABHIOG" varchar(1) NOT NULL default ' ',
  "AAABLYCG" varchar(2) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAABHHYX" varchar(3) NOT NULL default '000',
  "AAABHHYO" varchar(1) NOT NULL default ' ',
  "AAABJLIP" varchar(2) NOT NULL default ' ',
  "AAABHHYQ" varchar(1) NOT NULL default ' ',
  "AAABHIOI" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABLZNN","AAABHIOG","AAABLYCG","AAABLZNO","AAABHHYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHYY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABHHZM" varchar(5) NOT NULL default ' ',
  "AAABHHZN" varchar(1) NOT NULL default ' ',
  "AAABHHZO" varchar(10) NOT NULL default ' ',
  "AAABHHZG" varchar(5) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMX","AAABMAMY","AAABMEKQ","AAABHHZM","AAABHHZN","AAABHHZO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHZP" (
  "AAABHHZT" varchar(6) NOT NULL default ' ',
  "AAABHHZR" varchar(6) NOT NULL default ' ',
  "AAABHHZS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHHZT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHHZU" (
  "AAABMBTQ" varchar(16) NOT NULL default ' ',
  "AAABHIAE" varchar(40) NOT NULL default ' ',
  "AAABHHZX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBTQ","AAABHIAE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIAA" (
  "AAABMBTQ" varchar(16) NOT NULL default ' ',
  "AAABHIAE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBTQ","AAABHIAE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIAF" (
  "AAABMBTQ" varchar(16) NOT NULL default ' ',
  "AAABHIGR" varchar(11) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBTQ","AAABHIGR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIAK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZNN" varchar(10) NOT NULL default ' ',
  "AAABLYCG" varchar(2) NOT NULL default ' ',
  "AAABHIOG" varchar(1) NOT NULL default ' ',
  "AAABHIAP" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHIAR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZNN","AAABLYCG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIAV" (
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIBA" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMKFH","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIBI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLZNN" varchar(10) NOT NULL default ' ',
  "AAABLYCG" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLZNN","AAABLYCG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIBS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABKJVG" varchar(10) NOT NULL default ' ',
  "AAABMQCE" varchar(10) NOT NULL default ' ',
  "AAABHIHQ" varchar(1) NOT NULL default ' ',
  "AAABIVCA" varchar(1) NOT NULL default ' ',
  "AAABKUQR" varchar(8) NOT NULL default '00000000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQCD","AAABKJVG","AAABMQCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHICF" (
  "AAABLZNN" varchar(10) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAABHICI" varchar(4) NOT NULL default ' ',
  "AAABHICJ" varchar(1) NOT NULL default ' ',
  "AAABHICK" varchar(1) NOT NULL default ' ',
  "AAABHICL" varchar(1) NOT NULL default ' ',
  "AAABHICM" varchar(1) NOT NULL default ' ',
  "AAABHIFR" varchar(6) NOT NULL default ' ',
  "AAABHICO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLZNN","AAABLZNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHICR" (
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHICV" (
  "AAABHIDE" varchar(10) NOT NULL default ' ',
  "AAABHIDF" varchar(4) NOT NULL default ' ',
  "AAABHIDG" varchar(10) NOT NULL default ' ',
  "AAABHIDH" varchar(4) NOT NULL default ' ',
  "AAABHIDA" varchar(1) NOT NULL default ' ',
  "AAABHIDB" varchar(1) NOT NULL default ' ',
  "AAABHIDC" varchar(1) NOT NULL default ' ',
  "AAABHIDD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHIDE","AAABHIDF","AAABHIDG","AAABHIDH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIDI" (
  "AAABKEXE" varchar(1) NOT NULL default ' ',
  "AAABLZNN" varchar(10) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAABHIFX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABKEXE","AAABLZNN","AAABLZNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIDQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABMCXD" varchar(15) NOT NULL default ' ',
  "AAABHIDU" varchar(1) NOT NULL default ' ',
  "AAABHIDV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMEKQ","AAABMCXD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIDZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABHIJV" varchar(6) NOT NULL default ' ',
  "AAABJDPL" varchar(8) NOT NULL default '00000000',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  "AAABHIEF" varchar(30) NOT NULL default ' ',
  "AAABHIEG" varchar(15) NOT NULL default ' ',
  "AAABHIEH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMEKQ","AAABHIJV","AAABJDPL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIEM" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABHIES" longblob,
  PRIMARY KEY ("AAABMDEN","AAABMRFQ","AAABMLES","AAABMREL","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIEY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABHIJV" varchar(6) NOT NULL default ' ',
  "AAABJDPL" varchar(8) NOT NULL default '00000000',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMEKQ","AAABHIJV","AAABJDPL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIFK" (
  "AAABLZNN" varchar(10) NOT NULL default ' ',
  "AAABHIFM" varchar(1) NOT NULL default ' ',
  "AAABIFKS" varchar(2) NOT NULL default ' ',
  "AAABHIFO" varchar(3) NOT NULL default '000',
  "AAABHIFP" varchar(8) NOT NULL default ' ',
  "AAABMBTQ" varchar(16) NOT NULL default ' ',
  "AAABHIFR" varchar(6) NOT NULL default ' ',
  "AAABHIFS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLZNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIFU" (
  "AAABKEXE" varchar(1) NOT NULL default ' ',
  "AAABLZNN" varchar(10) NOT NULL default ' ',
  "AAABHIFX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABKEXE","AAABLZNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIGA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHIHQ" varchar(1) NOT NULL default ' ',
  "AAABHIGH" varchar(10) NOT NULL default ' ',
  "AAABHIJY" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHIHQ","AAABHIGH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIGI" (
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKGJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIGL" (
  "AAABLZNN" varchar(10) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAABHIGR" varchar(11) NOT NULL default ' ',
  PRIMARY KEY ("AAABLZNN","AAABLZNO","AAABHIGR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIGS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRND" varchar(1) NOT NULL default ' ',
  "AAABHIGV" varchar(10) NOT NULL default ' ',
  "AAABKNFK" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKRND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIGZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKRND" varchar(1) NOT NULL default ' ',
  "AAABHIHD" varchar(8) NOT NULL default ' ',
  "AAABHIHE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKRND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIHI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHIHQ" varchar(1) NOT NULL default ' ',
  "AAABHIJY" varchar(10) NOT NULL default ' ',
  "AAABHIHN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHIHQ","AAABHIJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIHS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABIFJO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCPX","AAABIFJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABIFJO" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCPX","AAABIFJO","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIIJ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIMZU" varchar(22) NOT NULL default ' ',
  "AAABHIIM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABIMZU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIIP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAABHIKM" varchar(10) NOT NULL default ' ',
  "AAABHIJH" varchar(4) NOT NULL default '0000',
  "AAABHIIU" varchar(1) NOT NULL default ' ',
  "AAABLPNX" varchar(2) NOT NULL default ' ',
  "AAABLOBA" varchar(8) NOT NULL default '00000000',
  "AAABHIIX" varchar(8) NOT NULL default '00000000',
  "AAABHIIY" varchar(1) NOT NULL default ' ',
  "AAABHIIZ" varchar(8) NOT NULL default '00000000',
  "AAABMQCE" varchar(10) NOT NULL default ' ',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABKNBI" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPMM","AAABHIKM","AAABHIJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIJI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABHIJV" varchar(6) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABHIJX" varchar(15) NOT NULL default ' ',
  "AAABHIJY" varchar(10) NOT NULL default ' ',
  "AAABJDPL" varchar(8) NOT NULL default '00000000',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  "AAABHIJR" varchar(1) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMEKQ","AAABHIJV","AAABMPTW","AAABHIJX","AAABHIJY","AAABJDPL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIKA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABKJVG" varchar(10) NOT NULL default ' ',
  "AAABMQCE" varchar(10) NOT NULL default ' ',
  "AAABLTXD" varchar(6) NOT NULL default '000000',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABHIOC" varchar(15) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAABIYVH" varchar(4) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABHIKM" varchar(10) NOT NULL default ' ',
  "AAABHIKN" varchar(6) NOT NULL default '000000',
  "AAABHIKO" varchar(16) NOT NULL default ' ',
  "AAABHIKP" varchar(10) NOT NULL default ' ',
  "AAABHIKQ" varchar(8) NOT NULL default ' ',
  "AAABHIKR" varchar(8) NOT NULL default ' ',
  "AAABHIKS" varchar(15) NOT NULL default ' ',
  "AAABHIKT" varchar(15) NOT NULL default ' ',
  "AAABHIKU" varchar(15) NOT NULL default ' ',
  "AAABHIKV" varchar(15) NOT NULL default ' ',
  "AAABHIKW" varchar(1) NOT NULL default ' ',
  "AAABHIKX" varchar(1) NOT NULL default ' ',
  "AAABHIKY" varchar(1) NOT NULL default ' ',
  "AAABHIKZ" varchar(2) NOT NULL default '00',
  "AAABHILA" varchar(2) NOT NULL default '00',
  "AAABHILB" varchar(2) NOT NULL default '00',
  "AAABHILC" varchar(2) NOT NULL default '00',
  "AAABHILD" varchar(1) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABHILF" varchar(10) NOT NULL default ' ',
  "AAABMKAH" varchar(13) NOT NULL default ' ',
  "AAABMKAI" varchar(3) NOT NULL default ' ',
  "AAABKGMO" varchar(15) NOT NULL default ' ',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABHILK" varchar(3) NOT NULL default ' ',
  "AAABHILL" varchar(15) NOT NULL default ' ',
  "AAABHILM" varchar(15) NOT NULL default ' ',
  "AAABHILN" varchar(1) NOT NULL default ' ',
  "AAABHILO" varchar(1) NOT NULL default ' ',
  "AAABHILP" varchar(1) NOT NULL default ' ',
  "AAABHILQ" varchar(1) NOT NULL default ' ',
  "AAABHILR" varchar(15) NOT NULL default ' ',
  "AAABHILS" varchar(15) NOT NULL default ' ',
  "AAABHILT" varchar(15) NOT NULL default ' ',
  "AAABHILU" varchar(15) NOT NULL default ' ',
  "AAABHILV" varchar(15) NOT NULL default ' ',
  "AAABHILW" varchar(15) NOT NULL default ' ',
  "AAABHILX" varchar(15) NOT NULL default ' ',
  "AAABHILY" varchar(15) NOT NULL default ' ',
  "AAABHILZ" varchar(15) NOT NULL default ' ',
  "AAABHIMA" varchar(15) NOT NULL default ' ',
  "AAABHIMB" varchar(15) NOT NULL default ' ',
  "AAABHIMC" varchar(15) NOT NULL default ' ',
  "AAABHIMD" varchar(15) NOT NULL default ' ',
  "AAABHIME" varchar(15) NOT NULL default ' ',
  "AAABHIMF" varchar(15) NOT NULL default ' ',
  "AAABHIMG" varchar(15) NOT NULL default ' ',
  "AAABHIMH" varchar(15) NOT NULL default ' ',
  "AAABHIMI" varchar(15) NOT NULL default ' ',
  "AAABHIMJ" varchar(15) NOT NULL default ' ',
  "AAABHIMK" varchar(15) NOT NULL default ' ',
  "AAABHIML" varchar(15) NOT NULL default ' ',
  "AAABHIMM" varchar(15) NOT NULL default ' ',
  "AAABHIMN" varchar(15) NOT NULL default ' ',
  "AAABHIMO" varchar(15) NOT NULL default ' ',
  "AAABHIMP" varchar(15) NOT NULL default ' ',
  "AAABHIMQ" varchar(15) NOT NULL default ' ',
  "AAABHIMR" varchar(15) NOT NULL default ' ',
  "AAABHIMS" varchar(15) NOT NULL default ' ',
  "AAABHIMT" varchar(15) NOT NULL default ' ',
  "AAABHIMU" varchar(15) NOT NULL default ' ',
  "AAABHIMV" varchar(15) NOT NULL default ' ',
  "AAABHIMW" varchar(15) NOT NULL default ' ',
  "AAABHIMX" varchar(2) NOT NULL default ' ',
  "AAABHIMY" varchar(2) NOT NULL default ' ',
  "AAABHIMZ" varchar(15) NOT NULL default ' ',
  "AAABHINA" varchar(15) NOT NULL default ' ',
  "AAABHINB" varchar(15) NOT NULL default ' ',
  "AAABHINC" varchar(15) NOT NULL default ' ',
  "AAABHIND" varchar(15) NOT NULL default ' ',
  "AAABHINE" varchar(15) NOT NULL default ' ',
  "AAABHINF" varchar(15) NOT NULL default ' ',
  "AAABHING" varchar(15) NOT NULL default ' ',
  "AAABHINH" varchar(15) NOT NULL default ' ',
  "AAABHINI" varchar(15) NOT NULL default ' ',
  "AAABHINJ" varchar(2) NOT NULL default ' ',
  "AAABHINK" varchar(2) NOT NULL default ' ',
  "AAABHINL" varchar(2) NOT NULL default ' ',
  "AAABHINM" varchar(2) NOT NULL default ' ',
  "AAABHINN" varchar(2) NOT NULL default ' ',
  "AAABHINO" varchar(2) NOT NULL default ' ',
  "AAABHINP" varchar(2) NOT NULL default ' ',
  "AAABHINQ" varchar(2) NOT NULL default ' ',
  "AAABHINR" varchar(2) NOT NULL default ' ',
  "AAABHINS" varchar(2) NOT NULL default ' ',
  "AAABHINT" varchar(25) NOT NULL default ' ',
  "AAABHINU" varchar(2) NOT NULL default ' ',
  "AAABHINV" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQCD","AAABKJVG","AAABMQCE","AAABLTXD","AAABMPTW","AAABHIOC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIOD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHIOI" varchar(4) NOT NULL default '0000',
  "AAABHIOG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHIOI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQRG" (
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABHIOL" varchar(10) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABHION" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQCD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIOP" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIOU" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABHIOX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMQCD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIPA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQCD","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIPJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLYTF" varchar(6) NOT NULL default ' ',
  "AAABHIPW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAABLYTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIPR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABHIPZ" varchar(3) NOT NULL default ' ',
  "AAABLYTF" varchar(6) NOT NULL default ' ',
  "AAABHIPW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAABHIPZ","AAABLYTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIQB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABHIQI" varchar(10) NOT NULL default ' ',
  "AAABLYTF" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAABHIQI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIQJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHIRJ" varchar(4) NOT NULL default ' ',
  "AAABHIQM" varchar(30) NOT NULL default ' ',
  "AAABHIQN" varchar(30) NOT NULL default ' ',
  "AAABHIQO" varchar(30) NOT NULL default ' ',
  "AAABHIQP" varchar(30) NOT NULL default ' ',
  "AAABHIQQ" varchar(30) NOT NULL default ' ',
  "AAABHIQR" varchar(4) NOT NULL default ' ',
  "AAABHIQS" varchar(30) NOT NULL default ' ',
  "AAABHIQT" varchar(30) NOT NULL default ' ',
  "AAABHIQU" varchar(10) NOT NULL default ' ',
  "AAABHIQV" varchar(30) NOT NULL default ' ',
  "AAABHIQW" varchar(30) NOT NULL default ' ',
  "AAABHIQX" varchar(30) NOT NULL default ' ',
  "AAABHIQY" varchar(30) NOT NULL default ' ',
  "AAABHIQZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABHIRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIRC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIGKR" varchar(1) NOT NULL default ' ',
  "AAABHIRJ" varchar(4) NOT NULL default ' ',
  "AAABJLPZ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIGKR","AAABHIRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIRK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHIRM" varchar(6) NOT NULL default ' ',
  "AAABHIRN" varchar(6) NOT NULL default ' ',
  "AAABHIUU" varchar(1) NOT NULL default ' ',
  "AAABHIRP" varchar(1) NOT NULL default ' ',
  "AAABHIRQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIRS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHIUT" varchar(6) NOT NULL default ' ',
  "AAABHIRV" varchar(6) NOT NULL default '000000',
  "AAABHIUU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABHIUT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIRZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHIUT" varchar(6) NOT NULL default ' ',
  "AAABHISJ" varchar(4) NOT NULL default '0000',
  "AAABLRXN" varchar(8) NOT NULL default '00000000',
  "AAABHISZ" varchar(6) NOT NULL default '000000',
  "AAABLRXO" varchar(8) NOT NULL default '00000000',
  "AAABLKLU" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUXQ","AAABHIUT","AAABHISJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHISK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABHIUT" varchar(6) NOT NULL default ' ',
  "AAABLLLP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLFZK","AAABHIUT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHISS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHIUT" varchar(6) NOT NULL default ' ',
  "AAABKMOR" varchar(4) NOT NULL default '0000',
  "AAABKKBI" varchar(2) NOT NULL default '00',
  "AAABHISX" varchar(1) NOT NULL default '0',
  "AAABHISY" varchar(1) NOT NULL default '0',
  "AAABHISZ" varchar(6) NOT NULL default '000000',
  "AAABHITA" varchar(2) NOT NULL default '00',
  "AAABHITB" varchar(1) NOT NULL default '0',
  "AAABHITC" varchar(1) NOT NULL default '0',
  "AAABLKLU" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUXQ","AAABHIUT","AAABKMOR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHITH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABHITU" varchar(20) NOT NULL default ' ',
  "AAABLYTF" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAABHITU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHITP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLYTF" varchar(6) NOT NULL default ' ',
  "AAABHITY" varchar(1) NOT NULL default ' ',
  "AAABHITU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAABLYTF","AAABHITY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHITZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHIUS" varchar(6) NOT NULL default ' ',
  "AAABHIUC" varchar(6) NOT NULL default '000000',
  "AAABHIUD" varchar(1) NOT NULL default ' ',
  "AAABHIUU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABHIUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIUH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABHIUS" varchar(6) NOT NULL default ' ',
  "AAABLLLP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLFZK","AAABHIUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIUP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLYTF" varchar(6) NOT NULL default ' ',
  "AAABHIUS" varchar(6) NOT NULL default ' ',
  "AAABHIUT" varchar(6) NOT NULL default ' ',
  "AAABHIUU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLYTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIUX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLYTF" varchar(6) NOT NULL default ' ',
  "AAABLLLP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABLYTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIVF" (
  "AAABHJRW" varchar(4) NOT NULL default '0000',
  "AAABHIVM" varchar(2) NOT NULL default '00',
  "AAABHIVI" varchar(30) NOT NULL default ' ',
  "AAABLUTZ" varchar(1) NOT NULL default ' ',
  "AAABHIVK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHJRW","AAABHIVM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIVN" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHJRW" varchar(4) NOT NULL default '0000',
  "AAABHIVQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABHJRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIVT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABHJRW" varchar(4) NOT NULL default '0000',
  "AAABMKAI" varchar(4) NOT NULL default '0000',
  "AAABHIVY" varchar(8) NOT NULL default '00000000',
  "AAABHIVZ" varchar(8) NOT NULL default '00000000',
  "AAABMLPO" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABLZKF","AAABHJRW","AAABMKAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIWF" (
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABHIXQ" varchar(32) NOT NULL default ' ',
  "AAABIJQL" varchar(30) NOT NULL default ' ',
  "AAABHIWJ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLUER","AAABHIXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIWM" (
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABHIWR" varchar(32) NOT NULL default ' ',
  "AAABHIWS" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER","AAABHIWR","AAABHIWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIWT" (
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABHIXQ" varchar(32) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHIWX" varchar(30) NOT NULL default ' ',
  "AAABHIWY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER","AAABHIXQ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIXC" (
  "AAABHIXQ" varchar(32) NOT NULL default ' ',
  "AAABMBXE" varchar(1) NOT NULL default '0',
  "AAABHIXI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHIXQ","AAABMBXE","AAABHIXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIXJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABHIXQ" varchar(32) NOT NULL default ' ',
  "AAABHIXN" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLUER","AAABHIXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIXR" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMKFH" varchar(20) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMDEN","AAABIRUN","AAABMNYT","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIYB" (
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAABHIYE" varchar(8) NOT NULL default '00000000',
  "AAABHIYF" varchar(6) NOT NULL default '000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABHIYH" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIYJ" (
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMKFH" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRFQ","AAABMDEN","AAABIRUN","AAABMNYT","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIYU" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMKFH" varchar(20) NOT NULL default ' ',
  "AAABMJVD" varchar(60) NOT NULL default ' ',
  "AAABMJVE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMDEN","AAABIRUN","AAABMNYT","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIZH" (
  "AAABHIZJ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABHIZJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIZK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHJRW" varchar(4) NOT NULL default '0000',
  "AAABMKAI" varchar(4) NOT NULL default '0000',
  "AAABHJAF" varchar(8) NOT NULL default '00000000',
  "AAABMLPO" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABHJRW","AAABMKAI","AAABHJAF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHIZU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHJRW" varchar(4) NOT NULL default '0000',
  "AAABMKAI" varchar(4) NOT NULL default '0000',
  "AAABHJAF" varchar(8) NOT NULL default '00000000',
  "AAABLITM" varchar(64) NOT NULL default ' ',
  "AAABHJAH" varchar(4) NOT NULL default '0000',
  "AAABMLPO" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABHJRW","AAABMKAI","AAABHJAF","AAABLITM","AAABHJAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJAI" (
  "AAABHJEC" varchar(10) NOT NULL default ' ',
  "AAABHJED" varchar(10) NOT NULL default ' ',
  "AAABHJEE" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABHJEC","AAABHJED","AAABHJEE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJAP" (
  "AAABIMWL" varchar(2) NOT NULL default ' ',
  "AAABMMCV" varchar(132) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default '0',
  "AAABMFSJ" varchar(1) NOT NULL default '0',
  "AAABLTTF" varchar(1) NOT NULL default '0',
  "AAABHJAV" varchar(1) NOT NULL default '0',
  "AAABHJAW" varchar(1) NOT NULL default '0',
  "AAABMFSI" varchar(1) NOT NULL default '0',
  "AAABHJAY" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABIMWL","AAABMMCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJBB" (
  "AAABHJRW" varchar(4) NOT NULL default '0000',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABHJBF" varchar(8) NOT NULL default '00000000',
  "AAABHJBG" varchar(6) NOT NULL default '000000',
  "AAABKWFM" varchar(8) NOT NULL default '00000000',
  "AAABLTLM" varchar(6) NOT NULL default '000000',
  "AAABMKKB" varchar(8) NOT NULL default '00000000',
  "AAABLTLN" varchar(6) NOT NULL default '000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABHJBM" varchar(32) NOT NULL default ' ',
  "AAABHJBN" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABHJRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJBP" (
  "AAABHJEC" varchar(10) NOT NULL default ' ',
  "AAABHJED" varchar(10) NOT NULL default ' ',
  "AAABHJEE" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABHJEC","AAABHJED","AAABHJEE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJBW" (
  "AAABHJRW" varchar(4) NOT NULL default '0000',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABHJRW","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJCI" (
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABLUVJ" varchar(32) NOT NULL default ' ',
  "AAABJELB" varchar(2) NOT NULL default ' ',
  "AAABHJDB" varchar(21) NOT NULL default ' ',
  "AAABHJCN" varchar(8) NOT NULL default '00000000',
  "AAABHJCO" varchar(8) NOT NULL default '00000000',
  "AAABHJCP" varchar(6) NOT NULL default '000000',
  "AAABHJCQ" int(11) NOT NULL default '0',
  "AAABHJCR" int(11) NOT NULL default '0',
  "AAABHJCS" int(11) NOT NULL default '0',
  "AAABHJCT" varchar(8) NOT NULL default '00000000',
  "AAABHJCU" varchar(6) NOT NULL default '000000',
  "AAABHJCV" int(11) NOT NULL default '0',
  "AAABHJCW" int(11) NOT NULL default '0',
  "AAABHJCX" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLENH","AAABLUVJ","AAABJELB","AAABHJDB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJDC" (
  "AAABHJDF" varchar(20) NOT NULL default ' ',
  "AAABMMJT" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABHJDF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJDG" (
  "AAABIMVN" varchar(2) NOT NULL default ' ',
  "AAABIMVA" varchar(2) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABIMVC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIMVN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJDM" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIMVN" varchar(2) NOT NULL default ' ',
  "AAABHJDP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIMVN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJDS" (
  "AAABHJDZ" varchar(6) NOT NULL default '000000',
  "AAABHJPS" varchar(8) NOT NULL default '00000000',
  "AAABHJPT" varchar(6) NOT NULL default '000000',
  "AAABHJEC" varchar(10) NOT NULL default ' ',
  "AAABHJED" varchar(10) NOT NULL default ' ',
  "AAABHJEE" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABHJDZ","AAABHJPS","AAABHJPT","AAABHJEC","AAABHJED","AAABHJEE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJEF" (
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABHPJW" varchar(4) NOT NULL default ' ',
  "AAABHJFJ" varchar(4) NOT NULL default '0000',
  "AAABHPJO" varchar(4) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABHPJQ" varchar(4) NOT NULL default '0000',
  "AAABHPJR" int(11) NOT NULL default '0',
  "AAABHPJS" varchar(8) NOT NULL default ' ',
  "AAABHPJT" double NOT NULL default '0',
  PRIMARY KEY ("AAABHPJU","AAABHPJV","AAABHPJW","AAABHJFJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJEU" (
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABHPJW" varchar(4) NOT NULL default ' ',
  "AAABHJFJ" varchar(4) NOT NULL default '0000',
  "AAABHPJO" varchar(4) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABHPJQ" varchar(4) NOT NULL default '0000',
  "AAABHPJR" int(11) NOT NULL default '0',
  "AAABHPJS" varchar(8) NOT NULL default ' ',
  "AAABHPJT" double NOT NULL default '0',
  "AAABMRCK" longtext,
  PRIMARY KEY ("AAABHPJU","AAABHPJV","AAABHPJW","AAABHJFJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJFK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHJFM" smallint(6) NOT NULL default '0',
  "AAABHJFN" smallint(6) NOT NULL default '0',
  "AAABHJFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJFQ" (
  "AAABLLIO" varchar(10) NOT NULL default ' ',
  "AAABHJFS" varchar(30) NOT NULL default ' ',
  "AAABKWXP" varchar(1) NOT NULL default ' ',
  "AAABHJGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJFW" (
  "AAABLLIO" varchar(10) NOT NULL default ' ',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABHJHF" varchar(30) NOT NULL default ' ',
  "AAABHJGM" varchar(30) NOT NULL default ' ',
  "AAABHJHC" varchar(30) NOT NULL default ' ',
  "AAABHJMN" varchar(8) NOT NULL default ' ',
  "AAABHJMM" varchar(8) NOT NULL default ' ',
  "AAABHJLB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLIO"),
  KEY "AAABHJGG" ("AAABHJHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJGI" (
  "AAABLLIO" varchar(10) NOT NULL default ' ',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABHJHF" varchar(30) NOT NULL default ' ',
  "AAABHJGM" varchar(30) NOT NULL default ' ',
  "AAABHJGN" varchar(1) NOT NULL default ' ',
  "AAABHJIC" varchar(30) NOT NULL default ' ',
  "AAABHJIQ" varchar(30) NOT NULL default ' ',
  "AAABHJIR" varchar(30) NOT NULL default ' ',
  "AAABHJLB" varchar(30) NOT NULL default ' ',
  "AAABHJIS" varchar(30) NOT NULL default ' ',
  "AAABHJIT" varchar(30) NOT NULL default ' ',
  "AAABHJIH" varchar(30) NOT NULL default ' ',
  "AAABHJIU" varchar(30) NOT NULL default ' ',
  "AAABHJIJ" varchar(30) NOT NULL default ' ',
  "AAABHJIK" varchar(1) NOT NULL default ' ',
  "AAABHJIL" varchar(1) NOT NULL default ' ',
  "AAABLUZG" varchar(1) NOT NULL default ' ',
  "AAABHJHA" varchar(8) NOT NULL default ' ',
  "AAABHJHB" varchar(8) NOT NULL default ' ',
  "AAABHJHC" varchar(27) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLIO"),
  KEY "AAABHJHE" ("AAABHJHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJHG" (
  "AAABLLIO" varchar(10) NOT NULL default ' ',
  "AAABIJDG" varchar(4) NOT NULL default '0000',
  "AAABHJIK" varchar(1) NOT NULL default ' ',
  "AAABLUZG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJHM" (
  "AAABLLIO" varchar(10) NOT NULL default ' ',
  "AAABHJIQ" varchar(30) NOT NULL default ' ',
  "AAABHJIR" varchar(30) NOT NULL default ' ',
  "AAABHJIS" varchar(30) NOT NULL default ' ',
  "AAABHJIT" varchar(30) NOT NULL default ' ',
  "AAABHJIU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJHU" (
  "AAABLLGQ" varchar(10) NOT NULL default ' ',
  "AAABLLGS" varchar(10) NOT NULL default ' ',
  "AAABHJHX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLGQ","AAABLLGS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJIA" (
  "AAABLLIO" varchar(10) NOT NULL default ' ',
  "AAABHJIC" varchar(30) NOT NULL default ' ',
  "AAABHJIQ" varchar(30) NOT NULL default ' ',
  "AAABHJIR" varchar(30) NOT NULL default ' ',
  "AAABHJIS" varchar(30) NOT NULL default ' ',
  "AAABHJIT" varchar(30) NOT NULL default ' ',
  "AAABHJIH" varchar(30) NOT NULL default ' ',
  "AAABHJIU" varchar(30) NOT NULL default ' ',
  "AAABHJIJ" varchar(30) NOT NULL default ' ',
  "AAABHJIK" varchar(1) NOT NULL default ' ',
  "AAABHJIL" varchar(1) NOT NULL default ' ',
  "AAABLUZG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJIO" (
  "AAABLLIO" varchar(10) NOT NULL default ' ',
  "AAABHJIQ" varchar(30) NOT NULL default ' ',
  "AAABHJIR" varchar(30) NOT NULL default ' ',
  "AAABHJIS" varchar(30) NOT NULL default ' ',
  "AAABHJIT" varchar(30) NOT NULL default ' ',
  "AAABHJIU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJIW" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLLIO" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLLIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJJC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIJBV" varchar(10) NOT NULL default ' ',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABIJEU" varchar(1) NOT NULL default ' ',
  "AAABHJJH" varchar(1) NOT NULL default ' ',
  "AAABHJJI" varchar(1) NOT NULL default ' ',
  "AAABHJJJ" varchar(1) NOT NULL default ' ',
  "AAABHJJK" varchar(1) NOT NULL default ' ',
  "AAABHJMU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIJBV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJJO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIJBV" varchar(10) NOT NULL default ' ',
  "AAABHJJR" varchar(1) NOT NULL default ' ',
  "AAABHJJS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIJBV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJJV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIJBV" varchar(10) NOT NULL default ' ',
  "AAABHJJY" varchar(1) NOT NULL default ' ',
  "AAABHJJZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIJBV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJKC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIJBV" varchar(10) NOT NULL default ' ',
  "AAABLLIO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIJBV","AAABLLIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJKJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIJBV" varchar(10) NOT NULL default ' ',
  "AAABIJAW" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIJBV","AAABIJAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJKQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIJBV" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIJBV","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJKY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIJBV" varchar(10) NOT NULL default ' ',
  "AAABHJLB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIJBV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJLE" (
  "AAABIJAW" varchar(5) NOT NULL default ' ',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  "AAABHJLH" varchar(1) NOT NULL default ' ',
  "AAABIJAY" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABIJAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJLK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLSCQ" varchar(1) NOT NULL default ' ',
  "AAABHJLO" varchar(6) NOT NULL default '000000',
  "AAABHJLP" varchar(8) NOT NULL default '00000000',
  "AAABHJLQ" varchar(1) NOT NULL default ' ',
  "AAABHJLR" varchar(1) NOT NULL default ' ',
  "AAABHJLS" varchar(1) NOT NULL default ' ',
  "AAABJTIO" varchar(1) NOT NULL default ' ',
  "AAABHJLU" varchar(4) NOT NULL default '0000',
  "AAABHJLV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABLSCQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJLZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABIJEW" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJMG" (
  "AAABHJMU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABHJMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJMJ" (
  "AAABHJMU" varchar(10) NOT NULL default ' ',
  "AAABJSLF" varchar(30) NOT NULL default ' ',
  "AAABHJMM" varchar(30) NOT NULL default ' ',
  "AAABHJMN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHJMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJMP" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHJMU" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABHJMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJMV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHJNQ" varchar(10) NOT NULL default ' ',
  "AAABHJNR" varchar(30) NOT NULL default ' ',
  "AAABHJNT" varchar(10) NOT NULL default ' ',
  "AAABHJNU" varchar(30) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHJNQ","AAABHJNR","AAABHJNT","AAABHJNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJNH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHJNQ" varchar(10) NOT NULL default ' ',
  "AAABHJNR" varchar(30) NOT NULL default ' ',
  "AAABHJNS" varchar(30) NOT NULL default ' ',
  "AAABHJNT" varchar(10) NOT NULL default ' ',
  "AAABHJNU" varchar(30) NOT NULL default ' ',
  "AAABHJNV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHJNQ","AAABHJNR","AAABHJNS","AAABHJNT","AAABHJNU","AAABHJNV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJNW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLILW" varchar(10) NOT NULL default ' ',
  "AAABHJOA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQDU","AAABLILW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJOE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLILW" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLILW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJOJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLILW" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIAOH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLILW","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJOR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHJPA" varchar(8) NOT NULL default '00000000',
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABHJPC" varchar(10) NOT NULL default ' ',
  "AAABHJPD" varchar(8) NOT NULL default '00000000',
  "AAABHJPE" varchar(8) NOT NULL default '00000000',
  "AAABMLPO" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABLUXQ","AAABHJPA","AAABHJPB","AAABHJPC","AAABHJPD","AAABHJPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJPF" (
  "AAABHJPN" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABHJPN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJPI" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHJPN" varchar(6) NOT NULL default ' ',
  "AAABHJPL" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABHJPN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJPO" (
  "AAABLUVB" varchar(10) NOT NULL default ' ',
  "AAABHJQB" varchar(6) NOT NULL default '000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABHJPS" varchar(8) NOT NULL default '00000000',
  "AAABHJPT" varchar(6) NOT NULL default '000000',
  "AAABHJPU" varchar(8) NOT NULL default '00000000',
  "AAABHJPV" varchar(6) NOT NULL default '000000',
  "AAABHJPW" varchar(8) NOT NULL default '00000000',
  "AAABHJPX" varchar(6) NOT NULL default '000000',
  "AAABJFUT" varchar(8) NOT NULL default '00000000',
  "AAABHJPZ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUVB","AAABHJQB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJQC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPVQ" varchar(30) NOT NULL default ' ',
  "AAABIMWL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPVQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJQI" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIMWL" varchar(2) NOT NULL default ' ',
  "AAABIMWG" varchar(30) NOT NULL default ' ',
  "AAABIMWH" varchar(55) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIMWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJQP" (
  "AAABIMWL" varchar(2) NOT NULL default ' ',
  "AAABIMVT" varchar(1) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABIMWV" varchar(1) NOT NULL default ' ',
  "AAABLEHK" varchar(1) NOT NULL default ' ',
  "AAABMNMP" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABIMWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJQX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABINIP" varchar(10) NOT NULL default ' ',
  "AAABIMWL" varchar(2) NOT NULL default ' ',
  "AAABIMWV" varchar(1) NOT NULL default ' ',
  "AAABLEHK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABINIP","AAABIMWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJRG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABINIP" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIMWL" varchar(2) NOT NULL default ' ',
  "AAABIMWG" varchar(30) NOT NULL default ' ',
  "AAABMNMP" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABINIP","AAABMLQA","AAABIMWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJRR" (
  "AAABHJRW" varchar(4) NOT NULL default '0000',
  "AAABMKAI" varchar(4) NOT NULL default '0000',
  "AAABIMWL" varchar(2) NOT NULL default ' ',
  "AAABHJRV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHJRW","AAABMKAI","AAABIMWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJRZ" (
  "AAABIMVN" varchar(2) NOT NULL default ' ',
  "AAABIMWL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABIMVN","AAABIMWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJSE" (
  "AAABMKAI" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMKAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJSH" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABMKAI" varchar(4) NOT NULL default '0000',
  "AAABHJSK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABMKAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJSN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJSKP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJSKP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJSS" (
  "AAABHJTB" varchar(3) NOT NULL default '000',
  "AAABHJSU" varchar(1) NOT NULL default ' ',
  "AAABHJSV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABHJTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJSX" (
  "AAABHJTB" varchar(3) NOT NULL default '000',
  "AAABHJSZ" varchar(5) NOT NULL default ' ',
  "AAABHJTA" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABHJTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJTC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(10) NOT NULL default '0000000000',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABLUSV" varchar(64) NOT NULL default ' ',
  "AAABINNI" varchar(30) NOT NULL default ' ',
  "AAABINNJ" varchar(254) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLTVR","AAABLTXI","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJTT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(10) NOT NULL default '0000000000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLTVR","AAABLTXI","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJUD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(10) NOT NULL default '0000000000',
  "AAABMBXE" varchar(1) NOT NULL default '0',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABLUSV" varchar(64) NOT NULL default ' ',
  "AAABINNI" varchar(30) NOT NULL default ' ',
  "AAABINNJ" varchar(254) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLTVR","AAABLTXI","AAABMBXE","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJUW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(10) NOT NULL default '0000000000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMBXE" varchar(1) NOT NULL default '0',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLTVR","AAABLTXI","AAABMLQA","AAABMBXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJVI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHJWN" varchar(20) NOT NULL default ' ',
  "AAABLITM" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJVO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHJWN" varchar(20) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHJWN","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJVW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHJWN" varchar(20) NOT NULL default ' ',
  "AAABMLPO" int(11) NOT NULL default '0',
  "AAABHJWA" varchar(75) NOT NULL default ' ',
  "AAABHJWB" varchar(1) NOT NULL default ' ',
  "AAABMLZS" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABHJWN","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJWG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHJWN" varchar(20) NOT NULL default ' ',
  "AAABMLPO" int(11) NOT NULL default '0',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHJWN","AAABMLPO","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJWQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(10) NOT NULL default '0000000000',
  "AAABMBXE" varchar(1) NOT NULL default '0',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABLUSV" varchar(64) NOT NULL default ' ',
  "AAABINNI" varchar(30) NOT NULL default ' ',
  "AAABINNJ" varchar(254) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLTVR","AAABLTXI","AAABMBXE","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJXJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(10) NOT NULL default '0000000000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMBXE" varchar(1) NOT NULL default '0',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLTVR","AAABLTXI","AAABMLQA","AAABMBXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJXV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABHJZT" varchar(32) NOT NULL default ' ',
  "AAABMLPO" varchar(8) NOT NULL default '00000000',
  "AAABHKAA" varchar(32) NOT NULL default ' ',
  "AAABHKAB" varchar(10) NOT NULL default '0000000000',
  "AAABHKAC" varchar(32) NOT NULL default ' ',
  "AAABHKAD" varchar(10) NOT NULL default '0000000000',
  "AAABHJZU" varchar(32) NOT NULL default ' ',
  "AAABJSEJ" varchar(10) NOT NULL default '0000000000',
  "AAABLUFA" varchar(10) NOT NULL default ' ',
  "AAABHJYH" varchar(12) NOT NULL default ' ',
  "AAABHJYI" varchar(10) NOT NULL default ' ',
  "AAABHJYJ" varchar(241) NOT NULL default ' ',
  "AAABHJYK" varchar(80) NOT NULL default ' ',
  "AAABHKGK" varchar(8) NOT NULL default '00000000',
  "AAABHKGL" varchar(6) NOT NULL default '000000',
  "AAABHKGM" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLUER","AAABHJZT","AAABMLPO"),
  KEY "AAABHJYS" ("AAABLUXQ","AAABHKAA","AAABHKAC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHJYW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABHJZT" varchar(32) NOT NULL default ' ',
  "AAABMLPO" varchar(16) NOT NULL default ' ',
  "AAABHKAA" varchar(32) NOT NULL default ' ',
  "AAABHKAB" varchar(10) NOT NULL default '0000000000',
  "AAABHKAC" varchar(32) NOT NULL default ' ',
  "AAABHKAD" varchar(10) NOT NULL default '0000000000',
  "AAABHJZF" varchar(32) NOT NULL default ' ',
  "AAABHJZG" varchar(10) NOT NULL default '0000000000',
  "AAABHJZU" varchar(32) NOT NULL default ' ',
  "AAABJSEJ" varchar(10) NOT NULL default '0000000000',
  "AAABHJZY" varchar(32) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABLRLL" decimal(21,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABLUXQ","AAABLUER","AAABHJZT","AAABMLPO"),
  KEY "AAABHJZQ" ("AAABLUXQ","AAABLUER","AAABHJZT","AAABHJZU"),
  KEY "AAABHJZV" ("AAABLUXQ","AAABLUER","AAABHJZY"),
  KEY "AAABHJZZ" ("AAABHKAA","AAABHKAB","AAABHKAC","AAABHKAD","AAABLRLL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(10) NOT NULL default '0000000000',
  "AAABMBXE" varchar(1) NOT NULL default '0',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABLUSV" varchar(64) NOT NULL default ' ',
  "AAABINNI" varchar(30) NOT NULL default ' ',
  "AAABINNJ" varchar(254) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLTVR","AAABLTXI","AAABMBXE","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKAY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(10) NOT NULL default '0000000000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMBXE" varchar(1) NOT NULL default '0',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLTVR","AAABLTXI","AAABMLQA","AAABMBXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKBK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHKCV" varchar(32) NOT NULL default ' ',
  "AAABHKCW" varchar(10) NOT NULL default '0000000000',
  "AAABMBXE" varchar(1) NOT NULL default '0',
  "AAABHMRI" varchar(32) NOT NULL default ' ',
  "AAABHKCZ" varchar(32) NOT NULL default ' ',
  "AAABHKBR" varchar(10) NOT NULL default ' ',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABHKBT" varchar(60) NOT NULL default ' ',
  "AAABHKBU" varchar(60) NOT NULL default ' ',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABLUSV" varchar(64) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABHKCV","AAABHKCW","AAABMBXE","AAABHMRI","AAABHKCZ"),
  KEY "AAABHKCE" ("AAABMKFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKCG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHKCV" varchar(32) NOT NULL default ' ',
  "AAABHKCW" varchar(10) NOT NULL default '0000000000',
  "AAABMBXE" varchar(1) NOT NULL default '0',
  "AAABHMRI" varchar(32) NOT NULL default ' ',
  "AAABHKCZ" varchar(32) NOT NULL default ' ',
  "AAABHKDA" varchar(60) NOT NULL default ' ',
  "AAABHKCO" varchar(60) NOT NULL default ' ',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABLUSV" varchar(64) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABHKCS" varchar(6) NOT NULL default ' ',
  "AAABLNGY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHKCV","AAABHKCW","AAABMBXE","AAABHMRI","AAABHKCZ","AAABHKDA"),
  KEY "AAABHKDB" ("AAABMKFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKDD" (
  "AAABINNI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABINNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKDG" (
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABHKDI" varchar(30) NOT NULL default ' ',
  "AAABHKDJ" varchar(32) NOT NULL default ' ',
  "AAABHKDK" varchar(10) NOT NULL default ' ',
  "AAABHKDL" varchar(10) NOT NULL default ' ',
  "AAABHKDM" varchar(25) NOT NULL default ' ',
  "AAABHKDN" varchar(25) NOT NULL default ' ',
  "AAABHKDO" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKDQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABLUXQ","AAABLTVR","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKDX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(10) NOT NULL default '0000000000',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABLUSV" varchar(64) NOT NULL default ' ',
  "AAABINNI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTVR","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKEH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(10) NOT NULL default '0000000000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLTVR","AAABLTXI","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKER" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHKFN" varchar(6) NOT NULL default ' ',
  "AAABHKEU" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABHKFN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKEX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMBXE" varchar(1) NOT NULL default '0',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(10) NOT NULL default '0000000000',
  "AAABLRQP" varchar(30) NOT NULL default ' ',
  "AAABLEEW" varchar(30) NOT NULL default ' ',
  "AAABHKFN" varchar(6) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMBXE","AAABLTVR","AAABLTXI","AAABLRQP","AAABLEEW","AAABHKFN","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKFP" (
  "AAABMBXE" varchar(1) NOT NULL default '0',
  "AAABHKFW" varchar(40) NOT NULL default ' ',
  "AAABLRQP" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXE","AAABHKFW","AAABLRQP","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKFZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABHKGP" varchar(32) NOT NULL default ' ',
  "AAABHKGQ" varchar(10) NOT NULL default '0000000000',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABMMJT" varchar(40) NOT NULL default ' ',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  "AAABHKGV" varchar(32) NOT NULL default ' ',
  "AAABHKGW" varchar(32) NOT NULL default ' ',
  "AAABHKGJ" varchar(32) NOT NULL default ' ',
  "AAABHKGK" varchar(8) NOT NULL default '00000000',
  "AAABHKGL" varchar(6) NOT NULL default '000000',
  "AAABHKGM" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLUER","AAABHKGP","AAABHKGQ","AAABLTVR"),
  KEY "AAABHKGS" ("AAABLUXQ","AAABLUER","AAABHKGV","AAABHKGW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKGX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIMWV" varchar(2) NOT NULL default ' ',
  "AAABIMWR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIMWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKHD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABINIP" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIMWV" varchar(2) NOT NULL default ' ',
  "AAABIMWR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABINIP","AAABMLQA","AAABIMWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKHN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHKHZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHKHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKHS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHKHZ" varchar(4) NOT NULL default ' ',
  "AAABHKHW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHKHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKIA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHKIM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHKIM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKIF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHKIM" varchar(4) NOT NULL default ' ',
  "AAABHKIJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHKIM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKIN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYWK" varchar(2) NOT NULL default ' ',
  "AAABHKIQ" varchar(1) NOT NULL default ' ',
  "AAABHKIR" varchar(1) NOT NULL default ' ',
  "AAABHKIS" varchar(1) NOT NULL default ' ',
  "AAABLXUC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYWK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLYWK" varchar(2) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLYWK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKJE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABMDBN" varchar(4) NOT NULL default ' ',
  "AAABHLNU" varchar(4) NOT NULL default ' ',
  "AAABKPXT" varchar(4) NOT NULL default ' ',
  "AAABMLHO" varchar(4) NOT NULL default ' ',
  "AAABHYJM" varchar(2) NOT NULL default ' ',
  "AAABHYJN" varchar(2) NOT NULL default ' ',
  "AAABHOOP" varchar(6) NOT NULL default '000000',
  "AAABHOOJ" varchar(20) NOT NULL default ' ',
  "AAABLNZH" varchar(2) NOT NULL default ' ',
  "AAABHOOL" varchar(20) NOT NULL default ' ',
  "AAABHKJR" varchar(1) NOT NULL default ' ',
  "AAABLKXQ" varchar(4) NOT NULL default ' ',
  "AAABLXHW" varchar(2) NOT NULL default ' ',
  "AAABLXHV" varchar(2) NOT NULL default ' ',
  "AAABMAGA" varchar(1) NOT NULL default ' ',
  "AAABHKJW" varchar(1) NOT NULL default ' ',
  "AAABHKJX" varchar(1) NOT NULL default ' ',
  "AAABHKJY" varchar(1) NOT NULL default ' ',
  "AAABHKJZ" varchar(1) NOT NULL default ' ',
  "AAABHOSP" varchar(8) NOT NULL default ' ',
  "AAABHODI" varchar(1) NOT NULL default ' ',
  "AAABHRBN" varchar(4) NOT NULL default ' ',
  "AAABHOCQ" varchar(1) NOT NULL default ' ',
  "AAABLXQS" varchar(3) NOT NULL default '000',
  "AAABLZUS" varchar(1) NOT NULL default ' ',
  "AAABLXIP" varchar(2) NOT NULL default ' ',
  "AAABHODA" varchar(1) NOT NULL default ' ',
  "AAABLYCG" varchar(2) NOT NULL default ' ',
  "AAABLUKA" varchar(1) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABJLIP" varchar(2) NOT NULL default ' ',
  "AAABHKKM" varchar(1) NOT NULL default ' ',
  "AAABHNCG" varchar(1) NOT NULL default ' ',
  "AAABHKKO" varchar(1) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAABHOPE" varchar(2) NOT NULL default ' ',
  "AAABHPOY" varchar(6) NOT NULL default ' ',
  "AAABHKKV" varchar(6) NOT NULL default ' ',
  "AAABHKKW" varchar(6) NOT NULL default ' ',
  "AAABHKKX" varchar(4) NOT NULL default ' ',
  "AAABHKKY" varchar(2) NOT NULL default ' ',
  "AAABHKKZ" varchar(10) NOT NULL default ' ',
  "AAABHKLA" varchar(10) NOT NULL default ' ',
  "AAABLXKA" varchar(1) NOT NULL default ' ',
  "AAABLXKB" varchar(3) NOT NULL default ' ',
  "AAABLXKC" varchar(1) NOT NULL default ' ',
  "AAABLYDD" varchar(1) NOT NULL default ' ',
  "AAABHKLF" varchar(1) NOT NULL default ' ',
  "AAABHKLG" decimal(2,0) NOT NULL default '0',
  "AAABHKLH" varchar(1) NOT NULL default ' ',
  "AAABHKLI" varchar(1) NOT NULL default ' ',
  "AAABHKLJ" varchar(1) NOT NULL default ' ',
  "AAABHKLK" varchar(1) NOT NULL default ' ',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABHKLM" varchar(1) NOT NULL default ' ',
  "AAABHKLN" varchar(4) NOT NULL default ' ',
  "AAABHKLO" varchar(4) NOT NULL default ' ',
  "AAABKRBX" varchar(4) NOT NULL default ' ',
  "AAABKODR" varchar(4) NOT NULL default ' ',
  "AAABHKLR" varchar(1) NOT NULL default ' ',
  "AAABKPVV" varchar(2) NOT NULL default ' ',
  "AAABHKLT" varchar(4) NOT NULL default ' ',
  "AAABHOPM" varchar(1) NOT NULL default ' ',
  "AAABHKLV" varchar(1) NOT NULL default ' ',
  "AAABHKLW" varchar(6) NOT NULL default ' ',
  "AAABHKLX" varchar(6) NOT NULL default ' ',
  "AAABHKLY" varchar(2) NOT NULL default ' ',
  "AAABHKLZ" varchar(4) NOT NULL default ' ',
  "AAABHKMA" varchar(1) NOT NULL default ' ',
  "AAABHKMB" varchar(2) NOT NULL default ' ',
  "AAABHKMC" varchar(1) NOT NULL default ' ',
  "AAABLXKX" varchar(4) NOT NULL default ' ',
  "AAABHKME" varchar(1) NOT NULL default ' ',
  "AAABHKMF" varchar(1) NOT NULL default ' ',
  "AAABLXKY" varchar(1) NOT NULL default ' ',
  "AAABHKMH" varchar(2) NOT NULL default ' ',
  "AAABHKMI" varchar(1) NOT NULL default ' ',
  "AAABLXIN" varchar(1) NOT NULL default ' ',
  "AAABHKMK" varchar(2) NOT NULL default ' ',
  "AAABHKML" varchar(6) NOT NULL default '000000',
  "AAABHKMM" varchar(2) NOT NULL default ' ',
  "AAABHKMN" varchar(1) NOT NULL default ' ',
  "AAABHKMO" varchar(1) NOT NULL default ' ',
  "AAABHKMP" varchar(2) NOT NULL default ' ',
  "AAABHOPV" varchar(1) NOT NULL default ' ',
  "AAABHOPW" varchar(1) NOT NULL default ' ',
  "AAABHOPX" varchar(1) NOT NULL default ' ',
  "AAABHKMT" varchar(1) NOT NULL default ' ',
  "AAABHKMU" varchar(1) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAABHKMW" varchar(4) NOT NULL default ' ',
  "AAABHKMX" varchar(4) NOT NULL default ' ',
  "AAABHKMY" varchar(1) NOT NULL default ' ',
  "AAABLEST" varchar(30) NOT NULL default ' ',
  "AAABHKNA" varchar(1) NOT NULL default ' ',
  "AAABHKNB" varchar(1) NOT NULL default ' ',
  "AAABHKNC" varchar(1) NOT NULL default ' ',
  "AAABHKND" varchar(2) NOT NULL default ' ',
  "AAABHMYG" varchar(14) NOT NULL default ' ',
  "AAABHKNF" varchar(2) NOT NULL default ' ',
  "AAABHKYN" varchar(4) NOT NULL default ' ',
  "AAABHKNH" varchar(1) NOT NULL default ' ',
  "AAABHKNI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKNL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKNT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLGN" varchar(4) NOT NULL default ' ',
  "AAABLXOG" varchar(1) NOT NULL default ' ',
  "AAABJJXP" varchar(1) NOT NULL default ' ',
  "AAABMDBO" varchar(4) NOT NULL default ' ',
  "AAABLKXQ" varchar(4) NOT NULL default ' ',
  "AAABJAHB" varchar(4) NOT NULL default ' ',
  "AAABHKOB" varchar(1) NOT NULL default ' ',
  "AAABHKOC" varchar(1) NOT NULL default ' ',
  "AAABHKOD" varchar(1) NOT NULL default ' ',
  "AAABHKOE" varchar(1) NOT NULL default ' ',
  "AAABLXUC" varchar(1) NOT NULL default ' ',
  "AAABHOSN" varchar(4) NOT NULL default ' ',
  "AAABLZAP" varchar(2) NOT NULL default ' ',
  "AAABHKOI" varchar(1) NOT NULL default ' ',
  "AAABLXQI" varchar(1) NOT NULL default ' ',
  "AAABHOSP" varchar(8) NOT NULL default ' ',
  "AAABHKOL" varchar(1) NOT NULL default ' ',
  "AAABHKOM" varchar(1) NOT NULL default ' ',
  "AAABLYCG" varchar(2) NOT NULL default ' ',
  "AAABLXOE" varchar(1) NOT NULL default ' ',
  "AAABLXOF" varchar(1) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABHKOR" varchar(1) NOT NULL default ' ',
  "AAABHKOS" varchar(1) NOT NULL default ' ',
  "AAABHKOT" varchar(1) NOT NULL default ' ',
  "AAABJLIP" varchar(2) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABHKOW" varchar(1) NOT NULL default ' ',
  "AAABHOSU" varchar(2) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABLXSU" varchar(1) NOT NULL default ' ',
  "AAABLXTL" varchar(1) NOT NULL default ' ',
  "AAABHQRM" varchar(2) NOT NULL default ' ',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABHKPD" varchar(1) NOT NULL default ' ',
  "AAABHKPE" varchar(1) NOT NULL default ' ',
  "AAABKPVV" varchar(2) NOT NULL default ' ',
  "AAABHOSY" varchar(1) NOT NULL default ' ',
  "AAABHOSZ" varchar(1) NOT NULL default ' ',
  "AAABHKPI" varchar(4) NOT NULL default ' ',
  "AAABHQRL" varchar(1) NOT NULL default ' ',
  "AAABHKPK" varchar(1) NOT NULL default ' ',
  "AAABHKPL" varchar(1) NOT NULL default ' ',
  "AAABHKPM" varchar(18) NOT NULL default ' ',
  "AAABHKPN" varchar(1) NOT NULL default ' ',
  "AAABHOTE" varchar(2) NOT NULL default ' ',
  "AAABHKPP" varchar(1) NOT NULL default ' ',
  "AAABHKPQ" varchar(1) NOT NULL default ' ',
  "AAABHKPR" varchar(1) NOT NULL default ' ',
  "AAABKSTO" varchar(8) NOT NULL default ' ',
  "AAABHKPT" varchar(4) NOT NULL default ' ',
  "AAABHKPU" varchar(4) NOT NULL default ' ',
  "AAABKSTQ" varchar(2) NOT NULL default ' ',
  "AAABKSTR" varchar(1) NOT NULL default ' ',
  "AAABKSTS" varchar(1) NOT NULL default ' ',
  "AAABHOTI" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKQB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLGN" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKQJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHKQV" varchar(26) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHKQV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKQO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHKQV" varchar(26) NOT NULL default ' ',
  "AAABJARB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHKQV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKQW" (
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  "AAABHKRG" varchar(4) NOT NULL default ' ',
  "AAABIMZU" varchar(14) NOT NULL default ' ',
  "AAABKZXE" varchar(60) NOT NULL default ' ',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMLPV","AAABHKRG","AAABIMZU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKRI" (
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
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP","AAABLRSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKRZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABLZZR" varchar(4) NOT NULL default ' ',
  "AAABHKSD" varchar(4) NOT NULL default '0000',
  "AAABHKSE" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABLZZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKSI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABLZFC" varchar(4) NOT NULL default ' ',
  "AAABHKSM" varchar(4) NOT NULL default '0000',
  "AAABHKSN" varchar(4) NOT NULL default '0000',
  "AAABHKSO" varchar(1) NOT NULL default ' ',
  "AAABHKSP" varchar(1) NOT NULL default ' ',
  "AAABHKSQ" varchar(1) NOT NULL default ' ',
  "AAABHKSR" varchar(1) NOT NULL default ' ',
  "AAABHKSS" varchar(1) NOT NULL default ' ',
  "AAABHKST" varchar(1) NOT NULL default ' ',
  "AAABHKSU" varchar(1) NOT NULL default ' ',
  "AAABHKSV" varchar(1) NOT NULL default ' ',
  "AAABHKSW" varchar(1) NOT NULL default ' ',
  "AAABHKSX" varchar(1) NOT NULL default ' ',
  "AAABHKSY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZFD","AAABLZFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKTC" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMOIB" varchar(14) NOT NULL default ' ',
  "AAABMOHX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABMMVH","AAABMOIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKTK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABJSKP" varchar(20) NOT NULL default ' ',
  "AAABLENL" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLXIY","AAABJSKP"),
  KEY "AAABHKTS" ("AAABMRIL","AAABJSKP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKTV" (
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLENL" varchar(4) NOT NULL default '0000',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABMNMY" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(45) NOT NULL default ' ',
  "AAABMNNA" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV","AAABMOHM","AAABLENL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKUG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLENL" varchar(4) NOT NULL default '0000',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABMNMY" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(45) NOT NULL default ' ',
  "AAABMNNA" varchar(45) NOT NULL default ' ',
  "AAABHKUP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLPV","AAABMOHM","AAABLENL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKUU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHKVH" varchar(4) NOT NULL default ' ',
  "AAABHKUX" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHKVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKVA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHKVH" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHKVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKVI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHKVP" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHKVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKVQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABHKVT" varchar(1) NOT NULL default ' ',
  "AAABHKVU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMEKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKVX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCFY" varchar(3) NOT NULL default ' ',
  "AAABHKWA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCFY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKWD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMCFY" varchar(3) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABLXIF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABMCFY","AAABMNZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKWT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCFY" varchar(3) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCFY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKXB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHKXN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHKXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKXG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHKXN" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHKXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKXO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXRR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKXT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXRR" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKYB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHKYN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHKYN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKYG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHKYN" varchar(4) NOT NULL default ' ',
  "AAABHKYK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHKYN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKYO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMALV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMALV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKYT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMALV" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMALV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKZB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLDNL" varchar(30) NOT NULL default ' ',
  "AAABJLIY" varchar(30) NOT NULL default ' ',
  "AAABLRBI" varchar(1) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLDNL","AAABJLIY","AAABLRBI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKZN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXID" varchar(4) NOT NULL default ' ',
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABHPXH" varchar(16) NOT NULL default ' ',
  "AAABHPXI" varchar(16) NOT NULL default ' ',
  "AAABHPXJ" varchar(16) NOT NULL default ' ',
  "AAABHPXK" varchar(16) NOT NULL default ' ',
  "AAABHPXX" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXID")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHKZZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXID" varchar(4) NOT NULL default ' ',
  "AAABLXIC" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXID","AAABLXIC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLAG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABHLAU" varchar(8) NOT NULL default '00000000',
  "AAABHLAV" varchar(6) NOT NULL default '000000',
  "AAABHLAW" varchar(12) NOT NULL default ' ',
  "AAABHLAX" varchar(8) NOT NULL default '00000000',
  "AAABHLAY" varchar(6) NOT NULL default '000000',
  "AAABIKFN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLAQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABHLAU" varchar(8) NOT NULL default '00000000',
  "AAABHLAV" varchar(6) NOT NULL default '000000',
  "AAABHLAW" varchar(12) NOT NULL default ' ',
  "AAABHLAX" varchar(8) NOT NULL default '00000000',
  "AAABHLAY" varchar(6) NOT NULL default '000000',
  "AAABIKFN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLBC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJIOZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJIOZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLBH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJIOZ" varchar(3) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJIOZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLBP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJIOX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJIOX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLBU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJIOX" varchar(3) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJIOX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLCC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJIOY" varchar(1) NOT NULL default ' ',
  "AAABHLCF" varchar(1) NOT NULL default ' ',
  "AAABLXFQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJIOY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLCJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJIOY" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJIOY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLCR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJIPA" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJIPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLCW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJIPA" varchar(3) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJIPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLDE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHODG" varchar(3) NOT NULL default '000',
  "AAABJIOY" varchar(1) NOT NULL default ' ',
  "AAABJLCZ" varchar(1) NOT NULL default ' ',
  "AAABMRCU" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHODG","AAABJIOY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLDN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHODG" varchar(3) NOT NULL default '000',
  "AAABJIOY" varchar(1) NOT NULL default ' ',
  "AAABJIOX" varchar(3) NOT NULL default ' ',
  "AAABJIOZ" varchar(3) NOT NULL default ' ',
  "AAABJIPA" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHODG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLDW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHODG" varchar(3) NOT NULL default '000',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHODG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLEE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJIPN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJIPN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLEJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHLFC" varchar(2) NOT NULL default ' ',
  "AAABHLFD" varchar(2) NOT NULL default ' ',
  "AAABHLEN" varchar(3) NOT NULL default '000',
  "AAABHLEO" varchar(1) NOT NULL default ' ',
  "AAABHLEP" varchar(3) NOT NULL default '000',
  "AAABHLEQ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABHLFC","AAABHLFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLEU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHLFC" varchar(2) NOT NULL default ' ',
  "AAABHLFD" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHLFC","AAABHLFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLFE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJIPN" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJIPN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLFM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJIPO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJIPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLFR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJIPO" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJIPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLFZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJIPP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJIPP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLGE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJIPP" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJIPP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLGM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJIPQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJIPQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLGR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJIPQ" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJIPQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLGZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJIPR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJIPR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJIPR" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJIPR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLHM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJIPS" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJIPS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLHR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJIPS" varchar(3) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJIPS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJIPT" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJIPT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLIE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJIPT" varchar(3) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJIPT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLIM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJIPU" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJIPU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLIR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJIPU" varchar(3) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJIPU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLIZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJIPV" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJIPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLJE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQPP" varchar(4) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABHLJI" varchar(4) NOT NULL default ' ',
  "AAABHLJJ" varchar(30) NOT NULL default ' ',
  "AAABHLJK" varchar(32) NOT NULL default ' ',
  "AAABHLJL" varchar(3) NOT NULL default '000',
  "AAABHLJM" varchar(1) NOT NULL default ' ',
  "AAABHLJN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQPP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLJQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLQPP" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLQPP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLJY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJIPV" varchar(3) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJIPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJIPW" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJIPW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLKL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJIPW" varchar(3) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJIPW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJLQV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJLQV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLKY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJLQV" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJLQV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLLG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHLMP" varchar(4) NOT NULL default ' ',
  "AAABHLNT" varchar(4) NOT NULL default ' ',
  "AAABKPXT" varchar(4) NOT NULL default ' ',
  "AAABHLMS" varchar(2) NOT NULL default ' ',
  "AAABMLGN" varchar(4) NOT NULL default ' ',
  "AAABHLNQ" varchar(4) NOT NULL default ' ',
  "AAABHLNB" varchar(3) NOT NULL default '000',
  "AAABHLLP" varchar(3) NOT NULL default '000',
  "AAABHLLQ" varchar(3) NOT NULL default '000',
  "AAABHLLR" varchar(3) NOT NULL default '000',
  "AAABHLLS" varchar(3) NOT NULL default '000',
  "AAABHLLT" varchar(3) NOT NULL default '000',
  "AAABHLNI" varchar(1) NOT NULL default ' ',
  "AAABHLLV" varchar(2) NOT NULL default ' ',
  "AAABHLLW" varchar(1) NOT NULL default ' ',
  "AAABHLLX" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXQG" varchar(1) NOT NULL default ' ',
  "AAABLHZN" varchar(1) NOT NULL default ' ',
  "AAABLXQH" varchar(1) NOT NULL default ' ',
  "AAABHLNE" varchar(3) NOT NULL default '000',
  "AAABHLSE" varchar(1) NOT NULL default ' ',
  "AAABHLNJ" varchar(1) NOT NULL default ' ',
  "AAABHLMF" varchar(3) NOT NULL default '000',
  "AAABHLMG" varchar(1) NOT NULL default ' ',
  "AAABHLMH" varchar(1) NOT NULL default ' ',
  "AAABHLMI" varchar(1) NOT NULL default ' ',
  "AAABHLMJ" varchar(1) NOT NULL default ' ',
  "AAABHLMK" varchar(4) NOT NULL default ' ',
  "AAABHLML" varchar(1) NOT NULL default ' ',
  "AAABHLMM" varchar(1) NOT NULL default ' ',
  "AAABHLMN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHLMP","AAABHLNT","AAABKPXT","AAABHLMS","AAABMLGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLMU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHLNS" varchar(4) NOT NULL default ' ',
  "AAABHLNT" varchar(4) NOT NULL default ' ',
  "AAABHLNU" varchar(4) NOT NULL default ' ',
  "AAABKPXT" varchar(4) NOT NULL default ' ',
  "AAABMLGN" varchar(4) NOT NULL default ' ',
  "AAABHLNB" varchar(3) NOT NULL default '000',
  "AAABHLNC" varchar(3) NOT NULL default '000',
  "AAABHLND" varchar(3) NOT NULL default '000',
  "AAABHLNE" varchar(3) NOT NULL default '000',
  "AAABHLSE" varchar(1) NOT NULL default ' ',
  "AAABLHZN" varchar(1) NOT NULL default ' ',
  "AAABHLNH" varchar(1) NOT NULL default ' ',
  "AAABHLNI" varchar(1) NOT NULL default ' ',
  "AAABHLNJ" varchar(1) NOT NULL default ' ',
  "AAABHLNK" varchar(1) NOT NULL default ' ',
  "AAABHLNL" varchar(1) NOT NULL default ' ',
  "AAABHLNM" varchar(1) NOT NULL default ' ',
  "AAABHLNN" varchar(1) NOT NULL default ' ',
  "AAABHLNO" varchar(1) NOT NULL default ' ',
  "AAABHLNP" varchar(1) NOT NULL default ' ',
  "AAABHLNQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHLNS","AAABHLNT","AAABHLNU","AAABKPXT","AAABMLGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLNX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQPP" varchar(4) NOT NULL default ' ',
  "AAABHLOH" varchar(4) NOT NULL default '0000',
  "AAABHLOB" varchar(2) NOT NULL default ' ',
  "AAABHLOC" varchar(25) NOT NULL default ' ',
  "AAABHLOD" varchar(25) NOT NULL default ' ',
  "AAABLKKE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQPP","AAABHLOH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLOI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKKE" varchar(2) NOT NULL default ' ',
  "AAABHLOL" varchar(1) NOT NULL default ' ',
  "AAABHLOM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLKKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLOP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLKKE" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLKKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLOX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHODT" varchar(4) NOT NULL default '0000',
  "AAABHLPG" varchar(4) NOT NULL default ' ',
  "AAABHLPB" varchar(4) NOT NULL default '0000',
  "AAABHLPC" varchar(14) NOT NULL default ' ',
  "AAABLTXO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHODT","AAABHLPG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKSTN" varchar(3) NOT NULL default ' ',
  "AAABHLPK" varchar(3) NOT NULL default '000',
  "AAABHLPL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKSTN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLPO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKSTN" varchar(3) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKSTN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLPW" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKGBL" varchar(26) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABMIPG" varchar(4) NOT NULL default '0000',
  "AAABJSEW" varchar(4) NOT NULL default '0000',
  "AAABHLQD" varchar(4) NOT NULL default '0000',
  "AAABHLQE" varchar(20) NOT NULL default ' ',
  "AAABHLQF" varchar(1) NOT NULL default ' ',
  "AAABHLQG" varchar(1) NOT NULL default ' ',
  "AAABLMOC" varchar(8) NOT NULL default '00000000',
  "AAABLMOD" varchar(6) NOT NULL default '000000',
  "AAABKCWA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL"),
  KEY "AAABHLQL" ("AAABKGBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLQN" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMIPG" varchar(4) NOT NULL default '0000',
  "AAABLMOC" varchar(8) NOT NULL default '00000000',
  "AAABLMOD" varchar(6) NOT NULL default '000000',
  "AAABHLQS" int(11) NOT NULL default '0',
  "AAABHLQT" varchar(1) NOT NULL default ' ',
  "AAABHLQU" varchar(1) NOT NULL default ' ',
  "AAABKGBL" varchar(26) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABIADW" varchar(1) NOT NULL default ' ',
  "AAABHLQY" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMREL"),
  KEY "AAABHLRA" ("AAABKGBL","AAABMREL"),
  KEY "AAABHLRD" ("AAABIRUN","AAABKGBL","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLRH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHLTE" varchar(4) NOT NULL default ' ',
  "AAABMLEP" varchar(1) NOT NULL default ' ',
  "AAABHLRL" varchar(1) NOT NULL default ' ',
  "AAABHLRM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHLTE","AAABMLEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLRQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHLTE" varchar(4) NOT NULL default ' ',
  "AAABLZZR" varchar(4) NOT NULL default ' ',
  "AAABHLRU" varchar(1) NOT NULL default ' ',
  "AAABHLRV" varchar(1) NOT NULL default ' ',
  "AAABHLRW" varchar(1) NOT NULL default ' ',
  "AAABHLRX" varchar(1) NOT NULL default ' ',
  "AAABHLRY" varchar(1) NOT NULL default ' ',
  "AAABHLRZ" varchar(1) NOT NULL default ' ',
  "AAABHLSA" varchar(1) NOT NULL default ' ',
  "AAABHLSB" varchar(1) NOT NULL default ' ',
  "AAABHLSC" varchar(1) NOT NULL default ' ',
  "AAABHLSD" varchar(1) NOT NULL default ' ',
  "AAABHLSE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHLTE","AAABLZZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLSI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHLTE" varchar(4) NOT NULL default ' ',
  "AAABHLSL" varchar(1) NOT NULL default ' ',
  "AAABHLSM" varchar(1) NOT NULL default ' ',
  "AAABHLSN" varchar(1) NOT NULL default ' ',
  "AAABHLSO" varchar(1) NOT NULL default ' ',
  "AAABHLSP" varchar(1) NOT NULL default ' ',
  "AAABHLSQ" varchar(1) NOT NULL default ' ',
  "AAABHLSR" varchar(1) NOT NULL default ' ',
  "AAABHLSS" varchar(1) NOT NULL default ' ',
  "AAABHLST" varchar(2) NOT NULL default ' ',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHLTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLSX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHLTE" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHLTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLTF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABJOWT" varchar(3) NOT NULL default ' ',
  "AAABLDCD" varchar(4) NOT NULL default ' ',
  "AAABMCUU" varchar(1) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLCAK","AAABJOWT","AAABLDCD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLTS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABJOWT" varchar(3) NOT NULL default ' ',
  "AAABLDCD" varchar(4) NOT NULL default ' ',
  "AAABHLUT" varchar(2) NOT NULL default ' ',
  "AAABHLUA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABLCAK","AAABJOWT","AAABLDCD","AAABHLUT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLUI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABJOWT" varchar(3) NOT NULL default ' ',
  "AAABHLUT" varchar(2) NOT NULL default ' ',
  "AAABHLUO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLCPX","AAABJOWT","AAABHLUT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLUU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJRYR" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJRYR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLUZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJRYR" varchar(5) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJRYR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLVH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJRYS" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJRYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLVM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJRYS" varchar(5) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJRYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJRYT" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJRYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLVZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJRYT" varchar(5) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJRYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLWH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJRYU" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJRYU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLWM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJRYU" varchar(5) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJRYU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLWU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJRYV" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJRYV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLWZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJRYV" varchar(5) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJRYV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLXZ" (
  "AAABLUTA" varchar(8) NOT NULL default ' ',
  "AAABJSVT" int(11) NOT NULL default '0',
  "AAABLENY" varchar(4) NOT NULL default '0000',
  "AAABLENX" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLUTA","AAABJSVT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLYA" (
  "AAABLUTA" varchar(5) NOT NULL default ' ',
  "AAABJSVT" int(11) NOT NULL default '0',
  "AAABLENY" varchar(3) NOT NULL default '000',
  "AAABLENX" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLUTA","AAABJSVT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLYB" (
  "AAABHLXZ" varchar(10) NOT NULL default ' ',
  "AAABHLYA" varchar(10) NOT NULL default ' ',
  "AAABHLYB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABHLXZ","AAABHLYA","AAABHLYB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLYC" (
  "AAABHLYK" varchar(8) NOT NULL default '00000000',
  "AAABHMVA" varchar(5) NOT NULL default '00000',
  "AAABHLYF" varchar(254) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABHLYH" varchar(8) NOT NULL default '00000000',
  "AAABHMUP" varchar(6) NOT NULL default '000000',
  "AAABHLYJ" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAABHLYK","AAABHMVA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLYM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLYR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXTU" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXTU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLYZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJO" varchar(4) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABHPJQ" varchar(4) NOT NULL default '0000',
  "AAABHPJT" double NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABHPJU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLZI" (
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJO" varchar(4) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABHPJQ" varchar(4) NOT NULL default '0000',
  "AAABHPJT" double NOT NULL default '0',
  PRIMARY KEY ("AAABHPJU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHLZP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABHMAD" varchar(4) NOT NULL default ' ',
  "AAABHMAE" int(11) NOT NULL default '0',
  "AAABHMAF" varchar(4) NOT NULL default '0000',
  "AAABHMAG" double NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABHPJU","AAABHPJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMAA" (
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABHMAD" varchar(4) NOT NULL default ' ',
  "AAABHMAE" int(11) NOT NULL default '0',
  "AAABHMAF" varchar(4) NOT NULL default '0000',
  "AAABHMAG" double NOT NULL default '0',
  PRIMARY KEY ("AAABHPJU","AAABHPJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMAJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHMHD" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHPJU","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMAR" (
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHMHD" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABHPJU","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMAX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJO" varchar(4) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABHPJQ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABHPJU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMBF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABHPJO" varchar(4) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABHPJU","AAABHPJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMBO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHMHG" varchar(4) NOT NULL default ' ',
  "AAABHPJO" varchar(4) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABHPJU","AAABHMHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMBX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABHPJW" varchar(4) NOT NULL default ' ',
  "AAABHMXF" int(11) NOT NULL default '0',
  "AAABHMCD" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHPJU","AAABHPJV","AAABHPJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMCI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABHMGV" varchar(4) NOT NULL default ' ',
  "AAABHPJO" varchar(4) NOT NULL default ' ',
  "AAABHMEO" varchar(4) NOT NULL default '0000',
  "AAABHMXH" double NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABHPJU","AAABHPJV","AAABHMGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMCU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHMHG" varchar(4) NOT NULL default ' ',
  "AAABHMHH" varchar(4) NOT NULL default ' ',
  "AAABHPJO" varchar(4) NOT NULL default ' ',
  "AAABHMUN" double NOT NULL default '0',
  "AAABHMDB" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHPJU","AAABHMHG","AAABHMHH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMDG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABHMGV" varchar(4) NOT NULL default ' ',
  "AAABHMDS" varchar(4) NOT NULL default ' ',
  "AAABHMEN" varchar(8) NOT NULL default ' ',
  "AAABHMEO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABHPJU","AAABHPJV","AAABHMGV","AAABHMDS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMDT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHMHG" varchar(4) NOT NULL default ' ',
  "AAABHMHH" varchar(4) NOT NULL default ' ',
  "AAABHMEU" varchar(4) NOT NULL default ' ',
  "AAABHMEN" varchar(8) NOT NULL default ' ',
  "AAABHMEO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABHPJU","AAABHMHG","AAABHMHH","AAABHMEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMEG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHMHG" varchar(4) NOT NULL default ' ',
  "AAABHMHH" varchar(4) NOT NULL default ' ',
  "AAABHMEU" varchar(4) NOT NULL default ' ',
  "AAABHMEV" varchar(4) NOT NULL default ' ',
  "AAABHMEN" varchar(8) NOT NULL default ' ',
  "AAABHMEO" varchar(4) NOT NULL default '0000',
  "AAABHMXH" double NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABHPJU","AAABHMHG","AAABHMHH","AAABHMEU","AAABHMEV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMEW" (
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABHMFI" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNLV","AAABHMFI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMFB" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABHMFI" varchar(2) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMNLV","AAABHMFI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMFJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHMHD" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHPJU","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMFR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHMHD" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHPJU","AAABHPJV","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMGB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHMHG" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHMHD" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHPJU","AAABHMHG","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMGL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABHMGV" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHMHD" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHPJU","AAABHPJV","AAABHMGV","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMGX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHMHG" varchar(4) NOT NULL default ' ',
  "AAABHMHH" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHMHD" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHPJU","AAABHMHG","AAABHMHH","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMHJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKSOS" varchar(2) NOT NULL default ' ',
  "AAABHMHM" varchar(1) NOT NULL default ' ',
  "AAABLXOF" varchar(1) NOT NULL default ' ',
  "AAABKSPC" varchar(2) NOT NULL default ' ',
  "AAABKSPD" varchar(1) NOT NULL default ' ',
  "AAABHMHQ" varchar(1) NOT NULL default ' ',
  "AAABHMHR" varchar(1) NOT NULL default ' ',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  "AAABLUHY" varchar(1) NOT NULL default ' ',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABHMHV" varchar(1) NOT NULL default ' ',
  "AAABKSPV" varchar(2) NOT NULL default ' ',
  "AAABHOSP" varchar(8) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABJLIP" varchar(2) NOT NULL default ' ',
  "AAABLUFX" varchar(1) NOT NULL default ' ',
  "AAABLXTD" varchar(1) NOT NULL default ' ',
  "AAABHMIC" varchar(1) NOT NULL default ' ',
  "AAABHMID" varchar(1) NOT NULL default ' ',
  "AAABHMIE" varchar(3) NOT NULL default ' ',
  "AAABHTKH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKSOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMII" (
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABHPJW" varchar(4) NOT NULL default ' ',
  "AAABHPJX" varchar(10) NOT NULL default '0000000000',
  "AAABHPJO" varchar(4) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABHPJQ" varchar(4) NOT NULL default '0000',
  "AAABHPJR" int(11) NOT NULL default '0',
  "AAABHPJS" varchar(8) NOT NULL default ' ',
  "AAABHPJT" double NOT NULL default '0',
  PRIMARY KEY ("AAABHPJU","AAABHPJV","AAABHPJW","AAABHPJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMIX" (
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABHPJW" varchar(1) NOT NULL default ' ',
  "AAABHPJX" varchar(10) NOT NULL default '0000000000',
  "AAABHPJO" varchar(2) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABHPJQ" varchar(3) NOT NULL default '000',
  "AAABHPJR" int(11) NOT NULL default '0',
  "AAABHPJS" varchar(1) NOT NULL default ' ',
  "AAABHPJT" double NOT NULL default '0',
  PRIMARY KEY ("AAABHPJU","AAABHPJV","AAABHPJW","AAABHPJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMJM" (
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABHPJW" varchar(4) NOT NULL default ' ',
  "AAABHPJX" varchar(10) NOT NULL default '0000000000',
  "AAABHPJO" varchar(4) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABHPJQ" varchar(4) NOT NULL default '0000',
  "AAABHPJR" int(11) NOT NULL default '0',
  "AAABHPJS" varchar(8) NOT NULL default ' ',
  "AAABHPJT" double NOT NULL default '0',
  PRIMARY KEY ("AAABHPJU","AAABHPJV","AAABHPJW","AAABHPJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMKB" (
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABHPJW" varchar(1) NOT NULL default ' ',
  "AAABHPJX" varchar(10) NOT NULL default '0000000000',
  "AAABHPJO" varchar(2) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABHPJQ" varchar(3) NOT NULL default '000',
  "AAABHPJR" int(11) NOT NULL default '0',
  "AAABHPJS" varchar(1) NOT NULL default ' ',
  "AAABHPJT" double NOT NULL default '0',
  PRIMARY KEY ("AAABHPJU","AAABHPJV","AAABHPJW","AAABHPJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMKQ" (
  "AAABLTYE" varchar(5) NOT NULL default '00000',
  "AAABHMLH" varchar(20) NOT NULL default ' ',
  "AAABHMKT" varchar(8) NOT NULL default '00000000',
  "AAABHMKU" varchar(6) NOT NULL default '000000',
  "AAABJSZA" tinyint(3) unsigned NOT NULL default '0',
  "AAABJSYW" smallint(6) NOT NULL default '0',
  "AAABHMKX" int(11) NOT NULL default '0',
  "AAABHMKY" double NOT NULL default '0',
  "AAABHMKZ" varchar(254) NOT NULL default ' ',
  "AAABHMLA" decimal(15,0) NOT NULL default '0',
  "AAABHMLB" VARBINARY(255),
  "AAABMLZG" smallint(6) NOT NULL default '0',
  "AAABHMLJ" text,
  PRIMARY KEY ("AAABLTYE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMLF" (
  "AAABLTYE" varchar(5) NOT NULL default '00000',
  "AAABHMLH" varchar(20) NOT NULL default ' ',
  "AAABMLZG" smallint(6) NOT NULL default '0',
  "AAABHMLJ" text,
  PRIMARY KEY ("AAABLTYE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMLL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKSOS" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKSOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMLT" (
  "AAABHMLV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHMLV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMLW" (
  "AAABHMMA" varchar(4) NOT NULL default ' ',
  "AAABHMLY" varchar(8) NOT NULL default ' ',
  "AAABHMLZ" varchar(11) NOT NULL default '00000000000',
  PRIMARY KEY ("AAABHMMA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMMB" (
  "AAABJSVM" varchar(4) NOT NULL default ' ',
  "AAABLNNK" VARBINARY(255) NOT NULL,
  "AAABHMME" double NOT NULL default '0',
  "AAABHMMF" int(11) NOT NULL default '0',
  "AAABHMMG" varchar(8) NOT NULL default '00000000',
  "AAABHMMH" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABJSVM","AAABLNNK"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMMK" (
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMMN" (
  "AAABLUSV" varchar(7) NOT NULL default '0000000',
  "AAABHMMP" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMMR" (
  "AAABLUSV" varchar(7) NOT NULL default '0000000',
  "AAABHMMT" decimal(7,0) NOT NULL default '0',
  "AAABHMMU" decimal(7,0) NOT NULL default '0',
  "AAABHMMV" decimal(7,0) NOT NULL default '0',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABHMNR" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMMZ" (
  "AAABLUSV" varchar(7) NOT NULL default '0000000',
  "AAABIACJ" varchar(12) NOT NULL default ' ',
  "AAABLMKH" varchar(8) NOT NULL default '00000000',
  "AAABLIRO" varchar(6) NOT NULL default '000000',
  "AAABHMNE" varchar(8) NOT NULL default ' ',
  "AAABLKMM" varchar(8) NOT NULL default '00000000',
  "AAABHMNR" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMNI" (
  "AAABHMRN" varchar(22) NOT NULL default ' ',
  "AAABHMOP" varchar(5) NOT NULL default '00000',
  "AAABHMRO" varchar(1) NOT NULL default '0',
  "AAABMNDU" varchar(8) NOT NULL default ' ',
  "AAABHMSE" varchar(4) NOT NULL default ' ',
  "AAABHMQO" varchar(1) NOT NULL default ' ',
  "AAABHMSB" varchar(80) NOT NULL default ' ',
  "AAABHMNQ" varchar(1) NOT NULL default ' ',
  "AAABHMNR" varchar(1) NOT NULL default ' ',
  "AAABLUUW" tinyint(3) unsigned NOT NULL default '0',
  "AAABHMNT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHMRN","AAABHMOP","AAABHMRO","AAABMNDU","AAABHMSE","AAABHMQO"),
  KEY "AAABHMOA" ("AAABMNDU","AAABHMSE","AAABHMQO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMOE" (
  "AAABHMRN" varchar(22) NOT NULL default ' ',
  "AAABHMOP" varchar(5) NOT NULL default '00000',
  "AAABHMRO" varchar(1) NOT NULL default '0',
  "AAABMNDU" varchar(8) NOT NULL default ' ',
  "AAABHMSE" varchar(4) NOT NULL default ' ',
  "AAABHMQO" varchar(1) NOT NULL default ' ',
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABHMRN","AAABHMOP","AAABHMRO","AAABMNDU","AAABHMSE","AAABHMQO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMOU" (
  "AAABHMSC" varchar(8) NOT NULL default ' ',
  "AAABMNDU" varchar(8) NOT NULL default ' ',
  "AAABHMSE" varchar(4) NOT NULL default ' ',
  "AAABHMQO" varchar(1) NOT NULL default ' ',
  "AAABHMQN" varchar(10) NOT NULL default ' ',
  "AAABHMPA" varchar(8) NOT NULL default '00000000',
  "AAABHMPB" varchar(8) NOT NULL default '00000000',
  "AAABHMPC" varchar(8) NOT NULL default '00000000',
  "AAABHMPD" varchar(22) NOT NULL default ' ',
  "AAABHMQH" varchar(12) NOT NULL default ' ',
  "AAABLUUW" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABHMSC","AAABMNDU","AAABHMSE","AAABHMQO","AAABHMQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMPL" (
  "AAABHMSC" varchar(8) NOT NULL default ' ',
  "AAABMNDU" varchar(8) NOT NULL default ' ',
  "AAABHMSE" varchar(4) NOT NULL default ' ',
  "AAABHMQN" varchar(10) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABHMPS" varchar(10) NOT NULL default '0000000000',
  "AAABHMPT" varchar(4) NOT NULL default '0000',
  "AAABLJJY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHMSC","AAABMNDU","AAABHMSE","AAABHMQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMPZ" (
  "AAABHMSC" varchar(8) NOT NULL default ' ',
  "AAABMNDU" varchar(8) NOT NULL default ' ',
  "AAABHMSE" varchar(4) NOT NULL default ' ',
  "AAABHMQN" varchar(10) NOT NULL default ' ',
  "AAABHMQO" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUVJ" varchar(32) NOT NULL default ' ',
  "AAABHMQH" varchar(12) NOT NULL default ' ',
  "AAABHMRV" varchar(4) NOT NULL default ' ',
  "AAABHMQJ" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABHMSC","AAABMNDU","AAABHMSE","AAABHMQN","AAABHMQO","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMQQ" (
  "AAABHMRC" varchar(22) NOT NULL default ' ',
  "AAABHMQS" varchar(22) NOT NULL default ' ',
  "AAABHMQT" varchar(8) NOT NULL default '00000000',
  "AAABHMQU" varchar(6) NOT NULL default '000000',
  "AAABHMRE" varchar(12) NOT NULL default ' ',
  "AAABHMQW" varchar(8) NOT NULL default ' ',
  "AAABHMQX" varchar(32) NOT NULL default ' ',
  "AAABHMQY" varchar(10) NOT NULL default ' ',
  "AAABHMQZ" decimal(15,0) NOT NULL default '0',
  "AAABHMRA" varchar(7) NOT NULL default '0000000',
  "AAABHMRB" varchar(7) NOT NULL default '0000000',
  PRIMARY KEY ("AAABHMRC"),
  KEY "AAABHMRD" ("AAABHMRE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMRF" (
  "AAABHMRN" varchar(22) NOT NULL default ' ',
  "AAABHMRO" int(11) NOT NULL default '0',
  "AAABHMRI" varchar(7) NOT NULL default '0000000',
  "AAABHMRJ" varchar(8) NOT NULL default ' ',
  "AAABHMRK" varchar(1) NOT NULL default ' ',
  "AAABHMRL" varchar(1) NOT NULL default ' ',
  "AAABLILS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHMRN","AAABHMRO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMRP" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHMRV" varchar(4) NOT NULL default ' ',
  "AAABLRPR" varchar(3) NOT NULL default ' ',
  "AAABMRCK" varchar(84) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHMRV","AAABLRPR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMRX" (
  "AAABHMSC" varchar(8) NOT NULL default ' ',
  "AAABMNDU" varchar(8) NOT NULL default ' ',
  "AAABHMSE" varchar(4) NOT NULL default ' ',
  "AAABHMSB" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABHMSC","AAABMNDU","AAABHMSE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMSF" (
  "AAABLUSV" varchar(7) NOT NULL default '0000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMSJ" (
  "AAABMLPV" varchar(12) NOT NULL default ' ',
  "AAABIYJF" varchar(241) NOT NULL default ' ',
  "AAABHMSM" varchar(1) NOT NULL default ' ',
  "AAABHMSN" varchar(1) NOT NULL default ' ',
  "AAABIKMF" varchar(1) NOT NULL default ' ',
  "AAABHMSP" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMSR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKSQJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKSQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMSW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKSQJ" varchar(1) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKSQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMTE" (
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABHPJW" varchar(4) NOT NULL default ' ',
  "AAABHPJX" int(11) NOT NULL default '0',
  "AAABHPJO" varchar(4) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABHPJQ" varchar(4) NOT NULL default '0000',
  "AAABHPJR" int(11) NOT NULL default '0',
  "AAABHPJS" varchar(8) NOT NULL default ' ',
  "AAABHPJT" double NOT NULL default '0',
  PRIMARY KEY ("AAABHPJU","AAABHPJV","AAABHPJW","AAABHPJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMTT" (
  "AAABHMXI" varchar(5) NOT NULL default '00000',
  "AAABHMXF" int(11) NOT NULL default '0',
  "AAABHMUN" double NOT NULL default '0',
  "AAABHMUO" varchar(30) NOT NULL default ' ',
  "AAABHMUP" varchar(6) NOT NULL default '000000',
  "AAABHMUQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABHMXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMUB" (
  "AAABHMXI" varchar(5) NOT NULL default '00000',
  "AAABHMVA" varchar(5) NOT NULL default '00000',
  "AAABHMXF" int(11) NOT NULL default '0',
  "AAABHMXG" varchar(30) NOT NULL default ' ',
  "AAABHMUX" varchar(6) NOT NULL default '000000',
  "AAABHMUY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABHMXI","AAABHMVA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMUK" (
  "AAABHMXI" varchar(5) NOT NULL default '00000',
  "AAABHMXF" int(11) NOT NULL default '0',
  "AAABHMUN" double NOT NULL default '0',
  "AAABHMUO" varchar(30) NOT NULL default ' ',
  "AAABHMUP" varchar(6) NOT NULL default '000000',
  "AAABHMUQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABHMXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMUS" (
  "AAABHMXI" varchar(5) NOT NULL default '00000',
  "AAABHMVA" varchar(5) NOT NULL default '00000',
  "AAABHMXF" int(11) NOT NULL default '0',
  "AAABHMXG" varchar(30) NOT NULL default ' ',
  "AAABHMUX" varchar(6) NOT NULL default '000000',
  "AAABHMUY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABHMXI","AAABHMVA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMVB" (
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABHPJW" varchar(1) NOT NULL default ' ',
  "AAABHPJX" int(11) NOT NULL default '0',
  "AAABHPJO" varchar(2) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABHPJQ" varchar(3) NOT NULL default '000',
  "AAABHPJR" int(11) NOT NULL default '0',
  "AAABHPJS" varchar(1) NOT NULL default ' ',
  "AAABHPJT" double NOT NULL default '0',
  PRIMARY KEY ("AAABHPJU","AAABHPJV","AAABHPJW","AAABHPJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMVQ" (
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABHPJW" varchar(4) NOT NULL default ' ',
  "AAABHPJX" int(11) NOT NULL default '0',
  "AAABHPJO" varchar(4) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABHPJQ" varchar(4) NOT NULL default '0000',
  "AAABHPJR" int(11) NOT NULL default '0',
  "AAABHPJS" varchar(8) NOT NULL default ' ',
  "AAABHPJT" double NOT NULL default '0',
  PRIMARY KEY ("AAABHPJU","AAABHPJV","AAABHPJW","AAABHPJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMWF" (
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABHPJW" varchar(1) NOT NULL default ' ',
  "AAABHPJX" int(11) NOT NULL default '0',
  "AAABHPJO" varchar(2) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABHPJQ" varchar(3) NOT NULL default '000',
  "AAABHPJR" int(11) NOT NULL default '0',
  "AAABHPJS" varchar(1) NOT NULL default ' ',
  "AAABHPJT" double NOT NULL default '0',
  PRIMARY KEY ("AAABHPJU","AAABHPJV","AAABHPJW","AAABHPJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMWU" (
  "AAABHMXI" varchar(5) NOT NULL default '00000',
  "AAABHMXJ" varchar(5) NOT NULL default ' ',
  "AAABHMXF" int(11) NOT NULL default '0',
  "AAABHMXG" varchar(200) NOT NULL default ' ',
  "AAABHMXH" double NOT NULL default '0',
  PRIMARY KEY ("AAABHMXI","AAABHMXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMXC" (
  "AAABHMXI" varchar(5) NOT NULL default '00000',
  "AAABHMXJ" varchar(3) NOT NULL default ' ',
  "AAABHMXF" smallint(6) NOT NULL default '0',
  "AAABHMXG" varchar(200) NOT NULL default ' ',
  "AAABHMXH" double NOT NULL default '0',
  PRIMARY KEY ("AAABHMXI","AAABHMXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMXK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHMYG" varchar(14) NOT NULL default ' ',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABLZFC" varchar(4) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABHMXQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHMYG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMXT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHMYG" varchar(14) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHMYG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMYB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHMYG" varchar(14) NOT NULL default ' ',
  "AAABHQGB" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHMYG","AAABHQGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMYI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABJTWS" varchar(6) NOT NULL default ' ',
  "AAABLBIH" varchar(18) NOT NULL default ' ',
  "AAABMLEV" decimal(15,3) NOT NULL default '0.000',
  "AAABMLEW" varchar(3) NOT NULL default ' ',
  "AAABMLEX" decimal(15,3) NOT NULL default '0.000',
  "AAABMLEY" varchar(3) NOT NULL default ' ',
  "AAABJRZT" decimal(6,0) NOT NULL default '0',
  "AAABJRZU" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABJTWS","AAABLBIH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMYX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABJUHN" varchar(2) NOT NULL default ' ',
  "AAABJTWS" varchar(6) NOT NULL default ' ',
  "AAABHMZC" decimal(8,4) NOT NULL default '0.0000',
  "AAABHMZD" varchar(3) NOT NULL default ' ',
  "AAABHMZE" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABJUHN","AAABJTWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMZJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABJTWS" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABJTWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHMZQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJTWS" varchar(6) NOT NULL default ' ',
  "AAABMAHQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMRHS","AAABJTWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNAA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHNBK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHNBK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHNBK" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHNBK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHNKZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHNKZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNAS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHNKZ" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHNKZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNBA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABHNKZ" varchar(4) NOT NULL default ' ',
  "AAABHNBK" varchar(4) NOT NULL default ' ',
  "AAABHNBY" varchar(4) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZFD","AAABHNKZ","AAABHNBK","AAABHNBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNBM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHNBY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHNBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNBR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHNBY" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHNBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNBZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMV" varchar(4) NOT NULL default ' ',
  "AAABMDBN" varchar(4) NOT NULL default ' ',
  "AAABHYJM" varchar(2) NOT NULL default ' ',
  "AAABHYJN" varchar(2) NOT NULL default ' ',
  "AAABHOOP" varchar(6) NOT NULL default '000000',
  "AAABHNCG" varchar(1) NOT NULL default ' ',
  "AAABHOOJ" varchar(4) NOT NULL default ' ',
  "AAABHOOL" varchar(20) NOT NULL default ' ',
  "AAABHNCJ" varchar(1) NOT NULL default ' ',
  "AAABMAGA" varchar(1) NOT NULL default ' ',
  "AAABHOSP" varchar(8) NOT NULL default ' ',
  "AAABLZUS" varchar(1) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABHNCO" varchar(4) NOT NULL default ' ',
  "AAABHNCP" varchar(4) NOT NULL default ' ',
  "AAABHNCQ" varchar(6) NOT NULL default ' ',
  "AAABHNCR" varchar(4) NOT NULL default ' ',
  "AAABLYCG" varchar(2) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLXSA" varchar(6) NOT NULL default ' ',
  "AAABHNCX" varchar(3) NOT NULL default ' ',
  "AAABLUKA" varchar(1) NOT NULL default ' ',
  "AAABHNCZ" varchar(2) NOT NULL default ' ',
  "AAABHNDA" varchar(1) NOT NULL default ' ',
  "AAABHNDB" varchar(6) NOT NULL default ' ',
  "AAABHNDC" varchar(1) NOT NULL default ' ',
  "AAABJJTE" varchar(4) NOT NULL default ' ',
  "AAABHNDE" varchar(1) NOT NULL default ' ',
  "AAABHNDF" varchar(1) NOT NULL default ' ',
  "AAABLXKA" varchar(1) NOT NULL default ' ',
  "AAABJJTR" varchar(4) NOT NULL default ' ',
  "AAABHNDI" varchar(3) NOT NULL default '000',
  "AAABHNDJ" varchar(1) NOT NULL default ' ',
  "AAABHNDK" varchar(1) NOT NULL default ' ',
  "AAABHNDL" varchar(1) NOT NULL default ' ',
  "AAABHNDM" varchar(2) NOT NULL default ' ',
  "AAABHNDN" varchar(4) NOT NULL default ' ',
  "AAABHNDO" varchar(1) NOT NULL default '0',
  "AAABHNDP" varchar(4) NOT NULL default ' ',
  "AAABHNDQ" varchar(1) NOT NULL default '0',
  "AAABKMSZ" varchar(1) NOT NULL default ' ',
  "AAABMCZS" varchar(1) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABHNDU" varchar(1) NOT NULL default ' ',
  "AAABHNDV" varchar(1) NOT NULL default ' ',
  "AAABHNDW" varchar(6) NOT NULL default ' ',
  "AAABHNDX" varchar(6) NOT NULL default ' ',
  "AAABHNDY" varchar(6) NOT NULL default ' ',
  "AAABHNDZ" varchar(6) NOT NULL default ' ',
  "AAABHNEA" varchar(1) NOT NULL default ' ',
  "AAABHNEB" varchar(1) NOT NULL default ' ',
  "AAABHNEC" varchar(1) NOT NULL default ' ',
  "AAABHOTI" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNEG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHYJM" varchar(2) NOT NULL default ' ',
  "AAABMQIK" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABHYJM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNEM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMV" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNEU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHNFH" varchar(4) NOT NULL default ' ',
  "AAABHNEX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHNFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNFA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHNFH" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHNFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNFI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABMAMV" varchar(4) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZFD","AAABMAMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNFS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABKYJI" varchar(10) NOT NULL default ' ',
  "AAABLHXQ" varchar(2) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABHNGG" varchar(10) NOT NULL default ' ',
  "AAABHNGH" varchar(10) NOT NULL default ' ',
  "AAABHNNM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZFD","AAABKYJI","AAABLHXQ","AAABMAMP","AAABHNGG","AAABHNGH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNGI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABKYJI" varchar(10) NOT NULL default ' ',
  "AAABLHXQ" varchar(2) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZFD","AAABKYJI","AAABLHXQ","AAABMAMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNGT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHNHN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHNHN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNGY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHNHN" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHNHN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNHG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHNHN" varchar(4) NOT NULL default ' ',
  "AAABKIUC" decimal(10,5) NOT NULL default '0.00000',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABHNHL" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABHNHN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNHO" (
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABHNIA" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMAMY","AAABHNIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNHT" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABHNIA" varchar(2) NOT NULL default ' ',
  "AAABMAHQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMAMY","AAABHNIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNIB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZAP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZAP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNIG" (
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABJUHN" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABHNIK" varchar(30) NOT NULL default ' ',
  "AAABLUEL" varchar(30) NOT NULL default ' ',
  "AAABHNIM" varchar(30) NOT NULL default ' ',
  "AAABHNIN" varchar(1) NOT NULL default ' ',
  "AAABHNIO" varchar(30) NOT NULL default ' ',
  "AAABHNIP" varchar(30) NOT NULL default ' ',
  "AAABHNIQ" varchar(30) NOT NULL default ' ',
  "AAABHNIR" varchar(30) NOT NULL default ' ',
  "AAABHNIS" varchar(30) NOT NULL default ' ',
  "AAABHNIT" varchar(30) NOT NULL default ' ',
  "AAABHNIU" varchar(1) NOT NULL default ' ',
  "AAABJVXZ" varchar(1) NOT NULL default ' ',
  "AAABHNIW" varchar(1) NOT NULL default ' ',
  "AAABJTWM" varchar(3) NOT NULL default '000',
  "AAABJTWN" varchar(3) NOT NULL default '000',
  "AAABHNIZ" varchar(1) NOT NULL default ' ',
  "AAABHNJA" varchar(1) NOT NULL default ' ',
  "AAABJVTD" varchar(6) NOT NULL default ' ',
  "AAABHNJC" varchar(30) NOT NULL default ' ',
  "AAABHNJD" varchar(40) NOT NULL default ' ',
  "AAABHNJE" varchar(4) NOT NULL default ' ',
  "AAABHNJF" varchar(1) NOT NULL default ' ',
  "AAABHNJG" varchar(1) NOT NULL default ' ',
  "AAABHNJH" varchar(1) NOT NULL default ' ',
  "AAABHNJI" varchar(1) NOT NULL default ' ',
  "AAABHNJJ" varchar(1) NOT NULL default ' ',
  "AAABHNJK" varchar(1) NOT NULL default ' ',
  "AAABHNJL" varchar(1) NOT NULL default ' ',
  "AAABHNJM" varchar(1) NOT NULL default ' ',
  "AAABHNJN" varchar(1) NOT NULL default ' ',
  "AAABHNJO" varchar(1) NOT NULL default ' ',
  "AAABHNJP" varchar(1) NOT NULL default ' ',
  "AAABHNJQ" varchar(1) NOT NULL default ' ',
  "AAABHNJR" varchar(1) NOT NULL default ' ',
  "AAABHNJS" varchar(1) NOT NULL default ' ',
  "AAABHNJT" varchar(1) NOT NULL default ' ',
  "AAABHNJU" varchar(1) NOT NULL default ' ',
  "AAABHNJV" varchar(1) NOT NULL default ' ',
  "AAABHNJW" varchar(1) NOT NULL default ' ',
  "AAABHNJX" varchar(1) NOT NULL default ' ',
  "AAABHNJY" varchar(1) NOT NULL default ' ',
  "AAABHNJZ" varchar(1) NOT NULL default ' ',
  "AAABHNKA" varchar(1) NOT NULL default ' ',
  "AAABHNKB" varchar(1) NOT NULL default ' ',
  "AAABHNKC" varchar(1) NOT NULL default ' ',
  "AAABHNKD" varchar(1) NOT NULL default ' ',
  "AAABHNKE" varchar(1) NOT NULL default ' ',
  "AAABHNKF" varchar(1) NOT NULL default ' ',
  "AAABHNKG" varchar(1) NOT NULL default ' ',
  "AAABHNKH" varchar(1) NOT NULL default ' ',
  "AAABHNKI" varchar(40) NOT NULL default ' ',
  "AAABHNKJ" varchar(30) NOT NULL default ' ',
  "AAABHNKK" varchar(40) NOT NULL default ' ',
  "AAABHNKL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMAMY","AAABJUHN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLZAP" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLZAP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJTXS" varchar(4) NOT NULL default ' ',
  "AAABHNKZ" varchar(4) NOT NULL default ' ',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAABJTZN" varchar(1) NOT NULL default ' ',
  "AAABHNLC" varchar(4) NOT NULL default ' ',
  "AAABMNZR" varchar(4) NOT NULL default ' ',
  "AAABLXTD" varchar(1) NOT NULL default ' ',
  "AAABHNLF" varchar(1) NOT NULL default ' ',
  "AAABHNLG" varchar(1) NOT NULL default ' ',
  "AAABHNLH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJTXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNLK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMV" varchar(4) NOT NULL default ' ',
  "AAABJTYQ" varchar(1) NOT NULL default ' ',
  "AAABJTXS" varchar(4) NOT NULL default ' ',
  "AAABHNLP" varchar(1) NOT NULL default ' ',
  "AAABLHRD" varchar(4) NOT NULL default ' ',
  "AAABHNLR" varchar(1) NOT NULL default ' ',
  "AAABHNLS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMV","AAABJTYQ","AAABJTXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNLX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHNMR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHNMR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNMC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHNMR" varchar(4) NOT NULL default ' ',
  "AAABHNMJ" varchar(4) NOT NULL default '0000',
  "AAABHNMG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHNMR","AAABHNMJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNMK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHNMR" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHNMR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNMS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJTXS" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJTXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNNA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHNNM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHNNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNNF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHNNM" varchar(10) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHNNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNNN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHNOA" varchar(1) NOT NULL default ' ',
  "AAABHNNQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHNOA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNNT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHNOA" varchar(1) NOT NULL default ' ',
  "AAABHNNX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHNOA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYOZ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYOZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNOG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKYOZ" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKYOZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNOO" (
  "AAABHNOZ" varchar(26) NOT NULL default ' ',
  "AAABHNPA" varchar(5) NOT NULL default ' ',
  "AAABHNPB" varchar(30) NOT NULL default ' ',
  "AAABMLEL" varchar(2) NOT NULL default '00',
  "AAABHNOT" varchar(30) NOT NULL default ' ',
  "AAABHNOU" varchar(30) NOT NULL default ' ',
  "AAABHNOV" varchar(1) NOT NULL default ' ',
  "AAABHNOW" varchar(20) NOT NULL default ' ',
  "AAABHNOX" varchar(2) NOT NULL default '00',
  "AAABHNOY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHNOZ","AAABHNPA","AAABHNPB","AAABMLEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHNPI" varchar(4) NOT NULL default ' ',
  "AAABHNPG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHNPI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNPJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHQRQ" varchar(4) NOT NULL default ' ',
  "AAABLDNL" varchar(30) NOT NULL default ' ',
  "AAABJLIY" varchar(30) NOT NULL default ' ',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAABHPNG" varchar(1) NOT NULL default ' ',
  "AAABHQLN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHQRQ","AAABLDNL","AAABJLIY","AAABMALG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNPW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHQRQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHQRQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNQB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHQRQ" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHQRQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNQJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXIC" varchar(3) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXIC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNQR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHNRX" varchar(10) NOT NULL default ' ',
  "AAABHNRH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHNRX","AAABHNRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHNRX" varchar(10) NOT NULL default ' ',
  "AAABHNRH" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHNRX","AAABHNRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNRI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHNRX" varchar(10) NOT NULL default ' ',
  "AAABHNRY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHNRX","AAABHNRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNRP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHNRX" varchar(10) NOT NULL default ' ',
  "AAABHNRY" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHNRX","AAABHNRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNRZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQIL","AAABLESS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNSH" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMPMH" varchar(2) NOT NULL default ' ',
  "AAABJLPG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNSN" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  "AAABHNTB" varchar(4) NOT NULL default '0000',
  "AAABHNSR" varchar(30) NOT NULL default ' ',
  "AAABHNSS" varchar(3) NOT NULL default ' ',
  "AAABMJCM" varchar(2) NOT NULL default ' ',
  "AAABMMJT" varchar(120) NOT NULL default ' ',
  "AAABHNSV" varchar(3) NOT NULL default ' ',
  "AAABLLWR" int(11) NOT NULL default '0',
  "AAABIGGO" varchar(1) NOT NULL default ' ',
  "AAABHNSY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABLEST","AAABHNTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNTC" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMREL","AAABLEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNTK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHNUB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHNUB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNTP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHNUB" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHNUB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNTX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHNUB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHNUB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNUC" (
  "AAABMNDU" varchar(40) NOT NULL default ' ',
  "AAABHNUE" varchar(1) NOT NULL default ' ',
  "AAABLABT" varchar(1) NOT NULL default ' ',
  "AAABHNUG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNUI" (
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHUGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNUM" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  "AAABJDMJ" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHUGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNUS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHNVE" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHNVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNUX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHNVE" varchar(3) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHNVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNVF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHNVR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHNVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNVK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHNVR" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHNVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNVS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHNWE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHNWE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNVX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHNWE" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHNWE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNWF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHNWR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHNWR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNWK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHNWR" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHNWR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNWS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHNXE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHNXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNWX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHNXE" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHNXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNXF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHNXR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHNXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNXK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHNXR" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHNXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNXS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHNYE" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHNYE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNXX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHNYE" varchar(3) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHNYE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHNYR" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHNYR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHNYR" varchar(3) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHNYR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNYS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHNZE" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHNZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNYX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHNZE" varchar(3) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHNZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNZF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHNZR" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHNZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNZK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHNZR" varchar(3) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHNZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHNZS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXID" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXID")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOAA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABLXID" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABLXID")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOAL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHOAX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHOAX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOAQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHOAX" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHOAX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOAY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHOCC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHOCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOBD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHOBP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHOBP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOBI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHOBP" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHOBP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOBQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXIC" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXIC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOBV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHOCC" varchar(2) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHOCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOCD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZNI" varchar(4) NOT NULL default ' ',
  "AAABMDBN" varchar(4) NOT NULL default ' ',
  "AAABHOOJ" varchar(20) NOT NULL default ' ',
  "AAABHYJN" varchar(2) NOT NULL default ' ',
  "AAABLKXQ" varchar(4) NOT NULL default ' ',
  "AAABLZUS" varchar(1) NOT NULL default ' ',
  "AAABJLDK" varchar(1) NOT NULL default ' ',
  "AAABMAGA" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABHOCQ" varchar(1) NOT NULL default ' ',
  "AAABLYCG" varchar(2) NOT NULL default ' ',
  "AAABHOCS" varchar(4) NOT NULL default ' ',
  "AAABHOCT" varchar(4) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABHOCV" varchar(4) NOT NULL default ' ',
  "AAABHOCW" varchar(4) NOT NULL default ' ',
  "AAABHOCX" varchar(4) NOT NULL default ' ',
  "AAABHOCY" varchar(4) NOT NULL default ' ',
  "AAABHOCZ" varchar(1) NOT NULL default ' ',
  "AAABHODA" varchar(1) NOT NULL default ' ',
  "AAABHODB" varchar(1) NOT NULL default ' ',
  "AAABKMAN" varchar(4) NOT NULL default ' ',
  "AAABHODD" varchar(4) NOT NULL default ' ',
  "AAABHODE" varchar(4) NOT NULL default ' ',
  "AAABHODF" varchar(2) NOT NULL default ' ',
  "AAABHODG" varchar(3) NOT NULL default '000',
  "AAABJLIP" varchar(2) NOT NULL default ' ',
  "AAABHODI" varchar(1) NOT NULL default ' ',
  "AAABHODJ" varchar(2) NOT NULL default ' ',
  "AAABMBXF" varchar(10) NOT NULL default ' ',
  "AAABJIQB" varchar(1) NOT NULL default ' ',
  "AAABHODM" varchar(1) NOT NULL default ' ',
  "AAABHODN" varchar(2) NOT NULL default ' ',
  "AAABLKZT" varchar(1) NOT NULL default ' ',
  "AAABHODP" varchar(11) NOT NULL default ' ',
  "AAABLXWZ" varchar(1) NOT NULL default ' ',
  "AAABHODR" varchar(4) NOT NULL default ' ',
  "AAABJIQF" varchar(1) NOT NULL default ' ',
  "AAABHODT" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABLZNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHODW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLZNI" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLZNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOEE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXRQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXRQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOEJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXRQ" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXRQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOER" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKORB" varchar(10) NOT NULL default ' ',
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  "AAABHOEV" varchar(1) NOT NULL default ' ',
  "AAABHOEW" varchar(1) NOT NULL default ' ',
  "AAABHOEX" varchar(1) NOT NULL default ' ',
  "AAABHOEY" varchar(1) NOT NULL default ' ',
  "AAABHOEZ" varchar(1) NOT NULL default ' ',
  "AAABHOFA" varchar(1) NOT NULL default ' ',
  "AAABHOFB" varchar(10) NOT NULL default ' ',
  "AAABHOFC" varchar(1) NOT NULL default ' ',
  "AAABHOFD" varchar(1) NOT NULL default ' ',
  "AAABHOFE" varchar(1) NOT NULL default ' ',
  "AAABHOFF" varchar(1) NOT NULL default ' ',
  "AAABHOFG" varchar(10) NOT NULL default ' ',
  "AAABHOFH" varchar(7) NOT NULL default ' ',
  "AAABLVJQ" varchar(6) NOT NULL default ' ',
  "AAABHPPS" varchar(2) NOT NULL default ' ',
  "AAABHOFK" decimal(11,0) NOT NULL default '0',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABHOFM" varchar(25) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABLWQI" varchar(2) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABHOFT" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKORB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOFW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKORB" varchar(10) NOT NULL default ' ',
  "AAABMAHQ" varchar(30) NOT NULL default ' ',
  "AAABMIUM" varchar(11) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKORB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOGF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  "AAABHPXH" varchar(16) NOT NULL default ' ',
  "AAABHPXI" varchar(16) NOT NULL default ' ',
  "AAABHPXJ" varchar(16) NOT NULL default ' ',
  "AAABHPXK" varchar(16) NOT NULL default ' ',
  "AAABHQCF" varchar(4) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABHOGR" varchar(1) NOT NULL default ' ',
  "AAABHOGS" varchar(2) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  "AAABLUHY" varchar(1) NOT NULL default ' ',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABHPXX" varchar(16) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABHOHD" varchar(1) NOT NULL default ' ',
  "AAABHOHE" varchar(1) NOT NULL default ' ',
  "AAABHOHF" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMQII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOHI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABHVDW" varchar(2) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLEZ","AAABMCVI","AAABHVDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOHS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABHQCB" varchar(2) NOT NULL default ' ',
  "AAABHQCD" varchar(2) NOT NULL default ' ',
  "AAABHQCH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOIC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMQII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOIK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABLHLU" varchar(2) NOT NULL default ' ',
  "AAABHQCG" varchar(2) NOT NULL default ' ',
  "AAABHQCA" varchar(2) NOT NULL default ' ',
  "AAABHVKG" varchar(4) NOT NULL default ' ',
  "AAABLHLV" varchar(1) NOT NULL default ' ',
  "AAABLHLW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOIX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOJH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHOLK" varchar(4) NOT NULL default ' ',
  "AAABKWRH" decimal(3,0) NOT NULL default '0',
  "AAABHOJL" varchar(1) NOT NULL default ' ',
  "AAABHOJM" varchar(2) NOT NULL default ' ',
  "AAABHOJN" decimal(3,0) NOT NULL default '0',
  "AAABHOJO" varchar(1) NOT NULL default ' ',
  "AAABHOJP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHOLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOJS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHOLK" varchar(4) NOT NULL default ' ',
  "AAABMAGQ" varchar(40) NOT NULL default ' ',
  "AAABHOJX" varchar(60) NOT NULL default ' ',
  "AAABHOJY" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHOLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOKC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJRPY" varchar(4) NOT NULL default ' ',
  "AAABHOKF" varchar(8) NOT NULL default '00000000',
  "AAABHOKG" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABJRPY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOKJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJRPY" varchar(4) NOT NULL default ' ',
  "AAABHOKN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJRPY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOKR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKSRT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKSRT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKSRT" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKSRT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOLE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJRPY" varchar(4) NOT NULL default ' ',
  "AAABHOLK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJRPY","AAABHOLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOLL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUKA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOLQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUKA" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOLY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOMH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABLWQI" varchar(2) NOT NULL default ' ',
  "AAABHOML" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLEZ","AAABLWQI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOMP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABLWQI" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLEZ","AAABLWQI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOMZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHONR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHONR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHONE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHONR" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHONR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHONM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHONT" varchar(3) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABHONQ" decimal(15,3) NOT NULL default '0.000',
  "AAABHONR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHONT","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHONV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHOVQ" varchar(2) NOT NULL default ' ',
  "AAABMCGA" varchar(4) NOT NULL default ' ',
  "AAABMCGB" varchar(4) NOT NULL default ' ',
  "AAABHOVM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHOVQ","AAABMCGA","AAABMCGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOOF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZZR" varchar(4) NOT NULL default ' ',
  "AAABMDBN" varchar(4) NOT NULL default ' ',
  "AAABHOOJ" varchar(4) NOT NULL default ' ',
  "AAABHOSP" varchar(8) NOT NULL default ' ',
  "AAABHOOL" varchar(20) NOT NULL default ' ',
  "AAABHYJM" varchar(2) NOT NULL default ' ',
  "AAABHYJN" varchar(2) NOT NULL default ' ',
  "AAABHOOO" varchar(1) NOT NULL default ' ',
  "AAABHOOP" varchar(6) NOT NULL default '000000',
  "AAABLKXQ" varchar(4) NOT NULL default ' ',
  "AAABHOOR" varchar(1) NOT NULL default ' ',
  "AAABJRCI" varchar(4) NOT NULL default ' ',
  "AAABHOOT" varchar(3) NOT NULL default '000',
  "AAABHOOU" varchar(3) NOT NULL default ' ',
  "AAABHOOV" varchar(4) NOT NULL default ' ',
  "AAABHOOW" varchar(1) NOT NULL default ' ',
  "AAABHOOX" varchar(1) NOT NULL default ' ',
  "AAABLZUS" varchar(1) NOT NULL default ' ',
  "AAABLYCG" varchar(2) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABHOPE" varchar(2) NOT NULL default ' ',
  "AAABHOPF" varchar(2) NOT NULL default ' ',
  "AAABHOPG" varchar(2) NOT NULL default ' ',
  "AAABHOPH" varchar(2) NOT NULL default ' ',
  "AAABHOPI" varchar(2) NOT NULL default ' ',
  "AAABKXWQ" varchar(2) NOT NULL default ' ',
  "AAABHPPU" varchar(1) NOT NULL default ' ',
  "AAABHQFG" varchar(7) NOT NULL default ' ',
  "AAABHOPM" varchar(1) NOT NULL default ' ',
  "AAABHOPN" varchar(1) NOT NULL default ' ',
  "AAABHOPO" varchar(6) NOT NULL default ' ',
  "AAABJLIP" varchar(2) NOT NULL default ' ',
  "AAABHOPQ" varchar(1) NOT NULL default ' ',
  "AAABHOPR" varchar(1) NOT NULL default ' ',
  "AAABHOPS" varchar(1) NOT NULL default ' ',
  "AAABHOPT" varchar(4) NOT NULL default ' ',
  "AAABHOPU" varchar(1) NOT NULL default ' ',
  "AAABHOPV" varchar(1) NOT NULL default ' ',
  "AAABHOPW" varchar(1) NOT NULL default ' ',
  "AAABHOPX" varchar(1) NOT NULL default ' ',
  "AAABHOPY" varchar(1) NOT NULL default ' ',
  "AAABHOPZ" varchar(1) NOT NULL default ' ',
  "AAABKYEV" varchar(10) NOT NULL default ' ',
  "AAABHOTI" varchar(3) NOT NULL default ' ',
  "AAABHOQC" varchar(1) NOT NULL default ' ',
  "AAABHOQD" varchar(1) NOT NULL default ' ',
  "AAABHOQE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOQH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHOTZ" varchar(1) NOT NULL default ' ',
  "AAABHOUA" varchar(10) NOT NULL default ' ',
  "AAABHOTY" varchar(4) NOT NULL default ' ',
  "AAABLZZR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHOTZ","AAABHOUA","AAABHOTY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOQR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABLZZR" varchar(4) NOT NULL default ' ',
  "AAABHYJM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABLZZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOQZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLZZR" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLZZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHORH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYGM" varchar(10) NOT NULL default ' ',
  "AAABHORO" varchar(4) NOT NULL default ' ',
  "AAABHORL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYGM","AAABHORO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHORP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHOVQ" varchar(2) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABHOVN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHOVQ","AAABMCVI","AAABMCEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHORZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLGN" varchar(4) NOT NULL default ' ',
  "AAABHOSC" varchar(1) NOT NULL default ' ',
  "AAABHOSD" varchar(1) NOT NULL default ' ',
  "AAABHOSE" varchar(1) NOT NULL default ' ',
  "AAABHOSF" varchar(1) NOT NULL default ' ',
  "AAABHOSG" varchar(1) NOT NULL default ' ',
  "AAABHOSH" varchar(1) NOT NULL default ' ',
  "AAABLHRA" varchar(1) NOT NULL default ' ',
  "AAABHOSJ" varchar(1) NOT NULL default ' ',
  "AAABHOSK" varchar(1) NOT NULL default ' ',
  "AAABHOSL" varchar(1) NOT NULL default ' ',
  "AAABMDBO" varchar(4) NOT NULL default ' ',
  "AAABHOSN" varchar(4) NOT NULL default ' ',
  "AAABHOSO" varchar(1) NOT NULL default ' ',
  "AAABHOSP" varchar(8) NOT NULL default ' ',
  "AAABLKXQ" varchar(4) NOT NULL default ' ',
  "AAABHOSR" varchar(1) NOT NULL default ' ',
  "AAABLYCG" varchar(2) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABHOSU" varchar(2) NOT NULL default ' ',
  "AAABHOSV" varchar(1) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABHOSX" varchar(1) NOT NULL default ' ',
  "AAABHOSY" varchar(1) NOT NULL default ' ',
  "AAABHOSZ" varchar(1) NOT NULL default ' ',
  "AAABHOTA" varchar(1) NOT NULL default ' ',
  "AAABHOTB" varchar(1) NOT NULL default ' ',
  "AAABHOTC" varchar(1) NOT NULL default ' ',
  "AAABJLIP" varchar(2) NOT NULL default ' ',
  "AAABHOTE" varchar(2) NOT NULL default ' ',
  "AAABHOTF" varchar(4) NOT NULL default ' ',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABLZUS" varchar(1) NOT NULL default ' ',
  "AAABHOTI" varchar(3) NOT NULL default ' ',
  "AAABKSTZ" varchar(1) NOT NULL default ' ',
  "AAABHOTK" varchar(1) NOT NULL default ' ',
  "AAABHOTL" varchar(1) NOT NULL default ' ',
  "AAABHOTM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOTP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHOTX" varchar(4) NOT NULL default ' ',
  "AAABHOTY" varchar(4) NOT NULL default ' ',
  "AAABHOTZ" varchar(1) NOT NULL default ' ',
  "AAABHOUA" varchar(10) NOT NULL default ' ',
  "AAABMLGN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHOTX","AAABHOTY","AAABHOTZ","AAABHOUA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOUB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABMLGN" varchar(4) NOT NULL default ' ',
  "AAABHOUF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABMLGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOUJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKSPV" varchar(2) NOT NULL default ' ',
  "AAABHOUM" varchar(1) NOT NULL default ' ',
  "AAABHOUN" varchar(1) NOT NULL default ' ',
  "AAABHOUO" varchar(1) NOT NULL default ' ',
  "AAABHOUP" varchar(1) NOT NULL default ' ',
  "AAABHOUQ" varchar(1) NOT NULL default ' ',
  "AAABHOUR" varchar(1) NOT NULL default ' ',
  "AAABHOUS" varchar(1) NOT NULL default ' ',
  "AAABHOUT" varchar(1) NOT NULL default ' ',
  "AAABHOUU" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABKSPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOUX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKSPV" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKSPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOVF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHOVQ" varchar(2) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABMCGA" varchar(4) NOT NULL default ' ',
  "AAABMCGB" varchar(4) NOT NULL default ' ',
  "AAABHOVM" varchar(4) NOT NULL default ' ',
  "AAABHOVN" varchar(4) NOT NULL default ' ',
  "AAABHOVO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHOVQ","AAABMCVI","AAABMCEO","AAABMCGA","AAABMCGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOVV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXKB" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOWA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXKB" varchar(3) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOWI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJRPT" varchar(2) NOT NULL default ' ',
  "AAABJRPR" varchar(3) NOT NULL default '000',
  "AAABJRPS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJRPT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOWP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJRPT" varchar(2) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJRPT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOWX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXSN" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXSN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOXC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXSN" varchar(3) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXSN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOXK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXSO" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXSO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOXP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXSO" varchar(3) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXSO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOXX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXSP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOYC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXSP" varchar(3) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXSQ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXSQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOYP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXSQ" varchar(3) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXSQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOYX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXSR" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOZC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXSR" varchar(3) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHOZK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMACA" varchar(2) NOT NULL default ' ',
  "AAABHOZN" varchar(1) NOT NULL default ' ',
  "AAABHOZO" varchar(1) NOT NULL default ' ',
  "AAABHOZP" varchar(1) NOT NULL default ' ',
  "AAABHOZQ" varchar(1) NOT NULL default ' ',
  "AAABHOZR" varchar(1) NOT NULL default ' ',
  "AAABHOZS" varchar(1) NOT NULL default ' ',
  "AAABHOZT" varchar(1) NOT NULL default ' ',
  "AAABHOZU" varchar(1) NOT NULL default ' ',
  "AAABHOZV" varchar(1) NOT NULL default ' ',
  "AAABHOZW" varchar(1) NOT NULL default ' ',
  "AAABHOZX" varchar(1) NOT NULL default ' ',
  "AAABHOZY" varchar(1) NOT NULL default ' ',
  "AAABKSPV" varchar(2) NOT NULL default ' ',
  "AAABHPAA" varchar(1) NOT NULL default ' ',
  "AAABHPAB" varchar(1) NOT NULL default ' ',
  "AAABHPAC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMACA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMACA" varchar(2) NOT NULL default ' ',
  "AAABHPAJ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMACA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPAZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHPAZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPAS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHPAZ" varchar(3) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHPAZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPBA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPBM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHPBM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPBF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHPBM" varchar(2) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHPBM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPBN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPBZ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHPBZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPBS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHPBZ" varchar(2) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHPBZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPCA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPCM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHPCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPCF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHPCM" varchar(2) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHPCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPCN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPCZ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHPCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPCS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHPCZ" varchar(2) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHPCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPDA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPDM" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHPDM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHPDM" varchar(3) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHPDM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPDN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPDZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHPDZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPDS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHPDZ" varchar(3) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHPDZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPEA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPEM" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHPEM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPEF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHPEM" varchar(3) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHPEM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPEN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPEZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHPEZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPES" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHPEZ" varchar(3) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHPEZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPFA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMELB" varchar(2) NOT NULL default ' ',
  "AAABMEGX" varchar(30) NOT NULL default ' ',
  "AAABHPGI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMELB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPFH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABHPFK" varchar(1) NOT NULL default ' ',
  "AAABHPFL" varchar(1) NOT NULL default ' ',
  "AAABLGME" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPFP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLKXQ" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLKXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPFX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMELB" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMELB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPGF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKXQ" varchar(4) NOT NULL default ' ',
  "AAABHPGI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLKXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPGL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPGX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHPGX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPGQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHPGX" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHPGX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPGY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWQ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPHD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPIJ" varchar(22) NOT NULL default ' ',
  "AAABLZMW" varchar(4) NOT NULL default ' ',
  "AAABHPHH" varchar(22) NOT NULL default ' ',
  "AAABHPHI" varchar(22) NOT NULL default ' ',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABHPHL" decimal(13,3) NOT NULL default '0.000',
  "AAABLZJP" varchar(3) NOT NULL default ' ',
  "AAABHPHN" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABLXPV" decimal(5,0) NOT NULL default '0',
  "AAABLXPW" decimal(5,0) NOT NULL default '0',
  "AAABHPHR" decimal(13,3) NOT NULL default '0.000',
  "AAABHPHS" decimal(13,3) NOT NULL default '0.000',
  "AAABHPHT" double NOT NULL default '0',
  "AAABHPHU" double NOT NULL default '0',
  "AAABLYVX" decimal(13,3) NOT NULL default '0.000',
  "AAABLFPM" decimal(13,3) NOT NULL default '0.000',
  "AAABHPHX" decimal(15,3) NOT NULL default '0.000',
  "AAABHPHY" decimal(15,3) NOT NULL default '0.000',
  "AAABHPIP" varchar(1) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLXOC" varchar(40) NOT NULL default ' ',
  "AAABHPIC" varchar(8) NOT NULL default '00000000',
  "AAABHPID" varchar(6) NOT NULL default '000000',
  "AAABLGHQ" varchar(8) NOT NULL default '00000000',
  "AAABHPIF" varchar(6) NOT NULL default '000000',
  "AAABHPIG" varchar(1) NOT NULL default ' ',
  "AAABHPIH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHPIJ","AAABLZMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPIL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZMW" varchar(4) NOT NULL default ' ',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABHPIP" varchar(1) NOT NULL default ' ',
  "AAABHPIQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPIT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZMW" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHPIX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZMW","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPJB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJGWQ" varchar(3) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJGWQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPJJ" (
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABHPJW" varchar(4) NOT NULL default ' ',
  "AAABHPJX" varchar(10) NOT NULL default '0000000000',
  "AAABHPJO" varchar(4) NOT NULL default ' ',
  "AAABHPJP" int(11) NOT NULL default '0',
  "AAABHPJQ" varchar(4) NOT NULL default '0000',
  "AAABHPJR" int(11) NOT NULL default '0',
  "AAABHPJS" varchar(8) NOT NULL default ' ',
  "AAABHPJT" double NOT NULL default '0',
  PRIMARY KEY ("AAABHPJU","AAABHPJV","AAABHPJW","AAABHPJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPJY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXRS" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXRS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPKD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXRS" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXRS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPKL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLGN" varchar(4) NOT NULL default ' ',
  "AAABHPKO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPKR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPLD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHPLD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHPLD" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHPLD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPLE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYRW" varchar(6) NOT NULL default ' ',
  "AAABKNPX" varchar(4) NOT NULL default '0000',
  "AAABHPRJ" varchar(3) NOT NULL default '000',
  "AAABHPLJ" varchar(1) NOT NULL default '0',
  "AAABHPWV" varchar(10) NOT NULL default ' ',
  "AAABHPWY" varchar(10) NOT NULL default ' ',
  "AAABHPLM" varchar(10) NOT NULL default ' ',
  "AAABKYJW" decimal(11,0) NOT NULL default '0',
  "AAABKYJX" decimal(11,0) NOT NULL default '0',
  "AAABKYJJ" decimal(13,3) NOT NULL default '0.000',
  "AAABKYJK" varchar(3) NOT NULL default ' ',
  "AAABKYJI" varchar(10) NOT NULL default ' ',
  "AAABKYIH" varchar(1) NOT NULL default ' ',
  "AAABLBCC" varchar(4) NOT NULL default ' ',
  "AAABKYHV" varchar(1) NOT NULL default ' ',
  "AAABHPPS" varchar(2) NOT NULL default ' ',
  "AAABHPPV" varchar(10) NOT NULL default ' ',
  "AAABLHXQ" varchar(2) NOT NULL default ' ',
  "AAABKYKB" varchar(1) NOT NULL default ' ',
  "AAABKYKC" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYRW","AAABKNPX"),
  KEY "AAABHPMD" ("AAABMRIL","AAABLYRW","AAABHPRJ"),
  KEY "AAABHPMH" ("AAABMRIL","AAABHPWV","AAABHPWY"),
  KEY "AAABHPML" ("AAABMRIL","AAABHPWY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPMO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPNG" varchar(2) NOT NULL default ' ',
  "AAABHPMR" varchar(2) NOT NULL default ' ',
  "AAABHPMS" decimal(3,0) NOT NULL default '0',
  "AAABHPMT" varchar(1) NOT NULL default ' ',
  "AAABHPMU" varchar(1) NOT NULL default ' ',
  "AAABHPMV" varchar(2) NOT NULL default ' ',
  "AAABHPMW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHPNG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPMZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHPNG" varchar(2) NOT NULL default ' ',
  "AAABMAGQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHPNG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPNH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPNU" varchar(1) NOT NULL default ' ',
  "AAABJAGZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHPNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPNN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHPNU" varchar(1) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHPNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPNV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHQRQ" varchar(4) NOT NULL default ' ',
  "AAABHPNY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHQRQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHQRQ" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHQRQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPOJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHQRQ" varchar(4) NOT NULL default ' ',
  "AAABLXFC" varchar(3) NOT NULL default ' ',
  "AAABLOLZ" varchar(3) NOT NULL default ' ',
  "AAABHPOO" varchar(2) NOT NULL default ' ',
  "AAABHPOP" varchar(1) NOT NULL default ' ',
  "AAABLPGG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHQRQ","AAABLXFC","AAABLOLZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPOV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOLZ" varchar(3) NOT NULL default ' ',
  "AAABHPOY" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOLZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPPB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLOLZ" varchar(3) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  "AAABHPPG" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLOLZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPPK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYRW" varchar(6) NOT NULL default ' ',
  "AAABHPPN" decimal(5,2) NOT NULL default '0.00',
  "AAABHPPO" decimal(11,0) NOT NULL default '0',
  "AAABHPPP" decimal(5,2) NOT NULL default '0.00',
  "AAABHPPQ" decimal(11,0) NOT NULL default '0',
  "AAABHPPR" decimal(11,0) NOT NULL default '0',
  "AAABHPPS" varchar(2) NOT NULL default ' ',
  "AAABHPPT" varchar(1) NOT NULL default ' ',
  "AAABHPPU" varchar(1) NOT NULL default ' ',
  "AAABHPPV" varchar(10) NOT NULL default ' ',
  "AAABKYJX" decimal(11,0) NOT NULL default '0',
  "AAABKYJJ" decimal(13,3) NOT NULL default '0.000',
  "AAABKYJK" varchar(3) NOT NULL default ' ',
  "AAABLHXQ" varchar(2) NOT NULL default ' ',
  "AAABKYCD" varchar(2) NOT NULL default ' ',
  "AAABKYCE" varchar(2) NOT NULL default ' ',
  "AAABKYJI" varchar(10) NOT NULL default ' ',
  "AAABHPQD" varchar(100) NOT NULL default ' ',
  "AAABHPQE" varchar(1) NOT NULL default ' ',
  "AAABKYGK" decimal(8,0) NOT NULL default '0',
  "AAABHPQG" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPQJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLYRW" varchar(6) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLYRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPQR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABMMXA" varchar(10) NOT NULL default ' ',
  "AAABJJNU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABLZBM","AAABMMXA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPRB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYRW" varchar(6) NOT NULL default ' ',
  "AAABHPRJ" varchar(3) NOT NULL default '000',
  "AAABHPWS" varchar(5) NOT NULL default ' ',
  "AAABLHXQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYRW","AAABHPRJ"),
  KEY "AAABHPRK" ("AAABMRIL","AAABHPWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPRN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLEP" varchar(1) NOT NULL default ' ',
  "AAABHYJM" varchar(2) NOT NULL default ' ',
  "AAABHPRR" varchar(1) NOT NULL default ' ',
  "AAABHPRS" varchar(1) NOT NULL default ' ',
  "AAABHPRT" varchar(1) NOT NULL default ' ',
  "AAABHPRU" varchar(1) NOT NULL default ' ',
  "AAABHPRV" varchar(1) NOT NULL default ' ',
  "AAABHPRW" varchar(1) NOT NULL default ' ',
  "AAABHPRX" varchar(1) NOT NULL default ' ',
  "AAABHPRY" varchar(1) NOT NULL default ' ',
  "AAABHPRZ" varchar(1) NOT NULL default ' ',
  "AAABHPSA" varchar(1) NOT NULL default ' ',
  "AAABHPSB" varchar(1) NOT NULL default ' ',
  "AAABHPSC" varchar(1) NOT NULL default ' ',
  "AAABHPSD" varchar(1) NOT NULL default ' ',
  "AAABJBVF" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPSK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLBCC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLBCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPSP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLBCC" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLBCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPSX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLEP" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPTF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXIP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXIP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPTK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXIP" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXIP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPTS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPUE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHPUE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPTX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHPUE" varchar(1) NOT NULL default ' ',
  "AAABHYIY" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHPUE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPUF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPUR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHPUR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPUK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHPUR" varchar(2) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHPUR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPUS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLHP" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPVA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPVC" varchar(1) NOT NULL default ' ',
  "AAABHPVD" varchar(1) NOT NULL default ' ',
  "AAABHPVE" varchar(1) NOT NULL default ' ',
  "AAABHPVF" varchar(1) NOT NULL default ' ',
  "AAABHPVG" varchar(1) NOT NULL default ' ',
  "AAABHPVH" varchar(1) NOT NULL default ' ',
  "AAABHPVI" varchar(1) NOT NULL default ' ',
  "AAABHPVJ" varchar(1) NOT NULL default ' ',
  "AAABHPVK" varchar(1) NOT NULL default ' ',
  "AAABHPVL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPVN" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLDNL" varchar(30) NOT NULL default ' ',
  "AAABJLIY" varchar(30) NOT NULL default ' ',
  "AAABLRBI" varchar(1) NOT NULL default ' ',
  "AAABHPVY" varchar(1) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLDNL","AAABJLIY","AAABLRBI","AAABHPVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPVZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPWL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPWE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHPWL" varchar(1) NOT NULL default ' ',
  "AAABHYIY" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPWM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHPWS" varchar(5) NOT NULL default ' ',
  "AAABHPWV" varchar(10) NOT NULL default ' ',
  "AAABHPWY" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHPWS"),
  KEY "AAABHPWT" ("AAABMRIL","AAABHPWV"),
  KEY "AAABHPWW" ("AAABMRIL","AAABHPWY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPWZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABLHMG" varchar(2) NOT NULL default ' ',
  "AAABHPXD" decimal(5,2) NOT NULL default '0.00',
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  "AAABLXHD" varchar(3) NOT NULL default ' ',
  "AAABHPXG" varchar(10) NOT NULL default ' ',
  "AAABHPXH" varchar(16) NOT NULL default ' ',
  "AAABHPXI" varchar(16) NOT NULL default ' ',
  "AAABHPXJ" varchar(16) NOT NULL default ' ',
  "AAABHPXK" varchar(16) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLOVD" decimal(2,0) NOT NULL default '0',
  "AAABHPXO" varchar(1) NOT NULL default ' ',
  "AAABMEKY" varchar(1) NOT NULL default ' ',
  "AAABHPXQ" varchar(1) NOT NULL default ' ',
  "AAABHPXR" varchar(1) NOT NULL default ' ',
  "AAABHPXS" decimal(5,2) NOT NULL default '0.00',
  "AAABHPXT" decimal(5,2) NOT NULL default '0.00',
  "AAABHPXU" varchar(1) NOT NULL default ' ',
  "AAABHPXV" varchar(10) NOT NULL default ' ',
  "AAABHPXW" varchar(1) NOT NULL default ' ',
  "AAABHPXX" varchar(16) NOT NULL default ' ',
  "AAABHPXY" varchar(1) NOT NULL default ' ',
  "AAABHPXZ" decimal(11,0) NOT NULL default '0',
  "AAABHPYA" decimal(11,0) NOT NULL default '0',
  "AAABHPYB" decimal(11,0) NOT NULL default '0',
  "AAABHPYC" decimal(11,0) NOT NULL default '0',
  "AAABHPYD" varchar(10) NOT NULL default ' ',
  "AAABHPYE" decimal(11,0) NOT NULL default '0',
  "AAABHPYF" decimal(11,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMLEZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPYI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABHPYR" varchar(2) NOT NULL default '00',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABMRFO" varchar(5) NOT NULL default ' ',
  "AAABHPYO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLHQ","AAABHPYR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPYS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLEZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXIP" varchar(2) NOT NULL default ' ',
  "AAABHQAS" varchar(4) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABHQAF" varchar(4) NOT NULL default ' ',
  "AAABHQAG" varchar(4) NOT NULL default ' ',
  "AAABHQAH" varchar(4) NOT NULL default ' ',
  "AAABHQAI" varchar(4) NOT NULL default ' ',
  "AAABHQAJ" varchar(4) NOT NULL default ' ',
  "AAABHQAK" varchar(4) NOT NULL default ' ',
  "AAABHQAL" varchar(4) NOT NULL default ' ',
  "AAABHQAM" varchar(4) NOT NULL default ' ',
  "AAABHQAN" varchar(4) NOT NULL default ' ',
  "AAABHQAO" varchar(4) NOT NULL default ' ',
  "AAABHQAP" varchar(4) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXIP","AAABHQAS","AAABMCVI","AAABMCEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHPZY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXIP" varchar(2) NOT NULL default ' ',
  "AAABHQAS" varchar(4) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABHQAF" varchar(4) NOT NULL default ' ',
  "AAABHQAG" varchar(4) NOT NULL default ' ',
  "AAABHQAH" varchar(4) NOT NULL default ' ',
  "AAABHQAI" varchar(4) NOT NULL default ' ',
  "AAABHQAJ" varchar(4) NOT NULL default ' ',
  "AAABHQAK" varchar(4) NOT NULL default ' ',
  "AAABHQAL" varchar(4) NOT NULL default ' ',
  "AAABHQAM" varchar(4) NOT NULL default ' ',
  "AAABHQAN" varchar(4) NOT NULL default ' ',
  "AAABHQAO" varchar(4) NOT NULL default ' ',
  "AAABHQAP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXIP","AAABHQAS","AAABMCVI","AAABMCEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQAV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXST" varchar(4) NOT NULL default ' ',
  "AAABHQAY" varchar(1) NOT NULL default ' ',
  "AAABHQAZ" varchar(1) NOT NULL default ' ',
  "AAABHQBA" varchar(1) NOT NULL default ' ',
  "AAABHQBB" varchar(1) NOT NULL default ' ',
  "AAABHQBC" varchar(1) NOT NULL default ' ',
  "AAABHQBD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQBG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXST" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQBO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABMLEZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQBV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABHQCA" varchar(2) NOT NULL default ' ',
  "AAABHQCB" varchar(2) NOT NULL default ' ',
  "AAABLHLU" varchar(2) NOT NULL default ' ',
  "AAABHQCD" varchar(2) NOT NULL default ' ',
  "AAABMCUX" varchar(2) NOT NULL default ' ',
  "AAABHQCF" varchar(4) NOT NULL default ' ',
  "AAABHQCG" varchar(2) NOT NULL default ' ',
  "AAABHQCH" varchar(2) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABHQCJ" varchar(3) NOT NULL default '000',
  "AAABHQCK" varchar(1) NOT NULL default ' ',
  "AAABLXSL" varchar(1) NOT NULL default ' ',
  "AAABHQCM" varchar(1) NOT NULL default ' ',
  "AAABMCZV" varchar(4) NOT NULL default ' ',
  "AAABHQCO" varchar(4) NOT NULL default ' ',
  "AAABHQCP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQCU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHQDG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHQDG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQCZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHQDG" varchar(4) NOT NULL default ' ',
  "AAABHQDD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHQDG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQDH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMV" varchar(4) NOT NULL default ' ',
  "AAABLZUS" varchar(1) NOT NULL default ' ',
  "AAABHYJM" varchar(2) NOT NULL default ' ',
  "AAABHYJN" varchar(2) NOT NULL default ' ',
  "AAABMDBN" varchar(4) NOT NULL default ' ',
  "AAABKYDT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQDR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMV" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQDZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKORD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKORD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQEE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKORD" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKORD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQEM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZFC" varchar(4) NOT NULL default ' ',
  "AAABLZUS" varchar(1) NOT NULL default ' ',
  "AAABHYJM" varchar(2) NOT NULL default ' ',
  "AAABHYJN" varchar(2) NOT NULL default ' ',
  "AAABMDBN" varchar(4) NOT NULL default ' ',
  "AAABKYBY" varchar(1) NOT NULL default ' ',
  "AAABKYBZ" varchar(1) NOT NULL default ' ',
  "AAABKYCA" varchar(1) NOT NULL default ' ',
  "AAABKYCB" varchar(1) NOT NULL default ' ',
  "AAABLHXQ" varchar(2) NOT NULL default ' ',
  "AAABKYCD" varchar(2) NOT NULL default ' ',
  "AAABKYCE" varchar(2) NOT NULL default ' ',
  "AAABKYIH" varchar(1) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABLYCG" varchar(2) NOT NULL default ' ',
  "AAABLKXQ" varchar(4) NOT NULL default ' ',
  "AAABHQFF" varchar(14) NOT NULL default ' ',
  "AAABHQFG" varchar(7) NOT NULL default ' ',
  "AAABKYDX" varchar(3) NOT NULL default ' ',
  "AAABKYDY" varchar(3) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABHQFK" varchar(1) NOT NULL default ' ',
  "AAABHQFL" varchar(1) NOT NULL default ' ',
  "AAABHQFM" varchar(14) NOT NULL default ' ',
  "AAABMAMV" varchar(4) NOT NULL default ' ',
  "AAABKYAT" varchar(2) NOT NULL default ' ',
  "AAABKYKB" varchar(1) NOT NULL default ' ',
  "AAABHQFQ" varchar(1) NOT NULL default ' ',
  "AAABHQFR" varchar(1) NOT NULL default ' ',
  "AAABHQFS" varchar(1) NOT NULL default ' ',
  "AAABHQFT" varchar(1) NOT NULL default ' ',
  "AAABHQFU" varchar(1) NOT NULL default ' ',
  "AAABHQFV" varchar(14) NOT NULL default ' ',
  "AAABHQFW" varchar(14) NOT NULL default ' ',
  "AAABHQFX" varchar(14) NOT NULL default ' ',
  "AAABHQFY" varchar(14) NOT NULL default ' ',
  "AAABHQFZ" varchar(14) NOT NULL default ' ',
  "AAABHQGA" varchar(14) NOT NULL default ' ',
  "AAABHQGB" varchar(14) NOT NULL default ' ',
  "AAABKYEV" varchar(10) NOT NULL default ' ',
  "AAABHQGD" varchar(1) NOT NULL default ' ',
  "AAABHQGE" varchar(1) NOT NULL default ' ',
  "AAABHQGF" varchar(3) NOT NULL default '000',
  "AAABHQGG" varchar(4) NOT NULL default ' ',
  "AAABHQGH" varchar(4) NOT NULL default ' ',
  "AAABKYGJ" varchar(1) NOT NULL default ' ',
  "AAABHQGJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQGM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLZFC" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLZFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQGU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHQHH" varchar(2) NOT NULL default ' ',
  "AAABHQGX" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHQHH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQHA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHQHH" varchar(2) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHQHH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQHI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMJXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQHN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMJXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQHV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKXWS" varchar(4) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMLFX" varchar(4) NOT NULL default '0000',
  "AAABHQIC" varchar(1) NOT NULL default ' ',
  "AAABJSAB" varchar(1) NOT NULL default ' ',
  "AAABHQIE" varchar(3) NOT NULL default '000',
  "AAABHQIF" varchar(1) NOT NULL default ' ',
  "AAABHQIG" varchar(1) NOT NULL default ' ',
  "AAABHQIH" varchar(2) NOT NULL default ' ',
  "AAABHQII" varchar(2) NOT NULL default ' ',
  "AAABHQIJ" varchar(1) NOT NULL default ' ',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABHQIL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKXWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQIO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKXWS" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKXWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKORC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKORC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQJB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKORC" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKORC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQJJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYJI" varchar(10) NOT NULL default ' ',
  "AAABHQKH" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYJI","AAABHQKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQJQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYJI" varchar(10) NOT NULL default ' ',
  "AAABLZFC" varchar(4) NOT NULL default ' ',
  "AAABHQKH" varchar(14) NOT NULL default ' ',
  "AAABHQJV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYJI","AAABLZFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQJZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYJI" varchar(10) NOT NULL default ' ',
  "AAABHQKH" varchar(14) NOT NULL default ' ',
  "AAABHQKD" varchar(1) NOT NULL default ' ',
  "AAABHQKE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYJI","AAABHQKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQKI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYJI" varchar(10) NOT NULL default ' ',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABLZFC" varchar(4) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYJI","AAABLZFD","AAABLZFC","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYJI" varchar(10) NOT NULL default ' ',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYJI","AAABLXIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQLA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJLIP" varchar(2) NOT NULL default ' ',
  "AAABHQMS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJLIP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQLG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJLIP" varchar(2) NOT NULL default ' ',
  "AAABLDNL" varchar(30) NOT NULL default ' ',
  "AAABJLIY" varchar(30) NOT NULL default ' ',
  "AAABJLIQ" varchar(2) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABHQLN" varchar(1) NOT NULL default ' ',
  "AAABMLFX" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABJLIP","AAABLDNL","AAABJLIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQLT" (
  "AAABHQMS" varchar(1) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABHQMS","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQLY" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHQMS" varchar(1) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABHQMC" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABHQMS","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQMG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHQMS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHQMS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQML" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHQMS" varchar(1) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHQMS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQMT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJLIQ" varchar(2) NOT NULL default ' ',
  "AAABJLGG" varchar(1) NOT NULL default ' ',
  "AAABJLGH" varchar(1) NOT NULL default ' ',
  "AAABJLGI" varchar(1) NOT NULL default ' ',
  "AAABJUPC" varchar(1) NOT NULL default ' ',
  "AAABJLGX" varchar(1) NOT NULL default ' ',
  "AAABJLGV" varchar(1) NOT NULL default ' ',
  "AAABJLGW" varchar(1) NOT NULL default ' ',
  "AAABJLEZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJLIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQNG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJLIP" varchar(2) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJLIP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQNO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXJH" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQNT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXJH" varchar(3) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXJI" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXJI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQOG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXJI" varchar(3) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXJI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQOO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXJJ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXJJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQOT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXJJ" varchar(3) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXJJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQPB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXJK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQPG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXJK" varchar(3) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQPO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXJL" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXJL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQPT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXJL" varchar(3) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXJL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQQB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRBX" varchar(4) NOT NULL default ' ',
  "AAABJRPT" varchar(2) NOT NULL default ' ',
  "AAABJRPY" varchar(4) NOT NULL default ' ',
  "AAABJRPZ" varchar(4) NOT NULL default ' ',
  "AAABJRQH" varchar(2) NOT NULL default ' ',
  "AAABJRQO" varchar(2) NOT NULL default ' ',
  "AAABJRQN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRBX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQQM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKRBX" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKRBX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQQU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHQRQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHQRQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQQZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHQRQ" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHQRQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQRH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHQRQ" varchar(4) NOT NULL default ' ',
  "AAABLXFC" varchar(3) NOT NULL default ' ',
  "AAABHQRL" varchar(1) NOT NULL default ' ',
  "AAABHQRM" varchar(1) NOT NULL default ' ',
  "AAABKSQG" varchar(1) NOT NULL default ' ',
  "AAABMNJF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHQRQ","AAABLXFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQRS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJAGZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJAGZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQRX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJAGZ" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJAGZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQSF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQLJ" varchar(3) NOT NULL default ' ',
  "AAABLQLK" varchar(6) NOT NULL default '000000',
  "AAABLQLL" varchar(6) NOT NULL default '000000',
  "AAABLQLM" varchar(6) NOT NULL default '000000',
  "AAABLQLN" varchar(6) NOT NULL default '000000',
  "AAABLQLO" varchar(6) NOT NULL default '000000',
  "AAABLQLP" varchar(6) NOT NULL default '000000',
  "AAABLQLQ" varchar(6) NOT NULL default '000000',
  "AAABLQLR" varchar(6) NOT NULL default '000000',
  "AAABLQLS" varchar(6) NOT NULL default '000000',
  "AAABLQLT" varchar(6) NOT NULL default '000000',
  "AAABLQLU" varchar(6) NOT NULL default '000000',
  "AAABLQLV" varchar(6) NOT NULL default '000000',
  "AAABLQLW" varchar(6) NOT NULL default '000000',
  "AAABLQLX" varchar(6) NOT NULL default '000000',
  "AAABLQLY" varchar(6) NOT NULL default '000000',
  "AAABLQLZ" varchar(6) NOT NULL default '000000',
  "AAABLQMA" varchar(6) NOT NULL default '000000',
  "AAABLQMB" varchar(6) NOT NULL default '000000',
  "AAABLQMC" varchar(6) NOT NULL default '000000',
  "AAABLQMD" varchar(6) NOT NULL default '000000',
  "AAABLQME" varchar(6) NOT NULL default '000000',
  "AAABLQMF" varchar(6) NOT NULL default '000000',
  "AAABLQMG" varchar(6) NOT NULL default '000000',
  "AAABLQMH" varchar(6) NOT NULL default '000000',
  "AAABLQMI" varchar(6) NOT NULL default '000000',
  "AAABLQMJ" varchar(6) NOT NULL default '000000',
  "AAABLQMK" varchar(6) NOT NULL default '000000',
  "AAABLQML" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLQLJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQTM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLQLJ" varchar(3) NOT NULL default ' ',
  "AAABMOHX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLQLJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQTU" (
  "AAABHQUC" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABHQUC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQTX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHQUC" varchar(2) NOT NULL default '00',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHQUC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQUD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMRQ" varchar(4) NOT NULL default ' ',
  "AAABHQUG" varchar(7) NOT NULL default '0000000',
  PRIMARY KEY ("AAABMRIL","AAABMMRQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQUJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMRQ" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMMRQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQUR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXUU" varchar(4) NOT NULL default ' ',
  "AAABHQUU" varchar(1) NOT NULL default ' ',
  "AAABHQUV" varchar(1) NOT NULL default ' ',
  "AAABHQUW" varchar(1) NOT NULL default ' ',
  "AAABHQUX" varchar(3) NOT NULL default '000',
  "AAABHQUY" varchar(1) NOT NULL default ' ',
  "AAABHQUZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXUU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQVC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXUU" varchar(4) NOT NULL default ' ',
  "AAABHQWC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXUU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQVK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHQWL" varchar(3) NOT NULL default ' ',
  "AAABHQVN" varchar(5) NOT NULL default '00000',
  "AAABHQVO" varchar(5) NOT NULL default '00000',
  "AAABHQVP" varchar(5) NOT NULL default '00000',
  "AAABHQVQ" varchar(5) NOT NULL default '00000',
  "AAABHQVR" varchar(5) NOT NULL default '00000',
  "AAABHQVS" varchar(5) NOT NULL default '00000',
  "AAABHQVT" varchar(5) NOT NULL default '00000',
  "AAABHQVU" varchar(1) NOT NULL default ' ',
  "AAABHQVV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHQWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQVY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHQWL" varchar(3) NOT NULL default ' ',
  "AAABHQWC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHQWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQWG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXUU" varchar(4) NOT NULL default ' ',
  "AAABHQWO" varchar(1) NOT NULL default ' ',
  "AAABHQWP" varchar(3) NOT NULL default '000',
  "AAABHQWL" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXUU","AAABHQWO","AAABHQWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQWQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHQWX" varchar(2) NOT NULL default ' ',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABHQWU" decimal(4,1) NOT NULL default '0.0',
  "AAABHQWV" decimal(4,1) NOT NULL default '0.0',
  PRIMARY KEY ("AAABMRIL","AAABHQWX","AAABMCVK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQWZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLAYA" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLAYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQXE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLAYA" varchar(3) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLAYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQXN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHQYA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHQYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQXS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHQYA" varchar(4) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHQYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQYB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKYXJ" varchar(3) NOT NULL default ' ',
  "AAABKZWO" varchar(3) NOT NULL default ' ',
  "AAABKXKU" varchar(2) NOT NULL default '00',
  "AAABKVXO" varchar(3) NOT NULL default ' ',
  "AAABMNDR" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKYXJ","AAABKZWO","AAABKXKU","AAABKVXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQYP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLAGW" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLAGW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQYU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLAGW" varchar(1) NOT NULL default '0',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLAGW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQZD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLAFA" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLAFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQZI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLAFA" varchar(1) NOT NULL default '0',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLAFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQZR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHRAD" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHRAD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHQZW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHRAD" varchar(3) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHRAD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRAE" (
  "AAABHRAI" varchar(20) NOT NULL default ' ',
  "AAABHRAG" varchar(58) NOT NULL default ' ',
  "AAABHRAH" varchar(58) NOT NULL default ' ',
  PRIMARY KEY ("AAABHRAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRAJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLAIE" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABLAIE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRAO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHRAW" varchar(2) NOT NULL default '00',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHRAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRAX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLGJS" varchar(4) NOT NULL default ' ',
  "AAABMDBL" varchar(4) NOT NULL default ' ',
  "AAABHRBB" varchar(3) NOT NULL default '000',
  "AAABHRBC" varchar(3) NOT NULL default '000',
  "AAABLGPK" varchar(1) NOT NULL default ' ',
  "AAABLGPL" varchar(1) NOT NULL default ' ',
  "AAABHRBF" varchar(3) NOT NULL default '000',
  "AAABHRBG" varchar(3) NOT NULL default '000',
  "AAABHRBH" varchar(3) NOT NULL default '000',
  "AAABHRBI" varchar(3) NOT NULL default '000',
  "AAABHYJM" varchar(2) NOT NULL default ' ',
  "AAABHYJN" varchar(2) NOT NULL default ' ',
  "AAABLGLC" varchar(4) NOT NULL default ' ',
  "AAABMDBN" varchar(4) NOT NULL default ' ',
  "AAABHRBN" varchar(4) NOT NULL default ' ',
  "AAABLSHU" varchar(4) NOT NULL default ' ',
  "AAABLGNW" varchar(4) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABLGJZ" varchar(6) NOT NULL default ' ',
  "AAABLGMD" varchar(4) NOT NULL default ' ',
  "AAABLOZZ" varchar(4) NOT NULL default ' ',
  "AAABLPAA" varchar(2) NOT NULL default ' ',
  "AAABLPAC" varchar(10) NOT NULL default ' ',
  "AAABLOLS" varchar(1) NOT NULL default ' ',
  "AAABLGMK" varchar(1) NOT NULL default ' ',
  "AAABLGPS" varchar(1) NOT NULL default ' ',
  "AAABLOLT" varchar(2) NOT NULL default ' ',
  "AAABLOOQ" varchar(2) NOT NULL default ' ',
  "AAABLGMQ" varchar(4) NOT NULL default ' ',
  "AAABLGMR" varchar(4) NOT NULL default ' ',
  "AAABLGQD" varchar(1) NOT NULL default ' ',
  "AAABHRCE" varchar(1) NOT NULL default ' ',
  "AAABLPAB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLGJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRCI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLGJS" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHRCM" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLGJS","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRCQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQWB" varchar(2) NOT NULL default ' ',
  "AAABHRCT" varchar(16) NOT NULL default ' ',
  "AAABHSDA" varchar(2) NOT NULL default ' ',
  "AAABHRCV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRCY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHRDU" varchar(3) NOT NULL default ' ',
  "AAABHRDB" varchar(1) NOT NULL default ' ',
  "AAABHRDC" varchar(1) NOT NULL default ' ',
  "AAABHRDD" varchar(1) NOT NULL default ' ',
  "AAABHRDE" varchar(1) NOT NULL default ' ',
  "AAABHRDF" varchar(2) NOT NULL default '00',
  "AAABHRDG" varchar(6) NOT NULL default '000000',
  "AAABHRDH" varchar(6) NOT NULL default '000000',
  "AAABHRDI" varchar(3) NOT NULL default '000',
  "AAABHRDJ" varchar(1) NOT NULL default ' ',
  "AAABHRDK" decimal(5,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABHRDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRDN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHRDU" varchar(3) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHRDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRDV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLGWV" varchar(2) NOT NULL default ' ',
  "AAABHRDY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLGWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHREB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLGWV" varchar(2) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLGWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHREJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLGZO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLGZO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHREO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLGZO" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHRES" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLGZO","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHREW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHRFB" varchar(3) NOT NULL default ' ',
  "AAABLDCD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHRFB","AAABLDCD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRFD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHBA" varchar(4) NOT NULL default ' ',
  "AAABHRFG" varchar(1) NOT NULL default ' ',
  "AAABHRFY" decimal(15,2) NOT NULL default '0.00',
  "AAABHRFZ" decimal(15,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABHRFK" varchar(1) NOT NULL default ' ',
  "AAABHRGB" varchar(3) NOT NULL default '000',
  "AAABHRFM" varchar(3) NOT NULL default ' ',
  "AAABHRFN" varchar(3) NOT NULL default ' ',
  "AAABHRFO" varchar(3) NOT NULL default ' ',
  "AAABHRFP" varchar(3) NOT NULL default ' ',
  "AAABHRFQ" varchar(3) NOT NULL default ' ',
  "AAABHRFR" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHBA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRFU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHBA" varchar(4) NOT NULL default ' ',
  "AAABHRGE" varchar(5) NOT NULL default ' ',
  "AAABHRFY" decimal(15,2) NOT NULL default '0.00',
  "AAABHRFZ" decimal(15,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABHRGB" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABLHBA","AAABHRGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRGF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHBA" varchar(4) NOT NULL default ' ',
  "AAABLHBI" varchar(4) NOT NULL default ' ',
  "AAABHRGJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHBA","AAABLHBI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRGN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLHBA" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLHBA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRGV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOLU" varchar(4) NOT NULL default ' ',
  "AAABIEWH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOLU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRHB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHRKN" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABHRHF" varchar(3) NOT NULL default ' ',
  "AAABHRHG" varchar(2) NOT NULL default ' ',
  "AAABHRHH" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHRKN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRHK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHRKN" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHRKN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRHS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLSZ" varchar(1) NOT NULL default ' ',
  "AAABLHMO" tinyint(3) unsigned NOT NULL default '0',
  "AAABHRHW" tinyint(3) unsigned NOT NULL default '0',
  "AAABHRHX" smallint(6) NOT NULL default '0',
  "AAABHRHY" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLLSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRIB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJPBG" varchar(3) NOT NULL default '000',
  "AAABLAXH" varchar(4) NOT NULL default ' ',
  "AAABHRIL" varchar(1) NOT NULL default ' ',
  "AAABIEJO" varchar(4) NOT NULL default ' ',
  "AAABIEJP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJPBG","AAABLAXH","AAABHRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRIM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLSZ" varchar(1) NOT NULL default ' ',
  "AAABHRIV" varchar(2) NOT NULL default ' ',
  "AAABHRIQ" tinyint(3) unsigned NOT NULL default '0',
  "AAABHRIR" tinyint(3) unsigned NOT NULL default '0',
  "AAABHRIS" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABLLSZ","AAABHRIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQMZ" varchar(4) NOT NULL default ' ',
  "AAABLLSZ" varchar(1) NOT NULL default ' ',
  "AAABHRJA" varchar(1) NOT NULL default ' ',
  "AAABLHIE" varchar(1) NOT NULL default ' ',
  "AAABLQMQ" varchar(4) NOT NULL default ' ',
  "AAABHRJD" varchar(30) NOT NULL default ' ',
  "AAABLHHP" varchar(30) NOT NULL default ' ',
  "AAABHRJF" smallint(6) NOT NULL default '0',
  "AAABHRJG" varchar(1) NOT NULL default ' ',
  "AAABHRJH" varchar(1) NOT NULL default ' ',
  "AAABHRJI" varchar(1) NOT NULL default ' ',
  "AAABHRJJ" varchar(30) NOT NULL default ' ',
  "AAABHWDX" varchar(1) NOT NULL default ' ',
  "AAABMBTQ" varchar(16) NOT NULL default ' ',
  "AAABMEEK" varchar(4) NOT NULL default ' ',
  "AAABHRJN" varchar(1) NOT NULL default ' ',
  "AAABHRJO" varchar(1) NOT NULL default ' ',
  "AAABHRJP" varchar(1) NOT NULL default ' ',
  "AAABHRJQ" varchar(1) NOT NULL default ' ',
  "AAABHRJR" varchar(1) NOT NULL default ' ',
  "AAABHRJS" varchar(1) NOT NULL default ' ',
  "AAABHRJT" varchar(1) NOT NULL default ' ',
  "AAABHRJU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQMZ","AAABLLSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRJY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLSZ" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLLTA" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLSZ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJBB" varchar(3) NOT NULL default ' ',
  "AAABHRKN" varchar(1) NOT NULL default ' ',
  "AAABHRTC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJBB","AAABHRKN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJBB" varchar(3) NOT NULL default ' ',
  "AAABHRRY" varchar(1) NOT NULL default ' ',
  "AAABHRLI" varchar(2) NOT NULL default ' ',
  "AAABHRKT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHRLI" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHRLI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRLB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHRLI" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHRLI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRLJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLJBB" varchar(3) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLJBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRLR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABLZZR" varchar(4) NOT NULL default ' ',
  "AAABLJBB" varchar(3) NOT NULL default ' ',
  "AAABHRMI" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABLJBC" varchar(1) NOT NULL default ' ',
  "AAABHRMB" varchar(1) NOT NULL default ' ',
  "AAABHRMC" varchar(4) NOT NULL default ' ',
  "AAABHRMD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJBH","AAABLZZR","AAABLJBB","AAABHRMI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRMJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABHRMQ" varchar(1) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJBH","AAABHRMQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRMR" (
  "AAABHRMU" varchar(24) NOT NULL default ' ',
  "AAABLMUZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABHRMU","AAABLMUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRMW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABIMHW" varchar(32) NOT NULL default ' ',
  "AAABHRNL" varchar(32) NOT NULL default ' ',
  "AAABLMUZ" varchar(4) NOT NULL default ' ',
  "AAABHRNC" varchar(8) NOT NULL default ' ',
  "AAABHRND" longtext,
  "AAABHRNE" varchar(32) NOT NULL default ' ',
  "AAABHRNF" varchar(1) NOT NULL default ' ',
  "AAABHRNG" varchar(8) NOT NULL default '00000000',
  "AAABHRNH" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABIMHW","AAABHRNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRNM" (
  "AAABHRNY" varchar(4) NOT NULL default ' ',
  "AAABHRNO" varchar(4) NOT NULL default ' ',
  "AAABHRNP" varchar(75) NOT NULL default ' ',
  "AAABHRNQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHRNY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRNS" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHRNY" varchar(4) NOT NULL default ' ',
  "AAABHRNV" varchar(75) NOT NULL default ' ',
  "AAABHRNW" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABHRNY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRNZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIKPR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIKPR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHROE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIKPR" varchar(4) NOT NULL default ' ',
  "AAABHROI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABIKPR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHROM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHROX" varchar(4) NOT NULL default ' ',
  "AAABHROP" decimal(3,0) NOT NULL default '0',
  "AAABHROQ" decimal(3,0) NOT NULL default '0',
  "AAABHROR" decimal(3,0) NOT NULL default '0',
  "AAABHROS" decimal(4,0) NOT NULL default '0',
  "AAABHROT" decimal(4,1) NOT NULL default '0.0',
  "AAABHROU" decimal(3,0) NOT NULL default '0',
  "AAABLHMG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHROX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHROY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIQZ" varchar(4) NOT NULL default ' ',
  "AAABHRRA" varchar(2) NOT NULL default ' ',
  "AAABHRRO" varchar(2) NOT NULL default ' ',
  "AAABHRPD" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABHRPF" decimal(13,3) NOT NULL default '0.000',
  "AAABMLEY" varchar(3) NOT NULL default ' ',
  "AAABHRPH" decimal(13,3) NOT NULL default '0.000',
  "AAABMLEW" varchar(3) NOT NULL default ' ',
  "AAABHRPJ" decimal(11,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABHRPM" decimal(4,1) NOT NULL default '0.0',
  "AAABHRPN" decimal(13,3) NOT NULL default '0.000',
  "AAABLJFZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLIQZ","AAABHRRA","AAABHRRO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRPT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIQZ" varchar(4) NOT NULL default ' ',
  "AAABHRPW" varchar(1) NOT NULL default ' ',
  "AAABHRPX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLIQZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRQA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLIQZ" varchar(4) NOT NULL default ' ',
  "AAABHRQE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLIQZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRQI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHRRA" varchar(2) NOT NULL default ' ',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABMLEY" varchar(3) NOT NULL default ' ',
  "AAABMLEW" varchar(3) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLJFZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHRRA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRQS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHRRA" varchar(2) NOT NULL default ' ',
  "AAABHRQW" varchar(8) NOT NULL default ' ',
  "AAABHRQX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHRRA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRRB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHRRO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHRRO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRRG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHRRO" varchar(2) NOT NULL default ' ',
  "AAABHRRK" varchar(8) NOT NULL default ' ',
  "AAABHRRL" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHRRO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRRP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQMZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQMZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRRU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJBG" varchar(1) NOT NULL default ' ',
  "AAABHRRX" varchar(1) NOT NULL default ' ',
  "AAABHRRY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRSB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLJBG" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLJBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRSJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHRTC" varchar(1) NOT NULL default ' ',
  "AAABLJBG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHRTC","AAABLJBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHRTC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHRTC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRSV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHRTC" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHRTC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRTD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQMZ" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAYK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQMZ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRTL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABHRTO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJBH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRTR" (
  "AAABMLPV" varchar(25) NOT NULL default ' ',
  "AAABMMJT" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRTV" (
  "AAABHRVE" varchar(20) NOT NULL default ' ',
  "AAABHRUG" varchar(4) NOT NULL default ' ',
  "AAABHRXB" varchar(4) NOT NULL default ' ',
  "AAABHRTZ" varchar(3) NOT NULL default ' ',
  "AAABHRWK" varchar(4) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABHRXA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHRVE","AAABHRUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRUH" (
  "AAABHRVE" varchar(20) NOT NULL default ' ',
  "AAABHRXC" varchar(4) NOT NULL default ' ',
  "AAABHRXD" varchar(24) NOT NULL default ' ',
  "AAABHRXE" varchar(24) NOT NULL default ' ',
  "AAABHRWW" varchar(2) NOT NULL default '00',
  "AAABHRUN" varchar(5) NOT NULL default '00000',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABHRXA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHRVE","AAABHRXC","AAABHRXD","AAABHRXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRUW" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHRVE" varchar(20) NOT NULL default ' ',
  "AAABHRXC" varchar(4) NOT NULL default ' ',
  "AAABHRXD" varchar(24) NOT NULL default ' ',
  "AAABHRXE" varchar(24) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABHRVE","AAABHRXC","AAABHRXD","AAABHRXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRVI" (
  "AAABLMLR" varchar(20) NOT NULL default ' ',
  "AAABHRVK" varchar(8) NOT NULL default '00000000',
  "AAABMKLH" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLMLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRVN" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLMLR" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLMLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRVT" (
  "AAABHRXB" varchar(4) NOT NULL default ' ',
  "AAABHRVV" varchar(20) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABHRXA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHRXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRWB" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHRXE" varchar(24) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABHRXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRWH" (
  "AAABHRXB" varchar(4) NOT NULL default ' ',
  "AAABHRXC" varchar(4) NOT NULL default ' ',
  "AAABHRWK" varchar(4) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABHRXA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHRXB","AAABHRXC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRWR" (
  "AAABHRXB" varchar(4) NOT NULL default ' ',
  "AAABHRXC" varchar(4) NOT NULL default ' ',
  "AAABHRXD" varchar(24) NOT NULL default ' ',
  "AAABHRXE" varchar(24) NOT NULL default ' ',
  "AAABHRWW" varchar(2) NOT NULL default '00',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABHRXA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHRXB","AAABHRXC","AAABHRXD","AAABHRXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRXF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCCC" varchar(20) NOT NULL default ' ',
  "AAABHRXI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRXL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCCC" varchar(20) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCCC","AAABMRHS","AAABMOHX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRXU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHRXZ" varchar(24) NOT NULL default ' ',
  "AAABLLJS" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHRXZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRYA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTQJ" varchar(11) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLTQJ" varchar(11) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLTQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRYN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHRZK" varchar(4) NOT NULL default ' ',
  "AAABHRYQ" varchar(3) NOT NULL default '000',
  "AAABHRYR" varchar(3) NOT NULL default '000',
  "AAABHRYS" varchar(3) NOT NULL default '000',
  "AAABHRYT" varchar(4) NOT NULL default ' ',
  "AAABHRYU" varchar(4) NOT NULL default ' ',
  "AAABHRYV" varchar(4) NOT NULL default ' ',
  "AAABHRYW" varchar(3) NOT NULL default ' ',
  "AAABHRYX" varchar(3) NOT NULL default ' ',
  "AAABHRYY" varchar(3) NOT NULL default ' ',
  "AAABHRYZ" varchar(1) NOT NULL default ' ',
  "AAABHRZA" varchar(1) NOT NULL default ' ',
  "AAABHRZB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHRZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRZE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHRZK" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHRZK","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHRZM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLAYL" varchar(10) NOT NULL default ' ',
  "AAABHRZQ" varchar(15) NOT NULL default ' ',
  "AAABHRZR" varchar(15) NOT NULL default ' ',
  "AAABHRZS" varchar(30) NOT NULL default ' ',
  "AAABHRZT" varchar(10) NOT NULL default ' ',
  "AAABHRZU" varchar(10) NOT NULL default ' ',
  "AAABHRZV" varchar(10) NOT NULL default ' ',
  "AAABHRZW" varchar(2) NOT NULL default ' ',
  "AAABLAYM" varchar(8) NOT NULL default '00000000',
  "AAABHRZY" varchar(4) NOT NULL default ' ',
  "AAABLBYC" varchar(1) NOT NULL default '0',
  "AAABHSAA" varchar(1) NOT NULL default ' ',
  "AAABLBYB" varchar(2) NOT NULL default '00',
  "AAABLBRN" varchar(8) NOT NULL default '00000000',
  "AAABLASX" varchar(10) NOT NULL default ' ',
  "AAABLBRP" varchar(8) NOT NULL default '00000000',
  "AAABLBRQ" varchar(6) NOT NULL default '000000',
  "AAABLZDP" varchar(12) NOT NULL default ' ',
  "AAABLCIO" varchar(8) NOT NULL default '00000000',
  "AAABLCIP" varchar(6) NOT NULL default '000000',
  "AAABLCIQ" varchar(10) NOT NULL default ' ',
  "AAABLCIS" varchar(12) NOT NULL default ' ',
  "AAABLCIT" varchar(8) NOT NULL default '00000000',
  "AAABLCIU" varchar(6) NOT NULL default '000000',
  "AAABLCIV" varchar(10) NOT NULL default ' ',
  "AAABKXGH" varchar(10) NOT NULL default ' ',
  "AAABLDCB" varchar(10) NOT NULL default ' ',
  "AAABHSAQ" varchar(3) NOT NULL default ' ',
  "AAABHSAR" varchar(3) NOT NULL default ' ',
  "AAABLCAV" varchar(1) NOT NULL default '0',
  "AAABLAYA" varchar(3) NOT NULL default ' ',
  "AAABLACY" varchar(5) NOT NULL default ' ',
  "AAABLACZ" varchar(5) NOT NULL default ' ',
  "AAABHSAW" varchar(10) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABJASU" varchar(2) NOT NULL default ' ',
  "AAABHSBA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLAYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSBE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSDI" varchar(4) NOT NULL default ' ',
  "AAABIKPR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSDI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSBK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSDA" varchar(2) NOT NULL default ' ',
  "AAABLMKJ" varchar(1) NOT NULL default ' ',
  "AAABLMKK" varchar(1) NOT NULL default '0',
  "AAABLMKL" varchar(1) NOT NULL default ' ',
  "AAABLMKM" varchar(1) NOT NULL default '0',
  "AAABLMKN" varchar(3) NOT NULL default '000',
  "AAABLMKO" varchar(1) NOT NULL default ' ',
  "AAABLMKP" varchar(1) NOT NULL default ' ',
  "AAABLMKQ" varchar(1) NOT NULL default ' ',
  "AAABLMKR" varchar(1) NOT NULL default ' ',
  "AAABLMKS" varchar(1) NOT NULL default ' ',
  "AAABLMKT" varchar(1) NOT NULL default ' ',
  "AAABLMKU" varchar(1) NOT NULL default ' ',
  "AAABLMKV" varchar(1) NOT NULL default ' ',
  "AAABLMKW" varchar(1) NOT NULL default ' ',
  "AAABLMKX" varchar(1) NOT NULL default ' ',
  "AAABLMKY" varchar(1) NOT NULL default ' ',
  "AAABLMKZ" varchar(3) NOT NULL default '000',
  "AAABLMLA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSCH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSDA" varchar(2) NOT NULL default ' ',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  "AAABLZZR" varchar(4) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABHSCN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSDA","AAABLUFD","AAABLZZR","AAABMLHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSCT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHSDA" varchar(2) NOT NULL default ' ',
  "AAABHSCX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHSDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSDB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHSDI" varchar(4) NOT NULL default ' ',
  "AAABHSDF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHSDI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSDJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLNUH" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLNUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSDO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLNUH" varchar(18) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLNUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSDW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABLQAJ" varchar(2) NOT NULL default ' ',
  "AAABMRFB" varchar(5) NOT NULL default ' ',
  "AAABHXQS" varchar(2) NOT NULL default ' ',
  "AAABLOPF" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABLXOB","AAABLQAJ","AAABMRFB","AAABHXQS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSEM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLNUG" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLNUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSER" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLNUG" varchar(10) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLNUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSEZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSFP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSFP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSFE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHSFP" varchar(2) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHSFP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSFM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLNUI" varchar(4) NOT NULL default ' ',
  "AAABHSFP" varchar(2) NOT NULL default ' ',
  "AAABLROG" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLNUI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSFT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLNUI" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLNUI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSGB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABLOOQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABLOOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSGI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABLOOQ" varchar(2) NOT NULL default ' ',
  "AAABHSGN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMY","AAABLOOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSGS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABLOOQ" varchar(2) NOT NULL default ' ',
  "AAABHSHJ" varchar(2) NOT NULL default '00',
  "AAABLOOR" varchar(1) NOT NULL default ' ',
  "AAABLOOS" varchar(1) NOT NULL default ' ',
  "AAABLOMN" varchar(2) NOT NULL default ' ',
  "AAABLOMO" varchar(6) NOT NULL default ' ',
  "AAABLOOT" varchar(30) NOT NULL default ' ',
  "AAABLOOV" varchar(1) NOT NULL default ' ',
  "AAABLOOW" varchar(5) NOT NULL default ' ',
  "AAABLOOX" varchar(1) NOT NULL default ' ',
  "AAABHSHF" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABLOOQ","AAABHSHJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSHK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDFM" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDFM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSHP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJDFM" varchar(18) NOT NULL default ' ',
  "AAABHSHT" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJDFM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSHX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSIJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSIC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHSIJ" varchar(2) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHSIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSIK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSIV" varchar(10) NOT NULL default ' ',
  "AAABLNZP" varchar(10) NOT NULL default ' ',
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABLUEO" varchar(32) NOT NULL default ' ',
  "AAABHSIQ" varchar(79) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABIKDN" varchar(14) NOT NULL default ' ',
  "AAABHSIT" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSKF" varchar(3) NOT NULL default ' ',
  "AAABHSIZ" varchar(1) NOT NULL default ' ',
  "AAABHSJA" varchar(1) NOT NULL default ' ',
  "AAABHSJB" varchar(2) NOT NULL default ' ',
  "AAABLXHV" varchar(2) NOT NULL default ' ',
  "AAABHSJD" varchar(2) NOT NULL default ' ',
  "AAABHSJE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSJH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSKF" varchar(3) NOT NULL default ' ',
  "AAABHSJX" varchar(2) NOT NULL default ' ',
  "AAABHSJL" varchar(1) NOT NULL default ' ',
  "AAABHSJM" varchar(1) NOT NULL default ' ',
  "AAABHSJN" varchar(1) NOT NULL default ' ',
  "AAABHSJO" varchar(1) NOT NULL default ' ',
  "AAABHSJP" varchar(1) NOT NULL default ' ',
  "AAABHSJQ" varchar(1) NOT NULL default ' ',
  "AAABHSJR" varchar(1) NOT NULL default ' ',
  "AAABHSJS" varchar(1) NOT NULL default ' ',
  "AAABHSJT" varchar(1) NOT NULL default ' ',
  "AAABHSJU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSKF","AAABHSJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSJY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHSKF" varchar(3) NOT NULL default ' ',
  "AAABHSKC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHSKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABHSKS" varchar(12) NOT NULL default ' ',
  "AAABHSKT" varchar(4) NOT NULL default '0000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(12) NOT NULL default ' ',
  "AAABHSKN" varchar(12) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABHSKP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABHSKS","AAABHSKT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSKU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSNY" varchar(4) NOT NULL default ' ',
  "AAABHSKX" varchar(1) NOT NULL default ' ',
  "AAABHSKY" varchar(2) NOT NULL default ' ',
  "AAABHSKZ" varchar(2) NOT NULL default ' ',
  "AAABHSLA" varchar(1) NOT NULL default ' ',
  "AAABHSLB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSNY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSLE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHSNY" varchar(4) NOT NULL default ' ',
  "AAABHSLI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHSNY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSLM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQPD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQPD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSLR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLQPD" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLQPD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSLZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUKQ" varchar(10) NOT NULL default ' ',
  "AAABMLFX" varchar(10) NOT NULL default '0000000000',
  "AAABLRWN" varchar(10) NOT NULL default ' ',
  "AAABHSMO" varchar(10) NOT NULL default '0000000000',
  "AAABHSMF" varchar(3) NOT NULL default '000',
  "AAABHSMG" varchar(10) NOT NULL default '0000000000',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUKQ","AAABMLFX"),
  KEY "AAABHSML" ("AAABMRIL","AAABLUKQ","AAABHSMO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSMP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUKQ" varchar(10) NOT NULL default ' ',
  "AAABMLFX" varchar(10) NOT NULL default '0000000000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUKQ","AAABMLFX","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSMZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABHSNJ" varchar(18) NOT NULL default ' ',
  "AAABHSNK" varchar(18) NOT NULL default ' ',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  "AAABLUPE" varchar(30) NOT NULL default ' ',
  "AAABHSNG" decimal(3,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLTUX","AAABHSNJ","AAABHSNK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSNL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSNN" varchar(1) NOT NULL default ' ',
  "AAABHSNO" varchar(1) NOT NULL default ' ',
  "AAABHSNP" varchar(1) NOT NULL default ' ',
  "AAABHSNQ" varchar(1) NOT NULL default ' ',
  "AAABHSNR" varchar(1) NOT NULL default ' ',
  "AAABHSNS" varchar(32) NOT NULL default ' ',
  "AAABHSNT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSNV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUKQ" varchar(10) NOT NULL default ' ',
  "AAABHSNY" varchar(4) NOT NULL default ' ',
  "AAABHSNZ" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABLUKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSOC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUKJ" varchar(10) NOT NULL default ' ',
  "AAABHSOF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUKJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSOI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUKQ" varchar(10) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSOQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSPC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSPC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSOV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSPC" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSPC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSPP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSPP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSPI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSPP" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSPP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSPQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABLOAT" varchar(4) NOT NULL default ' ',
  "AAABHSSE" varchar(4) NOT NULL default ' ',
  "AAABHSPW" varchar(3) NOT NULL default ' ',
  "AAABHSPX" varchar(3) NOT NULL default ' ',
  "AAABHSPY" varchar(3) NOT NULL default ' ',
  "AAABHSPZ" varchar(3) NOT NULL default ' ',
  "AAABHSQA" varchar(3) NOT NULL default ' ',
  "AAABHSQB" varchar(3) NOT NULL default ' ',
  "AAABHSQC" varchar(3) NOT NULL default ' ',
  "AAABHSQD" varchar(3) NOT NULL default ' ',
  "AAABHSQE" varchar(1) NOT NULL default ' ',
  "AAABHSQF" varchar(3) NOT NULL default ' ',
  "AAABHSQG" varchar(3) NOT NULL default ' ',
  "AAABHSQH" varchar(1) NOT NULL default ' ',
  "AAABHSQI" varchar(1) NOT NULL default ' ',
  "AAABHSSF" varchar(1) NOT NULL default ' ',
  "AAABHSSG" varchar(1) NOT NULL default ' ',
  "AAABHSQL" varchar(1) NOT NULL default ' ',
  "AAABHSSZ" varchar(2) NOT NULL default ' ',
  "AAABHSSH" varchar(1) NOT NULL default ' ',
  "AAABHSQO" varchar(1) NOT NULL default ' ',
  "AAABHSQP" varchar(2) NOT NULL default '00',
  "AAABHSQQ" varchar(2) NOT NULL default '00',
  "AAABHSQR" varchar(1) NOT NULL default ' ',
  "AAABHSSI" varchar(1) NOT NULL default ' ',
  "AAABHSSJ" varchar(1) NOT NULL default ' ',
  "AAABHSSK" varchar(1) NOT NULL default ' ',
  "AAABHSQV" varchar(3) NOT NULL default ' ',
  "AAABHSQW" varchar(1) NOT NULL default ' ',
  "AAABHSQX" varchar(1) NOT NULL default ' ',
  "AAABHSQY" varchar(1) NOT NULL default ' ',
  "AAABHSVO" varchar(4) NOT NULL default '0000',
  "AAABHSRA" varchar(4) NOT NULL default '0000',
  "AAABHSRB" varchar(4) NOT NULL default '0000',
  "AAABHSRC" varchar(4) NOT NULL default '0000',
  "AAABHSRD" varchar(4) NOT NULL default '0000',
  "AAABHSVP" varchar(4) NOT NULL default '0000',
  "AAABHSRF" varchar(4) NOT NULL default '0000',
  "AAABHSRG" varchar(4) NOT NULL default ' ',
  "AAABHSRH" varchar(1) NOT NULL default ' ',
  "AAABHSRI" varchar(3) NOT NULL default ' ',
  "AAABHSRJ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSRN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABLOAT" varchar(4) NOT NULL default ' ',
  "AAABHSRS" varchar(1) NOT NULL default ' ',
  "AAABHSRT" varchar(1) NOT NULL default ' ',
  "AAABHSRU" varchar(1) NOT NULL default ' ',
  "AAABHSRV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTOQ","AAABLOAT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSSA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABHSSE" varchar(4) NOT NULL default ' ',
  "AAABHSSF" varchar(1) NOT NULL default ' ',
  "AAABHSSG" varchar(1) NOT NULL default ' ',
  "AAABHSSH" varchar(1) NOT NULL default ' ',
  "AAABHSSI" varchar(1) NOT NULL default ' ',
  "AAABHSSJ" varchar(1) NOT NULL default ' ',
  "AAABHSSK" varchar(1) NOT NULL default ' ',
  "AAABHSSL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSSP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABHTPH" varchar(4) NOT NULL default ' ',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABLXSU" varchar(1) NOT NULL default ' ',
  "AAABHSZC" varchar(1) NOT NULL default ' ',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABHSSZ" varchar(2) NOT NULL default ' ',
  "AAABHSTA" varchar(1) NOT NULL default ' ',
  "AAABHSTB" varchar(1) NOT NULL default ' ',
  "AAABHSTC" varchar(1) NOT NULL default ' ',
  "AAABHSTD" varchar(4) NOT NULL default ' ',
  "AAABHSTE" varchar(4) NOT NULL default ' ',
  "AAABHSTF" varchar(1) NOT NULL default ' ',
  "AAABHSTG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTOQ","AAABHTPH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSTL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABHTHI" varchar(4) NOT NULL default ' ',
  "AAABHSTQ" varchar(1) NOT NULL default ' ',
  "AAABHSTR" varchar(4) NOT NULL default '0000',
  "AAABHSTS" varchar(4) NOT NULL default '0000',
  "AAABHSTT" varchar(4) NOT NULL default ' ',
  "AAABHSTU" varchar(3) NOT NULL default ' ',
  "AAABHSTV" varchar(1) NOT NULL default ' ',
  "AAABHSTW" varchar(3) NOT NULL default ' ',
  "AAABHSVN" varchar(4) NOT NULL default '0000',
  "AAABHSVM" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSUC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABHSUM" varchar(1) NOT NULL default ' ',
  "AAABLPNX" varchar(4) NOT NULL default ' ',
  "AAABHSZC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTOQ","AAABHSUM","AAABLPNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSUO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABHSUX" varchar(3) NOT NULL default ' ',
  "AAABHSUT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTOQ","AAABHSUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSUY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABHTHI" varchar(4) NOT NULL default ' ',
  "AAABHSVD" varchar(1) NOT NULL default ' ',
  "AAABHSVE" varchar(1) NOT NULL default ' ',
  "AAABHSVF" varchar(1) NOT NULL default ' ',
  "AAABHSVG" varchar(1) NOT NULL default ' ',
  "AAABHSVH" varchar(2) NOT NULL default '00',
  "AAABHSVI" varchar(2) NOT NULL default '00',
  "AAABHSVJ" varchar(1) NOT NULL default ' ',
  "AAABHSVK" varchar(1) NOT NULL default ' ',
  "AAABHSVL" varchar(1) NOT NULL default ' ',
  "AAABHSVM" varchar(4) NOT NULL default '0000',
  "AAABHSVN" varchar(4) NOT NULL default '0000',
  "AAABHSVO" varchar(4) NOT NULL default '0000',
  "AAABHSVP" varchar(4) NOT NULL default '0000',
  "AAABHSVQ" varchar(1) NOT NULL default ' ',
  "AAABHSVR" decimal(13,2) NOT NULL default '0.00',
  "AAABHSVS" varchar(1) NOT NULL default ' ',
  "AAABHSVT" decimal(13,2) NOT NULL default '0.00',
  "AAABHSVU" varchar(5) NOT NULL default ' ',
  "AAABHSVV" varchar(1) NOT NULL default ' ',
  "AAABHSVW" varchar(1) NOT NULL default ' ',
  "AAABHSVX" varchar(1) NOT NULL default ' ',
  "AAABHSVY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSWC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABIZLA" varchar(30) NOT NULL default ' ',
  "AAABHSZC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTOQ","AAABIZLA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSWM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABLPPR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSWU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAABLPPR" varchar(4) NOT NULL default ' ',
  "AAABLPPS" varchar(10) NOT NULL default ' ',
  "AAABHSXB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTOQ","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSXG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAABMDXM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHTPG","AAABHTOQ","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSXS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABHSYA" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTOQ","AAABHSYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSYB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABHSYF" varchar(10) NOT NULL default ' ',
  "AAABHSYG" varchar(1) NOT NULL default ' ',
  "AAABHSYH" varchar(1) NOT NULL default ' ',
  "AAABHSYI" varchar(1) NOT NULL default ' ',
  "AAABHSYJ" varchar(1) NOT NULL default ' ',
  "AAABHSYK" varchar(1) NOT NULL default ' ',
  "AAABHSYL" varchar(1) NOT NULL default ' ',
  "AAABHSYM" varchar(3) NOT NULL default ' ',
  "AAABHSYN" varchar(3) NOT NULL default ' ',
  "AAABHSYO" varchar(2) NOT NULL default ' ',
  "AAABHSYP" varchar(2) NOT NULL default ' ',
  "AAABHSYQ" varchar(1) NOT NULL default ' ',
  "AAABHSYR" varchar(1) NOT NULL default ' ',
  "AAABHSYS" varchar(1) NOT NULL default ' ',
  "AAABHSYT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSYX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABHSZC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTOQ","AAABMLHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHSZH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABHSZL" varchar(1) NOT NULL default ' ',
  "AAABHSZM" varchar(3) NOT NULL default '000',
  "AAABHSZN" varchar(1) NOT NULL default ' ',
  "AAABHSZO" varchar(1) NOT NULL default ' ',
  "AAABHSZP" varchar(1) NOT NULL default ' ',
  "AAABHSZQ" varchar(1) NOT NULL default ' ',
  "AAABHSZR" varchar(1) NOT NULL default ' ',
  "AAABHSZS" varchar(1) NOT NULL default ' ',
  "AAABHSZT" varchar(1) NOT NULL default ' ',
  "AAABHSZU" varchar(1) NOT NULL default ' ',
  "AAABHSZV" varchar(1) NOT NULL default ' ',
  "AAABHSZW" varchar(1) NOT NULL default ' ',
  "AAABHSZX" varchar(1) NOT NULL default ' ',
  "AAABHSZY" varchar(1) NOT NULL default ' ',
  "AAABHSZZ" varchar(4) NOT NULL default '0000',
  "AAABHTAA" varchar(1) NOT NULL default ' ',
  "AAABHTAB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABHTAJ" varchar(1) NOT NULL default ' ',
  "AAABHTAK" varchar(1) NOT NULL default '0',
  "AAABHTAL" varchar(1) NOT NULL default ' ',
  "AAABHTAM" varchar(1) NOT NULL default ' ',
  "AAABHTAN" varchar(1) NOT NULL default ' ',
  "AAABHTAO" varchar(1) NOT NULL default ' ',
  "AAABHTAP" varchar(1) NOT NULL default ' ',
  "AAABHTAQ" varchar(1) NOT NULL default ' ',
  "AAABHTAR" varchar(1) NOT NULL default ' ',
  "AAABLVFO" varchar(1) NOT NULL default ' ',
  "AAABHTBL" varchar(6) NOT NULL default '000000',
  "AAABHTAU" varchar(1) NOT NULL default ' ',
  "AAABHTBK" varchar(1) NOT NULL default ' ',
  "AAABHTAW" varchar(1) NOT NULL default ' ',
  "AAABHTAX" varchar(1) NOT NULL default ' ',
  "AAABHTAY" varchar(6) NOT NULL default '000000',
  "AAABHTAZ" varchar(1) NOT NULL default ' ',
  "AAABHTBA" varchar(1) NOT NULL default ' ',
  "AAABHTBB" varchar(1) NOT NULL default ' ',
  "AAABHTBC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTBG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABHTBK" varchar(1) NOT NULL default ' ',
  "AAABHTBL" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTBP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABHTBT" varchar(4) NOT NULL default '0000',
  "AAABHTBU" varchar(4) NOT NULL default '0000',
  "AAABHTBV" varchar(4) NOT NULL default '0000',
  "AAABHTBW" varchar(4) NOT NULL default ' ',
  "AAABHTBX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTCB" (
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTEX" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABHTDW" varchar(1) NOT NULL default ' ',
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  "AAABHTCI" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABHTPG","AAABHTEX","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTCN" (
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTEX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABHTPG","AAABHTEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTCS" (
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTEX" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABHTPG","AAABHTEX","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTDA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABHTEX" varchar(2) NOT NULL default ' ',
  "AAABHTEY" varchar(2) NOT NULL default '00',
  "AAABHTDG" varchar(1) NOT NULL default ' ',
  "AAABHTDH" varchar(14) NOT NULL default ' ',
  "AAABHTDI" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTOQ","AAABHTEX","AAABHTEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTDO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABHTEX" varchar(2) NOT NULL default ' ',
  "AAABHTEY" varchar(2) NOT NULL default '00',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABHTDW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTOQ","AAABHTEX","AAABHTEY","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTEE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABHTEX" varchar(2) NOT NULL default ' ',
  "AAABHTEY" varchar(2) NOT NULL default '00',
  "AAABHTEL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHTPG","AAABHTOQ","AAABHTEX","AAABHTEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTES" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABLTIX" varchar(20) NOT NULL default ' ',
  "AAABHTEX" varchar(2) NOT NULL default ' ',
  "AAABHTEY" varchar(2) NOT NULL default '00',
  "AAABHTEZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTOQ","AAABLTIX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTFE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABLTIX" varchar(20) NOT NULL default ' ',
  "AAABMDXM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHTPG","AAABHTOQ","AAABLTIX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTFQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABLPPR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTFY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAABLPPR" varchar(4) NOT NULL default ' ',
  "AAABLPPS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTOQ","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTGJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAABMDXM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHTPG","AAABHTOQ","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTGV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABMKAI" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTOQ","AAABMKAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABHTHI" varchar(4) NOT NULL default ' ',
  "AAABHTHJ" varchar(4) NOT NULL default ' ',
  "AAABHTHK" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTHO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABHTHS" varchar(1) NOT NULL default ' ',
  "AAABHTHT" varchar(1) NOT NULL default ' ',
  "AAABHTHU" varchar(1) NOT NULL default ' ',
  "AAABHTHV" varchar(1) NOT NULL default ' ',
  "AAABHTHW" varchar(35) NOT NULL default ' ',
  "AAABHTHX" varchar(14) NOT NULL default ' ',
  "AAABHTHY" varchar(2) NOT NULL default ' ',
  "AAABHTHZ" varchar(2) NOT NULL default ' ',
  "AAABHTIA" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTIE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABHTII" varchar(1) NOT NULL default ' ',
  "AAABILPS" varchar(1) NOT NULL default ' ',
  "AAABHTIK" varchar(1) NOT NULL default ' ',
  "AAABHTIL" varchar(1) NOT NULL default ' ',
  "AAABHTIM" varchar(1) NOT NULL default ' ',
  "AAABHTIN" tinyint(3) unsigned NOT NULL default '0',
  "AAABHTIO" tinyint(3) unsigned NOT NULL default '0',
  "AAABHTIP" int(11) NOT NULL default '0',
  "AAABHTIQ" int(11) NOT NULL default '0',
  "AAABHTIR" varchar(30) NOT NULL default ' ',
  "AAABHTIS" varchar(32) NOT NULL default ' ',
  "AAABHTIT" varchar(1) NOT NULL default ' ',
  "AAABHTIU" varchar(1) NOT NULL default ' ',
  "AAABHTIV" varchar(1) NOT NULL default ' ',
  "AAABHTIW" varchar(1) NOT NULL default ' ',
  "AAABHTIX" varchar(1) NOT NULL default ' ',
  "AAABHTIY" varchar(1) NOT NULL default ' ',
  "AAABHTIZ" varchar(1) NOT NULL default ' ',
  "AAABHTJA" varchar(1) NOT NULL default ' ',
  "AAABHTJB" varchar(1) NOT NULL default ' ',
  "AAABHTJC" varchar(1) NOT NULL default ' ',
  "AAABHTJD" varchar(1) NOT NULL default ' ',
  "AAABHTJE" varchar(1) NOT NULL default ' ',
  "AAABHTJF" varchar(1) NOT NULL default ' ',
  "AAABHTJG" varchar(1) NOT NULL default ' ',
  "AAABHTJH" varchar(30) NOT NULL default ' ',
  "AAABHTJI" varchar(32) NOT NULL default ' ',
  "AAABHTJJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABHTLD" varchar(4) NOT NULL default ' ',
  "AAABHTJS" varchar(1) NOT NULL default ' ',
  "AAABHTJT" varchar(1) NOT NULL default ' ',
  "AAABHTJU" varchar(1) NOT NULL default ' ',
  "AAABHTJV" varchar(1) NOT NULL default ' ',
  "AAABHTJW" varchar(1) NOT NULL default ' ',
  "AAABHTJX" varchar(10) NOT NULL default ' ',
  "AAABHTJY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTKC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABLRCB" varchar(4) NOT NULL default ' ',
  "AAABHTKH" varchar(1) NOT NULL default ' ',
  "AAABHTKI" varchar(1) NOT NULL default ' ',
  "AAABHTKJ" varchar(1) NOT NULL default ' ',
  "AAABHTKK" varchar(1) NOT NULL default ' ',
  "AAABHTKL" varchar(10) NOT NULL default ' ',
  "AAABHTKM" varchar(1) NOT NULL default ' ',
  "AAABHTKN" varchar(4) NOT NULL default ' ',
  "AAABHTKO" varchar(2) NOT NULL default ' ',
  "AAABHTKP" varchar(1) NOT NULL default ' ',
  "AAABHTKQ" decimal(4,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTKU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTLD" varchar(4) NOT NULL default ' ',
  "AAABHTLE" varchar(3) NOT NULL default ' ',
  "AAABMQNB" varchar(1) NOT NULL default ' ',
  "AAABHTLA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTLD","AAABHTLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTLF" (
  "AAABLECD" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTLI" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLECD" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTLO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTOQ","AAABMLDW","AAABMLDX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTMA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABHTME" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTPG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTMH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHTPG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTMP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTNM" varchar(10) NOT NULL default ' ',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAABHTMT" varchar(10) NOT NULL default ' ',
  "AAABHTMU" varchar(3) NOT NULL default ' ',
  "AAABHTMV" varchar(32) NOT NULL default ' ',
  "AAABHTMW" varchar(1) NOT NULL default ' ',
  "AAABHTMX" varchar(4) NOT NULL default ' ',
  "AAABHTMY" varchar(4) NOT NULL default ' ',
  "AAABHTMZ" varchar(4) NOT NULL default ' ',
  "AAABHTNA" varchar(32) NOT NULL default ' ',
  "AAABLXID" varchar(4) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABHTND" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTNG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTNM" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTNM","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTNO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABHTNS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTNW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABMCVI"),
  KEY "AAABHTOE" ("AAABMRIL","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTOH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTOQ" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHTPG","AAABHTOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTOR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTPH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTPG","AAABHTPH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTOY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHTPG" varchar(4) NOT NULL default ' ',
  "AAABHTPH" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHTPG","AAABHTPH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTPI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPK" varchar(2) NOT NULL default ' ',
  "AAABHTPL" varchar(3) NOT NULL default ' ',
  "AAABHTPM" varchar(3) NOT NULL default ' ',
  "AAABHTPN" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTPP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXVI" varchar(18) NOT NULL default ' ',
  "AAABLXVH" varchar(18) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLSKC" varchar(10) NOT NULL default ' ',
  "AAABHUEA" varchar(30) NOT NULL default ' ',
  "AAABHUEB" varchar(30) NOT NULL default ' ',
  "AAABHUEC" varchar(30) NOT NULL default ' ',
  "AAABHXCZ" varchar(2) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMBZP" varchar(3) NOT NULL default ' ',
  "AAABLSKK" varchar(5) NOT NULL default ' ',
  "AAABHTQD" varchar(8) NOT NULL default '00000000',
  "AAABHTQE" decimal(19,2) NOT NULL default '0.00',
  "AAABHTQF" decimal(19,2) NOT NULL default '0.00',
  "AAABHTQG" decimal(19,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLXVI","AAABLXVH","AAABLXOB","AAABMLCZ","AAABLSKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTQN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXVI" varchar(18) NOT NULL default ' ',
  "AAABLXVH" varchar(18) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABHUEA" varchar(30) NOT NULL default ' ',
  "AAABHUEB" varchar(30) NOT NULL default ' ',
  "AAABHUEC" varchar(30) NOT NULL default ' ',
  "AAABMBZP" varchar(3) NOT NULL default ' ',
  "AAABLSKK" varchar(5) NOT NULL default ' ',
  "AAABHTQY" decimal(19,2) NOT NULL default '0.00',
  "AAABHTQZ" decimal(19,2) NOT NULL default '0.00',
  "AAABHTRA" decimal(19,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLXVI","AAABLXVH","AAABLXOB","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTRG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTRL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSKD" varchar(2) NOT NULL default '00',
  "AAABLSKC" varchar(10) NOT NULL default ' ',
  "AAABLXVI" varchar(18) NOT NULL default ' ',
  "AAABLXVH" varchar(18) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABHTXT" decimal(8,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLSKD","AAABLSKC","AAABLXVI","AAABLXVH","AAABLXOB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTRZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTSB" varchar(5) NOT NULL default ' ',
  "AAABHTSC" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTSE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHUSA" varchar(20) NOT NULL default ' ',
  "AAABHTSH" varchar(1) NOT NULL default ' ',
  "AAABHTSI" varchar(30) NOT NULL default ' ',
  "AAABHTSJ" varchar(30) NOT NULL default ' ',
  "AAABHTSK" varchar(30) NOT NULL default ' ',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHUSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTSP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHUSA" varchar(20) NOT NULL default ' ',
  "AAABHTST" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHUSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTSX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABHUJN" varchar(1) NOT NULL default ' ',
  "AAABHUSM" varchar(2) NOT NULL default ' ',
  "AAABHTTC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIY","AAABHUJN","AAABHUSM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTTH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABMNYU" varchar(12) NOT NULL default ' ',
  "AAABMRFO" varchar(20) NOT NULL default ' ',
  "AAABMECK" varchar(20) NOT NULL default ' ',
  "AAABHTWT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIY","AAABLLJV","AAABMNYU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTTT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABMNYU" varchar(12) NOT NULL default ' ',
  "AAABLSMW" varchar(3) NOT NULL default '000',
  "AAABHTTZ" varchar(1) NOT NULL default ' ',
  "AAABHTUA" varchar(1) NOT NULL default ' ',
  "AAABICFV" varchar(40) NOT NULL default ' ',
  "AAABHTUC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIY","AAABLLJV","AAABMNYU","AAABLSMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTUI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABMNYU" varchar(12) NOT NULL default ' ',
  "AAABLSMW" varchar(3) NOT NULL default '000',
  "AAABHTUP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLLIY","AAABLLJV","AAABMNYU","AAABLSMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTUW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABMNYU" varchar(12) NOT NULL default ' ',
  "AAABLSMW" varchar(3) NOT NULL default '000',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABHTVO" varchar(1) NOT NULL default ' ',
  "AAABHTVP" varchar(10) NOT NULL default ' ',
  "AAABMNMZ" varchar(32) NOT NULL default ' ',
  "AAABMNNA" varchar(32) NOT NULL default ' ',
  "AAABHTVH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIY","AAABLLJV","AAABMNYU","AAABLSMW","AAABMKXW","AAABHTVO","AAABHTVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTVQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMALF" varchar(1) NOT NULL default ' ',
  "AAABLEEW" varchar(30) NOT NULL default ' ',
  "AAABHTWA" varchar(30) NOT NULL default ' ',
  "AAABHTWG" varchar(30) NOT NULL default ' ',
  "AAABHTVW" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMALF","AAABLEEW","AAABHTWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTWB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMALF" varchar(1) NOT NULL default ' ',
  "AAABHTWJ" varchar(30) NOT NULL default ' ',
  "AAABHTWK" varchar(30) NOT NULL default ' ',
  "AAABHTWG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMALF","AAABHTWJ","AAABHTWK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTWL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABMNYU" varchar(12) NOT NULL default ' ',
  "AAABHTWY" varchar(20) NOT NULL default ' ',
  "AAABHTWR" varchar(1) NOT NULL default ' ',
  "AAABHTWS" varchar(1) NOT NULL default ' ',
  "AAABHTWT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIY","AAABLLJV","AAABMNYU","AAABHTWY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTWZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSKD" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABLSKD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTXE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLSKD" varchar(2) NOT NULL default '00',
  "AAABLSJY" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLSKD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTXM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLXVI" varchar(18) NOT NULL default ' ',
  "AAABLXVH" varchar(18) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABLSKD" varchar(2) NOT NULL default '00',
  "AAABHTXT" decimal(8,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABLXVI","AAABLXVH","AAABLXOB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTXZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTYB" varchar(1) NOT NULL default ' ',
  "AAABHTYC" varchar(1) NOT NULL default ' ',
  "AAABHUMB" varchar(1) NOT NULL default ' ',
  "AAABHTYE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTYG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABKRIB" varchar(20) NOT NULL default ' ',
  "AAABLSHU" varchar(4) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAABLSJR" varchar(4) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLMDV" varchar(4) NOT NULL default ' ',
  "AAABLSFX" varchar(10) NOT NULL default ' ',
  "AAABHTYS" varchar(1) NOT NULL default ' ',
  "AAABHTZV" varchar(1) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABHTZY" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIY","AAABLLJV","AAABKRIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABKRIB" varchar(20) NOT NULL default ' ',
  "AAABHTZF" varchar(1) NOT NULL default ' ',
  "AAABHTZG" varchar(1) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABHTZY" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIY","AAABLLJV","AAABKRIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHTZN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABKRIB" varchar(20) NOT NULL default ' ',
  "AAABLGJS" varchar(4) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABHUZG" varchar(3) NOT NULL default ' ',
  "AAABHTZV" varchar(1) NOT NULL default ' ',
  "AAABHTZW" varchar(1) NOT NULL default ' ',
  "AAABHTZX" varchar(1) NOT NULL default ' ',
  "AAABHTZY" varchar(32) NOT NULL default ' ',
  "AAABHTZZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIY","AAABLLJV","AAABKRIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUAE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABHUVY" varchar(2) NOT NULL default ' ',
  "AAABHUCE" varchar(30) NOT NULL default ' ',
  "AAABHUCF" varchar(1) NOT NULL default ' ',
  "AAABHUAL" varchar(2) NOT NULL default ' ',
  "AAABHUAM" varchar(30) NOT NULL default ' ',
  "AAABHUAN" varchar(30) NOT NULL default ' ',
  "AAABHUAO" varchar(30) NOT NULL default ' ',
  "AAABHUAP" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABLLIY","AAABLLJV","AAABHUVY","AAABHUCE","AAABHUCF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUAW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABHUVY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIY","AAABLLJV","AAABHUVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUBF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABHUVY" varchar(2) NOT NULL default ' ',
  "AAABHUBL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLLIY","AAABLLJV","AAABHUVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUBR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABHUVY" varchar(2) NOT NULL default ' ',
  "AAABHUCE" varchar(30) NOT NULL default ' ',
  "AAABHUCF" varchar(1) NOT NULL default ' ',
  "AAABHUCG" varchar(2) NOT NULL default '00',
  "AAABIHPX" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIY","AAABLLJV","AAABHUVY","AAABHUCE","AAABHUCF","AAABHUCG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUCH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABHUUH" varchar(32) NOT NULL default ' ',
  "AAABHUCM" varchar(1) NOT NULL default ' ',
  "AAABHUCN" varchar(1) NOT NULL default ' ',
  "AAABHUCO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIY","AAABLLJV","AAABHUUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUCT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABHUWN" varchar(30) NOT NULL default ' ',
  "AAABHUCY" varchar(7) NOT NULL default '0000000',
  "AAABHUCZ" varchar(7) NOT NULL default '0000000',
  "AAABHUDA" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABLLIY","AAABLLJV","AAABHUWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABHUDN" varchar(1) NOT NULL default ' ',
  "AAABHUVS" varchar(30) NOT NULL default ' ',
  "AAABKMJV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIY","AAABHUDN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUDO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABHUDR" varchar(18) NOT NULL default ' ',
  "AAABHUDS" varchar(1) NOT NULL default ' ',
  "AAABHUDT" varchar(1) NOT NULL default ' ',
  "AAABHUDU" varchar(1) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABLSJL" varchar(2) NOT NULL default ' ',
  "AAABHXCZ" varchar(2) NOT NULL default ' ',
  "AAABLSKS" varchar(4) NOT NULL default ' ',
  "AAABLSKR" varchar(4) NOT NULL default ' ',
  "AAABHUEA" varchar(30) NOT NULL default ' ',
  "AAABHUEB" varchar(30) NOT NULL default ' ',
  "AAABHUEC" varchar(30) NOT NULL default ' ',
  "AAABLXSK" varchar(18) NOT NULL default ' ',
  "AAABIVCC" varchar(2) NOT NULL default ' ',
  "AAABHUEF" varchar(18) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABLRZL" varchar(18) NOT NULL default ' ',
  "AAABHUEI" varchar(18) NOT NULL default ' ',
  "AAABKRYY" varchar(10) NOT NULL default ' ',
  "AAABLPZD" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMLCT" varchar(4) NOT NULL default ' ',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABLIPB" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUER" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUEZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABHUFX" varchar(10) NOT NULL default ' ',
  "AAABHUFE" varchar(1) NOT NULL default ' ',
  "AAABHUFF" varchar(40) NOT NULL default ' ',
  "AAABHUFG" varchar(14) NOT NULL default ' ',
  "AAABHUMO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIY","AAABLLJV","AAABHUFX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUFM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABHUFX" varchar(10) NOT NULL default ' ',
  "AAABHUFS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLLIY","AAABLLJV","AAABHUFX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUFY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABHUVS" varchar(30) NOT NULL default ' ',
  "AAABHUGS" varchar(30) NOT NULL default ' ',
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  "AAABJELV" varchar(40) NOT NULL default ' ',
  "AAABHUGH" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABHUGJ" varchar(40) NOT NULL default ' ',
  "AAABHUGK" varchar(30) NOT NULL default ' ',
  "AAABHUGL" varchar(30) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABLDEV" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIY","AAABLLJV","AAABHUVS","AAABHUGS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUGT" (
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUGX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABHUHJ" varchar(40) NOT NULL default ' ',
  "AAABHUHC" varchar(1) NOT NULL default ' ',
  "AAABHUHD" varchar(1) NOT NULL default ' ',
  "AAABHUHE" varchar(14) NOT NULL default ' ',
  "AAABHUMO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIY","AAABLLJV","AAABHUHJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUHK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABHUHN" varchar(16) NOT NULL default ' ',
  "AAABHUHO" varchar(30) NOT NULL default ' ',
  "AAABHUHP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUHS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHUWS" varchar(3) NOT NULL default ' ',
  "AAABHUHV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHUWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUHY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHUWS" varchar(3) NOT NULL default ' ',
  "AAABHUIC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHUWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUIG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABKRIB" varchar(20) NOT NULL default ' ',
  "AAABHUSY" varchar(1) NOT NULL default ' ',
  "AAABMABE" varchar(3) NOT NULL default ' ',
  "AAABHUNU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIY","AAABLLJV","AAABKRIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUIS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABKRIB" varchar(20) NOT NULL default ' ',
  "AAABHUIY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLLIY","AAABLLJV","AAABKRIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUJE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABHUJI" varchar(1) NOT NULL default ' ',
  "AAABHUJJ" varchar(1) NOT NULL default ' ',
  "AAABHUJK" varchar(1) NOT NULL default ' ',
  "AAABHUJL" varchar(1) NOT NULL default ' ',
  "AAABLDEV" varchar(16) NOT NULL default ' ',
  "AAABHUJN" varchar(1) NOT NULL default ' ',
  "AAABHUSM" varchar(2) NOT NULL default ' ',
  "AAABHUJP" varchar(12) NOT NULL default ' ',
  "AAABHUJQ" varchar(8) NOT NULL default '00000000',
  "AAABHUJR" varchar(6) NOT NULL default '000000',
  "AAABHUJS" varchar(12) NOT NULL default ' ',
  "AAABHUJT" varchar(8) NOT NULL default '00000000',
  "AAABHUJU" varchar(6) NOT NULL default '000000',
  "AAABHUJV" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABLLIY","AAABLLJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUJZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABHUKE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLLIY","AAABLLJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUKJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABHUKN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIY","AAABLLJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUKR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABHUKV" varchar(3) NOT NULL default ' ',
  "AAABLXES" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIY","AAABLLJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHULA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLSKK" varchar(5) NOT NULL default ' ',
  "AAABHULE" varchar(12) NOT NULL default ' ',
  "AAABHULF" varchar(8) NOT NULL default '00000000',
  "AAABHULG" varchar(6) NOT NULL default '000000',
  "AAABHULH" varchar(12) NOT NULL default ' ',
  "AAABHULI" varchar(8) NOT NULL default '00000000',
  "AAABHULJ" varchar(6) NOT NULL default '000000',
  "AAABHULK" varchar(1) NOT NULL default ' ',
  "AAABMRCU" varchar(2) NOT NULL default '00',
  "AAABHULM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHULP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABHULT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLLIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHULX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABHUMB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUME" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABHUNL" varchar(10) NOT NULL default ' ',
  "AAABHUNM" varchar(10) NOT NULL default ' ',
  "AAABHUNN" varchar(3) NOT NULL default '000',
  "AAABHUMK" varchar(1) NOT NULL default ' ',
  "AAABHUML" varchar(40) NOT NULL default ' ',
  "AAABHUMM" varchar(14) NOT NULL default ' ',
  "AAABHUMN" varchar(20) NOT NULL default ' ',
  "AAABHUMO" varchar(1) NOT NULL default ' ',
  "AAABHUMP" varchar(12) NOT NULL default ' ',
  "AAABHUMQ" varchar(8) NOT NULL default '00000000',
  "AAABHUMR" varchar(6) NOT NULL default '000000',
  "AAABHUMS" varchar(12) NOT NULL default ' ',
  "AAABHUMT" varchar(8) NOT NULL default '00000000',
  "AAABHUMU" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLLIY","AAABHUNL","AAABHUNM","AAABHUNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUNA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABHUNL" varchar(10) NOT NULL default ' ',
  "AAABHUNM" varchar(10) NOT NULL default ' ',
  "AAABHUNN" varchar(3) NOT NULL default '000',
  "AAABLZKW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLLIY","AAABHUNL","AAABHUNM","AAABHUNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUNO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHUPJ" varchar(2) NOT NULL default ' ',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABMABE" varchar(3) NOT NULL default ' ',
  "AAABHUNU" varchar(1) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABHUNW" varchar(20) NOT NULL default ' ',
  "AAABHUNX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHUPJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUOA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHUPJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHUPJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUOF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHUPJ" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHUPJ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUON" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHUPJ" varchar(2) NOT NULL default ' ',
  "AAABKNQJ" varchar(30) NOT NULL default ' ',
  "AAABHUUB" varchar(30) NOT NULL default ' ',
  "AAABLDNU" varchar(4) NOT NULL default ' ',
  "AAABLBEI" varchar(6) NOT NULL default '000000',
  "AAABMNMM" varchar(6) NOT NULL default '000000',
  "AAABLUDG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHUPJ","AAABKNQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUOZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHUPJ" varchar(2) NOT NULL default ' ',
  "AAABHUVS" varchar(30) NOT NULL default ' ',
  "AAABHUUO" varchar(30) NOT NULL default ' ',
  "AAABLDNU" varchar(4) NOT NULL default ' ',
  "AAABLBEI" varchar(6) NOT NULL default '000000',
  "AAABMNMM" varchar(6) NOT NULL default '000000',
  "AAABLUDG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHUPJ","AAABHUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUPL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSKS" varchar(4) NOT NULL default ' ',
  "AAABLSKR" varchar(4) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABIBEE" varchar(3) NOT NULL default '000',
  "AAABIBEF" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABLSKS","AAABLSKR","AAABMLXQ","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMNYU" varchar(12) NOT NULL default ' ',
  "AAABHUQC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAABMNYU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUQG" (
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABHUQI" varchar(1) NOT NULL default ' ',
  "AAABHUQJ" varchar(1) NOT NULL default ' ',
  "AAABHUQK" varchar(1) NOT NULL default ' ',
  "AAABHUQL" varchar(1) NOT NULL default ' ',
  "AAABHUQM" varchar(1) NOT NULL default ' ',
  "AAABHUQN" varchar(4) NOT NULL default '0000',
  "AAABHUQO" varchar(1) NOT NULL default ' ',
  "AAABHUQP" varchar(1) NOT NULL default ' ',
  "AAABHUQQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLXIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUQS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXES" varchar(8) NOT NULL default ' ',
  "AAABMABE" varchar(3) NOT NULL default ' ',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABHUQZ" varchar(8) NOT NULL default '00000000',
  "AAABHUWS" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXES","AAABMABE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHURE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXES" varchar(8) NOT NULL default ' ',
  "AAABMABE" varchar(3) NOT NULL default ' ',
  "AAABHURJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXES","AAABMABE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHURO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABKRIB" varchar(20) NOT NULL default ' ',
  "AAABHUSA" varchar(20) NOT NULL default ' ',
  "AAABHUUO" varchar(30) NOT NULL default ' ',
  "AAABHUUP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIY","AAABLLJV","AAABKRIB","AAABHUSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUSB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABHUSL" varchar(30) NOT NULL default ' ',
  "AAABHUSM" varchar(2) NOT NULL default ' ',
  "AAABHUSG" varchar(30) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABHUSI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIY","AAABHUSL","AAABHUSM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUSN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABMNYU" varchar(12) NOT NULL default ' ',
  "AAABHUSY" varchar(1) NOT NULL default ' ',
  "AAABKRIB" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIY","AAABLLJV","AAABMNYU","AAABHUSY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUSZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABKNQJ" varchar(30) NOT NULL default ' ',
  "AAABHUTE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIY","AAABLLJV","AAABKNQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUTJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABKNQJ" varchar(30) NOT NULL default ' ',
  "AAABHUVM" varchar(30) NOT NULL default ' ',
  "AAABHUVN" varchar(4) NOT NULL default ' ',
  "AAABHUVO" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABLLIY","AAABLLJV","AAABKNQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABKRIB" varchar(20) NOT NULL default ' ',
  "AAABHUUH" varchar(32) NOT NULL default ' ',
  "AAABHUUB" varchar(30) NOT NULL default ' ',
  "AAABHUUC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIY","AAABLLJV","AAABKRIB","AAABHUUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUUI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABKRIB" varchar(20) NOT NULL default ' ',
  "AAABHUVS" varchar(30) NOT NULL default ' ',
  "AAABHUUO" varchar(30) NOT NULL default ' ',
  "AAABHUUP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIY","AAABLLJV","AAABKRIB","AAABHUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUUV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABHUVS" varchar(30) NOT NULL default ' ',
  "AAABHUVA" varchar(2) NOT NULL default '00',
  "AAABHUVB" varchar(1) NOT NULL default ' ',
  "AAABHUVC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIY","AAABLLJV","AAABHUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUVH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABHUVS" varchar(30) NOT NULL default ' ',
  "AAABHUVM" varchar(30) NOT NULL default ' ',
  "AAABHUVN" varchar(4) NOT NULL default ' ',
  "AAABHUVO" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABLLIY","AAABLLJV","AAABHUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUVT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABMNYU" varchar(12) NOT NULL default ' ',
  "AAABHUVY" varchar(2) NOT NULL default ' ',
  "AAABHUVZ" varchar(1) NOT NULL default ' ',
  "AAABHUWA" varchar(1) NOT NULL default ' ',
  "AAABHUWB" varchar(1) NOT NULL default ' ',
  "AAABHUWC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIY","AAABLLJV","AAABMNYU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUWH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHUWN" varchar(30) NOT NULL default ' ',
  "AAABHUWK" varchar(30) NOT NULL default ' ',
  "AAABHUWL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHUWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUWO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHUWS" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHUWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUWT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXES" varchar(8) NOT NULL default ' ',
  "AAABHUWW" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXES")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUWZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNYU" varchar(12) NOT NULL default ' ',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABIIVF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNYU","AAABLXIY","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUXJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSHT" varchar(10) NOT NULL default ' ',
  "AAABLMEE" varchar(40) NOT NULL default ' ',
  "AAABLSHU" varchar(4) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABKRIB" varchar(20) NOT NULL default ' ',
  "AAABHUZH" varchar(12) NOT NULL default ' ',
  "AAABLSCW" varchar(8) NOT NULL default '00000000',
  "AAABHUZJ" varchar(6) NOT NULL default '000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSHT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUXZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSHT" varchar(10) NOT NULL default ' ',
  "AAABLMEE" varchar(40) NOT NULL default ' ',
  "AAABLSHU" varchar(4) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABKRIB" varchar(20) NOT NULL default ' ',
  "AAABHUZH" varchar(12) NOT NULL default ' ',
  "AAABLSCW" varchar(8) NOT NULL default '00000000',
  "AAABHUZJ" varchar(6) NOT NULL default '000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSHT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUYP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWZD" varchar(10) NOT NULL default ' ',
  "AAABHUYS" varchar(40) NOT NULL default ' ',
  "AAABHUYT" varchar(4) NOT NULL default ' ',
  "AAABLGBU" varchar(4) NOT NULL default ' ',
  "AAABHUYV" varchar(8) NOT NULL default '00000000',
  "AAABHUYW" varchar(8) NOT NULL default '00000000',
  "AAABHUYX" varchar(5) NOT NULL default ' ',
  "AAABIYXW" varchar(4) NOT NULL default ' ',
  "AAABHUYZ" varchar(2) NOT NULL default ' ',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABKRIB" varchar(20) NOT NULL default ' ',
  "AAABMNYU" varchar(12) NOT NULL default ' ',
  "AAABLSMW" varchar(3) NOT NULL default '000',
  "AAABLSKD" varchar(18) NOT NULL default ' ',
  "AAABHUZG" varchar(3) NOT NULL default ' ',
  "AAABHUZH" varchar(12) NOT NULL default ' ',
  "AAABLSCW" varchar(8) NOT NULL default '00000000',
  "AAABHUZJ" varchar(6) NOT NULL default '000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLWZD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUZN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHUZV" varchar(32) NOT NULL default ' ',
  "AAABMDBB" varchar(32) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHUZV","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHUZX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABLQAJ" varchar(2) NOT NULL default ' ',
  "AAABMRFB" varchar(5) NOT NULL default ' ',
  "AAABHWNP" varchar(4) NOT NULL default ' ',
  "AAABHVAE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABLQAJ","AAABMRFB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVAK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHWNP" varchar(4) NOT NULL default ' ',
  "AAABLOMP" varchar(6) NOT NULL default ' ',
  "AAABHVAO" varchar(1) NOT NULL default ' ',
  "AAABHVAP" varchar(1) NOT NULL default ' ',
  "AAABLOLU" varchar(4) NOT NULL default ' ',
  "AAABLOON" varchar(1) NOT NULL default ' ',
  "AAABLONK" varchar(5) NOT NULL default ' ',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABHVAU" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHWNP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVAX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHWNP" varchar(4) NOT NULL default ' ',
  "AAABHVBB" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHWNP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVBF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABMCEO","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVBP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVGZ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVGZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVBU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVGZ" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVGZ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVCC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOOW" varchar(5) NOT NULL default ' ',
  "AAABHVCF" decimal(5,0) NOT NULL default '0',
  "AAABHVCG" varchar(1) NOT NULL default ' ',
  "AAABLJDA" varchar(1) NOT NULL default ' ',
  "AAABHVCI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOOW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVCL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOMP" varchar(6) NOT NULL default ' ',
  "AAABLOOW" varchar(5) NOT NULL default ' ',
  "AAABHVCP" varchar(5) NOT NULL default ' ',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABHVCT" varchar(4) NOT NULL default ' ',
  "AAABHWLW" varchar(4) NOT NULL default ' ',
  "AAABHVCV" varchar(1) NOT NULL default ' ',
  "AAABHVCW" varchar(1) NOT NULL default ' ',
  "AAABHVCX" varchar(1) NOT NULL default ' ',
  "AAABHVCY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOMP","AAABLOOW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVDC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLOOW" varchar(5) NOT NULL default ' ',
  "AAABIUHL" varchar(10) NOT NULL default ' ',
  "AAABLOHX" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLOOW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVDL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABHXVM" varchar(2) NOT NULL default ' ',
  "AAABHVDW" varchar(2) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABLIQN" decimal(11,2) NOT NULL default '0.00',
  "AAABHVDS" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABHXVM","AAABHVDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVDX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRYX" varchar(2) NOT NULL default ' ',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAABHVEB" varchar(1) NOT NULL default ' ',
  "AAABHVEC" varchar(1) NOT NULL default ' ',
  "AAABHVED" varchar(1) NOT NULL default ' ',
  "AAABHVEE" varchar(1) NOT NULL default ' ',
  "AAABHVEF" varchar(1) NOT NULL default ' ',
  "AAABHVEG" varchar(1) NOT NULL default ' ',
  "AAABHVEH" varchar(1) NOT NULL default ' ',
  "AAABHVEI" varchar(1) NOT NULL default ' ',
  "AAABHVEJ" varchar(1) NOT NULL default ' ',
  "AAABHVEK" varchar(30) NOT NULL default ' ',
  "AAABHVEL" varchar(1) NOT NULL default ' ',
  "AAABHVEM" varchar(1) NOT NULL default ' ',
  "AAABHVEN" varchar(1) NOT NULL default ' ',
  "AAABHVEO" varchar(1) NOT NULL default ' ',
  "AAABHVEP" varchar(1) NOT NULL default ' ',
  "AAABHVEQ" varchar(1) NOT NULL default ' ',
  "AAABHVER" varchar(1) NOT NULL default ' ',
  "AAABHVES" varchar(30) NOT NULL default ' ',
  "AAABHVET" varchar(2) NOT NULL default ' ',
  "AAABHVEU" varchar(1) NOT NULL default ' ',
  "AAABHVEV" varchar(1) NOT NULL default ' ',
  "AAABHVEW" varchar(30) NOT NULL default ' ',
  "AAABHVEX" varchar(30) NOT NULL default ' ',
  "AAABHVEY" varchar(30) NOT NULL default ' ',
  "AAABHVEZ" varchar(1) NOT NULL default ' ',
  "AAABHVFA" varchar(1) NOT NULL default ' ',
  "AAABHVFB" varchar(1) NOT NULL default ' ',
  "AAABHVFC" varchar(1) NOT NULL default ' ',
  "AAABHVFD" varchar(1) NOT NULL default ' ',
  "AAABHVFE" varchar(1) NOT NULL default ' ',
  "AAABHVFF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVFI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRYX" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRYX","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVFQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABHVFZ" varchar(4) NOT NULL default ' ',
  "AAABHVFV" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABMCEO","AAABHVFZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVGA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVGZ" varchar(2) NOT NULL default ' ',
  "AAABHVHA" varchar(2) NOT NULL default ' ',
  "AAABHVGE" varchar(2) NOT NULL default ' ',
  "AAABHVGF" varchar(2) NOT NULL default ' ',
  "AAABHVGG" varchar(2) NOT NULL default ' ',
  "AAABHVGH" varchar(2) NOT NULL default ' ',
  "AAABHVGI" varchar(2) NOT NULL default ' ',
  "AAABHVGJ" varchar(2) NOT NULL default ' ',
  "AAABHVGK" varchar(2) NOT NULL default ' ',
  "AAABHVGL" varchar(2) NOT NULL default ' ',
  "AAABHVGM" varchar(2) NOT NULL default ' ',
  "AAABHVGN" varchar(2) NOT NULL default ' ',
  "AAABHVGO" varchar(2) NOT NULL default ' ',
  "AAABHVGP" varchar(2) NOT NULL default ' ',
  "AAABHVGQ" varchar(2) NOT NULL default ' ',
  "AAABHVGR" varchar(2) NOT NULL default ' ',
  "AAABHVGS" varchar(2) NOT NULL default ' ',
  "AAABHVGT" varchar(2) NOT NULL default ' ',
  "AAABHVGU" varchar(2) NOT NULL default ' ',
  "AAABHVGV" varchar(2) NOT NULL default ' ',
  "AAABHVGW" varchar(2) NOT NULL default ' ',
  "AAABHVGX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVGZ","AAABHVHA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVHB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLNSP" varchar(4) NOT NULL default ' ',
  "AAABHYJM" varchar(2) NOT NULL default ' ',
  "AAABHYJN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLNSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVHI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLNSP" varchar(4) NOT NULL default ' ',
  "AAABHVHM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLNSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVHQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLURK" varchar(4) NOT NULL default ' ',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABHVHU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLURK","AAABMCVK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVHY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRWN" varchar(10) NOT NULL default ' ',
  "AAABHVIB" varchar(10) NOT NULL default ' ',
  "AAABHVIC" varchar(8) NOT NULL default '00000000',
  "AAABHVID" varchar(8) NOT NULL default '00000000',
  "AAABHVIE" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLRWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVIH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRWN" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRWN","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVIP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPHY" varchar(10) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABLPHC" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPHY","AAABLXOB","AAABMPLT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVIZ" (
  "AAABKNQN" varchar(4) NOT NULL default ' ',
  "AAABHVJK" varchar(2) NOT NULL default ' ',
  "AAABHVJC" varchar(1) NOT NULL default ' ',
  "AAABHVJD" varchar(40) NOT NULL default ' ',
  "AAABHVJE" varchar(4) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABHVJG" varchar(20) NOT NULL default ' ',
  "AAABHVJH" varchar(20) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABKNQN","AAABHVJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVJL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKMJV" varchar(30) NOT NULL default ' ',
  "AAABHVJO" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABKMJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVJR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVJT" varchar(30) NOT NULL default ' ',
  "AAABLMDV" varchar(4) NOT NULL default ' ',
  "AAABHVJV" varchar(30) NOT NULL default ' ',
  "AAABHVJW" varchar(1) NOT NULL default ' ',
  "AAABHVJX" varchar(30) NOT NULL default ' ',
  "AAABHVJY" varchar(1) NOT NULL default ' ',
  "AAABHVJZ" varchar(1) NOT NULL default ' ',
  "AAABHVKA" varchar(30) NOT NULL default ' ',
  "AAABHVKB" varchar(1) NOT NULL default ' ',
  "AAABHVKC" varchar(1) NOT NULL default ' ',
  "AAABHVKD" varchar(4) NOT NULL default '0000',
  "AAABIMOU" varchar(1) NOT NULL default ' ',
  "AAABHVKF" varchar(4) NOT NULL default ' ',
  "AAABHVKG" varchar(4) NOT NULL default ' ',
  "AAABHVKH" varchar(1) NOT NULL default ' ',
  "AAABLHRD" varchar(4) NOT NULL default ' ',
  "AAABHVKJ" varchar(4) NOT NULL default '0000',
  "AAABHVKK" varchar(1) NOT NULL default ' ',
  "AAABHVKL" varchar(3) NOT NULL default ' ',
  "AAABHVKM" varchar(30) NOT NULL default ' ',
  "AAABHVKN" varchar(4) NOT NULL default '0000',
  "AAABHVKO" varchar(4) NOT NULL default '0000',
  "AAABHVKP" varchar(1) NOT NULL default ' ',
  "AAABHVKQ" varchar(1) NOT NULL default ' ',
  "AAABHVKR" varchar(1) NOT NULL default ' ',
  "AAABHVKS" varchar(1) NOT NULL default ' ',
  "AAABLUKA" varchar(1) NOT NULL default ' ',
  "AAABHVKU" varchar(1) NOT NULL default ' ',
  "AAABHVKV" varchar(3) NOT NULL default ' ',
  "AAABHVKW" varchar(3) NOT NULL default ' ',
  "AAABHVKX" varchar(1) NOT NULL default ' ',
  "AAABHVKY" varchar(1) NOT NULL default ' ',
  "AAABHVKZ" varchar(1) NOT NULL default ' ',
  "AAABHVLA" varchar(1) NOT NULL default ' ',
  "AAABHVLB" varchar(1) NOT NULL default ' ',
  "AAABHVLC" varchar(1) NOT NULL default ' ',
  "AAABHVLD" varchar(1) NOT NULL default ' ',
  "AAABHVLE" varchar(30) NOT NULL default ' ',
  "AAABHVLF" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVLH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVLL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(10) NOT NULL default ' ',
  "AAABJCYJ" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVLR" (
  "AAABLECD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVLU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQMP" varchar(4) NOT NULL default ' ',
  "AAABHVLX" varchar(1) NOT NULL default ' ',
  "AAABHVLY" int(11) NOT NULL default '0',
  "AAABHVLZ" tinyint(3) unsigned NOT NULL default '0',
  "AAABHVMA" varchar(1) NOT NULL default ' ',
  "AAABHVMB" int(11) NOT NULL default '0',
  "AAABHVMC" tinyint(3) unsigned NOT NULL default '0',
  "AAABHVMD" tinyint(3) unsigned NOT NULL default '0',
  "AAABHVME" varchar(1) NOT NULL default ' ',
  "AAABHVMF" varchar(1) NOT NULL default ' ',
  "AAABHVMG" varchar(1) NOT NULL default ' ',
  "AAABHVMH" varchar(1) NOT NULL default ' ',
  "AAABHVMI" varchar(1) NOT NULL default ' ',
  "AAABHVMJ" varchar(1) NOT NULL default ' ',
  "AAABHVMK" varchar(1) NOT NULL default ' ',
  "AAABHVML" varchar(1) NOT NULL default ' ',
  "AAABHVMM" varchar(1) NOT NULL default ' ',
  "AAABHVMN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVMQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUKK" varchar(4) NOT NULL default ' ',
  "AAABHVNA" varchar(3) NOT NULL default ' ',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABHVMV" varchar(3) NOT NULL default ' ',
  "AAABJGGK" varchar(3) NOT NULL default ' ',
  "AAABLOZE" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUKK","AAABHVNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVNB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMHA" varchar(10) NOT NULL default ' ',
  "AAABLMHG" varchar(10) NOT NULL default ' ',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABHVNG" varchar(1) NOT NULL default ' ',
  "AAABLQTM" varchar(1) NOT NULL default ' ',
  "AAABMMJW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMHA","AAABLMHG","AAABLMHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVNN" (
  "AAABHWUM" VARBINARY(255) NOT NULL,
  "AAABHVNR" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABHWUM"(255),"AAABHVNR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVNS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUKK" varchar(4) NOT NULL default ' ',
  "AAABHVNV" varchar(1) NOT NULL default ' ',
  "AAABHVNW" varchar(1) NOT NULL default ' ',
  "AAABHVNX" varchar(1) NOT NULL default ' ',
  "AAABHVNY" varchar(2) NOT NULL default ' ',
  "AAABMIWE" varchar(1) NOT NULL default ' ',
  "AAABHVOA" varchar(1) NOT NULL default ' ',
  "AAABHVOB" varchar(1) NOT NULL default ' ',
  "AAABHVOC" varchar(1) NOT NULL default ' ',
  "AAABHVOD" varchar(1) NOT NULL default ' ',
  "AAABHVOE" varchar(1) NOT NULL default ' ',
  "AAABHVOF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUKK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVOI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUKK" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUKK","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVOQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIMHJ" varchar(10) NOT NULL default ' ',
  "AAABIMHK" varchar(10) NOT NULL default ' ',
  "AAABIMHG" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABIMHJ","AAABIMHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVPA" (
  "AAABIMHJ" varchar(10) NOT NULL default ' ',
  "AAABIMHK" varchar(10) NOT NULL default ' ',
  "AAABIMHF" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABIMHJ","AAABIMHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVPG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIMHJ" varchar(10) NOT NULL default ' ',
  "AAABIMHK" varchar(10) NOT NULL default ' ',
  "AAABIMHF" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIMHJ","AAABIMHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVPO" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIMHJ" varchar(10) NOT NULL default ' ',
  "AAABIMHK" varchar(10) NOT NULL default ' ',
  "AAABIMHG" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIMHJ","AAABIMHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVPW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHWBH" varchar(15) NOT NULL default ' ',
  "AAABHVTW" varchar(12) NOT NULL default ' ',
  "AAABHVQO" varchar(2) NOT NULL default '00',
  "AAABHVQP" varchar(1) NOT NULL default ' ',
  "AAABHVQQ" varchar(1) NOT NULL default ' ',
  "AAABJDQT" varchar(1) NOT NULL default ' ',
  "AAABHVUJ" varchar(15) NOT NULL default ' ',
  "AAABHVTO" varchar(15) NOT NULL default ' ',
  "AAABHVQU" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHWBH","AAABHVTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVQK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHWBH" varchar(15) NOT NULL default ' ',
  "AAABHVTW" varchar(12) NOT NULL default ' ',
  "AAABHVQO" varchar(2) NOT NULL default '00',
  "AAABHVQP" varchar(1) NOT NULL default ' ',
  "AAABHVQQ" varchar(1) NOT NULL default ' ',
  "AAABJDQT" varchar(1) NOT NULL default ' ',
  "AAABHVUJ" varchar(15) NOT NULL default ' ',
  "AAABHVTO" varchar(15) NOT NULL default ' ',
  "AAABHVQU" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHWBH","AAABHVTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHWBH" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHWBH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVRD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHWBH" varchar(15) NOT NULL default ' ',
  "AAABHVRH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHWBH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVRL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVTW" varchar(12) NOT NULL default ' ',
  "AAABHVRO" varchar(1) NOT NULL default ' ',
  "AAABHVRP" varchar(1) NOT NULL default '0',
  "AAABHVRQ" varchar(250) NOT NULL default ' ',
  "AAABHVRR" varchar(5) NOT NULL default ' ',
  "AAABHVRS" varchar(30) NOT NULL default ' ',
  "AAABHVRT" varchar(30) NOT NULL default ' ',
  "AAABHVRU" varchar(30) NOT NULL default ' ',
  "AAABHVRV" varchar(250) NOT NULL default ' ',
  "AAABHVRW" varchar(250) NOT NULL default ' ',
  "AAABHVRX" varchar(30) NOT NULL default ' ',
  "AAABHVRY" varchar(30) NOT NULL default ' ',
  "AAABLGEF" varchar(40) NOT NULL default ' ',
  "AAABHVSA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVSD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHWBI" varchar(15) NOT NULL default ' ',
  "AAABHVTW" varchar(12) NOT NULL default ' ',
  "AAABHVTT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHWBI","AAABHVTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVSN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHWBI" varchar(15) NOT NULL default ' ',
  "AAABHVTW" varchar(12) NOT NULL default ' ',
  "AAABHVTT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHWBI","AAABHVTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVSX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHWBI" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHWBI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVTC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVTO" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVTO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVTH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHVTO" varchar(15) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHVTO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVTP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHVTW" varchar(12) NOT NULL default ' ',
  "AAABHVTT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHVTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVTX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVUJ" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVUJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVUC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHVUJ" varchar(15) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHVUJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVUK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHWBH" varchar(15) NOT NULL default ' ',
  "AAABHVUU" varchar(15) NOT NULL default ' ',
  "AAABHVUV" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHWBH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVUR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHWBH" varchar(15) NOT NULL default ' ',
  "AAABHVUU" varchar(15) NOT NULL default ' ',
  "AAABHVUV" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHWBH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVUY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVVT" varchar(10) NOT NULL default ' ',
  "AAABHWCF" varchar(6) NOT NULL default ' ',
  "AAABHWBG" varchar(8) NOT NULL default ' ',
  "AAABHWBK" decimal(5,0) NOT NULL default '0',
  "AAABHWBN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVVT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVVH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVVT" varchar(10) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVVT","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVVO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVVT" varchar(10) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVVT","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVVV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHWCF" varchar(6) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHWCF","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVWC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHWCF" varchar(6) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHWCF","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVWJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVXQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVWO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHVXQ" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHVXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVWW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVXN" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLUKK" varchar(1) NOT NULL default ' ',
  "AAABHVXC" varchar(2) NOT NULL default ' ',
  "AAABIYDW" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABHVXN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVXH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVZD" varchar(4) NOT NULL default ' ',
  "AAABHVXQ" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHVXN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVZD","AAABHVXQ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVXS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVXX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHVYO" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHVYO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYO" varchar(4) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABHVYM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYO","AAABHVYP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVYR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVZD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVZD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVYW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHVZD" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHVZD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVZE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVZQ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABHVZQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--enable_query_log
