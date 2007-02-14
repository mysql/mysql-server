# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+
# |                             tbl19.inc                              |
# |                        Copyright 2006 by MySQL AB                  |
# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+

--disable_query_log
CREATE TABLE "AAAARCYW" (
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
CREATE TABLE "AAAARCZI" (
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
CREATE TABLE "AAAARCZW" (
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
  UNIQUE KEY "AAAARDAM" ("AAABMRIL","AAABLLNL"),
  KEY "AAAARDAP" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDAR" (
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
  UNIQUE KEY "AAAARDBH" ("AAABMRIL","AAABLLNL"),
  KEY "AAAARDBK" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDBM" (
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
  UNIQUE KEY "AAAARDCA" ("AAABLLNL"),
  KEY "AAAARDCC" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDCE" (
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
  UNIQUE KEY "AAAARDCS" ("AAABLLNL"),
  KEY "AAAARDCU" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDCW" (
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
  UNIQUE KEY "AAAARDDM" ("AAABMRIL","AAABLLNL"),
  KEY "AAAARDDP" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDDR" (
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
  UNIQUE KEY "AAAARDEH" ("AAABMRIL","AAABLLNL"),
  KEY "AAAARDEK" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDEM" (
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
  UNIQUE KEY "AAAARDFC" ("AAABMRIL","AAABLLNL"),
  KEY "AAAARDFF" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDFH" (
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
  UNIQUE KEY "AAAARDFV" ("AAABLLNL"),
  KEY "AAAARDFX" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDFZ" (
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
  UNIQUE KEY "AAAARDGN" ("AAABLLNL"),
  KEY "AAAARDGP" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDGR" (
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
  UNIQUE KEY "AAAARDHH" ("AAABMRIL","AAABLLNL"),
  KEY "AAAARDHK" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDHM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAARDHY" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDIC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAARDIO" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDIS" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAARDJC" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDJG" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAARDJQ" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDJU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAARDKG" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDKK" (
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
  UNIQUE KEY "AAAARDKZ" ("AAABMRIL","AAABLLNL"),
  KEY "AAAARDLC" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDLE" (
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
  UNIQUE KEY "AAAARDLT" ("AAABMRIL","AAABLLNL"),
  KEY "AAAARDLW" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDLY" (
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
  UNIQUE KEY "AAAARDML" ("AAABLLNL"),
  KEY "AAAARDMN" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDMP" (
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
  UNIQUE KEY "AAAARDNC" ("AAABLLNL"),
  KEY "AAAARDNE" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDNG" (
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
  UNIQUE KEY "AAAARDNV" ("AAABMRIL","AAABLLNL"),
  KEY "AAAARDNY" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDOA" (
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
  UNIQUE KEY "AAAARDOP" ("AAABMRIL","AAABLLNL"),
  KEY "AAAARDOS" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDOU" (
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
  UNIQUE KEY "AAAARDPJ" ("AAABMRIL","AAABLLNL"),
  KEY "AAAARDPM" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDPO" (
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
  UNIQUE KEY "AAAARDQB" ("AAABLLNL"),
  KEY "AAAARDQD" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDQF" (
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
  UNIQUE KEY "AAAARDQS" ("AAABLLNL"),
  KEY "AAAARDQU" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDQW" (
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
  UNIQUE KEY "AAAARDRL" ("AAABMRIL","AAABLLNL"),
  KEY "AAAARDRO" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDRQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAABKRXG" varchar(1) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRXI" varchar(14) NOT NULL default ' ',
  "AAABLOLZ" varchar(4) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAABKRXL" varchar(2) NOT NULL default '00',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  "AAABKRXO" varchar(3) NOT NULL default '000',
  "AAABKRVS" varchar(1) NOT NULL default '0',
  "AAABGINJ" varchar(1) NOT NULL default ' ',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAAARDSG" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDH" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDI" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDJ" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDK" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDL" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDM" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDN" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDO" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDP" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDQ" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDR" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDS" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDT" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDU" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDV" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDW" decimal(15,3) NOT NULL default '0.000',
  "AAAARDSX" varchar(1) NOT NULL default ' ',
  "AAAAVUEN" varchar(1) NOT NULL default ' ',
  "AAAAVUEO" varchar(1) NOT NULL default ' ',
  "AAAAVUEP" varchar(1) NOT NULL default ' ',
  "AAAAVUEQ" varchar(1) NOT NULL default ' ',
  "AAAAVUER" varchar(1) NOT NULL default ' ',
  "AAAAVUES" varchar(1) NOT NULL default ' ',
  "AAAAVUET" varchar(1) NOT NULL default ' ',
  "AAAAVUEU" varchar(1) NOT NULL default ' ',
  "AAAAVUEV" varchar(1) NOT NULL default ' ',
  "AAAAVUEW" varchar(1) NOT NULL default ' ',
  "AAAAVUEX" varchar(1) NOT NULL default ' ',
  "AAAAVUEY" varchar(1) NOT NULL default ' ',
  "AAAAVUEZ" varchar(1) NOT NULL default ' ',
  "AAAAVUFA" varchar(1) NOT NULL default ' ',
  "AAAAVUFB" varchar(1) NOT NULL default ' ',
  "AAAAVUFC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAABKRXF","AAABKRXG","AAABMDBB","AAABKRXI","AAABLOLZ","AAABKRXK","AAABKRXL","AAABMCZG","AAABKRXN","AAABKRXO"),
  KEY "AAAARDUA" ("AAABLXTG","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDUD" (
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
CREATE TABLE "AAAARDUY" (
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
  KEY "AAAARDWA" ("AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDWC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKQYI" varchar(10) NOT NULL default '0000000000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABKQUO" varchar(4) NOT NULL default ' ',
  "AAABKQWF" varchar(4) NOT NULL default ' ',
  "AAAARDWQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABKQYI"),
  UNIQUE KEY "AAAARDWL" ("AAABMRIL","AAABMLCZ","AAABKQUO","AAABKQWF","AAAARDWQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDWR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKQYI" varchar(10) NOT NULL default '0000000000',
  "AAABKSAB" varchar(1) NOT NULL default ' ',
  "AAAARDXJ" varchar(5) NOT NULL default '00000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABKRNW" varchar(10) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABLXSU" varchar(1) NOT NULL default ' ',
  "AAABKROA" varchar(8) NOT NULL default '00000000',
  "AAABKROB" decimal(13,3) NOT NULL default '0.000',
  "AAABKROF" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKQYI","AAABKSAB","AAAARDXJ"),
  KEY "AAAARDXK" ("AAABMRIL","AAABMLCZ","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDXO" (
  "AAAARDXT" varchar(4) NOT NULL default '0000',
  "AAABJSLF" varchar(30) NOT NULL default ' ',
  "AAAARDXR" int(11) NOT NULL default '0',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAARDXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDXU" (
  "AAABLQGL" varchar(70) NOT NULL default ' ',
  "AAAARDYJ" varchar(6) NOT NULL default '000000',
  "AAAARDXX" varchar(132) NOT NULL default ' ',
  "AAABMMYV" varchar(30) NOT NULL default ' ',
  "AAABMKFH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLQGL","AAAARDYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDYC" (
  "AAABLQGL" varchar(70) NOT NULL default ' ',
  "AAAARDYJ" varchar(6) NOT NULL default '000000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARDYG" varchar(255) NOT NULL default ' ',
  "AAABCFJB" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLQGL","AAAARDYJ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDYL" (
  "AAABBDQH" varchar(2) NOT NULL default '00',
  "AAAARDYN" longtext,
  PRIMARY KEY ("AAABBDQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDYP" (
  "AAABLUSV" int(11) NOT NULL default '0',
  "AAABMMRH" int(11) NOT NULL default '0',
  "AAAARDYS" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABMMRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDYV" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABGFYL" varchar(30) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAAARDZW" varchar(4) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAAARDZC" varchar(1) NOT NULL default ' ',
  "AAAARDZD" int(11) NOT NULL default '0',
  "AAABIGGP" varchar(1) NOT NULL default ' ',
  "AAABGFKN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHKQ","AAABGFYL","AAABIWNN","AAAARDZW","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDZL" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABGFYL" varchar(30) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAAARDZW" varchar(4) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCFJB" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHKQ","AAABGFYL","AAABIWNN","AAAARDZW","AAABMDEN","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARDZZ" (
  "AAAAVJHP" varchar(32) NOT NULL default ' ',
  "AAAAREAL" varchar(64) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABGFLE" int(11) NOT NULL default '0',
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAAARVLG" varchar(30) NOT NULL default ' ',
  "AAABLUFA" varchar(10) NOT NULL default ' ',
  "AAABMRFO" varchar(4) NOT NULL default ' ',
  "AAAARVOH" int(11) NOT NULL default '0',
  "AAAAREAJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAVJHP","AAAAREAL","AAABMKFU","AAABGFLE"),
  KEY "AAAAREAO" ("AAAARVLG"),
  KEY "AAAAREAQ" ("AAAARYNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREAS" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAAZLLF" varchar(12) NOT NULL default ' ',
  "AAABMCWV" varchar(30) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABJHAF" varchar(8) NOT NULL default '00000000',
  "AAABJHAG" varchar(6) NOT NULL default '000000',
  "AAAAREAZ" varchar(60) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAAAZLLF","AAABMCWV","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREBG" (
  "AAAARSHO" varchar(3) NOT NULL default ' ',
  "AAAARSHP" varchar(3) NOT NULL default ' ',
  "AAABLFWZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARSHO","AAAARSHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREBM" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARSHO" varchar(3) NOT NULL default ' ',
  "AAAARSHP" varchar(3) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAARSHO","AAAARSHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREBU" (
  "AAAARECK" varchar(10) NOT NULL default ' ',
  "AAAARXQG" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAAAREBY" varchar(1) NOT NULL default ' ',
  "AAAAREBZ" varchar(6) NOT NULL default '000000',
  "AAAARECA" varchar(1) NOT NULL default ' ',
  "AAAARECB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARECK","AAAARXQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARECE" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARECK" varchar(10) NOT NULL default ' ',
  "AAAARXQG" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAARECK","AAAARXQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARECM" (
  "AAAARYMD" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIJQN" varchar(6) NOT NULL default '000000',
  "AAABMMRH" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYMD","AAABIWFD","AAABIJQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARECU" (
  "AAAARYMD" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIJQN" varchar(6) NOT NULL default '000000',
  "AAABMMRH" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYMD","AAABIWFD","AAABIJQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREDC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRZZ" varchar(10) NOT NULL default '0000000000',
  "AAABLMJL" varchar(4) NOT NULL default '0000',
  "AAABKSAB" varchar(1) NOT NULL default ' ',
  "AAABLUFJ" varchar(12) NOT NULL default ' ',
  "AAABLUHX" varchar(10) NOT NULL default ' ',
  "AAABKRXX" varchar(1) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABKRXZ" varchar(10) NOT NULL default ' ',
  "AAABKRYA" varchar(10) NOT NULL default ' ',
  "AAABLXTZ" varchar(12) NOT NULL default '000000000000',
  "AAABLXUA" varchar(4) NOT NULL default ' ',
  "AAABLUFL" decimal(9,5) NOT NULL default '0.00000',
  "AAABKRYE" varchar(1) NOT NULL default ' ',
  "AAABKRYF" varchar(3) NOT NULL default ' ',
  "AAABKSSD" varchar(10) NOT NULL default ' ',
  "AAABKRYH" varchar(10) NOT NULL default ' ',
  "AAABKRYI" varchar(8) NOT NULL default ' ',
  "AAABKRYJ" varchar(1) NOT NULL default ' ',
  "AAABKRYK" varchar(6) NOT NULL default '000000',
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABKRYM" VARBINARY(255),
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABKRYO" varchar(2) NOT NULL default ' ',
  "AAABKRYP" varchar(1) NOT NULL default ' ',
  "AAABLTZM" varchar(8) NOT NULL default '00000000',
  "AAABLTZN" varchar(12) NOT NULL default ' ',
  "AAABKRYS" varchar(63) NOT NULL default ' ',
  "AAABKRYT" varchar(12) NOT NULL default ' ',
  "AAABKRYU" varchar(8) NOT NULL default '00000000',
  "AAABKRYV" varchar(8) NOT NULL default '00000000',
  "AAABLUFU" varchar(35) NOT NULL default ' ',
  "AAABKRYX" varchar(40) NOT NULL default ' ',
  "AAABKRYY" varchar(10) NOT NULL default ' ',
  "AAABKRYZ" varchar(20) NOT NULL default ' ',
  "AAABKRZA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRZZ","AAABLMJL","AAABKSAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREER" (
  "AAABMKFH" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREEV" (
  "AAAAROSN" varchar(8) NOT NULL default ' ',
  "AAAAREEX" varchar(12) NOT NULL default ' ',
  "AAAAREEY" varchar(12) NOT NULL default ' ',
  "AAAAREEZ" varchar(20) NOT NULL default ' ',
  "AAAAREFA" varchar(27) NOT NULL default ' ',
  "AAAAREFB" varchar(27) NOT NULL default ' ',
  "AAAAREFC" varchar(3) NOT NULL default ' ',
  "AAAAREFD" varchar(4) NOT NULL default ' ',
  "AAAAREFE" varchar(5) NOT NULL default '00000',
  "AAAAREFF" varchar(7) NOT NULL default '0000000',
  "AAAAREFG" varchar(3) NOT NULL default '000',
  "AAAAREFH" varchar(10) NOT NULL default '0000000000',
  "AAAAREFI" varchar(1) NOT NULL default ' ',
  "AAAAREFJ" varchar(3) NOT NULL default ' ',
  "AAAAREFK" varchar(3) NOT NULL default '000',
  "AAAAREFL" varchar(8) NOT NULL default '00000000',
  "AAAAREFM" varchar(6) NOT NULL default '000000',
  "AAAAREFN" varchar(1) NOT NULL default ' ',
  "AAAAREFO" varchar(32) NOT NULL default ' ',
  "AAAAREFP" varchar(32) NOT NULL default ' ',
  "AAAAREFQ" varchar(32) NOT NULL default ' ',
  "AAAAREFR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAROSN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREFT" (
  "AAAAROSN" varchar(8) NOT NULL default ' ',
  "AAAAREFV" varchar(12) NOT NULL default ' ',
  "AAAAREFW" varchar(1) NOT NULL default '0',
  "AAAARXBI" varchar(1) NOT NULL default ' ',
  "AAAAREFY" varchar(1) NOT NULL default ' ',
  "AAAAREFZ" varchar(1) NOT NULL default ' ',
  "AAAAREGA" varchar(1) NOT NULL default ' ',
  "AAAAREGB" varchar(1) NOT NULL default ' ',
  "AAAAREGC" varchar(2) NOT NULL default '00',
  "AAAAREGD" varchar(8) NOT NULL default '00000000',
  "AAAAREGE" varchar(14) NOT NULL default '00000000000000',
  "AAAAREGF" varchar(73) NOT NULL default ' ',
  "AAAAREGG" varchar(73) NOT NULL default ' ',
  "AAAAREGH" varchar(73) NOT NULL default ' ',
  "AAAAREGI" varchar(73) NOT NULL default ' ',
  "AAAAREGJ" varchar(1) NOT NULL default ' ',
  "AAAAREGK" varchar(73) NOT NULL default ' ',
  "AAAAREGL" decimal(15,0) NOT NULL default '0',
  "AAAAREGM" varchar(1) NOT NULL default ' ',
  "AAAAREGN" varchar(1) NOT NULL default ' ',
  "AAAAREGO" varchar(1) NOT NULL default ' ',
  "AAAAREGP" varchar(1) NOT NULL default ' ',
  "AAAAREGQ" varchar(1) NOT NULL default ' ',
  "AAAAREGR" varchar(1) NOT NULL default ' ',
  "AAAAREGS" varchar(2) NOT NULL default '00',
  "AAAAREGT" int(11) NOT NULL default '0',
  "AAAAREGU" varchar(1) NOT NULL default '0',
  "AAAAREGV" int(11) NOT NULL default '0',
  "AAAAREGW" varchar(80) NOT NULL default ' ',
  "AAAAREGX" varchar(10) NOT NULL default ' ',
  "AAAAREGY" varchar(80) NOT NULL default ' ',
  "AAAARWHO" varchar(25) NOT NULL default ' ',
  "AAABAKZI" varchar(20) NOT NULL default ' ',
  "AAAARZQI" varchar(30) NOT NULL default ' ',
  "AAAAREHC" varchar(30) NOT NULL default ' ',
  "AAAAREHD" varchar(1) NOT NULL default ' ',
  "AAAAVJSZ" varchar(80) NOT NULL default ' ',
  "AAAAREHF" varchar(32) NOT NULL default ' ',
  "AAAAREHG" varchar(1) NOT NULL default ' ',
  "AAAARJNG" varchar(60) NOT NULL default ' ',
  "AAAAREHI" varchar(1) NOT NULL default ' ',
  "AAAAREHJ" varchar(32) NOT NULL default ' ',
  "AAAAREHK" varchar(1) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  "AAAAREHM" int(11) NOT NULL default '0',
  "AAAAREHN" int(11) NOT NULL default '0',
  "AAAAREHO" varchar(1) NOT NULL default ' ',
  "AAABLEJW" varchar(60) NOT NULL default ' ',
  "AAAAREHQ" varchar(60) NOT NULL default ' ',
  "AAAAREHR" varchar(10) NOT NULL default ' ',
  "AAAAREHS" varchar(1) NOT NULL default ' ',
  "AAAAREHT" varchar(1) NOT NULL default ' ',
  "AAAAREHU" varchar(1) NOT NULL default ' ',
  "AAAAREHV" varchar(1) NOT NULL default ' ',
  "AAAAREHW" varchar(40) NOT NULL default ' ',
  "AAAAREHX" varchar(8) NOT NULL default '00000000',
  "AAAAREHY" varchar(8) NOT NULL default '00000000',
  "AAAAREHZ" varchar(30) NOT NULL default ' ',
  "AAAAREIA" varchar(30) NOT NULL default ' ',
  "AAAAREIB" varchar(30) NOT NULL default ' ',
  "AAAAREIC" varchar(8) NOT NULL default '00000000',
  "AAAAREID" varchar(1) NOT NULL default ' ',
  "AAAAREIE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAROSN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREIG" (
  "AAABMKFH" varchar(30) NOT NULL default ' ',
  "AAAAREII" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREIK" (
  "AAAAROSN" varchar(8) NOT NULL default ' ',
  "AAAAREIM" varchar(1) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAAREIO" varchar(10) NOT NULL default ' ',
  "AAAAREIP" varchar(10) NOT NULL default ' ',
  "AAAAREIQ" varchar(1) NOT NULL default ' ',
  "AAAAREIR" varchar(1) NOT NULL default ' ',
  "AAAAREIS" varchar(8) NOT NULL default ' ',
  "AAAAREIT" varchar(1) NOT NULL default ' ',
  "AAABLMIG" varchar(3) NOT NULL default ' ',
  "AAAARPSO" varchar(1) NOT NULL default ' ',
  "AAAAREIW" varchar(8) NOT NULL default '00000000',
  "AAAAREIX" varchar(8) NOT NULL default '00000000',
  "AAAATQIT" varchar(2) NOT NULL default ' ',
  "AAAAREIZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAROSN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREJB" (
  "AAABCQHT" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAAREJE" varchar(2) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  "AAABJLPP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABCQHT","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREJQ" (
  "AAAAREJV" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAAREJT" varchar(25) NOT NULL default ' ',
  "AAABHCYD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAREJV","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREJX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFNVZ" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABFNVZ","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREKF" (
  "AAABFNVZ" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAAREKI" varchar(25) NOT NULL default ' ',
  "AAAAREKJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFNVZ","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREKM" (
  "AAAAREKP" varchar(1) NOT NULL default ' ',
  "AAAAREKO" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAAAREKP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREKQ" (
  "AAAAROSN" varchar(8) NOT NULL default ' ',
  "AAAAREKS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAROSN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREKU" (
  "AAABMJER" varchar(30) NOT NULL default ' ',
  "AAABJSLF" varchar(50) NOT NULL default ' ',
  "AAAAREKX" varchar(30) NOT NULL default ' ',
  "AAAAREKY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMJER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARELA" (
  "AAABMJER" varchar(30) NOT NULL default ' ',
  "AAABGFET" varchar(30) NOT NULL default ' ',
  "AAABLEEK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMJER","AAABGFET")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARELG" (
  "AAABMJER" varchar(30) NOT NULL default ' ',
  "AAABLMUZ" varchar(3) NOT NULL default ' ',
  "AAAARELJ" varchar(30) NOT NULL default ' ',
  "AAAARELK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMJER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARELM" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMJER" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMJER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARELS" (
  "AAAAREMC" varchar(30) NOT NULL default ' ',
  "AAAARELU" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAAREMC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARELW" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAREMC" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(20) NOT NULL default ' ',
  "AAABEZGI" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAREMC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREMD" (
  "AAABLJJN" varchar(30) NOT NULL default ' ',
  "AAAAREMF" varchar(30) NOT NULL default ' ',
  "AAAAREMG" varchar(1) NOT NULL default ' ',
  "AAABMJER" varchar(30) NOT NULL default ' ',
  "AAAAREMI" varchar(60) NOT NULL default ' ',
  "AAAAREMJ" varchar(80) NOT NULL default ' ',
  "AAAAREMK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLJJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREMM" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLJJN" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(20) NOT NULL default ' ',
  "AAABEZGI" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLJJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREMT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAREMX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAREMX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREMY" (
  "AAABKWJY" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  "AAABLECD" varchar(16) NOT NULL default ' ',
  "AAABKWWJ" longtext,
  PRIMARY KEY ("AAABKWJY","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARENN" (
  "AAABKWJY" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARPSA" varchar(3) NOT NULL default '000',
  "AAABHFOL" varchar(4) NOT NULL default ' ',
  "AAABBDMV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABKWJY","AAABIWFD","AAAARPSA","AAABHFOL","AAABBDMV"),
  KEY "AAAARENY" ("AAABHFOL","AAABBDMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREOB" (
  "AAAAREPB" varchar(30) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAAAREOE" varchar(1) NOT NULL default ' ',
  "AAABKWJY" varchar(30) NOT NULL default ' ',
  "AAAAREOG" varchar(12) NOT NULL default ' ',
  "AAABLTLM" decimal(15,0) NOT NULL default '0',
  "AAABLTLN" decimal(15,0) NOT NULL default '0',
  "AAAAREOT" varchar(4) NOT NULL default ' ',
  "AAAAREOS" varchar(40) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAABLRFC" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAAAREPB"),
  KEY "AAAAREOP" ("AAABKWJY"),
  KEY "AAAAREOR" ("AAAAREOS","AAAAREOT"),
  KEY "AAAAREOU" ("AAABLTLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREOW" (
  "AAAAREPB" varchar(30) NOT NULL default ' ',
  "AAABHMRI" varchar(30) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAAREPB","AAABHMRI","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREPE" (
  "AAABKWJY" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABKWJY","AAABIWFD","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREPN" (
  "AAABKWJY" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(30) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABKWJY","AAABIWFD","AAABMLQA","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREPX" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMLZS" varchar(3) NOT NULL default '000',
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEN","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREQD" (
  "AAABLECD" varchar(16) NOT NULL default ' ',
  "AAAAREQF" varchar(16) NOT NULL default ' ',
  "AAAAREQG" varchar(4) NOT NULL default '0000',
  "AAAAREQH" varchar(40) NOT NULL default ' ',
  "AAAAREQI" varchar(1) NOT NULL default ' ',
  "AAABIJOU" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREQL" (
  "AAABLECD" varchar(16) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREQR" (
  "AAAARESS" varchar(32) NOT NULL default ' ',
  "AAABCREP" varchar(1) NOT NULL default ' ',
  "AAAARISU" varchar(12) NOT NULL default ' ',
  "AAAAREQV" decimal(15,0) NOT NULL default '0',
  "AAAAREQW" varchar(10) NOT NULL default ' ',
  "AAAAREQX" varchar(30) NOT NULL default ' ',
  "AAAAREQY" varchar(30) NOT NULL default ' ',
  "AAAAREQZ" varchar(10) NOT NULL default ' ',
  "AAAARERA" varchar(3) NOT NULL default '000',
  "AAAARERB" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAARESS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARERD" (
  "AAAARESS" varchar(32) NOT NULL default ' ',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARESS"),
  UNIQUE KEY "AAAARERH" ("AAABMNMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARERJ" (
  "AAAARESS" varchar(32) NOT NULL default ' ',
  "AAABLMNC" int(11) NOT NULL default '0',
  "AAAARERM" varchar(30) NOT NULL default ' ',
  "AAAARERN" varchar(30) NOT NULL default ' ',
  "AAAARERO" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAARESS","AAABLMNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARERR" (
  "AAAARESS" varchar(32) NOT NULL default ' ',
  "AAAARFLY" varchar(60) NOT NULL default ' ',
  "AAAARERZ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAARESS"),
  UNIQUE KEY "AAAARERW" ("AAAARFLY"),
  UNIQUE KEY "AAAARERY" ("AAAARERZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARESA" (
  "AAAARESI" varchar(10) NOT NULL default ' ',
  "AAABHZQN" varchar(30) NOT NULL default ' ',
  "AAAARESK" varchar(30) NOT NULL default ' ',
  "AAAARESL" varchar(10) NOT NULL default ' ',
  "AAAARESM" varchar(3) NOT NULL default '000',
  "AAAARESS" varchar(32) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAARESI","AAABHZQN","AAAARESK","AAAARESL","AAAARESM"),
  KEY "AAAARESN" ("AAAARESS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARESP" (
  "AAAARESS" varchar(32) NOT NULL default ' ',
  "AAABKWWJ" longblob,
  PRIMARY KEY ("AAAARESS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREST" (
  "AAABIJOU" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABIJOU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARESW" (
  "AAABIJOU" varchar(16) NOT NULL default ' ',
  "AAABLJJN" varchar(30) NOT NULL default ' ',
  "AAAARESZ" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABIJOU","AAABLJJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARETC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAARKOT" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAARKOT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARETH" (
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAABLEJW" varchar(255) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABLTVR" varchar(25) NOT NULL default ' ',
  "AAAARETM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATJNW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARETO" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAAARETQ" varchar(25) NOT NULL default ' ',
  "AAAARETR" decimal(15,0) NOT NULL default '0',
  "AAAARETS" varchar(25) NOT NULL default ' ',
  "AAAARETT" decimal(15,0) NOT NULL default '0',
  "AAAARETU" varchar(30) NOT NULL default ' ',
  "AAAAREUH" int(11) NOT NULL default '0',
  "AAAARETW" int(11) NOT NULL default '0',
  "AAAARETX" varchar(1) NOT NULL default ' ',
  "AAAARETY" double NOT NULL default '0',
  "AAAARETZ" int(11) NOT NULL default '0',
  "AAAAREUA" decimal(15,0) NOT NULL default '0',
  "AAABLTXI" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABIHKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREUD" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAAAREUH" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABIHKQ","AAAAREUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREUI" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABIKGM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREUN" (
  "AAABFYWO" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAAREVI" varchar(30) NOT NULL default ' ',
  "AAAAREVJ" varchar(30) NOT NULL default ' ',
  "AAAAREVK" varchar(30) NOT NULL default ' ',
  "AAABILHK" varchar(4) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABFYWO","AAABIWFD"),
  KEY "AAAAREVD" ("AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREVF" (
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAABFYWO" varchar(30) NOT NULL default ' ',
  "AAAAREVI" varchar(30) NOT NULL default ' ',
  "AAAAREVJ" varchar(30) NOT NULL default ' ',
  "AAAAREVK" varchar(30) NOT NULL default ' ',
  "AAABILHK" varchar(4) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAARZRE","AAABFYWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREVQ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFYWO" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTO" varchar(40) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABFYWO","AAABIWFD"),
  KEY "AAAAREWA" ("AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREWC" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAABFYWO" varchar(30) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTO" varchar(40) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAARZRE","AAABFYWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREWM" (
  "AAABIWNE" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAAAREWR" varchar(20) NOT NULL default ' ',
  "AAABLEJW" varchar(60) NOT NULL default ' ',
  "AAAAREWT" varchar(1) NOT NULL default ' ',
  "AAAAREWU" int(11) NOT NULL default '0',
  "AAAAREWV" int(11) NOT NULL default '0',
  "AAAAREWW" varchar(30) NOT NULL default ' ',
  "AAAAREWX" varchar(1) NOT NULL default ' ',
  "AAAAREWY" varchar(1) NOT NULL default ' ',
  "AAAAREWZ" varchar(1) NOT NULL default ' ',
  "AAAAREXA" varchar(32) NOT NULL default ' ',
  "AAAAREXB" varchar(64) NOT NULL default ' ',
  "AAAAVILO" varchar(1) NOT NULL default ' ',
  "AAAAREXD" varchar(1) NOT NULL default ' ',
  "AAAARLIA" varchar(30) NOT NULL default ' ',
  "AAAAREZN" varchar(1) NOT NULL default ' ',
  "AAAAREZO" decimal(3,0) NOT NULL default '0',
  "AAAAREZP" int(11) NOT NULL default '0',
  "AAAAREXI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWNE","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREXL" (
  "AAABIWNE" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAAAREXP" varchar(4) NOT NULL default '0000',
  "AAABDFYR" varchar(1) NOT NULL default ' ',
  "AAAAREXR" varchar(4) NOT NULL default '0000',
  "AAAAREXS" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABIWNE","AAABIWFD","AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREXW" (
  "AAABIWNE" varchar(30) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAAAREYB" varchar(1) NOT NULL default '0',
  "AAAAREYC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWNE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREYE" (
  "AAABIWNE" varchar(30) NOT NULL default ' ',
  "AAAARFAR" varchar(4) NOT NULL default '0000',
  "AAABIKFN" varchar(30) NOT NULL default ' ',
  "AAABLQVJ" varchar(4) NOT NULL default '0000',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABLDPO" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(60) NOT NULL default ' ',
  "AAABMNNA" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWNE","AAAARFAR","AAABIKFN","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREYR" (
  "AAABIWNE" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABEDDT" varchar(1) NOT NULL default ' ',
  "AAABICEE" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWNE","AAABIWFD","AAABEDDT"),
  UNIQUE KEY "AAAAREYZ" ("AAABICEE","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREZC" (
  "AAABIWQH" varchar(30) NOT NULL default ' ',
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABLLJR" varchar(6) NOT NULL default '000000',
  "AAAAREZG" varchar(1) NOT NULL default ' ',
  "AAAAREZH" decimal(15,0) NOT NULL default '0',
  "AAAARHQS" decimal(15,0) NOT NULL default '0',
  "AAAAREZJ" varchar(60) NOT NULL default ' ',
  "AAAAREZK" int(11) NOT NULL default '0',
  "AAAAREZL" int(11) NOT NULL default '0',
  "AAAAREZM" int(11) NOT NULL default '0',
  "AAAAREZN" varchar(1) NOT NULL default ' ',
  "AAAAREZO" decimal(3,0) NOT NULL default '0',
  "AAAAREZP" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABIWQH"),
  KEY "AAAAREZR" ("AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREZT" (
  "AAABIWQH" varchar(30) NOT NULL default ' ',
  "AAABLLJR" varchar(6) NOT NULL default '000000',
  "AAAAWCJG" varchar(20) NOT NULL default ' ',
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAAAREZY" decimal(15,0) NOT NULL default '0',
  "AAABMQXM" int(11) NOT NULL default '0',
  "AAAARFAA" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABIWQH","AAABLLJR","AAAAWCJG"),
  KEY "AAAARFAE" ("AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFAG" (
  "AAABIWQH" varchar(30) NOT NULL default ' ',
  "AAAARFAR" varchar(4) NOT NULL default '0000',
  "AAABIKFN" varchar(30) NOT NULL default ' ',
  "AAABLQVJ" varchar(4) NOT NULL default '0000',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABLDPO" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(60) NOT NULL default ' ',
  "AAABMNNA" varchar(60) NOT NULL default ' ',
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWQH","AAAARFAR","AAABIKFN","AAABLQVJ"),
  KEY "AAAARFAU" ("AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFAW" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAAAWYIH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHKQ","AAAAWYIH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFBB" (
  "AAAARYMD" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYMD","AAABIWFD","AAABFTZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFBI" (
  "AAAARYMD" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARFBL" varchar(2) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  "AAABJLPP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYMD","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFBX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARYMD" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAARYMD","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFCF" (
  "AAABGHPP" varchar(8) NOT NULL default ' ',
  "AAAARFCT" varchar(4) NOT NULL default '0000',
  "AAAARFCI" varchar(8) NOT NULL default ' ',
  "AAABGPGH" int(11) NOT NULL default '0',
  "AAABLUUW" int(11) NOT NULL default '0',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAARFCO" varchar(12) NOT NULL default ' ',
  "AAABLMKH" varchar(8) NOT NULL default '00000000',
  "AAABFTXS" VARBINARY(255),
  "AAAARFCR" VARBINARY(255),
  PRIMARY KEY ("AAABGHPP","AAAARFCT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFCU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAARXFT" varchar(32) NOT NULL default ' ',
  "AAABKPPY" varchar(10) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABIWNE" varchar(30) NOT NULL default ' ',
  "AAAARXFL" varchar(30) NOT NULL default ' ',
  "AAAARXFM" varchar(1) NOT NULL default ' ',
  "AAAARXFN" varchar(15) NOT NULL default '000000000000000',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAAARXFT","AAABKPPY","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFDM" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARFDR" varchar(1) NOT NULL default ' ',
  "AAABLPWW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAARFDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFDS" (
  "AAABFNEP" varchar(10) NOT NULL default ' ',
  "AAABFNEQ" varchar(10) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARFDW" varchar(30) NOT NULL default ' ',
  "AAAARFDX" varchar(3) NOT NULL default '000',
  "AAAARFDY" varchar(2) NOT NULL default ' ',
  "AAABJETS" varchar(1) NOT NULL default ' ',
  "AAABJBGU" varchar(4) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAAARFED" varchar(4) NOT NULL default ' ',
  "AAAARFEE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABFNEP","AAABFNEQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFEH" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABLXTF" varchar(4) NOT NULL default '0000',
  "AAABGYRY" varchar(2) NOT NULL default ' ',
  "AAABGYRZ" varchar(4) NOT NULL default ' ',
  "AAABGYSA" varchar(2) NOT NULL default ' ',
  "AAABGYSC" varchar(54) NOT NULL default ' ',
  "AAABKWHA" varchar(32) NOT NULL default ' ',
  "AAABGYSE" varchar(14) NOT NULL default ' ',
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABLHAH" longtext,
  "AAAARFIB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABIHKQ","AAABIWFD","AAABLXTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFEX" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABIHKQ","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFFJ" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABLXTF" varchar(4) NOT NULL default '0000',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAAARFHB" varchar(1) NOT NULL default ' ',
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  "AAABFTZK" varchar(30) NOT NULL default ' ',
  "AAABFMTC" varchar(30) NOT NULL default ' ',
  "AAABBVNZ" varchar(20) NOT NULL default ' ',
  "AAABMNLA" varchar(1) NOT NULL default ' ',
  "AAABLISJ" varchar(1) NOT NULL default ' ',
  "AAABBQFF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHKQ","AAABIWFD","AAABLXTF","AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFGA" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLXTF" varchar(4) NOT NULL default '0000',
  "AAABMRCK" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHKQ","AAABIWFD","AAABMLQA","AAABLXTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFGK" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABINIK" decimal(15,0) NOT NULL default '0',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFGV" (
  "AAAARZBP" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABLXTF" varchar(4) NOT NULL default '0000',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAAARFHB" varchar(1) NOT NULL default ' ',
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  "AAABFTZK" varchar(30) NOT NULL default ' ',
  "AAABFMTC" varchar(30) NOT NULL default ' ',
  "AAABBVNZ" varchar(20) NOT NULL default ' ',
  "AAABMNLA" varchar(1) NOT NULL default ' ',
  "AAABLISJ" varchar(1) NOT NULL default ' ',
  "AAABBQFF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAARZBP","AAABIWFD","AAABIHKQ","AAABLXTF","AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFHO" (
  "AAAARZBP" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABLXTF" varchar(4) NOT NULL default '0000',
  "AAABGYRY" varchar(2) NOT NULL default ' ',
  "AAABGYRZ" varchar(4) NOT NULL default ' ',
  "AAABGYSA" varchar(2) NOT NULL default ' ',
  "AAABGYSC" varchar(54) NOT NULL default ' ',
  "AAABKWHA" varchar(32) NOT NULL default ' ',
  "AAABGYSE" varchar(14) NOT NULL default ' ',
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABLHAH" longtext,
  "AAAARFIB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAAARZBP","AAABIWFD","AAABIHKQ","AAABLXTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFIG" (
  "AAAARZBP" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAARZBP","AAABIWFD","AAABIHKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFIU" (
  "AAAARZBP" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABLXTF" varchar(4) NOT NULL default '0000',
  "AAABMRCK" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAAARZBP","AAABIWFD","AAABMLQA","AAABIHKQ","AAABLXTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFJG" (
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLHAH" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFJK" (
  "AAABIWQH" int(11) NOT NULL default '0',
  "AAAARGBB" varchar(6) NOT NULL default '000000',
  "AAAARGBE" int(11) NOT NULL default '0',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAARGAC" decimal(15,0) NOT NULL default '0',
  "AAAARGAD" decimal(15,0) NOT NULL default '0',
  "AAAAVQDA" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABIWQH","AAAARGBB","AAAARGBE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFJV" (
  "AAABIWQH" int(11) NOT NULL default '0',
  "AAABIHLK" int(11) NOT NULL default '0',
  "AAAARGBB" varchar(6) NOT NULL default '000000',
  "AAAARGBD" varchar(1) NOT NULL default ' ',
  "AAAARGBE" int(11) NOT NULL default '0',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  "AAABLDEN" decimal(15,0) NOT NULL default '0',
  "AAAARTLU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWQH","AAABIHLK","AAAARGBB","AAAARGBD","AAAARGBE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFKP" (
  "AAAARFXS" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABLDEV" varchar(30) NOT NULL default ' ',
  "AAAARFKT" varchar(1) NOT NULL default ' ',
  "AAAAUNXM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARFXS","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFKX" (
  "AAAARVLM" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARVLM","AAABIWFD","AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFLF" (
  "AAABLQGL" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFLI" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLQGL" varchar(4) NOT NULL default '0000',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFLP" (
  "AAAARFXS" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABLRJS" varchar(128) NOT NULL default ' ',
  "AAABLQHQ" varchar(255) NOT NULL default ' ',
  "AAABLOKS" varchar(1) NOT NULL default ' ',
  "AAAARFLV" varchar(10) NOT NULL default ' ',
  "AAAAVQFA" varchar(1) NOT NULL default ' ',
  "AAAARFYC" varchar(20) NOT NULL default ' ',
  "AAAARFLY" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAARFXS","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFMB" (
  "AAAARVLM" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABLRJQ" varchar(4) NOT NULL default '0000',
  "AAABMLMY" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAARVLM","AAABIWFD","AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFMK" (
  "AAABHEYG" varchar(30) NOT NULL default ' ',
  "AAAARYEC" varchar(6) NOT NULL default '000000',
  "AAAARVLG" varchar(30) NOT NULL default ' ',
  "AAABIJNY" int(11) NOT NULL default '0',
  "AAABLQGL" varchar(4) NOT NULL default '0000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABHEYG","AAAARYEC","AAAARVLG","AAABIJNY","AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFMY" (
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFNB" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARFNH" varchar(10) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAARFNH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFNI" (
  "AAAARFNQ" varchar(12) NOT NULL default ' ',
  "AAAARFNK" varchar(2) NOT NULL default '00',
  "AAABLRXN" varchar(8) NOT NULL default '00000000',
  "AAABLRXO" varchar(8) NOT NULL default '00000000',
  "AAAARVLM" varchar(30) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABIWQH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARFNQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFNR" (
  "AAAARRIM" varchar(30) NOT NULL default ' ',
  "AAABIWQH" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAARRIM","AAABIWQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFNW" (
  "AAAARRIM" varchar(30) NOT NULL default ' ',
  "AAAARFWY" varchar(1) NOT NULL default ' ',
  "AAABLHGN" int(11) NOT NULL default '0',
  "AAAARFOA" int(11) NOT NULL default '0',
  "AAAARFOB" int(11) NOT NULL default '0',
  "AAAARFOC" int(11) NOT NULL default '0',
  "AAAARXWI" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAAARRIM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFOF" (
  "AAAARFXS" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARFOI" varchar(4) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAARYFH" varchar(30) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAARFXS","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFOV" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARFXS" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAARFXS","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFPE" (
  "AAAARFXS" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAAARFXS","AAABIWFD","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFPN" (
  "AAAARVLR" varchar(30) NOT NULL default ' ',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAAARLSM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARVLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFPS" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARVLR" varchar(30) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAARVLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFPY" (
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAAARFQL" varchar(16) NOT NULL default '0000000000000000',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAAARYEC" varchar(6) NOT NULL default '000000',
  "AAAARFQD" varchar(1) NOT NULL default ' ',
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFO" varchar(10) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAAAROVJ" varchar(30) NOT NULL default ' ',
  "AAAARFQJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTJH","AAAARFQL","AAABLTJI","AAAARYEC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFQO" (
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARMEE" varchar(30) NOT NULL default ' ',
  "AAAARMEF" decimal(15,0) NOT NULL default '0',
  "AAAARFQT" varchar(25) NOT NULL default ' ',
  "AAAARMEH" varchar(30) NOT NULL default ' ',
  "AAAARMEI" decimal(15,0) NOT NULL default '0',
  "AAAARMEJ" varchar(25) NOT NULL default ' ',
  "AAAARMEK" varchar(30) NOT NULL default ' ',
  "AAAARMEL" decimal(15,0) NOT NULL default '0',
  "AAAARMEM" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWNN","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFRC" (
  "AAAARFVW" int(11) NOT NULL default '0',
  "AAAARFVS" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAARFVW","AAAARFVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFRH" (
  "AAABIWQH" int(11) NOT NULL default '0',
  "AAABIHLK" int(11) NOT NULL default '0',
  "AAAARGBD" varchar(1) NOT NULL default ' ',
  "AAAARGBE" int(11) NOT NULL default '0',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  "AAABLDEN" decimal(15,0) NOT NULL default '0',
  "AAAARTLU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWQH","AAABIHLK","AAAARGBD","AAAARGBE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFRZ" (
  "AAABIWQH" int(11) NOT NULL default '0',
  "AAABHEYG" varchar(30) NOT NULL default ' ',
  "AAAARVLM" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(5) NOT NULL default '00000',
  "AAAARXBN" varchar(2) NOT NULL default ' ',
  "AAABCKCB" varchar(1) NOT NULL default ' ',
  "AAAARFXT" varchar(30) NOT NULL default ' ',
  "AAABJETS" varchar(6) NOT NULL default ' ',
  "AAAARFXS" varchar(30) NOT NULL default ' ',
  "AAABIGIZ" varchar(25) NOT NULL default ' ',
  "AAAAVQDD" varchar(6) NOT NULL default '000000',
  "AAAARFSL" varchar(30) NOT NULL default ' ',
  "AAAARYFH" varchar(30) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAAARFYC" varchar(20) NOT NULL default ' ',
  "AAAARFYD" varchar(1) NOT NULL default ' ',
  "AAABHGJR" varchar(25) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAAARPBZ" varchar(1) NOT NULL default ' ',
  "AAAAVQDA" int(11) NOT NULL default '0',
  "AAAARGBE" int(11) NOT NULL default '0',
  "AAAARGAC" decimal(15,0) NOT NULL default '0',
  "AAAARGAD" decimal(15,0) NOT NULL default '0',
  "AAAARGBD" varchar(1) NOT NULL default ' ',
  "AAAARFTU" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABIWQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFTB" (
  "AAABIWQH" int(11) NOT NULL default '0',
  "AAAARFXS" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWQH"),
  KEY "AAAARFTG" ("AAAARFXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFTI" (
  "AAABIWQH" int(11) NOT NULL default '0',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWQH","AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFTN" (
  "AAABIWQH" int(11) NOT NULL default '0',
  "AAAARGBE" int(11) NOT NULL default '0',
  "AAAARPBZ" varchar(1) NOT NULL default ' ',
  "AAAAVQDA" int(11) NOT NULL default '0',
  "AAAARGAC" decimal(15,0) NOT NULL default '0',
  "AAAARGAD" decimal(15,0) NOT NULL default '0',
  "AAAARFTU" int(11) NOT NULL default '0',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWQH","AAAARGBE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFTY" (
  "AAABIWQH" int(11) NOT NULL default '0',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(45) NOT NULL default ' ',
  "AAABMNNA" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWQH","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFUI" (
  "AAAARFXU" varchar(6) NOT NULL default ' ',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABKQPS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAARFXU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFUN" (
  "AAAARVLM" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABFYWO" varchar(30) NOT NULL default ' ',
  "AAAARFXS" varchar(30) NOT NULL default ' ',
  "AAAARFXT" varchar(30) NOT NULL default ' ',
  "AAAARFXU" varchar(6) NOT NULL default ' ',
  "AAAARXBN" varchar(2) NOT NULL default ' ',
  "AAAAVQDD" varchar(6) NOT NULL default '000000',
  "AAABIWGC" varchar(10) NOT NULL default ' ',
  "AAAARFXY" varchar(30) NOT NULL default ' ',
  "AAAARFXZ" varchar(30) NOT NULL default ' ',
  "AAAAVOGI" varchar(20) NOT NULL default ' ',
  "AAAARFYB" varchar(1) NOT NULL default '0',
  "AAAARFYC" varchar(20) NOT NULL default ' ',
  "AAAARFYD" varchar(1) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAARVLM","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFVN" (
  "AAAARVLM" varchar(30) NOT NULL default ' ',
  "AAAARFVW" int(11) NOT NULL default '0',
  "AAAARFXS" varchar(30) NOT NULL default ' ',
  "AAAARFXT" varchar(30) NOT NULL default ' ',
  "AAAARFVS" int(11) NOT NULL default '0',
  "AAAARPBZ" varchar(1) NOT NULL default ' ',
  "AAAARFVU" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAAARVLM","AAAARFVW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFVX" (
  "AAAARVLM" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABIHMA" varchar(3) NOT NULL default ' ',
  "AAAARGBN" varchar(1) NOT NULL default ' ',
  "AAAARFYU" varchar(30) NOT NULL default ' ',
  "AAAARFYV" varchar(1) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAAARVLM","AAABIWFD","AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFWJ" (
  "AAAARVLM" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(45) NOT NULL default ' ',
  "AAABMNNA" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAAARVLM","AAABIWFD","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFWV" (
  "AAAARVLM" varchar(30) NOT NULL default ' ',
  "AAABBKFD" varchar(5) NOT NULL default '00000',
  "AAAARFWY" varchar(1) NOT NULL default ' ',
  "AAAARPBZ" varchar(1) NOT NULL default ' ',
  "AAAARFXA" int(11) NOT NULL default '0',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAAARYQT" varchar(8) NOT NULL default '00000000',
  "AAAARYQU" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAARVLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFXF" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARVLM" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAARVLM","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFXO" (
  "AAAARVLM" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(5) NOT NULL default '00000',
  "AAABFYWO" varchar(30) NOT NULL default ' ',
  "AAAARFXS" varchar(30) NOT NULL default ' ',
  "AAAARFXT" varchar(30) NOT NULL default ' ',
  "AAAARFXU" varchar(6) NOT NULL default ' ',
  "AAAARXBN" varchar(2) NOT NULL default ' ',
  "AAAAVQDD" varchar(6) NOT NULL default '000000',
  "AAABIWGC" varchar(10) NOT NULL default ' ',
  "AAAARFXY" varchar(30) NOT NULL default ' ',
  "AAAARFXZ" varchar(30) NOT NULL default ' ',
  "AAAAVOGI" varchar(20) NOT NULL default ' ',
  "AAAARFYB" varchar(1) NOT NULL default '0',
  "AAAARFYC" varchar(20) NOT NULL default ' ',
  "AAAARFYD" varchar(1) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAARVLM","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFYO" (
  "AAAARVLM" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(5) NOT NULL default '00000',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABIHMA" varchar(3) NOT NULL default ' ',
  "AAAARGBN" varchar(1) NOT NULL default ' ',
  "AAAARFYU" varchar(30) NOT NULL default ' ',
  "AAAARFYV" varchar(1) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAAARVLM","AAABLTXI","AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFZA" (
  "AAAARVLM" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(5) NOT NULL default '00000',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(45) NOT NULL default ' ',
  "AAABMNNA" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAAARVLM","AAABLTXI","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFZM" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARVLM" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(5) NOT NULL default '00000',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAARVLM","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARFZV" (
  "AAABIWQH" int(11) NOT NULL default '0',
  "AAAARGBB" varchar(6) NOT NULL default '000000',
  "AAABFOBJ" int(11) NOT NULL default '0',
  "AAAARGBE" int(11) NOT NULL default '0',
  "AAAARVLR" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAAARGAC" decimal(15,0) NOT NULL default '0',
  "AAAARGAD" decimal(15,0) NOT NULL default '0',
  "AAAAVQDA" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABIWQH","AAAARGBB","AAABFOBJ","AAAARGBE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGAJ" (
  "AAABIWQH" int(11) NOT NULL default '0',
  "AAABIHLK" int(11) NOT NULL default '0',
  "AAAARGBB" varchar(6) NOT NULL default '000000',
  "AAABFOBJ" int(11) NOT NULL default '0',
  "AAAARGBD" varchar(1) NOT NULL default ' ',
  "AAAARGBE" int(11) NOT NULL default '0',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  "AAABLDEN" decimal(15,0) NOT NULL default '0',
  "AAAARTLU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWQH","AAABIHLK","AAAARGBB","AAABFOBJ","AAAARGBD","AAAARGBE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGBF" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGBI" (
  "AAAARVLM" varchar(30) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABIRTO" varchar(1) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAAARGBN" varchar(1) NOT NULL default ' ',
  "AAABCXXG" varchar(30) NOT NULL default ' ',
  "AAABMKXX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARVLM","AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGBS" (
  "AAABJSZC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABJSZC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGBV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGSIS" varchar(8) NOT NULL default ' ',
  "AAABGSIT" varchar(4) NOT NULL default ' ',
  "AAABGSIU" varchar(8) NOT NULL default ' ',
  "AAABGSIV" varchar(4) NOT NULL default ' ',
  "AAABGSHN" varchar(24) NOT NULL default ' ',
  "AAABHCQK" varchar(32) NOT NULL default ' ',
  "AAABGSHP" varchar(24) NOT NULL default '000000000000000000000000',
  "AAABGSGE" varchar(24) NOT NULL default ' ',
  "AAABGSHG" varchar(1) NOT NULL default ' ',
  "AAABGSHX" varchar(8) NOT NULL default ' ',
  "AAABGSGH" varchar(24) NOT NULL default '000000000000000000000000',
  "AAABGSGI" varchar(12) NOT NULL default ' ',
  "AAABGSGJ" varchar(30) NOT NULL default ' ',
  "AAABGSGK" varchar(8) NOT NULL default '00000000',
  "AAABGSGL" varchar(6) NOT NULL default '000000',
  "AAABGSGM" varchar(8) NOT NULL default ' ',
  "AAABGSGN" varchar(1) NOT NULL default ' ',
  "AAABGSGO" varchar(73) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGSIS","AAABGSIT","AAABGSIU","AAABGSIV","AAABGSHN","AAABHCQK","AAABGSHP"),
  KEY "AAAARGCX" ("AAABMRIL","AAABGSHN","AAABHCQK","AAABGSHP"),
  KEY "AAAARGDC" ("AAABMRIL","AAABGSHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGDF" (
  "AAAARGDI" varchar(30) NOT NULL default ' ',
  "AAABMLPO" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAARGDI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGDJ" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAABJVTD" int(11) NOT NULL default '0',
  "AAAARLBA" varchar(8) NOT NULL default '00000000',
  "AAAARLBB" varchar(6) NOT NULL default '000000',
  "AAAARLBC" varchar(12) NOT NULL default ' ',
  "AAAARLBD" varchar(30) NOT NULL default ' ',
  "AAAARLBE" varchar(30) NOT NULL default ' ',
  "AAAARLBF" int(11) NOT NULL default '0',
  "AAAARLBG" int(11) NOT NULL default '0',
  "AAABJHGJ" varchar(1) NOT NULL default ' ',
  "AAABJHGK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHKQ","AAABMNMH","AAABJVTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGDZ" (
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABLTIZ" varchar(6) NOT NULL default '000000',
  "AAABLUFA" varchar(30) NOT NULL default ' ',
  "AAABHTTC" varchar(30) NOT NULL default ' ',
  "AAABIWQH" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRFQ","AAABLTIZ","AAABLUFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGEI" (
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  "AAABHEIS" varchar(3) NOT NULL default ' ',
  "AAABMLPO" int(11) NOT NULL default '0',
  "AAABLEPD" int(11) NOT NULL default '0',
  "AAABLEPE" int(11) NOT NULL default '0',
  "AAABLEPF" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRFQ","AAABMQIX","AAABHEIS","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGEU" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAARRIM" varchar(30) NOT NULL default ' ',
  "AAAARYEC" varchar(6) NOT NULL default '000000',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAAARRIM","AAAARYEC","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGFF" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHFOL" varchar(4) NOT NULL default ' ',
  "AAABFYWO" varchar(30) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHFOL","AAABFYWO"),
  KEY "AAAARGFN" ("AAABHFOL","AAABFYWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGFQ" (
  "AAAARYGE" varchar(6) NOT NULL default ' ',
  "AAABJBGU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGFU" (
  "AAAARGGC" varchar(32) NOT NULL default ' ',
  "AAABMMRH" varchar(4) NOT NULL default '0000',
  "AAAARKJY" varchar(8) NOT NULL default ' ',
  "AAAARKJZ" varchar(1) NOT NULL default ' ',
  "AAAARKKA" varchar(2) NOT NULL default ' ',
  "AAAARKKB" varchar(64) NOT NULL default ' ',
  "AAAARKKC" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAAARGGC","AAABMMRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGGE" (
  "AAAARGLS" varchar(30) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(10) NOT NULL default '0000000000',
  "AAAARGGI" varchar(64) NOT NULL default ' ',
  "AAAARGGJ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAARGLS","AAABIWNN","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGGN" (
  "AAAARGLS" varchar(30) NOT NULL default ' ',
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABLUNJ" varchar(30) NOT NULL default ' ',
  "AAABLDNU" varchar(4) NOT NULL default ' ',
  "AAABLBEI" varchar(6) NOT NULL default '000000',
  "AAABLUWW" varchar(4) NOT NULL default ' ',
  "AAAARGGV" varchar(6) NOT NULL default '000000',
  "AAABGZYP" varchar(60) NOT NULL default ' ',
  "AAABIHLK" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAARGLS","AAABFTZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGHA" (
  "AAAARGND" varchar(32) NOT NULL default ' ',
  "AAAARGNF" varchar(30) NOT NULL default ' ',
  "AAAARGHD" varchar(30) NOT NULL default ' ',
  "AAAARGHQ" varchar(6) NOT NULL default ' ',
  "AAAARGHF" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAARGND","AAAARGNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGHI" (
  "AAAARGLS" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARGLQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAARGLS","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGHO" (
  "AAAARGHQ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAAARGHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGHR" (
  "AAAARGHU" varchar(70) NOT NULL default ' ',
  "AAABKMVZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARGHU","AAABKMVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGHW" (
  "AAABLUER" varchar(20) NOT NULL default ' ',
  "AAAARGIB" varchar(12) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER","AAAARGIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGIC" (
  "AAAARKQK" varchar(32) NOT NULL default ' ',
  "AAABMRFO" varchar(4) NOT NULL default ' ',
  "AAABFVIF" varchar(3) NOT NULL default '000',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABDSTC" varchar(12) NOT NULL default ' ',
  "AAAARKQF" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAARKQK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGIK" (
  "AAAARKQK" varchar(32) NOT NULL default ' ',
  "AAAARGIW" varchar(4) NOT NULL default '0000',
  "AAABIGNZ" varchar(1) NOT NULL default ' ',
  "AAAAUKJO" varchar(20) NOT NULL default ' ',
  "AAABLNXL" varchar(3) NOT NULL default '000',
  "AAAARGIQ" varchar(50) NOT NULL default ' ',
  "AAAARGIR" varchar(50) NOT NULL default ' ',
  "AAAARGIS" varchar(50) NOT NULL default ' ',
  "AAAARGIT" varchar(50) NOT NULL default ' ',
  "AAAARGIU" varchar(220) NOT NULL default ' ',
  PRIMARY KEY ("AAAARKQK","AAAARGIW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGIX" (
  "AAAARGJC" varchar(42) NOT NULL default ' ',
  "AAABHWCF" varchar(30) NOT NULL default ' ',
  "AAAARZQZ" varchar(40) NOT NULL default ' ',
  "AAAARGJB" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAAARGJC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGJD" (
  "AAAARGKX" varchar(32) NOT NULL default ' ',
  "AAAARUSF" varchar(3) NOT NULL default '000',
  "AAAARGJM" varchar(10) NOT NULL default '0000000000',
  "AAAARGJN" varchar(4) NOT NULL default '0000',
  "AAAARGJO" varchar(4) NOT NULL default '0000',
  "AAAARGKY" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAAARGKX","AAAARUSF","AAAARGJM","AAAARGJN","AAAARGJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGJP" (
  "AAAARGKX" varchar(32) NOT NULL default ' ',
  "AAAARGJZ" varchar(10) NOT NULL default '0000000000',
  "AAAARGKA" varchar(10) NOT NULL default '0000000000',
  "AAABMMJT" varchar(22) NOT NULL default ' ',
  "AAAARGKW" varchar(40) NOT NULL default ' ',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABMKAI" varchar(3) NOT NULL default ' ',
  "AAAATPFH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARGKX","AAAARGJZ","AAAARGKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGKB" (
  "AAAARGKX" varchar(32) NOT NULL default ' ',
  "AAAATYPN" int(11) NOT NULL default '0',
  "AAABMRFO" varchar(10) NOT NULL default ' ',
  "AAAARGKF" int(11) NOT NULL default '0',
  "AAAARGKG" int(11) NOT NULL default '0',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(64) NOT NULL default ' ',
  "AAABMNNA" varchar(64) NOT NULL default ' ',
  "AAABLEAH" varchar(30) NOT NULL default ' ',
  "AAABKWFM" varchar(8) NOT NULL default '00000000',
  "AAABLTLM" varchar(6) NOT NULL default '000000',
  "AAABMKKB" varchar(8) NOT NULL default '00000000',
  "AAABLTLN" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAARGKX","AAAATYPN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGKS" (
  "AAAARGKX" varchar(32) NOT NULL default ' ',
  "AAAARGKY" varchar(10) NOT NULL default '0000000000',
  "AAABMMJT" varchar(64) NOT NULL default ' ',
  "AAAARGKW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAARGKX","AAAARGKY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGKZ" (
  "AAABILOU" varchar(32) NOT NULL default ' ',
  "AAABJECS" varchar(30) NOT NULL default ' ',
  "AAABDDKB" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABILOU","AAABJECS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGLF" (
  "AAAARGLS" varchar(30) NOT NULL default ' ',
  "AAAARGLH" varchar(1) NOT NULL default ' ',
  "AAAARGLI" varchar(10) NOT NULL default '0000000000',
  "AAAARGLJ" varchar(4) NOT NULL default ' ',
  "AAAARGLK" varchar(72) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABDSTC" varchar(12) NOT NULL default ' ',
  "AAABLQHQ" varchar(255) NOT NULL default ' ',
  "AAAARGLO" varchar(10) NOT NULL default '0000000000',
  "AAABLOKS" varchar(1) NOT NULL default ' ',
  "AAAARGLQ" varchar(60) NOT NULL default ' ',
  "AAABLEJW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAARGLS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGLT" (
  "AAAARGMA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAARGMA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGLW" (
  "AAAARGMA" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAARGMA","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGMC" (
  "AAAARGND" varchar(32) NOT NULL default ' ',
  "AAABLILW" varchar(10) NOT NULL default ' ',
  "AAAARGMF" varchar(1) NOT NULL default ' ',
  "AAAARKEO" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAAARGND","AAABLILW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGMJ" (
  "AAAARGNF" varchar(30) NOT NULL default ' ',
  "AAABGBCT" varchar(40) NOT NULL default ' ',
  "AAAARGMM" varchar(8) NOT NULL default '00000000',
  "AAABGPVW" varchar(1) NOT NULL default ' ',
  "AAABKWVJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAARGNF","AAABGBCT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGMR" (
  "AAAARGND" varchar(32) NOT NULL default ' ',
  "AAAARKEI" varchar(32) NOT NULL default ' ',
  "AAAARGMU" int(11) NOT NULL default '0',
  "AAAARGMV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARGND","AAAARKEI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGMY" (
  "AAAARGND" varchar(32) NOT NULL default ' ',
  "AAAARKEI" varchar(32) NOT NULL default ' ',
  "AAAARGNF" varchar(30) NOT NULL default ' ',
  "AAABGBCT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAAARGND","AAAARKEI","AAAARGNF","AAABGBCT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGNH" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMLZS" varchar(3) NOT NULL default '000',
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEN","AAABMLZS","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGNO" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAARGNY" varchar(70) NOT NULL default ' ',
  "AAABHWCF" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAAARGNU" varchar(60) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAAARGNY","AAABHWCF","AAABLTXI","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGOC" (
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAAARNTS" varchar(2) NOT NULL default '00',
  "AAAARNTT" varchar(2) NOT NULL default '00',
  "AAAARNTU" varchar(2) NOT NULL default '00',
  "AAAARNTV" varchar(2) NOT NULL default '00',
  "AAAARNTW" varchar(2) NOT NULL default '00',
  "AAAARNTX" varchar(6) NOT NULL default '000000',
  "AAAARNTY" varchar(4) NOT NULL default '0000',
  "AAAARNTZ" varchar(4) NOT NULL default '0000',
  "AAAARNUA" varchar(3) NOT NULL default '000',
  "AAAARNUB" varchar(2) NOT NULL default '00',
  "AAAARNUC" varchar(4) NOT NULL default '0000',
  "AAAARNUD" varchar(1) NOT NULL default ' ',
  "AAAARNUE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGOS" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAAARYNB","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGPB" (
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAABGSIS" varchar(8) NOT NULL default ' ',
  "AAABGSIT" varchar(4) NOT NULL default ' ',
  "AAABGSIU" varchar(8) NOT NULL default ' ',
  "AAABGSIV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYNB","AAABGSIS","AAABGSIT","AAABGSIU","AAABGSIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGPM" (
  "AAAARNTS" varchar(2) NOT NULL default '00',
  "AAAARYHM" varchar(30) NOT NULL default ' ',
  "AAAARGTE" varchar(30) NOT NULL default ' ',
  "AAAARGSK" varchar(1) NOT NULL default ' ',
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAAARGRX" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAARGSQ" varchar(6) NOT NULL default '000000',
  "AAAARGSR" int(11) NOT NULL default '0',
  "AAAARGSS" varchar(6) NOT NULL default '000000',
  "AAAARGST" int(11) NOT NULL default '0',
  "AAAARGPY" varchar(1) NOT NULL default ' ',
  "AAAARGPZ" decimal(15,0) NOT NULL default '0',
  "AAAARGQA" varchar(4) NOT NULL default '0000',
  "AAAARGQB" varchar(6) NOT NULL default '000000',
  "AAAARGQC" int(11) NOT NULL default '0',
  "AAAARGQD" decimal(15,0) NOT NULL default '0',
  "AAAARGQE" decimal(15,0) NOT NULL default '0',
  "AAAARGQF" decimal(15,0) NOT NULL default '0',
  "AAAARGSX" int(11) NOT NULL default '0',
  "AAAARGQH" decimal(15,0) NOT NULL default '0',
  "AAAARGQI" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAAARNTS","AAAARYHM","AAAARGTE","AAAARGSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGQN" (
  "AAAARGTE" varchar(30) NOT NULL default ' ',
  "AAAARGRX" varchar(30) NOT NULL default ' ',
  "AAAARGQQ" varchar(6) NOT NULL default '000000',
  "AAAARGQR" int(11) NOT NULL default '0',
  "AAAARGQS" varchar(6) NOT NULL default '000000',
  "AAAARGQT" int(11) NOT NULL default '0',
  "AAAARGQU" varchar(6) NOT NULL default '000000',
  "AAAARGQV" int(11) NOT NULL default '0',
  "AAAARGQW" varchar(6) NOT NULL default '000000',
  "AAAARGQX" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAARGTE","AAAARGRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGRA" (
  "AAAARNTS" varchar(2) NOT NULL default '00',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAAARGRE" varchar(40) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  "AAAARGTN" decimal(15,0) NOT NULL default '0',
  "AAAARGRH" varchar(12) NOT NULL default ' ',
  "AAAARGRI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARNTS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGRK" (
  "AAAARNTS" varchar(2) NOT NULL default '00',
  "AAAARYHM" varchar(30) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABGFLE" int(11) NOT NULL default '0',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAAARGRX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARNTS","AAAARYHM","AAABMKFU","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGSC" (
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAAARYHM" varchar(30) NOT NULL default ' ',
  "AAAARNTS" varchar(2) NOT NULL default '00',
  "AAAARGSX" int(11) NOT NULL default '0',
  "AAABGFLE" int(11) NOT NULL default '0',
  "AAABLUSG" decimal(21,7) NOT NULL default '0.0000000',
  "AAAARGTE" varchar(30) NOT NULL default ' ',
  "AAAARGSK" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAARGSM" varchar(3) NOT NULL default ' ',
  "AAAARGSN" int(11) NOT NULL default '0',
  "AAAARGSO" int(11) NOT NULL default '0',
  "AAAARGSP" int(11) NOT NULL default '0',
  "AAAARGSQ" varchar(6) NOT NULL default '000000',
  "AAAARGSR" int(11) NOT NULL default '0',
  "AAAARGSS" varchar(6) NOT NULL default '000000',
  "AAAARGST" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAARYNB","AAAARYHM","AAAARNTS","AAAARGSX","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGSZ" (
  "AAAARGTE" varchar(30) NOT NULL default ' ',
  "AAAARUPY" varchar(30) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAVSOW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARGTE","AAAARUPY","AAABMQDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGTH" (
  "AAAARNTS" varchar(2) NOT NULL default '00',
  "AAAARYHM" varchar(30) NOT NULL default ' ',
  "AAAARGTK" varchar(1) NOT NULL default ' ',
  "AAAARGTL" varchar(1) NOT NULL default ' ',
  "AAAARGTM" varchar(1) NOT NULL default ' ',
  "AAAARGTN" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAARNTS","AAAARYHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGTQ" (
  "AAABMJWN" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGTU" (
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAAARGUT" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARGUJ" varchar(120) NOT NULL default ' ',
  "AAABIMRZ" varchar(2) NOT NULL default ' ',
  "AAABIIDV" varchar(2) NOT NULL default ' ',
  "AAABMJYX" varchar(64) NOT NULL default ' ',
  "AAAARZQM" varchar(3) NOT NULL default '000',
  "AAAARGUD" varchar(3) NOT NULL default '000',
  "AAAARGUE" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFZK","AAAARGUT","AAABIWFD","AAAARGUJ","AAABIMRZ","AAABIIDV","AAABMJYX","AAAARZQM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGUO" (
  "AAAARGUT" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABLHAH" varchar(255) NOT NULL default ' ',
  "AAAARGUS" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAAARGUT","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGUV" (
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAABMRFO" varchar(10) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAAARGVA" decimal(15,0) NOT NULL default '0',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAARGVC" int(11) NOT NULL default '0',
  "AAAARGVD" varchar(32) NOT NULL default ' ',
  "AAAARGVE" varchar(64) NOT NULL default ' ',
  "AAAARGVF" varchar(32) NOT NULL default ' ',
  "AAAARGVG" varchar(64) NOT NULL default ' ',
  "AAAARGVH" varchar(14) NOT NULL default ' ',
  "AAAAURFY" varchar(1) NOT NULL default ' ',
  "AAABFORA" varchar(8) NOT NULL default '00000000',
  "AAABLESZ" varchar(6) NOT NULL default '000000',
  "AAABDEFP" varchar(8) NOT NULL default '00000000',
  "AAABDEFQ" varchar(6) NOT NULL default '000000',
  "AAAARWTL" varchar(32) NOT NULL default ' ',
  "AAABCROU" varchar(8) NOT NULL default ' ',
  "AAABCRSF" varchar(1) NOT NULL default ' ',
  "AAABLESX" varchar(32) NOT NULL default ' ',
  "AAABLESY" varchar(64) NOT NULL default ' ',
  "AAAARWTQ" varchar(1) NOT NULL default ' ',
  "AAABCRXB" varchar(2) NOT NULL default '00',
  "AAABCRXC" varchar(2) NOT NULL default '00',
  "AAABCRXD" varchar(3) NOT NULL default '000',
  "AAABCRXE" varchar(2) NOT NULL default '00',
  "AAABCRXF" varchar(2) NOT NULL default '00',
  "AAABCRXG" varchar(1) NOT NULL default ' ',
  "AAABGHVC" varchar(20) NOT NULL default ' ',
  "AAABHHSJ" varchar(2) NOT NULL default ' ',
  "AAABCRXN" int(11) NOT NULL default '0',
  "AAABCRXR" int(11) NOT NULL default '0',
  "AAAARWUB" varchar(1) NOT NULL default ' ',
  "AAABCRSH" varchar(1) NOT NULL default ' ',
  "AAAARWUD" varchar(2) NOT NULL default '00',
  "AAAARWUE" varchar(2) NOT NULL default '00',
  "AAAARWUF" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLTJH","AAABLTJI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGWK" (
  "AAABIMEG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABIMEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGWN" (
  "AAAARYVG" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABLMLH" varchar(4) NOT NULL default ' ',
  "AAABCNNC" varchar(5) NOT NULL default ' ',
  "AAAARGWS" varchar(5) NOT NULL default ' ',
  "AAABFYDZ" varchar(1) NOT NULL default ' ',
  "AAAARGWU" varchar(1) NOT NULL default ' ',
  "AAABLMKH" varchar(8) NOT NULL default '00000000',
  "AAABIHLA" varchar(1) NOT NULL default ' ',
  "AAAARGWX" varchar(30) NOT NULL default ' ',
  "AAAARGWY" varchar(30) NOT NULL default ' ',
  "AAAARGWZ" varchar(30) NOT NULL default ' ',
  "AAAARGXA" varchar(30) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARGXI" varchar(25) NOT NULL default ' ',
  "AAAARGXJ" varchar(25) NOT NULL default ' ',
  "AAAARGXK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYVG","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGXN" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARYVG" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWTO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAARYVG","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGXV" (
  "AAAARGYZ" varchar(3) NOT NULL default '000',
  "AAAARGZS" varchar(4) NOT NULL default ' ',
  "AAAARGYY" varchar(6) NOT NULL default ' ',
  "AAAARGZA" varchar(5) NOT NULL default ' ',
  "AAABKYSP" decimal(10,2) NOT NULL default '0.00',
  "AAAARGZC" varchar(8) NOT NULL default '00000000',
  "AAAARGZD" decimal(8,0) NOT NULL default '0',
  "AAAARGZE" double NOT NULL default '0',
  "AAAARGYE" decimal(10,2) NOT NULL default '0.00',
  "AAAARGZF" tinyint(3) unsigned NOT NULL default '0',
  "AAAARGZG" smallint(6) NOT NULL default '0',
  "AAAARGZH" int(11) NOT NULL default '0',
  "AAAARGZI" varchar(1) NOT NULL default ' ',
  "AAAARGZJ" varchar(4) NOT NULL default '0000',
  "AAAARGYK" decimal(8,2) NOT NULL default '0.00',
  "AAAARGZK" decimal(2,0) NOT NULL default '0',
  "AAAARGZL" decimal(3,0) NOT NULL default '0',
  "AAAARGYN" varchar(1) NOT NULL default ' ',
  "AAAARGZM" VARBINARY(255),
  "AAAARGZN" varchar(6) NOT NULL default '000000',
  "AAAARGYQ" decimal(3,0) NOT NULL default '0',
  "AAAASMXN" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAAARGYZ","AAAARGZS","AAAARGYY","AAAARGZA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGYW" (
  "AAAARGZS" varchar(4) NOT NULL default ' ',
  "AAAARGYY" varchar(6) NOT NULL default ' ',
  "AAAARGYZ" varchar(3) NOT NULL default '000',
  "AAAARGZA" varchar(5) NOT NULL default ' ',
  "AAABKYSP" decimal(10,2) NOT NULL default '0.00',
  "AAAARGZC" varchar(8) NOT NULL default '00000000',
  "AAAARGZD" decimal(8,2) NOT NULL default '0.00',
  "AAAARGZE" double NOT NULL default '0',
  "AAAARGZF" tinyint(3) unsigned NOT NULL default '0',
  "AAAARGZG" smallint(6) NOT NULL default '0',
  "AAAARGZH" int(11) NOT NULL default '0',
  "AAAARGZI" varchar(1) NOT NULL default ' ',
  "AAAARGZJ" varchar(4) NOT NULL default '0000',
  "AAAARGZK" decimal(2,0) NOT NULL default '0',
  "AAAARGZL" decimal(3,0) NOT NULL default '0',
  "AAAARGZM" VARBINARY(255),
  "AAAARGZN" varchar(6) NOT NULL default '000000',
  "AAAASMXN" varchar(3) NOT NULL default ' ',
  "AAAARGZP" varchar(10) NOT NULL default ' ',
  "AAAARGZQ" longtext,
  "AAAARGZR" longblob,
  PRIMARY KEY ("AAAARGZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARGZT" (
  "AAAAWYIH" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARHAB" varchar(30) NOT NULL default ' ',
  "AAAARHAC" varchar(30) NOT NULL default ' ',
  "AAAARHAD" varchar(30) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAAWYIH","AAABIWFD"),
  KEY "AAAARHAI" ("AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHAK" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAWYIH" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAWYIH","AAABIWFD"),
  KEY "AAAARHAT" ("AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHAV" (
  "AAABIKFN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARZAE" varchar(30) NOT NULL default ' ',
  "AAAARKUA" varchar(30) NOT NULL default ' ',
  "AAAARHBA" varchar(1) NOT NULL default ' ',
  "AAABIJZO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIKFN","AAABIWFD","AAAARZAE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHBF" (
  "AAAARKUA" varchar(30) NOT NULL default ' ',
  "AAABIKFN" varchar(30) NOT NULL default ' ',
  "AAAARZAE" varchar(30) NOT NULL default ' ',
  "AAAARHBJ" varchar(30) NOT NULL default ' ',
  "AAAARHBK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARKUA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHBM" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIKFN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARZAE" varchar(30) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIKFN","AAABIWFD","AAAARZAE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHBX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRZZ" varchar(10) NOT NULL default '0000000000',
  "AAABLMJL" varchar(4) NOT NULL default '0000',
  "AAABKSAB" varchar(1) NOT NULL default ' ',
  "AAABKSAC" varchar(2) NOT NULL default '00',
  "AAABKSQL" varchar(10) NOT NULL default ' ',
  "AAABKSPJ" varchar(10) NOT NULL default ' ',
  "AAABKSQI" varchar(5) NOT NULL default '00000',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABKRZT" varchar(4) NOT NULL default '0000',
  "AAABKRZU" varchar(2) NOT NULL default ' ',
  "AAABKRZV" varchar(3) NOT NULL default ' ',
  "AAABKRZW" decimal(5,1) NOT NULL default '0.0',
  PRIMARY KEY ("AAABMRIL","AAABKRZZ","AAABLMJL","AAABKSAB","AAABKSAC"),
  KEY "AAAARHCS" ("AAABMRIL","AAABKSPJ","AAABKSQI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHCW" (
  "AAABFMTA" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARZAE" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAAARHDB" varchar(3) NOT NULL default ' ',
  "AAAARHDC" varchar(1) NOT NULL default ' ',
  "AAAARHDD" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABFMTA","AAABIWFD","AAAARZAE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHDH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRZZ" varchar(10) NOT NULL default '0000000000',
  "AAABLMJL" varchar(4) NOT NULL default '0000',
  "AAABKSAB" varchar(1) NOT NULL default ' ',
  "AAABKSAC" varchar(2) NOT NULL default '00',
  "AAABKSPJ" varchar(10) NOT NULL default ' ',
  "AAABKSQI" varchar(5) NOT NULL default '00000',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABKRZZ","AAABLMJL","AAABKSAB","AAABKSAC"),
  KEY "AAAARHDW" ("AAABMRIL","AAABKSPJ","AAABKSQI"),
  KEY "AAAARHEA" ("AAABMRIL","AAABMCVP","AAABMCVQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHEE" (
  "AAABLOSZ" varchar(10) NOT NULL default ' ',
  "AAABLECD" varchar(30) NOT NULL default ' ',
  "AAABJECS" varchar(64) NOT NULL default ' ',
  "AAABHDKR" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOSZ","AAABLECD","AAABJECS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHEM" (
  "AAABIKFN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABFMTA" varchar(30) NOT NULL default ' ',
  "AAAARLIJ" varchar(1) NOT NULL default ' ',
  "AAAARLIL" varchar(1) NOT NULL default ' ',
  "AAAARLHR" varchar(60) NOT NULL default ' ',
  "AAAARLHS" varchar(1) NOT NULL default ' ',
  "AAAARLHT" varchar(10) NOT NULL default ' ',
  "AAAARLHU" varchar(1) NOT NULL default ' ',
  "AAABIJZO" varchar(1) NOT NULL default ' ',
  "AAAARHEX" varchar(1) NOT NULL default ' ',
  "AAAARHEY" varchar(60) NOT NULL default ' ',
  "AAAARZAZ" varchar(1) NOT NULL default ' ',
  "AAAARLIN" varchar(1) NOT NULL default ' ',
  "AAAARLIO" varchar(1) NOT NULL default ' ',
  "AAAARLIP" varchar(1) NOT NULL default ' ',
  "AAAARTMO" varchar(1) NOT NULL default ' ',
  "AAAARTMP" varchar(1) NOT NULL default ' ',
  "AAABFNLO" varchar(1) NOT NULL default ' ',
  "AAAARVTY" varchar(1) NOT NULL default ' ',
  "AAAARTMS" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABIKFN","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHFK" (
  "AAABFMTA" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIHRF" varchar(4) NOT NULL default ' ',
  "AAABJTEQ" varchar(6) NOT NULL default '000000',
  "AAABMLMY" varchar(6) NOT NULL default '000000',
  "AAABHVRR" varchar(5) NOT NULL default ' ',
  "AAAARHWH" varchar(1) NOT NULL default ' ',
  "AAABJEDH" varchar(1) NOT NULL default ' ',
  "AAAARHFT" varchar(1) NOT NULL default '0',
  "AAAARHFU" varchar(30) NOT NULL default ' ',
  "AAAAWYIH" varchar(30) NOT NULL default ' ',
  "AAABFYWO" varchar(30) NOT NULL default ' ',
  "AAABLKXV" varchar(10) NOT NULL default ' ',
  "AAAARHWJ" varchar(1) NOT NULL default ' ',
  "AAAARHWQ" varchar(1) NOT NULL default ' ',
  "AAAARKVT" varchar(1) NOT NULL default ' ',
  "AAAARHGB" varchar(1) NOT NULL default ' ',
  "AAAARSLK" varchar(1) NOT NULL default ' ',
  "AAAARHWP" varchar(1) NOT NULL default ' ',
  "AAAARHGE" varchar(1) NOT NULL default ' ',
  "AAAARHGF" varchar(30) NOT NULL default ' ',
  "AAAARHWO" varchar(1) NOT NULL default ' ',
  "AAAARHWN" varchar(1) NOT NULL default ' ',
  "AAAARHGI" varchar(1) NOT NULL default ' ',
  "AAAARHWK" varchar(1) NOT NULL default ' ',
  "AAAARHWL" varchar(1) NOT NULL default ' ',
  "AAAARHWM" varchar(1) NOT NULL default ' ',
  "AAABIHRN" varchar(30) NOT NULL default ' ',
  "AAAARHGN" varchar(25) NOT NULL default ' ',
  "AAAARHWR" varchar(1) NOT NULL default ' ',
  "AAAARHWS" varchar(1) NOT NULL default ' ',
  "AAAARHWT" varchar(1) NOT NULL default ' ',
  "AAAARVMT" varchar(1) NOT NULL default ' ',
  "AAAARHWV" varchar(1) NOT NULL default ' ',
  "AAAARHWW" varchar(1) NOT NULL default ' ',
  "AAAARHGU" varchar(2) NOT NULL default ' ',
  "AAAARHGV" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABFMTA","AAABIWFD"),
  KEY "AAAARHGY" ("AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHHA" (
  "AAABFMTA" varchar(30) NOT NULL default ' ',
  "AAAARKWB" varchar(25) NOT NULL default ' ',
  "AAAARIND" varchar(7) NOT NULL default ' ',
  "AAABETOY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFMTA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHHG" (
  "AAABIWNE" varchar(30) NOT NULL default ' ',
  "AAAARHHI" decimal(21,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABIWNE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHHK" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  "AAAARIMD" varchar(1) NOT NULL default ' ',
  "AAAAWYIH" varchar(30) NOT NULL default ' ',
  "AAAARHHV" varchar(30) NOT NULL default ' ',
  "AAABIWGC" varchar(10) NOT NULL default ' ',
  "AAAARHHX" varchar(1) NOT NULL default ' ',
  "AAAARHHY" varchar(1) NOT NULL default ' ',
  "AAAARHHZ" varchar(1) NOT NULL default ' ',
  "AAAARHIA" varchar(1) NOT NULL default ' ',
  "AAAARHIB" varchar(1) NOT NULL default ' ',
  "AAAARHIC" varchar(1) NOT NULL default ' ',
  "AAAARHID" varchar(1) NOT NULL default ' ',
  "AAAARHIE" varchar(1) NOT NULL default ' ',
  "AAAARHIF" varchar(10) NOT NULL default ' ',
  "AAAARHIG" varchar(1) NOT NULL default ' ',
  "AAAARHIH" varchar(1) NOT NULL default ' ',
  "AAAARHII" varchar(1) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAAARHIK" varchar(1) NOT NULL default ' ',
  "AAAARXGB" varchar(30) NOT NULL default ' ',
  "AAABENCY" varchar(1) NOT NULL default ' ',
  "AAABFNEP" varchar(10) NOT NULL default ' ',
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAAARPJF" varchar(1) NOT NULL default ' ',
  "AAAARXWZ" varchar(1) NOT NULL default ' ',
  "AAAARHIR" varchar(1) NOT NULL default ' ',
  "AAAARHIS" varchar(2) NOT NULL default '00',
  "AAAARHIT" varchar(30) NOT NULL default ' ',
  "AAAARHIU" varchar(10) NOT NULL default ' ',
  "AAAARHIV" varchar(10) NOT NULL default ' ',
  "AAAARHIW" int(11) NOT NULL default '0',
  "AAAARHIX" varchar(2) NOT NULL default ' ',
  "AAAARHIY" varchar(5) NOT NULL default ' ',
  "AAAARHIZ" varchar(2) NOT NULL default ' ',
  "AAAARHJA" varchar(5) NOT NULL default ' ',
  "AAAARTMO" varchar(1) NOT NULL default ' ',
  "AAAARTMP" varchar(1) NOT NULL default ' ',
  "AAAARONQ" varchar(1) NOT NULL default ' ',
  "AAAARONR" varchar(1) NOT NULL default ' ',
  "AAAARONS" varchar(1) NOT NULL default ' ',
  "AAAARHJG" varchar(1) NOT NULL default ' ',
  "AAAARONV" varchar(1) NOT NULL default ' ',
  "AAAARONW" varchar(1) NOT NULL default ' ',
  "AAAARONX" varchar(1) NOT NULL default ' ',
  "AAAARONY" varchar(1) NOT NULL default ' ',
  "AAABIKEM" varchar(3) NOT NULL default '000',
  "AAAAROOA" varchar(1) NOT NULL default ' ',
  "AAAARHJN" varchar(1) NOT NULL default ' ',
  "AAAAROOC" varchar(1) NOT NULL default ' ',
  "AAAARHJP" varchar(2) NOT NULL default ' ',
  "AAAARHJQ" varchar(5) NOT NULL default ' ',
  "AAAARHJR" varchar(2) NOT NULL default ' ',
  "AAAARHJS" varchar(5) NOT NULL default ' ',
  "AAABFNLO" varchar(1) NOT NULL default ' ',
  "AAAARVTY" varchar(1) NOT NULL default ' ',
  "AAAARTMS" int(11) NOT NULL default '0',
  "AAAARHJW" varchar(1) NOT NULL default ' ',
  "AAAARHJX" varchar(3) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABIHKQ","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHKC" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHKQ","AAABIWFD","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHKK" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHKP" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAAARIJB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHKQ","AAABIWFD","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHKX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIHKQ","AAABIWFD"),
  KEY "AAAARHLG" ("AAABIHKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHLI" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABLMYF" varchar(30) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABIHKQ","AAABLMYF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHLP" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABLMYF" varchar(30) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABLGEL" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHKQ","AAABLMYF","AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHLY" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABLMYF" varchar(30) NOT NULL default ' ',
  "AAABFRTL" varchar(1) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAAARHME" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHKQ","AAABLMYF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHMH" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABLMYF" varchar(30) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIHKQ","AAABLMYF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHMQ" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAAARHMS" varchar(25) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABIHKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHMV" (
  "AAAARKXM" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAAARIAC" varchar(1) NOT NULL default ' ',
  "AAABIVVH" varchar(25) NOT NULL default ' ',
  "AAABIWES" int(11) NOT NULL default '0',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  "AAABMMJT" varchar(60) NOT NULL default ' ',
  "AAAARIAG" int(11) NOT NULL default '0',
  "AAABIWLU" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default ' ',
  "AAABLRXO" varchar(8) NOT NULL default '00000000',
  "AAAARHNI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARKXM","AAABIWFD","AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHNM" (
  "AAAARKXM" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAAARHPD" varchar(30) NOT NULL default ' ',
  "AAAARHZJ" varchar(30) NOT NULL default ' ',
  "AAAARHPH" int(11) NOT NULL default '0',
  "AAAARHPI" int(11) NOT NULL default '0',
  "AAAARHPJ" int(11) NOT NULL default '0',
  "AAAARHPK" varchar(1) NOT NULL default ' ',
  "AAAARHPL" varchar(1) NOT NULL default ' ',
  "AAAARHPM" varchar(25) NOT NULL default ' ',
  "AAAARHPN" varchar(25) NOT NULL default ' ',
  "AAAARHPO" varchar(25) NOT NULL default ' ',
  "AAAARHPP" int(11) NOT NULL default '0',
  "AAAARHPQ" int(11) NOT NULL default '0',
  "AAAARHPR" int(11) NOT NULL default '0',
  "AAAARHPS" decimal(15,0) NOT NULL default '0',
  "AAAARHPT" decimal(15,0) NOT NULL default '0',
  "AAAARHPU" varchar(12) NOT NULL default ' ',
  "AAAARHPX" int(11) NOT NULL default '0',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAAARYKT" varchar(1) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  "AAAARHOQ" varchar(1) NOT NULL default ' ',
  "AAAARHOR" varchar(25) NOT NULL default ' ',
  "AAABDTGM" varchar(1) NOT NULL default ' ',
  "AAAARHOT" varchar(8) NOT NULL default '00000000',
  "AAABHWXF" decimal(15,0) NOT NULL default '0',
  "AAAARHOV" varchar(1) NOT NULL default ' ',
  "AAAARHOW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARKXM","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHOZ" (
  "AAAARKXM" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAAARHPD" varchar(30) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  "AAAARHZJ" varchar(30) NOT NULL default ' ',
  "AAAARHPH" int(11) NOT NULL default '0',
  "AAAARHPI" int(11) NOT NULL default '0',
  "AAAARHPJ" int(11) NOT NULL default '0',
  "AAAARHPK" varchar(1) NOT NULL default ' ',
  "AAAARHPL" varchar(1) NOT NULL default ' ',
  "AAAARHPM" varchar(25) NOT NULL default ' ',
  "AAAARHPN" varchar(25) NOT NULL default ' ',
  "AAAARHPO" varchar(25) NOT NULL default ' ',
  "AAAARHPP" int(11) NOT NULL default '0',
  "AAAARHPQ" int(11) NOT NULL default '0',
  "AAAARHPR" int(11) NOT NULL default '0',
  "AAAARHPS" decimal(15,0) NOT NULL default '0',
  "AAAARHPT" decimal(15,0) NOT NULL default '0',
  "AAAARHPU" varchar(12) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAAARHPX" int(11) NOT NULL default '0',
  "AAAARYKT" varchar(1) NOT NULL default ' ',
  "AAABMKGE" varchar(1) NOT NULL default ' ',
  "AAABLTXN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARKXM","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHQD" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAAARHQG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHKQ","AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHQJ" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAAARKXM" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHKQ","AAAARKXM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHQO" (
  "AAAARHRP" int(11) NOT NULL default '0',
  "AAAARHQQ" varchar(1) NOT NULL default ' ',
  "AAAARHQR" decimal(15,0) NOT NULL default '0',
  "AAAARHQS" decimal(15,0) NOT NULL default '0',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABDKYI" varchar(1) NOT NULL default ' ',
  "AAAARHQV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARHRP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHQX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARKXM" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAARKXM","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHRG" (
  "AAAARHRP" int(11) NOT NULL default '0',
  "AAABFMTA" varchar(30) NOT NULL default ' ',
  "AAABIVVH" varchar(25) NOT NULL default ' ',
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAAARHRL" varchar(1) NOT NULL default ' ',
  "AAAARVMT" varchar(1) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAARHRP","AAABFMTA","AAABIVVH","AAABIHKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHRT" (
  "AAAARKXM" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAAARKXM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHRW" (
  "AAABIVVH" varchar(25) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIVVH","AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHSB" (
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABKGBL" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(5) NOT NULL default ' ',
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAAARZSO" varchar(25) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARHSK" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHSN" (
  "AAABLSTE" varchar(25) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHST" (
  "AAABLSTE" varchar(25) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHSZ" (
  "AAAAWDEM" varchar(30) NOT NULL default ' ',
  "AAABIHMA" varchar(3) NOT NULL default ' ',
  "AAAAWYIH" varchar(30) NOT NULL default ' ',
  "AAAARLCM" varchar(30) NOT NULL default ' ',
  "AAAARHUM" varchar(10) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAAWDEM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHTI" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAAWDEM" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABIHMA" varchar(3) NOT NULL default ' ',
  "AAAARHTO" varchar(1) NOT NULL default ' ',
  "AAAARHTP" varchar(1) NOT NULL default ' ',
  "AAAARLCM" varchar(30) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABIHKQ","AAABIWFD","AAAAWDEM"),
  KEY "AAAARHTW" ("AAAAWDEM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHTY" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAAWDEM" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHKQ","AAABIWFD","AAAAWDEM","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHUI" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAAAWDEM" varchar(30) NOT NULL default ' ',
  "AAAARIND" varchar(7) NOT NULL default ' ',
  "AAAARHUM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHKQ","AAAAWDEM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHUP" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAAWDEM" varchar(30) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIHKQ","AAABIWFD","AAAAWDEM"),
  KEY "AAAARHVA" ("AAABIHKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHVC" (
  "AAABLKHY" int(11) NOT NULL default '0',
  "AAABLKHX" int(11) NOT NULL default '0',
  "AAABIWAM" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLKHY","AAABLKHX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHVI" (
  "AAAARHVU" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARHVL" varchar(1) NOT NULL default ' ',
  "AAAARYPF" varchar(3) NOT NULL default ' ',
  "AAAARYPG" varchar(3) NOT NULL default ' ',
  "AAAARYPH" varchar(30) NOT NULL default ' ',
  "AAAARKVQ" varchar(1) NOT NULL default ' ',
  "AAAARKVR" varchar(1) NOT NULL default ' ',
  "AAAARKVS" varchar(1) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAARHVU","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHVW" (
  "AAABFMTA" varchar(30) NOT NULL default ' ',
  "AAAARHVY" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABFMTA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHWA" (
  "AAAARHXD" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIHRF" varchar(4) NOT NULL default ' ',
  "AAABJTEQ" varchar(6) NOT NULL default '000000',
  "AAABMLMY" varchar(6) NOT NULL default '000000',
  "AAABHVRR" varchar(5) NOT NULL default ' ',
  "AAAARHWH" varchar(1) NOT NULL default ' ',
  "AAABJEDH" varchar(1) NOT NULL default ' ',
  "AAAARHWJ" varchar(1) NOT NULL default ' ',
  "AAAARHWK" varchar(1) NOT NULL default ' ',
  "AAAARHWL" varchar(1) NOT NULL default ' ',
  "AAAARHWM" varchar(1) NOT NULL default ' ',
  "AAAARHWN" varchar(1) NOT NULL default ' ',
  "AAAARHWO" varchar(1) NOT NULL default ' ',
  "AAAARHWP" varchar(1) NOT NULL default ' ',
  "AAAARHWQ" varchar(1) NOT NULL default ' ',
  "AAAARHWR" varchar(1) NOT NULL default ' ',
  "AAAARHWS" varchar(1) NOT NULL default ' ',
  "AAAARHWT" varchar(1) NOT NULL default ' ',
  "AAAARVMT" varchar(1) NOT NULL default ' ',
  "AAAARHWV" varchar(1) NOT NULL default ' ',
  "AAAARHWW" varchar(1) NOT NULL default ' ',
  "AAAARZAZ" varchar(1) NOT NULL default ' ',
  "AAAARLIN" varchar(1) NOT NULL default ' ',
  "AAAARLIO" varchar(1) NOT NULL default ' ',
  "AAAARLIP" varchar(1) NOT NULL default ' ',
  "AAAARKWB" varchar(25) NOT NULL default ' ',
  "AAAARIND" varchar(7) NOT NULL default ' ',
  PRIMARY KEY ("AAAARHXD","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHXF" (
  "AAAARICL" varchar(25) NOT NULL default ' ',
  "AAAARHXH" varchar(25) NOT NULL default ' ',
  "AAAARHXI" varchar(25) NOT NULL default ' ',
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABLSTE" varchar(4) NOT NULL default '0000',
  "AAAARWLQ" varchar(25) NOT NULL default ' ',
  "AAABAKZI" varchar(40) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAARHXO" varchar(30) NOT NULL default ' ',
  "AAAARHXP" varchar(1) NOT NULL default ' ',
  "AAAARHXQ" varchar(3) NOT NULL default '000',
  "AAAARHXR" varchar(3) NOT NULL default '000',
  "AAAARHXS" decimal(15,0) NOT NULL default '0',
  "AAAARHXT" decimal(15,0) NOT NULL default '0',
  "AAAARHXU" decimal(15,0) NOT NULL default '0',
  "AAAARHXV" decimal(15,0) NOT NULL default '0',
  "AAAARHXW" decimal(15,0) NOT NULL default '0',
  "AAAARHXX" decimal(15,0) NOT NULL default '0',
  "AAAARHXY" decimal(15,0) NOT NULL default '0',
  "AAAARHXZ" decimal(15,0) NOT NULL default '0',
  "AAAARHYA" decimal(21,6) NOT NULL default '0.000000',
  "AAAARHYB" decimal(21,6) NOT NULL default '0.000000',
  "AAAARHYC" decimal(21,6) NOT NULL default '0.000000',
  "AAAARHYD" decimal(21,6) NOT NULL default '0.000000',
  "AAAARHYE" decimal(21,6) NOT NULL default '0.000000',
  "AAAARHYF" decimal(21,6) NOT NULL default '0.000000',
  "AAAARHYG" decimal(21,6) NOT NULL default '0.000000',
  "AAAARHYH" decimal(21,6) NOT NULL default '0.000000',
  "AAAARHYI" decimal(21,6) NOT NULL default '0.000000',
  "AAAARHYJ" decimal(21,6) NOT NULL default '0.000000',
  "AAAARHYK" decimal(21,6) NOT NULL default '0.000000',
  "AAAARHYL" decimal(21,6) NOT NULL default '0.000000',
  "AAAARHYM" decimal(21,6) NOT NULL default '0.000000',
  "AAAARHYN" decimal(21,6) NOT NULL default '0.000000',
  "AAAARHYO" decimal(21,6) NOT NULL default '0.000000',
  "AAAARHYP" decimal(21,6) NOT NULL default '0.000000',
  "AAAARHYQ" decimal(15,0) NOT NULL default '0',
  "AAAARHYR" decimal(15,0) NOT NULL default '0',
  "AAAARHYS" decimal(21,6) NOT NULL default '0.000000',
  "AAAARHYT" decimal(21,6) NOT NULL default '0.000000',
  "AAAARHYU" decimal(21,6) NOT NULL default '0.000000',
  "AAAARHYV" decimal(21,6) NOT NULL default '0.000000',
  "AAAARHYW" decimal(21,6) NOT NULL default '0.000000',
  "AAAARHYX" decimal(21,6) NOT NULL default '0.000000',
  "AAAARHYY" decimal(21,6) NOT NULL default '0.000000',
  "AAABLRTW" decimal(21,7) NOT NULL default '0.0000000',
  "AAAARHZA" decimal(21,6) NOT NULL default '0.000000',
  "AAAARHZB" decimal(21,6) NOT NULL default '0.000000',
  "AAAARHZC" decimal(21,6) NOT NULL default '0.000000',
  "AAAARHZD" decimal(21,6) NOT NULL default '0.000000',
  "AAAARHZE" decimal(21,6) NOT NULL default '0.000000',
  "AAAARHZF" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAARICL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHZH" (
  "AAAARICL" varchar(25) NOT NULL default ' ',
  "AAAARHZJ" varchar(30) NOT NULL default ' ',
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAAARHZL" varchar(30) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABALUY" varchar(1) NOT NULL default ' ',
  "AAAARHZO" decimal(21,6) NOT NULL default '0.000000',
  "AAAARHZP" decimal(21,6) NOT NULL default '0.000000',
  "AAAARHZQ" decimal(21,6) NOT NULL default '0.000000',
  "AAAARHZR" decimal(21,6) NOT NULL default '0.000000',
  "AAAARHZS" decimal(21,6) NOT NULL default '0.000000',
  "AAABLRTW" decimal(21,7) NOT NULL default '0.0000000',
  "AAAARHZU" decimal(15,0) NOT NULL default '0',
  "AAAARHZV" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAARICL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARHZX" (
  "AAAARICL" varchar(25) NOT NULL default ' ',
  "AAAARIAI" varchar(4) NOT NULL default '0000',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAAARIAB" varchar(30) NOT NULL default ' ',
  "AAAARIAC" varchar(1) NOT NULL default ' ',
  "AAABIWES" int(11) NOT NULL default '0',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  "AAABMMJT" varchar(60) NOT NULL default ' ',
  "AAAARIAG" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAARICL","AAAARIAI","AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIAK" (
  "AAAARICL" varchar(25) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABHFOL" varchar(4) NOT NULL default ' ',
  "AAAARIAO" decimal(21,6) NOT NULL default '0.000000',
  "AAAARIAP" int(11) NOT NULL default '0',
  "AAABLRTW" decimal(21,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAAARICL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIAS" (
  "AAAARICL" varchar(25) NOT NULL default ' ',
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLRTW" decimal(21,7) NOT NULL default '0.0000000',
  "AAAARIAX" decimal(15,0) NOT NULL default '0',
  "AAAARIAY" decimal(15,0) NOT NULL default '0',
  "AAAARIAZ" decimal(15,0) NOT NULL default '0',
  "AAAARIBA" decimal(15,0) NOT NULL default '0',
  "AAAARIBB" decimal(15,0) NOT NULL default '0',
  "AAAARIBC" decimal(15,0) NOT NULL default '0',
  "AAAARIBD" decimal(21,6) NOT NULL default '0.000000',
  "AAAARIBE" decimal(21,6) NOT NULL default '0.000000',
  PRIMARY KEY ("AAAARICL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIBG" (
  "AAAARICL" varchar(25) NOT NULL default ' ',
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAARKZT" varchar(1) NOT NULL default ' ',
  "AAABLRTW" decimal(21,7) NOT NULL default '0.0000000',
  "AAAARIBM" decimal(15,0) NOT NULL default '0',
  "AAAARIBN" decimal(15,0) NOT NULL default '0',
  "AAAARIBO" decimal(21,6) NOT NULL default '0.000000',
  "AAAARIBP" decimal(21,6) NOT NULL default '0.000000',
  "AAAARIBQ" decimal(21,6) NOT NULL default '0.000000',
  "AAAARIBR" decimal(21,6) NOT NULL default '0.000000',
  "AAAARIBS" decimal(21,6) NOT NULL default '0.000000',
  "AAAARIBT" decimal(21,6) NOT NULL default '0.000000',
  "AAAARIBU" decimal(21,6) NOT NULL default '0.000000',
  "AAAARIBV" decimal(21,6) NOT NULL default '0.000000',
  PRIMARY KEY ("AAAARICL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIBX" (
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABHISZ" decimal(21,7) NOT NULL default '0.0000000',
  "AAABLKLU" decimal(21,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMQDU","AAABIANB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARICE" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAARICL" varchar(25) NOT NULL default ' ',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAABMPMH" varchar(32) NOT NULL default ' ',
  "AAABLSTE" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMDEN","AAAARICL","AAABMLPO","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARICO" (
  "AAAARIDB" int(11) NOT NULL default '0',
  "AAAARIDC" varchar(30) NOT NULL default ' ',
  "AAABLUFA" varchar(3) NOT NULL default '000',
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLBET" varchar(1) NOT NULL default ' ',
  "AAAARICX" varchar(1) NOT NULL default ' ',
  "AAABICGI" decimal(15,0) NOT NULL default '0',
  "AAABLTIZ" decimal(21,6) NOT NULL default '0.000000',
  "AAABLRTW" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAARIDB","AAAARIDC","AAABLUFA","AAABIHKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIDF" (
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAABLECD" varchar(30) NOT NULL default ' ',
  "AAAARIDJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIDL" (
  "AAAARLCM" varchar(30) NOT NULL default ' ',
  "AAABMMJT" int(11) NOT NULL default '0',
  "AAABMKFH" varchar(30) NOT NULL default ' ',
  "AAAARIDP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARLCM","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIDS" (
  "AAAAVPZC" varchar(2) NOT NULL default ' ',
  "AAABLUSV" int(11) NOT NULL default '0',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAABLUHK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAVPZC","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIDZ" (
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAAARNWM" varchar(30) NOT NULL default ' ',
  "AAABGFLE" int(11) NOT NULL default '0',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(45) NOT NULL default ' ',
  "AAABMNNA" varchar(45) NOT NULL default ' ',
  "AAABMNLA" varchar(1) NOT NULL default ' ',
  "AAABHTVO" varchar(1) NOT NULL default ' ',
  "AAAARNVY" varchar(1) NOT NULL default ' ',
  "AAABLPQZ" varchar(1) NOT NULL default ' ',
  "AAAARNWA" varchar(1) NOT NULL default ' ',
  "AAAARNWB" varchar(1) NOT NULL default ' ',
  "AAAARNWC" varchar(1) NOT NULL default ' ',
  "AAAARNUZ" varchar(1) NOT NULL default ' ',
  "AAAARNWD" varchar(1) NOT NULL default ' ',
  "AAABMPMV" varchar(1) NOT NULL default ' ',
  "AAAARNWF" varchar(2) NOT NULL default '00',
  "AAAARNWG" varchar(1) NOT NULL default ' ',
  "AAABLZMT" varchar(1) NOT NULL default '0',
  "AAAARNWI" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(80) NOT NULL default ' ',
  "AAAARNWK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYNB","AAAARNWM","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIFC" (
  "AAAARVLG" varchar(30) NOT NULL default ' ',
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABMRFO" varchar(10) NOT NULL default ' ',
  "AAABLDBD" int(11) NOT NULL default '0',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAABFORA" varchar(8) NOT NULL default '00000000',
  "AAABLESZ" varchar(6) NOT NULL default '000000',
  "AAABDEFP" varchar(8) NOT NULL default '00000000',
  "AAABDEFQ" varchar(6) NOT NULL default '000000',
  "AAAARWTL" varchar(32) NOT NULL default ' ',
  "AAABCROU" varchar(8) NOT NULL default ' ',
  "AAABCRSF" varchar(1) NOT NULL default ' ',
  "AAABLESX" varchar(32) NOT NULL default ' ',
  "AAABLESY" varchar(64) NOT NULL default ' ',
  "AAAARWTQ" varchar(1) NOT NULL default ' ',
  "AAABCRXB" varchar(2) NOT NULL default '00',
  "AAABCRXC" varchar(2) NOT NULL default '00',
  "AAABCRXD" varchar(3) NOT NULL default '000',
  "AAABCRXE" varchar(2) NOT NULL default '00',
  "AAABCRXF" varchar(2) NOT NULL default '00',
  "AAABCRXG" varchar(1) NOT NULL default ' ',
  "AAABGHVC" varchar(20) NOT NULL default ' ',
  "AAABHHSJ" varchar(2) NOT NULL default ' ',
  "AAABCRXN" int(11) NOT NULL default '0',
  "AAABCRXR" int(11) NOT NULL default '0',
  "AAAARWUB" varchar(1) NOT NULL default ' ',
  "AAABCRSH" varchar(1) NOT NULL default ' ',
  "AAAARWUD" varchar(2) NOT NULL default '00',
  "AAAARWUE" varchar(2) NOT NULL default '00',
  "AAAARWUF" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAARVLG","AAAARYNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIGM" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAAARIHC" varchar(32) NOT NULL default ' ',
  "AAABMLPV" varchar(32) NOT NULL default ' ',
  "AAABMMJT" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIGS" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAAARIGU" varchar(96) NOT NULL default ' ',
  "AAAARIHC" varchar(32) NOT NULL default ' ',
  "AAABMNRG" tinyint(3) unsigned NOT NULL default '0',
  "AAAARIGX" varchar(96) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIGZ" (
  "AAAARIHC" varchar(32) NOT NULL default ' ',
  "AAAARIHB" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAAARIHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIHD" (
  "AAAARIHJ" varchar(20) NOT NULL default ' ',
  "AAABMLPV" varchar(132) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABIHMA" varchar(3) NOT NULL default ' ',
  "AAAARIHI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARIHJ","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIHL" (
  "AAABFMTA" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFMTA","AAABIWFD","AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIHS" (
  "AAABIHRQ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAAARISC" varchar(30) NOT NULL default ' ',
  "AAAARISD" varchar(30) NOT NULL default ' ',
  "AAAARIHY" varchar(8) NOT NULL default ' ',
  "AAAARIHZ" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHRQ","AAABIWFD","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIID" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIKFN" varchar(30) NOT NULL default ' ',
  "AAAARLHR" varchar(60) NOT NULL default ' ',
  "AAAARZAZ" varchar(1) NOT NULL default ' ',
  "AAAARLIN" varchar(1) NOT NULL default ' ',
  "AAAARLIO" varchar(1) NOT NULL default ' ',
  "AAAARLIP" varchar(1) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAAARKVT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHKQ","AAABIWFD","AAABIKFN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIIS" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAAARIJB" varchar(30) NOT NULL default ' ',
  "AAAARIIX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHKQ","AAABIWFD","AAABIWNN","AAAARIJB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIJC" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHKQ","AAABIWFD","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIJK" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIHRQ" varchar(30) NOT NULL default ' ',
  "AAAARKVQ" varchar(1) NOT NULL default ' ',
  "AAAARKVR" varchar(1) NOT NULL default ' ',
  "AAAARKVS" varchar(1) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABIHKQ","AAABIWFD","AAABIHRQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIJW" (
  "AAABFZAB" int(11) NOT NULL default '0',
  "AAAARIKD" varchar(10) NOT NULL default ' ',
  "AAABCQKC" varchar(1) NOT NULL default ' ',
  "AAABMMRH" varchar(72) NOT NULL default ' ',
  "AAAARIKB" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZAB","AAAARIKD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIKE" (
  "AAABIWNE" varchar(30) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABIWNE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIKJ" (
  "AAAARILS" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAABIHMA" varchar(3) NOT NULL default ' ',
  "AAAAWYIH" varchar(30) NOT NULL default ' ',
  "AAABIWGC" varchar(10) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAARILS","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIKY" (
  "AAAARILS" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARILS","AAABIWFD","AAABIWNN"),
  KEY "AAAARILF" ("AAABIWFD","AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARILI" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARILS" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAARILS","AAABIWFD"),
  KEY "AAAARILR" ("AAAARILS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARILT" (
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIHMA" varchar(3) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  "AAAARIMD" varchar(1) NOT NULL default ' ',
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  "AAAARIMF" varchar(1) NOT NULL default ' ',
  "AAABIWGC" varchar(10) NOT NULL default ' ',
  "AAABIJZO" varchar(1) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABIWNN","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIMM" (
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAAAROPH" varchar(30) NOT NULL default ' ',
  "AAAARIMR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWNN","AAABIWFD","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIMV" (
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABIHMA" varchar(3) NOT NULL default ' ',
  "AAAARIMY" varchar(30) NOT NULL default ' ',
  "AAABDKEL" varchar(30) NOT NULL default ' ',
  "AAABFTZK" varchar(30) NOT NULL default ' ',
  "AAABFMTC" varchar(30) NOT NULL default ' ',
  "AAABHVRR" varchar(5) NOT NULL default ' ',
  "AAAARIND" varchar(7) NOT NULL default ' ',
  "AAAARINE" varchar(30) NOT NULL default ' ',
  "AAAARINF" varchar(30) NOT NULL default ' ',
  "AAAARING" varchar(30) NOT NULL default ' ',
  "AAAARINH" varchar(30) NOT NULL default ' ',
  "AAAARINI" varchar(30) NOT NULL default ' ',
  "AAAARINJ" varchar(1) NOT NULL default ' ',
  "AAABMLPN" varchar(30) NOT NULL default ' ',
  "AAAARINL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARINN" (
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARINU" varchar(30) NOT NULL default ' ',
  "AAAARINR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWNN","AAABIWFD","AAAARINU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARINV" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIWNN","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIOE" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARION" (
  "AAABIHRQ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIHRE" varchar(3) NOT NULL default ' ',
  "AAAARYPE" varchar(1) NOT NULL default ' ',
  "AAABIJZA" varchar(5) NOT NULL default ' ',
  "AAABIJYZ" varchar(3) NOT NULL default ' ',
  "AAAARYPF" varchar(3) NOT NULL default ' ',
  "AAAARYPG" varchar(3) NOT NULL default ' ',
  "AAAARYPH" varchar(30) NOT NULL default ' ',
  "AAAARKVQ" varchar(1) NOT NULL default ' ',
  "AAAARKVR" varchar(1) NOT NULL default ' ',
  "AAAARKVS" varchar(1) NOT NULL default ' ',
  "AAABIHRN" varchar(30) NOT NULL default ' ',
  "AAABFMTC" varchar(30) NOT NULL default ' ',
  "AAAARVTW" varchar(1) NOT NULL default ' ',
  "AAABIHRF" varchar(4) NOT NULL default ' ',
  "AAAARLIP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHRQ","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIPH" (
  "AAABLGCM" varchar(40) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABLEKV" varchar(30) NOT NULL default ' ',
  "AAABLEKR" varchar(3) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAAARYQT" varchar(8) NOT NULL default '00000000',
  "AAAARYQU" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLGCM","AAABMQDU","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIPT" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABIFGN" int(11) NOT NULL default '0',
  "AAAARIQK" varchar(60) NOT NULL default ' ',
  "AAAARIQL" varchar(6) NOT NULL default '000000',
  "AAAARIPY" decimal(20,8) NOT NULL default '0.00000000',
  "AAAARIPZ" decimal(20,8) NOT NULL default '0.00000000',
  "AAAARIQA" double NOT NULL default '0',
  "AAAARIQB" double NOT NULL default '0',
  "AAAARIQC" int(11) NOT NULL default '0',
  "AAAARIQD" decimal(20,8) NOT NULL default '0.00000000',
  "AAAARIQE" decimal(20,8) NOT NULL default '0.00000000',
  "AAAARIQF" decimal(6,3) NOT NULL default '0.000',
  "AAAARIQG" decimal(6,3) NOT NULL default '0.000',
  "AAAARIQH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFJ","AAABIFGN","AAAARIQK","AAAARIQL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIQM" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAAARIQR" varchar(30) NOT NULL default ' ',
  "AAAARIQP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFJ","AAAARIQR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIQS" (
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABJQWM" varchar(1) NOT NULL default ' ',
  "AAABFMOY" varchar(1) NOT NULL default ' ',
  "AAAARIRH" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLETY","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIRA" (
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABLRXO" varchar(8) NOT NULL default '00000000',
  "AAABLRXN" varchar(8) NOT NULL default '00000000',
  "AAABJQWM" varchar(1) NOT NULL default ' ',
  "AAABFMOY" varchar(1) NOT NULL default ' ',
  "AAAARIRH" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLETY","AAABIWFD","AAABLRXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIRL" (
  "AAABJNHG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABJNHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIRO" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFJ","AAABIWFD","AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIRV" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABLEEW" varchar(30) NOT NULL default ' ',
  "AAABFZAB" varchar(60) NOT NULL default ' ',
  "AAAARISA" varchar(30) NOT NULL default ' ',
  "AAAARISB" varchar(30) NOT NULL default ' ',
  "AAAARISC" varchar(30) NOT NULL default ' ',
  "AAAARISD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFJ","AAABIWFD","AAABLEEW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARISH" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARISP" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFJ","AAABIWFD","AAAARISP","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARISR" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARISU" varchar(12) NOT NULL default ' ',
  "AAABJQCM" varchar(12) NOT NULL default ' ',
  "AAAARISW" decimal(15,0) NOT NULL default '0',
  "AAAARISX" decimal(15,0) NOT NULL default '0',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAARISZ" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABINFJ","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARITC" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFJ","AAABIWFD","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARITK" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABFZAB" varchar(60) NOT NULL default ' ',
  "AAABMMRH" int(11) NOT NULL default '0',
  "AAAARKJY" varchar(8) NOT NULL default ' ',
  "AAAARKJZ" varchar(1) NOT NULL default ' ',
  "AAAARKKA" varchar(2) NOT NULL default ' ',
  "AAAARKKB" varchar(64) NOT NULL default ' ',
  "AAAARKKC" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFJ","AAABIWFD","AAABFZAB","AAABMMRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARITY" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABLEEW" varchar(30) NOT NULL default ' ',
  "AAABMMRH" int(11) NOT NULL default '0',
  "AAAARIUD" varchar(30) NOT NULL default ' ',
  "AAAARIUE" varchar(2) NOT NULL default ' ',
  "AAAARIUF" varchar(45) NOT NULL default ' ',
  "AAAARIUG" varchar(45) NOT NULL default ' ',
  "AAAARIUH" varchar(30) NOT NULL default ' ',
  "AAAARIUI" varchar(2) NOT NULL default ' ',
  "AAAARIUJ" varchar(45) NOT NULL default ' ',
  "AAAARIUK" varchar(45) NOT NULL default ' ',
  "AAABMMJT" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFJ","AAABIWFD","AAABLEEW","AAABMMRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIUQ" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABLEEW" varchar(30) NOT NULL default ' ',
  "AAABKMWV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFJ","AAABIWFD","AAABLEEW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIUY" (
  "AAAARJHN" varchar(30) NOT NULL default ' ',
  "AAABFNEQ" varchar(10) NOT NULL default ' ',
  "AAABIWQH" varchar(10) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARJHN","AAABFNEQ","AAABIWQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIVG" (
  "AAABFMOY" varchar(1) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABJQWM" varchar(1) NOT NULL default ' ',
  "AAABLEEW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFMOY","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIVN" (
  "AAABFMOY" varchar(1) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABLRXO" varchar(8) NOT NULL default '00000000',
  "AAABLRXN" varchar(8) NOT NULL default '00000000',
  "AAABJQWM" varchar(1) NOT NULL default ' ',
  "AAAARIVT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFMOY","AAABIWFD","AAABLRXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIVX" (
  "AAABFMTA" varchar(30) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABFMTA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIWC" (
  "AAAARJHN" varchar(30) NOT NULL default ' ',
  "AAABFNEQ" varchar(10) NOT NULL default ' ',
  "AAABAMZN" int(11) NOT NULL default '0',
  "AAAARIWG" varchar(4) NOT NULL default '0000',
  "AAABHEYG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARJHN","AAABFNEQ","AAABAMZN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIWL" (
  "AAABFMTA" varchar(30) NOT NULL default ' ',
  "AAABIRTT" varchar(1) NOT NULL default ' ',
  "AAABFNES" varchar(30) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAAARIWR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFMTA","AAABIRTT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIWU" (
  "AAABJVTD" int(11) NOT NULL default '0',
  "AAAARIWW" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABJVTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIWY" (
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIXB" (
  "AAAARKJB" varchar(30) NOT NULL default ' ',
  "AAAARIXH" varchar(32) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAARKJB","AAAARIXH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIXI" (
  "AAAARJAD" varchar(30) NOT NULL default ' ',
  "AAABIFGN" int(11) NOT NULL default '0',
  "AAAARIXX" varchar(6) NOT NULL default '000000',
  "AAABHAPO" double NOT NULL default '0',
  "AAABHAPP" double NOT NULL default '0',
  "AAAARIXO" double NOT NULL default '0',
  "AAAARIXP" double NOT NULL default '0',
  "AAAARIXQ" int(11) NOT NULL default '0',
  "AAAARIXR" double NOT NULL default '0',
  "AAABLLJG" decimal(6,3) NOT NULL default '0.000',
  "AAABLOZE" varchar(60) NOT NULL default ' ',
  "AAAARIXU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARJAD","AAABIFGN","AAAARIXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIXY" (
  "AAAARJAD" varchar(30) NOT NULL default ' ',
  "AAAARIYA" varchar(72) NOT NULL default ' ',
  "AAAARIYB" varchar(30) NOT NULL default ' ',
  "AAAARIYC" varchar(30) NOT NULL default ' ',
  "AAAARJCD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARJAD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIYF" (
  "AAAARJAD" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(6) NOT NULL default '000000',
  "AAAARIYI" varchar(30) NOT NULL default ' ',
  "AAAARIYJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARJAD","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIYM" (
  "AAAARJAD" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(6) NOT NULL default '000000',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(60) NOT NULL default ' ',
  "AAABMNNA" varchar(60) NOT NULL default ' ',
  "AAAARIYT" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAARJAD","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIYW" (
  "AAAARJAD" varchar(30) NOT NULL default ' ',
  "AAAARIYY" varchar(30) NOT NULL default ' ',
  "AAAARIYZ" varchar(1) NOT NULL default ' ',
  "AAAARIZA" decimal(15,0) NOT NULL default '0',
  "AAABFNRW" varchar(12) NOT NULL default ' ',
  "AAAARIZC" varchar(1) NOT NULL default ' ',
  "AAAARIZD" decimal(15,0) NOT NULL default '0',
  "AAAARIZE" varchar(12) NOT NULL default ' ',
  "AAAARIZF" varchar(30) NOT NULL default ' ',
  "AAAARIZG" decimal(15,0) NOT NULL default '0',
  "AAAARIZH" varchar(12) NOT NULL default ' ',
  "AAAARIZI" varchar(1) NOT NULL default ' ',
  "AAAARIZJ" int(11) NOT NULL default '0',
  "AAAARIZK" int(11) NOT NULL default '0',
  "AAAARIZL" int(11) NOT NULL default '0',
  "AAAARIZM" int(11) NOT NULL default '0',
  "AAABGPVW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARJAD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIZP" (
  "AAAARJAD" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARIZS" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAARJAD","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARIZV" (
  "AAAARJAD" varchar(30) NOT NULL default ' ',
  "AAABMMRH" varchar(10) NOT NULL default '0000000000',
  "AAAARIZY" varchar(8) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(64) NOT NULL default ' ',
  "AAABMNNA" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAAARJAD","AAABMMRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJAF" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABGLCK" varchar(10) NOT NULL default '0000000000',
  "AAAARJBZ" varchar(6) NOT NULL default '000000',
  "AAAARJCA" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABINFJ","AAABGLCK","AAAARJBZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJAN" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAAARKIE" varchar(30) NOT NULL default ' ',
  "AAAARJAV" varchar(2) NOT NULL default ' ',
  "AAABGLCK" varchar(10) NOT NULL default '0000000000',
  "AAABLMYF" double NOT NULL default '0',
  PRIMARY KEY ("AAABINFJ","AAAARKIE","AAAARJAV","AAABGLCK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJAX" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAAARJBZ" varchar(6) NOT NULL default '000000',
  "AAAARJCA" varchar(6) NOT NULL default '000000',
  "AAAARJBB" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFJ","AAAARJBZ","AAAARJCA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJBF" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABGLCK" varchar(10) NOT NULL default '0000000000',
  "AAAARJBZ" varchar(6) NOT NULL default '000000',
  "AAAARJCA" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABINFJ","AAABGLCK","AAAARJBZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJBN" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABGLCK" varchar(10) NOT NULL default '0000000000',
  "AAAARJBX" double NOT NULL default '0',
  PRIMARY KEY ("AAABINFJ","AAABGLCK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJBT" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAAARJBZ" varchar(6) NOT NULL default '000000',
  "AAAARJCA" varchar(6) NOT NULL default '000000',
  "AAAARJBX" double NOT NULL default '0',
  PRIMARY KEY ("AAABINFJ","AAAARJBZ","AAAARJCA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJCB" (
  "AAAARJCE" int(11) NOT NULL default '0',
  "AAAARJCD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARJCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJCF" (
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
  KEY "AAAARJCT" ("AAABIVVH","AAABIWFD","AAABLECU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJCX" (
  "AAABIVVH" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIVVF" varchar(8) NOT NULL default '00000000',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABIWMM" varchar(60) NOT NULL default ' ',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  "AAABMRGO" varchar(1) NOT NULL default ' ',
  "AAABLECU" varchar(8) NOT NULL default '00000000',
  "AAABLECV" varchar(8) NOT NULL default '00000000',
  "AAABIVVC" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABIVVH","AAABIWFD","AAABIVVF"),
  KEY "AAAARJDL" ("AAABIVVH","AAABIWFD","AAABLECU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJDP" (
  "AAABIVVH" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIVVF" varchar(8) NOT NULL default '00000000',
  "AAAARVNX" varchar(32) NOT NULL default ' ',
  "AAAARVNY" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABIVVH","AAABIWFD","AAABIVVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJDY" (
  "AAABIVVH" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIVVF" varchar(8) NOT NULL default '00000000',
  "AAAARVNX" varchar(60) NOT NULL default ' ',
  "AAAARVNY" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABIVVH","AAABIWFD","AAABIVVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJEH" (
  "AAABIWES" int(11) NOT NULL default '0',
  "AAABIWET" varchar(30) NOT NULL default ' ',
  "AAABIWMM" varchar(32) NOT NULL default ' ',
  "AAABIWEV" varchar(2) NOT NULL default '00',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWER" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABIWES","AAABIWET","AAABIWMM","AAABIWEV"),
  UNIQUE KEY "AAAARJES" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJEU" (
  "AAABIWES" int(11) NOT NULL default '0',
  "AAABIWET" varchar(30) NOT NULL default ' ',
  "AAABIWMM" varchar(60) NOT NULL default ' ',
  "AAABIWEV" varchar(2) NOT NULL default '00',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWER" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABIWES","AAABIWET","AAABIWMM","AAABIWEV"),
  UNIQUE KEY "AAAARJFF" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJFH" (
  "AAABIWES" int(11) NOT NULL default '0',
  "AAABIWAW" varchar(1) NOT NULL default ' ',
  "AAABLKHX" int(11) NOT NULL default '0',
  "AAABLKHY" int(11) NOT NULL default '0',
  "AAABIWAT" int(11) NOT NULL default '0',
  "AAABIWAM" int(11) NOT NULL default '0',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  "AAABIWAO" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABIWES","AAABIWAW","AAABLKHX","AAABLKHY","AAABIWAT"),
  KEY "AAAARJFV" ("AAABIWES","AAABIWAW","AAABLKHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJFZ" (
  "AAABFMOY" varchar(1) NOT NULL default ' ',
  "AAABLEEW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFMOY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJGD" (
  "AAABFMOY" varchar(1) NOT NULL default ' ',
  "AAABLEEW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFMOY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJGH" (
  "AAABHEYG" varchar(30) NOT NULL default ' ',
  "AAAARYEC" varchar(6) NOT NULL default '000000',
  "AAABDUBC" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABHEYG","AAAARYEC","AAABDUBC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJGO" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAAARJRO" varchar(32) NOT NULL default ' ',
  "AAAARJRK" varchar(30) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABINFJ","AAAARJRO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJGV" (
  "AAABFMOY" varchar(1) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABIWSY" varchar(1) NOT NULL default ' ',
  "AAABIWSZ" varchar(1) NOT NULL default ' ',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFMOY"),
  UNIQUE KEY "AAAARJHC" ("AAABLETY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJHE" (
  "AAAARJHN" varchar(30) NOT NULL default ' ',
  "AAABFNEQ" varchar(10) NOT NULL default ' ',
  "AAAARJHH" varchar(1) NOT NULL default ' ',
  "AAAARJHI" varchar(1) NOT NULL default ' ',
  "AAAARJHJ" varchar(4) NOT NULL default '0000',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAAARYQT" varchar(8) NOT NULL default '00000000',
  "AAAARYQU" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAARJHN","AAABFNEQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJHP" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABIFGN" int(11) NOT NULL default '0',
  "AAABHKDA" varchar(30) NOT NULL default ' ',
  "AAABHKCO" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFJ","AAABIFGN","AAABHKDA","AAABHKCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJHY" (
  "AAAARKJB" varchar(30) NOT NULL default ' ',
  "AAABFTRT" varchar(12) NOT NULL default '000000000000',
  "AAAARJUG" decimal(5,2) NOT NULL default '0.00',
  "AAAARJIC" decimal(5,2) NOT NULL default '0.00',
  "AAAARJID" decimal(15,2) NOT NULL default '0.00',
  "AAAARJIE" smallint(6) NOT NULL default '0',
  "AAAARJIF" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAAARKJB","AAABFTRT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJII" (
  "AAABENSM" varchar(30) NOT NULL default ' ',
  "AAAARKJB" varchar(30) NOT NULL default ' ',
  "AAAARJIL" varchar(50) NOT NULL default ' ',
  "AAAARJIM" varchar(8) NOT NULL default '00000000',
  "AAAARJIN" varchar(12) NOT NULL default ' ',
  "AAAARJIO" decimal(5,2) NOT NULL default '0.00',
  "AAAARJIP" decimal(5,2) NOT NULL default '0.00',
  "AAAARJIQ" decimal(15,2) NOT NULL default '0.00',
  "AAAARJIR" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABENSM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJIT" (
  "AAAARKJB" varchar(30) NOT NULL default ' ',
  "AAABFTRT" varchar(12) NOT NULL default '000000000000',
  "AAABGLID" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAAARKJB","AAABFTRT","AAABGLID")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJJA" (
  "AAABENSM" varchar(30) NOT NULL default ' ',
  "AAABLIMF" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAABENSM","AAABLIMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJJF" (
  "AAAARKJB" varchar(30) NOT NULL default ' ',
  "AAABGLID" varchar(200) NOT NULL default ' ',
  "AAAARKNI" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAAARKJB","AAABGLID"),
  KEY "AAAARJJL" ("AAAARKNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJJN" (
  "AAAARJKF" varchar(64) NOT NULL default ' ',
  "AAABLTOK" varchar(30) NOT NULL default ' ',
  "AAAARJKA" varchar(30) NOT NULL default ' ',
  "AAAARJKB" varchar(30) NOT NULL default ' ',
  "AAABEWQH" varchar(10) NOT NULL default ' ',
  "AAAARJKD" varchar(10) NOT NULL default ' ',
  "AAAARJKE" varchar(30) NOT NULL default ' ',
  "AAAARJJV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARJKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJJX" (
  "AAAARJKF" varchar(64) NOT NULL default ' ',
  "AAABLTOK" varchar(30) NOT NULL default ' ',
  "AAAARJKA" varchar(30) NOT NULL default ' ',
  "AAAARJKB" varchar(30) NOT NULL default ' ',
  "AAABEWQH" varchar(10) NOT NULL default ' ',
  "AAAARJKD" varchar(10) NOT NULL default ' ',
  "AAAARJKE" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAAARJKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJKG" (
  "AAABENSM" varchar(30) NOT NULL default ' ',
  "AAABLIMF" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAABENSM","AAABLIMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJKL" (
  "AAAARKJB" varchar(30) NOT NULL default ' ',
  "AAABFTRT" varchar(12) NOT NULL default '000000000000',
  "AAABGLID" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAAARKJB","AAABFTRT","AAABGLID")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJKS" (
  "AAAARKJB" varchar(30) NOT NULL default ' ',
  "AAAARJLK" varchar(12) NOT NULL default '000000000000',
  "AAAARJUG" decimal(5,2) NOT NULL default '0.00',
  "AAABLNAW" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAAARKJB","AAAARJLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJKZ" (
  "AAAARKJB" varchar(30) NOT NULL default ' ',
  "AAABLNAW" smallint(6) NOT NULL default '0',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARKJB","AAABLNAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJLF" (
  "AAAARKJB" varchar(30) NOT NULL default ' ',
  "AAAARJLK" varchar(12) NOT NULL default '000000000000',
  "AAABGLID" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAAARKJB","AAAARJLK","AAABGLID")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJLM" (
  "AAAARKJB" varchar(30) NOT NULL default ' ',
  "AAAARJLQ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAARKJB","AAAARJLQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJLR" (
  "AAAARKJB" varchar(30) NOT NULL default ' ',
  "AAABAINM" varchar(30) NOT NULL default ' ',
  "AAAARKNI" varchar(5) NOT NULL default '00000',
  "AAAARJLV" varchar(64) NOT NULL default ' ',
  "AAAARJPX" decimal(21,5) NOT NULL default '0.00000',
  "AAAARJPY" decimal(21,5) NOT NULL default '0.00000',
  "AAAARJLY" varchar(1) NOT NULL default ' ',
  "AAAARJLZ" decimal(11,7) NOT NULL default '0.0000000',
  "AAABDMCO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARKJB","AAABAINM","AAAARKNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJME" (
  "AAAARKJB" varchar(30) NOT NULL default ' ',
  "AAABAINM" varchar(30) NOT NULL default ' ',
  "AAAARJNG" varchar(30) NOT NULL default ' ',
  "AAABGKVZ" varchar(3) NOT NULL default ' ',
  "AAABLOSY" decimal(5,0) NOT NULL default '0',
  "AAAARJMK" decimal(21,5) NOT NULL default '0.00000',
  "AAAARJML" decimal(21,5) NOT NULL default '0.00000',
  "AAAARJMM" decimal(11,7) NOT NULL default '0.0000000',
  "AAAARJMN" decimal(5,0) NOT NULL default '0',
  "AAABCFWM" varchar(1) NOT NULL default ' ',
  "AAAARJMP" varchar(1) NOT NULL default ' ',
  "AAAARJMQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARKJB","AAABAINM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJMT" (
  "AAAARKJB" varchar(30) NOT NULL default ' ',
  "AAABBKFL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAAARKJB","AAABBKFL","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJNB" (
  "AAAARKJB" varchar(30) NOT NULL default ' ',
  "AAAATVPT" varchar(3) NOT NULL default '000',
  "AAABAINM" varchar(30) NOT NULL default ' ',
  "AAABHKCO" varchar(64) NOT NULL default ' ',
  "AAAARJNG" varchar(30) NOT NULL default ' ',
  "AAABGKVZ" varchar(3) NOT NULL default ' ',
  "AAABMLPO" decimal(10,0) NOT NULL default '0',
  "AAABLLJG" decimal(5,2) NOT NULL default '0.00',
  "AAAAUXZV" decimal(21,5) NOT NULL default '0.00000',
  "AAABHIEG" decimal(21,5) NOT NULL default '0.00000',
  "AAABBLDD" decimal(19,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAAARKJB","AAAATVPT","AAABAINM","AAABHKCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJNR" (
  "AAAARKJB" varchar(30) NOT NULL default ' ',
  "AAABBKFL" varchar(3) NOT NULL default '000',
  "AAABAINM" varchar(30) NOT NULL default ' ',
  "AAAARKNI" varchar(5) NOT NULL default '00000',
  "AAABKMWV" decimal(19,7) NOT NULL default '0.0000000',
  "AAABMLPO" decimal(10,0) NOT NULL default '0',
  "AAAARJNY" decimal(11,7) NOT NULL default '0.0000000',
  "AAAARJNZ" decimal(11,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAAARKJB","AAABBKFL","AAABAINM","AAAARKNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJOE" (
  "AAAARKJB" varchar(30) NOT NULL default ' ',
  "AAABBKFL" varchar(3) NOT NULL default '000',
  "AAABMLPO" decimal(10,0) NOT NULL default '0',
  "AAAARJOI" decimal(19,7) NOT NULL default '0.0000000',
  "AAAARJOJ" decimal(19,7) NOT NULL default '0.0000000',
  "AAAARJOK" varchar(1) NOT NULL default ' ',
  "AAAARJOL" decimal(19,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAAARKJB","AAABBKFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJOO" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAAARJRK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJOS" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAAARJRO" varchar(32) NOT NULL default ' ',
  "AAAARJRJ" varchar(72) NOT NULL default ' ',
  "AAAARJRK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFJ","AAAARJRO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJOZ" (
  "AAAARKJB" varchar(30) NOT NULL default ' ',
  "AAAARJSD" longtext,
  "AAAARJSE" int(11) NOT NULL default '0',
  "AAAARJSF" longtext,
  "AAABLEJW" longtext,
  PRIMARY KEY ("AAAARKJB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJPG" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABAIMG" varchar(30) NOT NULL default ' ',
  "AAAARKNI" varchar(5) NOT NULL default '00000',
  "AAABMMJT" varchar(64) NOT NULL default ' ',
  "AAAARJUG" decimal(10,0) NOT NULL default '0',
  "AAABKMWV" decimal(19,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABINFJ","AAABAIMG","AAAARKNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJPQ" (
  "AAAARKJB" varchar(30) NOT NULL default ' ',
  "AAABAINM" varchar(30) NOT NULL default ' ',
  "AAAARKNI" varchar(5) NOT NULL default '00000',
  "AAABMMJT" varchar(64) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABKMWV" decimal(11,7) NOT NULL default '0.0000000',
  "AAAARJPX" decimal(21,5) NOT NULL default '0.00000',
  "AAAARJPY" decimal(21,5) NOT NULL default '0.00000',
  PRIMARY KEY ("AAAARKJB","AAABAINM","AAAARKNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJQC" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAAARJTN" varchar(30) NOT NULL default ' ',
  "AAAARJQM" varchar(5) NOT NULL default '00000',
  "AAAARJTM" varchar(30) NOT NULL default ' ',
  "AAAARJQH" varchar(64) NOT NULL default ' ',
  "AAAARJUG" decimal(10,0) NOT NULL default '0',
  "AAABKMWV" decimal(19,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABINFJ","AAAARJTN","AAAARJQM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJQN" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAAARJRO" varchar(32) NOT NULL default ' ',
  "AAAARJRP" varchar(30) NOT NULL default ' ',
  "AAAARJQZ" varchar(5) NOT NULL default '00000',
  "AAAARJRA" varchar(5) NOT NULL default '00000',
  "AAAARJQT" varchar(64) NOT NULL default ' ',
  "AAAARJQU" varchar(64) NOT NULL default ' ',
  "AAAARJQV" decimal(10,0) NOT NULL default '0',
  PRIMARY KEY ("AAABINFJ","AAAARJRO","AAAARJRP","AAAARJQZ","AAAARJRA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJRB" (
  "AAABLUSV" varchar(1) NOT NULL default '0',
  "AAAARKOG" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJRF" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAAARJRO" varchar(32) NOT NULL default ' ',
  "AAAARJRP" varchar(30) NOT NULL default ' ',
  "AAAARJRJ" varchar(72) NOT NULL default ' ',
  "AAAARJRK" varchar(30) NOT NULL default ' ',
  "AAAARJRL" decimal(7,5) NOT NULL default '0.00000',
  "AAAARJRM" decimal(10,0) NOT NULL default '0',
  PRIMARY KEY ("AAABINFJ","AAAARJRO","AAAARJRP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJRQ" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABLGEY" varchar(5) NOT NULL default '00000',
  "AAABHMRI" varchar(5) NOT NULL default '00000',
  "AAABLOZE" varchar(64) NOT NULL default ' ',
  "AAAARJUG" decimal(10,0) NOT NULL default '0',
  "AAABKMWV" decimal(19,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABINFJ","AAABLGEY","AAABHMRI","AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJSB" (
  "AAAARKJB" varchar(30) NOT NULL default ' ',
  "AAAARJSD" longtext,
  "AAAARJSE" int(11) NOT NULL default '0',
  "AAAARJSF" longtext,
  "AAABLEJW" longtext,
  PRIMARY KEY ("AAAARKJB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJSI" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABLGEY" varchar(5) NOT NULL default '00000',
  "AAABKWUR" varchar(5) NOT NULL default '00000',
  "AAAARJSM" varchar(64) NOT NULL default ' ',
  "AAAARJTO" decimal(9,5) NOT NULL default '0.00000',
  "AAAARJSO" varchar(5) NOT NULL default '00000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFJ","AAABLGEY","AAABKWUR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJST" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABLGEY" varchar(5) NOT NULL default '00000',
  "AAABKWUR" varchar(5) NOT NULL default '00000',
  "AAABFTRT" varchar(5) NOT NULL default '00000',
  "AAABGKVZ" varchar(3) NOT NULL default ' ',
  "AAAARJUB" varchar(30) NOT NULL default ' ',
  "AAAARJUA" varchar(30) NOT NULL default ' ',
  "AAABMJCM" varchar(2) NOT NULL default ' ',
  "AAAARJTC" varchar(64) NOT NULL default ' ',
  "AAABHMRI" varchar(5) NOT NULL default '00000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFJ","AAABLGEY","AAABKWUR","AAABFTRT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJTJ" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABLGEY" varchar(5) NOT NULL default '00000',
  "AAAARJTM" varchar(30) NOT NULL default ' ',
  "AAAARJTN" varchar(30) NOT NULL default ' ',
  "AAAARJTO" decimal(9,5) NOT NULL default '0.00000',
  "AAABKMWV" decimal(11,7) NOT NULL default '0.0000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFJ","AAABLGEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJTT" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABLGEY" varchar(5) NOT NULL default '00000',
  "AAABHMRI" varchar(5) NOT NULL default '00000',
  "AAABLTQF" varchar(5) NOT NULL default '00000',
  "AAABGLSO" varchar(1) NOT NULL default ' ',
  "AAABGKVZ" varchar(3) NOT NULL default ' ',
  "AAAARJUA" varchar(30) NOT NULL default ' ',
  "AAAARJUB" varchar(30) NOT NULL default ' ',
  "AAABMJCM" varchar(2) NOT NULL default ' ',
  "AAAARJUD" varchar(64) NOT NULL default ' ',
  "AAAARJUE" varchar(64) NOT NULL default ' ',
  "AAAARJUF" decimal(5,0) NOT NULL default '0',
  "AAAARJUG" decimal(10,0) NOT NULL default '0',
  "AAAARJUH" decimal(10,0) NOT NULL default '0',
  "AAABKMWV" decimal(19,7) NOT NULL default '0.0000000',
  "AAAARJUJ" decimal(19,7) NOT NULL default '0.0000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABHRWW" decimal(5,0) NOT NULL default '0',
  PRIMARY KEY ("AAABINFJ","AAABLGEY","AAABHMRI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJUP" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABFPRE" varchar(30) NOT NULL default ' ',
  "AAABLGEY" varchar(5) NOT NULL default '00000',
  "AAABHKGM" varchar(12) NOT NULL default ' ',
  "AAAARJUU" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFJ","AAABFPRE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJUX" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABAJUD" varchar(30) NOT NULL default ' ',
  "AAAARKNI" varchar(30) NOT NULL default ' ',
  "AAABMJWN" varchar(30) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAAARKMW" varchar(2) NOT NULL default ' ',
  "AAABMMJT" longtext,
  "AAAARKMY" longtext,
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFJ","AAABIWFD","AAABAJUD","AAAARKNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJVS" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABAJUD" varchar(30) NOT NULL default ' ',
  "AAABLFOX" varchar(10) NOT NULL default ' ',
  "AAABFIGO" varchar(30) NOT NULL default ' ',
  "AAABKMWV" decimal(11,7) NOT NULL default '0.0000000',
  "AAAARKHI" varchar(1) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAAARKMW" varchar(2) NOT NULL default ' ',
  "AAABMMJT" longtext,
  "AAAARKMY" longtext,
  "AAAARKHN" longtext,
  "AAAAUUTB" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFJ","AAABIWFD","AAABAJUD","AAABLFOX","AAABFIGO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJWS" (
  "AAAARKCG" varchar(40) NOT NULL default ' ',
  "AAAARJWU" varchar(30) NOT NULL default ' ',
  "AAAARJWV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARKCG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJWX" (
  "AAAARKCQ" varchar(32) NOT NULL default ' ',
  "AAABDNQK" varchar(40) NOT NULL default ' ',
  "AAAARJXA" varchar(50) NOT NULL default ' ',
  "AAAAVLJZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAAARKCQ","AAABDNQK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJXE" (
  "AAAARJXT" varchar(30) NOT NULL default ' ',
  "AAAAVHRN" varchar(30) NOT NULL default ' ',
  "AAABMOHM" varchar(3) NOT NULL default '000',
  "AAAARKNV" varchar(2) NOT NULL default '00',
  "AAAARKNW" varchar(1) NOT NULL default ' ',
  "AAAARKNX" varchar(1) NOT NULL default ' ',
  "AAAARKNY" varchar(1) NOT NULL default ' ',
  "AAAARKNZ" varchar(1) NOT NULL default ' ',
  "AAAARKOA" varchar(1) NOT NULL default ' ',
  "AAAARKOB" varchar(1) NOT NULL default ' ',
  "AAAARKOC" varchar(1) NOT NULL default ' ',
  "AAAARKCQ" varchar(32) NOT NULL default ' ',
  "AAABIJQL" varchar(30) NOT NULL default ' ',
  "AAAARJXS" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAAARJXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJXU" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABLRSJ" varchar(30) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAAAVLIK" varchar(12) NOT NULL default ' ',
  "AAABKNAR" varchar(14) NOT NULL default ' ',
  "AAABFOBV" varchar(14) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAAARKKS" varchar(30) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFJ","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJYN" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABAJUD" varchar(30) NOT NULL default ' ',
  "AAABIZHF" varchar(4) NOT NULL default ' ',
  "AAAARKLG" varchar(3) NOT NULL default '000',
  "AAAARKLH" varchar(2) NOT NULL default '00',
  "AAAARYVB" varchar(2) NOT NULL default '00',
  "AAAARKLJ" varchar(2) NOT NULL default '00',
  "AAAARKOT" varchar(3) NOT NULL default ' ',
  "AAAARKPF" varchar(3) NOT NULL default ' ',
  "AAABLPSU" varchar(50) NOT NULL default ' ',
  "AAAARKLN" varchar(30) NOT NULL default ' ',
  "AAABLFYC" varchar(1) NOT NULL default ' ',
  "AAAARKLP" varchar(1) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAAATOYJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFJ","AAABIWFD","AAABAJUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARJZN" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(50) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFJ","AAABIWFD","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKAB" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARKNI" varchar(30) NOT NULL default ' ',
  "AAABMJWN" varchar(30) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAAARKMW" varchar(2) NOT NULL default ' ',
  "AAABMMJT" longtext,
  "AAAARKMY" longtext,
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFJ","AAABIWFD","AAAARKNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKAU" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAAARKAY" longtext,
  PRIMARY KEY ("AAABINFJ","AAABIWFD","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKBC" (
  "AAABLRSJ" varchar(30) NOT NULL default ' ',
  "AAAAVLIX" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLRSJ","AAAAVLIX","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKBK" (
  "AAAARKCQ" varchar(32) NOT NULL default ' ',
  "AAAARKBM" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAARKCQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKBO" (
  "AAABLRSJ" varchar(30) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABDNQB" varchar(1) NOT NULL default ' ',
  "AAABIZHF" varchar(4) NOT NULL default ' ',
  "AAAARKPM" varchar(3) NOT NULL default '000',
  "AAABLBEK" varchar(1) NOT NULL default ' ',
  "AAABLNNZ" longtext,
  "AAAARKBW" varchar(1) NOT NULL default ' ',
  "AAAARKBX" varchar(1) NOT NULL default ' ',
  "AAAARKBY" varchar(30) NOT NULL default ' ',
  "AAABMNMM" varchar(6) NOT NULL default '000000',
  "AAAARKOT" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLRSJ","AAABMLPV","AAABDNQB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKCE" (
  "AAAARKCQ" varchar(32) NOT NULL default ' ',
  "AAAARKCG" varchar(40) NOT NULL default ' ',
  "AAAARKCH" varchar(40) NOT NULL default ' ',
  "AAAARKCI" varchar(1) NOT NULL default ' ',
  "AAAARKCJ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAAARKCQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKCL" (
  "AAAARKCQ" varchar(32) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAAARKCP" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAAARKCQ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKCS" (
  "AAABLUSV" varchar(30) NOT NULL default ' ',
  "AAABMLPV" varchar(32) NOT NULL default ' ',
  "AAABIJEA" varchar(8) NOT NULL default ' ',
  "AAABLGEF" varchar(30) NOT NULL default ' ',
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAAAVOHE" varchar(30) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABIFRO" varchar(12) NOT NULL default ' ',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(4) NOT NULL default ' ',
  "AAABLTLN" decimal(15,0) NOT NULL default '0',
  "AAABLTLM" decimal(15,0) NOT NULL default '0',
  "AAABLDKQ" varchar(22) NOT NULL default ' ',
  "AAABIZYS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKDI" (
  "AAABLUSV" varchar(30) NOT NULL default ' ',
  "AAABFMPA" varchar(30) NOT NULL default ' ',
  "AAAARKET" varchar(1) NOT NULL default ' ',
  "AAABJHEY" varchar(30) NOT NULL default ' ',
  "AAAARKEV" smallint(6) NOT NULL default '0',
  "AAAARKEW" varchar(30) NOT NULL default ' ',
  "AAAARKEX" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABFMPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKDS" (
  "AAABLUSV" varchar(30) NOT NULL default ' ',
  "AAAARKDU" varchar(40) NOT NULL default ' ',
  "AAAARKDV" varchar(1) NOT NULL default ' ',
  "AAAARKDW" varchar(1) NOT NULL default ' ',
  "AAAARKDX" varchar(8) NOT NULL default '00000000',
  "AAABGHLM" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKEA" (
  "AAABLUSV" varchar(30) NOT NULL default ' ',
  "AAABHMRI" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABLUSV","AAABHMRI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKEF" (
  "AAABLUSV" varchar(30) NOT NULL default ' ',
  "AAABHIXN" varchar(32) NOT NULL default ' ',
  "AAAARKEI" varchar(32) NOT NULL default ' ',
  "AAABLPSU" varchar(72) NOT NULL default ' ',
  "AAAARKEK" varchar(6) NOT NULL default ' ',
  "AAAARKEL" varchar(32) NOT NULL default ' ',
  "AAAARKEM" varchar(32) NOT NULL default ' ',
  "AAAARKEN" varchar(30) NOT NULL default ' ',
  "AAAARKEO" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKEQ" (
  "AAABLUSV" varchar(30) NOT NULL default ' ',
  "AAABFMPA" varchar(30) NOT NULL default ' ',
  "AAAARKET" varchar(1) NOT NULL default ' ',
  "AAABJHEY" varchar(30) NOT NULL default ' ',
  "AAAARKEV" smallint(6) NOT NULL default '0',
  "AAAARKEW" varchar(30) NOT NULL default ' ',
  "AAAARKEX" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABFMPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKFA" (
  "AAABLUSV" varchar(30) NOT NULL default ' ',
  "AAABIWLU" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABLRXO" varchar(8) NOT NULL default '00000000',
  "AAABLRXN" varchar(8) NOT NULL default '00000000',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTO" varchar(40) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKFM" (
  "AAABLUZL" varchar(32) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABAJUD" varchar(30) NOT NULL default ' ',
  "AAAARKFR" varchar(30) NOT NULL default ' ',
  "AAAARKFS" varchar(30) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  "AAABLFDD" varchar(1) NOT NULL default ' ',
  "AAAARKGA" varchar(1) NOT NULL default ' ',
  "AAAARKGB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUZL","AAABIWFD","AAABINFJ","AAABAJUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKGG" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABAJUD" varchar(30) NOT NULL default ' ',
  "AAAARKNI" varchar(30) NOT NULL default ' ',
  "AAABMJWN" varchar(30) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAAARKMW" varchar(2) NOT NULL default ' ',
  "AAABMMJT" varchar(64) NOT NULL default ' ',
  "AAAARKMY" varchar(64) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  "AAABLFDD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFJ","AAABIWFD","AAABAJUD","AAAARKNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKHC" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABAJUD" varchar(30) NOT NULL default ' ',
  "AAABFIGO" varchar(30) NOT NULL default ' ',
  "AAABKMWV" decimal(11,7) NOT NULL default '0.0000000',
  "AAAARKHI" varchar(1) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAAARKMW" varchar(2) NOT NULL default ' ',
  "AAABMMJT" varchar(64) NOT NULL default ' ',
  "AAAARKMY" varchar(64) NOT NULL default ' ',
  "AAAARKHN" varchar(64) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  "AAABLFDD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFJ","AAABIWFD","AAABAJUD","AAABFIGO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKHZ" (
  "AAABLUZL" varchar(32) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAAARKID" int(11) NOT NULL default '0',
  "AAAARKIE" varchar(30) NOT NULL default ' ',
  "AAABMLPV" varchar(72) NOT NULL default ' ',
  "AAAARKIG" varchar(30) NOT NULL default ' ',
  "AAAARKOU" varchar(1) NOT NULL default ' ',
  "AAAARKII" varchar(14) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAAAVLIK" varchar(12) NOT NULL default ' ',
  "AAABKNAR" varchar(14) NOT NULL default ' ',
  "AAABFOBV" varchar(14) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUZL","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKIX" (
  "AAABLUZL" varchar(32) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARXMN" varchar(30) NOT NULL default ' ',
  "AAAARKJB" varchar(30) NOT NULL default ' ',
  "AAAARKJC" varchar(30) NOT NULL default ' ',
  "AAABLRSJ" varchar(30) NOT NULL default ' ',
  "AAABFOBU" varchar(12) NOT NULL default ' ',
  "AAABFOBV" varchar(14) NOT NULL default ' ',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAABKNAR" varchar(14) NOT NULL default ' ',
  "AAAARKJI" varchar(1) NOT NULL default ' ',
  "AAABKWFM" varchar(8) NOT NULL default '00000000',
  "AAABMKKB" varchar(8) NOT NULL default '00000000',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUZL","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKJT" (
  "AAABLUZL" varchar(32) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABMKYH" varchar(30) NOT NULL default ' ',
  "AAAARKJY" varchar(8) NOT NULL default ' ',
  "AAAARKJZ" varchar(1) NOT NULL default ' ',
  "AAAARKKA" varchar(2) NOT NULL default ' ',
  "AAAARKKB" varchar(64) NOT NULL default ' ',
  "AAAARKKC" varchar(64) NOT NULL default ' ',
  "AAABLFDD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUZL","AAABIWFD","AAABINFJ","AAABMKYH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKKI" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABLRSJ" varchar(30) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAAAVLIK" varchar(12) NOT NULL default ' ',
  "AAABKNAR" varchar(14) NOT NULL default ' ',
  "AAABFOBV" varchar(14) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAAARKKS" varchar(30) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFJ","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKLB" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABAJUD" varchar(30) NOT NULL default ' ',
  "AAABIZHF" varchar(4) NOT NULL default ' ',
  "AAAARKLG" varchar(3) NOT NULL default '000',
  "AAAARKLH" varchar(2) NOT NULL default '00',
  "AAAARYVB" varchar(2) NOT NULL default '00',
  "AAAARKLJ" varchar(2) NOT NULL default '00',
  "AAAARKOT" varchar(3) NOT NULL default ' ',
  "AAAARKPF" varchar(3) NOT NULL default ' ',
  "AAABLPSU" varchar(50) NOT NULL default ' ',
  "AAAARKLN" varchar(30) NOT NULL default ' ',
  "AAABLFYC" varchar(1) NOT NULL default ' ',
  "AAAARKLP" varchar(1) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  "AAABLFDD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFJ","AAABIWFD","AAABAJUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKMA" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(50) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  "AAABLFDD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFJ","AAABIWFD","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKMP" (
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARKNI" varchar(30) NOT NULL default ' ',
  "AAABMJWN" varchar(30) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAAARKMW" varchar(2) NOT NULL default ' ',
  "AAABMMJT" varchar(64) NOT NULL default ' ',
  "AAAARKMY" varchar(64) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  "AAABLFDD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFJ","AAABIWFD","AAAARKNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKNJ" (
  "AAABLRSJ" varchar(30) NOT NULL default ' ',
  "AAAAVLIX" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLRSJ","AAAAVLIX","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKNR" (
  "AAABLRSJ" varchar(30) NOT NULL default ' ',
  "AAAAVHRN" varchar(30) NOT NULL default ' ',
  "AAABMOHM" varchar(3) NOT NULL default '000',
  "AAAARKNV" varchar(2) NOT NULL default '00',
  "AAAARKNW" varchar(1) NOT NULL default ' ',
  "AAAARKNX" varchar(1) NOT NULL default ' ',
  "AAAARKNY" varchar(1) NOT NULL default ' ',
  "AAAARKNZ" varchar(1) NOT NULL default ' ',
  "AAAARKOA" varchar(1) NOT NULL default ' ',
  "AAAARKOB" varchar(1) NOT NULL default ' ',
  "AAAARKOC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLRSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKOE" (
  "AAABLGEF" varchar(2) NOT NULL default ' ',
  "AAAARKOG" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLGEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKOI" (
  "AAAARKOM" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAAARKOM","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKOO" (
  "AAABLRSJ" varchar(30) NOT NULL default ' ',
  "AAAARKOT" varchar(3) NOT NULL default ' ',
  "AAAARKOU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLRSJ","AAAARKOT","AAAARKOU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKOV" (
  "AAABLUSV" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKPB" (
  "AAABLRSJ" varchar(30) NOT NULL default ' ',
  "AAAARKPF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLRSJ","AAAARKPF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKPG" (
  "AAABLRSJ" varchar(30) NOT NULL default ' ',
  "AAAAVLIX" varchar(30) NOT NULL default ' ',
  "AAABDNQB" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABIZHF" varchar(4) NOT NULL default ' ',
  "AAAARKPM" varchar(3) NOT NULL default '000',
  "AAABLBEK" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLNNZ" varchar(64) NOT NULL default ' ',
  "AAAARKPQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLRSJ","AAAAVLIX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKPT" (
  "AAABLUSV" varchar(30) NOT NULL default ' ',
  "AAAARKPV" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKPY" (
  "AAAARKQK" varchar(32) NOT NULL default ' ',
  "AAABMRFO" varchar(4) NOT NULL default ' ',
  "AAABFVIF" varchar(3) NOT NULL default '000',
  "AAAARKQC" decimal(15,0) NOT NULL default '0',
  "AAABDSTC" varchar(12) NOT NULL default ' ',
  "AAABINFJ" varchar(30) NOT NULL default ' ',
  "AAAARKQF" decimal(15,0) NOT NULL default '0',
  "AAAARKQG" varchar(1) NOT NULL default ' ',
  "AAAARXMN" varchar(72) NOT NULL default ' ',
  "AAABLDKQ" varchar(22) NOT NULL default ' ',
  "AAABIZYS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAARKQK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKQL" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMLZS" varchar(3) NOT NULL default '000',
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEN","AAABMLZS","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKQS" (
  "AAABIKFN" varchar(30) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABIKFN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKQW" (
  "AAABFMOY" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLRXO" varchar(8) NOT NULL default '00000000',
  "AAABLRXN" varchar(8) NOT NULL default '00000000',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTO" varchar(40) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABFMOY","AAABMLQA","AAABLRXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKRH" (
  "AAAARXMN" varchar(30) NOT NULL default ' ',
  "AAAARKRQ" varchar(1) NOT NULL default '0',
  "AAABLRSJ" varchar(30) NOT NULL default ' ',
  "AAAARKRT" varchar(30) NOT NULL default ' ',
  "AAAARKRU" varchar(30) NOT NULL default ' ',
  "AAAARKRV" varchar(30) NOT NULL default ' ',
  "AAAARKRW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARXMN","AAAARKRQ"),
  KEY "AAAARKRR" ("AAABLRSJ","AAAARKRT","AAAARKRU","AAAARKRV","AAAARKRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKRX" (
  "AAAARKSC" int(11) NOT NULL default '0',
  "AAAARKSD" int(11) NOT NULL default '0',
  "AAAARKSE" int(11) NOT NULL default '0',
  "AAAARKSF" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAARKSC","AAAARKSD","AAAARKSE","AAAARKSF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKSG" (
  "AAABIKFN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARKUA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIKFN","AAABIWFD","AAAARKUA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKSN" (
  "AAAARRIM" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAAWYIH" varchar(30) NOT NULL default ' ',
  "AAABIWGC" varchar(10) NOT NULL default ' ',
  "AAAARONQ" varchar(1) NOT NULL default ' ',
  "AAAARONS" varchar(1) NOT NULL default ' ',
  "AAAARKSY" varchar(1) NOT NULL default ' ',
  "AAAARKSZ" varchar(1) NOT NULL default ' ',
  "AAAARKTA" varchar(1) NOT NULL default ' ',
  "AAAARKTB" varchar(1) NOT NULL default ' ',
  "AAAARKTC" varchar(2) NOT NULL default ' ',
  "AAAARKTD" varchar(5) NOT NULL default ' ',
  "AAAARKTE" varchar(2) NOT NULL default ' ',
  "AAAARKTF" varchar(5) NOT NULL default ' ',
  "AAAARKTG" varchar(1) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAAARTMO" varchar(1) NOT NULL default ' ',
  "AAAARTMP" varchar(1) NOT NULL default ' ',
  "AAABFNLO" varchar(1) NOT NULL default ' ',
  "AAAARVTY" varchar(1) NOT NULL default ' ',
  "AAAARTMS" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAARRIM","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKTQ" (
  "AAAARRIM" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAAARKUA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARRIM","AAABIWFD","AAABIHLK"),
  KEY "AAAARKTY" ("AAABIWFD","AAAARKUA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKUB" (
  "AAAARRIM" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARKVF" varchar(3) NOT NULL default ' ',
  "AAABIITF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARRIM","AAABIWFD","AAAARKVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKUJ" (
  "AAAARRIM" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARKWS" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABIRTO" varchar(1) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARRIM","AAABIWFD","AAAARKWS","AAABIHLK"),
  KEY "AAAARKUU" ("AAABIWFD","AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKUX" (
  "AAAARRIM" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARKVF" varchar(3) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARRIM","AAABIWFD","AAAARKVF","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKVH" (
  "AAAARRIM" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAAARLHR" varchar(60) NOT NULL default ' ',
  "AAAARZAZ" varchar(1) NOT NULL default ' ',
  "AAAARLIN" varchar(1) NOT NULL default ' ',
  "AAAARLIO" varchar(1) NOT NULL default ' ',
  "AAAARLIP" varchar(1) NOT NULL default ' ',
  "AAAARKVQ" varchar(1) NOT NULL default ' ',
  "AAAARKVR" varchar(1) NOT NULL default ' ',
  "AAAARKVS" varchar(1) NOT NULL default ' ',
  "AAAARKVT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARRIM","AAABIWFD","AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKVX" (
  "AAAARRIM" varchar(30) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  "AAAARKWB" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAAARRIM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKWD" (
  "AAAARRIM" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAARRIM","AAABIWFD","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKWM" (
  "AAAARRIM" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARKWS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARRIM","AAABIWFD","AAAARKWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKWT" (
  "AAAARKYS" varchar(24) NOT NULL default ' ',
  "AAAARKYT" varchar(1) NOT NULL default ' ',
  "AAAARKYN" varchar(24) NOT NULL default ' ',
  "AAAARKWX" varchar(2) NOT NULL default ' ',
  "AAAARKWY" decimal(15,0) NOT NULL default '0',
  "AAAAUZGL" varchar(12) NOT NULL default ' ',
  "AAAARKXA" decimal(15,0) NOT NULL default '0',
  "AAAARKXB" varchar(12) NOT NULL default ' ',
  "AAAARKXC" varchar(30) NOT NULL default ' ',
  "AAAARKXD" varchar(3) NOT NULL default ' ',
  "AAAARKXE" decimal(15,0) NOT NULL default '0',
  "AAAARKXF" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAARKYS","AAAARKYT"),
  KEY "AAAARKXI" ("AAAARKYN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKXK" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAAARKXM" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKXO" (
  "AAABHTTC" varchar(24) NOT NULL default ' ',
  "AAAAWDEM" varchar(24) NOT NULL default ' ',
  "AAAARKXR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABHTTC","AAAAWDEM"),
  KEY "AAAARKXU" ("AAAAWDEM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKXW" (
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMNMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKYA" (
  "AAABILUK" varchar(40) NOT NULL default ' ',
  "AAAARKYG" varchar(2) NOT NULL default '00',
  "AAABLRQQ" varchar(24) NOT NULL default ' ',
  "AAABJERY" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABILUK","AAAARKYG"),
  UNIQUE KEY "AAAARKYH" ("AAABLRQQ","AAABJERY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKYK" (
  "AAAARKYS" varchar(24) NOT NULL default ' ',
  "AAAARKYT" varchar(1) NOT NULL default ' ',
  "AAAARKYN" varchar(24) NOT NULL default ' ',
  "AAAARKYO" varchar(1) NOT NULL default ' ',
  "AAAARKYP" varchar(1) NOT NULL default ' ',
  "AAAARKYQ" varchar(1) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAARKYS","AAAARKYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKYU" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKYX" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAARUJJ" varchar(25) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAAARUJJ","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKZH" (
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAAARKZJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARKZL" (
  "AAABGIWA" varchar(8) NOT NULL default '00000000',
  "AAABGIWB" varchar(6) NOT NULL default '000000',
  "AAAAROKW" varchar(12) NOT NULL default ' ',
  "AAAARLAA" varchar(30) NOT NULL default ' ',
  "AAAARLCM" varchar(30) NOT NULL default ' ',
  "AAAARLAC" varchar(30) NOT NULL default ' ',
  "AAABGVGY" int(11) NOT NULL default '0',
  "AAAARKZT" varchar(1) NOT NULL default ' ',
  "AAAARKZU" varchar(1) NOT NULL default ' ',
  "AAAARKZV" double NOT NULL default '0',
  "AAAARKZW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGIWA","AAABGIWB","AAAAROKW","AAAARLAA","AAAARLCM","AAAARLAC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARLAD" (
  "AAABGVGY" int(11) NOT NULL default '0',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(60) NOT NULL default ' ',
  "AAABMNNA" varchar(60) NOT NULL default ' ',
  "AAAARLAT" int(11) NOT NULL default '0',
  "AAAARLAU" int(11) NOT NULL default '0',
  "AAAARWAG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGVGY","AAABIWNN","AAABMNML","AAABIJIG","AAABMNMZ","AAABMNNA","AAAARLAT","AAAARLAU","AAAARWAG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARLAW" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAABJVTD" int(11) NOT NULL default '0',
  "AAAARLBA" varchar(8) NOT NULL default '00000000',
  "AAAARLBB" varchar(6) NOT NULL default '000000',
  "AAAARLBC" varchar(12) NOT NULL default ' ',
  "AAAARLBD" varchar(30) NOT NULL default ' ',
  "AAAARLBE" varchar(30) NOT NULL default ' ',
  "AAAARLBF" int(11) NOT NULL default '0',
  "AAAARLBG" int(11) NOT NULL default '0',
  "AAABJHGJ" varchar(1) NOT NULL default ' ',
  "AAABJHGK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHKQ","AAABMNMH","AAABJVTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARLBM" (
  "AAABIWES" int(11) NOT NULL default '0',
  "AAABIWAW" varchar(1) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAAARWNN" int(11) NOT NULL default '0',
  "AAAARLCC" int(11) NOT NULL default '0',
  "AAAARLCD" decimal(15,0) NOT NULL default '0',
  "AAAARLCM" varchar(30) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAAARLBV" int(11) NOT NULL default '0',
  "AAAARLBW" varchar(1) NOT NULL default ' ',
  "AAAARLBX" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABIWES","AAABIWAW","AAABLETY","AAAARWNN","AAAARLCC","AAAARLCD"),
  UNIQUE KEY "AAAARLCE" ("AAAARLCM"),
  KEY "AAAARLCG" ("AAABLUSG","AAABIWES","AAABIWAW","AAABLETY","AAAARWNN","AAAARLCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARLCN" (
  "AAABJYKA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJYKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARLCQ" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAAAVRPE" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABIHKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARLCU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(22) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAAARLFZ" varchar(8) NOT NULL default '00000000',
  "AAAARLFY" int(11) NOT NULL default '0',
  "AAABLPSU" varchar(29) NOT NULL default ' ',
  "AAABGSHN" varchar(29) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARLDK" (
  "AAAARLFY" int(11) NOT NULL default '0',
  "AAAARLFZ" varchar(8) NOT NULL default '00000000',
  "AAAARLGA" varchar(6) NOT NULL default '000000',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAAARLEC" varchar(1) NOT NULL default ' ',
  "AAAARLED" varchar(1) NOT NULL default ' ',
  "AAAARLEE" varchar(1) NOT NULL default ' ',
  "AAAARLFS" varchar(1) NOT NULL default ' ',
  "AAAARLFT" varchar(1) NOT NULL default ' ',
  "AAAARLDU" varchar(1) NOT NULL default ' ',
  "AAAARLDV" varchar(1) NOT NULL default ' ',
  "AAAARLFV" int(11) NOT NULL default '0',
  "AAABIJNY" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAARLFY","AAAARLFZ","AAAARLGA","AAABLZKF","AAAARLEC","AAAARLED","AAAARLEE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARLEF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(22) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAAARLFZ" varchar(8) NOT NULL default '00000000',
  "AAAARLFY" int(11) NOT NULL default '0',
  "AAABIWNE" varchar(30) NOT NULL default ' ',
  "AAABLPSU" varchar(29) NOT NULL default ' ',
  "AAABGSHN" varchar(29) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARLEW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(22) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAAARLFZ" varchar(8) NOT NULL default '00000000',
  "AAAARLFY" int(11) NOT NULL default '0',
  "AAABIWNE" varchar(30) NOT NULL default ' ',
  "AAABLPSU" varchar(29) NOT NULL default ' ',
  "AAABGSHN" varchar(29) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARLFN" (
  "AAAARLFY" int(11) NOT NULL default '0',
  "AAAARLFZ" varchar(8) NOT NULL default '00000000',
  "AAAARLGA" varchar(6) NOT NULL default '000000',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAAARLFS" varchar(1) NOT NULL default ' ',
  "AAAARLFT" varchar(1) NOT NULL default ' ',
  "AAAARLFU" varchar(1) NOT NULL default ' ',
  "AAAARLFV" int(11) NOT NULL default '0',
  "AAABIJNY" int(11) NOT NULL default '0',
  "AAAARLFX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARLFY","AAAARLFZ","AAAARLGA","AAABLZKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARLGC" (
  "AAABLSDL" varchar(3) NOT NULL default ' ',
  "AAAARLGS" varchar(20) NOT NULL default ' ',
  "AAABMLPV" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSDL","AAAARLGS","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARLGJ" (
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABLSDL" varchar(3) NOT NULL default ' ',
  "AAAARLGS" varchar(20) NOT NULL default ' ',
  "AAABMLPV" varchar(20) NOT NULL default ' ',
  "AAABLENL" varchar(2) NOT NULL default ' ',
  "AAABHPIX" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNJW","AAABLSDL","AAAARLGS","AAABMLPV","AAABLENL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARLGV" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(50) NOT NULL default ' ',
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
CREATE TABLE "AAAARLHL" (
  "AAAARLIA" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABFMTA" varchar(30) NOT NULL default ' ',
  "AAAARLIJ" varchar(1) NOT NULL default ' ',
  "AAAARLIL" varchar(1) NOT NULL default ' ',
  "AAAARLHR" varchar(60) NOT NULL default ' ',
  "AAAARLHS" varchar(1) NOT NULL default ' ',
  "AAAARLHT" varchar(10) NOT NULL default ' ',
  "AAAARLHU" varchar(1) NOT NULL default ' ',
  "AAABIJZO" varchar(1) NOT NULL default ' ',
  "AAAARZAZ" varchar(1) NOT NULL default ' ',
  "AAAARLIN" varchar(1) NOT NULL default ' ',
  "AAAARLIO" varchar(1) NOT NULL default ' ',
  "AAAARLIP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARLIA","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARLIC" (
  "AAAARLIE" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAARLIE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARLIF" (
  "AAABIHRN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABFMTA" varchar(30) NOT NULL default ' ',
  "AAAARLIJ" varchar(1) NOT NULL default ' ',
  "AAAARLIK" varchar(3) NOT NULL default ' ',
  "AAAARLIL" varchar(1) NOT NULL default ' ',
  "AAAARZAZ" varchar(1) NOT NULL default ' ',
  "AAAARLIN" varchar(1) NOT NULL default ' ',
  "AAAARLIO" varchar(1) NOT NULL default ' ',
  "AAAARLIP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHRN","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARLIS" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARLIV" (
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAAARLIX" VARBINARY(255),
  PRIMARY KEY ("AAABLESS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARLIZ" (
  "AAABMLXR" varchar(63) NOT NULL default ' ',
  "AAABMLPX" varchar(40) NOT NULL default ' ',
  "AAAARLJC" VARBINARY(255),
  PRIMARY KEY ("AAABMLXR","AAABMLPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARLJF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLXR" varchar(63) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAAARLJJ" varchar(8) NOT NULL default '00000000',
  "AAAARLJK" varchar(6) NOT NULL default '000000',
  "AAABLSBY" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAABMLXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARLJO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMPWO" varchar(6) NOT NULL default '000000',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABMCVR" varchar(2) NOT NULL default '00',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVV" varchar(4) NOT NULL default ' ',
  "AAABMCVW" varchar(10) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCQA" decimal(13,2) NOT NULL default '0.00',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABMCXD" varchar(15) NOT NULL default ' ',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABLIPB" varchar(3) NOT NULL default ' ',
  "AAABMCVN" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVO" varchar(3) NOT NULL default ' ',
  "AAABLHQX" decimal(13,3) NOT NULL default '0.000',
  "AAAARLKK" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABLUFX" varchar(1) NOT NULL default ' ',
  "AAABLXTD" varchar(1) NOT NULL default ' ',
  "AAABLHRD" varchar(4) NOT NULL default ' ',
  "AAABLLZK" varchar(1) NOT NULL default ' ',
  "AAAARLKQ" varchar(1) NOT NULL default ' ',
  "AAAARLKR" varchar(1) NOT NULL default ' ',
  "AAABMCWE" varchar(1) NOT NULL default ' ',
  "AAABMCWF" varchar(1) NOT NULL default ' ',
  "AAABMCWG" varchar(1) NOT NULL default ' ',
  "AAABMCWH" varchar(1) NOT NULL default ' ',
  "AAABMCWI" varchar(1) NOT NULL default ' ',
  "AAABMCWJ" varchar(1) NOT NULL default ' ',
  "AAABMCWK" varchar(1) NOT NULL default ' ',
  "AAABMCXP" varchar(1) NOT NULL default ' ',
  "AAABMCXQ" varchar(1) NOT NULL default ' ',
  "AAABMCVX" varchar(4) NOT NULL default ' ',
  "AAABMDGB" varchar(16) NOT NULL default ' ',
  "AAABMCNM" varchar(1) NOT NULL default ' ',
  "AAAARLLE" decimal(13,2) NOT NULL default '0.00',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLHQY" decimal(13,2) NOT NULL default '0.00',
  "AAABLHRG" decimal(13,2) NOT NULL default '0.00',
  "AAAAWKUQ" varchar(1) NOT NULL default ' ',
  "AAABJKVB" varchar(10) NOT NULL default ' ',
  "AAABLHRV" varchar(4) NOT NULL default '0000',
  "AAABJKVC" varchar(4) NOT NULL default '0000',
  "AAABLJGQ" varchar(18) NOT NULL default ' ',
  "AAAARLLN" decimal(13,3) NOT NULL default '0.000',
  "AAAARLLO" decimal(13,3) NOT NULL default '0.000',
  "AAAARLLP" decimal(13,2) NOT NULL default '0.00',
  "AAAARLLQ" varchar(1) NOT NULL default ' ',
  "AAABMDAC" varchar(2) NOT NULL default ' ',
  "AAABMDAD" varchar(3) NOT NULL default ' ',
  "AAABMDAE" varchar(2) NOT NULL default ' ',
  "AAABKPPY" varchar(10) NOT NULL default '0000000000',
  "AAAARLLV" varchar(10) NOT NULL default '0000000000',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAABMCSI" varchar(1) NOT NULL default ' ',
  "AAAARLLY" varchar(1) NOT NULL default ' ',
  "AAAARLLZ" varchar(1) NOT NULL default ' ',
  "AAABLTXE" varchar(3) NOT NULL default '000',
  "AAABLTXF" varchar(2) NOT NULL default '00',
  "AAAARLMC" decimal(13,2) NOT NULL default '0.00',
  "AAAARLMD" decimal(13,2) NOT NULL default '0.00',
  "AAAARLME" decimal(13,2) NOT NULL default '0.00',
  "AAAAZQMM" varchar(1) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLOMA" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDBA","AAABMDBB","AAABMPWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARLMM" (
  "AAABIVVH" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABIWMM" varchar(32) NOT NULL default ' ',
  "AAAARLMR" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABIVVH","AAABIWFD","AAABIWNN","AAABIWMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARLMW" (
  "AAABFMTA" varchar(30) NOT NULL default ' ',
  "AAABIRTT" varchar(1) NOT NULL default ' ',
  "AAABIKGM" varchar(60) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFMTA","AAABIRTT","AAABIKGM","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARLNF" (
  "AAABFMTA" varchar(30) NOT NULL default ' ',
  "AAABIRTT" varchar(1) NOT NULL default ' ',
  "AAAARLNN" varchar(1) NOT NULL default ' ',
  "AAABFNES" varchar(30) NOT NULL default ' ',
  "AAAARYEC" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABFMTA","AAABIRTT","AAAARLNN","AAABFNES","AAAARYEC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARLNQ" (
  "AAABIGIV" varchar(30) NOT NULL default ' ',
  "AAABLUFA" varchar(30) NOT NULL default ' ',
  "AAAARLNT" decimal(15,0) NOT NULL default '0',
  "AAAARLNU" decimal(15,0) NOT NULL default '0',
  "AAAARLNV" varchar(12) NOT NULL default ' ',
  "AAAARLNW" varchar(12) NOT NULL default ' ',
  "AAAARLNX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIGIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARLNZ" (
  "AAABIGIV" varchar(30) NOT NULL default ' ',
  "AAABGFLE" int(11) NOT NULL default '0',
  "AAAARLOC" varchar(1) NOT NULL default ' ',
  "AAABMKFH" varchar(30) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(72) NOT NULL default ' ',
  "AAABMNMZ" varchar(60) NOT NULL default ' ',
  "AAABMNNA" varchar(60) NOT NULL default ' ',
  "AAABMMVU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIGIV","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARLOL" (
  "AAABMKFH" varchar(30) NOT NULL default ' ',
  "AAAAVCUB" varchar(15) NOT NULL default ' ',
  "AAABLUFA" varchar(15) NOT NULL default ' ',
  "AAABLRFA" varchar(30) NOT NULL default ' ',
  "AAAARLOV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAAAVCUB","AAABLUFA","AAABLRFA","AAAARLOV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARLOW" (
  "AAABHEYG" varchar(30) NOT NULL default ' ',
  "AAAARYEC" varchar(6) NOT NULL default '000000',
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABDUBC" int(11) NOT NULL default '0',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAABKWQO" varchar(1) NOT NULL default ' ',
  "AAABJQWM" varchar(1) NOT NULL default ' ',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEYG","AAAARYEC","AAABIHKQ","AAABDUBC"),
  KEY "AAAARLPK" ("AAABHEYG","AAABMPMT"),
  KEY "AAAARLPN" ("AAABHEYG","AAABKWQO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARLPQ" (
  "AAABHEYG" varchar(30) NOT NULL default ' ',
  "AAAARYEC" varchar(6) NOT NULL default '000000',
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABDUBC" int(11) NOT NULL default '0',
  "AAABGFLE" varchar(10) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEYG","AAAARYEC","AAABIHKQ","AAABDUBC","AAABGFLE"),
  KEY "AAAARLQI" ("AAABHEYG","AAAARYEC","AAABDUBC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARLQM" (
  "AAABLHPU" varchar(40) NOT NULL default ' ',
  "AAABLJIM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHPU","AAABLJIM"),
  KEY "AAAARLQR" ("AAABLJIM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARLQT" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABEUWE" varchar(1) NOT NULL default ' ',
  "AAAARLQW" varchar(1) NOT NULL default ' ',
  "AAABJEDH" varchar(1) NOT NULL default ' ',
  "AAAARLQY" varchar(20) NOT NULL default ' ',
  "AAAARLQZ" varchar(20) NOT NULL default ' ',
  "AAABDFYR" varchar(3) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABGPQK" varchar(26) NOT NULL default ' ',
  "AAABLGEF" varchar(14) NOT NULL default ' ',
  "AAAARLRF" varchar(30) NOT NULL default ' ',
  "AAAARLRG" varchar(30) NOT NULL default ' ',
  "AAAARLRH" varchar(1) NOT NULL default ' ',
  "AAAARLRI" varchar(4) NOT NULL default '0000',
  "AAAARLRJ" int(11) NOT NULL default '0',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  "AAAARLRL" int(11) NOT NULL default '0',
  "AAABFWOA" varchar(10) NOT NULL default ' ',
  "AAAARLRN" varchar(1) NOT NULL default ' ',
  "AAAARLRO" varchar(1) NOT NULL default ' ',
  "AAAARLRP" varchar(1) NOT NULL default ' ',
  "AAAARLRQ" varchar(1) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAAARLRS" varchar(1) NOT NULL default ' ',
  "AAAARLRT" varchar(1) NOT NULL default ' ',
  "AAAARLRU" int(11) NOT NULL default '0',
  "AAAARLRV" decimal(5,2) NOT NULL default '0.00',
  "AAAARLRW" varchar(2) NOT NULL default ' ',
  "AAAARLRX" varchar(1) NOT NULL default ' ',
  "AAAARLRY" varchar(1) NOT NULL default ' ',
  "AAAARLRZ" varchar(1) NOT NULL default ' ',
  "AAAARLSA" varchar(1) NOT NULL default ' ',
  "AAAARLSB" varchar(1) NOT NULL default ' ',
  "AAAARLSC" varchar(1) NOT NULL default ' ',
  "AAAARLSD" varchar(1) NOT NULL default ' ',
  "AAABLITJ" varchar(1) NOT NULL default ' ',
  "AAABLITK" varchar(1) NOT NULL default ' ',
  "AAABLITN" varchar(1) NOT NULL default ' ',
  "AAAARLSH" varchar(128) NOT NULL default ' ',
  "AAABGPVW" varchar(1) NOT NULL default ' ',
  "AAAARLSJ" varchar(1) NOT NULL default ' ',
  "AAAARLSK" varchar(1) NOT NULL default ' ',
  "AAAARLSL" varchar(1) NOT NULL default ' ',
  "AAAARLSM" varchar(1) NOT NULL default ' ',
  "AAAARLSN" varchar(1) NOT NULL default ' ',
  "AAAARLSO" varchar(1) NOT NULL default ' ',
  "AAAARLSP" varchar(1) NOT NULL default ' ',
  "AAAARLSQ" varchar(2) NOT NULL default '00',
  "AAAARLSR" varchar(2) NOT NULL default '00',
  "AAAARLSS" varchar(8) NOT NULL default ' ',
  "AAABLUVJ" varchar(32) NOT NULL default ' ',
  "AAAARLSU" varchar(1) NOT NULL default ' ',
  "AAAARLSV" varchar(1) NOT NULL default ' ',
  "AAAARLSW" varchar(1) NOT NULL default ' ',
  "AAAARLSX" varchar(1) NOT NULL default ' ',
  "AAAARLSY" varchar(1) NOT NULL default ' ',
  "AAAARLSZ" varchar(1) NOT NULL default ' ',
  "AAAARLTA" varchar(1) NOT NULL default ' ',
  "AAAARLTB" varchar(1) NOT NULL default ' ',
  "AAAARLTC" varchar(1) NOT NULL default ' ',
  "AAAARLTD" varchar(1) NOT NULL default ' ',
  "AAAARLTE" varchar(1) NOT NULL default ' ',
  "AAAARLTF" varchar(1) NOT NULL default ' ',
  "AAAARLTG" varchar(1) NOT NULL default ' ',
  "AAAARLTH" varchar(1) NOT NULL default ' ',
  "AAAARLTI" varchar(1) NOT NULL default ' ',
  "AAAARLTJ" varchar(1) NOT NULL default ' ',
  "AAAARLTK" varchar(1) NOT NULL default ' ',
  "AAAARLTL" varchar(1) NOT NULL default ' ',
  "AAAARLTM" varchar(1) NOT NULL default ' ',
  "AAAARLTN" varchar(1) NOT NULL default ' ',
  "AAAARLTO" varchar(1) NOT NULL default ' ',
  "AAAARLTP" varchar(1) NOT NULL default ' ',
  "AAAARLTQ" varchar(1) NOT NULL default ' ',
  "AAAARLTR" varchar(1) NOT NULL default ' ',
  "AAAARLTS" varchar(1) NOT NULL default ' ',
  "AAAARLTT" varchar(1) NOT NULL default ' ',
  "AAAARLTU" varchar(1) NOT NULL default ' ',
  "AAAARLTV" varchar(1) NOT NULL default ' ',
  "AAAARLTW" varchar(1) NOT NULL default ' ',
  "AAAARLTX" varchar(1) NOT NULL default ' ',
  "AAAARLTY" varchar(1) NOT NULL default ' ',
  "AAAARLTZ" varchar(1) NOT NULL default ' ',
  "AAAARLUA" varchar(1) NOT NULL default ' ',
  "AAAARLUB" varchar(1) NOT NULL default ' ',
  "AAAARLUC" varchar(1) NOT NULL default ' ',
  "AAAARLUD" varchar(1) NOT NULL default ' ',
  "AAAARLUE" varchar(1) NOT NULL default ' ',
  "AAAARLUF" varchar(1) NOT NULL default ' ',
  "AAAARLUG" varchar(1) NOT NULL default ' ',
  "AAAARLUH" varchar(1) NOT NULL default ' ',
  "AAAARLUI" varchar(1) NOT NULL default ' ',
  "AAAARLUJ" varchar(1) NOT NULL default ' ',
  "AAAARLUK" varchar(1) NOT NULL default ' ',
  "AAAARLUL" varchar(1) NOT NULL default ' ',
  "AAABJSLF" varchar(30) NOT NULL default ' ',
  "AAAARLUN" varchar(1) NOT NULL default ' ',
  "AAAARLUO" varchar(1) NOT NULL default ' ',
  "AAAARLUP" varchar(1) NOT NULL default ' ',
  "AAAARLUQ" varchar(1) NOT NULL default ' ',
  "AAABLTUD" varchar(1) NOT NULL default ' ',
  "AAAARLUS" varchar(1) NOT NULL default ' ',
  "AAAARLUT" varchar(1) NOT NULL default ' ',
  "AAAARLUU" varchar(1) NOT NULL default ' ',
  "AAAARLUV" varchar(32) NOT NULL default ' ',
  "AAAARLUW" int(11) NOT NULL default '0',
  "AAAARLUX" varchar(1) NOT NULL default ' ',
  "AAAARLUY" varchar(12) NOT NULL default ' ',
  "AAAARLUZ" varchar(1) NOT NULL default ' ',
  "AAAARLVA" varchar(1) NOT NULL default ' ',
  "AAAARLVB" varchar(6) NOT NULL default '000000',
  "AAAARLVC" varchar(40) NOT NULL default ' ',
  "AAAARLVD" varchar(128) NOT NULL default ' ',
  "AAAARLVE" varchar(1) NOT NULL default ' ',
  "AAAARLVF" varchar(1) NOT NULL default ' ',
  "AAAARLVG" varchar(1) NOT NULL default ' ',
  "AAAARLVH" varchar(1) NOT NULL default ' ',
  "AAAARLVI" varchar(1) NOT NULL default ' ',
  "AAAARLVJ" varchar(1) NOT NULL default ' ',
  "AAAARLVK" varchar(1) NOT NULL default ' ',
  "AAAARLVL" varchar(1) NOT NULL default ' ',
  "AAAARLVM" varchar(1) NOT NULL default ' ',
  "AAAARLVN" varchar(1) NOT NULL default ' ',
  "AAABDNYG" varchar(1) NOT NULL default ' ',
  "AAABLQJP" varchar(1) NOT NULL default ' ',
  "AAAARLVQ" varchar(1) NOT NULL default ' ',
  "AAAARLVR" varchar(1) NOT NULL default ' ',
  "AAAARLVS" varchar(1) NOT NULL default ' ',
  "AAAAUPPD" varchar(1) NOT NULL default ' ',
  "AAAARLVU" varchar(1) NOT NULL default ' ',
  "AAAARLVV" varchar(1) NOT NULL default ' ',
  "AAAARLVW" varchar(1) NOT NULL default ' ',
  "AAAARLVX" varchar(1) NOT NULL default ' ',
  "AAABLENR" varchar(1) NOT NULL default ' ',
  "AAAARLVZ" varchar(1) NOT NULL default ' ',
  "AAAARLWA" varchar(1) NOT NULL default ' ',
  "AAAARLWB" varchar(1) NOT NULL default ' ',
  "AAAARLWC" varchar(1) NOT NULL default ' ',
  "AAAARLWD" varchar(1) NOT NULL default ' ',
  "AAABLFTX" int(11) NOT NULL default '0',
  "AAAARLWF" int(11) NOT NULL default '0',
  "AAAARLWG" varchar(1) NOT NULL default ' ',
  "AAAARLWH" varchar(1) NOT NULL default ' ',
  "AAAARLWI" varchar(1) NOT NULL default ' ',
  "AAAARLWJ" varchar(1) NOT NULL default ' ',
  "AAAARLWK" varchar(1) NOT NULL default ' ',
  "AAAARLWL" varchar(1) NOT NULL default ' ',
  "AAAARLWM" varchar(1) NOT NULL default ' ',
  "AAAARLWN" varchar(1) NOT NULL default ' ',
  "AAAARLWO" varchar(1) NOT NULL default ' ',
  "AAAARLWP" varchar(132) NOT NULL default ' ',
  "AAAARLWQ" varchar(32) NOT NULL default ' ',
  "AAAARLWR" varchar(1) NOT NULL default ' ',
  "AAAARLWS" varchar(40) NOT NULL default ' ',
  "AAAARLWT" varchar(1) NOT NULL default ' ',
  "AAAARLWU" varchar(1) NOT NULL default ' ',
  "AAAARLWV" varchar(1) NOT NULL default ' ',
  "AAAARLWW" varchar(1) NOT NULL default ' ',
  "AAAARLWX" varchar(132) NOT NULL default ' ',
  "AAAARLWY" varchar(132) NOT NULL default ' ',
  "AAABJRGL" varchar(10) NOT NULL default ' ',
  "AAAAVQFA" varchar(132) NOT NULL default ' ',
  "AAAARLXB" varchar(10) NOT NULL default '0000000000',
  "AAAARLXC" varchar(132) NOT NULL default ' ',
  "AAAARLXD" varchar(180) NOT NULL default ' ',
  "AAAARLXE" varchar(1) NOT NULL default ' ',
  "AAAARLXF" varchar(10) NOT NULL default ' ',
  "AAAARLXG" varchar(30) NOT NULL default ' ',
  "AAAARLXH" varchar(1) NOT NULL default ' ',
  "AAAARLXI" varchar(1) NOT NULL default ' ',
  "AAAARLXJ" varchar(1) NOT NULL default ' ',
  "AAAARLXK" varchar(1) NOT NULL default ' ',
  "AAAARLXL" varchar(1) NOT NULL default ' ',
  "AAAARLXM" varchar(1) NOT NULL default ' ',
  "AAAARLXN" varchar(1) NOT NULL default ' ',
  "AAABIJJA" varchar(1) NOT NULL default ' ',
  "AAAARLXP" varchar(1) NOT NULL default ' ',
  "AAAARLXQ" varchar(1) NOT NULL default ' ',
  "AAAARLXR" varchar(1) NOT NULL default ' ',
  "AAAARLXS" varchar(1) NOT NULL default ' ',
  "AAAARLXT" varchar(1) NOT NULL default ' ',
  "AAAARLXU" varchar(1) NOT NULL default ' ',
  "AAAARLXV" varchar(1) NOT NULL default ' ',
  "AAAARLXW" varchar(1) NOT NULL default ' ',
  "AAAARLXX" varchar(255) NOT NULL default ' ',
  "AAAARLXY" varchar(255) NOT NULL default ' ',
  "AAAARLXZ" varchar(255) NOT NULL default ' ',
  "AAAARLYA" varchar(4) NOT NULL default '0000',
  "AAAARLYB" varchar(4) NOT NULL default '0000',
  "AAAARLYC" varchar(4) NOT NULL default '0000',
  "AAAARLYD" varchar(4) NOT NULL default '0000',
  "AAAARLYE" varchar(4) NOT NULL default '0000',
  "AAAARLYF" varchar(4) NOT NULL default '0000',
  "AAAARLYG" varchar(30) NOT NULL default ' ',
  "AAAARLYH" varchar(255) NOT NULL default ' ',
  "AAAARLYI" varchar(40) NOT NULL default ' ',
  "AAAARLYJ" varchar(40) NOT NULL default ' ',
  "AAAARLYK" varchar(15) NOT NULL default ' ',
  "AAAARLYL" tinyint(3) unsigned NOT NULL default '0',
  "AAAARLYM" tinyint(3) unsigned NOT NULL default '0',
  "AAAARLYN" tinyint(3) unsigned NOT NULL default '0',
  "AAAARLYO" varchar(10) NOT NULL default ' ',
  "AAAARLYP" int(11) NOT NULL default '0',
  "AAAARLYQ" int(11) NOT NULL default '0',
  "AAAARLYR" int(11) NOT NULL default '0',
  "AAAARLYS" int(11) NOT NULL default '0',
  "AAAARLYT" int(11) NOT NULL default '0',
  "AAAARLYU" int(11) NOT NULL default '0',
  "AAAARLYV" int(11) NOT NULL default '0',
  "AAAARLYW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARLYY" (
  "AAABLILW" varchar(1) NOT NULL default ' ',
  "AAABGGGL" varchar(8) NOT NULL default ' ',
  "AAAARLZM" varchar(10) NOT NULL default ' ',
  "AAAARLZN" varchar(6) NOT NULL default '000000',
  "AAAARLZO" varchar(6) NOT NULL default '000000',
  "AAABCKCB" varchar(1) NOT NULL default ' ',
  "AAAARLZF" int(11) NOT NULL default '0',
  "AAAARLZG" int(11) NOT NULL default '0',
  "AAAARLZH" int(11) NOT NULL default '0',
  "AAAARLZI" int(11) NOT NULL default '0',
  "AAAARLZJ" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLILW","AAABGGGL","AAAARLZM","AAAARLZN","AAAARLZO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARLZP" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAARXMN" varchar(30) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAAARMAG" varchar(3) NOT NULL default '000',
  "AAABLLIO" varchar(1) NOT NULL default ' ',
  "AAAARLZW" varchar(1) NOT NULL default ' ',
  "AAAARLZX" varchar(1) NOT NULL default ' ',
  "AAABIJZA" varchar(1) NOT NULL default ' ',
  "AAAAROPY" varchar(8) NOT NULL default '00000000',
  "AAAAROPZ" varchar(6) NOT NULL default '000000',
  "AAABIGSL" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHKQ","AAABMQDU","AAAARXMN","AAABIWNN","AAAARMAG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMAH" (
  "AAABLHPU" varchar(40) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHPU","AAABMREL"),
  KEY "AAAARMAM" ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMAO" (
  "AAAARTFE" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABGFLE" int(11) NOT NULL default '0',
  "AAABMPMH" varchar(32) NOT NULL default ' ',
  "AAABMLPK" varchar(64) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARTFE","AAABIWFD","AAABGFLE"),
  KEY "AAAARMAZ" ("AAABMPMH","AAABMLPK","AAABLTJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMBD" (
  "AAAARTFE" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAAARMBJ" varchar(6) NOT NULL default '000000',
  "AAAARMBK" varchar(6) NOT NULL default '000000',
  "AAAARMBL" varchar(32) NOT NULL default ' ',
  "AAAARMBM" varchar(64) NOT NULL default ' ',
  "AAAARMBN" varchar(32) NOT NULL default ' ',
  "AAAARMBO" varchar(64) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARMBU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARTFE","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMBX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARTFE" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAARTFE","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMCF" (
  "AAAARTFE" varchar(30) NOT NULL default ' ',
  "AAABIGIZ" varchar(25) NOT NULL default ' ',
  "AAABGFLE" int(11) NOT NULL default '0',
  "AAABHFAP" varchar(30) NOT NULL default ' ',
  "AAABMPMH" varchar(32) NOT NULL default ' ',
  "AAABMLPK" varchar(64) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAARTFE","AAABIGIZ","AAABGFLE"),
  KEY "AAAARMCR" ("AAAARTFE","AAABHFAP","AAABMPMH","AAABMLPK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMCW" (
  "AAAARTFE" varchar(30) NOT NULL default ' ',
  "AAABGFLE" int(11) NOT NULL default '0',
  "AAABMPMH" varchar(32) NOT NULL default ' ',
  "AAABMLPK" varchar(64) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAARTFE","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMDF" (
  "AAAAVEPQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAVEPQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMDI" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAVEPQ" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAAVEPQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMDO" (
  "AAAARMDX" varchar(8) NOT NULL default ' ',
  "AAABHBVI" varchar(20) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  "AAABLEJW" varchar(20) NOT NULL default ' ',
  "AAAARMDT" varchar(20) NOT NULL default ' ',
  "AAAAVDMX" varchar(10) NOT NULL default ' ',
  "AAAAZZKK" varchar(20) NOT NULL default ' ',
  "AAABAUSZ" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAAARMDX","AAABHBVI","AAABMOHM","AAABLEJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMEB" (
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARMEE" varchar(40) NOT NULL default ' ',
  "AAAARMEF" decimal(15,0) NOT NULL default '0',
  "AAAARMEG" varchar(25) NOT NULL default ' ',
  "AAAARMEH" varchar(40) NOT NULL default ' ',
  "AAAARMEI" decimal(15,0) NOT NULL default '0',
  "AAAARMEJ" varchar(25) NOT NULL default ' ',
  "AAAARMEK" varchar(40) NOT NULL default ' ',
  "AAAARMEL" decimal(15,0) NOT NULL default '0',
  "AAAARMEM" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMEO" (
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(45) NOT NULL default ' ',
  "AAABMNNA" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABFTZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMEV" (
  "AAABFNVZ" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(61) NOT NULL default ' ',
  "AAABFIGO" varchar(1) NOT NULL default ' ',
  "AAABMMRE" longtext,
  PRIMARY KEY ("AAABFNVZ","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMFD" (
  "AAABFNVZ" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABMMRH" varchar(5) NOT NULL default '00000',
  "AAABGBKQ" varchar(15) NOT NULL default ' ',
  "AAAATWHI" varchar(61) NOT NULL default ' ',
  "AAABMNMW" varchar(2) NOT NULL default '00',
  "AAAAUNUN" varchar(5) NOT NULL default '00000',
  "AAABMOHM" varchar(61) NOT NULL default ' ',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFNVZ","AAABIWFD","AAABMMRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMFR" (
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABIWMM" varchar(32) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWNN","AAABIWMM","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMFY" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABIWMM" varchar(32) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTO" varchar(40) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIWNN","AAABIWMM","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMGK" (
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAABJBGU" varchar(4) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAATJNW","AAABJBGU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMGQ" (
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAABJBGU" varchar(4) NOT NULL default ' ',
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAATJNW","AAABJBGU","AAABFTZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMGY" (
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAAARYGG" varchar(4) NOT NULL default ' ',
  "AAAARXMN" varchar(30) NOT NULL default ' ',
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAAARNMF" varchar(1) NOT NULL default ' ',
  "AAABCGEQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATJNW","AAAARYGG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMHH" (
  "AAABIVVH" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIJMG" varchar(1) NOT NULL default ' ',
  "AAABIWLU" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default ' ',
  "AAABLRXO" varchar(8) NOT NULL default '00000000',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABLRXN" varchar(8) NOT NULL default '00000000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABHEYG" varchar(30) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARMHW" varchar(16) NOT NULL default ' ',
  "AAAARVMT" varchar(1) NOT NULL default ' ',
  "AAAARVMX" varchar(1) NOT NULL default ' ',
  "AAAARMHZ" varchar(2) NOT NULL default '00',
  "AAAARMIA" varchar(2) NOT NULL default '00',
  "AAAARMIB" varchar(8) NOT NULL default '00000000',
  "AAAARMIC" varchar(1) NOT NULL default '0',
  "AAAARMID" varchar(1) NOT NULL default ' ',
  "AAABIWFT" varchar(1) NOT NULL default ' ',
  "AAABIWFK" varchar(1) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAABBLME" varchar(1) NOT NULL default ' ',
  "AAAARMIJ" varchar(1) NOT NULL default ' ',
  "AAAARMIK" varchar(1) NOT NULL default ' ',
  "AAAATWYP" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABIVVH","AAABIWFD"),
  UNIQUE KEY "AAAARMIO" ("AAABIWFD","AAABIJMG","AAABIWLU","AAABLTXI","AAABLRXO","AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMIV" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIVVH" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTO" varchar(40) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIVVH","AAABIWFD"),
  KEY "AAAARMJF" ("AAABIVVH","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMJI" (
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAABGFLE" varchar(10) NOT NULL default ' ',
  "AAAARYHM" varchar(30) NOT NULL default ' ',
  "AAAARYHN" varchar(30) NOT NULL default ' ',
  "AAABIWLU" varchar(30) NOT NULL default ' ',
  "AAAARSHR" varchar(3) NOT NULL default ' ',
  "AAAARSHS" varchar(8) NOT NULL default '00000000',
  "AAAARSHT" varchar(4) NOT NULL default ' ',
  "AAAARSHU" varchar(8) NOT NULL default '00000000',
  "AAAAROFH" varchar(1) NOT NULL default ' ',
  "AAAAROFI" varchar(30) NOT NULL default ' ',
  "AAAARXCK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYNB","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMJX" (
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIWMM" varchar(32) NOT NULL default ' ',
  "AAAARMKB" varchar(30) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTO" varchar(40) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYNB","AAABMLQA","AAABIWMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMKI" (
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAABIVVF" varchar(8) NOT NULL default '00000000',
  "AAAARVNX" varchar(32) NOT NULL default ' ',
  "AAAARVNY" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYNB","AAABIVVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMKP" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIJMG" varchar(1) NOT NULL default ' ',
  "AAABIWLU" varchar(30) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTO" varchar(40) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIJMG","AAABIWLU","AAABIWNN","AAABLTQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMLD" (
  "AAABIVVH" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTO" varchar(40) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABIVVH","AAABIWFD","AAABMLQA","AAABLTQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMLP" (
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
  "AAABLRXN" varchar(8) NOT NULL default '00000000',
  "AAABLRXO" varchar(8) NOT NULL default '00000000',
  "AAAARMMC" varchar(1) NOT NULL default ' ',
  "AAABDPTM" varchar(5) NOT NULL default '00000',
  "AAABDPTN" varchar(5) NOT NULL default '00000',
  "AAABLTQJ" varchar(2) NOT NULL default ' ',
  "AAAARMMG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIVVH","AAABIWFD","AAABIVVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMMK" (
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAABIVVF" varchar(8) NOT NULL default '00000000',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABIWMM" varchar(32) NOT NULL default ' ',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  "AAABMRGO" varchar(1) NOT NULL default ' ',
  "AAABLECU" varchar(8) NOT NULL default '00000000',
  "AAABLECV" varchar(8) NOT NULL default '00000000',
  "AAABIVVC" varchar(8) NOT NULL default '00000000',
  "AAABLRXN" varchar(8) NOT NULL default '00000000',
  "AAABLRXO" varchar(8) NOT NULL default '00000000',
  "AAAAROMX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYNB","AAABIVVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMMZ" (
  "AAABFMTA" varchar(30) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAAARMNC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFMTA","AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMNF" (
  "AAABIVVH" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWMM" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABIVVH","AAABIWFD","AAABIWMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMNM" (
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTO" varchar(40) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYNB","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMNU" (
  "AAAARRLA" varchar(2) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARRLA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMNY" (
  "AAAARVLG" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(10) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAAARMUH" int(11) NOT NULL default '0',
  "AAAARMUI" int(11) NOT NULL default '0',
  "AAAARMQP" varchar(1) NOT NULL default ' ',
  "AAABFORA" varchar(8) NOT NULL default '00000000',
  "AAABLESZ" varchar(6) NOT NULL default '000000',
  "AAABDEFP" varchar(8) NOT NULL default '00000000',
  "AAABDEFQ" varchar(6) NOT NULL default '000000',
  "AAAARWTL" varchar(32) NOT NULL default ' ',
  "AAABCROU" varchar(8) NOT NULL default ' ',
  "AAABCRSF" varchar(1) NOT NULL default ' ',
  "AAABLESX" varchar(32) NOT NULL default ' ',
  "AAABLESY" varchar(64) NOT NULL default ' ',
  "AAAARWTQ" varchar(1) NOT NULL default ' ',
  "AAABCRXB" varchar(2) NOT NULL default '00',
  "AAABCRXC" varchar(2) NOT NULL default '00',
  "AAABCRXD" varchar(3) NOT NULL default '000',
  "AAABCRXE" varchar(2) NOT NULL default '00',
  "AAABCRXF" varchar(2) NOT NULL default '00',
  "AAABCRXG" varchar(1) NOT NULL default ' ',
  "AAABGHVC" varchar(20) NOT NULL default ' ',
  "AAABHHSJ" varchar(2) NOT NULL default ' ',
  "AAABCRXN" int(11) NOT NULL default '0',
  "AAABCRXR" int(11) NOT NULL default '0',
  "AAAARWUB" varchar(1) NOT NULL default ' ',
  "AAABCRSH" varchar(1) NOT NULL default ' ',
  "AAAARWUD" varchar(2) NOT NULL default '00',
  "AAAARWUE" varchar(2) NOT NULL default '00',
  "AAAARWUF" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAARVLG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMPI" (
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAAARVLG" varchar(30) NOT NULL default ' ',
  "AAAARMPV" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARMPN" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(10) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAAARVOH" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLTJH","AAAARVLG","AAAARMPV"),
  KEY "AAAARMPW" ("AAAARVLG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMPY" (
  "AAAARVLG" varchar(30) NOT NULL default ' ',
  "AAAARVOH" int(11) NOT NULL default '0',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAAARVLG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMQG" (
  "AAAARVLG" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(10) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAAARMUH" int(11) NOT NULL default '0',
  "AAAARMUI" int(11) NOT NULL default '0',
  "AAAARMQP" varchar(1) NOT NULL default ' ',
  "AAABFORA" varchar(8) NOT NULL default '00000000',
  "AAABLESZ" varchar(6) NOT NULL default '000000',
  "AAABDEFP" varchar(8) NOT NULL default '00000000',
  "AAABDEFQ" varchar(6) NOT NULL default '000000',
  "AAAARWTL" varchar(32) NOT NULL default ' ',
  "AAABCROU" varchar(8) NOT NULL default ' ',
  "AAABCRSF" varchar(1) NOT NULL default ' ',
  "AAABLESX" varchar(32) NOT NULL default ' ',
  "AAABLESY" varchar(64) NOT NULL default ' ',
  "AAAARWTQ" varchar(1) NOT NULL default ' ',
  "AAABCRXB" varchar(2) NOT NULL default '00',
  "AAABCRXC" varchar(2) NOT NULL default '00',
  "AAABCRXD" varchar(3) NOT NULL default '000',
  "AAABCRXE" varchar(2) NOT NULL default '00',
  "AAABCRXF" varchar(2) NOT NULL default '00',
  "AAABCRXG" varchar(1) NOT NULL default ' ',
  "AAABGHVC" varchar(20) NOT NULL default ' ',
  "AAABHHSJ" varchar(2) NOT NULL default ' ',
  "AAABCRXN" int(11) NOT NULL default '0',
  "AAABCRXR" int(11) NOT NULL default '0',
  "AAAARWUB" varchar(1) NOT NULL default ' ',
  "AAABCRSH" varchar(1) NOT NULL default ' ',
  "AAAARWUD" varchar(2) NOT NULL default '00',
  "AAAARWUE" varchar(2) NOT NULL default '00',
  "AAAARWUF" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAARVLG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMRQ" (
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAAARVLG" varchar(30) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAAARVJO" int(11) NOT NULL default '0',
  "AAAARVJP" varchar(1) NOT NULL default ' ',
  "AAAARMRW" decimal(15,0) NOT NULL default '0',
  "AAAARMRX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYNB","AAAARVLG"),
  KEY "AAAARMSA" ("AAAARVLG","AAABMKFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMSD" (
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAAARVLG" varchar(30) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAAARVJO" int(11) NOT NULL default '0',
  "AAAARVJP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYNB","AAAARVLG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMSL" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMSO" (
  "AAAARVLG" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(10) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAAARMSV" varchar(1) NOT NULL default ' ',
  "AAABFORA" varchar(8) NOT NULL default '00000000',
  "AAABLESZ" varchar(6) NOT NULL default '000000',
  "AAABDEFP" varchar(8) NOT NULL default '00000000',
  "AAABDEFQ" varchar(6) NOT NULL default '000000',
  "AAAARWTL" varchar(32) NOT NULL default ' ',
  "AAABCROU" varchar(8) NOT NULL default ' ',
  "AAABCRSF" varchar(1) NOT NULL default ' ',
  "AAABLESX" varchar(32) NOT NULL default ' ',
  "AAABLESY" varchar(64) NOT NULL default ' ',
  "AAAARWTQ" varchar(1) NOT NULL default ' ',
  "AAABCRXB" varchar(2) NOT NULL default '00',
  "AAABCRXC" varchar(2) NOT NULL default '00',
  "AAABCRXD" varchar(3) NOT NULL default '000',
  "AAABCRXE" varchar(2) NOT NULL default '00',
  "AAABCRXF" varchar(2) NOT NULL default '00',
  "AAABCRXG" varchar(1) NOT NULL default ' ',
  "AAABGHVC" varchar(20) NOT NULL default ' ',
  "AAABHHSJ" varchar(2) NOT NULL default ' ',
  "AAABCRXN" int(11) NOT NULL default '0',
  "AAABCRXR" int(11) NOT NULL default '0',
  "AAAARWUB" varchar(1) NOT NULL default ' ',
  "AAABCRSH" varchar(1) NOT NULL default ' ',
  "AAAARWUD" varchar(2) NOT NULL default '00',
  "AAAARWUE" varchar(2) NOT NULL default '00',
  "AAAARWUF" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAARVLG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMTW" (
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAAARVLG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTJH","AAAARYNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMUC" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABLUFA" varchar(10) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAARMUH" int(11) NOT NULL default '0',
  "AAAARMUI" int(11) NOT NULL default '0',
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(10) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  "AAAARMUT" varchar(100) NOT NULL default ' ',
  "AAABIFQE" varchar(8) NOT NULL default '00000000',
  "AAABIFQF" varchar(6) NOT NULL default '000000',
  "AAAARMUW" decimal(15,0) NOT NULL default '0',
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  "AAABMOIB" varchar(30) NOT NULL default ' ',
  "AAABHFAP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHKQ","AAABMKFU","AAABLUFA"),
  KEY "AAAARMVD" ("AAAARYNB"),
  KEY "AAAARMVF" ("AAABMOHM","AAABMOIB","AAABHFAP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMVJ" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAAARMVL" varchar(3) NOT NULL default '000',
  "AAAARMVM" varchar(3) NOT NULL default '000',
  "AAAARMVN" varchar(1) NOT NULL default ' ',
  "AAAARMVO" varchar(3) NOT NULL default '000',
  "AAAARMVP" varchar(3) NOT NULL default '000',
  "AAAARMVQ" varchar(1) NOT NULL default ' ',
  "AAAARMVR" varchar(32) NOT NULL default ' ',
  "AAAARMVS" varchar(64) NOT NULL default ' ',
  "AAAARMVT" varchar(32) NOT NULL default ' ',
  "AAAARMVU" varchar(64) NOT NULL default ' ',
  "AAAARMVV" varchar(32) NOT NULL default ' ',
  "AAAARMVW" varchar(64) NOT NULL default ' ',
  "AAAARMVX" varchar(32) NOT NULL default ' ',
  "AAAARMVY" varchar(64) NOT NULL default ' ',
  "AAAARMVZ" varchar(32) NOT NULL default ' ',
  "AAAARMWA" varchar(64) NOT NULL default ' ',
  "AAAARMWB" varchar(32) NOT NULL default ' ',
  "AAAARMWC" varchar(64) NOT NULL default ' ',
  "AAAARMWD" varchar(32) NOT NULL default ' ',
  "AAAARMWE" varchar(64) NOT NULL default ' ',
  "AAAARMWF" varchar(32) NOT NULL default ' ',
  "AAAARMWG" varchar(64) NOT NULL default ' ',
  "AAAARMWH" varchar(32) NOT NULL default ' ',
  "AAAARMWI" varchar(64) NOT NULL default ' ',
  "AAAARMWJ" varchar(32) NOT NULL default ' ',
  "AAAARMWK" varchar(64) NOT NULL default ' ',
  "AAAARMWL" varchar(32) NOT NULL default ' ',
  "AAAARMWM" varchar(64) NOT NULL default ' ',
  "AAAARMWN" varchar(32) NOT NULL default ' ',
  "AAAARMWO" varchar(64) NOT NULL default ' ',
  "AAAARMWP" varchar(32) NOT NULL default ' ',
  "AAAARMWQ" varchar(64) NOT NULL default ' ',
  "AAAARMWR" varchar(32) NOT NULL default ' ',
  "AAAARMWS" varchar(64) NOT NULL default ' ',
  "AAAARMWT" varchar(32) NOT NULL default ' ',
  "AAAARMWU" varchar(64) NOT NULL default ' ',
  "AAAARMWV" varchar(32) NOT NULL default ' ',
  "AAAARMWW" varchar(64) NOT NULL default ' ',
  "AAAARMWX" varchar(32) NOT NULL default ' ',
  "AAAARMWY" varchar(64) NOT NULL default ' ',
  "AAAARMWZ" varchar(32) NOT NULL default ' ',
  "AAAARMXA" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMXC" (
  "AAAARVLG" varchar(30) NOT NULL default ' ',
  "AAAARMYM" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(10) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAAARMXH" varchar(1) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAABJDRC" varchar(40) NOT NULL default ' ',
  "AAABFORA" varchar(8) NOT NULL default '00000000',
  "AAABLESZ" varchar(6) NOT NULL default '000000',
  "AAABDEFP" varchar(8) NOT NULL default '00000000',
  "AAABDEFQ" varchar(6) NOT NULL default '000000',
  "AAAARWTL" varchar(32) NOT NULL default ' ',
  "AAABCROU" varchar(8) NOT NULL default ' ',
  "AAABCRSF" varchar(1) NOT NULL default ' ',
  "AAABLESX" varchar(32) NOT NULL default ' ',
  "AAABLESY" varchar(64) NOT NULL default ' ',
  "AAAARWTQ" varchar(1) NOT NULL default ' ',
  "AAABCRXB" varchar(2) NOT NULL default '00',
  "AAABCRXC" varchar(2) NOT NULL default '00',
  "AAABCRXD" varchar(3) NOT NULL default '000',
  "AAABCRXE" varchar(2) NOT NULL default '00',
  "AAABCRXF" varchar(2) NOT NULL default '00',
  "AAABCRXG" varchar(1) NOT NULL default ' ',
  "AAABGHVC" varchar(20) NOT NULL default ' ',
  "AAABHHSJ" varchar(2) NOT NULL default ' ',
  "AAABCRXN" int(11) NOT NULL default '0',
  "AAABCRXR" int(11) NOT NULL default '0',
  "AAAARWUB" varchar(1) NOT NULL default ' ',
  "AAABCRSH" varchar(1) NOT NULL default ' ',
  "AAAARWUD" varchar(2) NOT NULL default '00',
  "AAAARWUE" varchar(2) NOT NULL default '00',
  "AAAARWUF" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAARVLG","AAAARMYM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMYN" (
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAAARMZA" varchar(1) NOT NULL default ' ',
  "AAABGFLE" varchar(8) NOT NULL default '00000000',
  "AAABIWVY" varchar(16) NOT NULL default '0000000000000000',
  "AAAAROVJ" varchar(30) NOT NULL default ' ',
  "AAAARMYT" varchar(2) NOT NULL default ' ',
  "AAAARYEC" varchar(6) NOT NULL default '000000',
  "AAAARMYV" varchar(6) NOT NULL default '000000',
  "AAAARPBZ" varchar(1) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAARYNB","AAAARMZA","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMZC" (
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABILZK" varchar(40) NOT NULL default ' ',
  "AAABLJIM" varchar(2) NOT NULL default ' ',
  "AAABLECD" varchar(1) NOT NULL default ' ',
  "AAABENHV" varchar(1) NOT NULL default ' ',
  "AAABENHX" varchar(1) NOT NULL default ' ',
  "AAAARMZJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKGJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMZL" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABIWNE" varchar(30) NOT NULL default ' ',
  "AAAAUZYK" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABIWNE","AAAAUZYK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARMZU" (
  "AAAARNBA" varchar(4) NOT NULL default ' ',
  "AAAARNBC" varchar(4) NOT NULL default ' ',
  "AAAARMZX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARNBA","AAAARNBC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNAA" (
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAAARNAC" varchar(30) NOT NULL default ' ',
  "AAAARNAD" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(8) NOT NULL default ' ',
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNAH" (
  "AAAARNBA" varchar(4) NOT NULL default ' ',
  "AAAARNBC" varchar(4) NOT NULL default ' ',
  "AAAARNAR" varchar(1) NOT NULL default ' ',
  "AAAARNAS" varchar(3) NOT NULL default ' ',
  "AAAARNAT" varchar(2) NOT NULL default ' ',
  "AAAARNAN" varchar(4) NOT NULL default ' ',
  "AAAARNAO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARNBA","AAAARNBC","AAAARNAR","AAAARNAS","AAAARNAT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNAU" (
  "AAAARNBA" varchar(4) NOT NULL default ' ',
  "AAAARNBB" varchar(4) NOT NULL default ' ',
  "AAAARNBC" varchar(4) NOT NULL default ' ',
  "AAAARNAY" varchar(30) NOT NULL default ' ',
  "AAAARNAZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAARNBA","AAAARNBB","AAAARNBC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNBD" (
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABFYWO" varchar(30) NOT NULL default ' ',
  "AAAARYFH" varchar(30) NOT NULL default ' ',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABIWGC" varchar(10) NOT NULL default ' ',
  "AAAARNKV" varchar(1) NOT NULL default ' ',
  "AAABMFSI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIANB","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNBS" (
  "AAABLECD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNBV" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLECD" varchar(2) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNCC" (
  "AAAARNDL" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  "AAAAWYIH" varchar(30) NOT NULL default ' ',
  "AAABIWGC" varchar(10) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAAARTMM" varchar(12) NOT NULL default ' ',
  "AAAARNCQ" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAARNDL","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNCT" (
  "AAAARNDL" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABLFWZ" longtext,
  PRIMARY KEY ("AAAARNDL","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNCZ" (
  "AAAARNDL" varchar(30) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARNDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNDE" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARNDL" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAARNDL","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNDN" (
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIANB","AAABIWFD","AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNDU" (
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARYGG" varchar(4) NOT NULL default ' ',
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABLBEK" varchar(1) NOT NULL default ' ',
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  "AAABFTZK" varchar(30) NOT NULL default ' ',
  "AAABLDNU" varchar(4) NOT NULL default ' ',
  "AAABJTEQ" varchar(6) NOT NULL default '000000',
  "AAABLBEI" varchar(6) NOT NULL default '000000',
  "AAABLUWW" varchar(1) NOT NULL default ' ',
  "AAABHVRR" varchar(5) NOT NULL default ' ',
  "AAABMNMM" varchar(6) NOT NULL default '000000',
  "AAAARYDA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARZRE","AAAARYGG","AAABIANB","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNEO" (
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABLTVR" varchar(30) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMLPV","AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNEV" (
  "AAABLGEF" varchar(20) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABLPVX" varchar(2) NOT NULL default '00',
  "AAAARNEZ" varchar(30) NOT NULL default ' ',
  "AAAARNFA" varchar(30) NOT NULL default ' ',
  "AAAARTMM" varchar(12) NOT NULL default ' ',
  "AAAARTMN" decimal(15,0) NOT NULL default '0',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLGEF","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNFH" (
  "AAABLGEF" varchar(20) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABLECD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEF","AAABIWFD","AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNFO" (
  "AAABLGEF" varchar(20) NOT NULL default ' ',
  "AAABJGGK" varchar(2) NOT NULL default ' ',
  "AAABLECD" varchar(2) NOT NULL default ' ',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEF","AAABJGGK","AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNFW" (
  "AAABLGEF" varchar(20) NOT NULL default ' ',
  "AAABJGGK" varchar(2) NOT NULL default ' ',
  "AAABLECD" varchar(2) NOT NULL default ' ',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEF","AAABJGGK","AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNGE" (
  "AAABLGEF" varchar(20) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABKWWJ" longblob,
  PRIMARY KEY ("AAABLGEF","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNGK" (
  "AAABLGEF" varchar(20) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARNGR" varchar(30) NOT NULL default ' ',
  "AAAARNGO" longtext,
  PRIMARY KEY ("AAABLGEF","AAABIWFD","AAAARNGR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNGS" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLGEF" varchar(20) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABKGKQ" varchar(5) NOT NULL default '00000',
  "AAAARNHE" varchar(40) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLGEF","AAABIWFD","AAABKGKQ","AAAARNHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNHF" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLGEF" varchar(20) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLGEF","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNHO" (
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAAARWYH" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABIANB","AAAARWYH","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNHY" (
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABLBEK" varchar(1) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  "AAABFTZK" varchar(30) NOT NULL default ' ',
  "AAABLDNU" varchar(4) NOT NULL default ' ',
  "AAABJTEQ" varchar(6) NOT NULL default '000000',
  "AAABLBEI" varchar(6) NOT NULL default '000000',
  "AAABLUWW" varchar(1) NOT NULL default ' ',
  "AAABHVRR" varchar(5) NOT NULL default ' ',
  "AAABMNMM" varchar(6) NOT NULL default '000000',
  "AAAARYDA" varchar(30) NOT NULL default ' ',
  "AAABMLPN" varchar(1) NOT NULL default ' ',
  "AAABLMVF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIANB","AAABMQDU","AAABIWFD","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNIU" (
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARYGH" varchar(27) NOT NULL default ' ',
  "AAABHFPL" varchar(1) NOT NULL default ' ',
  "AAAARSIE" varchar(1) NOT NULL default ' ',
  "AAAARSLL" varchar(1) NOT NULL default ' ',
  "AAAARNNQ" varchar(1) NOT NULL default ' ',
  "AAAARRTW" varchar(1) NOT NULL default ' ',
  "AAAARRTX" varchar(1) NOT NULL default ' ',
  "AAAARNJF" varchar(1) NOT NULL default ' ',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAABCGEQ" varchar(1) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAAARRTU" varchar(50) NOT NULL default ' ',
  "AAAARRTZ" varchar(27) NOT NULL default ' ',
  "AAAARRUA" varchar(1) NOT NULL default ' ',
  "AAABFYWO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIANB","AAABMQDU","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNJR" (
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIANB","AAABMQDU","AAABIWFD","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNKB" (
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAAARYGG" varchar(4) NOT NULL default ' ',
  "AAAARRLA" varchar(2) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATJNW","AAAARYGG","AAAARRLA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNKJ" (
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAAARYGH" varchar(27) NOT NULL default ' ',
  "AAAARNMF" varchar(1) NOT NULL default ' ',
  "AAABCGEQ" varchar(1) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAAARNKV" varchar(1) NOT NULL default ' ',
  "AAABMFSI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATJNW","AAABMQDU","AAABIWFD"),
  UNIQUE KEY "AAAARNLA" ("AAABMQDU","AAABIWFD","AAAARNMF","AAAATJNW"),
  UNIQUE KEY "AAAARNLF" ("AAABIANB","AAABMQDU","AAABIWFD","AAAATJNW"),
  UNIQUE KEY "AAAARNLK" ("AAAARNMF","AAABMQDU","AAABIWFD","AAAATJNW"),
  UNIQUE KEY "AAAARNLP" ("AAABCGEQ","AAABMQDU","AAABIWFD","AAAATJNW"),
  UNIQUE KEY "AAAARNLU" ("AAABIWFD","AAAATJNW","AAABMQDU"),
  KEY "AAAARNLY" ("AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNMA" (
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAAARYGG" varchar(4) NOT NULL default ' ',
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAAARYGH" varchar(1) NOT NULL default ' ',
  "AAAARNMF" varchar(1) NOT NULL default ' ',
  "AAABCGEQ" varchar(1) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAATJNW","AAAARYGG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNML" (
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAAARYGG" varchar(4) NOT NULL default ' ',
  "AAAARRLG" varchar(3) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATJNW","AAAARYGG","AAAARRLG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNMT" (
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARYGG" varchar(4) NOT NULL default ' ',
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARZRE","AAAARYGG","AAABIANB","AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNNC" (
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARYGG" varchar(4) NOT NULL default ' ',
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABFYWO" varchar(30) NOT NULL default ' ',
  "AAAARYFH" varchar(30) NOT NULL default ' ',
  "AAAARYGH" varchar(27) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAAARRTU" varchar(50) NOT NULL default ' ',
  "AAAARRTZ" varchar(27) NOT NULL default ' ',
  "AAAARRUA" varchar(1) NOT NULL default ' ',
  "AAABHFPL" varchar(1) NOT NULL default ' ',
  "AAAARSIE" varchar(1) NOT NULL default ' ',
  "AAAARNNQ" varchar(1) NOT NULL default ' ',
  "AAAARRTW" varchar(1) NOT NULL default ' ',
  "AAAARRTX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARZRE","AAAARYGG","AAABIANB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNNW" (
  "AAABLPVX" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLPVX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNNZ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPVX" varchar(2) NOT NULL default '00',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLPVX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNOG" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTO" varchar(40) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIANB","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNOQ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARYGG" varchar(4) NOT NULL default ' ',
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTO" varchar(40) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAARZRE","AAAARYGG","AAABIANB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNPC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABINIK" decimal(15,0) NOT NULL default '0',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNPP" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNQA" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABJLTX" varchar(42) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAAARNQE" varchar(64) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAARNQG" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABJLTX","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNQM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABINIK" decimal(15,0) NOT NULL default '0',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNQZ" (
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAAARNRB" varchar(6) NOT NULL default '000000',
  "AAAARNRC" varchar(6) NOT NULL default '000000',
  "AAAARNRD" varchar(6) NOT NULL default '000000',
  "AAAARNRE" varchar(6) NOT NULL default '000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNRH" (
  "AAAARYFH" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  "AAAARNRM" varchar(30) NOT NULL default ' ',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAABLFWZ" varchar(6) NOT NULL default ' ',
  "AAAARNRP" varchar(1) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAARYFH","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNRV" (
  "AAAARYFH" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  "AAABFTZK" varchar(30) NOT NULL default ' ',
  "AAABLDNU" varchar(4) NOT NULL default ' ',
  "AAABJTEQ" varchar(6) NOT NULL default '000000',
  "AAABLBEI" varchar(6) NOT NULL default '000000',
  "AAABLUWW" varchar(1) NOT NULL default ' ',
  "AAABHVRR" varchar(5) NOT NULL default ' ',
  "AAABMNMM" varchar(6) NOT NULL default '000000',
  "AAAARYDA" varchar(30) NOT NULL default ' ',
  "AAAARNSJ" varchar(1) NOT NULL default ' ',
  "AAAARNSK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYFH","AAABIWFD","AAABIHLK"),
  KEY "AAAARNSO" ("AAABIWNN","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNSR" (
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARYFH" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  "AAABFTZK" varchar(30) NOT NULL default ' ',
  "AAABLDNU" varchar(4) NOT NULL default ' ',
  "AAABJTEQ" varchar(6) NOT NULL default '000000',
  "AAABLBEI" varchar(6) NOT NULL default '000000',
  "AAABLUWW" varchar(1) NOT NULL default ' ',
  "AAABHVRR" varchar(5) NOT NULL default ' ',
  "AAABMNMM" varchar(6) NOT NULL default '000000',
  "AAAARYDA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARZRE","AAAARYFH","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNTI" (
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARYFH" varchar(30) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAARZRE","AAAARYFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNTP" (
  "AAAARYHM" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARNTS" varchar(2) NOT NULL default '00',
  "AAAARNTT" varchar(2) NOT NULL default '00',
  "AAAARNTU" varchar(2) NOT NULL default '00',
  "AAAARNTV" varchar(2) NOT NULL default '00',
  "AAAARNTW" varchar(2) NOT NULL default '00',
  "AAAARNTX" varchar(6) NOT NULL default '000000',
  "AAAARNTY" varchar(4) NOT NULL default '0000',
  "AAAARNTZ" varchar(4) NOT NULL default '0000',
  "AAAARNUA" varchar(3) NOT NULL default '000',
  "AAAARNUB" varchar(2) NOT NULL default '00',
  "AAAARNUC" varchar(4) NOT NULL default '0000',
  "AAAARNUD" varchar(1) NOT NULL default ' ',
  "AAAARNUE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYHM","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNUH" (
  "AAAARYHM" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARNWM" varchar(30) NOT NULL default ' ',
  "AAABGFLE" int(11) NOT NULL default '0',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(45) NOT NULL default ' ',
  "AAABMNNA" varchar(45) NOT NULL default ' ',
  "AAABMNLA" varchar(1) NOT NULL default ' ',
  "AAABHTVO" varchar(1) NOT NULL default ' ',
  "AAAARNVY" varchar(1) NOT NULL default ' ',
  "AAABLPQZ" varchar(1) NOT NULL default ' ',
  "AAAARNWA" varchar(1) NOT NULL default ' ',
  "AAAARNWB" varchar(1) NOT NULL default ' ',
  "AAAARNWC" varchar(1) NOT NULL default ' ',
  "AAAARNUZ" varchar(1) NOT NULL default ' ',
  "AAAARNWD" varchar(1) NOT NULL default ' ',
  "AAABMPMV" varchar(1) NOT NULL default ' ',
  "AAAARNWF" varchar(2) NOT NULL default '00',
  "AAAARNWG" varchar(1) NOT NULL default ' ',
  "AAABLZMT" varchar(1) NOT NULL default '0',
  "AAAARNWI" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(80) NOT NULL default ' ',
  "AAAARNWK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYHM","AAABIWFD","AAAARNWM","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNVM" (
  "AAAARYHM" varchar(30) NOT NULL default ' ',
  "AAAARNWM" varchar(30) NOT NULL default ' ',
  "AAABGFLE" int(11) NOT NULL default '0',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(45) NOT NULL default ' ',
  "AAABMNNA" varchar(45) NOT NULL default ' ',
  "AAABMNLA" varchar(1) NOT NULL default ' ',
  "AAABHTVO" varchar(1) NOT NULL default ' ',
  "AAAARNVY" varchar(1) NOT NULL default ' ',
  "AAABLPQZ" varchar(1) NOT NULL default ' ',
  "AAAARNWA" varchar(1) NOT NULL default ' ',
  "AAAARNWB" varchar(1) NOT NULL default ' ',
  "AAAARNWC" varchar(1) NOT NULL default ' ',
  "AAAARNWD" varchar(1) NOT NULL default ' ',
  "AAABMPMV" varchar(1) NOT NULL default ' ',
  "AAAARNWF" varchar(2) NOT NULL default '00',
  "AAAARNWG" varchar(1) NOT NULL default ' ',
  "AAABLZMT" varchar(1) NOT NULL default '0',
  "AAAARNWI" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(80) NOT NULL default ' ',
  "AAAARNWK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYHM","AAAARNWM","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNWO" (
  "AAAARYHM" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABLEST" varchar(12) NOT NULL default ' ',
  "AAABLUZL" varchar(30) NOT NULL default ' ',
  "AAABMMVU" varchar(1) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAROJW" varchar(30) NOT NULL default ' ',
  "AAAAROJX" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAAARXCR" varchar(1) NOT NULL default ' ',
  "AAABHRXA" varchar(1) NOT NULL default ' ',
  "AAAARXDF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYHM","AAABIWFD"),
  KEY "AAAARNXK" ("AAABMQDU","AAAATJNW"),
  KEY "AAAARNXN" ("AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNXP" (
  "AAAARYHM" varchar(30) NOT NULL default ' ',
  "AAAARYGG" varchar(4) NOT NULL default ' ',
  "AAABLEST" varchar(12) NOT NULL default ' ',
  "AAABLUZL" varchar(30) NOT NULL default ' ',
  "AAABMMVU" varchar(1) NOT NULL default ' ',
  "AAAAROJW" varchar(30) NOT NULL default ' ',
  "AAAAROJX" varchar(1) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAAARXCR" varchar(1) NOT NULL default ' ',
  "AAABHRXA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYHM"),
  KEY "AAAARNYG" ("AAAARYGG","AAABLUZL","AAAATJNW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNYK" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARYHM" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABLEST" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAARYHM","AAABIWFD"),
  KEY "AAAARNYT" ("AAAARYHM","AAABIWFD"),
  KEY "AAAARNYW" ("AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNYY" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARYHM" varchar(30) NOT NULL default ' ',
  "AAABLEST" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAARYHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNZF" (
  "AAAARYHM" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABGFLE" int(11) NOT NULL default '0',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABMMRH" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYHM","AAABIWFD","AAABMREF","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNZQ" (
  "AAAARYHM" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABGFLE" int(11) NOT NULL default '0',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABMMRH" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYHM","AAABMREF","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARNZZ" (
  "AAAARYHM" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABGFLE" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(45) NOT NULL default ' ',
  "AAABMNNA" varchar(45) NOT NULL default ' ',
  "AAABMNLA" varchar(1) NOT NULL default ' ',
  "AAAARYHO" varchar(30) NOT NULL default ' ',
  "AAAARYHP" varchar(1) NOT NULL default ' ',
  "AAABIOAH" varchar(1) NOT NULL default ' ',
  "AAAARYYV" varchar(1) NOT NULL default ' ',
  "AAAAWNFG" varchar(1) NOT NULL default ' ',
  "AAAARYYU" varchar(2) NOT NULL default ' ',
  "AAAARXAT" varchar(1) NOT NULL default ' ',
  "AAABIWLU" varchar(30) NOT NULL default ' ',
  "AAAARSHR" varchar(3) NOT NULL default ' ',
  "AAAARSHS" varchar(8) NOT NULL default '00000000',
  "AAAARSHU" varchar(8) NOT NULL default '00000000',
  "AAAARSHT" varchar(4) NOT NULL default ' ',
  "AAABHTVO" varchar(1) NOT NULL default ' ',
  "AAAARXAG" varchar(45) NOT NULL default ' ',
  "AAAARXAH" varchar(10) NOT NULL default ' ',
  "AAAARXAI" varchar(1) NOT NULL default ' ',
  "AAAARXAJ" varchar(1) NOT NULL default ' ',
  "AAAARXAK" varchar(8) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAABLPSU" varchar(80) NOT NULL default ' ',
  "AAAARXAZ" varchar(1) NOT NULL default ' ',
  "AAABLKIV" varchar(1) NOT NULL default ' ',
  "AAABLEJW" varchar(128) NOT NULL default ' ',
  "AAAARXBD" varchar(1) NOT NULL default ' ',
  "AAAARXBE" VARBINARY(255),
  "AAAARXBH" VARBINARY(255),
  "AAAARXBI" varchar(1) NOT NULL default ' ',
  "AAAARXBJ" varchar(1) NOT NULL default ' ',
  "AAAARXBK" varchar(1) NOT NULL default ' ',
  "AAABHIYE" varchar(8) NOT NULL default '00000000',
  "AAABHIYF" varchar(6) NOT NULL default '000000',
  "AAAARXQG" varchar(30) NOT NULL default ' ',
  "AAAARXQH" varchar(128) NOT NULL default ' ',
  "AAABKYPM" varchar(32) NOT NULL default ' ',
  "AAABLEKT" varchar(8) NOT NULL default ' ',
  "AAAARXQJ" varchar(1) NOT NULL default ' ',
  "AAAARYHN" varchar(30) NOT NULL default ' ',
  "AAABCKCB" varchar(1) NOT NULL default ' ',
  "AAAARXBQ" varchar(1) NOT NULL default '0',
  "AAABEWQH" varchar(30) NOT NULL default ' ',
  "AAAARXBS" varchar(1) NOT NULL default ' ',
  "AAAARXBT" varchar(1) NOT NULL default ' ',
  "AAAARXBY" varchar(1) NOT NULL default ' ',
  "AAAARXCC" varchar(1) NOT NULL default ' ',
  "AAABGXFD" varchar(10) NOT NULL default ' ',
  "AAAARXCE" varchar(1) NOT NULL default ' ',
  "AAAAROFH" varchar(1) NOT NULL default ' ',
  "AAAAROFI" varchar(30) NOT NULL default ' ',
  "AAAARXCF" varchar(1) NOT NULL default ' ',
  "AAAARXCG" varchar(1) NOT NULL default ' ',
  "AAAARXCH" int(11) NOT NULL default '0',
  "AAAARXCI" varchar(2) NOT NULL default '00',
  "AAAARXCJ" varchar(1) NOT NULL default ' ',
  "AAAARXCK" varchar(1) NOT NULL default ' ',
  "AAAARXCQ" varchar(1) NOT NULL default ' ',
  "AAAAROFQ" varchar(1) NOT NULL default ' ',
  "AAAARXCS" varchar(1) NOT NULL default ' ',
  "AAAARXCT" varchar(1) NOT NULL default ' ',
  "AAAARXCU" varchar(1) NOT NULL default ' ',
  "AAAARXCV" varchar(1) NOT NULL default ' ',
  "AAAARXCW" varchar(1) NOT NULL default ' ',
  "AAAAVNMQ" varchar(8) NOT NULL default '00000000',
  "AAABLNYE" varchar(1) NOT NULL default ' ',
  "AAAARXCZ" varchar(1) NOT NULL default ' ',
  "AAAARXDA" varchar(4) NOT NULL default '0000',
  "AAAARXDB" varchar(1) NOT NULL default ' ',
  "AAAARXDC" varchar(1) NOT NULL default ' ',
  "AAAARXDD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYHM","AAABIWFD","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARODD" (
  "AAAARYHM" varchar(30) NOT NULL default ' ',
  "AAABGFLE" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(45) NOT NULL default ' ',
  "AAABMNNA" varchar(45) NOT NULL default ' ',
  "AAABMNLA" varchar(1) NOT NULL default ' ',
  "AAAARYHO" varchar(30) NOT NULL default ' ',
  "AAAARYHP" varchar(1) NOT NULL default ' ',
  "AAABIOAH" varchar(1) NOT NULL default ' ',
  "AAAARYYV" varchar(1) NOT NULL default ' ',
  "AAAAWNFG" varchar(1) NOT NULL default ' ',
  "AAAARYYU" varchar(2) NOT NULL default ' ',
  "AAAARXAT" varchar(1) NOT NULL default ' ',
  "AAABIWLU" varchar(30) NOT NULL default ' ',
  "AAAARSHR" varchar(3) NOT NULL default ' ',
  "AAAARSHS" varchar(8) NOT NULL default '00000000',
  "AAAARSHU" varchar(8) NOT NULL default '00000000',
  "AAAARSHT" varchar(4) NOT NULL default ' ',
  "AAABHTVO" varchar(1) NOT NULL default ' ',
  "AAAARXAG" varchar(45) NOT NULL default ' ',
  "AAAARXAH" varchar(10) NOT NULL default ' ',
  "AAAARXAI" varchar(1) NOT NULL default ' ',
  "AAAARXAJ" varchar(1) NOT NULL default ' ',
  "AAAARXAK" varchar(8) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAABLPSU" varchar(80) NOT NULL default ' ',
  "AAAARXAZ" varchar(1) NOT NULL default ' ',
  "AAABLKIV" varchar(1) NOT NULL default ' ',
  "AAABLEJW" varchar(128) NOT NULL default ' ',
  "AAAARXBD" varchar(1) NOT NULL default ' ',
  "AAAARXBE" VARBINARY(255),
  "AAAARXBH" VARBINARY(255),
  "AAAARXBI" varchar(1) NOT NULL default ' ',
  "AAAARXBJ" varchar(1) NOT NULL default ' ',
  "AAAARXBK" varchar(1) NOT NULL default ' ',
  "AAABHIYE" varchar(8) NOT NULL default '00000000',
  "AAABHIYF" varchar(6) NOT NULL default '000000',
  "AAAARXQG" varchar(30) NOT NULL default ' ',
  "AAAARXQH" varchar(128) NOT NULL default ' ',
  "AAABKYPM" varchar(32) NOT NULL default ' ',
  "AAABLEKT" varchar(8) NOT NULL default ' ',
  "AAAARXQJ" varchar(1) NOT NULL default ' ',
  "AAAARYHN" varchar(30) NOT NULL default ' ',
  "AAABCKCB" varchar(1) NOT NULL default ' ',
  "AAAARXBQ" varchar(1) NOT NULL default '0',
  "AAABEWQH" varchar(30) NOT NULL default ' ',
  "AAAARXBS" varchar(1) NOT NULL default ' ',
  "AAAARXBT" varchar(1) NOT NULL default ' ',
  "AAAARXBY" varchar(1) NOT NULL default ' ',
  "AAAARXCC" varchar(1) NOT NULL default ' ',
  "AAABGXFD" varchar(10) NOT NULL default ' ',
  "AAAARXCE" varchar(1) NOT NULL default ' ',
  "AAAAROFH" varchar(1) NOT NULL default ' ',
  "AAAAROFI" varchar(30) NOT NULL default ' ',
  "AAAARXCF" varchar(1) NOT NULL default ' ',
  "AAAARXCG" varchar(1) NOT NULL default ' ',
  "AAAARXCH" int(11) NOT NULL default '0',
  "AAAARXCI" varchar(2) NOT NULL default '00',
  "AAAARXCJ" varchar(1) NOT NULL default ' ',
  "AAAARXCK" varchar(1) NOT NULL default ' ',
  "AAAARXCQ" varchar(1) NOT NULL default ' ',
  "AAAAROFQ" varchar(1) NOT NULL default ' ',
  "AAAARXCS" varchar(1) NOT NULL default ' ',
  "AAAARXCT" varchar(1) NOT NULL default ' ',
  "AAAARXCU" varchar(1) NOT NULL default ' ',
  "AAAARXCV" varchar(1) NOT NULL default ' ',
  "AAAAVNMQ" varchar(8) NOT NULL default '00000000',
  "AAABLNYE" varchar(1) NOT NULL default ' ',
  "AAAARXCZ" varchar(1) NOT NULL default ' ',
  "AAAARXDA" varchar(4) NOT NULL default '0000',
  "AAAARXDB" varchar(1) NOT NULL default ' ',
  "AAAARXDC" varchar(1) NOT NULL default ' ',
  "AAAARXDD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYHM","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROGE" (
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARYHM" varchar(30) NOT NULL default ' ',
  "AAAARXDL" varchar(30) NOT NULL default ' ',
  "AAAARYHN" varchar(30) NOT NULL default ' ',
  "AAAAROGK" varchar(60) NOT NULL default ' ',
  "AAAAROGL" varchar(60) NOT NULL default ' ',
  "AAAAROGM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYNB","AAABMLQA"),
  KEY "AAAAROGP" ("AAAARYHM"),
  KEY "AAAAROGR" ("AAABMLQA"),
  KEY "AAAAROGT" ("AAAARYNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROGV" (
  "AAAARXDL" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABLEST" varchar(12) NOT NULL default ' ',
  "AAAAROHV" varchar(30) NOT NULL default ' ',
  "AAAAROHW" varchar(30) NOT NULL default ' ',
  "AAAAROHX" varchar(30) NOT NULL default ' ',
  "AAAAROJX" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAARXDL","AAABIWFD","AAABLEST"),
  KEY "AAAAROHN" ("AAABIWFD","AAABLEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROHQ" (
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARYGG" varchar(4) NOT NULL default ' ',
  "AAAARXDL" varchar(30) NOT NULL default ' ',
  "AAABLEST" varchar(12) NOT NULL default ' ',
  "AAAAROHV" varchar(30) NOT NULL default ' ',
  "AAAAROHW" varchar(30) NOT NULL default ' ',
  "AAAAROHX" varchar(30) NOT NULL default ' ',
  "AAAAROJX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARZRE","AAAARYGG","AAAARXDL","AAABLEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROID" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARXDL" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABLEST" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAARXDL","AAABIWFD","AAABLEST"),
  KEY "AAAAROIN" ("AAABIWFD","AAABLEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROIQ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARYGG" varchar(4) NOT NULL default ' ',
  "AAAARXDL" varchar(30) NOT NULL default ' ',
  "AAABLEST" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAARZRE","AAAARYGG","AAAARXDL","AAABLEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROJC" (
  "AAAARXDL" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARYHM" varchar(30) NOT NULL default ' ',
  "AAABLEST" varchar(12) NOT NULL default ' ',
  "AAAAROJW" varchar(30) NOT NULL default ' ',
  "AAAAROJX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARXDL","AAABIWFD","AAAARYHM","AAABLEST"),
  KEY "AAAAROJN" ("AAABIWFD","AAABLEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROJQ" (
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARYGG" varchar(4) NOT NULL default ' ',
  "AAAARXDL" varchar(30) NOT NULL default ' ',
  "AAAARYHM" varchar(30) NOT NULL default ' ',
  "AAABLEST" varchar(12) NOT NULL default ' ',
  "AAAAROJW" varchar(30) NOT NULL default ' ',
  "AAAAROJX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARZRE","AAAARYGG","AAAARXDL","AAAARYHM","AAABLEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROKD" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAAAROKG" varchar(1) NOT NULL default ' ',
  "AAABMMRH" varchar(1) NOT NULL default '0',
  "AAABMLZS" varchar(3) NOT NULL default '000',
  "AAABJSDF" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMDEN","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROKM" (
  "AAAARZQI" varchar(30) NOT NULL default ' ',
  "AAABCZJZ" varchar(30) NOT NULL default ' ',
  "AAAAROKW" varchar(12) NOT NULL default ' ',
  "AAAARZOM" varchar(30) NOT NULL default ' ',
  "AAABGLID" varchar(30) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  "AAAAROKT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARZQI","AAABCZJZ","AAAAROKW","AAAARZOM","AAABGLID")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROKZ" (
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAARSHO" varchar(3) NOT NULL default ' ',
  "AAAARSHP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROLE" (
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROLH" (
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAROLJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROLL" (
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABHIXN" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROLP" (
  "AAAAROLS" varchar(10) NOT NULL default ' ',
  "AAAAROLR" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAROLS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROLT" (
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABJETS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROLX" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABGFLE" varchar(2) NOT NULL default '00',
  "AAAAWBOJ" varchar(1) NOT NULL default ' ',
  "AAABLLJS" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEN","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROME" (
  "AAABFMTA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFMTA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROMH" (
  "AAABFMTA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFMTA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROMK" (
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
  "AAABLRXN" varchar(8) NOT NULL default '00000000',
  "AAABLRXO" varchar(8) NOT NULL default '00000000',
  "AAAAROMX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIVVH","AAABIWFD","AAABIVVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARONB" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAAAROND" int(11) NOT NULL default '0',
  "AAAAROOJ" int(11) NOT NULL default '0',
  "AAAARONF" int(11) NOT NULL default '0',
  "AAABFUKI" int(11) NOT NULL default '0',
  "AAAARONH" int(11) NOT NULL default '0',
  "AAAARONI" int(11) NOT NULL default '0',
  "AAAARONJ" int(11) NOT NULL default '0',
  "AAAARONK" decimal(15,0) NOT NULL default '0',
  "AAAAROOK" decimal(15,0) NOT NULL default '0',
  "AAABKWWX" decimal(15,0) NOT NULL default '0',
  "AAABDFIY" int(11) NOT NULL default '0',
  "AAAARONO" int(11) NOT NULL default '0',
  "AAAARONP" varchar(1) NOT NULL default ' ',
  "AAAARONQ" varchar(1) NOT NULL default ' ',
  "AAAARONR" varchar(1) NOT NULL default ' ',
  "AAAARONS" varchar(1) NOT NULL default ' ',
  "AAAARTMO" varchar(1) NOT NULL default ' ',
  "AAAARTMP" varchar(1) NOT NULL default ' ',
  "AAAARONV" varchar(1) NOT NULL default ' ',
  "AAAARONW" varchar(1) NOT NULL default ' ',
  "AAAARONX" varchar(1) NOT NULL default ' ',
  "AAAARONY" varchar(1) NOT NULL default ' ',
  "AAABIKEM" varchar(3) NOT NULL default '000',
  "AAAAROOA" varchar(1) NOT NULL default ' ',
  "AAAAROOB" varchar(1) NOT NULL default ' ',
  "AAAAROOC" varchar(1) NOT NULL default ' ',
  "AAAAROOD" varchar(1) NOT NULL default ' ',
  "AAAAROOE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROOG" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAROOJ" int(11) NOT NULL default '0',
  "AAAAROOK" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABIHKQ","AAABMQDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROON" (
  "AAABFMTA" varchar(30) NOT NULL default ' ',
  "AAAAROOP" varchar(30) NOT NULL default ' ',
  "AAAAROOQ" varchar(30) NOT NULL default ' ',
  "AAAAROOR" varchar(1) NOT NULL default ' ',
  "AAAAROOS" varchar(1) NOT NULL default ' ',
  "AAABHVRR" varchar(5) NOT NULL default ' ',
  "AAAAROPL" varchar(1) NOT NULL default ' ',
  "AAAAROOV" varchar(1) NOT NULL default ' ',
  "AAAAROPF" int(11) NOT NULL default '0',
  "AAAAROOX" int(11) NOT NULL default '0',
  "AAAAROOY" varchar(1) NOT NULL default ' ',
  "AAAARZXN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFMTA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROPB" (
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAAAROPH" varchar(30) NOT NULL default ' ',
  "AAAAROPE" varchar(30) NOT NULL default ' ',
  "AAAAROPF" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABIWNN","AAAAROPH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROPI" (
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAABFMTA" varchar(30) NOT NULL default ' ',
  "AAAAROPL" varchar(1) NOT NULL default ' ',
  "AAAAROPM" varchar(1) NOT NULL default ' ',
  "AAABHVRR" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSG","AAABFMTA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROPQ" (
  "AAABHVRR" varchar(5) NOT NULL default ' ',
  "AAAAROPS" varchar(1) NOT NULL default ' ',
  "AAAAROPT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHVRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROPV" (
  "AAABHTTC" varchar(30) NOT NULL default ' ',
  "AAAAWDEM" varchar(30) NOT NULL default ' ',
  "AAAAROPY" varchar(8) NOT NULL default '00000000',
  "AAAAROPZ" varchar(6) NOT NULL default '000000',
  "AAABIGSL" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABHTTC","AAAAWDEM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROQD" (
  "AAABFMTA" varchar(30) NOT NULL default ' ',
  "AAABLPJD" varchar(1) NOT NULL default ' ',
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFMTA","AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROQK" (
  "AAABFMTA" varchar(30) NOT NULL default ' ',
  "AAAAROQM" varchar(30) NOT NULL default ' ',
  "AAAAROQN" varchar(30) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABFMTA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROQQ" (
  "AAABIVVH" varchar(25) NOT NULL default ' ',
  "AAABJJLG" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABIVVH","AAABJJLG","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROQX" (
  "AAABJYFV" varchar(4) NOT NULL default ' ',
  "AAABFMTA" varchar(30) NOT NULL default ' ',
  "AAABLUFA" varchar(30) NOT NULL default ' ',
  "AAABIVVH" varchar(25) NOT NULL default ' ',
  "AAAARORC" varchar(1) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABJYFV","AAABFMTA","AAABLUFA","AAABIVVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARORJ" (
  "AAAARORQ" varchar(60) NOT NULL default ' ',
  "AAAARORL" varchar(60) NOT NULL default ' ',
  "AAAAROTA" varchar(60) NOT NULL default ' ',
  "AAAAROTB" int(11) NOT NULL default '0',
  "AAAAROTC" varchar(60) NOT NULL default ' ',
  "AAAAROTD" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAARORQ"),
  KEY "AAAARORR" ("AAAAROTA"),
  KEY "AAAARORT" ("AAAAROTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARORV" (
  "AAAARRIM" varchar(30) NOT NULL default ' ',
  "AAAARYEC" varchar(6) NOT NULL default '000000',
  "AAABMRFO" varchar(1) NOT NULL default '0',
  "AAABLJJO" varchar(1) NOT NULL default ' ',
  "AAAARRHE" varchar(10) NOT NULL default '0000000000',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAARRIM","AAAARYEC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROSE" (
  "AAAAROSN" varchar(8) NOT NULL default ' ',
  "AAAAROSG" varchar(1) NOT NULL default ' ',
  "AAAAROSH" varchar(1) NOT NULL default ' ',
  "AAAAROSI" varchar(25) NOT NULL default ' ',
  "AAAAROSJ" varchar(1) NOT NULL default ' ',
  "AAABFMTA" varchar(30) NOT NULL default ' ',
  "AAABHVRR" varchar(5) NOT NULL default ' ',
  "AAAAROSM" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAAROSN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROSO" (
  "AAAAROSU" varchar(60) NOT NULL default ' ',
  "AAAAROSQ" varchar(4) NOT NULL default ' ',
  "AAABMNJW" varchar(2) NOT NULL default ' ',
  "AAAAROSS" varchar(60) NOT NULL default ' ',
  "AAAAROST" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAAROSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROSV" (
  "AAAAROTA" varchar(60) NOT NULL default ' ',
  "AAAAROTB" int(11) NOT NULL default '0',
  "AAAAROTC" varchar(60) NOT NULL default ' ',
  "AAAAROTD" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAAROTA","AAAAROTB","AAAAROTC","AAAAROTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROTE" (
  "AAABJYFV" varchar(4) NOT NULL default ' ',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAAAROTI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJYFV"),
  UNIQUE KEY "AAAAROTK" ("AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROTM" (
  "AAABJYFV" varchar(4) NOT NULL default ' ',
  "AAABETOY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJYFV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROTQ" (
  "AAABIVVH" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWMM" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABIVVH","AAABIWFD","AAABIWMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROTX" (
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABMMRH" varchar(4) NOT NULL default '0000',
  "AAABLSBY" VARBINARY(255),
  PRIMARY KEY ("AAABMLPV","AAABMMRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROUD" (
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABFWCT" varchar(10) NOT NULL default '0000000000',
  "AAABDXHF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROUI" (
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABMMVU" varchar(1) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABIWNN","AAABMMVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROUO" (
  "AAAAROUZ" varchar(3) NOT NULL default '000',
  "AAABIWJO" varchar(8) NOT NULL default '00000000',
  "AAABEUSG" varchar(6) NOT NULL default '000000',
  "AAAAROVC" varchar(3) NOT NULL default '000',
  "AAABKOUF" varchar(12) NOT NULL default ' ',
  "AAAAROUU" varchar(3) NOT NULL default '000',
  "AAAAROUV" varchar(32) NOT NULL default ' ',
  "AAAAROUW" varchar(8) NOT NULL default ' ',
  "AAAAROUX" varchar(1) NOT NULL default ' ',
  "AAAAROUY" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAAAROUZ","AAABIWJO","AAABEUSG","AAAAROVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROVD" (
  "AAAARYGR" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAARYGR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROVG" (
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAAARPBV" varchar(2) NOT NULL default ' ',
  "AAAAROVJ" varchar(1) NOT NULL default ' ',
  "AAAARXAQ" varchar(1) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTO" varchar(40) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABINAW","AAAARPBV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROVQ" (
  "AAABMOIB" varchar(12) NOT NULL default ' ',
  "AAABLUZL" varchar(30) NOT NULL default ' ',
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAABMMVU" varchar(1) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAARYHM" varchar(30) NOT NULL default ' ',
  "AAAAVIZO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOIB","AAABLUZL","AAAATJNW","AAABMMVU","AAABMQDU","AAAARYHM","AAAAVIZO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROWF" (
  "AAAARVLG" varchar(30) NOT NULL default ' ',
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAAAROWI" int(11) NOT NULL default '0',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABMRFO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAARVLG","AAAARYNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROWN" (
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAAARXLM" varchar(6) NOT NULL default '000000',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAAAROWR" int(11) NOT NULL default '0',
  "AAAAROWS" int(11) NOT NULL default '0',
  "AAAAROWT" int(11) NOT NULL default '0',
  "AAAAROWU" int(11) NOT NULL default '0',
  "AAAAROWV" int(11) NOT NULL default '0',
  "AAAAROWW" int(11) NOT NULL default '0',
  "AAAAROWX" int(11) NOT NULL default '0',
  "AAAAROWY" int(11) NOT NULL default '0',
  "AAAAROWZ" int(11) NOT NULL default '0',
  "AAAAROXA" varchar(10) NOT NULL default '0000000000',
  "AAAAROZF" varchar(10) NOT NULL default '0000000000',
  "AAAAROXC" varchar(1) NOT NULL default ' ',
  "AAABIWVY" varchar(16) NOT NULL default '0000000000000000',
  "AAAARPDR" varchar(10) NOT NULL default '0000000000',
  "AAAAROXF" varchar(2) NOT NULL default ' ',
  "AAAAROXG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYNB","AAAARXLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROXJ" (
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAAARVLG" varchar(30) NOT NULL default ' ',
  "AAAARXLM" varchar(6) NOT NULL default '000000',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABMRFO" varchar(4) NOT NULL default ' ',
  "AAAARVHL" varchar(4) NOT NULL default ' ',
  "AAAARPBV" varchar(2) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAROXY" int(11) NOT NULL default '0',
  "AAAAROXZ" int(11) NOT NULL default '0',
  "AAAAROYM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYNB","AAAARVLG","AAAARXLM"),
  KEY "AAAAROYE" ("AAAARVLG","AAAARXLM"),
  KEY "AAAAROYH" ("AAAARYNB","AAAARVLG","AAAAROYM"),
  KEY "AAAAROYL" ("AAAAROYM"),
  KEY "AAAAROYN" ("AAABMKFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROYP" (
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAAAROZF" varchar(10) NOT NULL default '0000000000',
  "AAABGFLE" varchar(10) NOT NULL default '0000000000',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABJEDB" varchar(30) NOT NULL default ' ',
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAAAROYW" int(11) NOT NULL default '0',
  "AAAAROYX" int(11) NOT NULL default '0',
  "AAAAROYY" int(11) NOT NULL default '0',
  "AAAAROYZ" int(11) NOT NULL default '0',
  "AAAAROZA" int(11) NOT NULL default '0',
  "AAAAROZB" int(11) NOT NULL default '0',
  "AAAAROZC" int(11) NOT NULL default '0',
  "AAAARPDR" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAAARYNB","AAAAROZF","AAABGFLE"),
  KEY "AAAAROZH" ("AAABJEDB"),
  KEY "AAAAROZJ" ("AAABIHKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROZL" (
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAAARPCE" varchar(6) NOT NULL default '000000',
  "AAAARXLM" varchar(6) NOT NULL default '000000',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAAARPBZ" varchar(1) NOT NULL default ' ',
  "AAABIWVY" varchar(16) NOT NULL default '0000000000000000',
  "AAAAROZS" varchar(12) NOT NULL default '000000000000',
  "AAAARPDR" varchar(10) NOT NULL default '0000000000',
  "AAAAROZU" varchar(12) NOT NULL default '000000000000',
  PRIMARY KEY ("AAAARYNB","AAAARPCE","AAAARXLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAROZY" (
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAABKWQO" varchar(1) NOT NULL default ' ',
  "AAABHBOX" varchar(3) NOT NULL default '000',
  "AAAARPAC" varchar(6) NOT NULL default '000000',
  "AAAARPAD" varchar(6) NOT NULL default '000000',
  "AAAARPAE" varchar(6) NOT NULL default '000000',
  "AAAARPAF" varchar(6) NOT NULL default '000000',
  "AAAARPAG" varchar(6) NOT NULL default '000000',
  "AAAARPAH" varchar(6) NOT NULL default '000000',
  "AAAARPAI" varchar(6) NOT NULL default '000000',
  "AAAARPAJ" varchar(6) NOT NULL default '000000',
  "AAAARPAK" varchar(6) NOT NULL default '000000',
  "AAAARPAL" varchar(6) NOT NULL default '000000',
  "AAAARPAM" varchar(6) NOT NULL default '000000',
  "AAAARPAN" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAARYNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPAP" (
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAAARPDR" varchar(10) NOT NULL default '0000000000',
  "AAAARPCE" varchar(6) NOT NULL default '000000',
  "AAAARXLM" varchar(6) NOT NULL default '000000',
  "AAAARPBZ" varchar(1) NOT NULL default ' ',
  "AAABGFLE" varchar(10) NOT NULL default '0000000000',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  "AAABMNDU" varchar(80) NOT NULL default ' ',
  "AAAARPBV" varchar(2) NOT NULL default ' ',
  "AAABLRLL" decimal(21,7) NOT NULL default '0.0000000',
  "AAABLSKU" varchar(1) NOT NULL default ' ',
  "AAAARPBI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYNB","AAAARPDR","AAAARPCE","AAAARXLM","AAAARPBZ","AAABGFLE"),
  KEY "AAAARPBP" ("AAAARYNB","AAAARPBV"),
  KEY "AAAARPBS" ("AAAARPBV"),
  KEY "AAAARPBU" ("AAAARPBV","AAAARPBZ","AAABMKFU"),
  KEY "AAAARPBY" ("AAAARPBZ"),
  KEY "AAAARPCA" ("AAABMKFU"),
  KEY "AAAARPCC" ("AAAARYNB","AAAARPCE","AAAARXLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPCG" (
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAAARPCM" varchar(30) NOT NULL default ' ',
  "AAABLUZZ" varchar(1) NOT NULL default ' ',
  "AAAARPCP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYNB","AAAARPCM","AAABLUZZ"),
  KEY "AAAARPCO" ("AAAARPCP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPCQ" (
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAABHGJR" varchar(25) NOT NULL default ' ',
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  "AAABMOIB" varchar(30) NOT NULL default ' ',
  "AAABHFAP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYNB","AAABHGJR","AAABMOHM","AAABMOIB","AAABHFAP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPDB" (
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  "AAABMOIB" varchar(30) NOT NULL default ' ',
  "AAABHFAP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYNB","AAABMOHM","AAABMOIB","AAABHFAP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPDK" (
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAABIWVY" varchar(16) NOT NULL default '0000000000000000',
  "AAABMLFE" varchar(1) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAABKWQO" varchar(1) NOT NULL default ' ',
  "AAAARPDR" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAAARYNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPDT" (
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAAARPEA" varchar(25) NOT NULL default ' ',
  "AAABGFLE" varchar(10) NOT NULL default '0000000000',
  "AAABLJIM" varchar(40) NOT NULL default ' ',
  "AAABMMRH" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAARYNB","AAAARPEA","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPEC" (
  "AAAAVMEK" varchar(4) NOT NULL default ' ',
  "AAABIXWA" varchar(1) NOT NULL default ' ',
  "AAABMNRG" varchar(1) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAAAVMEK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPEI" (
  "AAAAVMEK" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAAAVMEK","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPEO" (
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABLFYC" varchar(1) NOT NULL default ' ',
  "AAAARPFY" varchar(20) NOT NULL default ' ',
  "AAAARPFZ" varchar(4) NOT NULL default ' ',
  "AAAARZMC" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  "AAABLQHQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLESS","AAABLSCE","AAABLFYC","AAAARPFY","AAAARPFZ","AAAARZMC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPFD" (
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABLFYC" varchar(1) NOT NULL default ' ',
  "AAAARPFY" varchar(20) NOT NULL default ' ',
  "AAAARPFZ" varchar(4) NOT NULL default ' ',
  "AAAARZMC" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  "AAABLQHQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLESS","AAABLSCE","AAABLFYC","AAAARPFY","AAAARPFZ","AAAARZMC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPFS" (
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAAARPGB" varchar(4) NOT NULL default ' ',
  "AAAARPGC" varchar(1) NOT NULL default ' ',
  "AAAARPGD" varchar(1) NOT NULL default ' ',
  "AAABLFYC" varchar(1) NOT NULL default ' ',
  "AAAARPFY" varchar(20) NOT NULL default ' ',
  "AAAARPFZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLESS","AAAARPGB","AAAARPGC","AAAARPGD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPGE" (
  "AAAARXTZ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAAARXTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPGH" (
  "AAABAARF" varchar(3) NOT NULL default '000',
  "AAAARYGR" varchar(6) NOT NULL default '000000',
  "AAAARYGS" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABAARF","AAAARYGR","AAAARYGS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPGO" (
  "AAABAARF" varchar(3) NOT NULL default '000',
  "AAAARYHB" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABAARF","AAAARYHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPGT" (
  "AAABAARF" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABAARF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPGW" (
  "AAAARRLG" varchar(3) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARRLG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPHA" (
  "AAABGLSO" varchar(4) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGLSO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPHE" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  "AAAARPII" varchar(1) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABKMZH" varchar(1) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABBERJ" varchar(8) NOT NULL default '00000000',
  "AAAARPHN" varchar(6) NOT NULL default '000000',
  "AAAARPHO" varchar(12) NOT NULL default ' ',
  "AAAARPHP" varchar(8) NOT NULL default '00000000',
  "AAAARPHQ" varchar(6) NOT NULL default '000000',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABMLPV","AAAARPII","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPHX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLFYC" varchar(4) NOT NULL default ' ',
  "AAABKMVY" varchar(4) NOT NULL default ' ',
  "AAAARPII" varchar(1) NOT NULL default ' ',
  "AAABLVJB" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  "AAABLQHQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLFYC","AAABKMVY","AAAARPII","AAABLVJB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPIK" (
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABJSPH" varchar(10) NOT NULL default ' ',
  "AAAARTKB" varchar(10) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARZRH","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPIS" (
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAABHFOL" varchar(4) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABBDMV" varchar(40) NOT NULL default ' ',
  "AAAARPIX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARZRH","AAABHFOL","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPJB" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAAARPJF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHKQ","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPJI" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHKQ","AAABMQDU","AAAATJNW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPJP" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHKQ","AAABMQDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPJU" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAAARXLV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHKQ","AAAARXLV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPJZ" (
  "AAAAWRXN" varchar(30) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABFYWO" varchar(30) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABLTQF" varchar(30) NOT NULL default ' ',
  "AAABLTQG" varchar(30) NOT NULL default ' ',
  "AAAARPKG" varchar(30) NOT NULL default ' ',
  "AAAARPKH" varchar(30) NOT NULL default ' ',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAAWRXN","AAABMQDU","AAABFYWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPKM" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAWRXN" varchar(30) NOT NULL default ' ',
  "AAABFYWO" varchar(30) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTO" varchar(40) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMQDU","AAAAWRXN","AAABFYWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPKY" (
  "AAAARRXT" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARPPI" varchar(4) NOT NULL default ' ',
  "AAAARPPJ" varchar(40) NOT NULL default ' ',
  "AAAARPPK" varchar(4) NOT NULL default ' ',
  "AAAARPPL" varchar(40) NOT NULL default ' ',
  "AAAARPSA" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAAARRXT","AAABIWFD","AAAARPPI","AAAARPPJ","AAAARPPK","AAAARPPL"),
  KEY "AAAARPLM" ("AAABIWFD","AAAARPPK","AAAARPPL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPLQ" (
  "AAAARRXT" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARPPI" varchar(4) NOT NULL default ' ',
  "AAAARPPJ" varchar(40) NOT NULL default ' ',
  "AAAARPPK" varchar(4) NOT NULL default ' ',
  "AAAARPPL" varchar(40) NOT NULL default ' ',
  "AAAARPSA" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAAARRXT","AAABIWFD","AAAARPPI","AAAARPPJ","AAAARPPK","AAAARPPL"),
  KEY "AAAARPME" ("AAABIWFD","AAAARPPK","AAAARPPL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPMI" (
  "AAAARRXT" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARPPI" varchar(4) NOT NULL default ' ',
  "AAAARPPJ" varchar(40) NOT NULL default ' ',
  "AAAARPPK" varchar(4) NOT NULL default ' ',
  "AAAARPPL" varchar(40) NOT NULL default ' ',
  "AAAARPSA" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAAARRXT","AAABIWFD","AAAARPPI","AAAARPPJ","AAAARPPK","AAAARPPL"),
  KEY "AAAARPMW" ("AAABIWFD","AAAARPPK","AAAARPPL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPNA" (
  "AAAARRXT" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARPPI" varchar(4) NOT NULL default ' ',
  "AAAARPPJ" varchar(40) NOT NULL default ' ',
  "AAAARPPK" varchar(4) NOT NULL default ' ',
  "AAAARPPL" varchar(40) NOT NULL default ' ',
  "AAAARPSA" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAAARRXT","AAABIWFD","AAAARPPI","AAAARPPJ","AAAARPPK","AAAARPPL"),
  KEY "AAAARPNO" ("AAABIWFD","AAAARPPK","AAAARPPL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPNS" (
  "AAAARRXT" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARPPI" varchar(4) NOT NULL default ' ',
  "AAAARPPJ" varchar(40) NOT NULL default ' ',
  "AAAARPPK" varchar(4) NOT NULL default ' ',
  "AAAARPPL" varchar(40) NOT NULL default ' ',
  "AAAARPSA" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAAARRXT","AAABIWFD","AAAARPPI","AAAARPPJ","AAAARPPK","AAAARPPL"),
  KEY "AAAARPOG" ("AAABIWFD","AAAARPPK","AAAARPPL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPOK" (
  "AAAARRXT" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARPPI" varchar(4) NOT NULL default ' ',
  "AAAARPPJ" varchar(40) NOT NULL default ' ',
  "AAAARPPK" varchar(4) NOT NULL default ' ',
  "AAAARPPL" varchar(40) NOT NULL default ' ',
  "AAAARPSA" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAAARRXT","AAABIWFD","AAAARPPI","AAAARPPJ","AAAARPPK","AAAARPPL"),
  KEY "AAAARPOY" ("AAABIWFD","AAAARPPK","AAAARPPL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPPC" (
  "AAAARPPI" varchar(4) NOT NULL default ' ',
  "AAAARPPJ" varchar(40) NOT NULL default ' ',
  "AAAARPPK" varchar(4) NOT NULL default ' ',
  "AAAARPPL" varchar(40) NOT NULL default ' ',
  "AAABLTVR" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAAARPPI","AAAARPPJ","AAAARPPK","AAAARPPL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPPM" (
  "AAABHFOL" varchar(4) NOT NULL default ' ',
  "AAABBDMV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFOL","AAABBDMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPPR" (
  "AAABHFOL" varchar(4) NOT NULL default ' ',
  "AAABBDMV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFOL","AAABBDMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPPW" (
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABILWI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABIRUN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPQA" (
  "AAABMJER" varchar(80) NOT NULL default ' ',
  "AAABFCQJ" varchar(1) NOT NULL default ' ',
  "AAAARPQD" varchar(1) NOT NULL default ' ',
  "AAAARPQE" varchar(1) NOT NULL default ' ',
  "AAABLNRX" varchar(32) NOT NULL default ' ',
  "AAAARPQG" varchar(1) NOT NULL default ' ',
  "AAABHFOL" varchar(4) NOT NULL default ' ',
  "AAAARPQI" varchar(32) NOT NULL default ' ',
  "AAAARPQJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMJER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPQL" (
  "AAABMJER" varchar(80) NOT NULL default ' ',
  "AAABLEEW" varchar(30) NOT NULL default ' ',
  "AAABLEEK" varchar(80) NOT NULL default ' ',
  "AAABLNRX" varchar(32) NOT NULL default ' ',
  "AAAARXTK" varchar(1) NOT NULL default ' ',
  "AAABKBEZ" varchar(1) NOT NULL default ' ',
  "AAABKRHS" varchar(32) NOT NULL default ' ',
  "AAAARPRM" varchar(10) NOT NULL default ' ',
  "AAAARPRN" varchar(10) NOT NULL default ' ',
  "AAAARPRO" varchar(1) NOT NULL default ' ',
  "AAAARPRP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMJER","AAABLEEW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPQZ" (
  "AAABMJER" varchar(80) NOT NULL default ' ',
  "AAAARPRD" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMJER","AAAARPRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPRE" (
  "AAABMJER" varchar(80) NOT NULL default ' ',
  "AAABMKXF" varchar(30) NOT NULL default ' ',
  "AAABJSLL" varchar(80) NOT NULL default ' ',
  "AAAARPRI" varchar(80) NOT NULL default ' ',
  "AAABLNRX" varchar(32) NOT NULL default ' ',
  "AAAARXTK" varchar(1) NOT NULL default ' ',
  "AAABGCIW" varchar(1) NOT NULL default ' ',
  "AAAARPRM" varchar(10) NOT NULL default ' ',
  "AAAARPRN" varchar(10) NOT NULL default ' ',
  "AAAARPRO" varchar(1) NOT NULL default ' ',
  "AAAARPRP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMJER","AAABMKXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPRS" (
  "AAABIYDX" varchar(3) NOT NULL default '000',
  "AAAARPRY" varchar(1) NOT NULL default ' ',
  "AAABHFOL" varchar(4) NOT NULL default ' ',
  "AAAARPSA" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABIYDX","AAAARPRY","AAABHFOL","AAAARPSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPSB" (
  "AAABIWGC" varchar(10) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWGC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPSF" (
  "AAABIWGC" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWGC","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPSL" (
  "AAABHFOL" varchar(4) NOT NULL default ' ',
  "AAABBDMV" varchar(40) NOT NULL default ' ',
  "AAAARPSO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFOL","AAABBDMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPSR" (
  "AAABMLPV" varchar(25) NOT NULL default ' ',
  "AAABMMJT" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPSV" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPTC" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPTJ" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPTQ" (
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
CREATE TABLE "AAAARPUB" (
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
CREATE TABLE "AAAARPUM" (
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
CREATE TABLE "AAAARPUX" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPVH" (
  "AAABHFOL" varchar(4) NOT NULL default ' ',
  "AAABBDMV" varchar(40) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFOL","AAABBDMV","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPVO" (
  "AAABHFOL" varchar(4) NOT NULL default ' ',
  "AAABBDMV" varchar(40) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABHFOL","AAABBDMV","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPVW" (
  "AAABHFOL" varchar(4) NOT NULL default ' ',
  "AAABBDMV" varchar(40) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLTVR" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFOL","AAABBDMV","AAABMLQA","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPWG" (
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
  KEY "AAAARPWU" ("AAABMRFO","AAABMLQA","AAABHZDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPWY" (
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
  KEY "AAAARPXM" ("AAABMRFO","AAABMLQA","AAABHZDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPXQ" (
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
  KEY "AAAARPYE" ("AAABMRFO","AAABMLQA","AAABHZDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPYI" (
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
  KEY "AAAARPZB" ("AAABLLLO"),
  KEY "AAAARPZD" ("AAABLLLH"),
  KEY "AAAARPZF" ("AAABLLLI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARPZH" (
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
  KEY "AAAARQAA" ("AAABLLLJ","AAABLLLG"),
  KEY "AAAARQAD" ("AAABLLLH","AAABLLLI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQAG" (
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
  KEY "AAAARQAZ" ("AAABLLLG"),
  KEY "AAAARQBB" ("AAABLLLH","AAABLLLI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQBE" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQBL" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQBS" (
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
  KEY "AAAARQCL" ("AAABLLLO"),
  KEY "AAAARQCN" ("AAABLLLH"),
  KEY "AAAARQCP" ("AAABLLLI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQCR" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQCY" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQDF" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAARQDP" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQDT" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAARQED" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQEH" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAARQER" ("AAABLLMD","AAABLLLX","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQEW" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAARQFG" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQFK" (
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
  UNIQUE KEY "AAAARQFX" ("AAABLLNL"),
  KEY "AAAARQFZ" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQGB" (
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
  UNIQUE KEY "AAAARQGO" ("AAABLLNL"),
  KEY "AAAARQGQ" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQGS" (
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
  UNIQUE KEY "AAAARQHF" ("AAABLLNL"),
  KEY "AAAARQHH" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQHJ" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNN","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAARQHW" ("AAABLLMD","AAABLLME","AAABLLND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQIA" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNN","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAARQIN" ("AAABLLMD","AAABLLME","AAABLLND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQIR" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNN","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAARQJE" ("AAABLLMD","AAABLLME","AAABLLND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQJI" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNN","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAARQJV" ("AAABLLMD","AAABLLME","AAABLLND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQJZ" (
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
  UNIQUE KEY "AAAARQKM" ("AAABLLNL"),
  KEY "AAAARQKO" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQKQ" (
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
  UNIQUE KEY "AAAARQLD" ("AAABLLNL"),
  KEY "AAAARQLF" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQLH" (
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
  UNIQUE KEY "AAAARQLU" ("AAABLLNL"),
  KEY "AAAARQLW" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQLY" (
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
  UNIQUE KEY "AAAARQML" ("AAABLLNL"),
  KEY "AAAARQMN" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQMP" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNI","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAARQNC" ("AAABLLMD","AAABLLME","AAABLLNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQNG" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNI","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAARQNT" ("AAABLLMD","AAABLLME","AAABLLNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQNX" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNI","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAARQOK" ("AAABLLMD","AAABLLME","AAABLLNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQOO" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNI","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAARQPB" ("AAABLLMD","AAABLLME","AAABLLNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQPF" (
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
  UNIQUE KEY "AAAARQPS" ("AAABLLNL"),
  KEY "AAAARQPU" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQPW" (
  "AAABHFOL" varchar(4) NOT NULL default ' ',
  "AAABBDMV" varchar(40) NOT NULL default ' ',
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFOL","AAABBDMV","AAABLRST","AAABMKFH","AAABLFYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQQH" (
  "AAABHFOL" varchar(4) NOT NULL default ' ',
  "AAABBDMV" varchar(40) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABHFOL","AAABBDMV","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQQP" (
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
CREATE TABLE "AAAARQRB" (
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
CREATE TABLE "AAAARQRN" (
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
CREATE TABLE "AAAARQRZ" (
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
  UNIQUE KEY "AAAARQSN" ("AAABLLNL"),
  KEY "AAAARQSP" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQSR" (
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
  UNIQUE KEY "AAAARQTF" ("AAABLLNL"),
  KEY "AAAARQTH" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQTJ" (
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
  UNIQUE KEY "AAAARQTX" ("AAABLLNL"),
  KEY "AAAARQTZ" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQUB" (
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
CREATE TABLE "AAAARQVB" (
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
CREATE TABLE "AAAARQWB" (
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
  KEY "AAAARQXB" ("AAABLLLR"),
  KEY "AAAARQXD" ("AAABHZJL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQXF" (
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
  UNIQUE KEY "AAAARQXT" ("AAABLLNL"),
  KEY "AAAARQXV" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQXX" (
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
  UNIQUE KEY "AAAARQYL" ("AAABLLNL"),
  KEY "AAAARQYN" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQYP" (
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
  UNIQUE KEY "AAAARQZD" ("AAABLLNL"),
  KEY "AAAARQZF" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQZH" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAARQZR" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARQZV" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAARRAF" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRAJ" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAARRAT" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRAX" (
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
  UNIQUE KEY "AAAARRBK" ("AAABLLNL"),
  KEY "AAAARRBM" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRBO" (
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
  UNIQUE KEY "AAAARRCB" ("AAABLLNL"),
  KEY "AAAARRCD" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRCF" (
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
  UNIQUE KEY "AAAARRCS" ("AAABLLNL"),
  KEY "AAAARRCU" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRCW" (
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
  UNIQUE KEY "AAAARRDJ" ("AAABLLNL"),
  KEY "AAAARRDL" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRDN" (
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
  UNIQUE KEY "AAAARREA" ("AAABLLNL"),
  KEY "AAAARREC" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARREE" (
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
  UNIQUE KEY "AAAARRER" ("AAABLLNL"),
  KEY "AAAARRET" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARREV" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABMRFO" varchar(10) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAAARRFA" decimal(15,0) NOT NULL default '0',
  "AAAARRFB" decimal(15,0) NOT NULL default '0',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAAARRFE" varchar(1) NOT NULL default ' ',
  "AAAARRFF" varchar(1) NOT NULL default ' ',
  "AAAARRIC" varchar(30) NOT NULL default ' ',
  "AAABFORA" varchar(8) NOT NULL default '00000000',
  "AAABLESZ" varchar(6) NOT NULL default '000000',
  "AAABDEFP" varchar(8) NOT NULL default '00000000',
  "AAABDEFQ" varchar(6) NOT NULL default '000000',
  "AAAARWTL" varchar(32) NOT NULL default ' ',
  "AAABCROU" varchar(8) NOT NULL default ' ',
  "AAABCRSF" varchar(1) NOT NULL default ' ',
  "AAABLESX" varchar(32) NOT NULL default ' ',
  "AAABLESY" varchar(64) NOT NULL default ' ',
  "AAAARWTQ" varchar(1) NOT NULL default ' ',
  "AAABCRXB" varchar(2) NOT NULL default '00',
  "AAABCRXC" varchar(2) NOT NULL default '00',
  "AAABCRXD" varchar(3) NOT NULL default '000',
  "AAABCRXE" varchar(2) NOT NULL default '00',
  "AAABCRXF" varchar(2) NOT NULL default '00',
  "AAABCRXG" varchar(1) NOT NULL default ' ',
  "AAABGHVC" varchar(20) NOT NULL default ' ',
  "AAABHHSJ" varchar(2) NOT NULL default ' ',
  "AAABCRXN" int(11) NOT NULL default '0',
  "AAABCRXR" int(11) NOT NULL default '0',
  "AAAARWUB" varchar(1) NOT NULL default ' ',
  "AAABCRSH" varchar(1) NOT NULL default ' ',
  "AAAARWUD" varchar(2) NOT NULL default '00',
  "AAAARWUE" varchar(2) NOT NULL default '00',
  "AAAARWUF" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABIHKQ","AAABMKFU"),
  KEY "AAAARRGI" ("AAAARRIC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRGK" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAARRIM" varchar(30) NOT NULL default ' ',
  "AAAARYEC" varchar(6) NOT NULL default '000000',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAAARRIM","AAAARYEC","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRGV" (
  "AAAARRIM" varchar(30) NOT NULL default ' ',
  "AAABHEYG" varchar(30) NOT NULL default ' ',
  "AAAARYEC" varchar(6) NOT NULL default '000000',
  "AAABGAQM" varchar(1) NOT NULL default ' ',
  "AAAARRHA" varchar(1) NOT NULL default ' ',
  "AAAARRHP" varchar(30) NOT NULL default ' ',
  "AAABLJJO" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default '0',
  "AAAARRHE" varchar(10) NOT NULL default '0000000000',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAARRIM","AAABHEYG","AAAARYEC"),
  KEY "AAAARRHJ" ("AAAARRHP"),
  KEY "AAAARRHL" ("AAABHEYG"),
  KEY "AAAARRHN" ("AAABHEYG","AAAARRHP"),
  KEY "AAAARRHQ" ("AAAARRIM","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRHT" (
  "AAAARRIC" varchar(30) NOT NULL default ' ',
  "AAABGFLE" int(11) NOT NULL default '0',
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARRIC","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRIE" (
  "AAAARRIM" varchar(30) NOT NULL default ' ',
  "AAABHEYG" varchar(30) NOT NULL default ' ',
  "AAABGFLE" int(11) NOT NULL default '0',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  "AAAARYPF" varchar(3) NOT NULL default ' ',
  "AAAARRIL" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAAARRIM","AAABHEYG","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRIP" (
  "AAABIWQH" varchar(30) NOT NULL default ' ',
  "AAAARRIR" decimal(21,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABIWQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRIT" (
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAAARRIV" varchar(32) NOT NULL default ' ',
  "AAAARRIW" varchar(64) NOT NULL default ' ',
  "AAAARRIX" varchar(32) NOT NULL default ' ',
  "AAAARRIY" varchar(64) NOT NULL default ' ',
  "AAAARRIZ" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABIHKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRJB" (
  "AAAARRJI" varchar(12) NOT NULL default ' ',
  "AAABJOTK" varchar(4) NOT NULL default ' ',
  "AAAARRJK" varchar(14) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABMLPV" varchar(25) NOT NULL default ' ',
  "AAABMMJT" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAAARRJI","AAABJOTK","AAAARRJK","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRJM" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(200) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRJV" (
  "AAABLHAH" varchar(120) NOT NULL default ' ',
  "AAAARRKA" varchar(120) NOT NULL default ' ',
  "AAABLUZG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHAH","AAAARRKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRKB" (
  "AAABLUNJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRKE" (
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  "AAAARRKS" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABFTZK" varchar(30) NOT NULL default ' ',
  "AAABLBEI" varchar(6) NOT NULL default '000000',
  "AAABLDNU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATJNW","AAABMQDU","AAABIWFD","AAABFTZS","AAAARRKS"),
  KEY "AAAARRKT" ("AAABMQDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRKV" (
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAARRLA" varchar(2) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQDU","AAAARRLA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRLB" (
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAARRLG" varchar(3) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQDU","AAAARRLG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRLH" (
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABFYWO" varchar(30) NOT NULL default ' ',
  "AAAARRTU" varchar(30) NOT NULL default ' ',
  "AAABKQPS" varchar(4) NOT NULL default ' ',
  "AAAARRTW" varchar(1) NOT NULL default ' ',
  "AAAARRTX" varchar(1) NOT NULL default ' ',
  "AAAARSLK" varchar(1) NOT NULL default ' ',
  "AAAARRTZ" varchar(30) NOT NULL default ' ',
  "AAAARRUA" varchar(1) NOT NULL default ' ',
  "AAAARSIE" varchar(1) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAAARRLX" varchar(1) NOT NULL default ' ',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAAARRMB" varchar(30) NOT NULL default ' ',
  "AAAARRMC" varchar(1) NOT NULL default ' ',
  "AAAARRMD" varchar(1) NOT NULL default ' ',
  "AAAARXCS" varchar(1) NOT NULL default ' ',
  "AAABIWGC" varchar(10) NOT NULL default ' ',
  "AAAARRMG" varchar(1) NOT NULL default ' ',
  "AAAARRMH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATJNW","AAABMQDU","AAABIWFD"),
  UNIQUE KEY "AAAARRML" ("AAABMQDU","AAABIWFD","AAABMOHM","AAAATJNW"),
  UNIQUE KEY "AAAARRMQ" ("AAABMOHM","AAABIWFD","AAABMQDU","AAAATJNW"),
  UNIQUE KEY "AAAARRMV" ("AAABIWFD","AAABMQDU","AAABMOHM","AAAATJNW"),
  KEY "AAAARRNA" ("AAABMQDU"),
  KEY "AAAARRNC" ("AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRNE" (
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAAARSHO" varchar(3) NOT NULL default ' ',
  "AAAARSHP" varchar(3) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABLBEK" varchar(1) NOT NULL default ' ',
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  "AAABFTZK" varchar(30) NOT NULL default ' ',
  "AAABGYQP" varchar(30) NOT NULL default ' ',
  "AAABLDNU" varchar(4) NOT NULL default ' ',
  "AAABJTEQ" varchar(6) NOT NULL default '000000',
  "AAABLBEI" varchar(6) NOT NULL default '000000',
  "AAABLUWW" varchar(1) NOT NULL default ' ',
  "AAABHVRR" varchar(5) NOT NULL default ' ',
  "AAABMNMM" varchar(6) NOT NULL default '000000',
  "AAAARYDA" varchar(30) NOT NULL default ' ',
  "AAABMLPN" varchar(1) NOT NULL default ' ',
  "AAABLMVF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATJNW","AAAARSHO","AAAARSHP","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARROA" (
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABLBEK" varchar(1) NOT NULL default ' ',
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  "AAABFTZK" varchar(30) NOT NULL default ' ',
  "AAABFMTC" varchar(30) NOT NULL default ' ',
  "AAABLDNU" varchar(4) NOT NULL default ' ',
  "AAABJTEQ" varchar(6) NOT NULL default '000000',
  "AAABLBEI" varchar(6) NOT NULL default '000000',
  "AAABLUWW" varchar(1) NOT NULL default ' ',
  "AAABHVRR" varchar(5) NOT NULL default ' ',
  "AAABMNMM" varchar(6) NOT NULL default '000000',
  "AAAARYDA" varchar(30) NOT NULL default ' ',
  "AAABMLPN" varchar(1) NOT NULL default ' ',
  "AAABLMVF" varchar(1) NOT NULL default ' ',
  "AAAARROS" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAATJNW","AAABMQDU","AAABIWFD","AAABIHLK"),
  UNIQUE KEY "AAAARROX" ("AAABIWFD","AAAATJNW","AAABMQDU","AAABIHLK"),
  KEY "AAAARRPC" ("AAABFTZK"),
  KEY "AAAARRPE" ("AAABFTZS"),
  KEY "AAAARRPG" ("AAABFMTC"),
  KEY "AAAARRPI" ("AAABMQDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRPK" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTO" varchar(40) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAATJNW","AAABMQDU","AAABIWFD","AAABIHLK"),
  UNIQUE KEY "AAAARRPZ" ("AAAATJNW","AAABMQDU","AAABIWFD","AAABIHLK","AAABMLQA"),
  UNIQUE KEY "AAAARRQF" ("AAABMLQA","AAABIWFD","AAAATJNW","AAABMQDU","AAABIHLK"),
  KEY "AAAARRQL" ("AAABMQDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRQN" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAAARSHO" varchar(3) NOT NULL default ' ',
  "AAAARSHP" varchar(3) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTO" varchar(40) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAATJNW","AAAARSHO","AAAARSHP","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRRB" (
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABDSZW" varchar(1) NOT NULL default ' ',
  "AAAARRRG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAATJNW","AAABMQDU","AAABIWFD","AAABDSZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRRL" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABDSZW" varchar(1) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTO" varchar(40) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAATJNW","AAABMQDU","AAABIWFD","AAABDSZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRRZ" (
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABDSZW" varchar(1) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABLBEK" varchar(1) NOT NULL default ' ',
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  "AAABFTZK" varchar(30) NOT NULL default ' ',
  "AAABFMTC" varchar(30) NOT NULL default ' ',
  "AAABLDNU" varchar(4) NOT NULL default ' ',
  "AAABJTEQ" varchar(6) NOT NULL default '000000',
  "AAABLBEI" varchar(6) NOT NULL default '000000',
  "AAABLUWW" varchar(1) NOT NULL default ' ',
  "AAABHVRR" varchar(5) NOT NULL default ' ',
  "AAABMNMM" varchar(6) NOT NULL default '000000',
  "AAAARYDA" varchar(30) NOT NULL default ' ',
  "AAABMLPN" varchar(1) NOT NULL default ' ',
  "AAABLMVF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATJNW","AAABMQDU","AAABIWFD","AAABDSZW","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRSX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABDSZW" varchar(1) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTO" varchar(40) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAATJNW","AAABMQDU","AAABIWFD","AAABDSZW","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRTO" (
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAAARSHO" varchar(3) NOT NULL default ' ',
  "AAAARSHP" varchar(3) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABFYWO" varchar(30) NOT NULL default ' ',
  "AAAARRTU" varchar(50) NOT NULL default ' ',
  "AAABKQPS" varchar(1) NOT NULL default ' ',
  "AAAARRTW" varchar(1) NOT NULL default ' ',
  "AAAARRTX" varchar(1) NOT NULL default ' ',
  "AAAARSLK" varchar(1) NOT NULL default ' ',
  "AAAARRTZ" varchar(27) NOT NULL default ' ',
  "AAAARRUA" varchar(1) NOT NULL default ' ',
  "AAAARSIE" varchar(1) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABIWGC" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAATJNW","AAAARSHO","AAAARSHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRUI" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTO" varchar(40) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAATJNW","AAABMQDU","AAABIWFD"),
  UNIQUE KEY "AAAARRUU" ("AAAATJNW","AAABMQDU","AAABIWFD","AAABMLQA"),
  UNIQUE KEY "AAAARRUZ" ("AAABMQDU","AAABIWFD","AAAATJNW","AAABMLQA"),
  UNIQUE KEY "AAAARRVE" ("AAABMLQA","AAABMQDU","AAABIWFD","AAAATJNW"),
  UNIQUE KEY "AAAARRVJ" ("AAABIWFD","AAAATJNW","AAABMQDU","AAABMLQA"),
  KEY "AAAARRVO" ("AAABMLQA","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRVR" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAAARSHO" varchar(3) NOT NULL default ' ',
  "AAAARSHP" varchar(3) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTO" varchar(40) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAATJNW","AAAARSHO","AAAARSHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRWD" (
  "AAAARRXT" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABHFOL" varchar(4) NOT NULL default ' ',
  "AAABBDMV" varchar(40) NOT NULL default ' ',
  "AAAARRWI" varchar(1) NOT NULL default ' ',
  "AAAARRWJ" varchar(1) NOT NULL default ' ',
  "AAABGKLP" varchar(1) NOT NULL default ' ',
  "AAABDBLY" varchar(1) NOT NULL default ' ',
  "AAAARRWM" varchar(1) NOT NULL default ' ',
  "AAAARRWN" varchar(1) NOT NULL default ' ',
  "AAAARRWO" varchar(1) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAABIHMA" varchar(3) NOT NULL default ' ',
  "AAAARRWY" varchar(3) NOT NULL default ' ',
  "AAABIWGC" varchar(10) NOT NULL default ' ',
  "AAAARRXA" decimal(21,6) NOT NULL default '0.000000',
  "AAAARRXB" decimal(21,6) NOT NULL default '0.000000',
  "AAAARRXC" decimal(21,6) NOT NULL default '0.000000',
  "AAAARRXD" decimal(21,6) NOT NULL default '0.000000',
  "AAAARRXE" decimal(21,6) NOT NULL default '0.000000',
  "AAAARRXF" decimal(21,6) NOT NULL default '0.000000',
  "AAAARRXG" decimal(21,6) NOT NULL default '0.000000',
  PRIMARY KEY ("AAAARRXT","AAABIWFD","AAABHFOL","AAABBDMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRXL" (
  "AAAARRXT" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABHFOL" varchar(4) NOT NULL default ' ',
  "AAABBDMV" varchar(40) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAARRXT","AAABIWFD","AAABHFOL","AAABBDMV","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRXY" (
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABGZSJ" varchar(32) NOT NULL default ' ',
  "AAAARRYB" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLZKF","AAABGZSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRYF" (
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABGZSJ" varchar(32) NOT NULL default ' ',
  "AAABMMJT" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLZKF","AAABGZSJ","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRYM" (
  "AAAARSMS" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABFYWO" varchar(30) NOT NULL default ' ',
  "AAAARSLH" varchar(1) NOT NULL default ' ',
  "AAAARSLI" varchar(1) NOT NULL default ' ',
  "AAAARSLJ" varchar(1) NOT NULL default ' ',
  "AAAARSLK" varchar(1) NOT NULL default ' ',
  "AAAARYFH" varchar(30) NOT NULL default ' ',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAARSMS","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRZA" (
  "AAAARSMS" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARSMS","AAABIWFD","AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARRZH" (
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARYGG" varchar(4) NOT NULL default ' ',
  "AAAARSMS" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABLBEK" varchar(1) NOT NULL default ' ',
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  "AAABFTZK" varchar(30) NOT NULL default ' ',
  "AAABLDNU" varchar(4) NOT NULL default ' ',
  "AAABJTEQ" varchar(6) NOT NULL default '000000',
  "AAABLBEI" varchar(6) NOT NULL default '000000',
  "AAABLUWW" varchar(1) NOT NULL default ' ',
  "AAABHVRR" varchar(5) NOT NULL default ' ',
  "AAABMNMM" varchar(6) NOT NULL default '000000',
  "AAAARYDA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARZRE","AAAARYGG","AAAARSMS","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSAB" (
  "AAAARSMS" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARSMS","AAABIWFD","AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSAI" (
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABGGAL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATJNW","AAABMQDU","AAABIWFD","AAABFTZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSAT" (
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAAARYGG" varchar(4) NOT NULL default ' ',
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAATJNW","AAAARYGG","AAABFTZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSBB" (
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARYGG" varchar(4) NOT NULL default ' ',
  "AAAARSMS" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABLBEK" varchar(1) NOT NULL default ' ',
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  "AAABFTZK" varchar(30) NOT NULL default ' ',
  "AAABLDNU" varchar(4) NOT NULL default ' ',
  "AAABJTEQ" varchar(6) NOT NULL default '000000',
  "AAABLBEI" varchar(6) NOT NULL default '000000',
  "AAABLUWW" varchar(1) NOT NULL default ' ',
  "AAABHVRR" varchar(5) NOT NULL default ' ',
  "AAABMNMM" varchar(6) NOT NULL default '000000',
  "AAAARYDA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARZRE","AAAARYGG","AAAARSMS","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSBV" (
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAABJBGU" varchar(4) NOT NULL default ' ',
  "AAABLUSV" varchar(30) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAATJNW","AAABJBGU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSCC" (
  "AAABLUSV" varchar(30) NOT NULL default ' ',
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABFTZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSCI" (
  "AAAARSMS" varchar(30) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABLBEK" varchar(1) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  "AAABFTZK" varchar(30) NOT NULL default ' ',
  "AAABLDNU" varchar(4) NOT NULL default ' ',
  "AAABJTEQ" varchar(6) NOT NULL default '000000',
  "AAABLBEI" varchar(6) NOT NULL default '000000',
  "AAABLUWW" varchar(1) NOT NULL default ' ',
  "AAABHVRR" varchar(5) NOT NULL default ' ',
  "AAABMNMM" varchar(6) NOT NULL default '000000',
  "AAAARYDA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARSMS","AAABMQDU","AAABIWFD","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSDC" (
  "AAAARSMS" varchar(30) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  "AAABFTZK" varchar(30) NOT NULL default ' ',
  "AAABLDNU" varchar(4) NOT NULL default ' ',
  "AAABJTEQ" varchar(6) NOT NULL default '000000',
  "AAABLBEI" varchar(6) NOT NULL default '000000',
  "AAABLUWW" varchar(1) NOT NULL default ' ',
  "AAABHVRR" varchar(5) NOT NULL default ' ',
  "AAABMNMM" varchar(6) NOT NULL default '000000',
  "AAAARYDA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARSMS","AAABMQDU","AAABIWFD","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSDV" (
  "AAAARSMS" varchar(30) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWLU" varchar(30) NOT NULL default ' ',
  "AAAARSHR" varchar(3) NOT NULL default ' ',
  "AAAARSHS" varchar(8) NOT NULL default '00000000',
  "AAAARSHT" varchar(4) NOT NULL default ' ',
  "AAAARSHU" varchar(8) NOT NULL default '00000000',
  "AAAARSHH" varchar(30) NOT NULL default ' ',
  "AAABIVVH" varchar(25) NOT NULL default ' ',
  "AAAARSFK" varchar(1) NOT NULL default ' ',
  "AAAARSFL" varchar(1) NOT NULL default ' ',
  "AAAARSFM" varchar(1) NOT NULL default ' ',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAAARSEM" varchar(1) NOT NULL default ' ',
  "AAAARSEN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARSMS","AAABMQDU","AAABIWFD","AAABIWLU","AAAARSHR","AAAARSHS","AAAARSHT","AAAARSHU"),
  UNIQUE KEY "AAAARSEW" ("AAABIWFD","AAABIVVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSEZ" (
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAAARSHO" varchar(3) NOT NULL default ' ',
  "AAAARSHP" varchar(3) NOT NULL default ' ',
  "AAABIWLU" varchar(30) NOT NULL default ' ',
  "AAAARSHR" varchar(3) NOT NULL default ' ',
  "AAAARSHS" varchar(8) NOT NULL default '00000000',
  "AAAARSHT" varchar(4) NOT NULL default ' ',
  "AAAARSHU" varchar(8) NOT NULL default '00000000',
  "AAAARSHH" varchar(30) NOT NULL default ' ',
  "AAABIVVH" varchar(25) NOT NULL default ' ',
  "AAAARSFK" varchar(1) NOT NULL default ' ',
  "AAAARSFL" varchar(1) NOT NULL default ' ',
  "AAAARSFM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATJNW","AAAARSHO","AAAARSHP","AAABIWLU","AAAARSHR","AAAARSHS","AAAARSHT","AAAARSHU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSFV" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARSMS" varchar(30) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWLU" varchar(30) NOT NULL default ' ',
  "AAAARSHR" varchar(3) NOT NULL default ' ',
  "AAAARSHS" varchar(8) NOT NULL default '00000000',
  "AAAARSHT" varchar(4) NOT NULL default ' ',
  "AAAARSHU" varchar(8) NOT NULL default '00000000',
  "AAAARSHH" varchar(30) NOT NULL default ' ',
  "AAABIVVH" varchar(25) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTO" varchar(40) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAARSMS","AAABMQDU","AAABIWFD","AAABIWLU","AAAARSHR","AAAARSHS","AAAARSHT","AAAARSHU"),
  UNIQUE KEY "AAAARSGT" ("AAABMLQA","AAABIWFD","AAABIVVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSGX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAAARSHO" varchar(3) NOT NULL default ' ',
  "AAAARSHP" varchar(3) NOT NULL default ' ',
  "AAABIWLU" varchar(30) NOT NULL default ' ',
  "AAAARSHR" varchar(3) NOT NULL default ' ',
  "AAAARSHS" varchar(8) NOT NULL default '00000000',
  "AAAARSHT" varchar(4) NOT NULL default ' ',
  "AAAARSHU" varchar(8) NOT NULL default '00000000',
  "AAAARSHH" varchar(30) NOT NULL default ' ',
  "AAABIVVH" varchar(25) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTO" varchar(40) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAATJNW","AAAARSHO","AAAARSHP","AAABIWLU","AAAARSHR","AAAARSHS","AAAARSHT","AAAARSHU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSHV" (
  "AAAARSMS" varchar(30) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARSLH" varchar(1) NOT NULL default ' ',
  "AAAARSLI" varchar(1) NOT NULL default ' ',
  "AAAARSLJ" varchar(1) NOT NULL default ' ',
  "AAAARSLK" varchar(1) NOT NULL default ' ',
  "AAAARYGH" varchar(27) NOT NULL default ' ',
  "AAAARSIE" varchar(1) NOT NULL default ' ',
  "AAABHFPL" varchar(1) NOT NULL default ' ',
  "AAAARSLL" varchar(1) NOT NULL default ' ',
  "AAAARSLM" varchar(1) NOT NULL default ' ',
  "AAAARSLN" varchar(1) NOT NULL default ' ',
  "AAAARSLO" varchar(1) NOT NULL default ' ',
  "AAAARSLP" varchar(1) NOT NULL default ' ',
  "AAAARSIL" varchar(1) NOT NULL default ' ',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAARSMS","AAABMQDU","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSIS" (
  "AAAARSMS" varchar(30) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARSMS","AAABMQDU","AAABIWFD","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSJC" (
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABDSZW" varchar(1) NOT NULL default ' ',
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAATJNW","AAABMQDU","AAABIWFD","AAABDSZW","AAABFTZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSJO" (
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAAARYGG" varchar(4) NOT NULL default ' ',
  "AAABDSZW" varchar(1) NOT NULL default ' ',
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAATJNW","AAAARYGG","AAABDSZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSJX" (
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABDSZW" varchar(1) NOT NULL default ' ',
  "AAAARSKO" varchar(1) NOT NULL default ' ',
  "AAAARYGH" varchar(27) NOT NULL default ' ',
  PRIMARY KEY ("AAAATJNW","AAABMQDU","AAABIWFD","AAABDSZW"),
  KEY "AAAARSKI" ("AAAARYGH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSKK" (
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAAARYGG" varchar(4) NOT NULL default ' ',
  "AAABDSZW" varchar(1) NOT NULL default ' ',
  "AAAARSKO" varchar(1) NOT NULL default ' ',
  "AAAARYGH" varchar(27) NOT NULL default ' ',
  PRIMARY KEY ("AAAATJNW","AAAARYGG","AAABDSZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSKT" (
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARYGG" varchar(4) NOT NULL default ' ',
  "AAAARSMS" varchar(30) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARZRE","AAAARYGG","AAAARSMS","AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSLC" (
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARYGG" varchar(4) NOT NULL default ' ',
  "AAAARSMS" varchar(30) NOT NULL default ' ',
  "AAABFYWO" varchar(30) NOT NULL default ' ',
  "AAAARSLH" varchar(1) NOT NULL default ' ',
  "AAAARSLI" varchar(1) NOT NULL default ' ',
  "AAAARSLJ" varchar(1) NOT NULL default ' ',
  "AAAARSLK" varchar(1) NOT NULL default ' ',
  "AAAARSLL" varchar(1) NOT NULL default ' ',
  "AAAARSLM" varchar(1) NOT NULL default ' ',
  "AAAARSLN" varchar(1) NOT NULL default ' ',
  "AAAARSLO" varchar(1) NOT NULL default ' ',
  "AAAARSLP" varchar(1) NOT NULL default ' ',
  "AAAARYFH" varchar(30) NOT NULL default ' ',
  "AAAARYGH" varchar(27) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAARZRE","AAAARYGG","AAAARSMS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSLX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARSMS" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTO" varchar(40) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAARSMS","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSMH" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARYGG" varchar(4) NOT NULL default ' ',
  "AAAARSMS" varchar(30) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTO" varchar(40) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAARZRE","AAAARYGG","AAAARSMS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSMT" (
  "AAAARSND" varchar(4) NOT NULL default '0000',
  "AAAARSNE" varchar(1) NOT NULL default ' ',
  "AAABHFOL" varchar(4) NOT NULL default ' ',
  "AAABBDMV" varchar(40) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAAARSMZ" varchar(1) NOT NULL default ' ',
  "AAAARSNA" varchar(40) NOT NULL default ' ',
  "AAAARSNB" varchar(60) NOT NULL default ' ',
  "AAAARSNC" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAARSND","AAAARSNE","AAABHFOL","AAABBDMV","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSNI" (
  "AAABHFOL" varchar(4) NOT NULL default ' ',
  "AAAARSNO" varchar(4) NOT NULL default ' ',
  "AAAARSNL" varchar(1) NOT NULL default ' ',
  "AAAARSNM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFOL","AAAARSNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSNP" (
  "AAABHFOL" varchar(4) NOT NULL default ' ',
  "AAAARSNU" varchar(10) NOT NULL default ' ',
  "AAAARSNX" varchar(13) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFOL","AAAARSNU"),
  UNIQUE KEY "AAAARSNV" ("AAABHFOL","AAAARSNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSNY" (
  "AAABMLPV" varchar(120) NOT NULL default ' ',
  "AAAARSOD" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV"),
  UNIQUE KEY "AAAARSOC" ("AAAARSOD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSOE" (
  "AAABMLPV" varchar(32) NOT NULL default ' ',
  "AAAARSOG" decimal(15,0) NOT NULL default '0',
  "AAAARSOH" varchar(12) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default '0',
  "AAABLHAH" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSOL" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(200) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSOU" (
  "AAABMLPV" varchar(32) NOT NULL default ' ',
  "AAAARSPC" decimal(15,0) NOT NULL default '0',
  "AAAARSOX" varchar(12) NOT NULL default ' ',
  "AAABLEJE" int(11) NOT NULL default '0',
  "AAABJRGL" varchar(100) NOT NULL default ' ',
  "AAAARSPA" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV","AAAARSPC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSPD" (
  "AAAARXDL" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARYHM" varchar(30) NOT NULL default ' ',
  "AAABIJSB" varchar(4) NOT NULL default '0000',
  "AAAARSPI" varchar(1) NOT NULL default ' ',
  "AAAARSRF" varchar(4) NOT NULL default '0000',
  "AAAARSPK" varchar(1) NOT NULL default ' ',
  "AAABGFLE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAARXDL","AAABIWFD","AAAARYHM"),
  KEY "AAAARSPP" ("AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSPR" (
  "AAABMOIB" varchar(30) NOT NULL default ' ',
  "AAABHFAP" varchar(30) NOT NULL default ' ',
  "AAABHGJR" varchar(25) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABLBET" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOIB","AAABHFAP"),
  KEY "AAAARSQB" ("AAABMOIB","AAABLVCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSQE" (
  "AAABLRXL" varchar(2) NOT NULL default '00',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLRXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSQI" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLRXL" varchar(2) NOT NULL default '00',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLRXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSQO" (
  "AAAARTFE" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  "AAABMOIB" varchar(30) NOT NULL default ' ',
  "AAABGFLE" varchar(10) NOT NULL default ' ',
  "AAAARSZK" varchar(32) NOT NULL default ' ',
  "AAAARSZL" varchar(64) NOT NULL default ' ',
  "AAAARSQW" varchar(2) NOT NULL default '00',
  "AAAARSYG" varchar(1) NOT NULL default ' ',
  "AAAARSQY" varchar(32) NOT NULL default ' ',
  "AAAARSQZ" varchar(64) NOT NULL default ' ',
  "AAAARSRA" varchar(1) NOT NULL default ' ',
  "AAAARSRB" varchar(32) NOT NULL default ' ',
  "AAAARSRC" varchar(64) NOT NULL default ' ',
  "AAAARSRD" varchar(1) NOT NULL default ' ',
  "AAAARSRE" varchar(1) NOT NULL default ' ',
  "AAAARSRF" int(11) NOT NULL default '0',
  "AAAARSRG" varchar(30) NOT NULL default ' ',
  "AAAARSRH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARTFE","AAABIWFD","AAABMOHM","AAABMOIB","AAABGFLE"),
  KEY "AAAARSRN" ("AAABMOHM","AAABMOIB","AAABIWFD","AAABGFLE"),
  KEY "AAAARSRS" ("AAABMOHM","AAABIWFD","AAABGFLE"),
  KEY "AAAARSRW" ("AAABIWFD","AAAARSZK","AAAARSZL"),
  KEY "AAAARSSA" ("AAABMOHM","AAABMOIB","AAABIWFD","AAAARSZK","AAAARSZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSSG" (
  "AAAARTFE" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABIWGC" varchar(10) NOT NULL default ' ',
  "AAABLITM" varchar(32) NOT NULL default ' ',
  "AAAARSSP" varchar(1) NOT NULL default ' ',
  "AAABHHSB" decimal(15,0) NOT NULL default '0',
  "AAAARSSX" varchar(1) NOT NULL default ' ',
  "AAABFYWO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARTFE","AAABIWFD"),
  KEY "AAAARSSV" ("AAABIWFD","AAAARSSX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSSY" (
  "AAAARTFE" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  "AAABMOIB" varchar(30) NOT NULL default ' ',
  "AAABGFLE" varchar(10) NOT NULL default ' ',
  "AAABLRIF" varchar(2) NOT NULL default '00',
  "AAABMPMH" varchar(32) NOT NULL default ' ',
  "AAAARSTG" varchar(64) NOT NULL default ' ',
  "AAABMRCK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAARTFE","AAABIWFD","AAABMOHM","AAABMOIB","AAABGFLE","AAABLRIF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSTO" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARTFE" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAARTFE","AAABIWFD"),
  KEY "AAAARSTW" ("AAAARTFE","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSTZ" (
  "AAABMOIB" varchar(30) NOT NULL default ' ',
  "AAABHFAP" varchar(30) NOT NULL default ' ',
  "AAABGFLE" int(11) NOT NULL default '0',
  "AAABJSDF" varchar(3) NOT NULL default ' ',
  "AAABKNBI" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOIB","AAABHFAP","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSUI" (
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  "AAABMOIB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM","AAABMOIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSUN" (
  "AAABMOIB" varchar(30) NOT NULL default ' ',
  "AAABHGJR" varchar(25) NOT NULL default ' ',
  "AAABHFAP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOIB","AAABHGJR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSUT" (
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABJECQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSUW" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABJECQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSVC" (
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  "AAABMOIB" varchar(30) NOT NULL default ' ',
  "AAABHFAP" varchar(30) NOT NULL default ' ',
  "AAABGFLE" varchar(10) NOT NULL default ' ',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(45) NOT NULL default ' ',
  "AAABMNNA" varchar(45) NOT NULL default ' ',
  "AAAARSVM" varchar(10) NOT NULL default ' ',
  "AAAARSVN" varchar(30) NOT NULL default ' ',
  "AAAARSVO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM","AAABMOIB","AAABHFAP","AAABGFLE"),
  KEY "AAAARSVT" ("AAABJECQ","AAABMNMZ","AAABMNNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSVX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  "AAABMOIB" varchar(30) NOT NULL default ' ',
  "AAABHFAP" varchar(30) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMOHM","AAABMOIB","AAABHFAP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSWH" (
  "AAAARTFE" varchar(25) NOT NULL default ' ',
  "AAABHGJR" varchar(25) NOT NULL default ' ',
  "AAAARSWK" varchar(10) NOT NULL default ' ',
  "AAAARSWL" varchar(30) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  "AAAARSXF" varchar(1) NOT NULL default ' ',
  "AAAARTAA" varchar(25) NOT NULL default ' ',
  "AAAARSWQ" decimal(21,7) NOT NULL default '0.0000000',
  "AAAARSWR" varchar(32) NOT NULL default ' ',
  "AAAARSWS" varchar(12) NOT NULL default '000000000000',
  "AAAARSWT" varchar(10) NOT NULL default ' ',
  "AAAARSWU" decimal(15,0) NOT NULL default '0',
  "AAAARSWV" varchar(1) NOT NULL default ' ',
  "AAAARSWW" varchar(1) NOT NULL default ' ',
  "AAAARSWX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARTFE","AAABHGJR"),
  KEY "AAAARSXA" ("AAABHGJR"),
  KEY "AAAARSXC" ("AAAARTFE","AAABMRFQ","AAAARSXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSXG" (
  "AAAARTFE" varchar(25) NOT NULL default ' ',
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  "AAABMOIB" varchar(30) NOT NULL default ' ',
  "AAAARSXP" varchar(25) NOT NULL default ' ',
  "AAAARSXL" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAAARTFE","AAABMOHM","AAABMOIB","AAAARSXP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSXQ" (
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  "AAABMOIB" varchar(30) NOT NULL default ' ',
  "AAABHFAP" varchar(30) NOT NULL default ' ',
  "AAABLDKQ" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM","AAABMOIB","AAABHFAP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSXY" (
  "AAABHGJR" varchar(25) NOT NULL default ' ',
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  "AAAARSZK" varchar(32) NOT NULL default ' ',
  "AAAARSZL" varchar(64) NOT NULL default ' ',
  "AAABHJBN" varchar(8) NOT NULL default ' ',
  "AAAARSZN" varchar(8) NOT NULL default '00000000',
  "AAAARSZO" varchar(6) NOT NULL default '000000',
  "AAAARSYG" varchar(1) NOT NULL default ' ',
  "AAABMOIB" varchar(30) NOT NULL default ' ',
  "AAABHFAP" varchar(30) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAABCKCW" varchar(32) NOT NULL default ' ',
  "AAAARSYL" varchar(64) NOT NULL default ' ',
  "AAAARSYM" varchar(1) NOT NULL default ' ',
  "AAAARSYN" varchar(1) NOT NULL default ' ',
  "AAAARSYO" decimal(21,7) NOT NULL default '0.0000000',
  "AAAARSZR" decimal(21,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABHGJR","AAABMOHM","AAAARSZK","AAAARSZL","AAABHJBN","AAAARSZN","AAAARSZO"),
  KEY "AAAARSYX" ("AAAARSZK","AAAARSZL","AAABHJBN","AAAARSZN","AAAARSZO"),
  KEY "AAAARSZD" ("AAABHGJR","AAABMOHM","AAABMOIB","AAABHFAP"),
  KEY "AAAARSZI" ("AAABMOHM","AAAARSZK","AAAARSZL","AAABHJBN","AAAARSZN","AAAARSZO","AAABMOIB"),
  KEY "AAAARSZQ" ("AAAARSZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARSZS" (
  "AAAARTAA" varchar(25) NOT NULL default ' ',
  "AAABGFLE" varchar(10) NOT NULL default ' ',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(45) NOT NULL default ' ',
  "AAABMNNA" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAAARTAA","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTAC" (
  "AAABMOIB" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABFORA" varchar(8) NOT NULL default '00000000',
  "AAABLESZ" varchar(6) NOT NULL default '000000',
  "AAABDEFP" varchar(8) NOT NULL default '00000000',
  "AAABDEFQ" varchar(6) NOT NULL default '000000',
  "AAAARWTL" varchar(32) NOT NULL default ' ',
  "AAABCROU" varchar(8) NOT NULL default ' ',
  "AAABCRSF" varchar(1) NOT NULL default ' ',
  "AAABLESX" varchar(32) NOT NULL default ' ',
  "AAABLESY" varchar(64) NOT NULL default ' ',
  "AAAARWTQ" varchar(1) NOT NULL default ' ',
  "AAABCRXB" varchar(2) NOT NULL default '00',
  "AAABCRXC" varchar(2) NOT NULL default '00',
  "AAABCRXD" varchar(3) NOT NULL default '000',
  "AAABCRXE" varchar(2) NOT NULL default '00',
  "AAABCRXF" varchar(2) NOT NULL default '00',
  "AAABCRXG" varchar(1) NOT NULL default ' ',
  "AAABGHVC" varchar(20) NOT NULL default ' ',
  "AAABHHSJ" varchar(2) NOT NULL default ' ',
  "AAABCRXN" int(11) NOT NULL default '0',
  "AAABCRXR" int(11) NOT NULL default '0',
  "AAAARWUB" varchar(1) NOT NULL default ' ',
  "AAABCRSH" varchar(1) NOT NULL default ' ',
  "AAAARWUD" varchar(2) NOT NULL default '00',
  "AAAARWUE" varchar(2) NOT NULL default '00',
  "AAAARWUF" varchar(8) NOT NULL default '00000000',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAAARTBI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOIB","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTBL" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMOIB" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMOIB","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTBT" (
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  "AAABGKLP" varchar(10) NOT NULL default ' ',
  "AAABCNZN" varchar(1) NOT NULL default ' ',
  "AAABJSWG" varchar(1) NOT NULL default ' ',
  "AAAARTBY" varchar(1) NOT NULL default ' ',
  "AAAARTBZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM","AAABGKLP"),
  KEY "AAAARTCC" ("AAABGKLP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTCE" (
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  "AAAARXJM" varchar(10) NOT NULL default ' ',
  "AAAARTCH" varchar(10) NOT NULL default ' ',
  "AAABLUEO" varchar(1) NOT NULL default ' ',
  "AAAARTCJ" varchar(1) NOT NULL default ' ',
  "AAAARTCK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM","AAAARXJM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTCN" (
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  "AAABMOIB" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABGFLE" varchar(10) NOT NULL default ' ',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(45) NOT NULL default ' ',
  "AAABMNNA" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM","AAABMOIB","AAABIWFD","AAABGFLE"),
  KEY "AAAARTDB" ("AAABJECQ","AAABMNMZ","AAABMNNA"),
  KEY "AAAARTDF" ("AAABMOHM","AAABIWFD","AAABJECQ","AAABMNML","AAABIJIG","AAABMNMZ","AAABMNNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTDN" (
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  "AAABMOIB" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABIWGC" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM","AAABMOIB","AAABIWFD"),
  KEY "AAAARTDZ" ("AAABMOHM","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTEC" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  "AAABMOIB" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMOHM","AAABMOIB","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTEM" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABMOIB" varchar(30) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHFAP" varchar(30) NOT NULL default ' ',
  "AAABHGJR" varchar(25) NOT NULL default ' ',
  "AAAARTFE" varchar(25) NOT NULL default ' ',
  "AAABHIXN" varchar(32) NOT NULL default ' ',
  "AAAAUXRR" varchar(12) NOT NULL default '000000000000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABMOIB","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTFB" (
  "AAAARTFE" varchar(25) NOT NULL default ' ',
  "AAAARTFD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARTFE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTFF" (
  "AAABFYWO" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAAARTFJ" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(30) NOT NULL default ' ',
  "AAABIHLK" smallint(6) NOT NULL default '0',
  "AAAARTFM" varchar(1) NOT NULL default ' ',
  "AAAARTFN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFYWO","AAABIWFD","AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTFR" (
  "AAABFYWO" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAARTFV" varchar(30) NOT NULL default ' ',
  "AAAARTFW" varchar(30) NOT NULL default ' ',
  "AAAARTFX" varchar(30) NOT NULL default ' ',
  "AAABFZAB" varchar(40) NOT NULL default ' ',
  "AAAARTFZ" varchar(30) NOT NULL default ' ',
  "AAAARTGA" varchar(30) NOT NULL default ' ',
  "AAABGWOD" int(11) NOT NULL default '0',
  "AAABLTVR" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABFYWO","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTGF" (
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABLUSV" varchar(2) NOT NULL default '00',
  "AAABHHCG" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMODI","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTGL" (
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAAARTGN" varchar(80) NOT NULL default ' ',
  "AAAARTGO" varchar(1) NOT NULL default ' ',
  "AAAARTGP" varchar(255) NOT NULL default ' ',
  "AAAARTGQ" varchar(255) NOT NULL default ' ',
  "AAAARTGR" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMODI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTGT" (
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  "AAABMKFH" varchar(30) NOT NULL default ' ',
  "AAABMIRY" varchar(2) NOT NULL default ' ',
  "AAAARTGX" varchar(1) NOT NULL default '0',
  "AAAARTGY" varchar(1) NOT NULL default ' ',
  "AAAARTGZ" varchar(1) NOT NULL default ' ',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  "AAAARTHB" varchar(1) NOT NULL default ' ',
  "AAAARTHC" varchar(1) NOT NULL default ' ',
  "AAABLRXL" varchar(2) NOT NULL default '00',
  "AAAARTHE" varchar(2) NOT NULL default '00',
  "AAAARTHF" varchar(2) NOT NULL default ' ',
  "AAAARTHG" varchar(60) NOT NULL default ' ',
  "AAABIWGC" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTHJ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  "AAABIFOM" varchar(20) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTHQ" (
  "AAAARYDV" varchar(30) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAAARXBU" varchar(32) NOT NULL default ' ',
  "AAAARXBV" varchar(64) NOT NULL default ' ',
  "AAAARXBW" varchar(32) NOT NULL default ' ',
  "AAAARXBX" varchar(64) NOT NULL default ' ',
  "AAAARTHZ" int(11) NOT NULL default '0',
  "AAAARTIA" varchar(8) NOT NULL default '00000000',
  "AAAARTIB" varchar(1) NOT NULL default ' ',
  "AAAARTIC" varchar(1) NOT NULL default ' ',
  "AAAARTID" varchar(1) NOT NULL default ' ',
  "AAAARTIE" varchar(1) NOT NULL default ' ',
  "AAABFORA" varchar(8) NOT NULL default '00000000',
  "AAABLESZ" varchar(6) NOT NULL default '000000',
  "AAABDEFP" varchar(8) NOT NULL default '00000000',
  "AAABDEFQ" varchar(6) NOT NULL default '000000',
  "AAAARWTL" varchar(32) NOT NULL default ' ',
  "AAABCROU" varchar(8) NOT NULL default ' ',
  "AAABCRSF" varchar(1) NOT NULL default ' ',
  "AAABLESX" varchar(32) NOT NULL default ' ',
  "AAABLESY" varchar(64) NOT NULL default ' ',
  "AAAARWTQ" varchar(1) NOT NULL default ' ',
  "AAABCRXB" varchar(2) NOT NULL default '00',
  "AAABCRXC" varchar(2) NOT NULL default '00',
  "AAABCRXD" varchar(3) NOT NULL default '000',
  "AAABCRXE" varchar(2) NOT NULL default '00',
  "AAABCRXF" varchar(2) NOT NULL default '00',
  "AAABCRXG" varchar(1) NOT NULL default ' ',
  "AAABGHVC" varchar(20) NOT NULL default ' ',
  "AAABHHSJ" varchar(2) NOT NULL default ' ',
  "AAABCRXN" int(11) NOT NULL default '0',
  "AAABCRXR" int(11) NOT NULL default '0',
  "AAAARWUB" varchar(1) NOT NULL default ' ',
  "AAABCRSH" varchar(1) NOT NULL default ' ',
  "AAAARWUD" varchar(2) NOT NULL default '00',
  "AAAARWUE" varchar(2) NOT NULL default '00',
  "AAAARWUF" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAARYDV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTJF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRZZ" varchar(10) NOT NULL default '0000000000',
  "AAABLMJL" varchar(4) NOT NULL default '0000',
  "AAABKSAB" varchar(1) NOT NULL default ' ',
  "AAABLXSX" varchar(8) NOT NULL default '00000000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABKPSV" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABKRZZ","AAABLMJL","AAABKSAB","AAABLXSX","AAABMQIL","AAABKPSV"),
  KEY "AAAARTJU" ("AAABLXSX"),
  KEY "AAAARTJW" ("AAABMQIL","AAABKPSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTJZ" (
  "AAABJSPH" varchar(10) NOT NULL default ' ',
  "AAAARTKB" varchar(10) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJSPH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTKF" (
  "AAAARTPY" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARTLE" varchar(6) NOT NULL default ' ',
  "AAAARTPQ" varchar(6) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABEPJZ" varchar(2) NOT NULL default ' ',
  "AAAARTKM" varchar(6) NOT NULL default ' ',
  "AAABIRTO" varchar(1) NOT NULL default ' ',
  "AAABMKGE" varchar(1) NOT NULL default ' ',
  "AAAARTKP" varchar(1) NOT NULL default ' ',
  "AAAARTKQ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAAARTPY","AAABIWFD","AAAARTLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTKU" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARTPY" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARTLE" varchar(6) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAARTPY","AAABIWFD","AAAARTLE"),
  KEY "AAAARTLF" ("AAAARTPY","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTLI" (
  "AAAARTPY" varchar(30) NOT NULL default ' ',
  "AAABIWNE" varchar(30) NOT NULL default ' ',
  "AAABMMRH" varchar(5) NOT NULL default '00000',
  "AAABHFOL" varchar(4) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  "AAAARTLU" varchar(1) NOT NULL default ' ',
  "AAABLRXL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAAARTPY","AAABIWNE","AAABMMRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTLZ" (
  "AAAARTPY" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  "AAAAWYIH" varchar(30) NOT NULL default ' ',
  "AAABIWGC" varchar(10) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAAARTMM" varchar(12) NOT NULL default ' ',
  "AAAARTMN" decimal(15,0) NOT NULL default '0',
  "AAAARTMO" varchar(1) NOT NULL default ' ',
  "AAAARTMP" varchar(1) NOT NULL default ' ',
  "AAABFNLO" varchar(1) NOT NULL default ' ',
  "AAAARVTY" varchar(1) NOT NULL default ' ',
  "AAAARTMS" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAARTPY","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTMV" (
  "AAAARTPY" varchar(30) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARTPY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTNA" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARTPY" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAARTPY","AAABIWFD"),
  KEY "AAAARTNJ" ("AAAARTPY","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTNM" (
  "AAAARTPY" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABCGEQ" varchar(2) NOT NULL default '00',
  "AAAARTNQ" varchar(2) NOT NULL default '00',
  "AAAARTNR" varchar(5) NOT NULL default '00000',
  "AAAARTNS" varchar(5) NOT NULL default '00000',
  "AAAARTNT" varchar(30) NOT NULL default ' ',
  "AAAAXMXX" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAAARTPY","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTNX" (
  "AAAARTPY" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARTOA" longtext,
  PRIMARY KEY ("AAAARTPY","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTOD" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(40) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTOM" (
  "AAAARTPY" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARTPQ" varchar(6) NOT NULL default ' ',
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAABLXIY" varchar(30) NOT NULL default ' ',
  "AAABBRWV" varchar(2) NOT NULL default ' ',
  "AAABLTQP" varchar(8) NOT NULL default '00000000',
  "AAAARTOU" varchar(1) NOT NULL default ' ',
  "AAAARTOV" varchar(8) NOT NULL default '00000000',
  "AAAARTOW" varchar(8) NOT NULL default '00000000',
  "AAAARTOX" varchar(8) NOT NULL default '00000000',
  "AAAARTOY" varchar(8) NOT NULL default '00000000',
  "AAAARTOZ" varchar(12) NOT NULL default ' ',
  "AAAARTPA" varchar(12) NOT NULL default ' ',
  "AAAARTPB" varchar(12) NOT NULL default ' ',
  "AAAARTPC" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAAARTPY","AAABIWFD","AAAARTPQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTPG" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARTPY" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARTPQ" varchar(6) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAARTPY","AAABIWFD","AAAARTPQ"),
  KEY "AAAARTPR" ("AAAARTPY","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTPU" (
  "AAAARTPY" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARTPX" longtext,
  PRIMARY KEY ("AAAARTPY","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTQA" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAARTQC" varchar(1) NOT NULL default ' ',
  "AAAARTQD" varchar(1) NOT NULL default ' ',
  "AAAARTQE" varchar(1) NOT NULL default ' ',
  "AAABMCMR" varchar(1) NOT NULL default ' ',
  "AAABLUHK" varchar(1) NOT NULL default ' ',
  "AAABJXTS" varchar(1) NOT NULL default ' ',
  "AAABMIPG" varchar(1) NOT NULL default ' ',
  "AAABMFSV" varchar(1) NOT NULL default ' ',
  "AAAARTQK" varchar(1) NOT NULL default '0',
  "AAAARYYT" varchar(30) NOT NULL default ' ',
  "AAAARTQM" varchar(1) NOT NULL default ' ',
  "AAAARTQN" int(11) NOT NULL default '0',
  "AAAARTQO" varchar(10) NOT NULL default ' ',
  "AAAARTQP" varchar(30) NOT NULL default ' ',
  "AAAARTQQ" varchar(10) NOT NULL default ' ',
  "AAAARTQR" varchar(1) NOT NULL default ' ',
  "AAAATWYD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTQU" (
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABIKVA" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLZKF","AAABIKVA","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTRB" (
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAAARUQN" varchar(30) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default '0000',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAABLOKZ" decimal(15,0) NOT NULL default '0',
  "AAABIMRZ" varchar(30) NOT NULL default ' ',
  "AAABMBXY" varchar(30) NOT NULL default ' ',
  "AAABEMYQ" varchar(30) NOT NULL default ' ',
  "AAABLFYJ" varchar(30) NOT NULL default ' ',
  "AAAARUJD" varchar(30) NOT NULL default ' ',
  "AAAARUJE" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABIWQH" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAARUJJ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTJH","AAABLTJI","AAAARUQN","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTRX" (
  "AAAARUQN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABGFLE" varchar(10) NOT NULL default ' ',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  "AAABFSAL" varchar(10) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(128) NOT NULL default ' ',
  "AAABMNNA" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAAARUQN","AAABIWFD","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTSK" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARUQN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABGFLE" varchar(10) NOT NULL default ' ',
  "AAABMRCK" longtext,
  PRIMARY KEY ("AAABMLQA","AAAARUQN","AAABIWFD","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTSU" (
  "AAAARTSY" varchar(12) NOT NULL default ' ',
  "AAAARTSZ" varchar(30) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAARTSY","AAAARTSZ","AAABMKFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTTB" (
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAAARUQN" varchar(30) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default '0000',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAABLOKZ" decimal(15,0) NOT NULL default '0',
  "AAABIMRZ" varchar(2) NOT NULL default ' ',
  "AAABMBXY" varchar(30) NOT NULL default ' ',
  "AAABMOIB" varchar(14) NOT NULL default ' ',
  "AAAARWHO" varchar(25) NOT NULL default ' ',
  "AAABHWCF" varchar(40) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABIWQH" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAARUJJ" varchar(32) NOT NULL default ' ',
  "AAAARTTS" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTJH","AAABLTJI","AAAARUQN","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTTX" (
  "AAAARUQN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARUVE" varchar(1) NOT NULL default ' ',
  "AAAARUVF" int(11) NOT NULL default '0',
  "AAAARUVG" varchar(1) NOT NULL default ' ',
  "AAAARUVL" varchar(1) NOT NULL default ' ',
  "AAAARUVM" int(11) NOT NULL default '0',
  "AAAARUVN" int(11) NOT NULL default '0',
  "AAAARUVO" int(11) NOT NULL default '0',
  "AAAARUVP" int(11) NOT NULL default '0',
  "AAAARYTS" varchar(1) NOT NULL default ' ',
  "AAAARYTQ" varchar(1) NOT NULL default ' ',
  "AAAARYTU" varchar(1) NOT NULL default ' ',
  "AAAARUVT" varchar(1) NOT NULL default ' ',
  "AAAARUVU" varchar(1) NOT NULL default ' ',
  "AAAARTUN" int(11) NOT NULL default '0',
  "AAAARTUO" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAARUQN","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTUR" (
  "AAAARUQN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIMRZ" varchar(1) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAAARTWJ" varchar(60) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABKGBL" varchar(1) NOT NULL default ' ',
  "AAAARYUR" varchar(1) NOT NULL default ' ',
  "AAAARUBG" varchar(2) NOT NULL default '00',
  "AAAARUBH" varchar(2) NOT NULL default '00',
  "AAAARTVC" varchar(1) NOT NULL default ' ',
  "AAAARYUO" varchar(1) NOT NULL default ' ',
  "AAAARTVE" varchar(1) NOT NULL default ' ',
  "AAAARYUP" varchar(1) NOT NULL default ' ',
  "AAAARTVG" varchar(1) NOT NULL default ' ',
  "AAAARTVH" varchar(1) NOT NULL default ' ',
  "AAAARTVI" varchar(1) NOT NULL default ' ',
  "AAAARTVJ" int(11) NOT NULL default '0',
  "AAAARTVK" int(11) NOT NULL default '0',
  "AAAARTVL" varchar(1) NOT NULL default ' ',
  "AAAARTVM" varchar(1) NOT NULL default ' ',
  "AAABIWLU" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default ' ',
  "AAABLRXO" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAARUQN","AAABIWFD","AAABIMRZ","AAABIWNN","AAAARTWJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTVV" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARUQN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIMRZ" varchar(1) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAAARTWJ" varchar(60) NOT NULL default ' ',
  "AAAARTWC" varchar(60) NOT NULL default ' ',
  "AAAARTWD" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAARUQN","AAABIWFD","AAABIMRZ","AAABIWNN","AAAARTWJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTWK" (
  "AAAARUQN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABEYFW" varchar(60) NOT NULL default ' ',
  "AAABIIDV" varchar(1) NOT NULL default ' ',
  "AAAARZZP" varchar(4) NOT NULL default '0000',
  "AAAARZZQ" varchar(4) NOT NULL default '0000',
  "AAAARZZC" varchar(4) NOT NULL default '0000',
  "AAAARZZD" varchar(4) NOT NULL default '0000',
  "AAAARTWT" varchar(3) NOT NULL default '000',
  "AAAARZZF" varchar(1) NOT NULL default ' ',
  "AAABLTQJ" varchar(1) NOT NULL default ' ',
  "AAABDQTO" varchar(1) NOT NULL default ' ',
  "AAABFPZD" varchar(1) NOT NULL default ' ',
  "AAAARZZJ" varchar(1) NOT NULL default ' ',
  "AAAARZZK" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAAARUQN","AAABIWFD","AAABEYFW","AAABIIDV","AAAARZZP","AAAARZZQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTXG" (
  "AAAARUQN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABEYFW" varchar(60) NOT NULL default ' ',
  "AAABIIDV" varchar(1) NOT NULL default ' ',
  "AAAARZZW" varchar(4) NOT NULL default '0000',
  "AAAARZZX" varchar(4) NOT NULL default '0000',
  "AAABEUPL" varchar(12) NOT NULL default ' ',
  "AAAARZZZ" varchar(8) NOT NULL default '00000000',
  "AAABEUPM" varchar(6) NOT NULL default '000000',
  "AAABBKEH" varchar(12) NOT NULL default ' ',
  "AAABBKEI" varchar(8) NOT NULL default '00000000',
  "AAABBKEN" varchar(6) NOT NULL default '000000',
  "AAABJHGE" varchar(1) NOT NULL default ' ',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  "AAAARTXV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAARUQN","AAABIWFD","AAABEYFW","AAABIIDV"),
  KEY "AAAARTYA" ("AAABEYFW","AAABIIDV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTYD" (
  "AAAARUQN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABEYFW" varchar(60) NOT NULL default ' ',
  "AAABIIDV" varchar(1) NOT NULL default ' ',
  "AAAASABL" varchar(1) NOT NULL default ' ',
  "AAABKMWI" varchar(4) NOT NULL default '0000',
  "AAABMMRH" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAARUQN","AAABIWFD","AAABEYFW","AAABIIDV","AAAASABL","AAABKMWI"),
  KEY "AAAARTYR" ("AAABEYFW","AAABIIDV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTYU" (
  "AAAARUQN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABEYFW" varchar(60) NOT NULL default ' ',
  "AAABIIDV" varchar(1) NOT NULL default ' ',
  "AAAASABL" varchar(1) NOT NULL default ' ',
  "AAAASABE" varchar(12) NOT NULL default ' ',
  "AAAASABF" varchar(8) NOT NULL default '00000000',
  "AAAASABG" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAARUQN","AAABIWFD","AAABEYFW","AAABIIDV","AAAASABL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTZI" (
  "AAAARUQN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABGFLE" varchar(10) NOT NULL default ' ',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  "AAABFSAL" varchar(10) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(128) NOT NULL default ' ',
  "AAABMNNA" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAAARUQN","AAABIWFD","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARTZV" (
  "AAAARUQN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABGFLE" varchar(10) NOT NULL default ' ',
  "AAABMNMP" varchar(4) NOT NULL default '0000',
  "AAAARUAA" varchar(128) NOT NULL default ' ',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARUQN","AAABIWFD","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUAF" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARUQN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABGFLE" varchar(10) NOT NULL default ' ',
  "AAABMRCK" longtext,
  PRIMARY KEY ("AAABMLQA","AAAARUQN","AAABIWFD","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUAP" (
  "AAAARUQN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARUGY" varchar(25) NOT NULL default ' ',
  "AAABFNHW" varchar(32) NOT NULL default ' ',
  "AAAAVNUD" varchar(6) NOT NULL default '000000',
  "AAABMMJT" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAARUQN","AAABIWFD","AAAARUGY","AAABFNHW","AAAAVNUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUBB" (
  "AAAARUQN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAAARUBG" varchar(2) NOT NULL default '00',
  "AAAARUBH" varchar(2) NOT NULL default '00',
  "AAABIWLU" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default ' ',
  "AAABLRXO" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAARUQN","AAABIWFD","AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUBO" (
  "AAAARUQN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARUGY" varchar(25) NOT NULL default ' ',
  "AAAARUEP" varchar(25) NOT NULL default ' ',
  "AAABFNUR" varchar(4) NOT NULL default '0000',
  "AAAARUBU" varchar(1) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABGYUM" varchar(1) NOT NULL default ' ',
  "AAAAVLHP" varchar(1) NOT NULL default ' ',
  "AAAARUGT" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAARUQN","AAABIWFD","AAAARUGY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUCC" (
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAAARUNG" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAAARUCG" varchar(1) NOT NULL default ' ',
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABFZAB" varchar(30) NOT NULL default ' ',
  "AAAARZQS" varchar(25) NOT NULL default ' ',
  "AAABGKTI" varchar(14) NOT NULL default ' ',
  "AAAARUCL" int(11) NOT NULL default '0',
  "AAAARUCM" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLUSG","AAAARUNG","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUCQ" (
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAAARUQN" varchar(30) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default '0000',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAABLOKZ" decimal(15,0) NOT NULL default '0',
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAAARWLQ" varchar(30) NOT NULL default ' ',
  "AAABMOIB" varchar(14) NOT NULL default ' ',
  "AAABIMRZ" varchar(30) NOT NULL default ' ',
  "AAABMBXY" varchar(30) NOT NULL default ' ',
  "AAABEMYQ" varchar(30) NOT NULL default ' ',
  "AAABLFYJ" varchar(30) NOT NULL default ' ',
  "AAAARUJD" varchar(30) NOT NULL default ' ',
  "AAAARUJE" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABIWQH" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAARUDK" varchar(4) NOT NULL default '0000',
  "AAAARZBA" varchar(2) NOT NULL default '00',
  "AAAARUJJ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTJH","AAABLTJI","AAAARUQN","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUDR" (
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAAARUNG" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAAARUEP" varchar(25) NOT NULL default ' ',
  "AAAARUDW" varchar(60) NOT NULL default ' ',
  "AAABFNUR" varchar(4) NOT NULL default '0000',
  "AAAARUDY" varchar(255) NOT NULL default ' ',
  "AAABGKTX" varchar(25) NOT NULL default ' ',
  "AAAARUEA" varchar(255) NOT NULL default ' ',
  "AAAARUEB" int(11) NOT NULL default '0',
  "AAAARUEC" varchar(2) NOT NULL default '00',
  "AAAARUED" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSG","AAAARUNG","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUEH" (
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAAARUNG" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAAARUEZ" varchar(25) NOT NULL default ' ',
  "AAABFNUR" varchar(4) NOT NULL default '0000',
  "AAAARUGY" varchar(25) NOT NULL default ' ',
  "AAABGKTX" varchar(25) NOT NULL default ' ',
  "AAAARUEP" varchar(25) NOT NULL default ' ',
  "AAAARUEQ" varchar(1) NOT NULL default ' ',
  "AAAAVLHP" varchar(1) NOT NULL default ' ',
  "AAAARUGT" varchar(60) NOT NULL default ' ',
  "AAAARUET" varchar(4) NOT NULL default '0000',
  "AAAARUEU" varchar(4) NOT NULL default '0000',
  "AAAARUEV" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLUSG","AAAARUNG","AAABIHLK","AAAARUEZ","AAABFNUR","AAAARUGY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUFC" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLUSG" varchar(64) NOT NULL default ' ',
  "AAAARUNG" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABGKTX" varchar(25) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLUSG","AAAARUNG","AAABIHLK","AAABGKTX","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUFR" (
  "AAAARUQN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABGFLE" varchar(10) NOT NULL default ' ',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  "AAABFSAL" varchar(10) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(128) NOT NULL default ' ',
  "AAABMNNA" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAAARUQN","AAABIWFD","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUGE" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARUQN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABGFLE" varchar(10) NOT NULL default ' ',
  "AAABMRCK" longtext,
  PRIMARY KEY ("AAABMLQA","AAAARUQN","AAABIWFD","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUGO" (
  "AAAARUQN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARUGY" varchar(25) NOT NULL default ' ',
  "AAAAVLHP" varchar(1) NOT NULL default ' ',
  "AAAARUGT" varchar(60) NOT NULL default ' ',
  "AAABLPNX" varchar(3) NOT NULL default ' ',
  "AAABJGGL" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAAARUQN","AAABIWFD","AAAARUGY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUGZ" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAARWHO" varchar(25) NOT NULL default ' ',
  "AAABHWCF" varchar(40) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUSG" varchar(64) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAAARWHO","AAABHWCF","AAABMLQA","AAABLUSG","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUHO" (
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARUHT" varchar(30) NOT NULL default ' ',
  "AAAAWNLP" varchar(1) NOT NULL default ' ',
  "AAAARUHV" decimal(15,0) NOT NULL default '0',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTJH","AAABLTJI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUHZ" (
  "AAAARUNT" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABGFLE" varchar(10) NOT NULL default ' ',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  "AAABFSAL" varchar(10) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(128) NOT NULL default ' ',
  "AAABMNNA" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAAARUNT","AAABIWFD","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUIM" (
  "AAAARUIQ" varchar(25) NOT NULL default ' ',
  "AAABFJOS" varchar(8) NOT NULL default '00000000',
  "AAAARUIP" longtext,
  PRIMARY KEY ("AAAARUIQ","AAABFJOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUIS" (
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAAARUQN" varchar(30) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default '0000',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAABLOKZ" decimal(15,0) NOT NULL default '0',
  "AAABIMRZ" varchar(30) NOT NULL default ' ',
  "AAABMBXY" varchar(30) NOT NULL default ' ',
  "AAABEMYQ" varchar(30) NOT NULL default ' ',
  "AAABLFYJ" varchar(30) NOT NULL default ' ',
  "AAAARUJD" varchar(30) NOT NULL default ' ',
  "AAAARUJE" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABIWQH" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAARUJJ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTJH","AAABLTJI","AAAARUQN","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUJO" (
  "AAAARUQN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABGFLE" varchar(10) NOT NULL default ' ',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  "AAABFSAL" varchar(10) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(128) NOT NULL default ' ',
  "AAABMNNA" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAAARUQN","AAABIWFD","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUKB" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARUQN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABGFLE" varchar(10) NOT NULL default ' ',
  "AAABMRCK" longtext,
  PRIMARY KEY ("AAABMLQA","AAAARUQN","AAABIWFD","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUKL" (
  "AAAARUNG" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAAARUKP" varchar(4) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
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
  PRIMARY KEY ("AAAARUNG","AAABIWFD","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUMS" (
  "AAAARUNG" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAARUNG","AAABIWFD","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUNA" (
  "AAAARUNG" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIHLK" double NOT NULL default '0',
  "AAAARUQN" varchar(30) NOT NULL default ' ',
  "AAABGKTI" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAAARUNG","AAABIWFD","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUNJ" (
  "AAAARUNT" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABGFLE" varchar(10) NOT NULL default ' ',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  "AAABFSAL" varchar(10) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(128) NOT NULL default ' ',
  "AAABMNNA" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAAARUNT","AAABIWFD","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUNW" (
  "AAAARUQN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZJX" varchar(1) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAAARUOH" varchar(1) NOT NULL default ' ',
  "AAAARUOI" varchar(1) NOT NULL default ' ',
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABFZAB" varchar(30) NOT NULL default ' ',
  "AAAARUOL" varchar(1) NOT NULL default ' ',
  "AAAARUOM" tinyint(3) unsigned NOT NULL default '0',
  "AAAARUON" varchar(1) NOT NULL default ' ',
  "AAAARUOO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAAARUQN","AAABIWFD"),
  KEY "AAAARUOR" ("AAABIHKQ","AAABFZAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUOU" (
  "AAAARUQN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  "AAAARUOZ" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAAARUQN","AAABIWFD","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUPD" (
  "AAAARUQN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAAARUPT" varchar(30) NOT NULL default ' ',
  "AAAARUPV" varchar(30) NOT NULL default ' ',
  "AAABMOIB" varchar(14) NOT NULL default ' ',
  "AAAARUPK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARUQN","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUPN" (
  "AAAARUQN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARZQI" varchar(30) NOT NULL default ' ',
  "AAAARWHO" varchar(25) NOT NULL default ' ',
  "AAABAKZI" varchar(20) NOT NULL default ' ',
  "AAAARUPT" varchar(30) NOT NULL default ' ',
  "AAAARUPU" varchar(60) NOT NULL default ' ',
  "AAAARUPV" varchar(30) NOT NULL default ' ',
  "AAAARUPW" varchar(60) NOT NULL default ' ',
  "AAABJHIM" varchar(14) NOT NULL default ' ',
  "AAAARUPY" varchar(1) NOT NULL default ' ',
  "AAAARUPZ" varchar(1) NOT NULL default ' ',
  "AAAARUQA" varchar(1) NOT NULL default ' ',
  "AAAARUQB" varchar(1) NOT NULL default ' ',
  "AAAARUQC" varchar(1) NOT NULL default ' ',
  "AAAARUQD" varchar(1) NOT NULL default ' ',
  "AAAARUQE" varchar(1) NOT NULL default ' ',
  "AAAARUQF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARUQN","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUQI" (
  "AAAARUQN" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAARUQN","AAABIWFD","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUQQ" (
  "AAAARVQZ" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAAARZAE" varchar(30) NOT NULL default ' ',
  "AAAARUQV" varchar(60) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABJDQT" varchar(1) NOT NULL default ' ',
  "AAAARYUP" varchar(1) NOT NULL default ' ',
  "AAAARUTY" varchar(1) NOT NULL default ' ',
  "AAAARUTZ" varchar(1) NOT NULL default ' ',
  "AAAARUUA" varchar(1) NOT NULL default ' ',
  "AAAARUUB" int(11) NOT NULL default '0',
  "AAAARUUC" int(11) NOT NULL default '0',
  "AAAARUUD" varchar(1) NOT NULL default ' ',
  "AAAARUUE" varchar(1) NOT NULL default ' ',
  "AAAARUUR" varchar(60) NOT NULL default ' ',
  "AAAARUUS" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAARVQZ","AAABIWFD","AAABIWNN","AAAARZAE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARURM" (
  "AAAARVQZ" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAAARZAE" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARUUR" varchar(60) NOT NULL default ' ',
  "AAAARUUS" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAARVQZ","AAABIWFD","AAABIWNN","AAAARZAE","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARURZ" (
  "AAAARVQZ" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAAARUSD" varchar(60) NOT NULL default ' ',
  "AAAARUSE" varchar(1) NOT NULL default ' ',
  "AAAARUSF" varchar(1) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAAARYUP" varchar(1) NOT NULL default ' ',
  "AAAARYUQ" varchar(2) NOT NULL default '00',
  "AAAARYUR" varchar(1) NOT NULL default ' ',
  "AAAARUTY" varchar(1) NOT NULL default ' ',
  "AAAARUTZ" varchar(1) NOT NULL default ' ',
  "AAAARUUA" varchar(1) NOT NULL default ' ',
  "AAAARUUB" int(11) NOT NULL default '0',
  "AAAARUUC" int(11) NOT NULL default '0',
  "AAAARUUD" varchar(1) NOT NULL default ' ',
  "AAAARUUE" varchar(1) NOT NULL default ' ',
  "AAAARUSR" varchar(1) NOT NULL default ' ',
  "AAAARUSS" varchar(1) NOT NULL default ' ',
  "AAAARUST" varchar(1) NOT NULL default ' ',
  "AAAARUSU" varchar(1) NOT NULL default ' ',
  "AAAARUUR" varchar(60) NOT NULL default ' ',
  "AAAARUUS" varchar(60) NOT NULL default ' ',
  "AAAARUSX" varchar(60) NOT NULL default ' ',
  "AAABIWLU" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default ' ',
  "AAABLRXO" varchar(8) NOT NULL default '00000000',
  "AAAARYUO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARVQZ","AAABIWFD","AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUTF" (
  "AAAARVQZ" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARUUR" varchar(60) NOT NULL default ' ',
  "AAAARUUS" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAARVQZ","AAABIWFD","AAABIWNN","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUTQ" (
  "AAAARVQZ" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABIKGM" varchar(60) NOT NULL default ' ',
  "AAAARUTV" varchar(60) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABJDQT" varchar(1) NOT NULL default ' ',
  "AAAARUTY" varchar(1) NOT NULL default ' ',
  "AAAARUTZ" varchar(1) NOT NULL default ' ',
  "AAAARUUA" varchar(1) NOT NULL default ' ',
  "AAAARUUB" int(11) NOT NULL default '0',
  "AAAARUUC" int(11) NOT NULL default '0',
  "AAAARUUD" varchar(1) NOT NULL default ' ',
  "AAAARUUE" varchar(1) NOT NULL default ' ',
  "AAAARUUR" varchar(60) NOT NULL default ' ',
  "AAAARUUS" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAARVQZ","AAABIWFD","AAABIWNN","AAABIKGM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUUL" (
  "AAAARVQZ" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABIKGM" varchar(60) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARUUR" varchar(60) NOT NULL default ' ',
  "AAAARUUS" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAARVQZ","AAABIWFD","AAABIWNN","AAABIKGM","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUUY" (
  "AAABLUSV" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABGFYL" varchar(30) NOT NULL default ' ',
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAAARUVD" varchar(25) NOT NULL default ' ',
  "AAAARUVE" varchar(1) NOT NULL default ' ',
  "AAAARUVF" int(11) NOT NULL default '0',
  "AAAARUVG" varchar(1) NOT NULL default ' ',
  "AAAARUVH" varchar(60) NOT NULL default ' ',
  "AAAARUVI" varchar(60) NOT NULL default ' ',
  "AAAARUVJ" varchar(60) NOT NULL default ' ',
  "AAAARUVK" varchar(60) NOT NULL default ' ',
  "AAAARUVL" varchar(1) NOT NULL default ' ',
  "AAAARUVM" int(11) NOT NULL default '0',
  "AAAARUVN" int(11) NOT NULL default '0',
  "AAAARUVO" int(11) NOT NULL default '0',
  "AAAARUVP" int(11) NOT NULL default '0',
  "AAAARYTS" varchar(1) NOT NULL default ' ',
  "AAAARYTQ" varchar(1) NOT NULL default ' ',
  "AAAARYTU" varchar(1) NOT NULL default ' ',
  "AAAARUVT" varchar(1) NOT NULL default ' ',
  "AAAARUVU" varchar(1) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUSV","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUWE" (
  "AAABLUSV" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMECK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABIWFD","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUWM" (
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMRCK" varchar(10) NOT NULL default ' ',
  "AAABKCWA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUWR" (
  "AAABHDEU" varchar(40) NOT NULL default ' ',
  "AAABKERO" decimal(15,0) NOT NULL default '0',
  "AAAARZSO" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABHDEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUWW" (
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABIOBO" varchar(30) NOT NULL default ' ',
  "AAABGKVZ" varchar(10) NOT NULL default ' ',
  "AAAARUZF" varchar(10) NOT NULL default ' ',
  "AAABMLZS" varchar(3) NOT NULL default '000',
  "AAAARUXC" varchar(10) NOT NULL default ' ',
  "AAAARUXD" varchar(3) NOT NULL default '000',
  "AAAARUXE" varchar(3) NOT NULL default '000',
  "AAABJSDF" varchar(3) NOT NULL default '000',
  "AAABLFTW" varchar(3) NOT NULL default '000',
  "AAAARUXH" varchar(3) NOT NULL default '000',
  "AAAARUXI" varchar(3) NOT NULL default '000',
  "AAAARUXJ" varchar(3) NOT NULL default '000',
  "AAABLBEK" varchar(1) NOT NULL default ' ',
  "AAAARVRX" varchar(1) NOT NULL default ' ',
  "AAAARUXM" varchar(1) NOT NULL default ' ',
  "AAABLNNZ" varchar(255) NOT NULL default ' ',
  "AAAARZJJ" varchar(2) NOT NULL default ' ',
  "AAAARZJK" varchar(30) NOT NULL default ' ',
  "AAAARUXQ" varchar(2) NOT NULL default ' ',
  "AAAARVSC" varchar(30) NOT NULL default ' ',
  "AAAARUXS" varchar(1) NOT NULL default ' ',
  "AAAARUZM" varchar(1) NOT NULL default ' ',
  "AAAARUYU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAABIOBO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUXX" (
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABIOBO" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  "AAABLPSU" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAABIOBO","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUYG" (
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABIOBO" varchar(30) NOT NULL default ' ',
  "AAABHKCO" varchar(30) NOT NULL default ' ',
  "AAAARUYP" varchar(30) NOT NULL default ' ',
  "AAAARUYL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAABIOBO","AAABHKCO","AAAARUYP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUYQ" (
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAAARUZF" varchar(10) NOT NULL default ' ',
  "AAABMLZS" varchar(3) NOT NULL default '000',
  "AAAARUYU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAAARUZF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUYX" (
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAAARUZF" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  "AAAARUZC" varchar(60) NOT NULL default ' ',
  "AAABLPSU" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAAARUZF","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUZH" (
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABIOBO" varchar(30) NOT NULL default ' ',
  "AAABHKCO" varchar(30) NOT NULL default ' ',
  "AAABMLZS" varchar(3) NOT NULL default '000',
  "AAAARUZM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAABIOBO","AAABHKCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARUZQ" (
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABIOBO" varchar(30) NOT NULL default ' ',
  "AAABHKCO" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  "AAABLPSU" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAABIOBO","AAABHKCO","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVAB" (
  "AAAARVFC" varchar(30) NOT NULL default ' ',
  "AAABMLZS" varchar(3) NOT NULL default '000',
  "AAABJLPR" varchar(1) NOT NULL default ' ',
  "AAAATIWM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARVFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVAH" (
  "AAAARVFC" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  "AAABLPSU" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAAARVFC","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVAO" (
  "AAAARVFC" varchar(30) NOT NULL default ' ',
  "AAABIMRZ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAAARVFC","AAABIMRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVAT" (
  "AAAARVFE" varchar(30) NOT NULL default ' ',
  "AAABMLZS" varchar(3) NOT NULL default '000',
  "AAABJLPR" varchar(1) NOT NULL default ' ',
  "AAAARVAX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARVFE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVAZ" (
  "AAAARVFE" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  "AAABLPSU" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAAARVFE","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVBG" (
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAAARVBR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAAARVBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVBL" (
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAAARVBR" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAAARVBR","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVBT" (
  "AAAARVFD" varchar(30) NOT NULL default ' ',
  "AAABMLZS" varchar(3) NOT NULL default '000',
  "AAABJLPR" varchar(1) NOT NULL default ' ',
  "AAAARVBX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARVFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVBZ" (
  "AAAARVFD" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  "AAABLPSU" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAAARVFD","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVCG" (
  "AAAARVFD" varchar(30) NOT NULL default ' ',
  "AAAATIWM" varchar(30) NOT NULL default ' ',
  "AAAARVCY" varchar(30) NOT NULL default ' ',
  "AAABDSUX" varchar(30) NOT NULL default ' ',
  "AAAARVDA" varchar(30) NOT NULL default ' ',
  "AAAARVDB" varchar(30) NOT NULL default ' ',
  "AAAARVDC" varchar(30) NOT NULL default ' ',
  "AAAARVDD" varchar(30) NOT NULL default ' ',
  "AAAARVDE" varchar(30) NOT NULL default ' ',
  "AAAARVDF" varchar(30) NOT NULL default ' ',
  "AAAARVDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARVFD","AAAATIWM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVCU" (
  "AAAARVFD" varchar(30) NOT NULL default ' ',
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAAATIWM" varchar(30) NOT NULL default ' ',
  "AAAARVCY" varchar(30) NOT NULL default ' ',
  "AAABDSUX" varchar(30) NOT NULL default ' ',
  "AAAARVDA" varchar(30) NOT NULL default ' ',
  "AAAARVDB" varchar(30) NOT NULL default ' ',
  "AAAARVDC" varchar(30) NOT NULL default ' ',
  "AAAARVDD" varchar(30) NOT NULL default ' ',
  "AAAARVDE" varchar(30) NOT NULL default ' ',
  "AAAARVDF" varchar(30) NOT NULL default ' ',
  "AAAARVDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARVFD","AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVDJ" (
  "AAAATQHL" varchar(40) NOT NULL default ' ',
  "AAABMRCK" longtext,
  PRIMARY KEY ("AAAATQHL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVDN" (
  "AAAARVGU" varchar(40) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABLNDO" varchar(2) NOT NULL default ' ',
  "AAAAVJHP" varchar(30) NOT NULL default ' ',
  "AAABHHSB" decimal(15,0) NOT NULL default '0',
  "AAABKCAK" decimal(15,0) NOT NULL default '0',
  "AAAARVEY" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAAARVGU","AAABIWFD","AAABLNDO","AAAAVJHP"),
  KEY "AAAARVDZ" ("AAABLNDO","AAAAVJHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVEC" (
  "AAAARVGU" varchar(40) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABLNDO" varchar(2) NOT NULL default ' ',
  "AAAAVJHP" varchar(30) NOT NULL default ' ',
  "AAAARVEO" varchar(12) NOT NULL default ' ',
  "AAABKCAK" decimal(15,0) NOT NULL default '0',
  "AAAARVEY" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAAARVGU","AAABIWFD","AAABLNDO","AAAAVJHP","AAAARVEO"),
  KEY "AAAARVEP" ("AAABLNDO","AAAAVJHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVES" (
  "AAAARVGU" varchar(40) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIMRZ" varchar(2) NOT NULL default ' ',
  "AAABMBXY" varchar(40) NOT NULL default ' ',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARVEY" varchar(12) NOT NULL default ' ',
  "AAABHHSB" decimal(15,0) NOT NULL default '0',
  "AAAARVFA" decimal(15,0) NOT NULL default '0',
  "AAABKCAK" decimal(15,0) NOT NULL default '0',
  "AAAARVFC" varchar(30) NOT NULL default ' ',
  "AAAARVFD" varchar(30) NOT NULL default ' ',
  "AAAARVFE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARVGU","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVFH" (
  "AAAARVGU" varchar(40) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAARVGU","AAABIWFD","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVFP" (
  "AAAARVGU" varchar(40) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABHMRI" varchar(4) NOT NULL default '0000',
  "AAABLTQF" varchar(4) NOT NULL default '0000',
  "AAABDELM" varchar(4) NOT NULL default ' ',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAAARVFW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARVGU","AAABIWFD","AAABHMRI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVGA" (
  "AAAARVGU" varchar(40) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABHMRI" varchar(4) NOT NULL default '0000',
  "AAABLUSV" varchar(30) NOT NULL default ' ',
  "AAAARVGZ" int(11) NOT NULL default '0',
  "AAABMMJT" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAARVGU","AAABIWFD","AAABHMRI","AAABLUSV","AAAARVGZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVGM" (
  "AAAARVGU" varchar(40) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHMRI" varchar(4) NOT NULL default '0000',
  "AAABLUSV" varchar(30) NOT NULL default ' ',
  "AAAARVGZ" int(11) NOT NULL default '0',
  "AAABMMJT" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAARVGU","AAABIWFD","AAABMLQA","AAABHMRI","AAABLUSV","AAAARVGZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVHA" (
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAAARVIV" varchar(30) NOT NULL default ' ',
  "AAAARYHM" varchar(30) NOT NULL default ' ',
  "AAAARXDL" varchar(30) NOT NULL default ' ',
  "AAAARYHN" varchar(30) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAAARVJQ" varchar(4) NOT NULL default ' ',
  "AAAARVHJ" varchar(8) NOT NULL default '00000000',
  "AAAARVHK" varchar(6) NOT NULL default '000000',
  "AAAARVHL" varchar(4) NOT NULL default ' ',
  "AAAARVHM" varchar(12) NOT NULL default ' ',
  "AAAARVHN" varchar(8) NOT NULL default '00000000',
  "AAAARVHO" varchar(6) NOT NULL default '000000',
  "AAAARVHP" varchar(120) NOT NULL default ' ',
  "AAAARYDV" varchar(30) NOT NULL default ' ',
  "AAAAVGBJ" varchar(1) NOT NULL default ' ',
  "AAABHIOL" varchar(1) NOT NULL default ' ',
  "AAAARVJP" varchar(1) NOT NULL default ' ',
  "AAAARVHU" decimal(15,0) NOT NULL default '0',
  "AAAARVHV" decimal(15,0) NOT NULL default '0',
  "AAABICGI" int(11) NOT NULL default '0',
  "AAAARVHX" decimal(15,0) NOT NULL default '0',
  "AAABLBET" varchar(1) NOT NULL default ' ',
  "AAABMINC" varchar(1) NOT NULL default ' ',
  "AAAARVIA" varchar(1) NOT NULL default ' ',
  "AAAAWTIL" varchar(1) NOT NULL default ' ',
  "AAABFUKI" varchar(1) NOT NULL default ' ',
  "AAAARVID" varchar(1) NOT NULL default ' ',
  "AAAARVIE" varchar(1) NOT NULL default ' ',
  "AAAARXBQ" varchar(1) NOT NULL default ' ',
  "AAABHBOX" varchar(3) NOT NULL default '000',
  "AAAARVIH" varchar(1) NOT NULL default ' ',
  "AAAARVII" varchar(1) NOT NULL default ' ',
  "AAABMPMP" varchar(1) NOT NULL default ' ',
  "AAAARVIK" varchar(1) NOT NULL default ' ',
  "AAAARVIL" int(11) NOT NULL default '0',
  "AAAARVIM" varchar(1) NOT NULL default ' ',
  "AAAARVIN" varchar(1) NOT NULL default ' ',
  "AAAARVIO" varchar(1) NOT NULL default ' ',
  "AAABLKIV" varchar(2) NOT NULL default '00',
  "AAAARVIQ" varchar(1) NOT NULL default ' ',
  "AAAARVIR" varchar(25) NOT NULL default ' ',
  "AAAARVIS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYNB"),
  KEY "AAAARVIU" ("AAAARVIV"),
  KEY "AAAARVIW" ("AAAARYHN"),
  KEY "AAAARVIY" ("AAABMRFQ","AAABHBOX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVJB" (
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAABLTQG" varchar(30) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAARYNB","AAABLTQG"),
  KEY "AAAARVJH" ("AAABLTQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVJJ" (
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMMVU" varchar(1) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAAARVJO" int(11) NOT NULL default '0',
  "AAAARVJP" varchar(1) NOT NULL default ' ',
  "AAAARVJQ" varchar(4) NOT NULL default ' ',
  "AAAARVJR" decimal(15,0) NOT NULL default '0',
  "AAAARVJS" varchar(4) NOT NULL default ' ',
  "AAAARVJT" decimal(15,0) NOT NULL default '0',
  "AAAARVKF" varchar(30) NOT NULL default ' ',
  "AAAARVJV" int(11) NOT NULL default '0',
  "AAAARVJW" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAARYNB","AAABMREL","AAABMMVU"),
  KEY "AAAARVKA" ("AAABMREL","AAABMKFU","AAABMMVU"),
  KEY "AAAARVKE" ("AAAARVKF","AAABMREL","AAABMMVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVKI" (
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAABLEJI" varchar(25) NOT NULL default ' ',
  "AAABBKHQ" varchar(20) NOT NULL default ' ',
  "AAABGLWX" varchar(40) NOT NULL default ' ',
  "AAABLSBE" varchar(40) NOT NULL default ' ',
  "AAAARVKO" decimal(15,0) NOT NULL default '0',
  "AAAARVKP" varchar(40) NOT NULL default ' ',
  "AAAARVKQ" varchar(25) NOT NULL default ' ',
  "AAABLJJO" varchar(1) NOT NULL default ' ',
  "AAAARVKS" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAARYNB","AAABLEJI","AAABBKHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVKW" (
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAAARVLG" varchar(30) NOT NULL default ' ',
  "AAAARVLI" varchar(30) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAABMRFO" varchar(10) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAARYNB","AAAARVLG"),
  KEY "AAAARVLH" ("AAAARVLI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVLJ" (
  "AAABIWQH" int(11) NOT NULL default '0',
  "AAABHEYG" varchar(30) NOT NULL default ' ',
  "AAAARVLM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVLO" (
  "AAABIWQH" varchar(30) NOT NULL default ' ',
  "AAABFOBJ" int(11) NOT NULL default '0',
  "AAAARVLR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWQH","AAABFOBJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVLU" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIVVH" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIVVF" varchar(8) NOT NULL default '00000000',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTO" varchar(40) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIVVH","AAABIWFD","AAABIVVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVMG" (
  "AAABIWES" int(11) NOT NULL default '0',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABIGOQ" varchar(8) NOT NULL default '00000000',
  "AAAARVNG" decimal(15,0) NOT NULL default '0',
  "AAABIWAW" varchar(1) NOT NULL default ' ',
  "AAABIVVH" varchar(25) NOT NULL default ' ',
  "AAABFMTA" varchar(30) NOT NULL default ' ',
  "AAAARVMO" int(11) NOT NULL default '0',
  "AAAARVMP" int(11) NOT NULL default '0',
  "AAAARVMQ" int(11) NOT NULL default '0',
  "AAAARVMR" int(11) NOT NULL default '0',
  "AAAARVMS" varchar(1) NOT NULL default ' ',
  "AAAARVMT" varchar(1) NOT NULL default ' ',
  "AAAARVMU" varchar(1) NOT NULL default '0',
  "AAABFNLO" varchar(1) NOT NULL default ' ',
  "AAAARVMW" varchar(1) NOT NULL default ' ',
  "AAAARVMX" varchar(1) NOT NULL default ' ',
  "AAAARVMY" int(11) NOT NULL default '0',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
  "AAABIWFT" varchar(1) NOT NULL default ' ',
  "AAABIWFK" varchar(1) NOT NULL default ' ',
  "AAAARVNC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWES","AAABIWFD","AAABIGOQ","AAAARVNG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVNH" (
  "AAABIWES" int(11) NOT NULL default '0',
  "AAABLRXN" varchar(8) NOT NULL default '00000000',
  "AAABLRXO" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABIWES","AAABLRXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVNN" (
  "AAABIWES" int(11) NOT NULL default '0',
  "AAABIWAW" varchar(1) NOT NULL default ' ',
  "AAAARVNX" varchar(32) NOT NULL default ' ',
  "AAAARVNY" varchar(32) NOT NULL default ' ',
  "AAABLKHX" int(11) NOT NULL default '0',
  "AAABIWAT" int(11) NOT NULL default '0',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABIWES","AAABIWAW","AAAARVNX","AAAARVNY","AAABLKHX","AAABIWAT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVOB" (
  "AAAARVOH" int(11) NOT NULL default '0',
  "AAABFMTA" varchar(30) NOT NULL default ' ',
  "AAABIKGM" varchar(60) NOT NULL default ' ',
  "AAABLETY" int(11) NOT NULL default '0',
  "AAABLNZC" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAARVOH","AAABFMTA","AAABIKGM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVOK" (
  "AAAARYNB" varchar(30) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAARYNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVOO" (
  "AAABLUSV" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABGFYL" varchar(30) NOT NULL default ' ',
  "AAAARZRD" varchar(3) NOT NULL default ' ',
  "AAAARZRE" varchar(6) NOT NULL default ' ',
  "AAAARZRF" decimal(15,0) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAARZRH" varchar(10) NOT NULL default ' ',
  "AAAARZRB" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(15,0) NOT NULL default '0',
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
  "AAABAENI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABIWFD"),
  KEY "AAAARVQA" ("AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVQC" (
  "AAAARVRD" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUDG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAARVRD","AAABIWFD","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVQK" (
  "AAAARVRD" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAAARVRD","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVQQ" (
  "AAABLUSV" varchar(25) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARVRD" varchar(25) NOT NULL default ' ',
  "AAAARVQV" varchar(3) NOT NULL default '000',
  "AAAARZQS" varchar(25) NOT NULL default ' ',
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  "AAAARVQZ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABIWFD"),
  KEY "AAAARVRC" ("AAAARVRD","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVRF" (
  "AAAARZEC" varchar(30) NOT NULL default ' ',
  "AAAARVRH" varchar(6) NOT NULL default ' ',
  "AAAARVRI" varchar(2) NOT NULL default '00',
  "AAAARZFP" varchar(4) NOT NULL default ' ',
  "AAAARZFQ" varchar(1) NOT NULL default ' ',
  "AAAARZCE" varchar(6) NOT NULL default '000000',
  "AAAARVRM" varchar(60) NOT NULL default ' ',
  "AAAARVRN" varchar(60) NOT NULL default ' ',
  "AAABLTXI" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAARZEC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVRQ" (
  "AAAARZEC" varchar(30) NOT NULL default ' ',
  "AAAARVSZ" varchar(30) NOT NULL default ' ',
  "AAAARVRT" varchar(10) NOT NULL default ' ',
  "AAAARZJI" varchar(3) NOT NULL default '000',
  "AAABLBEK" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(250) NOT NULL default ' ',
  "AAAARVRX" varchar(1) NOT NULL default ' ',
  "AAAARZJJ" varchar(2) NOT NULL default ' ',
  "AAAARZJK" varchar(30) NOT NULL default ' ',
  "AAAARVSA" varchar(1) NOT NULL default ' ',
  "AAAARVSB" varchar(1) NOT NULL default ' ',
  "AAAARVSC" varchar(30) NOT NULL default ' ',
  "AAAARZJM" varchar(2) NOT NULL default ' ',
  "AAAARZCE" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAARZEC","AAAARVSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAARVSH" (
  "AAAARZEC" varchar(30) NOT NULL default ' ',
  "AAAARVSZ" varchar(30) NOT NULL default ' ',
  "AAAARVSQ" varchar(132) NOT NULL default ' ',
  "AAAARVSR" varchar(30) NOT NULL default ' ',
  "AAAARVSM" varchar(1) NOT NULL default ' ',
  "AAAARZCE" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAARZEC","AAAARVSZ","AAAARVSQ","AAAARVSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--enable_query_log
