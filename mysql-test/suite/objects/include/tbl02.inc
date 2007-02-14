# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+
# |                             tbl02.inc                              |
# |                        Copyright 2006 by MySQL AB                  |
# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+

--disable_query_log
CREATE TABLE "AAAAAYRQ" (
  "AAAAAZCE" VARBINARY(255) NOT NULL,
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIINC" varchar(40) NOT NULL default ' ',
  "AAAAAYRY" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAAAAZCE"(255),"AAABMLQA"),
  KEY "AAAAAYRX" ("AAAAAYRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAYRZ" (
  "AAABIINC" varchar(40) NOT NULL default ' ',
  "AAAAXEDQ" varchar(4) NOT NULL default '0000',
  "AAAATQGQ" varchar(30) NOT NULL default ' ',
  "AAABERKJ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABIINC","AAAAXEDQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAYSG" (
  "AAABIINC" varchar(40) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAAYSJ" varchar(128) NOT NULL default ' ',
  "AAAAAYSK" varchar(60) NOT NULL default ' ',
  "AAAAAYSL" varchar(60) NOT NULL default ' ',
  "AAAAAYSM" varchar(60) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABGJZC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABIINC","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAYSS" (
  "AAABIINC" varchar(40) NOT NULL default ' ',
  "AAAAAYSW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABIINC","AAAAAYSW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAYSX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(22) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABLRSS" varchar(12) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAYTK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAMDOV" varchar(18) NOT NULL default ' ',
  "AAAAAYUS" varchar(5) NOT NULL default '00000',
  "AAABLUFQ" varchar(18) NOT NULL default ' ',
  "AAABKRYY" varchar(10) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLRYP" varchar(8) NOT NULL default '00000000',
  "AAABMAWF" varchar(8) NOT NULL default '00000000',
  "AAABKRRX" varchar(2) NOT NULL default ' ',
  "AAABCDOD" varchar(4) NOT NULL default ' ',
  "AAAAAYUM" varchar(1) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAMDOV","AAAAAYUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAYUB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAMDOV" varchar(18) NOT NULL default ' ',
  "AAAAAYUS" varchar(5) NOT NULL default '00000',
  "AAABLUFQ" varchar(18) NOT NULL default ' ',
  "AAABKRYY" varchar(10) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLRYP" varchar(8) NOT NULL default '00000000',
  "AAABMAWF" varchar(8) NOT NULL default '00000000',
  "AAABKRRX" varchar(2) NOT NULL default ' ',
  "AAABCDOD" varchar(4) NOT NULL default ' ',
  "AAAAAYUM" varchar(1) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAAAAYUP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAMDOV","AAAAAYUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAYUT" (
  "AAAAAZCE" VARBINARY(255) NOT NULL,
  "AAAAXEDQ" varchar(8) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAAAAYUX" varchar(128) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  "AAABERKJ" varchar(32) NOT NULL default ' ',
  "AAAATQGQ" varchar(30) NOT NULL default ' ',
  "AAAAAYVG" varchar(4) NOT NULL default '0000',
  "AAAAAYVH" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAAAAZCE"(255),"AAAAXEDQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAYVK" (
  "AAAAAZCE" VARBINARY(255) NOT NULL,
  "AAABERKJ" varchar(32) NOT NULL default ' ',
  "AAAAXEDQ" varchar(8) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABHCRQ" varchar(8) NOT NULL default ' ',
  "AAAAAYVQ" varchar(3) NOT NULL default ' ',
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLTIZ" varchar(12) NOT NULL default ' ',
  "AAABLEQR" varchar(9) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABMKFH" varchar(30) NOT NULL default ' ',
  "AAABLJJO" varchar(7) NOT NULL default ' ',
  "AAABHEYE" int(11) NOT NULL default '0',
  "AAABLEKV" varchar(5) NOT NULL default ' ',
  "AAAAAYWA" varchar(210) NOT NULL default ' ',
  PRIMARY KEY ("AAAAAZCE"(255),"AAABERKJ","AAAAXEDQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAYWE" (
  "AAABHDEU" varchar(40) NOT NULL default ' ',
  "AAABLWIT" varchar(20) NOT NULL default ' ',
  "AAABLWIV" varchar(20) NOT NULL default ' ',
  "AAABLWIX" varchar(20) NOT NULL default ' ',
  "AAABLWIZ" varchar(20) NOT NULL default ' ',
  "AAABLWJB" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABHDEU"),
  KEY "AAAAAYWM" ("AAABLWIT","AAABLWIV","AAABLWIX","AAABLWIZ","AAABLWJB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAYWS" (
  "AAABMDEP" varchar(40) NOT NULL default ' ',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  "AAAAAYYA" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLVJK" varchar(8) NOT NULL default '00000000',
  "AAAAAYYE" varchar(4) NOT NULL default ' ',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAAYXB" varchar(40) NOT NULL default ' ',
  "AAAAUKIV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP","AAABJSAN","AAAAAYYA","AAABMDEN","AAABMRIL","AAABLVJK","AAAAAYYE","AAABLSCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAYXL" (
  "AAABMDEP" varchar(40) NOT NULL default ' ',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  "AAAAAYYA" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLVJK" varchar(8) NOT NULL default '00000000',
  "AAAAAYYE" varchar(4) NOT NULL default ' ',
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAAAAYYG" varchar(2) NOT NULL default ' ',
  "AAAAAYXV" int(11) NOT NULL default '0',
  "AAABEDVG" int(11) NOT NULL default '0',
  "AAAAAYXX" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMDEP","AAABJSAN","AAAAAYYA","AAABMDEN","AAABMRIL","AAABLVJK","AAAAAYYE","AAABLUAV","AAAAAYYG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAYYH" (
  "AAAAAZCE" VARBINARY(255) NOT NULL,
  "AAABERKJ" varchar(32) NOT NULL default ' ',
  "AAAAXEDQ" varchar(8) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAAZCA" varchar(110) NOT NULL default ' ',
  "AAAAAZCB" decimal(10,0) NOT NULL default '0',
  "AAAAAYYO" decimal(9,0) NOT NULL default '0',
  "AAAAVPZF" decimal(12,0) NOT NULL default '0',
  "AAAAAZCC" decimal(10,0) NOT NULL default '0',
  "AAAAAYYR" varchar(10) NOT NULL default ' ',
  "AAAAAYYS" varchar(10) NOT NULL default ' ',
  "AAAAAYYT" varchar(10) NOT NULL default ' ',
  "AAAAAYYU" varchar(10) NOT NULL default ' ',
  "AAAAAYYV" varchar(6) NOT NULL default ' ',
  "AAAAAYYW" varchar(5) NOT NULL default ' ',
  "AAAAAYYX" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAAAAZCE"(255),"AAABERKJ","AAAAXEDQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAYZB" (
  "AAAAAZCE" VARBINARY(255) NOT NULL,
  "AAABERKJ" varchar(32) NOT NULL default ' ',
  "AAAAXEDQ" varchar(8) NOT NULL default ' ',
  "AAABLRTV" varchar(8) NOT NULL default '00000000',
  "AAABLRTW" varchar(6) NOT NULL default '000000',
  "AAABLEQN" varchar(8) NOT NULL default '00000000',
  "AAABLEQO" varchar(6) NOT NULL default '000000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABEDDS" varchar(1) NOT NULL default ' ',
  "AAABECXR" varchar(4) NOT NULL default ' ',
  "AAAAAZAN" decimal(18,2) NOT NULL default '0.00',
  "AAAAAZAO" decimal(13,2) NOT NULL default '0.00',
  "AAAAAZAP" decimal(18,2) NOT NULL default '0.00',
  "AAABHDAD" decimal(13,2) NOT NULL default '0.00',
  "AAABBTMB" decimal(18,2) NOT NULL default '0.00',
  "AAAAAZAS" decimal(13,2) NOT NULL default '0.00',
  "AAAAAZAT" decimal(13,2) NOT NULL default '0.00',
  "AAAAGUHS" decimal(13,2) NOT NULL default '0.00',
  "AAAAAZAV" decimal(13,2) NOT NULL default '0.00',
  "AAAAAZAW" decimal(18,2) NOT NULL default '0.00',
  "AAAAAZAX" decimal(16,2) NOT NULL default '0.00',
  "AAAAAZAY" decimal(16,2) NOT NULL default '0.00',
  "AAAAAZBA" decimal(13,2) NOT NULL default '0.00',
  "AAAAAZAZ" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAAAAZCE"(255),"AAABERKJ","AAAAXEDQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAZAE" (
  "AAAAAZCE" VARBINARY(255) NOT NULL,
  "AAABERKJ" varchar(32) NOT NULL default ' ',
  "AAAAXEDQ" varchar(8) NOT NULL default ' ',
  "AAAAMQCY" varchar(56) NOT NULL default ' ',
  "AAAAAZAJ" varchar(56) NOT NULL default ' ',
  "AAAAAZCB" decimal(10,0) NOT NULL default '0',
  "AAAAVPZF" decimal(12,0) NOT NULL default '0',
  "AAAAAZCC" decimal(10,0) NOT NULL default '0',
  "AAAAAZAN" decimal(18,2) NOT NULL default '0.00',
  "AAAAAZAO" decimal(13,2) NOT NULL default '0.00',
  "AAAAAZAP" decimal(18,2) NOT NULL default '0.00',
  "AAABHDAD" decimal(13,2) NOT NULL default '0.00',
  "AAABBTMB" decimal(18,2) NOT NULL default '0.00',
  "AAAAAZAS" decimal(13,2) NOT NULL default '0.00',
  "AAAAAZAT" decimal(13,2) NOT NULL default '0.00',
  "AAAAGUHS" decimal(13,2) NOT NULL default '0.00',
  "AAAAAZAV" decimal(13,2) NOT NULL default '0.00',
  "AAAAAZAW" decimal(18,2) NOT NULL default '0.00',
  "AAAAAZAX" decimal(16,2) NOT NULL default '0.00',
  "AAAAAZAY" decimal(16,2) NOT NULL default '0.00',
  "AAAAAZAZ" decimal(13,2) NOT NULL default '0.00',
  "AAAAAZBA" decimal(13,2) NOT NULL default '0.00',
  "AAAAAZBB" decimal(13,2) NOT NULL default '0.00',
  "AAAAAZBC" decimal(15,2) NOT NULL default '0.00',
  "AAAAAZBD" decimal(13,2) NOT NULL default '0.00',
  "AAAAAZBE" decimal(18,2) NOT NULL default '0.00',
  "AAAAAZBF" decimal(13,2) NOT NULL default '0.00',
  "AAAAAZBG" decimal(18,2) NOT NULL default '0.00',
  "AAAAAZBH" decimal(13,2) NOT NULL default '0.00',
  "AAAAAZBI" decimal(18,2) NOT NULL default '0.00',
  "AAAAAZBJ" decimal(13,2) NOT NULL default '0.00',
  "AAAAAZBK" decimal(13,2) NOT NULL default '0.00',
  "AAAAAZBL" decimal(13,2) NOT NULL default '0.00',
  "AAAAAZBM" decimal(13,2) NOT NULL default '0.00',
  "AAAAAZBN" decimal(18,2) NOT NULL default '0.00',
  "AAAAAZBO" decimal(16,2) NOT NULL default '0.00',
  "AAAAAZBP" decimal(16,2) NOT NULL default '0.00',
  "AAAAAZBQ" decimal(13,2) NOT NULL default '0.00',
  "AAAAAZBR" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAAAAZCE"(255),"AAABERKJ","AAAAXEDQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAZBV" (
  "AAAAAZCE" VARBINARY(255) NOT NULL,
  "AAABERKJ" varchar(32) NOT NULL default ' ',
  "AAAAXEDQ" varchar(8) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAAZCA" varchar(6) NOT NULL default ' ',
  "AAAAAZCB" decimal(10,0) NOT NULL default '0',
  "AAAAAZCC" decimal(10,0) NOT NULL default '0',
  "AAAAVPZF" decimal(12,0) NOT NULL default '0',
  PRIMARY KEY ("AAAAAZCE"(255),"AAABERKJ","AAAAXEDQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAZCH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEKS" varchar(12) NOT NULL default ' ',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABLTQF" varchar(10) NOT NULL default ' ',
  "AAABHHCG" varchar(10) NOT NULL default ' ',
  "AAAAUKGK" varchar(1) NOT NULL default ' ',
  "AAABJTNV" varchar(70) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAUKGO" int(11) NOT NULL default '0',
  "AAABMRCK" varchar(132) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLEKS","AAABLTVR"),
  KEY "AAAAAZCX" ("AAABLEKS","AAABLTQF","AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAZDB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCHZ" varchar(8) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAAAVZCP" varchar(1) NOT NULL default ' ',
  "AAAAVZCQ" varchar(1) NOT NULL default ' ',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYCHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAZDO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAYCHZ" varchar(5) NOT NULL default ' ',
  "AAAAAZWI" varchar(50) NOT NULL default ' ',
  "AAAAVSAU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAYCHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAZDX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCXU" varchar(5) NOT NULL default '00000',
  "AAABIAWX" varchar(2) NOT NULL default '00',
  "AAABIAWY" varchar(4) NOT NULL default '0000',
  "AAABIAXI" varchar(4) NOT NULL default '0000',
  "AAAAVSIT" decimal(13,2) NOT NULL default '0.00',
  "AAABIATM" decimal(13,2) NOT NULL default '0.00',
  "AAABIATQ" decimal(13,2) NOT NULL default '0.00',
  "AAABIATW" decimal(13,2) NOT NULL default '0.00',
  "AAABIATX" decimal(13,2) NOT NULL default '0.00',
  "AAABIATO" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSIZ" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSJA" decimal(13,2) NOT NULL default '0.00',
  "AAABIATL" decimal(13,2) NOT NULL default '0.00',
  "AAABIATP" decimal(13,2) NOT NULL default '0.00',
  "AAABIATY" decimal(13,2) NOT NULL default '0.00',
  "AAABIATZ" decimal(13,2) NOT NULL default '0.00',
  "AAABIATN" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSJG" decimal(13,2) NOT NULL default '0.00',
  "AAABIATK" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSLE" varchar(1) NOT NULL default ' ',
  "AAABIATS" decimal(13,2) NOT NULL default '0.00',
  "AAABIATR" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMQVT","AAABMQVU","AAABMDBB","AAABMCXU","AAABIAWX","AAABIAWY","AAABIAXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAZFI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCXU" varchar(5) NOT NULL default '00000',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMCMR" varchar(2) NOT NULL default '00',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMCMV" varchar(8) NOT NULL default '00000000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMCMZ" varchar(16) NOT NULL default ' ',
  "AAABMDGB" varchar(16) NOT NULL default ' ',
  "AAABMCNC" varchar(10) NOT NULL default ' ',
  "AAABMCND" varchar(4) NOT NULL default '0000',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABIAUX" varchar(12) NOT NULL default ' ',
  "AAABIAUY" varchar(4) NOT NULL default ' ',
  "AAAAVSLE" varchar(1) NOT NULL default ' ',
  "AAABMCNN" varchar(4) NOT NULL default ' ',
  "AAABMCRA" varchar(6) NOT NULL default ' ',
  "AAABMCPW" varchar(4) NOT NULL default ' ',
  "AAABIAVC" varchar(3) NOT NULL default '000',
  "AAAAAZGM" varchar(3) NOT NULL default ' ',
  "AAAAAZGN" varchar(2) NOT NULL default '00',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(20) NOT NULL default ' ',
  "AAABMDGC" varchar(18) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABIAUZ" varchar(5) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMCRU" varchar(8) NOT NULL default '00000000',
  "AAABIAVP" varchar(1) NOT NULL default '0',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABMQCE" varchar(10) NOT NULL default ' ',
  "AAABMCOP" varchar(10) NOT NULL default ' ',
  "AAABKRFR" varchar(4) NOT NULL default ' ',
  "AAABMCRD" varchar(4) NOT NULL default ' ',
  "AAABFVNI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMQVT","AAABMQVU","AAABMDBB","AAABMCXU"),
  KEY "AAAAAZHM" ("AAABMDBA","AAABMQCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAZHP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCXU" varchar(5) NOT NULL default '00000',
  "AAABIAWX" varchar(2) NOT NULL default '00',
  "AAABIAWY" varchar(4) NOT NULL default '0000',
  "AAABIAXI" varchar(4) NOT NULL default '0000',
  "AAABKPWJ" varchar(3) NOT NULL default '000',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMCRU" varchar(8) NOT NULL default '00000000',
  "AAABIAXA" varchar(3) NOT NULL default ' ',
  "AAAAVSGZ" varchar(1) NOT NULL default '0',
  "AAABIAWZ" decimal(13,2) NOT NULL default '0.00',
  "AAABIAXC" decimal(13,2) NOT NULL default '0.00',
  "AAABIAXD" decimal(13,2) NOT NULL default '0.00',
  "AAABIAXE" decimal(13,2) NOT NULL default '0.00',
  "AAAAAZIJ" varchar(1) NOT NULL default ' ',
  "AAAAAZIK" varchar(1) NOT NULL default ' ',
  "AAAAAZIL" varchar(5) NOT NULL default '00000',
  "AAAAVSLE" varchar(1) NOT NULL default ' ',
  "AAABIAUZ" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMQVT","AAABMQVU","AAABMDBB","AAABMCXU","AAABIAWX","AAABIAWY","AAABIAXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAZIX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAAZJL" varchar(5) NOT NULL default '00000',
  "AAAAAZJR" varchar(5) NOT NULL default '00000',
  "AAAAAZJX" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMQVT","AAABMQVU","AAABMDBB","AAAAAZJL"),
  KEY "AAAAAZJM" ("AAABMRIL","AAABMREX","AAABMQVT","AAABMQVU","AAAAAZJR"),
  KEY "AAAAAZJS" ("AAABMRIL","AAABMREX","AAABMQVT","AAABMQVU","AAAAAZJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAZJY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAVRYA" varchar(8) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAAAYCHZ" varchar(5) NOT NULL default ' ',
  "AAAAVZCP" varchar(1) NOT NULL default ' ',
  "AAAAVZCQ" varchar(1) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAAAVRZE" varchar(30) NOT NULL default ' ',
  "AAAAVRYU" varchar(4) NOT NULL default ' ',
  "AAAAVRYV" varchar(4) NOT NULL default ' ',
  "AAAAVRYW" varchar(4) NOT NULL default ' ',
  "AAAAVRYX" varchar(4) NOT NULL default ' ',
  "AAAAVRZF" varchar(30) NOT NULL default ' ',
  "AAAAVRZT" varchar(1) NOT NULL default ' ',
  "AAABKPUA" varchar(6) NOT NULL default ' ',
  "AAABFLEJ" varchar(12) NOT NULL default ' ',
  "AAAAAZKS" varchar(8) NOT NULL default ' ',
  "AAAAAZKT" varchar(8) NOT NULL default ' ',
  "AAAAAZKU" varchar(9) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAVRYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAZKX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAAAVRYA" varchar(8) NOT NULL default ' ',
  "AAABIAWX" varchar(2) NOT NULL default '00',
  "AAABDGZI" varchar(8) NOT NULL default '00000000',
  "AAAAWIPQ" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAAAVZCP" varchar(1) NOT NULL default ' ',
  "AAAAWJLN" varchar(3) NOT NULL default '000',
  "AAAAVSFD" varchar(3) NOT NULL default '000',
  "AAABBVGN" varchar(5) NOT NULL default ' ',
  "AAAAVSFH" varchar(5) NOT NULL default ' ',
  "AAAAVSFI" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABICIU","AAAAVRYA","AAABIAWX","AAABDGZI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAZLT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAVRYA" varchar(8) NOT NULL default ' ',
  "AAAAAZWH" varchar(20) NOT NULL default ' ',
  "AAAAAZWI" varchar(50) NOT NULL default ' ',
  "AAAAVSAU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAVRYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAZMD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKGBX" varchar(8) NOT NULL default ' ',
  "AAAAVRYA" varchar(8) NOT NULL default ' ',
  "AAAAYCHZ" varchar(5) NOT NULL default ' ',
  "AAAAWJKS" varchar(8) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABLQTM" varchar(8) NOT NULL default '00000000',
  "AAABKPUA" varchar(6) NOT NULL default ' ',
  "AAAAVSKT" decimal(3,2) NOT NULL default '0.00',
  "AAAAVRYS" varchar(8) NOT NULL default '00000000',
  "AAAAVSKW" varchar(12) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAAAAZMW" decimal(5,2) NOT NULL default '0.00',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABFKVI" decimal(10,0) NOT NULL default '0',
  "AAABBHDK" decimal(15,2) NOT NULL default '0.00',
  "AAABJLKE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAZNE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABIAWX" varchar(2) NOT NULL default '00',
  "AAAAVSEX" varchar(8) NOT NULL default '00000000',
  "AAAAVSEY" varchar(8) NOT NULL default '00000000',
  "AAAAVSEZ" varchar(8) NOT NULL default '00000000',
  "AAAAWIBR" varchar(2) NOT NULL default ' ',
  "AAAAWJKJ" varchar(4) NOT NULL default ' ',
  "AAAAWJLN" varchar(3) NOT NULL default '000',
  "AAAAVSFD" varchar(3) NOT NULL default '000',
  "AAAAVSFE" decimal(7,4) NOT NULL default '0.0000',
  "AAAAVSFF" decimal(7,4) NOT NULL default '0.0000',
  "AAABBVGN" varchar(5) NOT NULL default ' ',
  "AAAAVSFH" varchar(5) NOT NULL default ' ',
  "AAAAVSFI" decimal(7,4) NOT NULL default '0.0000',
  "AAAAVSFJ" varchar(4) NOT NULL default '0000',
  "AAAAVSFM" decimal(13,2) NOT NULL default '0.00',
  "AAAAAZNX" decimal(3,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAABMRCU","AAABIAWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAZOC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGBX" varchar(8) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAAAVZCP" varchar(1) NOT NULL default ' ',
  "AAAAVZCQ" varchar(1) NOT NULL default ' ',
  "AAABMCRZ" varchar(1) NOT NULL default ' ',
  "AAABIEWH" varchar(2) NOT NULL default ' ',
  "AAAAWJKS" varchar(8) NOT NULL default ' ',
  "AAAAAZOO" varchar(16) NOT NULL default ' ',
  "AAAAWHWZ" varchar(4) NOT NULL default ' ',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAABLCEX" varchar(3) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABKPUA" varchar(6) NOT NULL default ' ',
  "AAAAVRZA" varchar(1) NOT NULL default '0',
  "AAAAVRZB" varchar(1) NOT NULL default '0',
  "AAAAAZOY" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAAAVRZE" varchar(30) NOT NULL default ' ',
  "AAAAWIFO" varchar(12) NOT NULL default ' ',
  "AAAAVRZG" varchar(1) NOT NULL default ' ',
  "AAAAVRZW" varchar(4) NOT NULL default ' ',
  "AAAAVRZX" varchar(1) NOT NULL default ' ',
  "AAAAWIBR" varchar(2) NOT NULL default ' ',
  "AAAAVRYU" varchar(4) NOT NULL default ' ',
  "AAAAVRYV" varchar(4) NOT NULL default ' ',
  "AAAAVRYW" varchar(4) NOT NULL default ' ',
  "AAAAVRYX" varchar(4) NOT NULL default ' ',
  "AAABKRIG" varchar(2) NOT NULL default ' ',
  "AAAAVRZF" varchar(30) NOT NULL default ' ',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABKKRR" varchar(3) NOT NULL default ' ',
  "AAAAVRZT" varchar(1) NOT NULL default ' ',
  "AAAAVSAK" varchar(25) NOT NULL default ' ',
  "AAAAWWQT" varchar(25) NOT NULL default ' ',
  "AAAAVRYL" varchar(1) NOT NULL default '0',
  "AAAAAZPT" varchar(1) NOT NULL default ' ',
  "AAABCDTJ" varchar(1) NOT NULL default ' ',
  "AAAAVSBA" varchar(10) NOT NULL default ' ',
  "AAAAVSBE" varchar(3) NOT NULL default '000',
  "AAAAVSBF" varchar(3) NOT NULL default '000',
  "AAAAVSBG" varchar(3) NOT NULL default '000',
  "AAAAVSBK" decimal(7,4) NOT NULL default '0.0000',
  "AAAAVSBQ" decimal(5,0) NOT NULL default '0',
  "AAABBAAI" varchar(50) NOT NULL default ' ',
  "AAAAVSCK" varchar(1) NOT NULL default '0',
  "AAAAWIER" varchar(8) NOT NULL default ' ',
  "AAAAVSCQ" varchar(1) NOT NULL default ' ',
  "AAAAVSCS" varchar(1) NOT NULL default ' ',
  "AAAAVSCU" varchar(1) NOT NULL default ' ',
  "AAAAZVRG" varchar(2) NOT NULL default ' ',
  "AAABKRGH" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKGBX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAZQL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGBX" varchar(8) NOT NULL default ' ',
  "AAAAAZQO" varchar(32) NOT NULL default ' ',
  "AAAAAZQP" varchar(32) NOT NULL default ' ',
  "AAAAAZQQ" varchar(32) NOT NULL default ' ',
  "AAAAAZQR" varchar(32) NOT NULL default ' ',
  "AAAAAZQS" varchar(32) NOT NULL default ' ',
  "AAAAAZQT" varchar(32) NOT NULL default ' ',
  "AAAAAZQU" varchar(32) NOT NULL default ' ',
  "AAAAAZQV" varchar(12) NOT NULL default ' ',
  "AAAAAZQW" varchar(4) NOT NULL default ' ',
  "AAABIZWL" varchar(1) NOT NULL default ' ',
  "AAAAWSNV" varchar(10) NOT NULL default ' ',
  "AAAAAZQZ" varchar(1) NOT NULL default ' ',
  "AAAAGVBW" varchar(1) NOT NULL default ' ',
  "AAAAAZRB" varchar(1) NOT NULL default ' ',
  "AAAABACH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKGBX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAZRF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGBX" varchar(8) NOT NULL default ' ',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAABIAWX" varchar(2) NOT NULL default '00',
  "AAAAWIPQ" varchar(8) NOT NULL default '00000000',
  "AAABDGZI" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAAAVZET" varchar(1) NOT NULL default ' ',
  "AAAAVZCP" varchar(1) NOT NULL default ' ',
  "AAAAWHWZ" varchar(4) NOT NULL default ' ',
  "AAAAWJKS" varchar(8) NOT NULL default ' ',
  "AAAAVSEU" varchar(1) NOT NULL default ' ',
  "AAAAVSEV" varchar(1) NOT NULL default '0',
  "AAAAAZRW" varchar(3) NOT NULL default '000',
  "AAAAAZRX" varchar(3) NOT NULL default '000',
  "AAAAAZRY" varchar(3) NOT NULL default '000',
  "AAAAAZRZ" varchar(3) NOT NULL default '000',
  "AAAAWIBR" varchar(2) NOT NULL default ' ',
  "AAAAWJKJ" varchar(4) NOT NULL default ' ',
  "AAAAWJLN" varchar(3) NOT NULL default '000',
  "AAAAVSFD" varchar(3) NOT NULL default '000',
  "AAAAVSFE" decimal(7,4) NOT NULL default '0.0000',
  "AAAAVSFF" decimal(7,4) NOT NULL default '0.0000',
  "AAABBVGN" varchar(5) NOT NULL default ' ',
  "AAAAVSFH" varchar(5) NOT NULL default ' ',
  "AAAAVSFI" decimal(7,4) NOT NULL default '0.0000',
  "AAAAVSEW" varchar(1) NOT NULL default ' ',
  "AAAAVSFR" varchar(1) NOT NULL default ' ',
  "AAAAVSGF" varchar(12) NOT NULL default ' ',
  "AAAAKQHZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKGBX","AAABICIU","AAABIAWX","AAAAWIPQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAZSS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAAAAZUN" varchar(10) NOT NULL default ' ',
  "AAAAAZSX" varchar(1) NOT NULL default '0',
  "AAAAWJKS" varchar(8) NOT NULL default ' ',
  "AAAAWJKP" varchar(50) NOT NULL default ' ',
  "AAAAWJGV" varchar(10) NOT NULL default ' ',
  "AAAAWJIL" varchar(10) NOT NULL default ' ',
  "AAAAWJIM" varchar(10) NOT NULL default ' ',
  "AAAAWJGX" varchar(10) NOT NULL default ' ',
  "AAAAWJKE" varchar(10) NOT NULL default ' ',
  "AAAAWJKD" varchar(10) NOT NULL default ' ',
  "AAAAAZTG" varchar(1) NOT NULL default '0',
  "AAABLYCI" varchar(2) NOT NULL default ' ',
  "AAABLBGA" varchar(20) NOT NULL default ' ',
  "AAABLBGB" varchar(20) NOT NULL default ' ',
  "AAABKBXN" varchar(1) NOT NULL default ' ',
  "AAAAAZTL" varchar(1) NOT NULL default '0',
  "AAAAWHWZ" varchar(4) NOT NULL default ' ',
  "AAAAAZTN" varchar(30) NOT NULL default ' ',
  "AAAAAZTO" varchar(1) NOT NULL default ' ',
  "AAAAAZTP" varchar(1) NOT NULL default ' ',
  "AAAAAZTQ" varchar(1) NOT NULL default ' ',
  "AAAAAZTR" varchar(1) NOT NULL default ' ',
  "AAAAAZTS" varchar(1) NOT NULL default ' ',
  "AAAAAZTT" varchar(1) NOT NULL default ' ',
  "AAAAAZTU" varchar(1) NOT NULL default ' ',
  "AAAAAZTV" varchar(1) NOT NULL default ' ',
  "AAAAAZTW" varchar(1) NOT NULL default ' ',
  "AAAAAZTX" varchar(1) NOT NULL default ' ',
  "AAAAAZTY" varchar(1) NOT NULL default ' ',
  "AAAAAZTZ" varchar(1) NOT NULL default ' ',
  "AAAAAZUA" varchar(1) NOT NULL default ' ',
  "AAAAAZUB" varchar(1) NOT NULL default '0',
  "AAABKGBX" varchar(8) NOT NULL default ' ',
  "AAAAAZUD" varchar(50) NOT NULL default ' ',
  "AAAAAZUE" varchar(4) NOT NULL default ' ',
  "AAAAAZUF" varchar(3) NOT NULL default '000',
  "AAAAAZUG" varchar(4) NOT NULL default ' ',
  "AAAAAZUH" varchar(3) NOT NULL default '000',
  "AAAAAZUI" varchar(5) NOT NULL default ' ',
  "AAAAAZUJ" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLZBM","AAABICIU","AAAAAZUN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAZUO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGBX" varchar(8) NOT NULL default ' ',
  "AAABGQUK" varchar(20) NOT NULL default ' ',
  "AAABLROG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKGBX","AAABGQUK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAZUW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGBX" varchar(8) NOT NULL default ' ',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAABGQUK" varchar(20) NOT NULL default ' ',
  "AAABLROG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKGBX","AAABICIU","AAABGQUK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAZVG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGBX" varchar(8) NOT NULL default ' ',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAAAWHWZ" varchar(4) NOT NULL default ' ',
  "AAAAWJKS" varchar(8) NOT NULL default ' ',
  "AAAAVRYU" varchar(4) NOT NULL default ' ',
  "AAAAVRYV" varchar(4) NOT NULL default ' ',
  "AAAAVRYW" varchar(4) NOT NULL default ' ',
  "AAAAVRYX" varchar(4) NOT NULL default ' ',
  "AAAAWIER" varchar(8) NOT NULL default ' ',
  "AAAAVRZW" varchar(4) NOT NULL default ' ',
  "AAABLHXQ" varchar(2) NOT NULL default ' ',
  "AAAABATS" varchar(5) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAAAVZCP" varchar(1) NOT NULL default ' ',
  "AAAAWCQZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKGBX","AAABICIU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAZWD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKGBX" varchar(8) NOT NULL default ' ',
  "AAAAAZWH" varchar(20) NOT NULL default ' ',
  "AAAAAZWI" varchar(50) NOT NULL default ' ',
  "AAABEPBY" varchar(50) NOT NULL default ' ',
  "AAAAVSAT" varchar(50) NOT NULL default ' ',
  "AAAAVSAU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKGBX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAZWP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGBX" varchar(8) NOT NULL default ' ',
  "AAAABAVC" varchar(2) NOT NULL default '00',
  "AAABLHXQ" varchar(2) NOT NULL default ' ',
  "AAAABATS" varchar(5) NOT NULL default ' ',
  "AAAAYBRU" varchar(2) NOT NULL default ' ',
  "AAAABAUW" varchar(1) NOT NULL default ' ',
  "AAAANJFZ" varchar(5) NOT NULL default ' ',
  "AAAABAUC" varchar(15) NOT NULL default ' ',
  "AAAABAUF" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKGBX","AAAABAVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAAZXD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABKGBX" varchar(8) NOT NULL default ' ',
  "AAAAVRYA" varchar(8) NOT NULL default ' ',
  "AAAAYCHZ" varchar(5) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAAAVZCP" varchar(1) NOT NULL default ' ',
  "AAAAVZET" varchar(1) NOT NULL default ' ',
  "AAAAWHWZ" varchar(4) NOT NULL default ' ',
  "AAAAWJKS" varchar(8) NOT NULL default ' ',
  "AAABMCRZ" varchar(1) NOT NULL default ' ',
  "AAAAVRYL" varchar(1) NOT NULL default '0',
  "AAABIAWY" varchar(4) NOT NULL default '0000',
  "AAAAVRYN" varchar(3) NOT NULL default '000',
  "AAAAVRYO" varchar(8) NOT NULL default '00000000',
  "AAABLQTM" varchar(8) NOT NULL default '00000000',
  "AAAAVRYQ" varchar(8) NOT NULL default '00000000',
  "AAAAVRYR" varchar(8) NOT NULL default '00000000',
  "AAAAVRYS" varchar(8) NOT NULL default '00000000',
  "AAAAVRYT" varchar(8) NOT NULL default '00000000',
  "AAAAVRYU" varchar(4) NOT NULL default ' ',
  "AAAAVRYV" varchar(4) NOT NULL default ' ',
  "AAAAVRYW" varchar(4) NOT NULL default ' ',
  "AAAAVRYX" varchar(4) NOT NULL default ' ',
  "AAAAWIFO" varchar(12) NOT NULL default ' ',
  "AAABLCEX" varchar(3) NOT NULL default ' ',
  "AAAAVRZA" varchar(1) NOT NULL default '0',
  "AAAAVRZB" varchar(1) NOT NULL default '0',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAVRZE" varchar(30) NOT NULL default ' ',
  "AAAAVRZF" varchar(30) NOT NULL default ' ',
  "AAAAVRZG" varchar(1) NOT NULL default ' ',
  "AAAAVRZH" varchar(12) NOT NULL default ' ',
  "AAAAVRZI" varchar(4) NOT NULL default ' ',
  "AAAAVRZJ" varchar(8) NOT NULL default '00000000',
  "AAAAVRZK" varchar(4) NOT NULL default '0000',
  "AAAAVRZL" decimal(13,2) NOT NULL default '0.00',
  "AAABLOQL" decimal(5,2) NOT NULL default '0.00',
  "AAABMCRL" varchar(16) NOT NULL default ' ',
  "AAAAVSDU" varchar(12) NOT NULL default ' ',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAAAVRZR" varchar(15) NOT NULL default ' ',
  "AAABKRIG" varchar(2) NOT NULL default ' ',
  "AAAAVRZT" varchar(1) NOT NULL default ' ',
  "AAAAVRZU" varchar(8) NOT NULL default '00000000',
  "AAAAVRZV" varchar(15) NOT NULL default ' ',
  "AAAAVRZW" varchar(4) NOT NULL default ' ',
  "AAAAVRZX" varchar(1) NOT NULL default ' ',
  "AAAAVRZY" decimal(13,2) NOT NULL default '0.00',
  "AAAAVRZZ" decimal(13,2) NOT NULL default '0.00',
  "AAABLULM" varchar(8) NOT NULL default '00000000',
  "AAAAVSAB" varchar(8) NOT NULL default '00000000',
  "AAAAVSAC" varchar(16) NOT NULL default ' ',
  "AAAAVSAD" varchar(8) NOT NULL default '00000000',
  "AAAAVSAE" varchar(8) NOT NULL default '00000000',
  "AAAAVSAF" varchar(5) NOT NULL default ' ',
  "AAAAVSAG" varchar(5) NOT NULL default ' ',
  "AAAAVSAH" varchar(4) NOT NULL default ' ',
  "AAAAVSAI" varchar(4) NOT NULL default ' ',
  "AAAAVSAJ" varchar(10) NOT NULL default ' ',
  "AAAAVSAK" varchar(25) NOT NULL default ' ',
  "AAAAWWQT" varchar(25) NOT NULL default ' ',
  "AAABLZMW" varchar(3) NOT NULL default ' ',
  "AAABKKRR" varchar(3) NOT NULL default ' ',
  "AAAAVSAO" decimal(13,3) NOT NULL default '0.000',
  "AAABMHEL" varchar(25) NOT NULL default ' ',
  "AAABMCRA" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEPBY" varchar(50) NOT NULL default ' ',
  "AAAAVSAT" varchar(50) NOT NULL default ' ',
  "AAAAVSAU" varchar(1) NOT NULL default ' ',
  "AAAAVSAV" varchar(1) NOT NULL default ' ',
  "AAAAVSAW" varchar(1) NOT NULL default ' ',
  "AAAAVSAX" varchar(1) NOT NULL default ' ',
  "AAAAVSAY" varchar(1) NOT NULL default ' ',
  "AAAAVSAZ" varchar(1) NOT NULL default ' ',
  "AAAAVSBA" varchar(10) NOT NULL default ' ',
  "AAAAXKFO" varchar(8) NOT NULL default '00000000',
  "AAAAXTSJ" varchar(8) NOT NULL default '00000000',
  "AAAAVSBD" varchar(8) NOT NULL default '00000000',
  "AAAAVSBE" varchar(3) NOT NULL default '000',
  "AAAAVSBF" varchar(3) NOT NULL default '000',
  "AAAAVSBG" varchar(3) NOT NULL default '000',
  "AAAAVSBH" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSBI" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSBJ" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSBK" decimal(7,4) NOT NULL default '0.0000',
  "AAAAVSBL" decimal(7,4) NOT NULL default '0.0000',
  "AAAAVSBM" varchar(8) NOT NULL default '00000000',
  "AAAAVSBN" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSBO" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSBP" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSBQ" decimal(5,0) NOT NULL default '0',
  "AAAAVSBR" varchar(15) NOT NULL default ' ',
  "AAABBAAI" varchar(50) NOT NULL default ' ',
  "AAABCDTJ" varchar(1) NOT NULL default ' ',
  "AAAAVSLE" varchar(1) NOT NULL default ' ',
  "AAABFBBM" varchar(10) NOT NULL default ' ',
  "AAAAVSBW" varchar(8) NOT NULL default '00000000',
  "AAAAVSBX" varchar(12) NOT NULL default ' ',
  "AAAAVSBY" varchar(8) NOT NULL default '00000000',
  "AAAAVSBZ" varchar(12) NOT NULL default ' ',
  "AAAAVSCA" varchar(8) NOT NULL default '00000000',
  "AAAAVSCB" varchar(12) NOT NULL default ' ',
  "AAAAVSCC" varchar(8) NOT NULL default '00000000',
  "AAAAVSCD" varchar(12) NOT NULL default ' ',
  "AAAAVSCE" varchar(8) NOT NULL default '00000000',
  "AAAAVSCF" varchar(12) NOT NULL default ' ',
  "AAAAVSCG" varchar(8) NOT NULL default '00000000',
  "AAAAVSCH" varchar(12) NOT NULL default ' ',
  "AAAAVSCI" varchar(8) NOT NULL default '00000000',
  "AAAAVSCJ" varchar(12) NOT NULL default ' ',
  "AAAAVSCK" varchar(1) NOT NULL default '0',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAZVRG" varchar(2) NOT NULL default ' ',
  "AAAAVSCN" varchar(1) NOT NULL default ' ',
  "AAAAWIER" varchar(8) NOT NULL default ' ',
  "AAABMLTD" varchar(8) NOT NULL default '00000000',
  "AAAAVSCQ" varchar(1) NOT NULL default ' ',
  "AAAAVSCR" varchar(1) NOT NULL default ' ',
  "AAAAVSCS" varchar(1) NOT NULL default ' ',
  "AAAAVSDF" varchar(30) NOT NULL default ' ',
  "AAAAVSCU" varchar(1) NOT NULL default ' ',
  "AAABLXTF" varchar(18) NOT NULL default ' ',
  "AAABKOVI" varchar(5) NOT NULL default ' ',
  "AAAAVSCX" varchar(8) NOT NULL default '00000000',
  "AAAABACH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMQVT","AAABMQVU"),
  KEY "AAAABACM" ("AAABMRIL","AAABMREX","AAAAVSDF","AAABKGBX"),
  KEY "AAAABACR" ("AAABKGBX","AAABMREX","AAABMRIL","AAABMQVT","AAABMQVU"),
  KEY "AAAABACX" ("AAAAWIFO","AAABMREX","AAABMRIL","AAABMQVT","AAABMQVU"),
  KEY "AAAABADD" ("AAAAVSDU","AAABMREX","AAABMRIL"),
  KEY "AAAABADH" ("AAABMHEL","AAABMREX","AAABMRIL"),
  KEY "AAAABADL" ("AAAAVSCS","AAABMREX","AAABMRIL","AAABMQVT","AAABMQVU"),
  KEY "AAAABADR" ("AAABMLTD","AAABMREX","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABADV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABIAWX" varchar(2) NOT NULL default '00',
  "AAAAWIPQ" varchar(8) NOT NULL default '00000000',
  "AAABDGZI" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAAAVZCP" varchar(1) NOT NULL default ' ',
  "AAAAVZET" varchar(1) NOT NULL default ' ',
  "AAAAWHWZ" varchar(4) NOT NULL default ' ',
  "AAAAVSEU" varchar(1) NOT NULL default ' ',
  "AAAAVSEV" varchar(1) NOT NULL default '0',
  "AAAAVSEW" varchar(1) NOT NULL default ' ',
  "AAAAVSEX" varchar(8) NOT NULL default '00000000',
  "AAAAVSEY" varchar(8) NOT NULL default '00000000',
  "AAAAVSEZ" varchar(8) NOT NULL default '00000000',
  "AAAAWIBR" varchar(2) NOT NULL default ' ',
  "AAAAWJKJ" varchar(4) NOT NULL default ' ',
  "AAAAWJLN" varchar(3) NOT NULL default '000',
  "AAAAVSFD" varchar(3) NOT NULL default '000',
  "AAAAVSFE" decimal(7,4) NOT NULL default '0.0000',
  "AAAAVSFF" decimal(7,4) NOT NULL default '0.0000',
  "AAABBVGN" varchar(5) NOT NULL default ' ',
  "AAAAVSFH" varchar(5) NOT NULL default ' ',
  "AAAAVSFI" decimal(7,4) NOT NULL default '0.0000',
  "AAAAVSFJ" varchar(4) NOT NULL default '0000',
  "AAAAVSFK" varchar(3) NOT NULL default '000',
  "AAAAVSFL" varchar(3) NOT NULL default '000',
  "AAAAVSFM" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSFN" varchar(4) NOT NULL default '0000',
  "AAAAVSFO" varchar(3) NOT NULL default '000',
  "AAAAVSLE" varchar(1) NOT NULL default ' ',
  "AAAAWIFY" varchar(2) NOT NULL default '00',
  "AAAAVSFR" varchar(1) NOT NULL default ' ',
  "AAAAVSGF" varchar(12) NOT NULL default ' ',
  "AAAAVSFT" varchar(4) NOT NULL default ' ',
  "AAABBRKQ" varchar(4) NOT NULL default '0000',
  "AAABBRKR" varchar(3) NOT NULL default '000',
  "AAAAVSFW" varchar(8) NOT NULL default '00000000',
  "AAAAVSFX" varchar(8) NOT NULL default '00000000',
  "AAAAKQHZ" varchar(4) NOT NULL default ' ',
  "AAAAKQKL" varchar(8) NOT NULL default '00000000',
  "AAAABAFQ" varchar(5) NOT NULL default ' ',
  "AAAABAFR" decimal(14,11) NOT NULL default '0.00000000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMQVT","AAABMQVU","AAABIAWX","AAAAWIPQ"),
  KEY "AAAABAFY" ("AAAAVSGF","AAABMREX","AAABMRIL","AAAAVSFT","AAABIAWX","AAABMQVT","AAABMQVU"),
  KEY "AAAABAGG" ("AAABMRIL","AAABMREX","AAAAWJKJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABAGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABIAWX" varchar(2) NOT NULL default '00',
  "AAABKKEO" varchar(4) NOT NULL default '0000',
  "AAAABAGR" decimal(15,13) NOT NULL default '0.0000000000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMQVT","AAABMQVU","AAABIAWX","AAABKKEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABAGY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABIAWX" varchar(2) NOT NULL default '00',
  "AAABIAWY" varchar(4) NOT NULL default '0000',
  "AAABIAXI" varchar(4) NOT NULL default '0000',
  "AAABCBAI" varchar(3) NOT NULL default '000',
  "AAABCBAJ" varchar(2) NOT NULL default '00',
  "AAAAVSGZ" varchar(1) NOT NULL default '0',
  "AAAAVSHA" varchar(3) NOT NULL default '000',
  "AAAAVSHB" varchar(3) NOT NULL default '000',
  "AAAAVSHC" varchar(3) NOT NULL default '000',
  "AAAAVSHD" varchar(3) NOT NULL default '000',
  "AAAAVSHE" decimal(15,0) NOT NULL default '0',
  "AAAAVSHF" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHG" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHH" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHI" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHJ" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHK" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHL" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHM" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHN" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHO" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHP" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHQ" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHR" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHS" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHT" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHU" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHV" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHW" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHX" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHY" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHZ" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSIA" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSIB" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSIC" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSID" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSIE" decimal(13,2) NOT NULL default '0.00',
  "AAABBOEJ" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSIG" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSIH" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSII" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSIJ" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSIK" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSIL" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSIM" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSIN" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSIO" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSIP" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSIQ" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSIR" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSIS" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSIT" decimal(13,2) NOT NULL default '0.00',
  "AAABIATM" decimal(13,2) NOT NULL default '0.00',
  "AAABIATQ" decimal(13,2) NOT NULL default '0.00',
  "AAABIATW" decimal(13,2) NOT NULL default '0.00',
  "AAABIATX" decimal(13,2) NOT NULL default '0.00',
  "AAABIATO" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSIZ" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSJA" decimal(13,2) NOT NULL default '0.00',
  "AAABIATL" decimal(13,2) NOT NULL default '0.00',
  "AAABIATP" decimal(13,2) NOT NULL default '0.00',
  "AAABIATY" decimal(13,2) NOT NULL default '0.00',
  "AAABIATZ" decimal(13,2) NOT NULL default '0.00',
  "AAABIATN" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSJG" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSLE" varchar(1) NOT NULL default ' ',
  "AAAAVSJI" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSJJ" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSJK" varchar(3) NOT NULL default '000',
  "AAAAVSJL" varchar(3) NOT NULL default '000',
  "AAAAVSJM" varchar(1) NOT NULL default ' ',
  "AAAAVSJN" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMQVT","AAABMQVU","AAABMDBB","AAABIAWX","AAABIAWY","AAABIAXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABAKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCXU" varchar(5) NOT NULL default '00000',
  "AAAAKIFV" varchar(10) NOT NULL default ' ',
  "AAAAHWVL" varchar(3) NOT NULL default '000',
  "AAAAIBWX" varchar(10) NOT NULL default ' ',
  "AAAAIBXA" varchar(3) NOT NULL default '000',
  "AAAAIBWZ" varchar(4) NOT NULL default '0000',
  "AAAAVTYL" varchar(1) NOT NULL default ' ',
  "AAAAVTYK" varchar(10) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABMCXV" varchar(14) NOT NULL default ' ',
  "AAAABAKW" decimal(15,2) NOT NULL default '0.00',
  "AAAABAKX" decimal(13,2) NOT NULL default '0.00',
  "AAABIBGA" varchar(22) NOT NULL default ' ',
  "AAAABAKZ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMQVT","AAABMQVU","AAABMDBB","AAABMCXU","AAAAKIFV","AAAAHWVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABALI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAAAVSKA" varchar(4) NOT NULL default ' ',
  "AAAAVSKB" varchar(5) NOT NULL default '00000',
  "AAAAVSLE" varchar(1) NOT NULL default ' ',
  "AAAAVSKD" varchar(4) NOT NULL default ' ',
  "AAABMHEW" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMQVT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABALU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABFKYS" varchar(2) NOT NULL default ' ',
  "AAABFKYT" varchar(3) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAAABAMC" varchar(1) NOT NULL default ' ',
  "AAABKOVB" varchar(6) NOT NULL default ' ',
  "AAABKRGH" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAABFKYS","AAABFKYT"),
  UNIQUE KEY "AAAABAMJ" ("AAABMQVT","AAABMQVU","AAABMREX","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWZK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(20) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABIBGA" varchar(22) NOT NULL default ' ',
  "AAABMCXV" varchar(14) NOT NULL default ' ',
  "AAAABAMZ" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMQVT","AAABMQVU","AAABMDBB","AAABLTXG","AAABLUNJ","AAABKOTY","AAABIBGA","AAABMCXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABANK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKPWJ" varchar(3) NOT NULL default '000',
  "AAAABARV" varchar(2) NOT NULL default '00',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABICIR" varchar(2) NOT NULL default '00',
  "AAABIAWY" varchar(4) NOT NULL default '0000',
  "AAABIAXI" varchar(4) NOT NULL default '0000',
  "AAAAVSHQ" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHX" decimal(13,2) NOT NULL default '0.00',
  "AAAABAQQ" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHR" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHY" decimal(13,2) NOT NULL default '0.00',
  "AAAABAQT" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHS" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHZ" decimal(13,2) NOT NULL default '0.00',
  "AAAABAQW" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHT" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSIA" decimal(13,2) NOT NULL default '0.00',
  "AAAABAQZ" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHU" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSIB" decimal(13,2) NOT NULL default '0.00',
  "AAAABARC" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHV" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSIC" decimal(13,2) NOT NULL default '0.00',
  "AAAANROB" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHW" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSID" decimal(13,2) NOT NULL default '0.00',
  "AAAABARI" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSIE" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSII" decimal(13,2) NOT NULL default '0.00',
  "AAAAWJKS" varchar(8) NOT NULL default ' ',
  "AAAAWJKJ" varchar(4) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAAAVSKW" varchar(12) NOT NULL default ' ',
  "AAAABAOX" varchar(1) NOT NULL default ' ',
  "AAAAVSLE" varchar(1) NOT NULL default ' ',
  "AAABMCZQ" varchar(16) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABKPUA" varchar(6) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABKRUO" varchar(20) NOT NULL default ' ',
  "AAAABAXO" varchar(10) NOT NULL default ' ',
  "AAAABAXP" varchar(16) NOT NULL default ' ',
  "AAAABAXQ" varchar(20) NOT NULL default ' ',
  "AAAABAXR" varchar(16) NOT NULL default ' ',
  "AAABMCXE" varchar(8) NOT NULL default ' ',
  "AAAABAXT" varchar(8) NOT NULL default '00000000',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBB","AAABKPWJ","AAAABARV","AAABMQVT","AAABMQVU","AAABICIR","AAABIAWY","AAABIAXI"),
  KEY "AAAABAPV" ("AAABMRIL","AAABMREX","AAABMQVT","AAABMQVU","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABAQB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKPWJ" varchar(3) NOT NULL default '000',
  "AAAAWJKS" varchar(8) NOT NULL default ' ',
  "AAABMCZQ" varchar(16) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAAAVSKW" varchar(12) NOT NULL default ' ',
  "AAABKPUA" varchar(6) NOT NULL default ' ',
  "AAAABARV" varchar(2) NOT NULL default '00',
  "AAABICIR" varchar(2) NOT NULL default '00',
  "AAAAVSHQ" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHX" decimal(13,2) NOT NULL default '0.00',
  "AAAABAQQ" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHR" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHY" decimal(13,2) NOT NULL default '0.00',
  "AAAABAQT" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHS" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHZ" decimal(13,2) NOT NULL default '0.00',
  "AAAABAQW" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHT" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSIA" decimal(13,2) NOT NULL default '0.00',
  "AAAABAQZ" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHU" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSIB" decimal(13,2) NOT NULL default '0.00',
  "AAAABARC" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHV" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSIC" decimal(13,2) NOT NULL default '0.00',
  "AAAANROB" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSHW" decimal(13,2) NOT NULL default '0.00',
  "AAAAVSID" decimal(13,2) NOT NULL default '0.00',
  "AAAABARI" decimal(13,2) NOT NULL default '0.00',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAAAVSLE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBB","AAABKPWJ","AAAAWJKS","AAABMCZQ","AAABMCRK","AAABMCPV","AAAAVSKW","AAABKPUA","AAAABARV","AAABICIR"),
  KEY "AAAABARX" ("AAABMCRK","AAABMREX","AAABMDBB","AAABKPWJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABASC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABEPBY" varchar(50) NOT NULL default ' ',
  "AAAAVSAT" varchar(50) NOT NULL default ' ',
  "AAAAVSAU" varchar(1) NOT NULL default ' ',
  "AAAAVSAV" varchar(1) NOT NULL default ' ',
  "AAAABASM" varchar(1) NOT NULL default ' ',
  "AAAAVSAZ" varchar(1) NOT NULL default ' ',
  "AAAAVSAW" varchar(1) NOT NULL default ' ',
  "AAAAVSAX" varchar(1) NOT NULL default ' ',
  "AAAAVSAY" varchar(1) NOT NULL default ' ',
  "AAAAVSLE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABMQVT","AAABMQVU"),
  KEY "AAAABASX" ("AAABMRIL","AAABMREX","AAABMQVT","AAABMQVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABATC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMQVT","AAABMQVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABATL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAAABAVC" varchar(2) NOT NULL default '00',
  "AAABLHXQ" varchar(2) NOT NULL default ' ',
  "AAAABATS" varchar(5) NOT NULL default ' ',
  "AAAABAUU" decimal(13,2) NOT NULL default '0.00',
  "AAAABATU" varchar(4) NOT NULL default '0000',
  "AAAAYBRU" varchar(2) NOT NULL default ' ',
  "AAAABAUW" varchar(1) NOT NULL default ' ',
  "AAAABAUX" decimal(13,2) NOT NULL default '0.00',
  "AAAABATY" varchar(4) NOT NULL default '0000',
  "AAAABATZ" varchar(4) NOT NULL default '0000',
  "AAAABAUA" varchar(8) NOT NULL default '00000000',
  "AAAANJFZ" varchar(5) NOT NULL default ' ',
  "AAAABAUC" varchar(15) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAABAUE" varchar(1) NOT NULL default ' ',
  "AAAABAUF" varchar(50) NOT NULL default ' ',
  "AAAAVSLE" varchar(1) NOT NULL default ' ',
  "AAABMCXU" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMQVT","AAABMQVU","AAAABAVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABAUN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAAABAVC" varchar(2) NOT NULL default '00',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAABAUU" decimal(13,2) NOT NULL default '0.00',
  "AAAABAUV" decimal(13,2) NOT NULL default '0.00',
  "AAAABAUW" varchar(1) NOT NULL default ' ',
  "AAAABAUX" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMQVT","AAABMQVU","AAAABAVC","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABAVE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAAABAVJ" varchar(1) NOT NULL default ' ',
  "AAAAVSBW" varchar(8) NOT NULL default '00000000',
  "AAAAVSBX" varchar(12) NOT NULL default ' ',
  "AAAABAVM" varchar(1) NOT NULL default ' ',
  "AAAAVSBY" varchar(8) NOT NULL default '00000000',
  "AAAAVSBZ" varchar(12) NOT NULL default ' ',
  "AAAABAVP" varchar(1) NOT NULL default ' ',
  "AAAAVSCA" varchar(8) NOT NULL default '00000000',
  "AAAAVSCB" varchar(12) NOT NULL default ' ',
  "AAAABAVS" varchar(1) NOT NULL default ' ',
  "AAAAVSCC" varchar(8) NOT NULL default '00000000',
  "AAAAVSCD" varchar(12) NOT NULL default ' ',
  "AAAABAVV" varchar(1) NOT NULL default ' ',
  "AAAAVSCE" varchar(8) NOT NULL default '00000000',
  "AAAAVSCF" varchar(12) NOT NULL default ' ',
  "AAAABAVY" varchar(1) NOT NULL default ' ',
  "AAAAVSCG" varchar(8) NOT NULL default '00000000',
  "AAAAVSCH" varchar(12) NOT NULL default ' ',
  "AAAABAWB" varchar(1) NOT NULL default ' ',
  "AAAAVSCI" varchar(8) NOT NULL default '00000000',
  "AAAAVSCJ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMQVT","AAABMQVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABAWI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAAAWIPQ" varchar(8) NOT NULL default '00000000',
  "AAABDGZI" varchar(8) NOT NULL default '00000000',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABKPUA" varchar(6) NOT NULL default ' ',
  "AAAAVSKT" decimal(3,2) NOT NULL default '0.00',
  "AAAAVSKU" varchar(1) NOT NULL default ' ',
  "AAABKRIO" varchar(10) NOT NULL default ' ',
  "AAAAVSKW" varchar(12) NOT NULL default ' ',
  "AAAAVSKX" varchar(12) NOT NULL default ' ',
  "AAAAVSKY" varchar(4) NOT NULL default ' ',
  "AAAAXXDX" varchar(8) NOT NULL default ' ',
  "AAAAVSLA" varchar(12) NOT NULL default ' ',
  "AAAAVSLB" varchar(16) NOT NULL default ' ',
  "AAABFCZA" varchar(3) NOT NULL default ' ',
  "AAABLKLZ" varchar(30) NOT NULL default ' ',
  "AAAAVSLE" varchar(1) NOT NULL default ' ',
  "AAABMCXD" varchar(15) NOT NULL default ' ',
  "AAAAVSLG" varchar(8) NOT NULL default '00000000',
  "AAABLWPH" varchar(15) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAVSLJ" varchar(10) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABMCZQ" varchar(16) NOT NULL default ' ',
  "AAABKRUO" varchar(20) NOT NULL default ' ',
  "AAAABAXO" varchar(10) NOT NULL default ' ',
  "AAAABAXP" varchar(16) NOT NULL default ' ',
  "AAAABAXQ" varchar(20) NOT NULL default ' ',
  "AAAABAXR" varchar(16) NOT NULL default ' ',
  "AAABMCXE" varchar(8) NOT NULL default ' ',
  "AAAABAXT" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMQVT","AAABMQVU","AAAAWIPQ"),
  KEY "AAAABAXZ" ("AAABMCPV","AAABMREX","AAABMRIL","AAABMQVT","AAABMQVU"),
  KEY "AAAABAYF" ("AAABMCRK","AAABMREX","AAABMRIL","AAABMQVT","AAABMQVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABAYL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIO" varchar(2) NOT NULL default ' ',
  "AAABCDPU" varchar(2) NOT NULL default ' ',
  "AAAABHVL" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIO","AAABCDPU","AAAABHVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABAYU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLLIO" varchar(2) NOT NULL default ' ',
  "AAABCDPU" varchar(2) NOT NULL default ' ',
  "AAAABHVL" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLLIO","AAABCDPU","AAAABHVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABAZG" (
  "AAAAQXDL" varchar(30) NOT NULL default ' ',
  "AAABLTQF" varchar(30) NOT NULL default ' ',
  "AAAABAZJ" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAAQXDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABAZN" (
  "AAAAQXDL" varchar(30) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKZAW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAAQXDL","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABAZT" (
  "AAAAQXDL" varchar(30) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAAQXDL","AAABIRUN"),
  KEY "AAAABBAA" ("AAABIRUN","AAAAQXDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBAD" (
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABLFYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBAL" (
  "AAAAQXDL" varchar(30) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAAABBBE" varchar(30) NOT NULL default ' ',
  "AAABCZIY" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAAQXDL","AAABMKFH","AAABLFYD","AAAABBBE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBAX" (
  "AAAAQXDL" varchar(30) NOT NULL default ' ',
  "AAAABBBE" varchar(30) NOT NULL default ' ',
  "AAABCZIY" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAAQXDL","AAAABBBE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBBF" (
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAAAQXDL" varchar(30) NOT NULL default ' ',
  "AAAABBCC" int(11) NOT NULL default '0',
  "AAAABBCD" int(11) NOT NULL default '0',
  "AAABLUZL" varchar(32) NOT NULL default ' ',
  "AAAABBCF" varchar(2) NOT NULL default '00',
  "AAABIVCA" varchar(1) NOT NULL default ' ',
  "AAABDEFZ" varchar(1) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABLFYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBBW" (
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAAAQXDL" varchar(30) NOT NULL default ' ',
  "AAAABBCC" int(11) NOT NULL default '0',
  "AAAABBCD" int(11) NOT NULL default '0',
  "AAABLUZL" varchar(32) NOT NULL default ' ',
  "AAAABBCF" varchar(2) NOT NULL default '00',
  "AAABIVCA" varchar(1) NOT NULL default ' ',
  "AAABDEFZ" varchar(1) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABLFYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBCN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCXU" varchar(5) NOT NULL default '00000',
  "AAABIAWX" varchar(2) NOT NULL default '00',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMQVT","AAABMQVU","AAABMDBB","AAABMCXU","AAABIAWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABICIR" varchar(2) NOT NULL default '00',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCMR" varchar(2) NOT NULL default '00',
  "AAABMCRA" varchar(6) NOT NULL default ' ',
  "AAAAWJKS" varchar(8) NOT NULL default ' ',
  "AAAABBDN" varchar(10) NOT NULL default ' ',
  "AAAABBDO" varchar(10) NOT NULL default ' ',
  "AAAAWJEQ" varchar(4) NOT NULL default ' ',
  "AAABGTHF" varchar(4) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABFVNI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABICIR","AAABMDBB","AAABMCMR","AAABMCRA","AAAAWJKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBEA" (
  "AAABEPTB" varchar(20) NOT NULL default ' ',
  "AAAATROL" varchar(10) NOT NULL default '0000000000',
  "AAABMNDU" varchar(40) NOT NULL default ' ',
  "AAAABBEE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPTB","AAAATROL","AAABMNDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBEI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABLRFA" varchar(6) NOT NULL default ' ',
  "AAABHIKM" varchar(12) NOT NULL default ' ',
  "AAAAKART" varchar(6) NOT NULL default '000000',
  "AAAALOKW" varchar(22) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLWVZ" varchar(6) NOT NULL default '000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAAABBES" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKFH","AAABLRFA","AAABHIKM","AAAAKART")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBEY" (
  "AAAAYSJE" varchar(12) NOT NULL default ' ',
  "AAABLMGD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYSJE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBFC" (
  "AAABHRMU" varchar(75) NOT NULL default ' ',
  "AAABKCWA" varchar(1) NOT NULL default ' ',
  "AAABEPTB" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABGWVP" varchar(8) NOT NULL default '00000000',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABHRMU","AAABKCWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBFL" (
  "AAAAVPNZ" varchar(20) NOT NULL default ' ',
  "AAABIFOV" int(11) NOT NULL default '0',
  "AAABJLFF" int(11) NOT NULL default '0',
  "AAAAZZFR" smallint(6) NOT NULL default '0',
  "AAAAVQDA" smallint(6) NOT NULL default '0',
  "AAABHNIU" smallint(6) NOT NULL default '0',
  "AAABLUSP" text,
  PRIMARY KEY ("AAAAVPNZ","AAABIFOV","AAABJLFF","AAAAZZFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBFX" (
  "AAAAMMXS" varchar(32) NOT NULL default ' ',
  "AAAABBHN" varchar(8) NOT NULL default ' ',
  "AAAADUHW" varchar(4) NOT NULL default ' ',
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABIMHJ" varchar(12) NOT NULL default ' ',
  "AAABLEJI" varchar(40) NOT NULL default ' ',
  "AAABFNVZ" varchar(30) NOT NULL default ' ',
  "AAAABBHT" varchar(1) NOT NULL default ' ',
  "AAAAVPNZ" varchar(20) NOT NULL default ' ',
  "AAABGSHX" varchar(1) NOT NULL default ' ',
  "AAAABBGI" varchar(1) NOT NULL default ' ',
  "AAABLEUZ" varchar(12) NOT NULL default ' ',
  "AAABGHUE" varchar(8) NOT NULL default ' ',
  "AAAAZOEQ" varchar(12) NOT NULL default ' ',
  "AAABJAEL" varchar(8) NOT NULL default '00000000',
  "AAABLOCX" varchar(6) NOT NULL default '000000',
  "AAAABBGO" varchar(40) NOT NULL default ' ',
  "AAAAFASD" varchar(1) NOT NULL default ' ',
  "AAABLRTV" varchar(8) NOT NULL default '00000000',
  "AAABLRTW" varchar(6) NOT NULL default '000000',
  "AAAABBGS" varchar(1) NOT NULL default ' ',
  "AAAABBGT" int(11) NOT NULL default '0',
  "AAAABBGU" int(11) NOT NULL default '0',
  "AAAABBGV" int(11) NOT NULL default '0',
  "AAAABBGW" varchar(8) NOT NULL default '00000000',
  "AAAABBGX" varchar(6) NOT NULL default '000000',
  "AAAABBGY" varchar(1) NOT NULL default ' ',
  "AAAARBDY" int(11) NOT NULL default '0',
  "AAAABBHA" int(11) NOT NULL default '0',
  "AAAABBHB" int(11) NOT NULL default '0',
  "AAAABBHC" varchar(8) NOT NULL default '00000000',
  "AAAABBHD" varchar(6) NOT NULL default '000000',
  "AAAABBHE" int(11) NOT NULL default '0',
  "AAABKPHH" int(11) NOT NULL default '0',
  "AAAABBHG" int(11) NOT NULL default '0',
  "AAAABBHH" int(11) NOT NULL default '0',
  "AAAABBHI" int(11) NOT NULL default '0',
  "AAAABBHJ" int(11) NOT NULL default '0',
  "AAAABBHK" int(11) NOT NULL default '0',
  "AAAABBHL" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAAMMXS","AAAABBHN","AAAADUHW","AAABMONG","AAABIMHJ","AAABLEJI","AAABFNVZ","AAAABBHT","AAAAVPNZ"),
  UNIQUE KEY "AAAABBHV" ("AAAAVPNZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBHX" (
  "AAAABBIH" varchar(20) NOT NULL default ' ',
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJAEL" varchar(8) NOT NULL default '00000000',
  "AAABLOCX" varchar(6) NOT NULL default '000000',
  "AAABIMHJ" varchar(12) NOT NULL default ' ',
  "AAAAVPNZ" varchar(20) NOT NULL default ' ',
  "AAAAZOEQ" varchar(12) NOT NULL default ' ',
  "AAAAMMXS" varchar(32) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAABBIH","AAABMONG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBIJ" (
  "AAABJEEM" varchar(20) NOT NULL default ' ',
  "AAABGCZX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEEM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBIN" (
  "AAABLUSV" varchar(20) NOT NULL default ' ',
  "AAABMLPV" varchar(60) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABLTQG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABMLPV"),
  KEY "AAAABBIW" ("AAABIRUN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBIY" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(20) NOT NULL default ' ',
  "AAABMLPV" varchar(60) NOT NULL default ' ',
  "AAABLPWW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLUSV","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBJG" (
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABAGTE" varchar(1) NOT NULL default ' ',
  "AAABANNN" varchar(24) NOT NULL default ' ',
  "AAABMLCA" varchar(30) NOT NULL default ' ',
  "AAABIKVA" varchar(12) NOT NULL default ' ',
  "AAABFZAB" varchar(14) NOT NULL default ' ',
  "AAAAQQFN" varchar(40) NOT NULL default ' ',
  "AAAABBKA" varchar(3) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABIANB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBJS" (
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABAGTE" varchar(1) NOT NULL default ' ',
  "AAABANNN" varchar(24) NOT NULL default ' ',
  "AAABMLCA" varchar(30) NOT NULL default ' ',
  "AAABIKVA" varchar(12) NOT NULL default ' ',
  "AAABFZAB" varchar(14) NOT NULL default ' ',
  "AAAAQQFN" varchar(40) NOT NULL default ' ',
  "AAAABBKA" varchar(3) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABIANB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBKE" (
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABMLZS" varchar(6) NOT NULL default '000000',
  "AAAABBKQ" varchar(30) NOT NULL default ' ',
  "AAAABBKR" varchar(70) NOT NULL default ' ',
  "AAAABBKS" varchar(8) NOT NULL default ' ',
  "AAAABBKT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIANB","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBKN" (
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABMLZS" varchar(6) NOT NULL default '000000',
  "AAAABBKQ" varchar(30) NOT NULL default ' ',
  "AAAABBKR" varchar(70) NOT NULL default ' ',
  "AAAABBKS" varchar(8) NOT NULL default ' ',
  "AAAABBKT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIANB","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(22) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLRSP" varchar(1) NOT NULL default ' ',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABLRSS" varchar(12) NOT NULL default ' ',
  "AAABLRST" varchar(8) NOT NULL default ' ',
  "AAABLRSU" varchar(8) NOT NULL default '00000000',
  "AAABLRSV" varchar(8) NOT NULL default '00000000',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBLO" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(60) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBLX" (
  "AAABIJSB" varchar(12) NOT NULL default ' ',
  "AAAAHYHV" varchar(5) NOT NULL default '00000',
  "AAAABBMA" varchar(12) NOT NULL default ' ',
  "AAAABBMB" varchar(8) NOT NULL default '00000000',
  "AAAABBMC" varchar(12) NOT NULL default ' ',
  "AAAABBMD" varchar(8) NOT NULL default '00000000',
  "AAABKNBX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIJSB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBMG" (
  "AAABIJSB" varchar(12) NOT NULL default ' ',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABJARY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIJSB","AAABLXIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBMM" (
  "AAABGUFO" varchar(24) NOT NULL default ' ',
  "AAABIJSB" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABGUFO","AAABIJSB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBMR" (
  "AAAARTPY" varchar(24) NOT NULL default ' ',
  "AAABLUNJ" varchar(32) NOT NULL default ' ',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAARTPY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBMW" (
  "AAABIJSB" varchar(12) NOT NULL default ' ',
  "AAAABBTK" varchar(14) NOT NULL default ' ',
  "AAABGUFO" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABIJSB","AAAABBTK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBNC" (
  "AAABIKVA" varchar(12) NOT NULL default ' ',
  "AAABFZAB" varchar(14) NOT NULL default ' ',
  "AAABLUNJ" varchar(32) NOT NULL default ' ',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABIKVA","AAABFZAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBNJ" (
  "AAABIKVA" varchar(12) NOT NULL default ' ',
  "AAABFZAB" varchar(14) NOT NULL default ' ',
  "AAAAJHOK" varchar(12) NOT NULL default '000000000000',
  "AAAABBSG" varchar(30) NOT NULL default ' ',
  "AAAABBSH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIKVA","AAABFZAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBNR" (
  "AAABGUFO" varchar(24) NOT NULL default ' ',
  "AAABJDAN" varchar(1) NOT NULL default ' ',
  "AAABGUEU" varchar(20) NOT NULL default ' ',
  "AAAABBSQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGUFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBNX" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABIJSB" varchar(12) NOT NULL default ' ',
  "AAAAHYHV" varchar(5) NOT NULL default '00000',
  "AAABGAMR" varchar(4) NOT NULL default '0000',
  "AAAABBYI" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABIJSB","AAAAHYHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBOH" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABIJSB" varchar(12) NOT NULL default ' ',
  "AAAABBTK" varchar(14) NOT NULL default ' ',
  "AAAAHYHV" varchar(5) NOT NULL default '00000',
  "AAAATJAQ" varchar(1) NOT NULL default ' ',
  "AAABGAMR" varchar(4) NOT NULL default '0000',
  "AAAABBYI" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABIJSB","AAAABBTK","AAAAHYHV"),
  KEY "AAAABBOU" ("AAABIJSB","AAAABBTK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBOX" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABGUFO" varchar(24) NOT NULL default ' ',
  "AAAAHYHV" varchar(5) NOT NULL default '00000',
  "AAAATJAQ" varchar(1) NOT NULL default ' ',
  "AAABGAMR" varchar(4) NOT NULL default '0000',
  "AAAABBYI" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABGUFO","AAAAHYHV"),
  KEY "AAAABBPI" ("AAABLSCE","AAAATJAQ"),
  KEY "AAAABBPL" ("AAABGUFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBPN" (
  "AAAALYIP" varchar(20) NOT NULL default ' ',
  "AAABLRPR" varchar(4) NOT NULL default '0000',
  "AAABMMRH" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAALYIP","AAABLRPR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBPT" (
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAAALYIP" varchar(20) NOT NULL default ' ',
  "AAABLRPR" varchar(4) NOT NULL default '0000',
  "AAAALYIM" varchar(1) NOT NULL default ' ',
  "AAABMMRH" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNJW","AAAALYIP","AAABLRPR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBQC" (
  "AAABLUFA" varchar(30) NOT NULL default ' ',
  "AAAABBQR" varchar(80) NOT NULL default ' ',
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAAABBQO" varchar(30) NOT NULL default ' ',
  "AAAAHRZR" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUFA","AAAABBQR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBQK" (
  "AAABLUFA" varchar(30) NOT NULL default ' ',
  "AAAABBQR" varchar(80) NOT NULL default ' ',
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAAABBQO" varchar(30) NOT NULL default ' ',
  "AAAAHRZR" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUFA","AAAABBQR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBQS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(60) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBRD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAARTPY" varchar(24) NOT NULL default ' ',
  "AAABLUNJ" varchar(32) NOT NULL default ' ',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAARTPY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBRK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIJSB" varchar(12) NOT NULL default ' ',
  "AAAABBTK" varchar(14) NOT NULL default ' ',
  "AAABGUFO" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIJSB","AAAABBTK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBRS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIKVA" varchar(12) NOT NULL default ' ',
  "AAABFZAB" varchar(14) NOT NULL default ' ',
  "AAABLUNJ" varchar(32) NOT NULL default ' ',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIKVA","AAABFZAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBSB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIKVA" varchar(12) NOT NULL default ' ',
  "AAABFZAB" varchar(14) NOT NULL default ' ',
  "AAAAJHOK" varchar(12) NOT NULL default '000000000000',
  "AAAABBSG" varchar(30) NOT NULL default ' ',
  "AAAABBSH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIKVA","AAABFZAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBSL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGUFO" varchar(24) NOT NULL default ' ',
  "AAABJDAN" varchar(1) NOT NULL default ' ',
  "AAABGUEU" varchar(20) NOT NULL default ' ',
  "AAAABBSQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGUFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBST" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABIJSB" varchar(12) NOT NULL default ' ',
  "AAAABBTK" varchar(14) NOT NULL default ' ',
  "AAAAHYHV" varchar(5) NOT NULL default '00000',
  "AAAATJAQ" varchar(1) NOT NULL default ' ',
  "AAABGAMR" varchar(4) NOT NULL default '0000',
  "AAAABBYI" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABIJSB","AAAABBTK","AAAAHYHV"),
  KEY "AAAABBTI" ("AAABIJSB","AAAABBTK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBTL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABGUFO" varchar(24) NOT NULL default ' ',
  "AAAAHYHV" varchar(5) NOT NULL default '00000',
  "AAAATJAQ" varchar(1) NOT NULL default ' ',
  "AAABGAMR" varchar(4) NOT NULL default '0000',
  "AAAABBYI" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABGUFO","AAAAHYHV"),
  KEY "AAAABBTY" ("AAABLSCE","AAAATJAQ"),
  KEY "AAAABBUB" ("AAABGUFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBUD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAQXER" varchar(32) NOT NULL default ' ',
  "AAABHYCA" varchar(6) NOT NULL default '000000',
  "AAAALPYR" varchar(1) NOT NULL default ' ',
  "AAABANNN" varchar(24) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAAABBUK" varchar(8) NOT NULL default '00000000',
  "AAAABBUL" varchar(6) NOT NULL default '000000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAABBUO" varchar(70) NOT NULL default ' ',
  "AAABLRJL" varchar(1) NOT NULL default '0',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABMNMY" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(45) NOT NULL default ' ',
  "AAABMNNA" varchar(45) NOT NULL default ' ',
  "AAAABBUU" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAQXER","AAABHYCA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBUY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAALPYR" varchar(1) NOT NULL default ' ',
  "AAABANNN" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAALPYR","AAABANNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBVF" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(40) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBVO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPVQ" varchar(30) NOT NULL default ' ',
  "AAAALPYR" varchar(1) NOT NULL default ' ',
  "AAABIKVA" varchar(12) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPVQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBVW" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(60) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBWG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(60) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBWS" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(60) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBXB" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAABBYL" varchar(1) NOT NULL default ' ',
  "AAABLOZE" varchar(1) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAAABBYO" varchar(3) NOT NULL default ' ',
  "AAAABBYP" varchar(3) NOT NULL default ' ',
  "AAABGJSL" varchar(1) NOT NULL default ' ',
  "AAAAHYHV" varchar(5) NOT NULL default '00000',
  "AAABLECD" varchar(1) NOT NULL default ' ',
  "AAAABBYH" varchar(6) NOT NULL default '000000',
  "AAAABBYI" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAABBYL","AAABLOZE","AAABMKFH","AAAABBYO","AAAABBYP","AAABGJSL","AAAAHYHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBXW" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAABBYL" varchar(1) NOT NULL default ' ',
  "AAABLOZE" varchar(1) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAAABBYO" varchar(3) NOT NULL default ' ',
  "AAAABBYP" varchar(3) NOT NULL default ' ',
  "AAABGJSL" varchar(1) NOT NULL default ' ',
  "AAAAHYHV" varchar(5) NOT NULL default '00000',
  "AAABMCWV" varchar(14) NOT NULL default ' ',
  "AAABLECD" varchar(1) NOT NULL default ' ',
  "AAAABBYH" varchar(6) NOT NULL default '000000',
  "AAAABBYI" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAABBYL","AAABLOZE","AAABMKFH","AAAABBYO","AAAABBYP","AAABGJSL","AAAAHYHV","AAABMCWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBYT" (
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAAABBYV" varchar(30) NOT NULL default ' ',
  "AAAAMNIM" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBYY" (
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABEGKI" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBZC" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBZI" (
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAAABBZQ" varchar(4) NOT NULL default '0000',
  "AAAABBZL" varchar(1) NOT NULL default ' ',
  "AAAABBZM" varchar(2) NOT NULL default ' ',
  "AAAABBZN" varchar(12) NOT NULL default ' ',
  "AAAABBZO" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLQGL","AAAABBZQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABBZR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAABCAC" varchar(32) NOT NULL default ' ',
  "AAAABCAZ" varchar(10) NOT NULL default ' ',
  "AAAABBZV" decimal(15,0) NOT NULL default '0',
  "AAAABBZW" double NOT NULL default '0',
  "AAAABBZX" double NOT NULL default '0',
  "AAAABBZY" double NOT NULL default '0',
  "AAAABBZZ" double NOT NULL default '0',
  "AAAABCAA" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAABCAC","AAAABCAZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCAE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAABCAZ" varchar(10) NOT NULL default ' ',
  "AAAADLEV" varchar(10) NOT NULL default ' ',
  "AAAABCAI" varchar(3) NOT NULL default ' ',
  "AAAABCAJ" int(11) NOT NULL default '0',
  "AAAABCAK" int(11) NOT NULL default '0',
  "AAAABCAL" varchar(1) NOT NULL default ' ',
  "AAAABCAM" int(11) NOT NULL default '0',
  "AAAABCAN" int(11) NOT NULL default '0',
  "AAAABCAO" varchar(1) NOT NULL default ' ',
  "AAAABCAP" int(11) NOT NULL default '0',
  "AAAABCAQ" int(11) NOT NULL default '0',
  "AAAABCAR" int(11) NOT NULL default '0',
  "AAAABCAS" varchar(1) NOT NULL default ' ',
  "AAAABCAT" varchar(1) NOT NULL default ' ',
  "AAAABCAU" varchar(40) NOT NULL default ' ',
  "AAABEYIJ" varchar(14) NOT NULL default ' ',
  "AAAABCAW" varchar(20) NOT NULL default ' ',
  "AAAABCAX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAABCAZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAADSVL" (
  "AAAADSVL" varchar(10) NOT NULL default ' ',
  "AAAABCBC" varchar(26) NOT NULL default ' ',
  PRIMARY KEY ("AAAADSVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCBE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLEHI" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCBK" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAADSVL" varchar(10) NOT NULL default ' ',
  "AAAABCBN" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAADSVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCBQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGFCP" varchar(32) NOT NULL default ' ',
  "AAABJCRH" varchar(1) NOT NULL default ' ',
  "AAAATPFI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGFCP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCBX" (
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABLMLR" varchar(4) NOT NULL default '0000',
  "AAABHMMT" varchar(30) NOT NULL default ' ',
  "AAAABCCB" varchar(30) NOT NULL default ' ',
  "AAABMJWH" varchar(30) NOT NULL default ' ',
  "AAAALNQW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABLMLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCCG" (
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAAADSVH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAAADSVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCEJ" (
  "AAAABCEJ" varchar(10) NOT NULL default ' ',
  "AAAABCCN" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAAABCEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCCS" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAABCEJ" varchar(10) NOT NULL default ' ',
  "AAAABCCS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAABCEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCDH" (
  "AAAABCDH" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAABCDH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCEM" (
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAAABCDG" varchar(7) NOT NULL default ' ',
  "AAAABCDH" varchar(20) NOT NULL default ' ',
  "AAAABCDI" varchar(30) NOT NULL default ' ',
  "AAAABCEO" varchar(40) NOT NULL default ' ',
  "AAAABCDE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAAABCDG","AAAABCDH","AAAABCDI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCDJ" (
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABMBXY" varchar(50) NOT NULL default ' ',
  "AAABLTWP" varchar(20) NOT NULL default ' ',
  "AAABHAGF" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMKFH","AAABMBXY"),
  KEY "AAAABCDQ" ("AAABLTWP","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCDT" (
  "AAABLTWR" varchar(20) NOT NULL default ' ',
  "AAABLTWP" varchar(20) NOT NULL default ' ',
  "AAABLTWQ" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLTWR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCDY" (
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAAABCEC" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAAABCEC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAVSPS" (
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABHJRW" varchar(2) NOT NULL default ' ',
  "AAAABCEG" varchar(40) NOT NULL default ' ',
  "AAAABCEH" varchar(40) NOT NULL default ' ',
  "AAAABCEI" varchar(40) NOT NULL default ' ',
  "AAAABCEJ" varchar(10) NOT NULL default ' ',
  "AAAABCEK" varchar(40) NOT NULL default ' ',
  "AAAABCEL" varchar(40) NOT NULL default ' ',
  "AAAABCEM" varchar(1) NOT NULL default ' ',
  "AAAABCEN" varchar(1) NOT NULL default ' ',
  "AAAABCEO" varchar(40) NOT NULL default ' ',
  "AAAABCEP" varchar(30) NOT NULL default ' ',
  "AAAABCEQ" varchar(60) NOT NULL default ' ',
  "AAAABCER" varchar(60) NOT NULL default ' ',
  "AAAABCES" varchar(60) NOT NULL default ' ',
  "AAAABCET" varchar(1) NOT NULL default ' ',
  "AAAABCEU" varchar(40) NOT NULL default ' ',
  "AAAABCEV" varchar(40) NOT NULL default ' ',
  "AAAABCEW" varchar(60) NOT NULL default ' ',
  "AAAABCEX" varchar(60) NOT NULL default ' ',
  "AAAABCEY" varchar(1) NOT NULL default ' ',
  "AAAABCEZ" varchar(1) NOT NULL default ' ',
  "AAAAREWR" varchar(20) NOT NULL default ' ',
  "AAAABCFB" varchar(40) NOT NULL default ' ',
  "AAAABCFC" varchar(40) NOT NULL default ' ',
  "AAAABCFD" varchar(1) NOT NULL default ' ',
  "AAAABCFE" varchar(1) NOT NULL default ' ',
  "AAAABCFF" varchar(1) NOT NULL default ' ',
  "AAAABCFG" varchar(60) NOT NULL default ' ',
  "AAAABCFH" varchar(1) NOT NULL default ' ',
  "AAABKWWJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCFK" (
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAAADSVL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAAADSVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCFP" (
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABGNEW" varchar(4) NOT NULL default ' ',
  "AAAABCFS" double NOT NULL default '0',
  PRIMARY KEY ("AAABMKFH","AAABGNEW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCFV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEGBX" varchar(1) NOT NULL default ' ',
  "AAAABCFY" varchar(1) NOT NULL default ' ',
  "AAAABCFZ" varchar(1) NOT NULL default ' ',
  "AAAABCGA" varchar(1) NOT NULL default ' ',
  "AAAABCGB" varchar(1) NOT NULL default ' ',
  "AAAABCGC" varchar(1) NOT NULL default ' ',
  "AAAABCGD" varchar(1) NOT NULL default ' ',
  "AAAABCGE" decimal(4,1) NOT NULL default '0.0',
  "AAAABCGF" decimal(6,1) NOT NULL default '0.0',
  "AAABCRWX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCGI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAAAURUV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKFH","AAABHJPB","AAABMOIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCGS" (
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAAAGGJS" varchar(40) NOT NULL default ' ',
  "AAAAURUV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAAAGGJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCGY" (
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABLITM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABLITM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCHD" (
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCHI" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAAAGKGO" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCHO" (
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABLEJW" varchar(60) NOT NULL default ' ',
  "AAAABCHR" varchar(10) NOT NULL default ' ',
  "AAAAREWX" varchar(1) NOT NULL default ' ',
  "AAAAREWU" int(11) NOT NULL default '0',
  "AAAAREWV" int(11) NOT NULL default '0',
  "AAAAREWT" varchar(1) NOT NULL default ' ',
  "AAAABCHW" int(11) NOT NULL default '0',
  "AAAABCHX" varchar(10) NOT NULL default ' ',
  "AAAABCHY" varchar(32) NOT NULL default ' ',
  "AAAABCHZ" varchar(1) NOT NULL default ' ',
  "AAAABCIA" varchar(14) NOT NULL default ' ',
  "AAAABCIB" varchar(14) NOT NULL default ' ',
  "AAAABCIC" varchar(1) NOT NULL default ' ',
  "AAAABCID" varchar(1) NOT NULL default ' ',
  "AAAABCIE" varchar(14) NOT NULL default ' ',
  "AAAAREWW" varchar(30) NOT NULL default ' ',
  "AAAAREWY" varchar(1) NOT NULL default ' ',
  "AAAAREWZ" varchar(1) NOT NULL default ' ',
  "AAAAREXA" varchar(32) NOT NULL default ' ',
  "AAAAREXB" varchar(64) NOT NULL default ' ',
  "AAAAVILO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCIM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLEJW" varchar(60) NOT NULL default ' ',
  "AAAABCIP" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLEJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCIS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDXM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDXM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCIX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABLTWP" varchar(20) NOT NULL default ' ',
  "AAABIHTL" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMQIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCJE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDXM" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(20) NOT NULL default ' ',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDXM","AAABMMJT","AAABMQIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABLTWP" varchar(20) NOT NULL default ' ',
  "AAABIHTL" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMQIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCJU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAAABCKF" varchar(10) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAAABCKH" varchar(1) NOT NULL default '0',
  "AAAABCKA" varchar(3) NOT NULL default '000',
  "AAAABCKB" varchar(1) NOT NULL default ' ',
  "AAAABCKC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTXG","AAAABCKF","AAABMCRJ","AAAABCKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCKI" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQCL" varchar(1) NOT NULL default ' ',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQCV" varchar(10) NOT NULL default ' ',
  "AAABMQCW" varchar(4) NOT NULL default '0000',
  "AAABMQCX" varchar(1) NOT NULL default ' ',
  "AAABLTWP" varchar(20) NOT NULL default ' ',
  "AAAACVWL" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMQHU","AAABMQCJ","AAABMQHY","AAABMQCL","AAABMQDO","AAABMQHV","AAABMQCV","AAABMQCW","AAABMQCX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCLD" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQCL" varchar(1) NOT NULL default ' ',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQCV" varchar(10) NOT NULL default ' ',
  "AAABMQCW" varchar(4) NOT NULL default '0000',
  "AAABMQCX" varchar(1) NOT NULL default ' ',
  "AAABLTWP" varchar(20) NOT NULL default ' ',
  "AAAACVWL" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMQHU","AAABMQCJ","AAABMQHY","AAABMQCL","AAABMQDO","AAABMQHV","AAABMQCV","AAABMQCW","AAABMQCX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCLY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJLTX" varchar(20) NOT NULL default ' ',
  "AAAABCMH" varchar(8) NOT NULL default '00000000',
  "AAABKRJY" varchar(24) NOT NULL default ' ',
  "AAABLTWP" varchar(20) NOT NULL default ' ',
  "AAABLTWQ" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABJLTX","AAAABCMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCMI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKJVG" varchar(10) NOT NULL default ' ',
  "AAABMQCE" varchar(10) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCMR" varchar(2) NOT NULL default '00',
  "AAABMCNN" varchar(4) NOT NULL default ' ',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABLTWP" varchar(20) NOT NULL default ' ',
  "AAAACVWL" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKJVG","AAABMQCE","AAABMREX","AAABMDBB","AAABMCMR","AAABMCNN","AAABMQCD","AAABMQBT","AAABMPWN","AAABMQCA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCNH" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQCL" varchar(1) NOT NULL default ' ',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAAAVUNB" varchar(4) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQCV" varchar(10) NOT NULL default ' ',
  "AAABMQCW" varchar(4) NOT NULL default '0000',
  "AAABMQCX" varchar(1) NOT NULL default ' ',
  "AAABLTWP" varchar(20) NOT NULL default ' ',
  "AAAACVWL" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMQHU","AAABMQCJ","AAABMQHY","AAABMQCL","AAABMQDO","AAABETQU","AAAAVUNB","AAABMQHV","AAABMQCV","AAABMQCW","AAABMQCX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCOG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABLXII" varchar(10) NOT NULL default ' ',
  "AAAABCOQ" varchar(2) NOT NULL default ' ',
  "AAABLTWP" varchar(20) NOT NULL default ' ',
  "AAAACVWL" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMDBB","AAABLXII","AAAABCOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCOR" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQCL" varchar(1) NOT NULL default ' ',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAAAVUNB" varchar(4) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQCV" varchar(10) NOT NULL default ' ',
  "AAABMQCW" varchar(4) NOT NULL default '0000',
  "AAABMQCX" varchar(1) NOT NULL default ' ',
  "AAABLTWP" varchar(20) NOT NULL default ' ',
  "AAAACVWL" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMQHU","AAABMQCJ","AAABMQHY","AAABMQCL","AAABMQDO","AAABETQU","AAAAVUNB","AAABMQHV","AAABMQCV","AAABMQCW","AAABMQCX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCPQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATUZM" varchar(5) NOT NULL default ' ',
  "AAABLTIZ" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAAATUZM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCPW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAATUZM" varchar(5) NOT NULL default ' ',
  "AAAABCQA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAATUZM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCQE" (
  "AAAABCQR" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAABCSP" varchar(30) NOT NULL default ' ',
  "AAAABCQQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAABCQR","AAABMREL","AAAABCSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCQM" (
  "AAAABCQR" varchar(1) NOT NULL default ' ',
  "AAAABCSP" varchar(24) NOT NULL default ' ',
  "AAABMREL" varchar(16) NOT NULL default ' ',
  "AAAABCQQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAABCQR","AAAABCSP","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCQU" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAKHWM" varchar(24) NOT NULL default ' ',
  "AAABMREL" varchar(16) NOT NULL default ' ',
  "AAAABCQY" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAAKHWM","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCRC" (
  "AAAABCRU" VARBINARY(255) NOT NULL,
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAABCSP" varchar(30) NOT NULL default ' ',
  "AAAABCRR" int(11) NOT NULL default '0',
  "AAABKWFM" varchar(8) NOT NULL default '00000000',
  "AAAABCRI" varchar(8) NOT NULL default '00000000',
  "AAABLTLM" varchar(6) NOT NULL default '000000',
  "AAAABCRK" varchar(6) NOT NULL default '000000',
  "AAAABCRL" varchar(12) NOT NULL default ' ',
  "AAAABCRM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAABCRU"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCRO" (
  "AAAARKNI" VARBINARY(255) NOT NULL,
  "AAAABCRU" VARBINARY(255),
  "AAAABCRR" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAARKNI"(255)),
  KEY "AAAABCRT" ("AAAABCRU"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCRV" (
  "AAAARKNI" VARBINARY(255) NOT NULL,
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEFTK" varchar(132) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARKNI"(255),"AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCSC" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAABCSP" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLRJQ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMREL","AAAABCSP","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCSK" (
  "AAAABCSP" varchar(24) NOT NULL default ' ',
  "AAABMREL" varchar(16) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLRJQ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAAABCSP","AAABMREL","AAABMREF"),
  KEY "AAAABCSS" ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCSU" (
  "AAAAKHWM" varchar(24) NOT NULL default ' ',
  "AAABMREL" varchar(16) NOT NULL default ' ',
  "AAAABCSX" varchar(30) NOT NULL default ' ',
  "AAAABCSY" varchar(6) NOT NULL default '000000',
  "AAAABCSZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKHWM","AAABMREL"),
  KEY "AAAABCTC" ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCTE" (
  "AAAAKHWM" varchar(24) NOT NULL default ' ',
  "AAABMREL" varchar(16) NOT NULL default ' ',
  "AAAABCTK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKHWM","AAABMREL","AAAABCTK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCTL" (
  "AAAAKHWM" varchar(24) NOT NULL default ' ',
  "AAABMREL" varchar(16) NOT NULL default ' ',
  "AAAABCTT" varchar(30) NOT NULL default ' ',
  "AAAABCTP" varchar(6) NOT NULL default '000000',
  "AAAABCTQ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAAKHWM","AAABMREL","AAAABCTT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCTU" (
  "AAABGSIS" varchar(8) NOT NULL default ' ',
  "AAABGSIT" varchar(4) NOT NULL default ' ',
  "AAABGSIU" varchar(8) NOT NULL default ' ',
  "AAABGSIV" varchar(4) NOT NULL default ' ',
  "AAABGSIW" varchar(32) NOT NULL default ' ',
  "AAABGSIX" varchar(8) NOT NULL default ' ',
  "AAABGSCC" VARBINARY(255) NOT NULL,
  "AAABGSBP" VARBINARY(255),
  "AAABGSBQ" VARBINARY(255),
  "AAABGSBR" VARBINARY(255),
  "AAABGSBS" VARBINARY(255),
  "AAABGSBT" VARBINARY(255),
  "AAABGSBU" VARBINARY(255),
  "AAABGSBV" VARBINARY(255),
  PRIMARY KEY ("AAABGSIS","AAABGSIT","AAABGSIU","AAABGSIV","AAABGSIW","AAABGSIX","AAABGSCC"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCUQ" (
  "AAABGSIS" varchar(8) NOT NULL default ' ',
  "AAABGSIT" varchar(4) NOT NULL default ' ',
  "AAABGSIU" varchar(8) NOT NULL default ' ',
  "AAABGSIV" varchar(4) NOT NULL default ' ',
  "AAABGSIW" varchar(32) NOT NULL default ' ',
  "AAABGSIX" varchar(8) NOT NULL default ' ',
  "AAABGSJE" varchar(8) NOT NULL default ' ',
  "AAABGSJC" varchar(30) NOT NULL default ' ',
  "AAABGSIJ" varchar(1) NOT NULL default ' ',
  "AAABGSIK" varchar(6) NOT NULL default '000000',
  "AAABGSJA" varchar(8) NOT NULL default '00000000',
  "AAABGSIZ" varchar(12) NOT NULL default ' ',
  "AAABGSIN" varchar(4) NOT NULL default '0000',
  "AAABGSIO" varchar(20) NOT NULL default ' ',
  "AAABGSIP" varchar(8) NOT NULL default ' ',
  "AAABGSIQ" varchar(50) NOT NULL default ' ',
  "AAABGSIR" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABGSIS","AAABGSIT","AAABGSIU","AAABGSIV","AAABGSIW","AAABGSIX"),
  KEY "AAAABCVO" ("AAABGSJE","AAABGSJA","AAABGSIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCVS" (
  "AAABGSIS" varchar(8) NOT NULL default ' ',
  "AAABGSIT" varchar(4) NOT NULL default ' ',
  "AAABGSIU" varchar(8) NOT NULL default ' ',
  "AAABGSIV" varchar(4) NOT NULL default ' ',
  "AAABGSIW" varchar(32) NOT NULL default ' ',
  "AAABGSIX" varchar(8) NOT NULL default ' ',
  "AAABGSCC" VARBINARY(255) NOT NULL,
  "AAABGSBP" VARBINARY(255),
  "AAABGSBQ" VARBINARY(255),
  "AAABGSBR" VARBINARY(255),
  "AAABGSBS" VARBINARY(255),
  "AAABGSBT" VARBINARY(255),
  "AAABGSBU" VARBINARY(255),
  "AAABGSBV" VARBINARY(255),
  PRIMARY KEY ("AAABGSIS","AAABGSIT","AAABGSIU","AAABGSIV","AAABGSIW","AAABGSIX","AAABGSCC"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCWO" (
  "AAABGSIS" varchar(8) NOT NULL default ' ',
  "AAABGSIT" varchar(4) NOT NULL default ' ',
  "AAABGSIU" varchar(8) NOT NULL default ' ',
  "AAABGSIV" varchar(4) NOT NULL default ' ',
  "AAABGSIW" varchar(32) NOT NULL default ' ',
  "AAABGSIX" varchar(8) NOT NULL default ' ',
  "AAABGSJE" varchar(8) NOT NULL default ' ',
  "AAABGSJC" varchar(30) NOT NULL default ' ',
  "AAABGSIJ" varchar(1) NOT NULL default ' ',
  "AAABGSIK" varchar(6) NOT NULL default '000000',
  "AAABGSJA" varchar(8) NOT NULL default '00000000',
  "AAABGSIZ" varchar(12) NOT NULL default ' ',
  "AAABGSIN" varchar(4) NOT NULL default '0000',
  "AAABGSIO" varchar(20) NOT NULL default ' ',
  "AAABGSIP" varchar(8) NOT NULL default ' ',
  "AAABGSIQ" varchar(50) NOT NULL default ' ',
  "AAABGSIR" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABGSIS","AAABGSIT","AAABGSIU","AAABGSIV","AAABGSIW","AAABGSIX"),
  KEY "AAAABCXM" ("AAABGSJE","AAABGSIZ"),
  KEY "AAAABCXP" ("AAABGSIZ","AAABGSJA","AAABGSIJ"),
  KEY "AAAABCXT" ("AAABGSJC"),
  KEY "AAAABCXV" ("AAABGSIW","AAABGSJE","AAABGSIJ"),
  KEY "AAAABCXZ" ("AAABGSJA","AAABGSIK","AAABGSIN","AAABGSIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCYE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABKJVG" varchar(10) NOT NULL default ' ',
  "AAABMQCE" varchar(10) NOT NULL default ' ',
  "AAABMCOP" varchar(10) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAAAWCJG" varchar(20) NOT NULL default ' ',
  "AAABIHTL" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBA","AAABMDBB"),
  UNIQUE KEY "AAAABCYV" ("AAABMRIL","AAAAWCJG","AAABIHTL"),
  KEY "AAAABCYZ" ("AAABMRIL","AAABMREX","AAABMQBT"),
  KEY "AAAABCZD" ("AAABMRIL","AAABKJVG","AAABMQCE","AAABMQCD","AAABMCOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABCZJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAAAWCJG" varchar(20) NOT NULL default ' ',
  "AAABIHTL" int(11) NOT NULL default '0',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCPM" varchar(8) NOT NULL default '00000000',
  "AAABMCPO" varchar(10) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMPLT","AAABMCMO","AAABMQBT","AAAAWCJG","AAABIHTL"),
  KEY "AAAABDAC" ("AAABMRIL","AAAAWCJG","AAABIHTL"),
  KEY "AAAABDAG" ("AAABMRIL","AAABMREX","AAABMPLT","AAABMCPM","AAABMCPO","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAAAWCJG" varchar(20) NOT NULL default ' ',
  "AAABIHTL" int(11) NOT NULL default '0',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCPM" varchar(8) NOT NULL default '00000000',
  "AAABMCPO" varchar(10) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCSR","AAABMCMO","AAABMQBT","AAAAWCJG","AAABIHTL"),
  KEY "AAAABDBG" ("AAABMRIL","AAAAWCJG","AAABIHTL"),
  KEY "AAAABDBK" ("AAABMRIL","AAABMREX","AAABMCSR","AAABMCPM","AAABMCPO","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDBR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAAAWCJG" varchar(20) NOT NULL default ' ',
  "AAABIHTL" int(11) NOT NULL default '0',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCPM" varchar(8) NOT NULL default '00000000',
  "AAABMCPO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMOAH","AAABMCMO","AAABMQBT","AAAAWCJG","AAABIHTL"),
  KEY "AAAABDCJ" ("AAABMRIL","AAAAWCJG","AAABIHTL"),
  KEY "AAAABDCN" ("AAABMRIL","AAABMREX","AAABMOAH","AAABMCPM","AAABMCPO","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDCU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAAAWCJG" varchar(20) NOT NULL default ' ',
  "AAABIHTL" int(11) NOT NULL default '0',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMOAH","AAABMCMO","AAABMQBT","AAAAWCJG","AAABIHTL"),
  KEY "AAAABDDK" ("AAABMRIL","AAAAWCJG","AAABIHTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDDO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAAABDDS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHJPB","AAABMOIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDDW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABJTNV" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABJTNV","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDEH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAAABDEL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHJPB","AAABMOIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDEP" (
  "AAAABDER" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAABDER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDES" (
  "AAABGCZX" varchar(20) NOT NULL default ' ',
  "AAAABDEU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABGCZX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDEW" (
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDEZ" (
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABIJYD" varchar(30) NOT NULL default ' ',
  "AAAABDFC" varchar(30) NOT NULL default ' ',
  "AAAAVNHH" varchar(30) NOT NULL default ' ',
  "AAABLSDL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDFG" (
  "AAABLLJR" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLLJR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDFJ" (
  "AAAABDFM" varchar(15) NOT NULL default ' ',
  "AAAAREGT" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAAABDFM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDFN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLNWD" varchar(18) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABGATU" varchar(4) NOT NULL default ' ',
  "AAAABDFY" varchar(10) NOT NULL default '0000000000',
  "AAABLXSK" varchar(18) NOT NULL default ' ',
  "AAABIVCC" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAABGATM" varchar(2) NOT NULL default ' ',
  "AAABGATN" varchar(8) NOT NULL default '00000000',
  "AAABLHRD" varchar(4) NOT NULL default ' ',
  "AAAABDGW" varchar(18) NOT NULL default ' ',
  "AAABFWNG" varchar(1) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAYXMN" varchar(3) NOT NULL default ' ',
  "AAAAGYQO" varchar(4) NOT NULL default '0000',
  "AAAAGWGW" varchar(18) NOT NULL default ' ',
  "AAABGAHN" varchar(1) NOT NULL default ' ',
  "AAAABDGN" decimal(5,0) NOT NULL default '0',
  "AAAABDGO" decimal(5,0) NOT NULL default '0',
  "AAAAGWET" varchar(3) NOT NULL default ' ',
  "AAAAWMBD" varchar(10) NOT NULL default ' ',
  "AAAAGWFP" varchar(1) NOT NULL default ' ',
  "AAAABDGS" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLNWD"),
  KEY "AAAABDGV" ("AAAABDGW","AAABHVYL","AAABLNWD"),
  KEY "AAAABDGZ" ("AAAAYXMN","AAAAGYQO","AAAAGWGW","AAABLNWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLNWD" varchar(18) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAABDHI" varchar(40) NOT NULL default ' ',
  "AAABLJYA" varchar(1) NOT NULL default ' ',
  "AAAAWSDB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLNWD","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDHO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLCR" varchar(12) NOT NULL default '000000000000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABBFVS" varchar(2) NOT NULL default ' ',
  "AAAAVZAM" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABGLCR"),
  UNIQUE KEY "AAAABDIB" ("AAABMRIL","AAABMCRV","AAABJFGL","AAABJFHB","AAABBFVS","AAAAVZAM","AAABKFCZ","AAABKFCV","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDIL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLCR" varchar(12) NOT NULL default '000000000000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMCXI" varchar(12) NOT NULL default ' ',
  "AAABMLTD" varchar(8) NOT NULL default '00000000',
  "AAABMCXJ" varchar(12) NOT NULL default ' ',
  "AAABLKML" varchar(4) NOT NULL default ' ',
  "AAABKROQ" varchar(10) NOT NULL default ' ',
  "AAABKROR" varchar(6) NOT NULL default '000000',
  "AAABMCXN" varchar(10) NOT NULL default '0000000000',
  "AAABKPUB" varchar(12) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABDCOH" varchar(18) NOT NULL default ' ',
  "AAABGLDX" varchar(6) NOT NULL default ' ',
  "AAABGLEH" varchar(4) NOT NULL default ' ',
  "AAABGLEI" varchar(4) NOT NULL default ' ',
  "AAABGLEG" varchar(10) NOT NULL default ' ',
  "AAABKPUA" varchar(6) NOT NULL default ' ',
  "AAAAXZWS" varchar(18) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABBHLV" varchar(8) NOT NULL default '00000000',
  "AAAAXZWW" decimal(7,2) NOT NULL default '0.00',
  "AAABGLEK" varchar(2) NOT NULL default ' ',
  "AAABGLEL" varchar(2) NOT NULL default ' ',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAABMCXD" varchar(15) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABMCZQ" varchar(16) NOT NULL default ' ',
  "AAABKRUO" varchar(20) NOT NULL default ' ',
  "AAAAXZXE" varchar(10) NOT NULL default ' ',
  "AAAAXZXF" varchar(16) NOT NULL default ' ',
  "AAAAXZXG" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGLCR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDKE" (
  "AAAABJHF" varchar(25) NOT NULL default ' ',
  "AAABHECV" varchar(20) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAABJHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDKK" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAABJHF" varchar(25) NOT NULL default ' ',
  "AAABMRCK" varchar(61) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAABJHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDKQ" (
  "AAAABDLG" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAABDLG","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDKW" (
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDLA" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAABDLG" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAABDLG","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDLI" (
  "AAAABDLP" varchar(3) NOT NULL default '000',
  "AAABKXFW" varchar(3) NOT NULL default '000',
  "AAABKXKU" varchar(2) NOT NULL default '00',
  "AAABMDER" varchar(4) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAAABDLO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAABDLP","AAABKXFW","AAABKXKU","AAABMDER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDLT" (
  "AAABKWBB" varchar(2) NOT NULL default '00',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABKWBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDLX" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWBB" varchar(2) NOT NULL default '00',
  "AAAABDMA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABKWBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDMD" (
  "AAABLDAZ" varchar(3) NOT NULL default '000',
  "AAABKXFW" varchar(3) NOT NULL default '000',
  "AAABKXGX" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABLDAZ","AAABKXFW","AAABKXGX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDMK" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLDAZ" varchar(3) NOT NULL default '000',
  "AAABKXFW" varchar(3) NOT NULL default '000',
  "AAABKXGX" varchar(5) NOT NULL default ' ',
  "AAAABDMP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLDAZ","AAABKXFW","AAABKXGX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDMU" (
  "AAABLDAZ" varchar(3) NOT NULL default '000',
  "AAABKXGX" varchar(5) NOT NULL default ' ',
  "AAABKXFW" varchar(3) NOT NULL default '000',
  "AAABKXKU" varchar(2) NOT NULL default '00',
  "AAABMRCU" varchar(2) NOT NULL default '00',
  "AAAABDNA" varchar(1) NOT NULL default ' ',
  "AAAABDNB" varchar(1) NOT NULL default ' ',
  "AAAABDNC" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLDAZ","AAABKXGX","AAABKXFW","AAABKXKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDNH" (
  "AAABKXMV" varchar(1) NOT NULL default ' ',
  "AAABKZVC" varchar(2) NOT NULL default ' ',
  "AAAABDNK" varchar(1) NOT NULL default ' ',
  "AAAABDNL" varchar(1) NOT NULL default ' ',
  "AAABMEBC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABKXMV","AAABKZVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDNP" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKXMV" varchar(1) NOT NULL default ' ',
  "AAABKZVC" varchar(2) NOT NULL default ' ',
  "AAAABDNT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABKXMV","AAABKZVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDNX" (
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAAABDOC" varchar(1) NOT NULL default ' ',
  "AAAABDOD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLCPX","AAAABDOC","AAAABDOD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDOE" (
  "AAAABDOM" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAAABDOM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDOH" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAABDOM" varchar(2) NOT NULL default '00',
  "AAAABDOK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAABDOM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDON" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXJ" varchar(3) NOT NULL default ' ',
  "AAABKZWO" varchar(3) NOT NULL default ' ',
  "AAABKXFW" varchar(3) NOT NULL default '000',
  "AAABIEWH" varchar(2) NOT NULL default ' ',
  "AAAABDOT" varchar(2) NOT NULL default ' ',
  "AAAABDOU" varchar(2) NOT NULL default ' ',
  "AAABKXGX" varchar(5) NOT NULL default ' ',
  "AAAABDOW" varchar(1) NOT NULL default ' ',
  "AAAABDOX" varchar(2) NOT NULL default ' ',
  "AAABKUUW" varchar(3) NOT NULL default ' ',
  "AAABLCYD" varchar(1) NOT NULL default ' ',
  "AAAABDPA" tinyint(3) unsigned NOT NULL default '0',
  "AAAABDPB" varchar(1) NOT NULL default ' ',
  "AAAABDPC" varchar(1) NOT NULL default ' ',
  "AAAABDPD" varchar(2) NOT NULL default '00',
  "AAAABDPE" varchar(4) NOT NULL default ' ',
  "AAAABDPF" varchar(4) NOT NULL default ' ',
  "AAAABDPG" varchar(4) NOT NULL default ' ',
  "AAAABDPH" varchar(4) NOT NULL default ' ',
  "AAAABDPI" varchar(4) NOT NULL default ' ',
  "AAAABDPJ" varchar(4) NOT NULL default ' ',
  "AAAABDPK" varchar(4) NOT NULL default ' ',
  "AAAABDPL" varchar(4) NOT NULL default ' ',
  "AAAABDPM" varchar(4) NOT NULL default ' ',
  "AAAABDPN" varchar(4) NOT NULL default ' ',
  "AAAABDPO" varchar(4) NOT NULL default ' ',
  "AAAABDPP" varchar(4) NOT NULL default ' ',
  "AAAABDPQ" varchar(4) NOT NULL default ' ',
  "AAAABDPR" varchar(4) NOT NULL default ' ',
  "AAAABDPS" varchar(4) NOT NULL default ' ',
  "AAAABDPT" varchar(4) NOT NULL default ' ',
  "AAAABDPU" varchar(4) NOT NULL default ' ',
  "AAAABDPV" varchar(4) NOT NULL default ' ',
  "AAAABDPW" varchar(4) NOT NULL default ' ',
  "AAAABDPX" varchar(4) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABAXGF" varchar(10) NOT NULL default ' ',
  "AAAABDQB" varchar(4) NOT NULL default ' ',
  "AAAABDQC" varchar(4) NOT NULL default ' ',
  "AAAABDQD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYXJ","AAABKZWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDQH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKYXJ" varchar(3) NOT NULL default ' ',
  "AAABKZWO" varchar(3) NOT NULL default ' ',
  "AAABIFJD" varchar(2) NOT NULL default ' ',
  "AAABIFIX" varchar(10) NOT NULL default ' ',
  "AAAABDQO" varchar(4) NOT NULL default '0000',
  "AAAABDQP" varchar(1) NOT NULL default ' ',
  "AAAABDQQ" varchar(4) NOT NULL default ' ',
  "AAAABDQR" varchar(4) NOT NULL default ' ',
  "AAAABDQS" varchar(4) NOT NULL default ' ',
  "AAAABDQT" varchar(4) NOT NULL default ' ',
  "AAAABDQU" varchar(1) NOT NULL default ' ',
  "AAAABDQV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKYXJ","AAABKZWO","AAABIFJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDRB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXJ" varchar(3) NOT NULL default ' ',
  "AAABKZWO" varchar(3) NOT NULL default ' ',
  "AAAABDRF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYXJ","AAABKZWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDRJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKYXJ" varchar(3) NOT NULL default ' ',
  "AAABKZWO" varchar(3) NOT NULL default ' ',
  "AAABJQTW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKYXJ","AAABKZWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDRT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXJ" varchar(3) NOT NULL default ' ',
  "AAABKZWO" varchar(3) NOT NULL default ' ',
  "AAAABFMA" varchar(4) NOT NULL default ' ',
  "AAAABDRY" varchar(4) NOT NULL default ' ',
  "AAAABDRZ" varchar(10) NOT NULL default ' ',
  "AAAABDSA" varchar(1) NOT NULL default ' ',
  "AAAABDSB" varchar(3) NOT NULL default ' ',
  "AAAABDSC" varchar(3) NOT NULL default ' ',
  "AAAABDSD" varchar(3) NOT NULL default ' ',
  "AAAABDSE" varchar(3) NOT NULL default ' ',
  "AAAABDSF" varchar(1) NOT NULL default ' ',
  "AAAABDSG" varchar(1) NOT NULL default ' ',
  "AAAABDSH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYXJ","AAABKZWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDSL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXJ" varchar(3) NOT NULL default ' ',
  "AAABKZWO" varchar(3) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  "AAABKXPA" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKYXJ","AAABKZWO","AAABLCPX","AAABLDBT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDSX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXJ" varchar(3) NOT NULL default ' ',
  "AAABKZWO" varchar(3) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABKXHP" varchar(4) NOT NULL default ' ',
  "AAABKXPA" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKYXJ","AAABKZWO","AAABLCPX","AAABKXHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDTJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXJ" varchar(3) NOT NULL default ' ',
  "AAABKZWO" varchar(3) NOT NULL default ' ',
  "AAAABDUB" varchar(2) NOT NULL default '00',
  "AAABKXQR" varchar(1) NOT NULL default ' ',
  "AAABKXHP" varchar(4) NOT NULL default ' ',
  "AAABKZWQ" varchar(1) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAAABDTS" varchar(3) NOT NULL default ' ',
  "AAAABDTT" varchar(3) NOT NULL default ' ',
  "AAAABDTU" varchar(4) NOT NULL default ' ',
  "AAAABDTV" varchar(1) NOT NULL default ' ',
  "AAAABDTW" varchar(4) NOT NULL default ' ',
  "AAAABDTX" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKYXJ","AAABKZWO","AAAABDUB","AAABKXQR","AAABKXHP","AAABKZWQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDUF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLBYG" varchar(8) NOT NULL default '00000000',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABKYSP" varchar(5) NOT NULL default ' ',
  "AAABKYSQ" varchar(5) NOT NULL default ' ',
  "AAAABDVA" varchar(6) NOT NULL default '000000',
  "AAABKXLO" decimal(9,5) NOT NULL default '0.00000',
  PRIMARY KEY ("AAABMRIL","AAABLBYG","AAABLXJP","AAABKYSP","AAABKYSQ","AAAABDVA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDUT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLBYG" varchar(8) NOT NULL default '00000000',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABKYSP" varchar(5) NOT NULL default ' ',
  "AAABKYSQ" varchar(5) NOT NULL default ' ',
  "AAAABDVH" varchar(3) NOT NULL default ' ',
  "AAAABDVA" varchar(6) NOT NULL default '000000',
  "AAABKXLO" decimal(9,5) NOT NULL default '0.00000',
  PRIMARY KEY ("AAABMRIL","AAABLBYG","AAABLXJP","AAABKYSP","AAABKYSQ","AAAABDVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDVI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXJ" varchar(3) NOT NULL default ' ',
  "AAABKZWO" varchar(3) NOT NULL default ' ',
  "AAABKXQF" varchar(5) NOT NULL default ' ',
  "AAABKXQG" varchar(5) NOT NULL default ' ',
  "AAABKXGD" varchar(10) NOT NULL default ' ',
  "AAABLOQJ" varchar(1) NOT NULL default ' ',
  "AAABMPSX" varchar(4) NOT NULL default ' ',
  "AAAABDVR" decimal(13,9) NOT NULL default '0.000000000',
  PRIMARY KEY ("AAABMRIL","AAABKYXJ","AAABKZWO","AAABKXQF","AAABKXQG","AAABKXGD","AAABLOQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDVZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKXGD" varchar(10) NOT NULL default ' ',
  "AAABKYXJ" varchar(3) NOT NULL default ' ',
  "AAABKZWO" varchar(3) NOT NULL default ' ',
  "AAAABDWF" varchar(4) NOT NULL default ' ',
  "AAAABDWG" varchar(10) NOT NULL default ' ',
  "AAAABDWH" varchar(3) NOT NULL default ' ',
  "AAAABDWI" varchar(3) NOT NULL default ' ',
  "AAAABDWJ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKXGD","AAABKYXJ","AAABKZWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDWP" (
  "AAAABECU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAABECU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDWS" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABMDER" varchar(1) NOT NULL default ' ',
  "AAABMDBL" varchar(1) NOT NULL default ' ',
  "AAABMNLX" varchar(2) NOT NULL default ' ',
  "AAABMNLY" varchar(4) NOT NULL default ' ',
  "AAABMDBN" varchar(4) NOT NULL default ' ',
  "AAABMDBO" varchar(4) NOT NULL default ' ',
  "AAABMDCO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDXD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABLDCD" varchar(4) NOT NULL default ' ',
  "AAABKXMV" varchar(1) NOT NULL default ' ',
  "AAABKZVC" varchar(2) NOT NULL default ' ',
  "AAAABDXJ" varchar(1) NOT NULL default ' ',
  "AAAABDXK" varchar(1) NOT NULL default ' ',
  "AAAABDXL" varchar(1) NOT NULL default ' ',
  "AAAABDXM" varchar(1) NOT NULL default ' ',
  "AAABIDQH" varchar(1) NOT NULL default ' ',
  "AAABIDQI" varchar(1) NOT NULL default ' ',
  "AAABIDQJ" varchar(4) NOT NULL default ' ',
  "AAABIDQK" varchar(4) NOT NULL default ' ',
  "AAABIDQL" varchar(4) NOT NULL default ' ',
  "AAABIDQM" varchar(4) NOT NULL default ' ',
  "AAAABDXT" varchar(4) NOT NULL default ' ',
  "AAAABDXU" varchar(1) NOT NULL default ' ',
  "AAAABDXV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCPX","AAABLDCD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDXZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  "AAABKXMV" varchar(1) NOT NULL default ' ',
  "AAABKZVC" varchar(2) NOT NULL default ' ',
  "AAABMDZM" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABLCPX","AAABLDBT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDYJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAQPVR" varchar(8) NOT NULL default '00000000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCXC" varchar(10) NOT NULL default ' ',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABKZWO" varchar(3) NOT NULL default ' ',
  "AAAABDZB" varchar(4) NOT NULL default ' ',
  "AAABKWVJ" varchar(10) NOT NULL default ' ',
  "AAAABEAI" varchar(2) NOT NULL default ' ',
  "AAAABEAJ" varchar(2) NOT NULL default ' ',
  "AAAABEAK" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAQPVR","AAABMREX","AAABMCXC","AAABLCAK","AAABKZWO","AAAABDZB","AAABKWVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDZD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKVYZ" varchar(5) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAAAJRHE" varchar(4) NOT NULL default ' ',
  "AAAAJRFL" varchar(1) NOT NULL default ' ',
  "AAAAJRHF" varchar(4) NOT NULL default ' ',
  "AAAAJRDX" varchar(1) NOT NULL default ' ',
  "AAAABDZX" varchar(8) NOT NULL default '00000000',
  "AAAABDZM" varchar(1) NOT NULL default ' ',
  "AAAABECK" decimal(10,7) NOT NULL default '0.0000000',
  "AAAABECN" varchar(10) NOT NULL default ' ',
  "AAAABDZP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKVYZ","AAABLCPX","AAAAJRHE","AAAAJRFL","AAAAJRHF","AAAAJRDX","AAAABDZX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABDZY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAQPVR" varchar(8) NOT NULL default '00000000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCXC" varchar(10) NOT NULL default ' ',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABKZWO" varchar(3) NOT NULL default ' ',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  "AAABKWVJ" varchar(10) NOT NULL default ' ',
  "AAAABEAV" varchar(8) NOT NULL default ' ',
  "AAAABEAI" varchar(2) NOT NULL default ' ',
  "AAAABEAJ" varchar(2) NOT NULL default ' ',
  "AAAABEAK" varchar(2) NOT NULL default ' ',
  "AAAABEAL" varchar(1) NOT NULL default ' ',
  "AAAABEAM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAQPVR","AAABMREX","AAABMCXC","AAABLCAK","AAABKZWO","AAABKZWL","AAABKWVJ","AAAABEAV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEAW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKVYZ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKVYZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEBB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKVYZ" varchar(5) NOT NULL default ' ',
  "AAAABEBF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKVYZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEBJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAABECN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAABECN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEBO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAABECN" varchar(10) NOT NULL default ' ',
  "AAAABEBS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAABECN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEBW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAABECN" varchar(10) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAABECA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAABECN","AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABECE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAABECN" varchar(10) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAABECP" decimal(13,2) NOT NULL default '0.00',
  "AAAABECJ" varchar(1) NOT NULL default ' ',
  "AAAABECK" decimal(10,7) NOT NULL default '0.0000000',
  "AAAABECL" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAABECN","AAABMPWL","AAAABECP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABECQ" (
  "AAAABECU" varchar(4) NOT NULL default ' ',
  "AAABMOCC" varchar(4) NOT NULL default ' ',
  "AAABJQHQ" decimal(10,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAAABECU","AAABMOCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABECW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABMEAB" varchar(1) NOT NULL default '0',
  "AAABMEAP" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLCPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKXQD" varchar(5) NOT NULL default ' ',
  "AAABKYXJ" varchar(3) NOT NULL default ' ',
  "AAABKXGD" varchar(10) NOT NULL default ' ',
  "AAABLAMO" varchar(3) NOT NULL default ' ',
  "AAABKXPO" varchar(5) NOT NULL default ' ',
  "AAABKXPN" varchar(5) NOT NULL default ' ',
  "AAAABEDN" varchar(30) NOT NULL default ' ',
  "AAAABEYR" varchar(3) NOT NULL default ' ',
  "AAAABEYS" varchar(3) NOT NULL default ' ',
  "AAABIEWH" varchar(2) NOT NULL default ' ',
  "AAAABEDR" varchar(1) NOT NULL default ' ',
  "AAABKXPQ" varchar(1) NOT NULL default ' ',
  "AAABKXQC" decimal(4,0) NOT NULL default '0',
  "AAAABEDU" varchar(1) NOT NULL default ' ',
  "AAABKXPX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKXQD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEDY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKXQD" varchar(5) NOT NULL default ' ',
  "AAAABEEC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKXQD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEEG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKXPO" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKXPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEEL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKXPO" varchar(5) NOT NULL default ' ',
  "AAAABEEP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKXPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEET" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKXPN" varchar(5) NOT NULL default ' ',
  "AAAABEEW" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKXPN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEEZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKXPN" varchar(5) NOT NULL default ' ',
  "AAAABEFD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKXPN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEFH" (
  "AAAABEFS" varchar(3) NOT NULL default '000',
  "AAABKVPA" varchar(10) NOT NULL default ' ',
  "AAAABEFK" varchar(4) NOT NULL default '0000',
  "AAABKKBY" int(11) NOT NULL default '0',
  "AAABMEAF" varchar(1) NOT NULL default '0',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAABMEFL" varchar(2) NOT NULL default ' ',
  "AAAABEFP" varchar(1) NOT NULL default '0',
  "AAAAKGDM" varchar(1) NOT NULL default ' ',
  "AAABKWRN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAABEFS","AAABKVPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEFU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKVTI" varchar(5) NOT NULL default ' ',
  "AAABKVTF" varchar(3) NOT NULL default ' ',
  "AAAABEFZ" varchar(1) NOT NULL default ' ',
  "AAABLTUX" varchar(4) NOT NULL default ' ',
  "AAAABEGB" decimal(4,0) NOT NULL default '0',
  "AAAABEGC" decimal(4,0) NOT NULL default '0',
  "AAAABEGD" decimal(13,2) NOT NULL default '0.00',
  "AAABCPCJ" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKVTI","AAABKVTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEGJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTUX" varchar(4) NOT NULL default ' ',
  "AAABKVQJ" varchar(2) NOT NULL default '00',
  "AAABIEWH" varchar(2) NOT NULL default ' ',
  "AAAAWFDU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEGR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLTUX" varchar(4) NOT NULL default ' ',
  "AAABKSZO" varchar(30) NOT NULL default ' ',
  "AAABIEYQ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLTUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEHA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKYXJ" varchar(3) NOT NULL default ' ',
  "AAABKZWO" varchar(3) NOT NULL default ' ',
  "AAABKXKU" varchar(2) NOT NULL default '00',
  "AAABKXKW" varchar(4) NOT NULL default ' ',
  "AAABKVXO" varchar(3) NOT NULL default ' ',
  "AAABMNDR" varchar(16) NOT NULL default ' ',
  "AAABGOWR" varchar(1) NOT NULL default ' ',
  "AAABKVXC" varchar(1) NOT NULL default ' ',
  "AAABMEEL" tinyint(3) unsigned NOT NULL default '0',
  "AAABMEEK" varchar(4) NOT NULL default ' ',
  "AAABIFKA" varchar(1) NOT NULL default ' ',
  "AAABIFKC" varchar(1) NOT NULL default ' ',
  "AAABLKWB" varchar(1) NOT NULL default ' ',
  "AAABMEEM" varchar(6) NOT NULL default ' ',
  "AAABMEEN" varchar(4) NOT NULL default ' ',
  "AAABMEEO" varchar(12) NOT NULL default ' ',
  "AAABMEEU" varchar(1) NOT NULL default ' ',
  "AAABMEEV" varchar(1) NOT NULL default ' ',
  "AAABLKWC" varchar(1) NOT NULL default '0',
  "AAABIFKL" varchar(3) NOT NULL default ' ',
  "AAABIFKM" varchar(8) NOT NULL default '00000000',
  "AAABIFKN" varchar(6) NOT NULL default '000000',
  "AAABIFKR" varchar(50) NOT NULL default ' ',
  "AAABMEEP" varchar(1) NOT NULL default ' ',
  "AAABMEES" varchar(68) NOT NULL default ' ',
  "AAABLKWD" varchar(1) NOT NULL default ' ',
  "AAAABEID" tinyint(3) unsigned NOT NULL default '0',
  "AAAABEIE" varchar(4) NOT NULL default ' ',
  "AAAABEIF" varchar(1) NOT NULL default ' ',
  "AAAABEIG" varchar(1) NOT NULL default ' ',
  "AAAABEIH" varchar(1) NOT NULL default ' ',
  "AAAABEII" varchar(6) NOT NULL default ' ',
  "AAAABEIJ" varchar(4) NOT NULL default ' ',
  "AAAABEIK" varchar(12) NOT NULL default ' ',
  "AAAABEIL" varchar(1) NOT NULL default ' ',
  "AAAABEIM" varchar(1) NOT NULL default ' ',
  "AAAABEIN" varchar(1) NOT NULL default '0',
  "AAAABEIO" varchar(3) NOT NULL default ' ',
  "AAAABEIP" varchar(8) NOT NULL default '00000000',
  "AAAABEIQ" varchar(6) NOT NULL default '000000',
  "AAAABEIR" varchar(50) NOT NULL default ' ',
  "AAAABEIS" varchar(1) NOT NULL default ' ',
  "AAAABEIT" varchar(68) NOT NULL default ' ',
  "AAAABEIU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKYXJ","AAABKZWO","AAABKXKU","AAABKXKW","AAABKVXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEJC" (
  "AAAABEJF" varchar(4) NOT NULL default ' ',
  "AAAABEJE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAABEJF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEJG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKVXO" varchar(3) NOT NULL default ' ',
  "AAABLXFQ" varchar(1) NOT NULL default ' ',
  "AAABLCCR" varchar(6) NOT NULL default ' ',
  "AAAAWYIS" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAABEJN" varchar(1) NOT NULL default ' ',
  "AAAAQKZO" varchar(10) NOT NULL default ' ',
  "AAABGXGE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKVXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEJS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAABEJX" varchar(11) NOT NULL default ' ',
  "AAABLDBD" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAABEJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEJY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKVXO" varchar(3) NOT NULL default ' ',
  "AAAABEKC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKVXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXJ" varchar(3) NOT NULL default ' ',
  "AAABKZWO" varchar(3) NOT NULL default ' ',
  "AAABIEXL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYXJ","AAABKZWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKTTZ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKTTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKTTZ" varchar(2) NOT NULL default ' ',
  "AAAABEKX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKTTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABELB" (
  "AAABMREL" varchar(10) NOT NULL default ' ',
  "AAABLUSR" varchar(50) NOT NULL default ' ',
  "AAABLUSO" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABMREL","AAABLUSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABELI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAABELR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABELO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAAABELR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABLCAK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABELU" (
  "AAABDVBY" varchar(10) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABDVBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABELY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGYJY" varchar(3) NOT NULL default ' ',
  "AAAABEMI" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABGYJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEME" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGYJY" varchar(3) NOT NULL default ' ',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAAABEMI" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABGYJY","AAABJGWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEMM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYWT" varchar(3) NOT NULL default ' ',
  "AAABKYSP" varchar(5) NOT NULL default ' ',
  "AAABKYSQ" varchar(5) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABKYWU" varchar(10) NOT NULL default '0000000000',
  "AAABKYUJ" varchar(8) NOT NULL default '00000000',
  "AAABKYWM" decimal(11,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAABKYWT","AAABKYSP","AAABKYSQ","AAABMNZR","AAABKYWU","AAABKYUJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABENC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGYJY" varchar(3) NOT NULL default ' ',
  "AAABGTBZ" varchar(3) NOT NULL default ' ',
  "AAAABENG" varchar(8) NOT NULL default ' ',
  "AAAABENH" varchar(8) NOT NULL default ' ',
  "AAAABENI" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGYJY","AAABGTBZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABENM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXGF" varchar(10) NOT NULL default ' ',
  "AAAABENP" decimal(2,0) NOT NULL default '0',
  "AAAABENQ" varchar(1) NOT NULL default ' ',
  "AAAABENR" decimal(2,0) NOT NULL default '0',
  "AAAABENS" varchar(1) NOT NULL default ' ',
  "AAAABENT" decimal(2,0) NOT NULL default '0',
  "AAAABENU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAXGF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABENX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXGF" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAXFY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAXGF","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEOF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABKXHP" varchar(4) NOT NULL default ' ',
  "AAABKZWQ" varchar(1) NOT NULL default ' ',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCPX","AAABKXHP","AAABKZWQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEOP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABKXHP" varchar(4) NOT NULL default ' ',
  "AAABKZWQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKZWL"),
  KEY "AAAABEOX" ("AAABMRIL","AAABLCPX","AAABKXHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEPB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYWT" varchar(3) NOT NULL default ' ',
  "AAABMDYY" varchar(10) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABKYWU" varchar(10) NOT NULL default '0000000000',
  "AAABKYUJ" varchar(8) NOT NULL default '00000000',
  "AAABKYWM" decimal(11,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAABKYWT","AAABMDYY","AAABMNZR","AAABKYWU","AAABKYUJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEPP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABETAB" varchar(3) NOT NULL default ' ',
  "AAAABFNK" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABETAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEPV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABETAB" varchar(3) NOT NULL default ' ',
  "AAABDXVI" varchar(30) NOT NULL default ' ',
  "AAABFCOF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABETAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEQE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKVAN" varchar(3) NOT NULL default ' ',
  "AAABKUXM" varchar(2) NOT NULL default '00',
  "AAAABEQI" varchar(1) NOT NULL default '0',
  "AAAABEQJ" varchar(1) NOT NULL default ' ',
  "AAAABEQK" varchar(1) NOT NULL default '0',
  "AAAABEQL" varchar(6) NOT NULL default ' ',
  "AAAALDFO" varchar(2) NOT NULL default '00',
  "AAAABEQN" varchar(1) NOT NULL default '0',
  "AAAABEQO" varchar(1) NOT NULL default ' ',
  "AAAABEQP" varchar(1) NOT NULL default ' ',
  "AAAABEQQ" varchar(1) NOT NULL default ' ',
  "AAAABEQR" varchar(1) NOT NULL default ' ',
  "AAAABEQS" varchar(3) NOT NULL default '000',
  "AAAABEQT" varchar(3) NOT NULL default '000',
  "AAABKUZG" varchar(5) NOT NULL default ' ',
  "AAAABEQV" varchar(1) NOT NULL default '0',
  "AAAALEFD" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKVAN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEQZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKVAN" varchar(3) NOT NULL default ' ',
  "AAABKUOX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKVAN","AAABKUOX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABERG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKVAN" varchar(3) NOT NULL default ' ',
  "AAABKUOX" varchar(30) NOT NULL default ' ',
  "AAAABEVM" varchar(20) NOT NULL default '00000000000000000000',
  "AAABMNMZ" varchar(32) NOT NULL default ' ',
  "AAABMNNA" varchar(32) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABDGCT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKVAN","AAABKUOX","AAAABEVM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABERT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKVAN" varchar(3) NOT NULL default ' ',
  "AAAABERX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKVAN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABESB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKUOX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKUOX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABESG" (
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLESS","AAABMGHE","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABESP" (
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLESS","AAABMGHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABESU" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLESS","AAABMGHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABETC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKUUW" varchar(3) NOT NULL default ' ',
  "AAABKRHS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKUUW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABETI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKUUW" varchar(3) NOT NULL default ' ',
  "AAAABETM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKUUW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABETQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAABETS" varchar(10) NOT NULL default ' ',
  "AAAAVBWM" varchar(3) NOT NULL default '000',
  "AAAABETU" varchar(3) NOT NULL default '000',
  "AAAABETV" varchar(6) NOT NULL default '000000',
  "AAABKURH" varchar(6) NOT NULL default '000000',
  "AAAABETX" varchar(1) NOT NULL default ' ',
  "AAAABETY" varchar(6) NOT NULL default '000000',
  "AAAAMCJN" varchar(2) NOT NULL default ' ',
  "AAAABEUA" varchar(1) NOT NULL default ' ',
  "AAAABEUB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEUD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFLY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEUH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKVAN" varchar(3) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABJTHM" varchar(1) NOT NULL default ' ',
  "AAAABEUP" varchar(1) NOT NULL default ' ',
  "AAAABEUQ" varchar(1) NOT NULL default ' ',
  "AAAABEUR" varchar(1) NOT NULL default ' ',
  "AAAABEUS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKVAN","AAABLESS","AAABMGHE","AAABMREL","AAABMREF","AAABJTHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEVA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAABEWB" varchar(20) NOT NULL default ' ',
  "AAABKUOX" varchar(30) NOT NULL default ' ',
  "AAAABEVM" varchar(4) NOT NULL default '0000',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABDGCT" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(32) NOT NULL default ' ',
  "AAABMNNA" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAABEWB","AAABKUOX","AAAABEVM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEVN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAABEWB" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAABEWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEVS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAABEWB" varchar(20) NOT NULL default ' ',
  "AAAALEEY" varchar(10) NOT NULL default ' ',
  "AAAALEEZ" varchar(20) NOT NULL default ' ',
  "AAABEGEF" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAABEWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEWC" (
  "AAABKUXN" varchar(1) NOT NULL default '0',
  "AAAABEWE" varchar(6) NOT NULL default '000000',
  "AAAABEWF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKUXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEWH" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKUXN" varchar(1) NOT NULL default '0',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABKUXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEWN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEWT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKVAW" varchar(3) NOT NULL default ' ',
  "AAABIEWH" varchar(2) NOT NULL default ' ',
  "AAABKWSY" varchar(1) NOT NULL default ' ',
  "AAABKVBF" varchar(1) NOT NULL default ' ',
  "AAABKVBG" varchar(1) NOT NULL default ' ',
  "AAABKWTB" varchar(1) NOT NULL default ' ',
  "AAABKVBJ" varchar(1) NOT NULL default ' ',
  "AAABKVBK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKVAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEXF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKVAW" varchar(3) NOT NULL default ' ',
  "AAAABEXJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKVAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEXN" (
  "AAABKXQA" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABKXQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEXQ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKXQA" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABKXQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEXW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKXLT" varchar(2) NOT NULL default ' ',
  "AAAABEXZ" varchar(1) NOT NULL default ' ',
  "AAAABEYA" varchar(1) NOT NULL default ' ',
  "AAAABEYB" varchar(1) NOT NULL default ' ',
  "AAAABEYC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKXLT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKXLT" varchar(2) NOT NULL default ' ',
  "AAAABEYJ" varchar(30) NOT NULL default ' ',
  "AAAABEYK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKXLT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEYO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXJ" varchar(3) NOT NULL default ' ',
  "AAAABEYR" varchar(3) NOT NULL default ' ',
  "AAAABEYS" varchar(3) NOT NULL default ' ',
  "AAABIEWH" varchar(2) NOT NULL default ' ',
  "AAABKXQC" decimal(4,0) NOT NULL default '0',
  "AAABKXQR" varchar(1) NOT NULL default ' ',
  "AAAABEYW" varchar(1) NOT NULL default '0',
  "AAABKXPQ" varchar(1) NOT NULL default ' ',
  "AAABKXPX" varchar(1) NOT NULL default ' ',
  "AAABLARY" varchar(1) NOT NULL default '0',
  "AAABKXQA" varchar(3) NOT NULL default '000',
  "AAAABEZB" varchar(1) NOT NULL default ' ',
  "AAAAKMYB" varchar(4) NOT NULL default '0000',
  "AAAAKMYC" varchar(4) NOT NULL default '0000',
  "AAAABEZE" varchar(2) NOT NULL default ' ',
  "AAAABEZF" varchar(1) NOT NULL default ' ',
  "AAABKXEC" varchar(2) NOT NULL default '00',
  "AAAABEZH" varchar(1) NOT NULL default ' ',
  "AAAABEZI" varchar(1) NOT NULL default ' ',
  "AAAABEZJ" varchar(2) NOT NULL default ' ',
  "AAABLACN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEZN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAABEZQ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABEZT" (
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABKZVC" varchar(2) NOT NULL default ' ',
  "AAAABFAA" varchar(1) NOT NULL default ' ',
  "AAABKZVA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLCPX","AAABKZVC","AAAABFAA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFAB" (
  "AAABKPIV" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABKPIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFAE" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKPIV" varchar(3) NOT NULL default ' ',
  "AAABCPGO" varchar(15) NOT NULL default ' ',
  "AAABKHGP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABKPIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFCA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABLATT" varchar(10) NOT NULL default ' ',
  "AAABLATF" varchar(2) NOT NULL default ' ',
  "AAABLATG" varchar(8) NOT NULL default '00000000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLASR" decimal(10,7) NOT NULL default '0.0000000',
  "AAABKYVW" decimal(15,6) NOT NULL default '0.000000',
  "AAABLASS" varchar(10) NOT NULL default ' ',
  "AAABLDCB" varchar(10) NOT NULL default ' ',
  "AAABLCIS" varchar(12) NOT NULL default ' ',
  "AAABLCIT" varchar(8) NOT NULL default '00000000',
  "AAABLCIU" varchar(6) NOT NULL default '000000',
  "AAABLASX" varchar(10) NOT NULL default ' ',
  "AAABLBRP" varchar(8) NOT NULL default '00000000',
  "AAABLBRQ" varchar(6) NOT NULL default '000000',
  "AAABLATA" varchar(6) NOT NULL default ' ',
  "AAABLATB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCPX","AAABMEFT","AAABLATT","AAABLATF","AAABLATG"),
  KEY "AAAABFBL" ("AAABLATG"),
  KEY "AAAABFBN" ("AAABMRIL","AAABMEFT","AAABLATG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFBR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKYXJ" varchar(3) NOT NULL default ' ',
  "AAABKZWO" varchar(3) NOT NULL default ' ',
  "AAABKVPZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKYXJ","AAABKZWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFCB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKYXJ" varchar(3) NOT NULL default ' ',
  "AAABKZWO" varchar(3) NOT NULL default ' ',
  "AAABKXKU" varchar(2) NOT NULL default '00',
  "AAABKVFD" varchar(2) NOT NULL default ' ',
  "AAABKVQA" varchar(1) NOT NULL default ' ',
  "AAAABFCJ" varchar(1) NOT NULL default ' ',
  "AAAABFCK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKYXJ","AAABKZWO","AAABKXKU","AAABKVFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFCR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCRI" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABLCRI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFCW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCRI" varchar(2) NOT NULL default '00',
  "AAABLCOG" varchar(1) NOT NULL default ' ',
  "AAABKTZA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCRI","AAABLCOG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFDE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKLSL" varchar(15) NOT NULL default ' ',
  "AAAAEKPN" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAAAKLSL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFDK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAEKPN" varchar(3) NOT NULL default '000',
  "AAABHIXN" varchar(32) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAAAWDEM" varchar(5) NOT NULL default '00000',
  "AAABBOPO" decimal(15,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAAAEKPN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFDT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAEKPN" varchar(3) NOT NULL default '000',
  "AAABFCOF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAEKPN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFEB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKLSL" varchar(15) NOT NULL default ' ',
  "AAAAKKNW" varchar(10) NOT NULL default ' ',
  "AAABKXPW" varchar(13) NOT NULL default ' ',
  "AAABLATT" varchar(10) NOT NULL default ' ',
  "AAABKYCB" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKLSL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFEK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKLSL" varchar(15) NOT NULL default ' ',
  "AAABETAB" varchar(3) NOT NULL default ' ',
  "AAAABFRY" varchar(15) NOT NULL default ' ',
  "AAAABFRZ" varchar(15) NOT NULL default ' ',
  "AAAABFSA" varchar(2) NOT NULL default '00',
  "AAAABFSB" varchar(15) NOT NULL default ' ',
  "AAAABFSC" varchar(15) NOT NULL default ' ',
  "AAAABFSD" varchar(2) NOT NULL default '00',
  "AAABKYWU" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAAAKLSL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFEX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAKLSL" varchar(15) NOT NULL default ' ',
  "AAABFCOF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAKLSL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFFF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYWT" varchar(3) NOT NULL default ' ',
  "AAAAKLSL" varchar(15) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABKYWU" varchar(10) NOT NULL default '0000000000',
  "AAAAWVAH" decimal(11,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAABKYWT","AAAAKLSL","AAABMNZR","AAABKYWU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFFR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKLSL" varchar(15) NOT NULL default ' ',
  "AAABKYWT" varchar(3) NOT NULL default ' ',
  "AAAABFFV" varchar(15) NOT NULL default ' ',
  "AAAABFFW" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKLSL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFFZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXH" varchar(8) NOT NULL default ' ',
  "AAABESZZ" varchar(4) NOT NULL default ' ',
  "AAAAKMXZ" varchar(4) NOT NULL default ' ',
  "AAAAKMYA" varchar(4) NOT NULL default ' ',
  "AAAAKMYB" varchar(4) NOT NULL default '0000',
  "AAAAKMYC" varchar(4) NOT NULL default '0000',
  "AAAAKMYD" varchar(2) NOT NULL default ' ',
  "AAAAMLNO" varchar(2) NOT NULL default ' ',
  "AAAAKMYF" varchar(3) NOT NULL default ' ',
  "AAAAKMYG" varchar(3) NOT NULL default ' ',
  "AAAAKMYH" varchar(3) NOT NULL default ' ',
  "AAAAKMYI" varchar(3) NOT NULL default ' ',
  "AAAAKMYJ" varchar(3) NOT NULL default ' ',
  "AAAAKMYK" varchar(3) NOT NULL default ' ',
  "AAAAKMYL" varchar(3) NOT NULL default ' ',
  "AAAAKMYM" varchar(3) NOT NULL default ' ',
  "AAAAKMYN" varchar(3) NOT NULL default ' ',
  "AAAAKMYO" varchar(3) NOT NULL default ' ',
  "AAABMCWV" varchar(15) NOT NULL default ' ',
  "AAAABFJB" varchar(4) NOT NULL default ' ',
  "AAAABFKU" varchar(4) NOT NULL default '0000',
  "AAAABFKX" varchar(3) NOT NULL default ' ',
  "AAAABFLA" varchar(3) NOT NULL default ' ',
  "AAAABFLD" varchar(3) NOT NULL default ' ',
  "AAAABFJS" varchar(3) NOT NULL default ' ',
  "AAAABFJK" varchar(3) NOT NULL default ' ',
  "AAAAKMYQ" varchar(4) NOT NULL default ' ',
  "AAAAKMYR" varchar(1) NOT NULL default ' ',
  "AAAAKMYS" decimal(3,0) NOT NULL default '0',
  "AAAAKMYT" varchar(3) NOT NULL default ' ',
  "AAAAKMYU" varchar(1) NOT NULL default ' ',
  "AAAAKMYV" varchar(1) NOT NULL default ' ',
  "AAAAKMYW" varchar(1) NOT NULL default ' ',
  "AAAAKNTC" varchar(10) NOT NULL default ' ',
  "AAABLAJX" varchar(15) NOT NULL default ' ',
  "AAAAKMYZ" varchar(1) NOT NULL default ' ',
  "AAAAKMZA" varchar(1) NOT NULL default ' ',
  "AAAAKMZB" varchar(1) NOT NULL default ' ',
  "AAAAKMZC" varchar(2) NOT NULL default ' ',
  "AAAAKMZD" varchar(4) NOT NULL default ' ',
  "AAABJGTE" int(11) NOT NULL default '0',
  "AAAAKMZF" decimal(12,6) NOT NULL default '0.000000',
  "AAAAKMZG" varchar(1) NOT NULL default ' ',
  "AAAAKMZH" varchar(1) NOT NULL default ' ',
  "AAAAKMZI" varchar(1) NOT NULL default ' ',
  "AAAAKMZJ" varchar(32) NOT NULL default ' ',
  "AAAAKMZK" varchar(30) NOT NULL default ' ',
  "AAAAKMZL" varchar(32) NOT NULL default ' ',
  "AAAAKMZM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYXH","AAABESZZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFIB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESZZ" varchar(4) NOT NULL default ' ',
  "AAABKYUV" varchar(4) NOT NULL default ' ',
  "AAAAKMYB" varchar(4) NOT NULL default '0000',
  "AAAAKMYC" varchar(4) NOT NULL default '0000',
  "AAAABFKU" varchar(4) NOT NULL default '0000',
  "AAAAKMYG" varchar(3) NOT NULL default ' ',
  "AAAAKMYF" varchar(3) NOT NULL default ' ',
  "AAAABFKX" varchar(3) NOT NULL default ' ',
  "AAAAKMYH" varchar(3) NOT NULL default ' ',
  "AAAAKMYI" varchar(3) NOT NULL default ' ',
  "AAAABFLA" varchar(3) NOT NULL default ' ',
  "AAAAKMYJ" varchar(3) NOT NULL default ' ',
  "AAAAKMYK" varchar(3) NOT NULL default ' ',
  "AAAABFLD" varchar(3) NOT NULL default ' ',
  "AAAAKMYT" varchar(3) NOT NULL default ' ',
  "AAAAKMZR" varchar(1) NOT NULL default ' ',
  "AAAABFIT" varchar(1) NOT NULL default ' ',
  "AAAABFIU" varchar(1) NOT NULL default ' ',
  "AAAABFIV" varchar(1) NOT NULL default ' ',
  "AAAABFIW" varchar(1) NOT NULL default ' ',
  "AAAABFIX" varchar(1) NOT NULL default ' ',
  "AAAAKMYD" varchar(2) NOT NULL default ' ',
  "AAAAKMXZ" varchar(4) NOT NULL default ' ',
  "AAAAKMYA" varchar(4) NOT NULL default ' ',
  "AAAABFJB" varchar(4) NOT NULL default ' ',
  "AAAAKMYS" decimal(3,0) NOT NULL default '0',
  "AAABKWHI" varchar(1) NOT NULL default ' ',
  "AAABKTBM" varchar(10) NOT NULL default ' ',
  "AAAAKMYU" varchar(1) NOT NULL default ' ',
  "AAAAMLNO" varchar(2) NOT NULL default ' ',
  "AAABKYCB" varchar(3) NOT NULL default ' ',
  "AAAAKMYN" varchar(3) NOT NULL default ' ',
  "AAAAKMYO" varchar(3) NOT NULL default ' ',
  "AAAABFJK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESZZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESZZ" varchar(4) NOT NULL default ' ',
  "AAAAKMYL" varchar(3) NOT NULL default ' ',
  "AAAAKMYM" varchar(3) NOT NULL default ' ',
  "AAAABFJS" varchar(3) NOT NULL default ' ',
  "AAAABFJT" varchar(10) NOT NULL default ' ',
  "AAAABFJU" decimal(10,6) NOT NULL default '0.000000',
  "AAAAKMZS" varchar(1) NOT NULL default ' ',
  "AAAAKNTC" varchar(10) NOT NULL default ' ',
  "AAAAKMYZ" varchar(1) NOT NULL default ' ',
  "AAAAKMZB" varchar(1) NOT NULL default ' ',
  "AAAAKMZC" varchar(2) NOT NULL default ' ',
  "AAAAKMZG" varchar(1) NOT NULL default ' ',
  "AAAAKMZH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESZZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFKE" (
  "AAAAKMYQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKMYQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFKH" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAKMYQ" varchar(4) NOT NULL default ' ',
  "AAAABFKK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAAKMYQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFKN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXJ" varchar(3) NOT NULL default ' ',
  "AAABESZZ" varchar(4) NOT NULL default ' ',
  "AAAAKMYQ" varchar(4) NOT NULL default ' ',
  "AAAAKMYB" varchar(4) NOT NULL default '0000',
  "AAAAKMYC" varchar(4) NOT NULL default '0000',
  "AAAABFKU" varchar(4) NOT NULL default '0000',
  "AAAAKMYG" varchar(3) NOT NULL default ' ',
  "AAAAKMYF" varchar(3) NOT NULL default ' ',
  "AAAABFKX" varchar(3) NOT NULL default ' ',
  "AAAAKMYI" varchar(3) NOT NULL default ' ',
  "AAAAKMYH" varchar(3) NOT NULL default ' ',
  "AAAABFLA" varchar(3) NOT NULL default ' ',
  "AAAAKMYK" varchar(3) NOT NULL default ' ',
  "AAAAKMYJ" varchar(3) NOT NULL default ' ',
  "AAAABFLD" varchar(3) NOT NULL default ' ',
  "AAAAKMYT" varchar(3) NOT NULL default ' ',
  "AAAAKMYR" varchar(1) NOT NULL default ' ',
  "AAAAKMYS" decimal(3,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKYXJ","AAABESZZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFLK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCWV" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFLP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCWV" varchar(15) NOT NULL default ' ',
  "AAABFCOF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFLX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYWT" varchar(3) NOT NULL default ' ',
  "AAAABFMA" varchar(2) NOT NULL default ' ',
  "AAAABFMB" varchar(2) NOT NULL default '00',
  "AAAABFNK" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABKYWT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFMF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKYWT" varchar(3) NOT NULL default ' ',
  "AAABJQTW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKYWT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFMN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAABFNK" varchar(3) NOT NULL default '000',
  "AAABGPGM" smallint(6) NOT NULL default '0',
  "AAAABFMR" varchar(2) NOT NULL default '00',
  "AAAABFMS" decimal(10,7) NOT NULL default '0.0000000',
  "AAAABFMT" varchar(2) NOT NULL default '00',
  "AAAABFMU" varchar(4) NOT NULL default ' ',
  "AAAABFMV" varchar(2) NOT NULL default ' ',
  "AAAAMLNO" varchar(2) NOT NULL default ' ',
  "AAABHIXN" varchar(32) NOT NULL default ' ',
  "AAABBFLM" varchar(30) NOT NULL default ' ',
  "AAABKWRJ" decimal(10,9) NOT NULL default '0.000000000',
  PRIMARY KEY ("AAABMRIL","AAAABFNK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFNC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAABFNK" varchar(3) NOT NULL default '000',
  "AAABDXVI" varchar(30) NOT NULL default ' ',
  "AAABFCOF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAABFNK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFNL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCWV" varchar(15) NOT NULL default ' ',
  "AAABKFFG" varchar(15) NOT NULL default ' ',
  "AAAABFNP" smallint(6) NOT NULL default '0',
  "AAAABFNQ" smallint(6) NOT NULL default '0',
  "AAAABFNR" decimal(7,5) NOT NULL default '0.00000',
  PRIMARY KEY ("AAABMRIL","AAABMCWV","AAABKFFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFNV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAABFOM" varchar(8) NOT NULL default '00000000',
  "AAABKYWT" varchar(3) NOT NULL default ' ',
  "AAABMCWV" varchar(15) NOT NULL default ' ',
  "AAABKFFG" varchar(15) NOT NULL default ' ',
  "AAABKYWM" decimal(11,7) NOT NULL default '0.0000000',
  "AAAABFOC" decimal(16,8) NOT NULL default '0.00000000',
  PRIMARY KEY ("AAABMRIL","AAAABFOM","AAABKYWT","AAABMCWV","AAABKFFG"),
  KEY "AAAABFOI" ("AAABMRIL","AAABMCWV","AAABKYWT","AAAABFOM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFON" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKVPA" varchar(10) NOT NULL default ' ',
  "AAABMCWV" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKVPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFOT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYSP" varchar(5) NOT NULL default ' ',
  "AAABKYSQ" varchar(5) NOT NULL default ' ',
  "AAABMCWV" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYSP","AAABKYSQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFPB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKXOX" varchar(13) NOT NULL default ' ',
  "AAABMCWV" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKXOX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYTL" varchar(10) NOT NULL default ' ',
  "AAABMCWV" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFPN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXD" varchar(4) NOT NULL default '0000',
  "AAABHYJH" varchar(5) NOT NULL default ' ',
  "AAABMCWV" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYXD","AAABHYJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFPV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYLX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYLX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFQA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKYLX" varchar(3) NOT NULL default ' ',
  "AAABJQTW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKYLX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFQI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKYUT" varchar(5) NOT NULL default ' ',
  "AAAABFQM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKYUT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYUT" varchar(5) NOT NULL default ' ',
  "AAABESZZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYUT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFQW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYWT" varchar(3) NOT NULL default ' ',
  "AAABKXOX" varchar(13) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABKYWU" varchar(10) NOT NULL default '0000000000',
  "AAABKYUJ" varchar(8) NOT NULL default '00000000',
  "AAABKYWM" decimal(11,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAABKYWT","AAABKXOX","AAABMNZR","AAABKYWU","AAABKYUJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFRK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABETAB" varchar(3) NOT NULL default ' ',
  "AAAABFRY" varchar(15) NOT NULL default ' ',
  "AAAABFRZ" varchar(15) NOT NULL default ' ',
  "AAAABFSA" varchar(2) NOT NULL default '00',
  "AAAABFSB" varchar(15) NOT NULL default ' ',
  "AAAABFSC" varchar(15) NOT NULL default ' ',
  "AAAABFSD" varchar(2) NOT NULL default '00',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABKYWU" varchar(10) NOT NULL default '0000000000',
  "AAAABFRV" decimal(10,9) NOT NULL default '0.000000000',
  PRIMARY KEY ("AAABMRIL","AAABETAB","AAAABFRY","AAAABFRZ","AAAABFSA","AAAABFSB","AAAABFSC","AAAABFSD","AAABMNZR","AAABKYWU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFSG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYWT" varchar(3) NOT NULL default ' ',
  "AAABKYTL" varchar(10) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABKYWU" varchar(10) NOT NULL default '0000000000',
  "AAABKYUJ" varchar(8) NOT NULL default '00000000',
  "AAABKYWM" decimal(11,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAABKYWT","AAABKYTL","AAABMNZR","AAABKYWU","AAABKYUJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFSU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYWT" varchar(3) NOT NULL default ' ',
  "AAABMDYY" varchar(10) NOT NULL default ' ',
  "AAABKYXD" varchar(4) NOT NULL default '0000',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABKYWU" varchar(10) NOT NULL default '0000000000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABKYWM" decimal(11,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAABKYWT","AAABMDYY","AAABKYXD","AAABMNZR","AAABKYWU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFTJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMRCU" varchar(5) NOT NULL default '00000',
  "AAABIAWX" varchar(2) NOT NULL default '00',
  "AAABIAWZ" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMDBA","AAABMRCU","AAABIAWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFTT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMCRU" varchar(8) NOT NULL default '00000000',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQBU" varchar(8) NOT NULL default '00000000',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKPXV" varchar(3) NOT NULL default '000',
  "AAABFKTH" varchar(3) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABKKET" varchar(1) NOT NULL default ' ',
  "AAABIEHR" varchar(10) NOT NULL default ' ',
  "AAABKKEU" varchar(1) NOT NULL default ' ',
  "AAABFTVQ" varchar(2) NOT NULL default ' ',
  "AAABFKYS" varchar(2) NOT NULL default ' ',
  "AAAAEAKG" decimal(15,3) NOT NULL default '0.000',
  "AAAAEAKH" varchar(3) NOT NULL default ' ',
  "AAAAEAKI" varchar(1) NOT NULL default ' ',
  "AAABISXO" smallint(6) NOT NULL default '0',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABFSQO" varchar(2) NOT NULL default ' ',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  "AAABMCOP" varchar(10) NOT NULL default ' ',
  "AAAABFUT" varchar(3) NOT NULL default ' ',
  "AAAABFUU" varchar(3) NOT NULL default '000',
  "AAAABFUV" varchar(4) NOT NULL default '0000',
  "AAABLUSG" decimal(16,0) NOT NULL default '0',
  "AAAABFUX" varchar(3) NOT NULL default '000',
  "AAAABFUY" varchar(3) NOT NULL default '000',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMDBA"),
  KEY "AAAABFVE" ("AAABMRIL","AAABLXTG","AAABMDBB","AAABKPXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFVJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAABFWI" varchar(1) NOT NULL default ' ',
  "AAAABFWJ" varchar(10) NOT NULL default ' ',
  "AAAABFWK" varchar(5) NOT NULL default ' ',
  "AAAABFWL" varchar(1) NOT NULL default ' ',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABJRQR" varchar(5) NOT NULL default ' ',
  "AAABKVPX" varchar(2) NOT NULL default '00',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLSHM" varchar(10) NOT NULL default ' ',
  "AAAABFVU" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLSHU" varchar(4) NOT NULL default ' ',
  "AAABLHZN" varchar(1) NOT NULL default ' ',
  "AAABMPTM" decimal(15,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAAABFWA" decimal(17,2) NOT NULL default '0.00',
  "AAAAMWEI" decimal(17,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAABFWI","AAAABFWJ","AAAABFWK","AAAABFWL","AAABMQIL","AAABJRQR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFWO" (
  "AAABAELO" int(11) NOT NULL default '0',
  "AAAABFWW" int(11) NOT NULL default '0',
  "AAABAEVU" varchar(40) NOT NULL default ' ',
  "AAABKGMO" decimal(10,2) NOT NULL default '0.00',
  "AAAABFWT" varchar(8) NOT NULL default '00000000',
  "AAAABFWU" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABAELO","AAAABFWW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFWX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUHU" varchar(8) NOT NULL default '00000000',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAAABFYS" varchar(10) NOT NULL default ' ',
  "AAABLSHT" varchar(10) NOT NULL default ' ',
  "AAABLSJA" varchar(5) NOT NULL default '00000',
  "AAAABHIB" varchar(5) NOT NULL default '00000',
  "AAAABHIC" varchar(5) NOT NULL default '00000',
  "AAAAXEDQ" varchar(5) NOT NULL default ' ',
  "AAAABHKX" varchar(10) NOT NULL default ' ',
  "AAABLSHM" varchar(10) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABLSJO" varchar(3) NOT NULL default ' ',
  "AAABLSGT" varchar(1) NOT NULL default ' ',
  "AAABLSGS" varchar(8) NOT NULL default '00000000',
  "AAAABHJM" varchar(1) NOT NULL default ' ',
  "AAAABHKR" varchar(8) NOT NULL default '00000000',
  "AAABJFDC" decimal(13,3) NOT NULL default '0.000',
  "AAABLIPB" varchar(3) NOT NULL default ' ',
  "AAAABHLR" varchar(10) NOT NULL default ' ',
  "AAAABHLS" varchar(5) NOT NULL default '00000',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  "AAABMCVS" varchar(1) NOT NULL default ' ',
  "AAABLSJR" varchar(4) NOT NULL default ' ',
  "AAABLSHL" varchar(4) NOT NULL default ' ',
  "AAAABFYA" varchar(4) NOT NULL default ' ',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAABLMDT" varchar(4) NOT NULL default ' ',
  "AAABLSGU" varchar(6) NOT NULL default '000000',
  "AAABLWZD" varchar(10) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAAAMWLX" varchar(1) NOT NULL default ' ',
  "AAABLUHE" varchar(1) NOT NULL default ' ',
  "AAAABFYI" varchar(4) NOT NULL default ' ',
  "AAAABFYJ" varchar(3) NOT NULL default ' ',
  "AAAABHHO" varchar(5) NOT NULL default '00000',
  "AAAABGQK" varchar(10) NOT NULL default ' ',
  "AAAABGQL" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABLUHU","AAABLURS","AAABLURA","AAABMCSR","AAAABFYS","AAABLSHT","AAABLSJA","AAAABHIB","AAAABHIC","AAAAXEDQ"),
  KEY "AAAABFYY" ("AAABMRIL","AAABLSHT","AAABLSJA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFZC" (
  "AAAABFZF" int(11) NOT NULL default '0',
  "AAAABFZE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAAABFZF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABFZG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSHT" varchar(10) NOT NULL default ' ',
  "AAABLSJA" varchar(5) NOT NULL default '00000',
  "AAAABHIB" varchar(5) NOT NULL default '00000',
  "AAAABFZL" varchar(10) NOT NULL default '0000000000',
  "AAABFILP" varchar(18) NOT NULL default ' ',
  "AAAABFZN" varchar(1) NOT NULL default ' ',
  "AAAAVFXQ" varchar(5) NOT NULL default '00000',
  "AAAABGAT" decimal(13,3) NOT NULL default '0.000',
  "AAAABGAU" decimal(13,3) NOT NULL default '0.000',
  "AAAABGAV" decimal(13,3) NOT NULL default '0.000',
  "AAABLSJO" varchar(3) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAAAVFYI" varchar(13) NOT NULL default '0000000000000',
  "AAAABGCI" decimal(13,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABLSHT","AAABLSJA","AAAABHIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABGAE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSHT" varchar(10) NOT NULL default ' ',
  "AAABLSJA" varchar(5) NOT NULL default '00000',
  "AAAABHIB" varchar(5) NOT NULL default '00000',
  "AAAABHIC" varchar(5) NOT NULL default '00000',
  "AAABLSHM" varchar(10) NOT NULL default ' ',
  "AAABLSHL" varchar(4) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAAABGAS" varchar(10) NOT NULL default ' ',
  "AAAABGAT" decimal(13,3) NOT NULL default '0.000',
  "AAAABGAU" decimal(13,3) NOT NULL default '0.000',
  "AAAABGAV" decimal(13,3) NOT NULL default '0.000',
  "AAABLGUS" decimal(13,3) NOT NULL default '0.000',
  "AAABLSJO" varchar(3) NOT NULL default ' ',
  "AAABHTYS" varchar(1) NOT NULL default ' ',
  "AAAABHLR" varchar(10) NOT NULL default ' ',
  "AAAABHLS" varchar(5) NOT NULL default '00000',
  "AAAABHDV" varchar(10) NOT NULL default ' ',
  "AAAANXVA" varchar(5) NOT NULL default '00000',
  "AAAABHHA" varchar(10) NOT NULL default ' ',
  "AAAABHHB" varchar(6) NOT NULL default '000000',
  "AAABLUHU" varchar(8) NOT NULL default '00000000',
  "AAAAONJD" varchar(1) NOT NULL default ' ',
  "AAABLMEI" varchar(8) NOT NULL default '00000000',
  "AAAABGBI" varchar(1) NOT NULL default ' ',
  "AAAAMVRA" varchar(8) NOT NULL default '00000000',
  "AAAABHKX" varchar(10) NOT NULL default ' ',
  "AAABLSJR" varchar(4) NOT NULL default ' ',
  "AAAABHKY" varchar(10) NOT NULL default ' ',
  "AAABLMEB" varchar(4) NOT NULL default ' ',
  "AAABLURC" decimal(3,0) NOT NULL default '0',
  "AAABLLYR" decimal(3,0) NOT NULL default '0',
  "AAAABGRX" varchar(8) NOT NULL default '00000000',
  "AAAABGRY" varchar(1) NOT NULL default ' ',
  "AAAABGBS" varchar(1) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABJFDC" decimal(13,3) NOT NULL default '0.000',
  "AAABLIPB" varchar(3) NOT NULL default ' ',
  "AAAABHLN" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFJ" varchar(3) NOT NULL default ' ',
  "AAABLXRZ" decimal(13,3) NOT NULL default '0.000',
  "AAABLZJP" varchar(3) NOT NULL default ' ',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAAAVFYI" varchar(13) NOT NULL default '0000000000000',
  "AAAABGCE" varchar(1) NOT NULL default ' ',
  "AAAABHMC" varchar(1) NOT NULL default ' ',
  "AAAABGCG" varchar(13) NOT NULL default '0000000000000',
  "AAABLLOE" decimal(13,3) NOT NULL default '0.000',
  "AAAABGCI" decimal(13,3) NOT NULL default '0.000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMCVS" varchar(1) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAAABHLT" varchar(1) NOT NULL default ' ',
  "AAAABHLU" varchar(1) NOT NULL default ' ',
  "AAAABGCP" varchar(1) NOT NULL default ' ',
  "AAAABGCQ" varchar(13) NOT NULL default '0000000000000',
  "AAAABGCR" decimal(13,3) NOT NULL default '0.000',
  "AAAABGCS" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABLSHT","AAABLSJA","AAAABHIB","AAAABHIC"),
  KEY "AAAABGCY" ("AAABLSHM","AAABLSHT","AAABLSJA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABGDC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABKQPE" varchar(2) NOT NULL default '00',
  "AAABLQCR" varchar(12) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  "AAABMOQQ" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABKOSW" varchar(23) NOT NULL default ' ',
  "AAABKOSX" varchar(10) NOT NULL default ' ',
  "AAABKRIO" varchar(10) NOT NULL default ' ',
  "AAABKOSZ" varchar(4) NOT NULL default ' ',
  "AAABKOTA" varchar(1) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABKRFP" varchar(2) NOT NULL default '00',
  "AAABLXQP" varchar(8) NOT NULL default '00000000',
  "AAABKOTE" varchar(2) NOT NULL default '00',
  "AAABKOTF" varchar(1) NOT NULL default ' ',
  "AAABKOTG" varchar(1) NOT NULL default ' ',
  "AAABKOTH" varchar(1) NOT NULL default ' ',
  "AAABKOTI" varchar(1) NOT NULL default ' ',
  "AAABKOTJ" varchar(8) NOT NULL default '00000000',
  "AAABKOTK" varchar(8) NOT NULL default '00000000',
  "AAABKOTL" varchar(8) NOT NULL default '00000000',
  "AAABKOTM" varchar(8) NOT NULL default '00000000',
  "AAABKOTN" varchar(8) NOT NULL default '00000000',
  "AAABKOTO" varchar(8) NOT NULL default '00000000',
  "AAABLUNJ" varchar(1) NOT NULL default ' ',
  "AAABKOTQ" varchar(4) NOT NULL default ' ',
  "AAABLRSP" varchar(1) NOT NULL default ' ',
  "AAABKOTS" varchar(1) NOT NULL default ' ',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABKRGG" varchar(6) NOT NULL default ' ',
  "AAABKOTX" varchar(2) NOT NULL default '00',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  "AAABKOUB" varchar(12) NOT NULL default ' ',
  "AAABKOUC" varchar(10) NOT NULL default ' ',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABLQVJ" varchar(4) NOT NULL default '0000',
  "AAABKOUF" varchar(20) NOT NULL default ' ',
  "AAABKOUG" varchar(20) NOT NULL default ' ',
  "AAABKOUH" varchar(20) NOT NULL default ' ',
  "AAABKOUI" varchar(20) NOT NULL default ' ',
  "AAABKOUJ" decimal(11,2) NOT NULL default '0.00',
  "AAABKOUK" varchar(8) NOT NULL default '00000000',
  "AAABKOUL" varchar(15) NOT NULL default ' ',
  "AAABKOUM" varchar(8) NOT NULL default '00000000',
  "AAABKOUN" varchar(8) NOT NULL default '00000000',
  "AAABKOUO" varchar(1) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAABLKMJ" varchar(8) NOT NULL default '00000000',
  "AAABKOUS" varchar(6) NOT NULL default ' ',
  "AAABKRGH" varchar(6) NOT NULL default ' ',
  "AAABMCXD" varchar(15) NOT NULL default ' ',
  "AAABKRIP" varchar(16) NOT NULL default ' ',
  "AAABKRHS" varchar(2) NOT NULL default ' ',
  "AAABKRIB" varchar(1) NOT NULL default ' ',
  "AAABKROQ" varchar(10) NOT NULL default ' ',
  "AAABKROR" varchar(6) NOT NULL default '000000',
  "AAABKOVA" varchar(20) NOT NULL default ' ',
  "AAABKOVB" varchar(6) NOT NULL default ' ',
  "AAABKRIM" varchar(10) NOT NULL default ' ',
  "AAABKOVD" varchar(1) NOT NULL default ' ',
  "AAABLCKC" varchar(4) NOT NULL default ' ',
  "AAABKRGJ" varchar(10) NOT NULL default ' ',
  "AAABKOVG" varchar(2) NOT NULL default ' ',
  "AAABKRIG" varchar(2) NOT NULL default ' ',
  "AAABKOVI" varchar(5) NOT NULL default ' ',
  "AAABKOVJ" varchar(1) NOT NULL default ' ',
  "AAABKRHV" varchar(7) NOT NULL default ' ',
  "AAABKOVL" varchar(12) NOT NULL default ' ',
  "AAABKOVM" varchar(12) NOT NULL default ' ',
  "AAABKOVN" varchar(12) NOT NULL default '000000000000',
  "AAABKOVO" varchar(4) NOT NULL default ' ',
  "AAABKOVP" varchar(1) NOT NULL default ' ',
  "AAABKSTP" varchar(2) NOT NULL default ' ',
  "AAABKOVR" varchar(10) NOT NULL default ' ',
  "AAABKOVS" varchar(6) NOT NULL default '000000',
  "AAABLZDH" varchar(6) NOT NULL default '000000',
  "AAABKOVU" varchar(4) NOT NULL default ' ',
  "AAABKOVV" varchar(12) NOT NULL default '000000000000',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCWW" varchar(2) NOT NULL default ' ',
  "AAABMCXS" varchar(3) NOT NULL default ' ',
  "AAABLHPM" varchar(4) NOT NULL default ' ',
  "AAABKOWA" varchar(3) NOT NULL default ' ',
  "AAABKOWB" varchar(5) NOT NULL default ' ',
  "AAABKOWC" varchar(1) NOT NULL default ' ',
  "AAABKOWG" varchar(8) NOT NULL default ' ',
  "AAABKOWH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRM"),
  KEY "AAAABGHD" ("AAABMRIL","AAABMCRJ","AAABKOTX","AAABMLHQ"),
  KEY "AAAABGHI" ("AAABMRIL","AAABKQPE","AAABMCVI"),
  KEY "AAAABGHM" ("AAABMRIL","AAABMLHQ","AAABMCRJ"),
  KEY "AAAABGHQ" ("AAABMRIL","AAABLKMJ"),
  KEY "AAAABGHT" ("AAABMRIL","AAABKOVN","AAABKOVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABGHX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLROG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLROG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABGIC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLROG" varchar(4) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLROG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABGIK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLROG" varchar(4) NOT NULL default ' ',
  "AAABMFSV" varchar(2) NOT NULL default ' ',
  "AAABLUHV" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLROG","AAABMFSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABGIU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLROG" varchar(4) NOT NULL default ' ',
  "AAABMFSV" varchar(2) NOT NULL default ' ',
  "AAABEDYX" varchar(2) NOT NULL default ' ',
  "AAAAWSWH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLROG","AAABMFSV","AAABEDYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABGJE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCIM" varchar(10) NOT NULL default ' ',
  "AAABMCME" varchar(4) NOT NULL default '0000',
  "AAABMCMG" varchar(4) NOT NULL default '0000',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABMCEY" varchar(10) NOT NULL default ' ',
  "AAABLXSU" varchar(1) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABKROQ" varchar(10) NOT NULL default ' ',
  "AAABKROR" varchar(6) NOT NULL default '000000',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABMCPZ" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMCVW" varchar(10) NOT NULL default ' ',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABMCVL" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVM" varchar(3) NOT NULL default ' ',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABMCVS" varchar(1) NOT NULL default ' ',
  "AAABMCKE" varchar(12) NOT NULL default ' ',
  "AAABLXWL" varchar(25) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABKRZZ" varchar(10) NOT NULL default '0000000000',
  "AAABLMJL" varchar(4) NOT NULL default '0000',
  "AAABKSAB" varchar(1) NOT NULL default ' ',
  "AAABKRNR" varchar(1) NOT NULL default ' ',
  "AAABMBFP" varchar(1) NOT NULL default ' ',
  "AAABKPOS" varchar(1) NOT NULL default ' ',
  "AAABLXSX" varchar(8) NOT NULL default '00000000',
  "AAABMCXJ" varchar(12) NOT NULL default ' ',
  "AAABMCXK" varchar(10) NOT NULL default '0000000000',
  "AAABMCXL" varchar(8) NOT NULL default '00000000',
  "AAABKPSQ" varchar(10) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCIM","AAABMCME","AAABMCMG"),
  KEY "AAAABGKY" ("AAABMRIL","AAABMCRM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABGLB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSHT" varchar(10) NOT NULL default ' ',
  "AAABLSJA" varchar(5) NOT NULL default '00000',
  "AAAABHIB" varchar(5) NOT NULL default '00000',
  "AAAABHIC" varchar(5) NOT NULL default '00000',
  "AAAAXEDQ" varchar(5) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABLZJP" varchar(3) NOT NULL default ' ',
  "AAABLSGS" varchar(8) NOT NULL default '00000000',
  "AAABLSGT" varchar(1) NOT NULL default ' ',
  "AAAABGLO" varchar(6) NOT NULL default '000000',
  "AAABLSJR" varchar(4) NOT NULL default ' ',
  "AAAABHKX" varchar(10) NOT NULL default ' ',
  "AAABLVIN" varchar(10) NOT NULL default ' ',
  "AAAABGLS" varchar(4) NOT NULL default ' ',
  "AAAABGLT" varchar(8) NOT NULL default '00000000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAABLMDT" varchar(4) NOT NULL default ' ',
  "AAAAZRZA" varchar(4) NOT NULL default ' ',
  "AAABLWZD" varchar(10) NOT NULL default ' ',
  "AAAAMWLX" varchar(1) NOT NULL default ' ',
  "AAAABHHO" varchar(5) NOT NULL default '00000',
  "AAABLUHE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSHT","AAABLSJA","AAAABHIB","AAAABHIC","AAAAXEDQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABGMK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSHT" varchar(10) NOT NULL default ' ',
  "AAABLSHU" varchar(4) NOT NULL default ' ',
  "AAABLMEE" varchar(40) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAAABGMR" varchar(1) NOT NULL default ' ',
  "AAAABGMS" varchar(1) NOT NULL default ' ',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAAABGMY" varchar(5) NOT NULL default '00000',
  "AAAAZRWT" varchar(5) NOT NULL default '00000',
  "AAABLMEG" varchar(8) NOT NULL default '00000000',
  "AAABLMEF" varchar(1) NOT NULL default ' ',
  "AAABLMEJ" varchar(8) NOT NULL default '00000000',
  "AAABLMEM" varchar(1) NOT NULL default ' ',
  "AAABLMEL" varchar(8) NOT NULL default '00000000',
  "AAABLMEO" varchar(1) NOT NULL default ' ',
  "AAABLMEK" varchar(8) NOT NULL default '00000000',
  "AAABLMEN" varchar(1) NOT NULL default ' ',
  "AAAABGRN" varchar(8) NOT NULL default '00000000',
  "AAAABGRO" varchar(1) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAAABGNL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSHT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABGNO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSHT" varchar(10) NOT NULL default ' ',
  "AAABLSJA" varchar(5) NOT NULL default '00000',
  "AAAABHIB" varchar(5) NOT NULL default '00000',
  "AAAABHIC" varchar(5) NOT NULL default '00000',
  "AAABLSGZ" varchar(3) NOT NULL default '000',
  "AAABLSGS" varchar(8) NOT NULL default '00000000',
  "AAABLSGT" varchar(1) NOT NULL default ' ',
  "AAABLSGU" varchar(6) NOT NULL default '000000',
  "AAABLSGV" varchar(3) NOT NULL default '000',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABLSJO" varchar(3) NOT NULL default ' ',
  "AAABKQYK" decimal(13,3) NOT NULL default '0.000',
  "AAABMCIM" varchar(10) NOT NULL default ' ',
  "AAAAQQZB" varchar(4) NOT NULL default '0000',
  "AAABLXRZ" decimal(13,3) NOT NULL default '0.000',
  "AAABLZJP" varchar(3) NOT NULL default ' ',
  "AAABJTNB" varchar(8) NOT NULL default '00000000',
  "AAAABHHA" varchar(10) NOT NULL default ' ',
  "AAAABHHB" varchar(6) NOT NULL default '000000',
  "AAABLUHU" varchar(8) NOT NULL default '00000000',
  "AAABJFDC" decimal(13,3) NOT NULL default '0.000',
  "AAABLIPB" varchar(3) NOT NULL default ' ',
  "AAAABHLR" varchar(10) NOT NULL default ' ',
  "AAAABHLS" varchar(5) NOT NULL default '00000',
  "AAABLNVG" varchar(8) NOT NULL default '00000000',
  "AAAABHLN" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFJ" varchar(3) NOT NULL default ' ',
  "AAAABHDV" varchar(10) NOT NULL default ' ',
  "AAAANXVA" varchar(5) NOT NULL default '00000',
  "AAABMCVS" varchar(1) NOT NULL default ' ',
  "AAAABHKE" decimal(13,3) NOT NULL default '0.000',
  "AAABLGUS" decimal(13,3) NOT NULL default '0.000',
  "AAAABGOW" varchar(10) NOT NULL default ' ',
  "AAAABGOX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLSHT","AAABLSJA","AAAABHIB","AAAABHIC","AAABLSGZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABGPE" (
  "AAABAELO" int(11) NOT NULL default '0',
  "AAABMDXM" longtext,
  PRIMARY KEY ("AAABAELO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABGPI" (
  "AAABAELO" int(11) NOT NULL default '0',
  "AAABLEUZ" varchar(40) NOT NULL default ' ',
  "AAAABGPL" varchar(40) NOT NULL default ' ',
  "AAABLEQN" varchar(8) NOT NULL default '00000000',
  "AAABLEQO" varchar(6) NOT NULL default '000000',
  "AAAABGPO" varchar(40) NOT NULL default ' ',
  "AAABCRYR" varchar(40) NOT NULL default ' ',
  "AAAABGPQ" varchar(40) NOT NULL default ' ',
  "AAAABGPR" varchar(40) NOT NULL default ' ',
  "AAAABGPS" tinyint(3) unsigned NOT NULL default '0',
  "AAABLRXL" int(11) NOT NULL default '0',
  "AAAABGPU" varchar(40) NOT NULL default ' ',
  "AAAABGPV" varchar(40) NOT NULL default ' ',
  "AAAABGPW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABAELO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABGPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSHT" varchar(10) NOT NULL default ' ',
  "AAABLSJA" varchar(5) NOT NULL default '00000',
  "AAABLMDT" varchar(4) NOT NULL default ' ',
  "AAAABGQD" varchar(1) NOT NULL default ' ',
  "AAAABGQE" varchar(1) NOT NULL default ' ',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAAABGQK" varchar(10) NOT NULL default ' ',
  "AAAABGQL" varchar(5) NOT NULL default '00000',
  "AAABKRZZ" varchar(10) NOT NULL default '0000000000',
  "AAABLMJL" varchar(4) NOT NULL default '0000',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABLWZD" varchar(10) NOT NULL default ' ',
  "AAAABGQR" varchar(2) NOT NULL default '00',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLSLC" varchar(40) NOT NULL default ' ',
  "AAABMCFC" varchar(10) NOT NULL default ' ',
  "AAABLSJR" varchar(4) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLSFX" varchar(10) NOT NULL default ' ',
  "AAAABGQY" varchar(10) NOT NULL default '0000000000',
  "AAAABGQZ" varchar(18) NOT NULL default ' ',
  "AAAABGRA" decimal(13,3) NOT NULL default '0.000',
  "AAAABGRB" decimal(13,3) NOT NULL default '0.000',
  "AAAABGRC" decimal(13,3) NOT NULL default '0.000',
  "AAABLSJO" varchar(3) NOT NULL default ' ',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAAABGRF" decimal(5,0) NOT NULL default '0',
  "AAAABGRG" decimal(5,0) NOT NULL default '0',
  "AAABLMDY" varchar(1) NOT NULL default ' ',
  "AAAAVYVL" varchar(1) NOT NULL default ' ',
  "AAABLMDZ" varchar(1) NOT NULL default ' ',
  "AAABLMEA" varchar(1) NOT NULL default ' ',
  "AAABLMDV" varchar(4) NOT NULL default ' ',
  "AAABHTYS" varchar(1) NOT NULL default ' ',
  "AAAABGRN" varchar(8) NOT NULL default '00000000',
  "AAAABGRO" varchar(1) NOT NULL default ' ',
  "AAABLMEB" varchar(4) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAAABHKX" varchar(10) NOT NULL default ' ',
  "AAAABHKY" varchar(10) NOT NULL default ' ',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  "AAAAZRYR" varchar(4) NOT NULL default ' ',
  "AAAAZRYS" varchar(4) NOT NULL default ' ',
  "AAABLZZR" varchar(4) NOT NULL default ' ',
  "AAAABGRX" varchar(8) NOT NULL default '00000000',
  "AAAABGRY" varchar(1) NOT NULL default ' ',
  "AAAABGRZ" varchar(1) NOT NULL default ' ',
  "AAAABGSA" varchar(1) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAAABGSC" decimal(13,3) NOT NULL default '0.000',
  "AAAAZSBV" varchar(1) NOT NULL default ' ',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABKPSV" varchar(6) NOT NULL default '000000',
  "AAAABHHO" varchar(5) NOT NULL default '00000',
  "AAABLSJL" varchar(2) NOT NULL default ' ',
  "AAABLUHE" varchar(1) NOT NULL default ' ',
  "AAABLMEC" varchar(1) NOT NULL default ' ',
  "AAAAZRZA" varchar(4) NOT NULL default ' ',
  "AAAAZRXV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSHT","AAABLSJA"),
  KEY "AAAABGSP" ("AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABGSR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSHT" varchar(10) NOT NULL default ' ',
  "AAABLSJA" varchar(5) NOT NULL default '00000',
  "AAAABHIB" varchar(5) NOT NULL default '00000',
  "AAAABHIC" varchar(5) NOT NULL default '00000',
  "AAAABHLT" varchar(1) NOT NULL default ' ',
  "AAAABHLU" varchar(1) NOT NULL default ' ',
  "AAAABHLV" varchar(1) NOT NULL default ' ',
  "AAAABHLW" varchar(1) NOT NULL default ' ',
  "AAAABHLX" varchar(1) NOT NULL default ' ',
  "AAAABHLY" varchar(1) NOT NULL default ' ',
  "AAAABHLZ" varchar(1) NOT NULL default ' ',
  "AAAABHMA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSHT","AAABLSJA","AAAABHIB","AAAABHIC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABGTK" (
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABMKXW" varchar(10) NOT NULL default ' ',
  "AAABMNMZ" varchar(40) NOT NULL default ' ',
  "AAABMNNA" varchar(40) NOT NULL default ' ',
  "AAABINIL" varchar(12) NOT NULL default ' ',
  "AAABLEER" varchar(8) NOT NULL default '00000000',
  "AAABINIK" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMLPV","AAABMOHM","AAABMKFH","AAABMKXW","AAABMNMZ"),
  KEY "AAAABGTZ" ("AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABGUB" (
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABMKXW" varchar(10) NOT NULL default ' ',
  "AAABMNMZ" varchar(40) NOT NULL default ' ',
  "AAABMNNA" varchar(40) NOT NULL default ' ',
  "AAABINIL" varchar(12) NOT NULL default ' ',
  "AAABLEER" varchar(8) NOT NULL default '00000000',
  "AAABINIK" varchar(6) NOT NULL default '000000',
  "AAABILFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV","AAABMOHM","AAABMKFH","AAABMKXW","AAABMNMZ"),
  KEY "AAAABGUR" ("AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABGUT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDRQC" varchar(50) NOT NULL default ' ',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  "AAABDHBB" varchar(3) NOT NULL default '000',
  "AAABDRQB" varchar(1) NOT NULL default ' ',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABDHCH" varchar(4) NOT NULL default '0000',
  "AAABLUPE" varchar(30) default NULL,
  "AAABDHBD" double default NULL,
  "AAABDHBE" varchar(3) default NULL,
  "AAABDHBF" double default NULL,
  "AAABDHBG" varchar(3) default NULL,
  "AAABDHBH" varchar(1) NOT NULL default ' ',
  "AAABDHBI" double default NULL,
  "AAABDHBJ" double default NULL,
  "AAABDHBK" varchar(1) default NULL,
  "AAABDHBL" double default NULL,
  "AAABDHBM" varchar(1) default NULL,
  "AAABKRQD" varchar(12) default NULL,
  "AAABLRYP" varchar(8) NOT NULL default '00000000',
  "AAABGKZH" varchar(1) NOT NULL default ' ',
  "AAABDHCD" varchar(10) default NULL,
  "AAABDHCE" varchar(3) default NULL,
  "AAABDHCF" varchar(4) NOT NULL default '0000',
  "AAABDHCG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDRQC","AAABLUSC","AAABDHBB","AAABDRQB","AAABLTUX","AAABDHCH"),
  KEY "AAAABGWA" ("AAABMRIL","AAABKRQD"),
  KEY "AAAABGWD" ("AAABMRIL","AAABLTUX","AAABLUSC","AAABLUPE","AAABDRQC","AAABDRQB","AAABLRYP"),
  KEY "AAAABGWL" ("AAABMRIL","AAABLTUX","AAABLUSC","AAABDHBD","AAABDRQC","AAABDRQB","AAABLRYP"),
  KEY "AAAABGWT" ("AAABMRIL","AAABDRQC","AAABLTUX","AAABLUSC","AAABLUPE","AAABDRQB","AAABLRYP"),
  KEY "AAAABGXB" ("AAABMRIL","AAABDRQC","AAABLTUX","AAABLUSC","AAABDHBD","AAABDRQB","AAABLRYP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABGXJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZNN" varchar(10) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAAAVEWG" varchar(8) NOT NULL default '00000000',
  "AAAABGXO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLZNN","AAABLZNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABGXS" (
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  "AAAAUNWW" varchar(1) NOT NULL default ' ',
  "AAABJZVA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHUGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABGXX" (
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAAABGYR" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABHUGE","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABGYD" (
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  "AAABJEDB" varchar(30) NOT NULL default ' ',
  "AAABIKUC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABGYJ" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABJRGL" varchar(1) NOT NULL default ' ',
  "AAABJZVA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABGYO" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAAABGYR" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABGYU" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP","AAABMKFH","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABGZB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" int(11) NOT NULL default '0',
  "AAABMKLH" varchar(8) NOT NULL default '00000000',
  "AAAABGZF" int(11) NOT NULL default '0',
  "AAAABGZG" int(11) NOT NULL default '0',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLLJR" varchar(6) NOT NULL default '000000',
  "AAAABGZJ" varchar(8) NOT NULL default '00000000',
  "AAAABGZK" varchar(6) NOT NULL default '000000',
  "AAAABGZL" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABGZO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCSTV" varchar(10) NOT NULL default ' ',
  "AAABLBDX" varchar(70) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAABKEXE" varchar(1) NOT NULL default ' ',
  "AAABLUSV" int(11) NOT NULL default '0',
  "AAAAYJOS" varchar(1) NOT NULL default ' ',
  "AAAABGZW" int(11) NOT NULL default '0',
  "AAABIGLE" varchar(8) NOT NULL default '00000000',
  "AAABIGLF" varchar(6) NOT NULL default '000000',
  "AAABIGIZ" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCSTV","AAABLBDX","AAABLZNO","AAABKEXE","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABHAG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLZNN" varchar(10) NOT NULL default ' ',
  "AAABLBDX" varchar(70) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAABKEXE" varchar(1) NOT NULL default ' ',
  "AAABLUMP" tinyint(3) unsigned NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLZNN","AAABLBDX","AAABLZNO","AAABKEXE","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABHAX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCSTV" varchar(10) NOT NULL default ' ',
  "AAABLBDX" varchar(70) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAABKEXE" varchar(1) NOT NULL default ' ',
  "AAABLUSV" int(11) NOT NULL default '0',
  "AAAABHBE" tinyint(3) unsigned NOT NULL default '0',
  "AAABHNLS" tinyint(3) unsigned NOT NULL default '0',
  "AAABIGLE" varchar(8) NOT NULL default '00000000',
  "AAABIGLF" varchar(6) NOT NULL default '000000',
  "AAABIGIZ" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCSTV","AAABLBDX","AAABLZNO","AAABKEXE","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABHBP" (
  "AAAABHCY" varchar(12) NOT NULL default ' ',
  "AAAABHCP" varchar(8) NOT NULL default '00000000',
  "AAAABHCQ" varchar(6) NOT NULL default '000000',
  "AAAABHCR" varchar(6) NOT NULL default ' ',
  "AAAABHCS" varchar(8) NOT NULL default '00000000',
  "AAAABHCT" varchar(6) NOT NULL default '000000',
  "AAAABHCU" varchar(8) NOT NULL default ' ',
  "AAAABHCV" varchar(4) NOT NULL default ' ',
  "AAAABHCW" varchar(10) NOT NULL default ' ',
  "AAAABHCX" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAABHCY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABHCB" (
  "AAAABHCY" varchar(12) NOT NULL default ' ',
  "AAAABHCP" varchar(8) NOT NULL default '00000000',
  "AAAABHCQ" varchar(6) NOT NULL default '000000',
  "AAAABHCR" varchar(6) NOT NULL default ' ',
  "AAAABHCS" varchar(8) NOT NULL default '00000000',
  "AAAABHCT" varchar(6) NOT NULL default '000000',
  "AAAABHCU" varchar(8) NOT NULL default ' ',
  "AAAABHCV" varchar(4) NOT NULL default ' ',
  "AAAABHCW" varchar(10) NOT NULL default ' ',
  "AAAABHCX" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAABHCY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABHCN" (
  "AAAABHCY" varchar(12) NOT NULL default ' ',
  "AAAABHCP" varchar(8) NOT NULL default '00000000',
  "AAAABHCQ" varchar(6) NOT NULL default '000000',
  "AAAABHCR" varchar(6) NOT NULL default ' ',
  "AAAABHCS" varchar(8) NOT NULL default '00000000',
  "AAAABHCT" varchar(6) NOT NULL default '000000',
  "AAAABHCU" varchar(8) NOT NULL default ' ',
  "AAAABHCV" varchar(4) NOT NULL default ' ',
  "AAAABHCW" varchar(10) NOT NULL default ' ',
  "AAAABHCX" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAABHCY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABHCZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLNVG" varchar(8) NOT NULL default '00000000',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAAABHKX" varchar(10) NOT NULL default ' ',
  "AAABLSHM" varchar(10) NOT NULL default ' ',
  "AAABLSHT" varchar(10) NOT NULL default ' ',
  "AAABLSJA" varchar(5) NOT NULL default '00000',
  "AAAABHIB" varchar(5) NOT NULL default '00000',
  "AAAABHIC" varchar(5) NOT NULL default '00000',
  "AAAAXEDQ" varchar(5) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABLSJO" varchar(3) NOT NULL default ' ',
  "AAABLSGT" varchar(1) NOT NULL default ' ',
  "AAABLSGS" varchar(8) NOT NULL default '00000000',
  "AAAABHJM" varchar(1) NOT NULL default ' ',
  "AAAABHKR" varchar(8) NOT NULL default '00000000',
  "AAAABHLN" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFJ" varchar(3) NOT NULL default ' ',
  "AAAABHDV" varchar(10) NOT NULL default ' ',
  "AAAANXVA" varchar(5) NOT NULL default '00000',
  "AAAAZRYR" varchar(4) NOT NULL default ' ',
  "AAABMCVS" varchar(1) NOT NULL default ' ',
  "AAABLSJR" varchar(4) NOT NULL default ' ',
  "AAAABHKY" varchar(10) NOT NULL default ' ',
  "AAABLSHL" varchar(4) NOT NULL default ' ',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAABLMDT" varchar(4) NOT NULL default ' ',
  "AAABLSGU" varchar(6) NOT NULL default '000000',
  "AAABLWZD" varchar(10) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABLUHE" varchar(1) NOT NULL default ' ',
  "AAAABHHO" varchar(5) NOT NULL default '00000',
  "AAAAMWLX" varchar(1) NOT NULL default ' ',
  "AAAABHEK" varchar(3) NOT NULL default ' ',
  "AAAABHEL" varchar(4) NOT NULL default ' ',
  "AAAABHEM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLNVG","AAABLURS","AAABLURA","AAABMCSR","AAAABHKX","AAABLSHM","AAABLSHT","AAABLSJA","AAAABHIB","AAAABHIC","AAAAXEDQ"),
  KEY "AAAABHEZ" ("AAABMRIL","AAABLSHT","AAABLSJA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABHFD" (
  "AAABAEVU" varchar(40) NOT NULL default ' ',
  "AAAACIPT" varchar(40) NOT NULL default ' ',
  "AAABIYEN" varchar(40) NOT NULL default ' ',
  "AAABIYEO" varchar(40) NOT NULL default ' ',
  "AAABGRXB" varchar(40) NOT NULL default ' ',
  "AAABJXDX" varchar(40) NOT NULL default ' ',
  "AAAAPRDO" varchar(40) NOT NULL default ' ',
  "AAABJSTP" varchar(40) NOT NULL default ' ',
  "AAABLEHK" varchar(40) NOT NULL default ' ',
  "AAAABHFN" varchar(40) NOT NULL default ' ',
  "AAABGANI" varchar(40) NOT NULL default ' ',
  "AAABGZJM" decimal(10,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABAEVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABHFR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABJTNB" varchar(8) NOT NULL default '00000000',
  "AAABLXQJ" varchar(2) NOT NULL default '00',
  "AAABLYRW" varchar(6) NOT NULL default ' ',
  "AAABLUGX" varchar(10) NOT NULL default ' ',
  "AAABKXWD" varchar(8) NOT NULL default '00000000',
  "AAABLSHM" varchar(10) NOT NULL default ' ',
  "AAABLSHT" varchar(10) NOT NULL default ' ',
  "AAABLSJA" varchar(5) NOT NULL default '00000',
  "AAAABHIB" varchar(5) NOT NULL default '00000',
  "AAAABHIC" varchar(5) NOT NULL default '00000',
  "AAAAXEDQ" varchar(5) NOT NULL default ' ',
  "AAABLWHG" varchar(10) NOT NULL default ' ',
  "AAAABHKX" varchar(10) NOT NULL default ' ',
  "AAABLSJR" varchar(4) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABLXID" varchar(4) NOT NULL default ' ',
  "AAABLXIC" varchar(3) NOT NULL default ' ',
  "AAABLXIL" varchar(1) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABLSJO" varchar(3) NOT NULL default ' ',
  "AAABKXWF" varchar(8) NOT NULL default '00000000',
  "AAABMLEV" decimal(15,3) NOT NULL default '0.000',
  "AAABMLEW" varchar(3) NOT NULL default ' ',
  "AAABMLEX" decimal(15,3) NOT NULL default '0.000',
  "AAABMLEY" varchar(3) NOT NULL default ' ',
  "AAABKOPN" decimal(5,2) NOT NULL default '0.00',
  "AAABKMAO" varchar(10) NOT NULL default ' ',
  "AAABKMDW" varchar(10) NOT NULL default ' ',
  "AAABLZZR" varchar(4) NOT NULL default ' ',
  "AAAABHHA" varchar(10) NOT NULL default ' ',
  "AAAABHHB" varchar(6) NOT NULL default '000000',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAABLMDT" varchar(4) NOT NULL default ' ',
  "AAABMCVS" varchar(1) NOT NULL default ' ',
  "AAAABHHF" decimal(13,3) NOT NULL default '0.000',
  "AAABLZJP" varchar(3) NOT NULL default ' ',
  "AAABLSGS" varchar(8) NOT NULL default '00000000',
  "AAABLSGT" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABLWZD" varchar(10) NOT NULL default ' ',
  "AAAALSIR" varchar(6) NOT NULL default '000000',
  "AAABLUHE" varchar(1) NOT NULL default ' ',
  "AAAABHHO" varchar(5) NOT NULL default '00000',
  "AAAAMWLX" varchar(1) NOT NULL default ' ',
  "AAABLXWL" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLEZ","AAABJTNB","AAABLXQJ","AAABLYRW","AAABLUGX","AAABKXWD","AAABLSHM","AAABLSHT","AAABLSJA","AAAABHIB","AAAABHIC","AAAAXEDQ"),
  KEY "AAAABHIE" ("AAABMRIL","AAABLSHT","AAABLSJA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABHII" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSHT" varchar(10) NOT NULL default ' ',
  "AAABLSJA" varchar(5) NOT NULL default '00000',
  "AAABLSHL" varchar(4) NOT NULL default ' ',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABLSJR" varchar(4) NOT NULL default ' ',
  "AAABLXQJ" varchar(2) NOT NULL default '00',
  "AAABLYRW" varchar(6) NOT NULL default ' ',
  "AAAABHIR" varchar(10) NOT NULL default ' ',
  "AAAABHIS" varchar(10) NOT NULL default ' ',
  "AAABLUGX" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABLURC" decimal(3,0) NOT NULL default '0',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABLXWL" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSHT","AAABLSJA","AAABLSHL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABHJF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSHT" varchar(10) NOT NULL default ' ',
  "AAABLSJA" varchar(5) NOT NULL default '00000',
  "AAABLSJR" varchar(4) NOT NULL default ' ',
  "AAAAXEDQ" varchar(3) NOT NULL default '000',
  "AAAABHKR" varchar(8) NOT NULL default '00000000',
  "AAAABHJM" varchar(1) NOT NULL default ' ',
  "AAABLSGU" varchar(6) NOT NULL default '000000',
  "AAABLSGV" varchar(3) NOT NULL default '000',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABLSJO" varchar(3) NOT NULL default ' ',
  "AAABKQYK" decimal(13,3) NOT NULL default '0.000',
  "AAABMCIM" varchar(10) NOT NULL default ' ',
  "AAAAQQZB" varchar(4) NOT NULL default '0000',
  "AAABLXRZ" decimal(13,3) NOT NULL default '0.000',
  "AAABLZJP" varchar(3) NOT NULL default ' ',
  "AAABLUHU" varchar(8) NOT NULL default '00000000',
  "AAABJFDC" decimal(13,3) NOT NULL default '0.000',
  "AAABLIPB" varchar(3) NOT NULL default ' ',
  "AAAABHLR" varchar(10) NOT NULL default ' ',
  "AAAABHLS" varchar(5) NOT NULL default '00000',
  "AAAABHLN" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFJ" varchar(3) NOT NULL default ' ',
  "AAABMCVS" varchar(1) NOT NULL default ' ',
  "AAAABHKE" decimal(13,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABLSHT","AAABLSJA","AAABLSJR","AAAAXEDQ"),
  KEY "AAAABHKK" ("AAABMRIL","AAAABHLR","AAAABHLS"),
  KEY "AAAABHKO" ("AAABMRIL","AAABLSJR","AAAABHKR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABHKS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSHT" varchar(10) NOT NULL default ' ',
  "AAABLSJA" varchar(5) NOT NULL default '00000',
  "AAABLSJR" varchar(4) NOT NULL default ' ',
  "AAAABHKX" varchar(10) NOT NULL default ' ',
  "AAAABHKY" varchar(10) NOT NULL default ' ',
  "AAAABHKZ" decimal(13,3) NOT NULL default '0.000',
  "AAAABHLA" decimal(13,3) NOT NULL default '0.000',
  "AAAABHLB" decimal(13,3) NOT NULL default '0.000',
  "AAABLSJO" varchar(3) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABLURC" decimal(3,0) NOT NULL default '0',
  "AAABLLYR" decimal(3,0) NOT NULL default '0',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAABJFDC" decimal(13,3) NOT NULL default '0.000',
  "AAABLIPB" varchar(3) NOT NULL default ' ',
  "AAAABHLN" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFJ" varchar(3) NOT NULL default ' ',
  "AAABLXRZ" decimal(13,3) NOT NULL default '0.000',
  "AAABLZJP" varchar(3) NOT NULL default ' ',
  "AAAABHLR" varchar(10) NOT NULL default ' ',
  "AAAABHLS" varchar(5) NOT NULL default '00000',
  "AAAABHLT" varchar(1) NOT NULL default ' ',
  "AAAABHLU" varchar(1) NOT NULL default ' ',
  "AAAABHLV" varchar(1) NOT NULL default ' ',
  "AAAABHLW" varchar(1) NOT NULL default ' ',
  "AAAABHLX" varchar(1) NOT NULL default ' ',
  "AAAABHLY" varchar(1) NOT NULL default ' ',
  "AAAABHLZ" varchar(1) NOT NULL default ' ',
  "AAAABHMA" varchar(1) NOT NULL default ' ',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAAABHMC" varchar(1) NOT NULL default ' ',
  "AAABMCVS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAABHMG" varchar(1) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSHT","AAABLSJA","AAABLSJR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABHMM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRVS" varchar(1) NOT NULL default ' ',
  "AAAABHNS" varchar(1) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAAABHNI" varchar(5) NOT NULL default '00000',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABLIPB" varchar(3) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVS" varchar(1) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAAABHND" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRVS","AAAABHNS","AAABMDBA","AAAABHNI"),
  KEY "AAAABHNJ" ("AAABMRIL","AAABKRVS","AAAABHNS","AAABMCVP","AAABMCVQ"),
  KEY "AAAABHNP" ("AAABMRIL","AAABKRVS","AAAABHNS","AAABMPWN","AAABMCVI","AAABMCSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJEG" (
  "AAABLLNR" varchar(10) NOT NULL default ' ',
  "AAABLEDY" varchar(12) NOT NULL default ' ',
  "AAAATUHB" varchar(10) NOT NULL default ' ',
  "AAABLRTV" varchar(8) NOT NULL default '00000000',
  "AAABLRTW" varchar(6) NOT NULL default '000000',
  "AAABLEQN" varchar(8) NOT NULL default '00000000',
  "AAABLEQO" varchar(6) NOT NULL default '000000',
  "AAAAMYCL" varchar(1) NOT NULL default ' ',
  "AAAARYIV" varchar(1) NOT NULL default ' ',
  "AAAAMYCN" varchar(20) NOT NULL default ' ',
  "AAAAMYCO" varchar(20) NOT NULL default ' ',
  "AAAAPIRV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNR","AAABLEDY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABHOL" (
  "AAABLLNR" varchar(10) NOT NULL default ' ',
  "AAABLEDY" varchar(12) NOT NULL default ' ',
  "AAAAMYCV" varchar(1) NOT NULL default ' ',
  "AAAAMYCW" varchar(1) NOT NULL default ' ',
  "AAAAMYCX" varchar(1) NOT NULL default ' ',
  "AAAAMYCY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNR","AAABLEDY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABHOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABLZBN" varchar(10) NOT NULL default ' ',
  "AAAAMOKP" varchar(16) NOT NULL default ' ',
  "AAAAWCQH" varchar(1) NOT NULL default ' ',
  "AAABLTHQ" varchar(10) NOT NULL default ' ',
  "AAABLCFQ" varchar(12) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAQUIH" varchar(8) NOT NULL default '00000000',
  "AAAAQUJA" decimal(13,2) NOT NULL default '0.00',
  "AAAAQUJB" decimal(13,2) NOT NULL default '0.00',
  "AAAAQUIX" varchar(8) NOT NULL default '00000000',
  "AAABJRIC" varchar(1) NOT NULL default ' ',
  "AAAAQUID" varchar(15) NOT NULL default ' ',
  "AAAAQUIC" varchar(18) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAAAYCIW" varchar(25) NOT NULL default ' ',
  "AAAAWHKA" varchar(2) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABJDQB" varchar(1) NOT NULL default ' ',
  "AAAABHSZ" varchar(1) NOT NULL default ' ',
  "AAAABHPV" varchar(1) NOT NULL default ' ',
  "AAAATVFV" varchar(1) NOT NULL default ' ',
  "AAAALEVN" varchar(3) NOT NULL default ' ',
  "AAAAWGNF" varchar(3) NOT NULL default ' ',
  "AAAAHHIM" varchar(20) NOT NULL default ' ',
  "AAAAHHIN" varchar(5) NOT NULL default ' ',
  "AAABLXRH" varchar(35) NOT NULL default ' ',
  "AAAABHQC" varchar(1) NOT NULL default ' ',
  "AAABJRIB" varchar(10) NOT NULL default ' ',
  "AAABKPPG" varchar(8) NOT NULL default '00000000',
  "AAAAHHCJ" varchar(6) NOT NULL default ' ',
  "AAAAHHCN" decimal(13,2) NOT NULL default '0.00',
  "AAABHFBD" varchar(3) NOT NULL default ' ',
  "AAABGCUQ" varchar(4) NOT NULL default ' ',
  "AAABCPEO" varchar(3) NOT NULL default ' ',
  "AAAABHQK" varchar(11) NOT NULL default ' ',
  "AAABMCNG" decimal(9,5) NOT NULL default '0.00000',
  "AAAAHHEH" varchar(8) NOT NULL default '00000000',
  "AAAAPOSH" varchar(13) NOT NULL default ' ',
  "AAAABHQO" varchar(1) NOT NULL default ' ',
  "AAAABHQP" varchar(2) NOT NULL default ' ',
  "AAAABHQQ" varchar(55) NOT NULL default ' ',
  "AAABMCQW" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMPWP","AAABLZBN","AAAAMOKP"),
  KEY "AAAABHQX" ("AAABMREX","AAABLXRH","AAABJRIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABHRB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABLZBN" varchar(10) NOT NULL default ' ',
  "AAAAMOKP" varchar(16) NOT NULL default ' ',
  "AAAABHUE" varchar(5) NOT NULL default '00000',
  "AAAAXIXF" varchar(10) NOT NULL default ' ',
  "AAAABHRJ" varchar(4) NOT NULL default ' ',
  "AAABCVPG" varchar(30) NOT NULL default ' ',
  "AAAABHRL" varchar(30) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDGB" varchar(16) NOT NULL default ' ',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMCSS" varchar(10) NOT NULL default ' ',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMDGC" varchar(18) NOT NULL default ' ',
  "AAAABHTY" varchar(3) NOT NULL default ' ',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAABMCQG" decimal(13,2) NOT NULL default '0.00',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABMCQA" decimal(13,2) NOT NULL default '0.00',
  "AAABMCTL" decimal(13,2) NOT NULL default '0.00',
  "AAABMCTF" decimal(13,2) NOT NULL default '0.00',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAAABHSC" varchar(1) NOT NULL default ' ',
  "AAAAQUOZ" decimal(5,3) NOT NULL default '0.000',
  "AAAAQUOY" decimal(3,0) NOT NULL default '0',
  "AAABBVKZ" decimal(13,2) NOT NULL default '0.00',
  "AAABBVLC" decimal(13,2) NOT NULL default '0.00',
  "AAABBVLA" decimal(13,2) NOT NULL default '0.00',
  "AAABBVLB" decimal(13,2) NOT NULL default '0.00',
  "AAAABHSJ" varchar(1) NOT NULL default ' ',
  "AAAABHSK" varchar(1) NOT NULL default ' ',
  "AAAAWHJR" varchar(1) NOT NULL default ' ',
  "AAAABHSM" varchar(1) NOT NULL default ' ',
  "AAAABHSN" varchar(3) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAATQJR" decimal(13,2) NOT NULL default '0.00',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABLGEL" varchar(1) NOT NULL default ' ',
  "AAAABHSS" varchar(1) NOT NULL default ' ',
  "AAABMCZL" varchar(12) NOT NULL default ' ',
  "AAABMCZM" varchar(12) NOT NULL default ' ',
  "AAAAWGNB" varchar(30) NOT NULL default ' ',
  "AAAABHSW" varchar(10) NOT NULL default ' ',
  "AAAAXUBJ" varchar(8) NOT NULL default '00000000',
  "AAABLXIT" varchar(20) NOT NULL default ' ',
  "AAAABHSZ" varchar(1) NOT NULL default ' ',
  "AAABJDQB" varchar(1) NOT NULL default ' ',
  "AAAABHTB" varchar(1) NOT NULL default ' ',
  "AAAABHTC" decimal(13,2) NOT NULL default '0.00',
  "AAAAZDHM" varchar(6) NOT NULL default ' ',
  "AAABMDAG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMPWP","AAABLZBN","AAAAMOKP","AAAABHUE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABHTL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABLZBN" varchar(10) NOT NULL default ' ',
  "AAAAMOKP" varchar(16) NOT NULL default ' ',
  "AAAABHUE" varchar(5) NOT NULL default '00000',
  "AAAABHUF" varchar(5) NOT NULL default '00000',
  "AAAABHTT" decimal(13,2) NOT NULL default '0.00',
  "AAAABHTU" decimal(13,2) NOT NULL default '0.00',
  "AAABMCWO" varchar(3) NOT NULL default ' ',
  "AAABMCZL" varchar(12) NOT NULL default ' ',
  "AAABMCZM" varchar(12) NOT NULL default ' ',
  "AAAABHTY" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMPWP","AAABLZBN","AAAAMOKP","AAAABHUE","AAAABHUF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABHUG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABLZBN" varchar(10) NOT NULL default ' ',
  "AAAAMOKP" varchar(16) NOT NULL default ' ',
  "AAABLSIN" varchar(3) NOT NULL default '000',
  "AAAAYCIW" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMPWP","AAABLZBN","AAAAMOKP","AAABLSIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABHUU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIO" varchar(2) NOT NULL default ' ',
  "AAABCDPU" varchar(2) NOT NULL default ' ',
  "AAAABHVL" varchar(12) NOT NULL default ' ',
  "AAAABHVM" varchar(50) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABJHFY" varchar(12) NOT NULL default ' ',
  "AAABLNUW" varchar(8) NOT NULL default '00000000',
  "AAAABHVE" varchar(6) NOT NULL default '000000',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABMCNJ" varchar(1) NOT NULL default ' ',
  "AAAABHVH" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABLLIO","AAABCDPU","AAAABHVL","AAAABHVM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABHVN" (
  "AAAABHVS" varchar(8) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABMPMH" varchar(32) NOT NULL default ' ',
  "AAAABHVR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAABHVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABHVT" (
  "AAAAMJSD" varchar(12) NOT NULL default ' ',
  "AAABLLIO" varchar(10) NOT NULL default ' ',
  "AAABMPMH" varchar(32) NOT NULL default ' ',
  "AAAAVPWB" varchar(1) NOT NULL default ' ',
  "AAAABHVY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAMJSD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABHWA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLXYU" varchar(100) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABLXYU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAAAOQWU" varchar(10) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMQII","AAAAOQWU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUIA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABLZNI" varchar(4) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMQII","AAABLZNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUID" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMQII","AAABMLHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUIF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMLHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUIG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMAMV" varchar(4) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMAMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUIH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLZZR" varchar(4) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABLZZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUII" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABLYRW" varchar(6) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMLEZ","AAABLYRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUIJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMAMV" varchar(4) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMQII","AAABMAMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUIK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABLZZR" varchar(4) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMQII","AAABLZZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUIL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMQII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUIM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMLEP" varchar(1) NOT NULL default ' ',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMLEP","AAABMLEZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUIN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMCZV" varchar(4) NOT NULL default ' ',
  "AAABLXJT" varchar(3) NOT NULL default ' ',
  "AAABLXJU" varchar(3) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMCZV","AAABLXJT","AAABLXJU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUIO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLNUI" varchar(4) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABLNUI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUIP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLZFC" varchar(4) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABLZFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUIQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMQII","AAABMAMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUIR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMQII","AAABMJXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUIS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABJUPW" varchar(4) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABJUPW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUIT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLZZR" varchar(4) NOT NULL default ' ',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABLZZR","AAABMLEZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUIU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABLZFC" varchar(4) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABLZFD","AAABLZFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUIV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABKYBZ" varchar(1) NOT NULL default ' ',
  "AAABKYJI" varchar(10) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABLZFD","AAABKYBZ","AAABKYJI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMLEZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIFQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABLUFD","AAABLURS","AAABMCSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIGE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABLUFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIGO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABLURS","AAABMCSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIHA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLYAT" varchar(10) NOT NULL default ' ',
  "AAABLBIH" varchar(18) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABLYAT","AAABLBIH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIHM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABDKSS" varchar(2) NOT NULL default '00',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABDKSS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIHW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABLTKC","AAABLXDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIII" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABDKSI" varchar(2) NOT NULL default '00',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLURO" varchar(3) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABDKSI","AAABMCVI","AAABLURO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABDKSI" varchar(2) NOT NULL default '00',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDVOC" varchar(3) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABDKSI","AAABMCVI","AAABDVOC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIJK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIJU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABEDBS" varchar(2) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABIMRD","AAABEDBS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLZFC" varchar(4) NOT NULL default ' ',
  "AAABKYJI" varchar(10) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABLZFC","AAABKYJI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIKS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCWF" varchar(1) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMREX","AAABMCWF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABILE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMREX","AAABMCSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABILQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIMA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAAAVDYP" varchar(1) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAAAVDYP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIMK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABGPLQ" varchar(2) NOT NULL default ' ',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLKLM" varchar(3) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABGPLQ","AAABLLIM","AAABLKLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIMY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABJFDW" varchar(2) NOT NULL default ' ',
  "AAAAMUQQ" varchar(1) NOT NULL default ' ',
  "AAAAWOZU" varchar(1) NOT NULL default ' ',
  "AAAAZULA" varchar(2) NOT NULL default ' ',
  "AAAAZULF" varchar(2) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABJFDW","AAAAMUQQ","AAAAWOZU","AAAAZULA","AAAAZULF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABINQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAAAVUIC" varchar(1) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAAAVUIC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIOA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABJFDW" varchar(2) NOT NULL default ' ',
  "AAAAMUQQ" varchar(1) NOT NULL default ' ',
  "AAAAWOZU" varchar(1) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABJFDW","AAAAMUQQ","AAAAWOZU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIOO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABJFDW" varchar(2) NOT NULL default ' ',
  "AAAAMUQQ" varchar(1) NOT NULL default ' ',
  "AAAAWOZU" varchar(1) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABJFDW","AAAAMUQQ","AAAAWOZU","AAABMPLS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIPE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAAAMWJX" varchar(1) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAAAMWJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIPO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAAAMWKX" varchar(1) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAAAMWKX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABLUFD","AAABLURS","AAABLXOB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIQM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAWCGS" varchar(4) NOT NULL default ' ',
  "AAAABIQZ" varchar(4) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMREX","AAAAWCGS","AAAABIQZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIRA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAAAPSFM" varchar(4) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMCVI","AAAAPSFM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIRM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAAAPSFM" varchar(4) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMCVI","AAAAPSFM","AAABMCSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABISA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMLHQ","AAABMQII","AAABMPLT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABISO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAAASDTB" varchar(10) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABLURS","AAAASDTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABITA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABLXYO" varchar(10) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMQII","AAABMJXV","AAABMJXW","AAABLXYO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABITQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLJCW" varchar(10) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABLURS","AAABLJCW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIUC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABLWHI" varchar(10) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMQII","AAABMJXV","AAABMJXW","AAABLWHI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIUS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLJCW" varchar(10) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMREX","AAABLJCW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIVE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLWHI" varchar(10) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMREX","AAABLWHI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIVQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABJLEO" varchar(1) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABJLEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIWA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMALG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIWK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAAAPVUX" varchar(1) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMALG","AAAAPVUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIWW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMELB" varchar(2) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMELB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIXG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMELB" varchar(2) NOT NULL default ' ',
  "AAAAPVUX" varchar(1) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMELB","AAAAPVUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIXS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAAABIYB" varchar(3) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAAABIYB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIYC" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABIZYS" varchar(20) NOT NULL default ' ',
  "AAABGWQE" varchar(6) NOT NULL default '000000',
  "AAAABIYL" varchar(6) NOT NULL default '000000',
  "AAAABIYH" VARBINARY(255),
  PRIMARY KEY ("AAABMONG","AAABIZYS","AAABGWQE","AAAABIYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIYM" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRFC" varchar(22) NOT NULL default ' ',
  "AAABJLFF" varchar(6) NOT NULL default '000000',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMONG","AAABLUNI","AAABLRFC","AAABJLFF","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIYZ" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABLRFC" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABLRFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABIZE" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABIZYS" varchar(20) NOT NULL default ' ',
  "AAABMKFH" varchar(20) NOT NULL default ' ',
  "AAABLRFA" varchar(20) NOT NULL default ' ',
  "AAAARMUT" varchar(100) NOT NULL default ' ',
  "AAAAFWDW" varchar(8) NOT NULL default '00000000',
  "AAAABIZL" varchar(6) NOT NULL default '000000',
  "AAABEUQW" varchar(12) NOT NULL default ' ',
  "AAAABIZN" varchar(20) NOT NULL default ' ',
  "AAAABKAN" varchar(40) NOT NULL default ' ',
  "AAAABIZP" varchar(1) NOT NULL default ' ',
  "AAAABJBW" varchar(28) NOT NULL default ' ',
  "AAAADUWH" varchar(40) NOT NULL default ' ',
  "AAAADUWI" varchar(30) NOT NULL default ' ',
  "AAAAEWBC" varchar(1) NOT NULL default ' ',
  "AAAABKAO" varchar(8) NOT NULL default '00000000',
  "AAAAFWDX" varchar(1) NOT NULL default ' ',
  "AAAABIZW" varchar(1) NOT NULL default ' ',
  "AAAADUWG" varchar(1) NOT NULL default ' ',
  "AAAABIZY" varchar(8) NOT NULL default '00000000',
  "AAAABIZZ" varchar(6) NOT NULL default '000000',
  "AAAABJAA" varchar(12) NOT NULL default ' ',
  "AAABLRFC" varchar(22) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAABJAD" varchar(6) NOT NULL default '000000',
  "AAAABJAE" varchar(6) NOT NULL default '000000',
  "AAAABJAF" varchar(6) NOT NULL default '000000',
  "AAAABJAG" varchar(6) NOT NULL default '000000',
  "AAAABJAH" varchar(6) NOT NULL default '000000',
  "AAAABJAI" varchar(6) NOT NULL default '000000',
  "AAAABJAJ" varchar(6) NOT NULL default '000000',
  "AAAABJAK" decimal(15,0) NOT NULL default '0',
  "AAAABJAL" varchar(4) NOT NULL default ' ',
  "AAAABJAM" varchar(6) NOT NULL default '000000',
  "AAAABJAN" varchar(6) NOT NULL default '000000',
  "AAAABJAO" varchar(6) NOT NULL default '000000',
  "AAAABJAP" varchar(6) NOT NULL default '000000',
  "AAAABJAQ" varchar(3) NOT NULL default '000',
  "AAAABJAR" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMONG","AAABIZYS"),
  KEY "AAAABJAU" ("AAABMKFH","AAABLRFA"),
  KEY "AAAABJAX" ("AAABLRFC","AAABMONG"),
  KEY "AAAABJBA" ("AAAARMUT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJBC" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABIZYS" varchar(20) NOT NULL default ' ',
  "AAAATQTZ" varchar(6) NOT NULL default '000000',
  "AAABJECS" varchar(10) NOT NULL default ' ',
  "AAABHDKR" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABIZYS","AAAATQTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJBL" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABIZYS" varchar(20) NOT NULL default ' ',
  "AAABGWQE" varchar(6) NOT NULL default '000000',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  "AAAABJBW" varchar(28) NOT NULL default ' ',
  "AAAADUWH" varchar(40) NOT NULL default ' ',
  "AAAADUWI" varchar(30) NOT NULL default ' ',
  "AAAARTLU" varchar(1) NOT NULL default ' ',
  "AAAAEWBC" varchar(1) NOT NULL default ' ',
  "AAAABJCB" varchar(3) NOT NULL default ' ',
  "AAAADUWG" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAABJCE" varchar(6) NOT NULL default '000000',
  "AAAABJCF" decimal(21,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMONG","AAABIZYS","AAABGWQE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJCJ" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABIZYS" varchar(20) NOT NULL default ' ',
  "AAABGWQE" varchar(6) NOT NULL default '000000',
  "AAAATQTZ" varchar(6) NOT NULL default '000000',
  "AAABJECS" varchar(10) NOT NULL default ' ',
  "AAABHDKR" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABIZYS","AAABGWQE","AAAATQTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPOY" (
  "AAABMKFH" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJCX" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMKFH" varchar(20) NOT NULL default ' ',
  "AAAALNVJ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJDD" (
  "AAABMKFH" varchar(20) NOT NULL default ' ',
  "AAABLRFA" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABLRFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJDI" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMKFH" varchar(20) NOT NULL default ' ',
  "AAABLRFA" varchar(20) NOT NULL default ' ',
  "AAAABJDM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMKFH","AAABLRFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJDQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAABJDZ" varchar(22) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAAABJDZ","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJEB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABIZYS" varchar(20) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABIZYS","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJEM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLUVE" varchar(2) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABJTAJ" varchar(6) NOT NULL default '000000',
  "AAABHKSD" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUXQ","AAABLUVE","AAABLESS","AAABKOUD","AAABJTAJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJEY" (
  "AAABLECD" varchar(2) NOT NULL default ' ',
  "AAABMCNO" varchar(5) NOT NULL default ' ',
  "AAAABJFB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABMCNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJFE" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLECD" varchar(2) NOT NULL default ' ',
  "AAABMCNO" varchar(5) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLECD","AAABMCNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJFM" (
  "AAABHMRI" varchar(5) NOT NULL default '00000',
  "AAABLECD" varchar(2) NOT NULL default ' ',
  "AAAABJFP" varchar(5) NOT NULL default '00000',
  "AAAABJFQ" varchar(1) NOT NULL default ' ',
  "AAAAUPXH" varchar(1) NOT NULL default ' ',
  "AAABLPJD" varchar(3) NOT NULL default ' ',
  "AAAAQVJS" varchar(3) NOT NULL default ' ',
  "AAABMCNO" varchar(5) NOT NULL default ' ',
  "AAABLUNJ" varchar(5) NOT NULL default ' ',
  "AAAABJJJ" varchar(2) NOT NULL default '00',
  "AAABHFMV" varchar(2) NOT NULL default ' ',
  "AAAABJFY" varchar(20) NOT NULL default ' ',
  "AAAABJFZ" varchar(3) NOT NULL default ' ',
  "AAAABJIK" varchar(30) NOT NULL default ' ',
  "AAABLMNC" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABHMRI","AAABLECD"),
  KEY "AAAABJGE" ("AAABLECD","AAABMCNO","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJGI" (
  "AAABHMRI" varchar(5) NOT NULL default '00000',
  "AAABLECD" varchar(2) NOT NULL default ' ',
  "AAAABJHF" varchar(30) NOT NULL default ' ',
  "AAAATPLO" varchar(73) NOT NULL default ' ',
  "AAAABJHH" varchar(30) NOT NULL default ' ',
  "AAAABJGO" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABHMRI","AAABLECD","AAAABJHF","AAAATPLO","AAAABJHH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJGU" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHMRI" varchar(5) NOT NULL default '00000',
  "AAABLECD" varchar(2) NOT NULL default ' ',
  "AAAABJHF" varchar(30) NOT NULL default ' ',
  "AAAATPLO" varchar(73) NOT NULL default ' ',
  "AAAABJHH" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABHMRI","AAABLECD","AAAABJHF","AAAATPLO","AAAABJHH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJHI" (
  "AAABLECD" varchar(2) NOT NULL default ' ',
  "AAABMCNO" varchar(5) NOT NULL default ' ',
  "AAABLUNJ" varchar(5) NOT NULL default ' ',
  "AAAABJJJ" varchar(2) NOT NULL default '00',
  "AAABLPJD" varchar(3) NOT NULL default ' ',
  "AAAABJHO" varchar(2) NOT NULL default '00',
  "AAAABJHP" varchar(1) NOT NULL default ' ',
  "AAAABJHQ" varchar(1) NOT NULL default ' ',
  "AAAABJHR" varchar(3) NOT NULL default '000',
  "AAABKGXT" varchar(30) NOT NULL default ' ',
  "AAABKGXU" varchar(30) NOT NULL default ' ',
  "AAAABJHU" varchar(30) NOT NULL default ' ',
  "AAAABJHV" varchar(60) NOT NULL default ' ',
  "AAAABJHW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABMCNO","AAABLUNJ","AAAABJJJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJIB" (
  "AAABLECD" varchar(2) NOT NULL default ' ',
  "AAABMCNO" varchar(5) NOT NULL default ' ',
  "AAABLUNJ" varchar(5) NOT NULL default ' ',
  "AAABAWWL" varchar(2) NOT NULL default '00',
  "AAAABJJJ" varchar(2) NOT NULL default '00',
  "AAAABJIH" varchar(1) NOT NULL default ' ',
  "AAAABJUR" varchar(3) NOT NULL default '000',
  "AAABFUQE" tinyint(3) unsigned NOT NULL default '0',
  "AAAABJIK" varchar(30) NOT NULL default ' ',
  "AAAABJIL" varchar(5) NOT NULL default ' ',
  "AAAABJIM" varchar(5) NOT NULL default ' ',
  "AAAABJIN" varchar(10) NOT NULL default ' ',
  "AAABJDQT" varchar(1) NOT NULL default ' ',
  "AAABECCJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABMCNO","AAABLUNJ","AAABAWWL","AAAABJJJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJIV" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLECD" varchar(2) NOT NULL default ' ',
  "AAABMCNO" varchar(5) NOT NULL default ' ',
  "AAABLUNJ" varchar(5) NOT NULL default ' ',
  "AAABAWWL" varchar(2) NOT NULL default '00',
  "AAAABJJJ" varchar(2) NOT NULL default '00',
  "AAAABJJC" varchar(60) NOT NULL default ' ',
  "AAAABJJD" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLECD","AAABMCNO","AAABLUNJ","AAABAWWL","AAAABJJJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJJK" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHMRI" varchar(5) NOT NULL default '00000',
  "AAABLECD" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABHMRI","AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJJS" (
  "AAABHEIG" varchar(4) NOT NULL default ' ',
  "AAABLECD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEIG","AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJJX" (
  "AAABHEIG" varchar(4) NOT NULL default ' ',
  "AAABLECD" varchar(2) NOT NULL default ' ',
  "AAABHMRI" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABHEIG","AAABLECD","AAABHMRI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJKE" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHEIG" varchar(4) NOT NULL default ' ',
  "AAABLECD" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABHEIG","AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJKM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLRTV" varchar(8) NOT NULL default '00000000',
  "AAAAETWF" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLUXQ","AAABLRTV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJKS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAADWOG" VARBINARY(255) NOT NULL,
  "AAAADWOH" varchar(10) NOT NULL default '0000000000',
  "AAAACBEU" varchar(40) NOT NULL default ' ',
  "AAAACBEV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAADWOG"(255),"AAAADWOH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJLB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAABJLU" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAABJLF" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAABJLU","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJLJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAADWOG" VARBINARY(255) NOT NULL,
  "AAAADWOH" varchar(10) NOT NULL default '0000000000',
  "AAAABJLN" VARBINARY(255),
  PRIMARY KEY ("AAABLUXQ","AAAADWOG"(255),"AAAADWOH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJLR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAADWOG" VARBINARY(255) NOT NULL,
  "AAAABJLU" varchar(30) NOT NULL default ' ',
  "AAAABJLV" varchar(1) NOT NULL default ' ',
  "AAABCOLM" varchar(10) NOT NULL default ' ',
  "AAAABJLX" varchar(200) NOT NULL default ' ',
  "AAAADWPA" varchar(1) NOT NULL default ' ',
  "AAAABJLZ" int(11) NOT NULL default '0',
  "AAAABJMA" varchar(200) NOT NULL default ' ',
  "AAAABKET" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAADWOG"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJME" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAADWOG" VARBINARY(255) NOT NULL,
  "AAABCOKA" varchar(4) NOT NULL default ' ',
  "AAABLCOX" varchar(40) NOT NULL default ' ',
  "AAAADWOH" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAAADWOG"(255),"AAABCOKA","AAABLCOX"),
  KEY "AAAABJMO" ("AAABMRIL","AAAADWOG"(255),"AAAADWOH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJMS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAADWOG" VARBINARY(255) NOT NULL,
  "AAAABJUK" varchar(10) NOT NULL default '0000000000',
  "AAAABJRK" varchar(1) NOT NULL default ' ',
  "AAAABJRL" varchar(200) NOT NULL default ' ',
  "AAABIJQE" varchar(50) NOT NULL default ' ',
  "AAAABJRN" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABLUXQ","AAAADWOG"(255),"AAAABJUK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJND" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAADWOG" VARBINARY(255) NOT NULL,
  "AAAABJUR" varchar(10) NOT NULL default '0000000000',
  "AAABLRRL" varchar(45) NOT NULL default ' ',
  "AAAABJUK" varchar(10) NOT NULL default '0000000000',
  "AAAABJUL" varchar(10) NOT NULL default '0000000000',
  "AAAABJUM" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAAADWOG"(255),"AAAABJUR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJNO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABEPPX" varchar(60) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAAADWOG" VARBINARY(255),
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABEPPX","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJOA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAADWOG" VARBINARY(255) NOT NULL,
  "AAABLITM" varchar(20) NOT NULL default ' ',
  "AAAABKDU" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAAADWOG"(255),"AAABLITM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJOI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEQVP" varchar(40) NOT NULL default ' ',
  "AAAAUZAL" varchar(8) NOT NULL default '00000000',
  "AAAADWQD" varchar(3) NOT NULL default '000',
  "AAAABJON" varchar(8) NOT NULL default '00000000',
  "AAAADWPA" varchar(1) NOT NULL default ' ',
  "AAAABJOP" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABEQVP","AAAAUZAL","AAAADWQD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEQVP" varchar(40) NOT NULL default ' ',
  "AAAAUZAL" varchar(8) NOT NULL default '00000000',
  "AAAADWQD" varchar(3) NOT NULL default '000',
  "AAABJOTS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEQVP","AAAAUZAL","AAAADWQD","AAABJOTS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJPF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAADWOG" VARBINARY(255) NOT NULL,
  "AAAADWOH" varchar(10) NOT NULL default '0000000000',
  "AAABJOTS" varchar(4) NOT NULL default ' ',
  "AAAABJPK" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAADWOG"(255),"AAAADWOH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJPO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCOKA" varchar(4) NOT NULL default ' ',
  "AAABLCOX" varchar(40) NOT NULL default ' ',
  "AAAABKFH" varchar(10) NOT NULL default ' ',
  "AAAABKFV" varchar(4) NOT NULL default ' ',
  "AAAABKFW" varchar(40) NOT NULL default ' ',
  "AAAABJPV" VARBINARY(255),
  "AAAABJPW" varchar(100) NOT NULL default ' ',
  "AAABCIJC" varchar(4) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  "AAAABKFQ" decimal(21,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAABCOKA","AAABLCOX","AAAABKFH","AAAABKFV","AAAABKFW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJQM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAQCIC" VARBINARY(255) NOT NULL,
  "AAABJOTS" varchar(4) NOT NULL default ' ',
  "AAAABJQQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAQCIC"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJQT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABEPPX" varchar(60) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABEPPX","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJRE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABEQVP" varchar(40) NOT NULL default ' ',
  "AAAAUZAL" varchar(8) NOT NULL default '00000000',
  "AAAADWQD" varchar(3) NOT NULL default '000',
  "AAAABJUK" varchar(10) NOT NULL default '0000000000',
  "AAAABJRK" varchar(1) NOT NULL default ' ',
  "AAAABJRL" varchar(200) NOT NULL default ' ',
  "AAABIJQE" varchar(50) NOT NULL default ' ',
  "AAAABJRN" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABLUXQ","AAABEQVP","AAAAUZAL","AAAADWQD","AAAABJUK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJRT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEQVP" varchar(40) NOT NULL default ' ',
  "AAAAUZAL" varchar(8) NOT NULL default '00000000',
  "AAAADWQD" varchar(3) NOT NULL default '000',
  "AAAACBKP" varchar(1) NOT NULL default ' ',
  "AAAACBLF" varchar(3) NOT NULL default '000',
  "AAAABJSA" varchar(3) NOT NULL default '000',
  "AAAABJSB" varchar(3) NOT NULL default '000',
  "AAAACBLH" varchar(3) NOT NULL default '000',
  "AAAABJSD" tinyint(3) unsigned NOT NULL default '0',
  "AAAABJSE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEQVP","AAAAUZAL","AAAADWQD","AAAACBKP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJSK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEQVP" varchar(40) NOT NULL default ' ',
  "AAAAUZAL" varchar(8) NOT NULL default '00000000',
  "AAAADWQD" varchar(3) NOT NULL default '000',
  "AAAABJTN" VARBINARY(255),
  "AAABCOLM" varchar(10) NOT NULL default ' ',
  "AAAABJSR" varchar(200) NOT NULL default ' ',
  "AAAACBKW" varchar(3) NOT NULL default '000',
  "AAAACBKX" varchar(3) NOT NULL default '000',
  "AAAAUFOE" varchar(12) NOT NULL default ' ',
  "AAABCRWQ" varchar(8) NOT NULL default '00000000',
  "AAABIJNY" varchar(6) NOT NULL default '000000',
  "AAAACBLB" decimal(15,0) NOT NULL default '0',
  "AAAABJSY" varchar(1) NOT NULL default ' ',
  "AAABKAGJ" varchar(100) NOT NULL default ' ',
  "AAAACBLF" varchar(3) NOT NULL default '000',
  "AAAACBLG" varchar(3) NOT NULL default '000',
  "AAAACBLH" varchar(3) NOT NULL default '000',
  "AAAAXEKY" VARBINARY(255),
  "AAAABKBX" VARBINARY(255),
  "AAAABJTQ" varchar(100) NOT NULL default ' ',
  "AAAABJTU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEQVP","AAAAUZAL","AAAADWQD"),
  UNIQUE KEY "AAAABJTL" ("AAABMRIL","AAAABJTN"(255)),
  KEY "AAAABJTO" ("AAABMRIL","AAAABJTQ"),
  KEY "AAAABJTR" ("AAABMRIL","AAABCOLM","AAAABJTU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEQVP" varchar(40) NOT NULL default ' ',
  "AAAAUZAL" varchar(8) NOT NULL default '00000000',
  "AAAADWQD" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABEQVP","AAAAUZAL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJUD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABEQVP" varchar(40) NOT NULL default ' ',
  "AAAAUZAL" varchar(8) NOT NULL default '00000000',
  "AAAADWQD" varchar(3) NOT NULL default '000',
  "AAAABJUR" varchar(10) NOT NULL default '0000000000',
  "AAABLRRL" varchar(45) NOT NULL default ' ',
  "AAAABJUK" varchar(10) NOT NULL default '0000000000',
  "AAAABJUL" varchar(10) NOT NULL default '0000000000',
  "AAAABJUM" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABEQVP","AAAAUZAL","AAAADWQD","AAAABJUR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJUS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABEPPX" varchar(60) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABEPPX","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJVD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJLTX" varchar(40) NOT NULL default ' ',
  "AAABJSLF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJLTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJVJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKHVH" varchar(40) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABHMMT" varchar(40) NOT NULL default ' ',
  "AAAACBMQ" varchar(1) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAAAGKJE" varchar(32) NOT NULL default ' ',
  "AAAACBMT" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKHVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGHFI" varchar(40) NOT NULL default ' ',
  "AAAACBNC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGHFI","AAAACBNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJWB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABEPPX" varchar(80) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABEPPX","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJWM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABEQVP" varchar(40) NOT NULL default ' ',
  "AAAAUZAL" varchar(8) NOT NULL default '00000000',
  "AAAADWQD" varchar(3) NOT NULL default '000',
  "AAAACBAP" varchar(3) NOT NULL default '000',
  "AAABLRFC" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABEQVP","AAAAUZAL","AAAADWQD","AAAACBAP","AAABLRFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJWZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEQVP" varchar(40) NOT NULL default ' ',
  "AAAAUZAL" varchar(8) NOT NULL default '00000000',
  "AAAADWQD" varchar(3) NOT NULL default '000',
  "AAAAVSPT" varchar(3) NOT NULL default '000',
  "AAAACBRM" VARBINARY(255),
  "AAAADWOG" VARBINARY(255),
  "AAABDUBG" VARBINARY(255),
  "AAABCEDB" varchar(12) NOT NULL default ' ',
  "AAABLRTW" varchar(15) NOT NULL default '000000000000000',
  PRIMARY KEY ("AAABMRIL","AAABEQVP","AAAAUZAL","AAAADWQD","AAAAVSPT"),
  KEY "AAAABJXP" ("AAABDUBG"(255)),
  KEY "AAAABJXR" ("AAAACBRM"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJXT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEQVP" varchar(40) NOT NULL default ' ',
  "AAAAUZAL" varchar(8) NOT NULL default '00000000',
  "AAAADWQD" varchar(3) NOT NULL default '000',
  "AAAABJXY" varchar(1) NOT NULL default ' ',
  "AAAABJXZ" varchar(1) NOT NULL default ' ',
  "AAAABJYA" varchar(1) NOT NULL default ' ',
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
  PRIMARY KEY ("AAABMRIL","AAABEQVP","AAAAUZAL","AAAADWQD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJZD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKHVH" VARBINARY(255) NOT NULL,
  "AAABEQVP" varchar(40) NOT NULL default ' ',
  "AAAAUZAL" varchar(8) NOT NULL default '00000000',
  "AAAADWQD" varchar(3) NOT NULL default '000',
  "AAAABJZJ" varchar(1) NOT NULL default ' ',
  "AAABCOIC" int(11) NOT NULL default '0',
  "AAAABJZL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKHVH"(255)),
  KEY "AAAABJZO" ("AAABEQVP","AAAADWQD","AAAAUZAL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABJZS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAABKBX" VARBINARY(255) NOT NULL,
  "AAAABKAF" varchar(50) NOT NULL default ' ',
  "AAAABJZW" varchar(20) NOT NULL default '00000000000000000000',
  "AAAABKBL" varchar(15) NOT NULL default '000000000000000',
  "AAAAREOG" varchar(12) NOT NULL default ' ',
  "AAAABJZZ" int(11) NOT NULL default '0',
  "AAAABKAA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAABKBX"(255)),
  KEY "AAAABKAD" ("AAABMRIL","AAAABKAF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKAG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAXEKY" VARBINARY(255) NOT NULL,
  "AAAABKAS" varchar(3) NOT NULL default '000',
  "AAABMKFH" varchar(20) NOT NULL default ' ',
  "AAABLRFA" varchar(20) NOT NULL default ' ',
  "AAAARMUT" varchar(100) NOT NULL default ' ',
  "AAAABKAN" varchar(40) NOT NULL default ' ',
  "AAAABKAO" varchar(8) NOT NULL default '00000000',
  "AAAABKAP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAXEKY"(255),"AAAABKAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKAT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXEKY" VARBINARY(255) NOT NULL,
  "AAABETOI" varchar(4) NOT NULL default ' ',
  "AAABCINI" varchar(50) NOT NULL default ' ',
  "AAAABKBX" VARBINARY(255),
  "AAABHMMT" VARBINARY(255),
  "AAAABKBA" varchar(1) NOT NULL default ' ',
  "AAAAMYFC" varchar(6) NOT NULL default '000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAABKBD" int(11) NOT NULL default '0',
  "AAABCINH" varchar(1) NOT NULL default ' ',
  "AAAABKBF" varchar(1) NOT NULL default ' ',
  "AAABCOMB" varchar(1) NOT NULL default ' ',
  "AAAABKBH" varchar(1) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAAACBMT" varchar(40) NOT NULL default ' ',
  "AAAABKBL" varchar(15) NOT NULL default '000000000000000',
  "AAAAREOG" varchar(12) NOT NULL default ' ',
  "AAAABKBN" varchar(3) NOT NULL default '000',
  "AAAABKBO" int(11) NOT NULL default '0',
  "AAAABKBP" varchar(1) NOT NULL default ' ',
  "AAABKAHC" varchar(8) NOT NULL default ' ',
  "AAAABKBR" varchar(40) NOT NULL default ' ',
  "AAAABKBS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXEKY"(255)),
  KEY "AAAABKBV" ("AAABMRIL","AAAABKBX"(255),"AAABCINI"),
  KEY "AAAABKBZ" ("AAABMRIL","AAABHMMT"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKCC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXEKY" VARBINARY(255) NOT NULL,
  "AAAABKCL" VARBINARY(255) NOT NULL,
  PRIMARY KEY ("AAABMRIL","AAAAXEKY"(255),"AAAABKCL"(255)),
  KEY "AAAABKCJ" ("AAABMRIL","AAAABKCL"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKCM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXEKY" VARBINARY(255) NOT NULL,
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABIMZU" varchar(14) NOT NULL default ' ',
  "AAAABKCR" varchar(1) NOT NULL default ' ',
  "AAABEQVP" varchar(40) NOT NULL default ' ',
  "AAAAUZAL" varchar(8) NOT NULL default '00000000',
  "AAAADWQD" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAAAXEKY"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKCX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXEKY" VARBINARY(255) NOT NULL,
  "AAABJGOK" varchar(8) NOT NULL default ' ',
  "AAABMNLA" varchar(1) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(45) NOT NULL default ' ',
  "AAABMNNA" varchar(45) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABJSDF" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAXEKY"(255),"AAABJGOK","AAABMNLA","AAABMNML","AAABIJIG","AAABMNMZ","AAABMNNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKDQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAACBRM" VARBINARY(255) NOT NULL,
  "AAABLITM" varchar(40) NOT NULL default ' ',
  "AAAABKDU" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAAACBRM"(255),"AAABLITM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKDY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAACBRM" VARBINARY(255) NOT NULL,
  "AAABLITM" varchar(40) NOT NULL default ' ',
  "AAAACDFA" varchar(2) NOT NULL default ' ',
  "AAAABKED" int(11) NOT NULL default '0',
  "AAAABKEE" int(11) NOT NULL default '0',
  "AAAABKEF" int(11) NOT NULL default '0',
  "AAAABKGS" int(11) NOT NULL default '0',
  "AAAABKEH" int(11) NOT NULL default '0',
  "AAAABKEI" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAACBRM"(255),"AAABLITM","AAAACDFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKEN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAACBRM" VARBINARY(255) NOT NULL,
  "AAAADWOG" VARBINARY(255),
  "AAAABKER" varchar(1) NOT NULL default ' ',
  "AAAABKES" varchar(1) NOT NULL default ' ',
  "AAAABKET" varchar(1) NOT NULL default ' ',
  "AAAABKEU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAACBRM"(255)),
  KEY "AAAABKEX" ("AAABMRIL","AAAADWOG"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKFA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCOKA" varchar(4) NOT NULL default ' ',
  "AAABLCOX" varchar(40) NOT NULL default ' ',
  "AAAABKFU" VARBINARY(255) NOT NULL,
  "AAAABKFV" varchar(4) NOT NULL default ' ',
  "AAAABKFW" varchar(40) NOT NULL default ' ',
  "AAAABKFH" varchar(10) NOT NULL default ' ',
  "AAABCIJC" varchar(4) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  "AAAABKFQ" decimal(21,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAABCOKA","AAABLCOX","AAAABKFU"(255),"AAAABKFV","AAAABKFW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKFX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAACBRM" VARBINARY(255) NOT NULL,
  "AAABCOKA" varchar(4) NOT NULL default ' ',
  "AAABLCOX" varchar(40) NOT NULL default ' ',
  "AAAADWOH" varchar(10) NOT NULL default '0000000000',
  "AAAAETNQ" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAACBRM"(255),"AAABCOKA","AAABLCOX"),
  KEY "AAAABKGI" ("AAABMRIL","AAAACBRM"(255),"AAAADWOH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKGM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAACBRM" VARBINARY(255) NOT NULL,
  "AAAADWOH" varchar(10) NOT NULL default '0000000000',
  "AAAABKGQ" int(11) NOT NULL default '0',
  "AAAABKGR" int(11) NOT NULL default '0',
  "AAAABKGS" int(11) NOT NULL default '0',
  "AAAABKHA" varchar(1) NOT NULL default ' ',
  "AAAABKGU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAACBRM"(255),"AAAADWOH"),
  KEY "AAAABKGY" ("AAAACBRM"(255),"AAAABKHA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKHB" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAABKHI" varchar(4) NOT NULL default '0000',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABMMRH" varchar(4) NOT NULL default '0000',
  "AAABMMJT" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEN","AAAABKHI","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKHK" (
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABLUEO" varchar(32) NOT NULL default ' ',
  "AAAAQZIB" varchar(30) NOT NULL default ' ',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPJD","AAABLUEO","AAAAQZIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKHS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAAABKHV" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKHY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATKCB" varchar(10) NOT NULL default ' ',
  "AAAATKBC" varchar(8) NOT NULL default '00000000',
  "AAAATKBD" varchar(255) NOT NULL default ' ',
  "AAAATKBE" tinyint(3) unsigned NOT NULL default '0',
  "AAAATKBF" smallint(6) NOT NULL default '0',
  "AAAATKBG" int(11) NOT NULL default '0',
  "AAAATKBH" decimal(3,0) NOT NULL default '0',
  "AAAATKBI" varchar(3) NOT NULL default ' ',
  "AAAATKBJ" varchar(10) NOT NULL default ' ',
  "AAAATKBK" decimal(10,2) NOT NULL default '0.00',
  "AAAATKBL" varchar(5) NOT NULL default ' ',
  "AAAATKBM" decimal(14,3) NOT NULL default '0.000',
  "AAAATKBN" decimal(31,0) NOT NULL default '0',
  "AAAATKBO" double NOT NULL default '0',
  "AAAATKBP" varchar(6) NOT NULL default '000000',
  "AAAATKBQ" decimal(2,0) NOT NULL default '0',
  "AAAATKBR" varchar(4) NOT NULL default '0000',
  "AAAATKBS" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAAATKCB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKIU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATKCB" varchar(10) NOT NULL default ' ',
  "AAAATKCC" varchar(20) NOT NULL default ' ',
  "AAAATKBZ" varchar(20) NOT NULL default '00000000000000000000',
  PRIMARY KEY ("AAABMRIL","AAAATKCB","AAAATKCC","AAAATKBZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKJD" (
  "AAABLUSV" varchar(6) NOT NULL default '000000',
  "AAABKNBT" varchar(10) NOT NULL default ' ',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMFSV" varchar(4) NOT NULL default '0000',
  "AAAABKJI" varchar(40) NOT NULL default ' ',
  "AAAABKJJ" varchar(40) NOT NULL default ' ',
  "AAABLTXO" varchar(46) NOT NULL default ' ',
  "AAAABKJL" varchar(1) NOT NULL default ' ',
  "AAABJDQU" varchar(1) NOT NULL default ' ',
  "AAAABKJN" varchar(1) NOT NULL default ' ',
  "AAAABKJO" varchar(1) NOT NULL default ' ',
  "AAAABKJP" varchar(1) NOT NULL default ' ',
  "AAAABKJQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABKNBT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKJT" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABLUNJ" varchar(16) NOT NULL default ' ',
  "AAABLEKT" VARBINARY(255),
  "AAAABKJY" varchar(1) NOT NULL default ' ',
  "AAABLGEF" tinyint(3) unsigned NOT NULL default '0',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAAABKKB" varchar(8) NOT NULL default '00000000',
  "AAABLRLF" varchar(8) NOT NULL default '00000000',
  "AAAABKKD" tinyint(3) unsigned NOT NULL default '0',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAAABKKG" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMONG","AAABLPJD","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKKK" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABLUNJ" varchar(16) NOT NULL default ' ',
  "AAABLPYC" varchar(8) NOT NULL default '00000000',
  "AAABLPYD" varchar(6) NOT NULL default '000000',
  "AAABLPYE" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABLPJD","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKKU" (
  "AAABIGIZ" varchar(24) NOT NULL default ' ',
  "AAABHZBN" varchar(14) NOT NULL default '00000000000000',
  "AAABHZAQ" varchar(16) NOT NULL default ' ',
  "AAABHZAR" varchar(16) NOT NULL default ' ',
  "AAABHZBP" varchar(12) NOT NULL default ' ',
  "AAABHZBR" varchar(16) NOT NULL default ' ',
  "AAABMMYV" varchar(3) NOT NULL default ' ',
  "AAABLTXI" varchar(10) NOT NULL default ' ',
  "AAABHZAW" varchar(24) NOT NULL default ' ',
  "AAABMKFH" varchar(50) NOT NULL default ' ',
  "AAABLITM" varchar(20) NOT NULL default ' ',
  "AAABHZAZ" varchar(1) NOT NULL default '0',
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
  PRIMARY KEY ("AAABIGIZ"),
  KEY "AAAABKLT" ("AAABHZBN"),
  KEY "AAAABKLV" ("AAABHZBP"),
  KEY "AAAABKLX" ("AAABHZBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKLZ" (
  "AAABIYZP" varchar(20) NOT NULL default ' ',
  "AAABDMLS" varchar(3) NOT NULL default '000',
  "AAAABKMC" varchar(3) NOT NULL default '000',
  "AAAATUNO" varchar(2) NOT NULL default '00',
  "AAABBZSR" varchar(1) NOT NULL default ' ',
  "AAAABKMF" varchar(1) NOT NULL default ' ',
  "AAABHHRK" varchar(1) NOT NULL default ' ',
  "AAAABKMH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIYZP","AAABDMLS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKMK" (
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABIKHY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKMO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDKXS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDKXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKMT" (
  "AAAABKOB" varchar(30) NOT NULL default ' ',
  "AAAABKOC" varchar(5) NOT NULL default '00000',
  "AAAABKMW" varchar(5) NOT NULL default '00000',
  "AAAABKMX" varchar(32) NOT NULL default ' ',
  "AAABCXGM" varchar(30) NOT NULL default ' ',
  "AAAABKMZ" varchar(10) NOT NULL default ' ',
  "AAAABKNA" smallint(6) NOT NULL default '0',
  "AAAABKNB" smallint(6) NOT NULL default '0',
  "AAAABKNC" smallint(6) NOT NULL default '0',
  "AAAABKND" varchar(5) NOT NULL default '00000',
  "AAAABKNE" varchar(32) NOT NULL default ' ',
  "AAAABKNF" varchar(30) NOT NULL default ' ',
  "AAAABKNG" varchar(10) NOT NULL default ' ',
  "AAAABKNH" smallint(6) NOT NULL default '0',
  "AAAABKNI" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAAABKOB","AAAABKOC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKNL" (
  "AAAABKOB" varchar(30) NOT NULL default ' ',
  "AAAABKOC" varchar(5) NOT NULL default '00000',
  "AAABLLLP" varchar(60) NOT NULL default ' ',
  "AAAABKNP" varchar(5) NOT NULL default '00000',
  "AAABMIRX" varchar(32) NOT NULL default ' ',
  "AAABFZAX" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAABKOB","AAAABKOC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKNU" (
  "AAAABKOB" varchar(30) NOT NULL default ' ',
  "AAAABKOC" varchar(5) NOT NULL default '00000',
  "AAABEETX" varchar(5) NOT NULL default '00000',
  "AAABLGDB" varchar(32) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLDNU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAABKOB","AAAABKOC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKOD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAABKSU" varchar(10) NOT NULL default ' ',
  "AAAABKSF" varchar(10) NOT NULL default ' ',
  "AAAABKSG" varchar(1) NOT NULL default ' ',
  "AAAABKSH" varchar(16) NOT NULL default '0000000000000000',
  "AAAABKOJ" varchar(10) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAABKSU","AAAABKSF","AAAABKSG","AAAABKSH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKOS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(32) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQDU","AAABLEHK","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKPC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(32) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABCRGY" varchar(12) NOT NULL default ' ',
  "AAABLPWW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQDU","AAABLHPM","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKPN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABKCWA" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKPV" (
  "AAAABKPX" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAAABKPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKPY" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAABKQD" varchar(2) NOT NULL default '00',
  "AAAABKQB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAABKQD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKQE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRYZ" varchar(20) NOT NULL default ' ',
  "AAABKWVJ" varchar(10) NOT NULL default ' ',
  "AAABLRTJ" varchar(12) NOT NULL default ' ',
  "AAAABKQJ" varchar(1) NOT NULL default ' ',
  "AAAABKQK" varchar(1) NOT NULL default ' ',
  "AAAAWCHE" varchar(1) NOT NULL default ' ',
  "AAAARKOT" varchar(2) NOT NULL default '00',
  "AAABLUEO" varchar(1) NOT NULL default ' ',
  "AAAAIVIB" varchar(1) NOT NULL default ' ',
  "AAAABKQP" varchar(1) NOT NULL default ' ',
  "AAAABKQQ" varchar(2) NOT NULL default ' ',
  "AAAABKQR" varchar(1) NOT NULL default ' ',
  "AAAABKQS" varchar(10) NOT NULL default ' ',
  "AAAABKQT" varchar(1) NOT NULL default ' ',
  "AAAABKQU" varchar(255) NOT NULL default ' ',
  "AAAABKQV" varchar(1) NOT NULL default ' ',
  "AAAABKQW" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABKRYZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKQZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKRYZ" varchar(20) NOT NULL default ' ',
  "AAAABKRD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKRYZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKRH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRYZ" varchar(20) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default '000',
  "AAABLRRK" varchar(20) NOT NULL default ' ',
  "AAABLRRL" varchar(132) NOT NULL default ' ',
  "AAABLRRM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRYZ","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKRR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRYZ" varchar(20) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default '000',
  "AAABLRRK" varchar(20) NOT NULL default ' ',
  "AAABLRRL" varchar(132) NOT NULL default ' ',
  "AAABLRRM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRYZ","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKSB" (
  "AAAABKSF" varchar(10) NOT NULL default ' ',
  "AAAABKSG" varchar(1) NOT NULL default ' ',
  "AAAABKSH" varchar(16) NOT NULL default '0000000000000000',
  PRIMARY KEY ("AAAABKSF","AAAABKSG","AAAABKSH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKSI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAABKSU" varchar(10) NOT NULL default ' ',
  "AAABMBXY" varchar(10) NOT NULL default ' ',
  "AAAAJOZE" varchar(10) NOT NULL default '0000000000',
  "AAAABKSX" varchar(10) NOT NULL default ' ',
  "AAAABKSY" varchar(5) NOT NULL default '00000',
  "AAAABKSZ" varchar(10) NOT NULL default '0000000000',
  "AAAABKTA" varchar(10) NOT NULL default '0000000000',
  "AAAABKTB" varchar(10) NOT NULL default '0000000000',
  "AAAABKTC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAABKSU","AAABMBXY","AAAAJOZE","AAAABKSX","AAAABKSY","AAAABKSZ","AAAABKTA","AAAABKTB","AAAABKTC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKTD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAABKTQ" varchar(10) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAABKTQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKTJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAABKTQ" varchar(10) NOT NULL default ' ',
  "AAAABKTN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAAABKTQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKTR" (
  "AAABLPNX" varchar(5) NOT NULL default ' ',
  "AAAAXFFU" varchar(1) NOT NULL default ' ',
  "AAAABKTY" varchar(30) NOT NULL default ' ',
  "AAAABKTV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPNX","AAAAXFFU","AAAABKTY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKTZ" (
  "AAAABKUC" varchar(7) NOT NULL default ' ',
  "AAAABKUD" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAAABKUC","AAAABKUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKUE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAAASNUS" varchar(12) NOT NULL default ' ',
  "AAABLRSP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAAASNUS"),
  KEY "AAAABKUM" ("AAABMRIL","AAABLRSP","AAABMCVI","AAAASNUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKUR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWVJ" varchar(10) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABHESL" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUXQ","AAABKWVJ"),
  KEY "AAAABKVA" ("AAABLUXQ","AAABMCSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKVD" (
  "AAAABKWS" varchar(20) NOT NULL default ' ',
  "AAAABKWP" varchar(20) NOT NULL default ' ',
  "AAAABKWQ" varchar(20) NOT NULL default ' ',
  "AAAABKWR" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAABKWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKVJ" (
  "AAAABKWS" varchar(20) NOT NULL default ' ',
  "AAAABKWP" varchar(20) NOT NULL default ' ',
  "AAAABKWQ" varchar(20) NOT NULL default ' ',
  "AAAABKWR" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAABKWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKVP" (
  "AAAABKWS" varchar(20) NOT NULL default ' ',
  "AAAABKWP" varchar(20) NOT NULL default ' ',
  "AAAABKWQ" varchar(20) NOT NULL default ' ',
  "AAAABKWR" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAABKWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKVV" (
  "AAAABKWS" varchar(20) NOT NULL default ' ',
  "AAAABKWP" varchar(20) NOT NULL default ' ',
  "AAAABKWQ" varchar(20) NOT NULL default ' ',
  "AAAABKWR" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAABKWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKWB" (
  "AAAABKWS" varchar(20) NOT NULL default ' ',
  "AAAABKWP" varchar(20) NOT NULL default ' ',
  "AAAABKWQ" varchar(20) NOT NULL default ' ',
  "AAAABKWR" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAABKWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKWH" (
  "AAAABKWS" varchar(20) NOT NULL default ' ',
  "AAAABKWP" varchar(20) NOT NULL default ' ',
  "AAAABKWQ" varchar(20) NOT NULL default ' ',
  "AAAABKWR" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAABKWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKWN" (
  "AAAABKWS" varchar(20) NOT NULL default ' ',
  "AAAABKWP" varchar(20) NOT NULL default ' ',
  "AAAABKWQ" varchar(20) NOT NULL default ' ',
  "AAAABKWR" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAABKWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKWT" (
  "AAAABMCD" varchar(7) NOT NULL default ' ',
  "AAAABKWV" varchar(15) NOT NULL default ' ',
  "AAAABKWW" varchar(15) NOT NULL default ' ',
  "AAAABKWX" varchar(5) NOT NULL default ' ',
  "AAAABKWY" varchar(5) NOT NULL default ' ',
  "AAAABMCI" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAAABMCD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKXB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAAABKXK" varchar(12) NOT NULL default '000000000000',
  "AAAABKXL" varchar(12) NOT NULL default '000000000000',
  "AAAABKXG" varchar(1) NOT NULL default ' ',
  "AAAABKXH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAAABKXK","AAAABKXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKXM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABLIMF" varchar(12) NOT NULL default '000000000000',
  "AAAACDAK" varchar(12) NOT NULL default '000000000000',
  "AAAABKXR" varchar(1) NOT NULL default ' ',
  "AAAABKXS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABLIMF","AAAACDAK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKXX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAAACDAK" varchar(12) NOT NULL default '000000000000',
  "AAAABYXZ" varchar(20) NOT NULL default ' ',
  "AAABCIXJ" varchar(10) NOT NULL default ' ',
  "AAAABLIE" varchar(3) NOT NULL default '000',
  "AAAACAUT" varchar(12) NOT NULL default '000000000000',
  "AAAABKYF" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAAACDAK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKYJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAABKZE" VARBINARY(255) NOT NULL,
  "AAABLEBM" varchar(15) NOT NULL default '000000000000000',
  "AAABCIXA" varchar(2) NOT NULL default ' ',
  "AAABCIUH" varchar(2) NOT NULL default ' ',
  "AAABCIUF" varchar(2) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABKVFD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAABKZE"(255),"AAABLEBM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKYV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAABKZE" VARBINARY(255) NOT NULL,
  "AAABDNIV" varchar(10) NOT NULL default ' ',
  "AAABFSPQ" varchar(70) NOT NULL default ' ',
  "AAAAOZJZ" varchar(2) NOT NULL default ' ',
  "AAABFJIN" varchar(1) NOT NULL default ' ',
  "AAABFJIO" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLUXQ","AAAABKZE"(255)),
  KEY "AAAABKZF" ("AAABLUXQ","AAABDNIV","AAABFSPQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABKZJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAAACDAK" varchar(12) NOT NULL default '000000000000',
  "AAABKWVJ" varchar(10) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABMECK" varchar(4) NOT NULL default ' ',
  "AAABHAAW" varchar(20) NOT NULL default ' ',
  "AAAACMRC" varchar(40) NOT NULL default ' ',
  "AAAACMRD" varchar(40) NOT NULL default ' ',
  "AAAACMRE" varchar(40) NOT NULL default ' ',
  "AAAACMRF" varchar(40) NOT NULL default ' ',
  "AAABGGVN" varchar(2) NOT NULL default ' ',
  "AAAATDSF" varchar(40) NOT NULL default ' ',
  "AAAATDSG" varchar(40) NOT NULL default ' ',
  "AAAACMRT" varchar(4) NOT NULL default ' ',
  "AAAABKZZ" varchar(20) NOT NULL default ' ',
  "AAAACMRU" varchar(4) NOT NULL default ' ',
  "AAAABLAB" varchar(20) NOT NULL default ' ',
  "AAAACMRV" varchar(4) NOT NULL default ' ',
  "AAAABLAD" varchar(20) NOT NULL default ' ',
  "AAAACMRW" varchar(4) NOT NULL default ' ',
  "AAAABLAF" varchar(20) NOT NULL default ' ',
  "AAAACMRX" varchar(4) NOT NULL default ' ',
  "AAAABLAH" varchar(20) NOT NULL default ' ',
  "AAAACMRY" varchar(80) NOT NULL default ' ',
  "AAAACMSE" varchar(1) NOT NULL default ' ',
  "AAAACMSF" varchar(1) NOT NULL default ' ',
  "AAABCFWX" varchar(4) NOT NULL default ' ',
  "AAAACMSX" varchar(40) NOT NULL default ' ',
  "AAAACMSY" varchar(40) NOT NULL default ' ',
  "AAAATGXW" varchar(40) NOT NULL default ' ',
  "AAAATGXX" varchar(40) NOT NULL default ' ',
  "AAAAIKNC" varchar(12) NOT NULL default ' ',
  "AAAATGXZ" varchar(10) NOT NULL default ' ',
  "AAAATGYA" varchar(10) NOT NULL default ' ',
  "AAAATGYB" varchar(10) NOT NULL default ' ',
  "AAABIXXS" varchar(10) NOT NULL default ' ',
  "AAAATGYH" varchar(40) NOT NULL default ' ',
  "AAAABLAW" varchar(15) NOT NULL default ' ',
  "AAAABLAX" varchar(10) NOT NULL default ' ',
  "AAABJXDX" varchar(60) NOT NULL default ' ',
  "AAAABLAZ" varchar(12) NOT NULL default ' ',
  "AAAABLBA" varchar(2) NOT NULL default ' ',
  "AAAATGXR" varchar(10) NOT NULL default ' ',
  "AAAATGXS" varchar(10) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIYJJ" varchar(3) NOT NULL default ' ',
  "AAAABLBG" varchar(3) NOT NULL default ' ',
  "AAABIXYH" varchar(30) NOT NULL default ' ',
  "AAABIXYI" varchar(10) NOT NULL default ' ',
  "AAABIXYJ" varchar(30) NOT NULL default ' ',
  "AAABIXYK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAAACDAK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLBO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAAAVSVR" varchar(10) NOT NULL default ' ',
  "AAABCNQM" varchar(4) NOT NULL default ' ',
  "AAABFODK" varchar(2) NOT NULL default '00',
  "AAAABLBU" decimal(17,2) NOT NULL default '0.00',
  "AAAABLBV" varchar(5) NOT NULL default ' ',
  "AAABLARI" varchar(8) NOT NULL default ' ',
  "AAABJDPL" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAAAVSVR","AAABCNQM","AAABFODK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLCD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAAAVSVR" varchar(10) NOT NULL default ' ',
  "AAAACDBX" varchar(20) NOT NULL default ' ',
  "AAABCOQN" varchar(1) NOT NULL default ' ',
  "AAAABLCJ" varchar(8) NOT NULL default '00000000',
  "AAAACDBR" varchar(8) NOT NULL default '00000000',
  "AAAABLCL" varchar(1) NOT NULL default ' ',
  "AAABHBRF" decimal(6,0) NOT NULL default '0',
  "AAABHZSA" varchar(1) NOT NULL default ' ',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  "AAABCNLC" decimal(17,2) NOT NULL default '0.00',
  "AAABKYSQ" varchar(5) NOT NULL default ' ',
  "AAABCNYI" varchar(35) NOT NULL default ' ',
  "AAAABLCS" varchar(40) NOT NULL default ' ',
  "AAABJWRR" varchar(3) NOT NULL default ' ',
  "AAABMGVL" varchar(15) NOT NULL default ' ',
  "AAAACCRM" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAAAVSVR","AAAACDBX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLDA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWVJ" varchar(10) NOT NULL default ' ',
  "AAAAEWEE" VARBINARY(255),
  "AAAAQXPH" varchar(8) NOT NULL default '00000000',
  "AAAABLDF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLDI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAAABLDL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLDO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAABLEE" varchar(4) NOT NULL default '0000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABIYJJ" varchar(3) NOT NULL default ' ',
  "AAABKWVJ" varchar(10) NOT NULL default ' ',
  "AAAAVSVR" varchar(10) NOT NULL default ' ',
  "AAABBTUU" decimal(23,2) NOT NULL default '0.00',
  "AAABLRNW" varchar(8) NOT NULL default '00000000',
  "AAABCJAU" varchar(3) NOT NULL default ' ',
  "AAAABLDZ" decimal(23,2) NOT NULL default '0.00',
  "AAABCIYY" varchar(3) NOT NULL default ' ',
  "AAAABLEB" decimal(23,2) NOT NULL default '0.00',
  "AAAABLEC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAABLEE","AAABHFHO","AAABLEHK","AAABIYJJ","AAABKWVJ","AAAAVSVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLEK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKWVJ" varchar(10) NOT NULL default ' ',
  "AAAABLEX" varchar(10) NOT NULL default '0000000000',
  "AAAABLEO" varchar(10) NOT NULL default '0000000000',
  "AAABCIZL" varchar(3) NOT NULL default ' ',
  "AAAABLEQ" varchar(30) NOT NULL default ' ',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  "AAAALQTU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKWVJ","AAAABLEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLEY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAAAVSVR" varchar(10) NOT NULL default ' ',
  "AAAAUXAG" varchar(10) NOT NULL default ' ',
  "AAABIFGN" varchar(8) NOT NULL default '00000000',
  "AAABCJEQ" varchar(2) NOT NULL default '00',
  "AAABCJEC" varchar(5) NOT NULL default ' ',
  "AAABCJED" decimal(23,2) NOT NULL default '0.00',
  "AAABLRLE" varchar(8) NOT NULL default '00000000',
  "AAABLRLF" varchar(8) NOT NULL default '00000000',
  "AAAABLFJ" varchar(30) NOT NULL default ' ',
  "AAABJCXS" varchar(2) NOT NULL default ' ',
  "AAAABLGE" varchar(1) NOT NULL default ' ',
  "AAAAPBUP" varchar(8) NOT NULL default '00000000',
  "AAAABLFN" varchar(8) NOT NULL default '00000000',
  "AAABJQCM" varchar(12) NOT NULL default ' ',
  "AAABJQCK" varchar(8) NOT NULL default '00000000',
  "AAAALNQW" varchar(1) NOT NULL default ' ',
  "AAABLDNI" varchar(12) NOT NULL default '000000000000',
  "AAABJJLP" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAAAVSVR","AAAAUXAG"),
  KEY "AAAABLFX" ("AAABMRIL","AAABHFHO","AAAAVSVR","AAABCJEQ","AAABLRLE","AAABLRLF","AAAABLGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLGF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABAWCT" varchar(12) NOT NULL default '000000000000',
  "AAAABLGU" varchar(5) NOT NULL default '00000',
  "AAAAVSVR" varchar(10) NOT NULL default ' ',
  "AAAABLGL" varchar(4) NOT NULL default ' ',
  "AAAACDAK" varchar(12) NOT NULL default '000000000000',
  "AAAABLGN" varchar(5) NOT NULL default '00000',
  "AAAABLGO" varchar(4) NOT NULL default ' ',
  "AAABLDNI" varchar(12) NOT NULL default '000000000000',
  "AAAABLGQ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABAWCT","AAAABLGU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLGV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAAAVSVR" varchar(10) NOT NULL default ' ',
  "AAAABLGZ" varchar(1) NOT NULL default ' ',
  "AAAABLHA" varchar(2) NOT NULL default '00',
  "AAABCISN" varchar(1) NOT NULL default ' ',
  "AAAABLHC" decimal(17,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAAAVSVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLHG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAAAVSVR" varchar(10) NOT NULL default ' ',
  "AAABFODK" varchar(2) NOT NULL default '00',
  "AAAABLHL" varchar(4) NOT NULL default ' ',
  "AAAABLHM" varchar(10) NOT NULL default ' ',
  "AAAABLHN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAAAVSVR","AAABFODK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLHS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAAAVSVR" varchar(10) NOT NULL default ' ',
  "AAABLRNW" varchar(8) NOT NULL default '00000000',
  "AAAABLHX" decimal(17,2) NOT NULL default '0.00',
  "AAAABLHY" varchar(5) NOT NULL default ' ',
  "AAAABLHZ" decimal(23,2) NOT NULL default '0.00',
  "AAAABLIA" varchar(5) NOT NULL default ' ',
  "AAAABLIB" decimal(17,2) NOT NULL default '0.00',
  "AAAABLIC" varchar(5) NOT NULL default ' ',
  "AAAACAVC" varchar(12) NOT NULL default '000000000000',
  "AAAABLIE" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAAAVSVR","AAABLRNW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLIJ" (
  "AAAABMPV" varchar(15) NOT NULL default ' ',
  "AAAABMPW" varchar(20) NOT NULL default ' ',
  "AAAABMCD" varchar(7) NOT NULL default ' ',
  "AAAABLIN" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAAABMPV","AAAABMPW","AAAABMCD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLIR" (
  "AAAABMPV" varchar(15) NOT NULL default ' ',
  "AAAABMCD" varchar(7) NOT NULL default ' ',
  "AAAABLIU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAABMPV","AAAABMCD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLIX" (
  "AAAAUNVN" varchar(12) NOT NULL default ' ',
  "AAAAUNVE" varchar(12) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAVFVO" varchar(64) NOT NULL default ' ',
  "AAAAVFVP" varchar(64) NOT NULL default ' ',
  "AAAABLJD" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUNVN","AAAAUNVE","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLJH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAALCSE" varchar(20) NOT NULL default ' ',
  "AAAALCSH" varchar(40) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABAKLL" varchar(20) NOT NULL default ' ',
  "AAAALCRL" varchar(60) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABFJDH" varchar(3) NOT NULL default ' ',
  "AAABLDBD" varchar(10) NOT NULL default ' ',
  "AAAALCRP" decimal(17,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAALCRR" varchar(8) NOT NULL default '00000000',
  "AAAALCRS" varchar(8) NOT NULL default '00000000',
  "AAAALCRT" varchar(1) NOT NULL default ' ',
  "AAABKXLF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAALCSE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLJZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAABLRG" varchar(13) NOT NULL default ' ',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAAAUAMG" tinyint(3) unsigned NOT NULL default '0',
  "AAABKSXJ" int(11) NOT NULL default '0',
  "AAAAUAMD" int(11) NOT NULL default '0',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEN" decimal(21,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAABLRG","AAABGZKH","AAAAUAMG","AAABKSXJ","AAAAUAMD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLKR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDCUU" varchar(13) NOT NULL default ' ',
  "AAABDCUN" varchar(13) NOT NULL default ' ',
  "AAABKSXJ" int(11) NOT NULL default '0',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABJGWF" varchar(13) NOT NULL default ' ',
  "AAABJGWE" varchar(10) NOT NULL default ' ',
  "AAABJMEG" varchar(4) NOT NULL default '0000',
  "AAABLIMM" varchar(10) NOT NULL default ' ',
  "AAABGRGT" varchar(12) NOT NULL default ' ',
  "AAABGZKF" varchar(8) NOT NULL default '00000000',
  "AAABGZKC" decimal(22,6) NOT NULL default '0.000000',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABDCUU","AAABDCUN","AAABKSXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLLM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDCUU" varchar(13) NOT NULL default ' ',
  "AAABKZRG" varchar(13) NOT NULL default ' ',
  "AAABKSXJ" int(11) NOT NULL default '0',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABJGWF" varchar(13) NOT NULL default ' ',
  "AAABJGWE" varchar(10) NOT NULL default ' ',
  "AAABJMEG" varchar(4) NOT NULL default '0000',
  "AAABLIMM" varchar(10) NOT NULL default ' ',
  "AAABGRGT" varchar(12) NOT NULL default ' ',
  "AAABGZKF" varchar(8) NOT NULL default '00000000',
  "AAABGZKC" decimal(22,6) NOT NULL default '0.000000',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABDCUU","AAABKZRG","AAABKSXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLMH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFWYA" varchar(32) NOT NULL default ' ',
  "AAABKSXJ" int(11) NOT NULL default '0',
  "AAAABLRG" varchar(13) NOT NULL default ' ',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABJGWI" varchar(13) NOT NULL default ' ',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGZEN" decimal(21,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABFWYA","AAABKSXJ","AAAABLRG","AAABJGWD","AAABJGWI"),
  KEY "AAAABLMX" ("AAABMRIL","AAAABLRG","AAABJGWD","AAABJGWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLNC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAABLRG" varchar(13) NOT NULL default ' ',
  "AAABHKGP" varchar(32) NOT NULL default ' ',
  "AAABGXLE" varchar(10) NOT NULL default '0000000000',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAABLRG","AAABHKGP","AAABGXLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLNN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAABLRG" varchar(13) NOT NULL default ' ',
  "AAABGRLZ" varchar(32) NOT NULL default ' ',
  "AAABKSXJ" int(11) NOT NULL default '0',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEN" decimal(21,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAABLRG","AAABGRLZ","AAABKSXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABJPDB" varchar(10) NOT NULL default ' ',
  "AAABLHFC" varchar(3) NOT NULL default ' ',
  "AAABHMRN" varchar(32) NOT NULL default ' ',
  "AAAABLRG" varchar(13) NOT NULL default ' ',
  "AAABJGWI" varchar(13) NOT NULL default ' ',
  "AAABMDHR" decimal(13,2) NOT NULL default '0.00',
  "AAABLBYH" varchar(5) NOT NULL default ' ',
  "AAABMDHS" decimal(13,2) NOT NULL default '0.00',
  "AAABLAYD" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWD","AAABJPDB","AAABLHFC","AAABHMRN","AAAABLRG","AAABJGWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABGVMX" varchar(32) NOT NULL default ' ',
  "AAABGVMY" int(11) NOT NULL default '0',
  "AAAABLRG" varchar(13) NOT NULL default ' ',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABJGWI" varchar(13) NOT NULL default ' ',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEN" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZKH","AAABGVMX","AAABGVMY","AAAABLRG","AAABJGWD","AAABJGWI"),
  KEY "AAAABLPM" ("AAABMRIL","AAAABLRG","AAABJGWD","AAABJGWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLPR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGVZV" varchar(32) NOT NULL default ' ',
  "AAABGWDE" int(11) NOT NULL default '0',
  "AAAABLRG" varchar(13) NOT NULL default ' ',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABJGWI" varchar(13) NOT NULL default ' ',
  "AAABGZFQ" varchar(32) NOT NULL default ' ',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEN" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGVZV","AAABGWDE","AAAABLRG","AAABJGWD","AAABJGWI"),
  KEY "AAAABLQK" ("AAABMRIL","AAABGZFQ"),
  KEY "AAAABLQN" ("AAABMRIL","AAAABLRG","AAABJGWD","AAABJGWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLQS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAAABLRG" varchar(13) NOT NULL default ' ',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABJGWI" varchar(13) NOT NULL default ' ',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZKH","AAAABLRG","AAABJGWD","AAABJGWI"),
  KEY "AAAABLRE" ("AAABMRIL","AAAABLRG","AAABJGWD","AAABJGWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLRJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLADI" varchar(13) NOT NULL default ' ',
  "AAABKXOZ" varchar(5) NOT NULL default '00000',
  "AAABKXML" varchar(8) NOT NULL default '00000000',
  "AAABKXMM" varchar(6) NOT NULL default '000000',
  "AAABKXKH" varchar(4) NOT NULL default '0000',
  "AAABKZZG" varchar(13) NOT NULL default ' ',
  "AAABMDHS" decimal(13,2) NOT NULL default '0.00',
  "AAABKTUI" varchar(5) NOT NULL default ' ',
  "AAABKXIU" varchar(5) NOT NULL default ' ',
  "AAABKTUJ" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLADI","AAABKXOZ","AAABKXML","AAABKXMM","AAABKXKH","AAABKZZG"),
  KEY "AAAABLSE" ("AAABMRIL","AAABKZZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLSH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKZLG" varchar(10) NOT NULL default ' ',
  "AAABLHFC" varchar(3) NOT NULL default ' ',
  "AAABKZZG" varchar(13) NOT NULL default ' ',
  "AAABLAYD" varchar(5) NOT NULL default ' ',
  "AAABLBYH" varchar(5) NOT NULL default ' ',
  "AAABMDHS" decimal(13,2) NOT NULL default '0.00',
  "AAABMDHR" decimal(13,2) NOT NULL default '0.00',
  "AAABMDZN" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKZLG","AAABLHFC","AAABKZZG"),
  KEY "AAAABLSX" ("AAABMRIL","AAABKZZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLTA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABLAYL" varchar(10) NOT NULL default ' ',
  "AAABLCCS" varchar(8) NOT NULL default '00000000',
  "AAABLADX" varchar(6) NOT NULL default '000000',
  "AAABMEAW" varchar(4) NOT NULL default '0000',
  "AAABKZZG" varchar(13) NOT NULL default ' ',
  "AAABLAYD" varchar(5) NOT NULL default ' ',
  "AAABLBYH" varchar(5) NOT NULL default ' ',
  "AAABMDHS" decimal(13,2) NOT NULL default '0.00',
  "AAABMDHR" decimal(13,2) NOT NULL default '0.00',
  "AAABMDZN" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMEFT","AAABLAYL","AAABLCCS","AAABLADX","AAABMEAW","AAABKZZG"),
  KEY "AAAABLTW" ("AAABMRIL","AAABMEFT","AAABKZZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLUA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLAYL" varchar(10) NOT NULL default ' ',
  "AAABKZZG" varchar(13) NOT NULL default ' ',
  "AAABLCIO" varchar(8) NOT NULL default '00000000',
  "AAABLCIP" varchar(6) NOT NULL default '000000',
  "AAABMEAW" varchar(4) NOT NULL default '0000',
  "AAABMDHS" decimal(13,2) NOT NULL default '0.00',
  "AAABLAYD" varchar(5) NOT NULL default ' ',
  "AAABMDHR" decimal(13,2) NOT NULL default '0.00',
  "AAABLBYH" varchar(5) NOT NULL default ' ',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLAYL","AAABKZZG","AAABLCIO","AAABLCIP","AAABMEAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLUU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLCCM" varchar(8) NOT NULL default '00000000',
  "AAABLCCS" varchar(8) NOT NULL default '00000000',
  "AAABLADX" varchar(6) NOT NULL default '000000',
  "AAABMEAW" varchar(4) NOT NULL default '0000',
  "AAABKZZG" varchar(13) NOT NULL default ' ',
  "AAABLAYD" varchar(5) NOT NULL default ' ',
  "AAABLBYH" varchar(5) NOT NULL default ' ',
  "AAABMDHS" decimal(13,2) NOT NULL default '0.00',
  "AAABMDHR" decimal(13,2) NOT NULL default '0.00',
  "AAABMDZN" decimal(13,2) NOT NULL default '0.00',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLCCM","AAABLCCS","AAABLADX","AAABMEAW","AAABKZZG"),
  KEY "AAAABLVP" ("AAABMRIL","AAABKZZG","AAABMEFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLVT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABKZZG" varchar(13) NOT NULL default ' ',
  "AAABLAEX" decimal(10,2) NOT NULL default '0.00',
  "AAABLAFG" decimal(15,2) NOT NULL default '0.00',
  "AAABLAFH" decimal(15,6) NOT NULL default '0.000000',
  PRIMARY KEY ("AAABMRIL","AAABMEFT","AAABKZZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLWD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABKZZG" varchar(13) NOT NULL default ' ',
  "AAABLAHH" decimal(10,2) NOT NULL default '0.00',
  "AAABLAHI" varchar(5) NOT NULL default ' ',
  "AAABLAID" decimal(15,6) NOT NULL default '0.000000',
  PRIMARY KEY ("AAABMRIL","AAABMEFT","AAABKZZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLWN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABKZZG" varchar(13) NOT NULL default ' ',
  "AAABLAIW" decimal(10,2) NOT NULL default '0.00',
  "AAABLAIX" decimal(13,2) NOT NULL default '0.00',
  "AAABLAIY" decimal(13,2) NOT NULL default '0.00',
  "AAABLAIZ" varchar(5) NOT NULL default ' ',
  "AAABLAJO" decimal(13,2) NOT NULL default '0.00',
  "AAABLAJQ" decimal(13,2) NOT NULL default '0.00',
  "AAABLAJS" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMEFT","AAABKZZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLXB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLARH" varchar(13) NOT NULL default ' ',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  "AAABKZZG" varchar(13) NOT NULL default ' ',
  "AAABLARC" decimal(15,6) NOT NULL default '0.000000',
  "AAABLARE" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLARH","AAABLARI","AAABKZZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLXM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABLAYL" varchar(10) NOT NULL default ' ',
  "AAABLAXT" varchar(8) NOT NULL default '00000000',
  "AAABLDCD" varchar(4) NOT NULL default ' ',
  "AAABKZZG" varchar(13) NOT NULL default ' ',
  "AAABLBYH" varchar(5) NOT NULL default ' ',
  "AAABMDHR" decimal(15,2) NOT NULL default '0.00',
  "AAABLAYD" varchar(5) NOT NULL default ' ',
  "AAABMDHS" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMEFT","AAABLAYL","AAABLAXT","AAABLDCD","AAABKZZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLAYQ" varchar(10) NOT NULL default ' ',
  "AAABLAYR" varchar(10) NOT NULL default ' ',
  "AAABKZZG" varchar(13) NOT NULL default ' ',
  "AAABMDHS" decimal(13,2) NOT NULL default '0.00',
  "AAABLAYD" varchar(5) NOT NULL default ' ',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLAYQ","AAABLAYR","AAABKZZG"),
  KEY "AAAABLYT" ("AAABMRIL","AAABKZZG","AAABMEFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLYX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABLDAF" varchar(8) NOT NULL default '00000000',
  "AAABKZZG" varchar(13) NOT NULL default ' ',
  "AAABLCLW" decimal(10,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMEFT","AAABLDAF","AAABKZZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABLZH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLDAZ" varchar(3) NOT NULL default '000',
  "AAABLDBA" varchar(13) NOT NULL default ' ',
  "AAABLDBB" varchar(8) NOT NULL default ' ',
  "AAABLDBC" varchar(8) NOT NULL default ' ',
  "AAABLDAF" varchar(8) NOT NULL default '00000000',
  "AAABLDAG" varchar(3) NOT NULL default '000',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  "AAABMDYP" varchar(8) NOT NULL default '00000000',
  "AAABMDYS" varchar(2) NOT NULL default '00',
  "AAABKZZG" varchar(13) NOT NULL default ' ',
  "AAABMDZH" decimal(13,2) NOT NULL default '0.00',
  "AAABMEFI" varchar(5) NOT NULL default ' ',
  "AAABMDZN" decimal(13,2) NOT NULL default '0.00',
  "AAABMDZI" decimal(15,6) NOT NULL default '0.000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLDAZ","AAABLDBA","AAABLDBB","AAABLDBC","AAABLDAF","AAABLDAG","AAABLDBT","AAABMDYP","AAABMDYS","AAABKZZG"),
  KEY "AAAABMAK" ("AAABMRIL","AAABKZZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLADI" varchar(13) NOT NULL default ' ',
  "AAABKXOZ" varchar(5) NOT NULL default '00000',
  "AAABKXML" varchar(8) NOT NULL default '00000000',
  "AAABKXMM" varchar(6) NOT NULL default '000000',
  "AAABKXKH" varchar(4) NOT NULL default '0000',
  "AAABKZZG" varchar(13) NOT NULL default ' ',
  "AAABMDHS" decimal(13,2) NOT NULL default '0.00',
  "AAABKXIT" decimal(13,2) NOT NULL default '0.00',
  "AAABKXIU" varchar(5) NOT NULL default ' ',
  "AAABKXIO" decimal(13,2) NOT NULL default '0.00',
  "AAABKXIP" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLADI","AAABKXOZ","AAABKXML","AAABKXMM","AAABKXKH","AAABKZZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMBH" (
  "AAABGYLC" varchar(12) NOT NULL default ' ',
  "AAAABMBJ" varchar(1) NOT NULL default ' ',
  "AAABGFQD" varchar(32) NOT NULL default ' ',
  "AAABLUXQ" varchar(10) NOT NULL default ' ',
  "AAABLTXI" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABGYLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMBO" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABJTAJ" varchar(6) NOT NULL default '000000',
  "AAAABMBS" varchar(60) NOT NULL default ' ',
  "AAAARRIC" varchar(50) NOT NULL default ' ',
  "AAABHZCZ" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMDEN","AAABKOUD","AAABJTAJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMBY" (
  "AAAABMPV" varchar(15) NOT NULL default ' ',
  "AAAABMCH" varchar(10) NOT NULL default ' ',
  "AAAABMCI" varchar(5) NOT NULL default ' ',
  "AAAABMCJ" varchar(2) NOT NULL default '00',
  "AAAABMCD" varchar(7) NOT NULL default ' ',
  "AAAABMCE" varchar(15) NOT NULL default ' ',
  "AAABLQSF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAABMPV","AAAABMCH","AAAABMCI","AAAABMCJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMCK" (
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAAABMCP" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFZK","AAAABMCP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMCQ" (
  "AAAABMPV" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAAABMPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMCT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAAAFAWE" varchar(1) NOT NULL default ' ',
  "AAAAFAWF" varchar(1) NOT NULL default ' ',
  "AAABFDQR" varchar(1) NOT NULL default ' ',
  "AAAAFAVX" varchar(1) NOT NULL default ' ',
  "AAAAFAVY" varchar(1) NOT NULL default ' ',
  "AAAAFAVZ" varchar(1) NOT NULL default ' ',
  "AAAAFAWA" varchar(1) NOT NULL default ' ',
  "AAAAFAXI" varchar(1) NOT NULL default ' ',
  "AAAAFAXJ" varchar(1) NOT NULL default ' ',
  "AAAAFAXK" varchar(1) NOT NULL default ' ',
  "AAAAFAWC" varchar(1) NOT NULL default ' ',
  "AAAAMTRU" varchar(1) NOT NULL default ' ',
  "AAAAFAVO" varchar(1) NOT NULL default ' ',
  "AAAAFAVP" varchar(1) NOT NULL default ' ',
  "AAAAFAVQ" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAFAVR" varchar(1) NOT NULL default ' ',
  "AAAAFAWM" varchar(1) NOT NULL default ' ',
  "AAAAFAWN" varchar(1) NOT NULL default ' ',
  "AAAAFAWO" varchar(1) NOT NULL default ' ',
  "AAAAFAWP" varchar(1) NOT NULL default ' ',
  "AAAAFAWQ" varchar(1) NOT NULL default ' ',
  "AAAAFAWR" varchar(1) NOT NULL default ' ',
  "AAAAFAYG" varchar(1) NOT NULL default ' ',
  "AAAAFAYH" varchar(1) NOT NULL default ' ',
  "AAAAMPZX" varchar(1) NOT NULL default ' ',
  "AAAAFAVS" varchar(1) NOT NULL default ' ',
  "AAAAFAVT" varchar(1) NOT NULL default ' ',
  "AAAAFAVU" varchar(1) NOT NULL default ' ',
  "AAAAFAVV" varchar(1) NOT NULL default ' ',
  "AAAAFAVW" varchar(1) NOT NULL default ' ',
  "AAAAFAXL" varchar(1) NOT NULL default ' ',
  "AAAAFAXF" varchar(1) NOT NULL default ' ',
  "AAAAFAXG" varchar(1) NOT NULL default ' ',
  "AAAAFAVD" varchar(1) NOT NULL default ' ',
  "AAAAFAXH" varchar(1) NOT NULL default ' ',
  "AAAAFAWH" varchar(1) NOT NULL default ' ',
  "AAAAFAWI" varchar(1) NOT NULL default ' ',
  "AAAAFAWJ" varchar(1) NOT NULL default ' ',
  "AAAAFAWK" varchar(1) NOT NULL default ' ',
  "AAAAFAWL" varchar(1) NOT NULL default ' ',
  "AAAABMEL" varchar(1) NOT NULL default ' ',
  "AAAABMEM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMEP" (
  "AAABLQGL" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLQGL","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMEV" (
  "AAAAPXVU" varchar(26) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAAAPXVU","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMFB" (
  "AAABLQGL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMFE" (
  "AAAAPXVU" varchar(26) NOT NULL default ' ',
  PRIMARY KEY ("AAAAPXVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMFH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABJTNV" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTVR"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMFP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAABILWE" varchar(10) NOT NULL default ' ',
  "AAABILWD" varchar(10) NOT NULL default ' ',
  "AAAAVEJX" varchar(3) NOT NULL default ' ',
  "AAABCGWY" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTVR"(255)),
  KEY "AAAABMFY" ("AAABMRIL","AAABILWE","AAABILWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMGC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" VARBINARY(255) NOT NULL,
  "AAAABMGF" VARBINARY(255),
  "AAAABMGG" VARBINARY(255),
  "AAABHEHZ" VARBINARY(255),
  "AAAABMPC" VARBINARY(255),
  "AAAABMNZ" VARBINARY(255),
  "AAAABMOA" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAABGZKH"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMGN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCOV" varchar(3) NOT NULL default ' ',
  "AAABLCOW" varchar(2) NOT NULL default ' ',
  "AAABLCOX" varchar(12) NOT NULL default ' ',
  "AAABLTVR" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAABLCOV","AAABLCOW","AAABLCOX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMGX" (
  "AAAAPXVU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAPXVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMHA" (
  "AAAAPXVU" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAAAPXVU","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMHG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(2) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMHR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZXX" varchar(3) NOT NULL default ' ',
  "AAABILWE" varchar(10) NOT NULL default ' ',
  "AAABILWD" varchar(10) NOT NULL default ' ',
  "AAABCGWY" varchar(3) NOT NULL default '000',
  "AAABLPNX" varchar(3) NOT NULL default ' ',
  "AAAADSYZ" varchar(30) NOT NULL default ' ',
  "AAAABMHZ" int(11) NOT NULL default '0',
  "AAAABMIA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZXX","AAABILWE","AAABILWD","AAABCGWY","AAABLPNX","AAAADSYZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMII" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAABLCOV" varchar(3) NOT NULL default ' ',
  "AAABLCOW" varchar(2) NOT NULL default ' ',
  "AAABLCOX" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTVR"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMIQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJOTS" varchar(14) NOT NULL default ' ',
  "AAABLTVR" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAABJOTS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAAABMIZ" VARBINARY(255),
  "AAAABMKE" VARBINARY(255),
  "AAAAPYOZ" VARBINARY(255),
  "AAAAPYOY" VARBINARY(255),
  "AAAABMJD" varchar(20) NOT NULL default ' ',
  "AAAABMJE" varchar(30) NOT NULL default ' ',
  "AAAAPYHC" varchar(1) NOT NULL default ' ',
  "AAAAPYEM" varchar(10) NOT NULL default ' ',
  "AAAAPYEN" varchar(10) NOT NULL default ' ',
  "AAAAPYFL" varchar(1) NOT NULL default ' ',
  "AAABEMDP" varchar(12) NOT NULL default ' ',
  "AAAAPYHA" varchar(1) NOT NULL default ' ',
  "AAAAPYHB" varchar(68) NOT NULL default ' ',
  "AAAAPYHD" varchar(12) NOT NULL default ' ',
  "AAABANGI" varchar(4) NOT NULL default ' ',
  "AAAAPYHF" varchar(6) NOT NULL default ' ',
  "AAAAPYHG" varchar(4) NOT NULL default ' ',
  "AAAAPYHH" varchar(12) NOT NULL default ' ',
  "AAAAPYHI" varchar(1) NOT NULL default ' ',
  "AAAAPYHJ" varchar(1) NOT NULL default ' ',
  "AAABHCQU" varchar(3) NOT NULL default '000',
  "AAAAPYHL" varchar(1) NOT NULL default ' ',
  "AAAAPYHZ" varchar(1) NOT NULL default ' ',
  "AAAAQHZI" varchar(12) NOT NULL default ' ',
  "AAAAVEJX" varchar(3) NOT NULL default ' ',
  "AAAAPYPB" VARBINARY(255),
  "AAAAPYPA" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAABLTVR"(255)),
  KEY "AAAABMKC" ("AAABMRIL","AAAABMKE"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMKF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" VARBINARY(255) NOT NULL,
  "AAAABMKY" VARBINARY(255) NOT NULL,
  "AAAABMKJ" VARBINARY(255),
  "AAAAVYWV" varchar(1) NOT NULL default ' ',
  "AAAAVYWW" varchar(1) NOT NULL default ' ',
  "AAAAVYWX" varchar(1) NOT NULL default ' ',
  "AAAAVYWY" varchar(1) NOT NULL default ' ',
  "AAAAVYWZ" varchar(1) NOT NULL default ' ',
  "AAAAVYXC" varchar(1) NOT NULL default ' ',
  "AAAAVYXA" varchar(1) NOT NULL default ' ',
  "AAAAUMFV" VARBINARY(255),
  "AAABKUSC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"(255),"AAAABMKY"(255)),
  KEY "AAAABMKW" ("AAABMRIL","AAAABMKY"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMKZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAABEMDP" varchar(12) NOT NULL default ' ',
  "AAAAPYHA" varchar(1) NOT NULL default ' ',
  "AAAAPYHB" varchar(68) NOT NULL default ' ',
  "AAAAPYHD" varchar(12) NOT NULL default ' ',
  "AAABANGI" varchar(4) NOT NULL default ' ',
  "AAAAPYHF" varchar(6) NOT NULL default ' ',
  "AAAAPYHG" varchar(4) NOT NULL default ' ',
  "AAAAPYHH" varchar(12) NOT NULL default ' ',
  "AAAAPYHI" varchar(1) NOT NULL default ' ',
  "AAAAPYHJ" varchar(1) NOT NULL default ' ',
  "AAABHCQU" varchar(3) NOT NULL default '000',
  "AAAAPYHL" varchar(1) NOT NULL default ' ',
  "AAAAPYHZ" varchar(1) NOT NULL default ' ',
  "AAAAQHZI" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTVR"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMLS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" VARBINARY(255) NOT NULL,
  "AAABGZXX" varchar(3) NOT NULL default ' ',
  "AAABILWE" varchar(10) NOT NULL default ' ',
  "AAABILWD" varchar(10) NOT NULL default ' ',
  "AAABCGWY" varchar(3) NOT NULL default '000',
  "AAAAVYWV" varchar(1) NOT NULL default ' ',
  "AAAAVYWW" varchar(1) NOT NULL default ' ',
  "AAAAVYWX" varchar(1) NOT NULL default ' ',
  "AAAAVYWY" varchar(1) NOT NULL default ' ',
  "AAAAVYWZ" varchar(1) NOT NULL default ' ',
  "AAAAVYXC" varchar(1) NOT NULL default ' ',
  "AAAAVYXA" varchar(1) NOT NULL default ' ',
  "AAAAUMFV" VARBINARY(255),
  "AAABKUSC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"(255),"AAABGZXX","AAABILWE","AAABILWD","AAABCGWY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMMO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPVQ" varchar(30) NOT NULL default ' ',
  "AAABLTVR" VARBINARY(255),
  "AAAABMMS" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPVQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMMV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAAABMMY" VARBINARY(255),
  "AAAABMMZ" VARBINARY(255),
  "AAAABMNA" VARBINARY(255),
  "AAAABMNB" VARBINARY(255),
  "AAABMECK" varchar(255) NOT NULL default ' ',
  "AAAABMND" varchar(1) NOT NULL default ' ',
  "AAABETFE" varchar(30) NOT NULL default ' ',
  "AAAABMNF" varchar(30) NOT NULL default ' ',
  "AAAABMNG" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAABLTVR"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMNJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" VARBINARY(255) NOT NULL,
  "AAABEBYS" varchar(12) NOT NULL default ' ',
  "AAAABMNN" VARBINARY(255),
  "AAAABMPI" VARBINARY(255),
  "AAAABMNP" VARBINARY(255),
  "AAAABMNQ" VARBINARY(255),
  "AAAABMNR" VARBINARY(255),
  "AAAABMNS" VARBINARY(255),
  "AAAABMNT" varchar(32) NOT NULL default ' ',
  "AAAAVJSF" varchar(1) NOT NULL default ' ',
  "AAAACKQE" varchar(1) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAABMNRG" varchar(1) NOT NULL default ' ',
  "AAAABMNZ" VARBINARY(255),
  "AAAABMOA" VARBINARY(255),
  "AAABHEHZ" VARBINARY(255),
  "AAAABMPC" VARBINARY(255),
  "AAAABMPF" VARBINARY(255),
  "AAAABMOE" varchar(32) NOT NULL default ' ',
  "AAAABMOF" varchar(1) NOT NULL default ' ',
  "AAAABMOG" varchar(1) NOT NULL default ' ',
  "AAAAULXJ" varchar(1) NOT NULL default ' ',
  "AAAAFGNV" varchar(1) NOT NULL default ' ',
  "AAAABMOJ" VARBINARY(255),
  "AAAABMOK" VARBINARY(255),
  "AAAABMOL" VARBINARY(255),
  "AAAABMOZ" varchar(17) NOT NULL default ' ',
  "AAABDSAH" varchar(4) NOT NULL default ' ',
  "AAAABMOO" varchar(1) NOT NULL default ' ',
  "AAAABMOP" decimal(15,0) NOT NULL default '0',
  "AAABKNAR" decimal(15,0) NOT NULL default '0',
  "AAABKNCM" decimal(15,0) NOT NULL default '0',
  "AAAABMOS" decimal(15,0) NOT NULL default '0',
  "AAAABMOT" varchar(1) NOT NULL default ' ',
  "AAAABMOU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"(255)),
  KEY "AAAABMOX" ("AAABMRIL","AAAABMOZ"),
  KEY "AAAABMPA" ("AAABMRIL","AAAABMPC"(255)),
  KEY "AAAABMPD" ("AAABMRIL","AAAABMPF"(255)),
  KEY "AAAABMPG" ("AAABMRIL","AAAABMPI"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMPJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZBP" varchar(12) NOT NULL default ' ',
  "AAABLTVR" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAABHZBP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMPP" (
  "AAAABMPV" varchar(15) NOT NULL default ' ',
  "AAAABMPW" varchar(20) NOT NULL default ' ',
  "AAAABMPS" varchar(10) NOT NULL default ' ',
  "AAAABMPT" varchar(10) NOT NULL default ' ',
  "AAAABMPU" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAAABMPV","AAAABMPW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMPX" (
  "AAAABMQG" varchar(8) NOT NULL default '00000000',
  "AAAABMQH" varchar(6) NOT NULL default '000000',
  "AAAABMQI" varchar(12) NOT NULL default ' ',
  "AAAABMQB" varchar(20) NOT NULL default ' ',
  "AAAABMQC" varchar(20) NOT NULL default ' ',
  "AAAABMQD" varchar(30) NOT NULL default ' ',
  "AAABLILS" varchar(1) NOT NULL default ' ',
  "AAAABMQF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAABMQG","AAAABMQH","AAAABMQI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMQJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWVJ" varchar(10) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABHESL" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUXQ","AAABKWVJ"),
  KEY "AAAABMQS" ("AAABLUXQ","AAABMPLT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMQV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCKLH" varchar(10) NOT NULL default ' ',
  "AAABCKLK" varchar(10) NOT NULL default ' ',
  "AAAAIXRU" varchar(30) NOT NULL default ' ',
  "AAAABMSF" varchar(3) NOT NULL default ' ',
  "AAAABMSG" varchar(3) NOT NULL default ' ',
  "AAAABMSH" varchar(8) NOT NULL default '00000000',
  "AAABLSEB" varchar(16) NOT NULL default '0000000000000000',
  "AAAABMRE" varchar(16) NOT NULL default '0000000000000000',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCKLH","AAABCKLK","AAAAIXRU","AAAABMSF","AAAABMSG","AAAABMSH","AAABLSEB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMRO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCKLH" varchar(10) NOT NULL default ' ',
  "AAABCKLK" varchar(10) NOT NULL default ' ',
  "AAAAIXRU" varchar(30) NOT NULL default ' ',
  "AAAABMSF" varchar(3) NOT NULL default ' ',
  "AAAABMSG" varchar(3) NOT NULL default ' ',
  "AAAABMSH" varchar(8) NOT NULL default '00000000',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABMLPO" int(11) NOT NULL default '0',
  "AAABBKEI" varchar(8) NOT NULL default '00000000',
  "AAABBKEN" varchar(6) NOT NULL default '000000',
  "AAAABMSA" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABCKLH","AAABCKLK","AAAAIXRU","AAAABMSF","AAAABMSG","AAAABMSH","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMSJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKGQC" varchar(10) NOT NULL default '0000000000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLTOL" varchar(254) NOT NULL default ' ',
  "AAABJEMU" varchar(30) NOT NULL default ' ',
  "AAABLOCX" varchar(14) NOT NULL default ' ',
  "AAABLOCY" varchar(14) NOT NULL default ' ',
  "AAAAKGPF" varchar(12) NOT NULL default ' ',
  "AAABCKGF" varchar(15) NOT NULL default ' ',
  "AAAABMTR" varchar(90) NOT NULL default ' ',
  "AAAABMTS" varchar(10) NOT NULL default ' ',
  "AAAAKGPA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKGQC"),
  KEY "AAAABMSY" ("AAABLOCX","AAABCKGF"),
  KEY "AAAABMTB" ("AAABMRIL","AAABLOCX","AAABLOCY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMTF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIWWJ" varchar(30) NOT NULL default ' ',
  "AAAAKGQC" varchar(10) NOT NULL default '0000000000',
  "AAABKWJY" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLTOL" varchar(254) NOT NULL default ' ',
  "AAABJEMU" varchar(30) NOT NULL default ' ',
  "AAABLOCX" varchar(14) NOT NULL default ' ',
  "AAABLOCY" varchar(14) NOT NULL default ' ',
  "AAAAKGPF" varchar(12) NOT NULL default ' ',
  "AAABCKGF" varchar(15) NOT NULL default ' ',
  "AAAABMTR" varchar(90) NOT NULL default ' ',
  "AAAABMTS" varchar(10) NOT NULL default ' ',
  "AAAAKGPA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIWWJ","AAAAKGQC"),
  KEY "AAAABMTX" ("AAABMRIL","AAABIWWJ","AAABKWJY","AAABLOCX","AAAAKGQC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMUD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKGQC" varchar(10) NOT NULL default '0000000000',
  "AAABIWWJ" varchar(30) NOT NULL default ' ',
  "AAABKWJY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKGQC","AAABIWWJ"),
  KEY "AAAABMUL" ("AAABMRIL","AAABIWWJ","AAABKWJY","AAAAKGQC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMUQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAABMUY" varchar(12) NOT NULL default ' ',
  "AAABFWAW" varchar(20) NOT NULL default ' ',
  "AAABIMHJ" varchar(12) NOT NULL default ' ',
  "AAAAZOEQ" varchar(12) NOT NULL default ' ',
  "AAAABMUW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAABMUY","AAABFWAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMVA" (
  "AAAABMVJ" varchar(10) NOT NULL default ' ',
  "AAAABMVH" varchar(10) NOT NULL default ' ',
  "AAAABMVI" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAAABMVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMVF" (
  "AAAABMVJ" varchar(10) NOT NULL default ' ',
  "AAAABMVH" varchar(10) NOT NULL default ' ',
  "AAAABMVI" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAAABMVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMVK" (
  "AAABLUXP" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAABLUWM" varchar(32) NOT NULL default ' ',
  "AAABLUWN" varchar(32) NOT NULL default ' ',
  "AAABMOHM" varchar(3) NOT NULL default ' ',
  "AAABLUVJ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXP","AAABLUXQ","AAABLUWM","AAABLUWN","AAABMOHM","AAABLUVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMVX" (
  "AAAABNDT" varchar(10) NOT NULL default ' ',
  "AAABLELJ" varchar(80) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAAABNDT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMWD" (
  "AAABLUVB" varchar(10) NOT NULL default ' ',
  "AAABMONG" varchar(3) NOT NULL default ' ',
  "AAAABOCN" varchar(10) NOT NULL default '0000000000',
  "AAABLETY" varchar(8) NOT NULL default ' ',
  "AAABIZMN" varchar(10) NOT NULL default '0000000000',
  "AAABLUUU" varchar(8) NOT NULL default ' ',
  "AAABLUUV" varchar(8) NOT NULL default ' ',
  "AAABJAQI" varchar(6) NOT NULL default ' ',
  "AAABLUVJ" varchar(32) NOT NULL default ' ',
  "AAABHFAP" varchar(2) NOT NULL default ' ',
  "AAABLUVE" varchar(4) NOT NULL default '0000',
  "AAABLMOZ" varchar(20) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLEHI" varchar(4) NOT NULL default '0000',
  "AAAATVPS" varchar(30) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMRFR" varchar(6) NOT NULL default '000000',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAAAMAVV" varchar(16) NOT NULL default '0000000000000000',
  "AAAABMWX" int(11) NOT NULL default '0',
  "AAAABMWY" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLUVB","AAABMONG","AAAABOCN","AAABLETY","AAABIZMN","AAABLUUU","AAABLUUV","AAABJAQI","AAABLUVJ","AAABHFAP","AAABLUVE","AAABLMOZ","AAABMOHM","AAABLEHI"),
  KEY "AAAABMXN" ("AAABLUVB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMXP" (
  "AAABLMOZ" varchar(20) NOT NULL default ' ',
  "AAABLENR" varchar(80) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMOZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMXV" (
  "AAABLMOZ" varchar(20) NOT NULL default ' ',
  "AAAABMXX" varchar(10) NOT NULL default ' ',
  "AAAABMXY" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMOZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMYC" (
  "AAABLUTU" varchar(8) NOT NULL default ' ',
  "AAABLUTS" varchar(80) NOT NULL default ' ',
  "AAABLUTT" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUTU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMYH" (
  "AAABLUVB" varchar(10) NOT NULL default ' ',
  "AAABJAAI" varchar(3) NOT NULL default ' ',
  "AAABLUUU" varchar(8) NOT NULL default ' ',
  "AAABLUUV" varchar(8) NOT NULL default ' ',
  "AAABLUTZ" varchar(3) NOT NULL default '000',
  "AAABLUUA" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLUVB","AAABJAAI","AAABLUUU","AAABLUUV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMYS" (
  "AAABLUUU" varchar(8) NOT NULL default ' ',
  "AAABLUUV" varchar(8) NOT NULL default ' ',
  "AAABLUVB" varchar(10) NOT NULL default ' ',
  "AAABLUTZ" varchar(3) NOT NULL default '000',
  "AAABLUUA" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLUUU","AAABLUUV","AAABLUVB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABMZB" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLUVB" varchar(10) NOT NULL default ' ',
  "AAABMONG" varchar(3) NOT NULL default ' ',
  "AAAABOCN" varchar(10) NOT NULL default '0000000000',
  "AAABLETY" varchar(8) NOT NULL default ' ',
  "AAABIZMN" varchar(10) NOT NULL default '0000000000',
  "AAABLUUU" varchar(8) NOT NULL default ' ',
  "AAABLUUV" varchar(8) NOT NULL default ' ',
  "AAABJAQI" varchar(6) NOT NULL default ' ',
  "AAABLUVJ" varchar(32) NOT NULL default ' ',
  "AAABHFAP" varchar(2) NOT NULL default ' ',
  "AAABLUVE" varchar(4) NOT NULL default '0000',
  "AAABLMOZ" varchar(20) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLEHI" varchar(4) NOT NULL default '0000',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHDEA" varchar(2) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLUVB","AAABMONG","AAAABOCN","AAABLETY","AAABIZMN","AAABLUUU","AAABLUUV","AAABJAQI","AAABLUVJ","AAABHFAP","AAABLUVE","AAABLMOZ","AAABMOHM","AAABLEHI","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNAL" (
  "AAABMLPV" varchar(32) NOT NULL default ' ',
  "AAABLUWE" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV","AAABLUWE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNAQ" (
  "AAABLUUU" varchar(8) NOT NULL default ' ',
  "AAABMDXM" varchar(8) NOT NULL default ' ',
  "AAAABNAT" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUUU","AAABMDXM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNAY" (
  "AAABLUVB" varchar(10) NOT NULL default ' ',
  "AAABMONG" varchar(3) NOT NULL default ' ',
  "AAAABOCN" varchar(10) NOT NULL default '0000000000',
  "AAABLETY" varchar(8) NOT NULL default ' ',
  "AAABIZMN" varchar(10) NOT NULL default '0000000000',
  "AAABLUUU" varchar(8) NOT NULL default ' ',
  "AAABLUUV" varchar(8) NOT NULL default ' ',
  "AAABJAQI" varchar(6) NOT NULL default ' ',
  "AAABLUVJ" varchar(32) NOT NULL default ' ',
  "AAABHFAP" varchar(2) NOT NULL default ' ',
  "AAABLUVE" varchar(4) NOT NULL default '0000',
  "AAABLMOZ" varchar(20) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLEHI" varchar(4) NOT NULL default '0000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMRFR" varchar(6) NOT NULL default '000000',
  "AAABFSOJ" varchar(1) NOT NULL default ' ',
  "AAAATVPS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUVB","AAABMONG","AAAABOCN","AAABLETY","AAABIZMN","AAABLUUU","AAABLUUV","AAABJAQI","AAABLUVJ","AAABHFAP","AAABLUVE","AAABLMOZ","AAABMOHM","AAABLEHI"),
  KEY "AAAABNCF" ("AAABLUVB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNCH" (
  "AAAATVPT" varchar(6) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAAABNCK" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVPT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNCO" (
  "AAAABNCU" varchar(100) NOT NULL default ' ',
  "AAABLUVE" varchar(4) NOT NULL default '0000',
  "AAAAVPPH" varchar(60) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAAABNCU","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNCW" (
  "AAAATVPS" varchar(30) NOT NULL default ' ',
  "AAAABNDE" varchar(30) NOT NULL default ' ',
  "AAABLUVE" varchar(4) NOT NULL default '0000',
  "AAAAVPPH" varchar(60) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVPS","AAAABNDE","AAABLUVE"),
  KEY "AAAABNDG" ("AAABLUXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNDI" (
  "AAABIMHJ" varchar(4) NOT NULL default '0000',
  "AAAABNDP" varchar(30) NOT NULL default ' ',
  "AAAABNDQ" varchar(4) NOT NULL default '0000',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIMHJ","AAAABNDP","AAAABNDQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNDR" (
  "AAAATVPS" varchar(30) NOT NULL default ' ',
  "AAAABNDT" varchar(10) NOT NULL default ' ',
  "AAABLELJ" varchar(80) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVPS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNDY" (
  "AAAATVPS" varchar(30) NOT NULL default ' ',
  "AAAATXZL" varchar(6) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVPS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNEE" (
  "AAAATVPS" varchar(30) NOT NULL default ' ',
  "AAAATVPT" varchar(6) NOT NULL default ' ',
  "AAABIMZU" varchar(30) NOT NULL default ' ',
  "AAABHJPL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVPS","AAAATVPT","AAABIMZU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNEM" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAATVPS" varchar(30) NOT NULL default ' ',
  "AAAATVPT" varchar(6) NOT NULL default ' ',
  "AAAABNEZ" varchar(6) NOT NULL default '000000',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAAATVPS","AAAATVPT","AAAABNEZ","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNFB" (
  "AAAATVPS" varchar(30) NOT NULL default ' ',
  "AAAATVPT" varchar(6) NOT NULL default ' ',
  "AAABMJWN" varchar(30) NOT NULL default ' ',
  "AAAABNFF" varchar(60) NOT NULL default ' ',
  "AAAABNFY" varchar(40) NOT NULL default ' ',
  "AAAAMMTL" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVPS","AAAATVPT","AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNFN" (
  "AAAATVPS" varchar(30) NOT NULL default ' ',
  "AAAATVPT" varchar(6) NOT NULL default ' ',
  "AAABMJWN" varchar(30) NOT NULL default ' ',
  "AAABLUVE" varchar(4) NOT NULL default '0000',
  "AAABLNIW" varchar(1) NOT NULL default ' ',
  "AAABMJWI" varchar(1) NOT NULL default ' ',
  "AAAAUPPD" varchar(1) NOT NULL default ' ',
  "AAABJSDF" int(11) NOT NULL default '0',
  "AAABMNMM" int(11) NOT NULL default '0',
  "AAABMMJT" varchar(30) NOT NULL default ' ',
  "AAAABNFY" varchar(81) NOT NULL default ' ',
  "AAABDBLY" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVPS","AAAATVPT","AAABMJWN","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNGG" (
  "AAAATVPS" varchar(30) NOT NULL default ' ',
  "AAAATVPT" varchar(6) NOT NULL default ' ',
  "AAABLUVE" int(11) NOT NULL default '0',
  "AAAABNGK" int(11) NOT NULL default '0',
  "AAABHJPL" varchar(30) NOT NULL default ' ',
  "AAABHAUM" varchar(30) NOT NULL default ' ',
  "AAABLUWW" varchar(1) NOT NULL default ' ',
  "AAABJTEQ" varchar(6) NOT NULL default '000000',
  "AAABMNMM" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAATVPS","AAAATVPT","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNGT" (
  "AAAATVPS" varchar(30) NOT NULL default ' ',
  "AAAATVPT" varchar(6) NOT NULL default ' ',
  "AAABLMNC" varchar(40) NOT NULL default ' ',
  "AAAABNHU" varchar(10) NOT NULL default ' ',
  "AAAABNHV" varchar(10) NOT NULL default ' ',
  "AAAABNHW" varchar(10) NOT NULL default ' ',
  "AAAABNHX" varchar(10) NOT NULL default ' ',
  "AAABLLNR" varchar(10) NOT NULL default ' ',
  "AAAABNHZ" varchar(10) NOT NULL default ' ',
  "AAAABNIA" varchar(10) NOT NULL default ' ',
  "AAABLEML" varchar(20) NOT NULL default ' ',
  "AAAABNID" varchar(10) NOT NULL default ' ',
  "AAAABNIE" varchar(10) NOT NULL default ' ',
  "AAAABNIF" varchar(20) NOT NULL default ' ',
  "AAAABNIG" varchar(20) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVPS","AAAATVPT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNHN" (
  "AAAATVPS" varchar(30) NOT NULL default ' ',
  "AAAATVPT" varchar(6) NOT NULL default ' ',
  "AAABLUVE" varchar(4) NOT NULL default '0000',
  "AAAATVOS" varchar(4) NOT NULL default ' ',
  "AAAATVOT" varchar(4) NOT NULL default ' ',
  "AAABLMNC" varchar(40) NOT NULL default ' ',
  "AAAABNHU" varchar(10) NOT NULL default ' ',
  "AAAABNHV" varchar(10) NOT NULL default ' ',
  "AAAABNHW" varchar(10) NOT NULL default ' ',
  "AAAABNHX" varchar(10) NOT NULL default ' ',
  "AAABLLNR" varchar(10) NOT NULL default ' ',
  "AAAABNHZ" varchar(10) NOT NULL default ' ',
  "AAAABNIA" varchar(10) NOT NULL default ' ',
  "AAAABNIB" varchar(30) NOT NULL default ' ',
  "AAAABNIC" varchar(30) NOT NULL default ' ',
  "AAAABNID" varchar(10) NOT NULL default ' ',
  "AAAABNIE" varchar(10) NOT NULL default ' ',
  "AAAABNIF" varchar(20) NOT NULL default ' ',
  "AAAABNIG" varchar(20) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVPS","AAAATVPT","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNIM" (
  "AAAATVPS" varchar(30) NOT NULL default ' ',
  "AAAATVPT" varchar(6) NOT NULL default ' ',
  "AAAATVOS" varchar(4) NOT NULL default ' ',
  "AAAATVOT" varchar(4) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAAABNIS" varchar(32) NOT NULL default ' ',
  "AAAABNIT" varchar(4) NOT NULL default ' ',
  "AAABHMQN" varchar(10) NOT NULL default ' ',
  "AAAABNIV" varchar(10) NOT NULL default ' ',
  "AAAABNIW" varchar(10) NOT NULL default ' ',
  "AAABLOSZ" varchar(10) NOT NULL default ' ',
  "AAAATVOU" varchar(20) NOT NULL default ' ',
  "AAAATVOV" varchar(20) NOT NULL default ' ',
  "AAAABNJA" varchar(4) NOT NULL default ' ',
  "AAAABNJB" varchar(4) NOT NULL default ' ',
  "AAAABNJC" varchar(4) NOT NULL default ' ',
  "AAAABNJD" varchar(4) NOT NULL default ' ',
  "AAAABNJE" varchar(4) NOT NULL default ' ',
  "AAAABNJF" varchar(4) NOT NULL default ' ',
  "AAAABNJG" varchar(4) NOT NULL default '0000',
  "AAAABNJH" varchar(4) NOT NULL default '0000',
  "AAAABNJI" varchar(10) NOT NULL default ' ',
  "AAAABNJJ" varchar(1) NOT NULL default ' ',
  "AAABLBEK" varchar(1) NOT NULL default ' ',
  "AAAABNJL" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVPS","AAAATVPT"),
  KEY "AAAABNJQ" ("AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNJS" (
  "AAABIMHJ" varchar(4) NOT NULL default '0000',
  "AAAACBNC" varchar(4) NOT NULL default '0000',
  "AAAARZJQ" varchar(20) NOT NULL default ' ',
  "AAAABNJW" varchar(1) NOT NULL default ' ',
  "AAABLMLR" varchar(4) NOT NULL default '0000',
  "AAABLMOZ" varchar(20) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  "AAABLENR" varchar(80) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIMHJ","AAAACBNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNKE" (
  "AAAAGEIF" varchar(2) NOT NULL default ' ',
  "AAABLUXP" varchar(13) NOT NULL default ' ',
  "AAABLFEA" varchar(8) NOT NULL default '00000000',
  "AAAAKZCT" varchar(6) NOT NULL default '000000',
  "AAAAGDTE" varchar(12) NOT NULL default ' ',
  "AAABJQCK" varchar(8) NOT NULL default '00000000',
  "AAABJQCL" varchar(6) NOT NULL default '000000',
  "AAABJQCM" varchar(12) NOT NULL default ' ',
  "AAAAGDTI" varchar(1) NOT NULL default ' ',
  "AAABLILS" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLAQG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAGEIF","AAABLUXP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNKT" (
  "AAAAVDOK" varchar(10) NOT NULL default ' ',
  "AAABLELJ" varchar(80) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAAAVDOK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNLA" (
  "AAAATVPS" varchar(30) NOT NULL default ' ',
  "AAAATVPT" varchar(6) NOT NULL default ' ',
  "AAABLNEE" varchar(30) NOT NULL default ' ',
  "AAABJHEY" varchar(26) NOT NULL default ' ',
  "AAABLNIW" varchar(21) NOT NULL default ' ',
  "AAAAUPPD" varchar(1) NOT NULL default ' ',
  "AAABMJWI" varchar(1) NOT NULL default ' ',
  "AAABMKXF" varchar(1) NOT NULL default ' ',
  "AAAABNPF" varchar(1) NOT NULL default ' ',
  "AAABMMVU" varchar(132) NOT NULL default ' ',
  "AAABLOZE" varchar(1) NOT NULL default ' ',
  "AAAAMDCZ" varchar(1) NOT NULL default ' ',
  "AAAAMDDA" varchar(1) NOT NULL default ' ',
  "AAAAMDDB" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVPS","AAAATVPT","AAABLNEE","AAABJHEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNLV" (
  "AAAATVPS" varchar(30) NOT NULL default ' ',
  "AAAATVPT" varchar(6) NOT NULL default ' ',
  "AAAABNLY" varchar(1) NOT NULL default ' ',
  "AAAABNLZ" varchar(1) NOT NULL default ' ',
  "AAABDHXI" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVPS","AAAATVPT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNMF" (
  "AAAATVPS" varchar(30) NOT NULL default ' ',
  "AAAATVPT" varchar(6) NOT NULL default ' ',
  "AAAABNMN" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVPS","AAAATVPT","AAAABNMN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNMO" (
  "AAAATVPS" varchar(30) NOT NULL default ' ',
  "AAAATVPT" varchar(6) NOT NULL default ' ',
  "AAABLNEE" varchar(30) NOT NULL default ' ',
  "AAABJHEY" varchar(26) NOT NULL default ' ',
  "AAAAUPPD" varchar(1) NOT NULL default ' ',
  "AAABMKXF" varchar(1) NOT NULL default ' ',
  "AAABMMVU" varchar(132) NOT NULL default ' ',
  "AAABLOZE" varchar(1) NOT NULL default ' ',
  "AAAAMDCZ" varchar(1) NOT NULL default ' ',
  "AAAAMDDA" varchar(1) NOT NULL default ' ',
  "AAAAMDDB" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVPS","AAAATVPT","AAABLNEE","AAABJHEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNNG" (
  "AAAATVPS" varchar(30) NOT NULL default ' ',
  "AAAATVPT" varchar(6) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABJECZ" varchar(4) NOT NULL default '0000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEZYX" varchar(1) NOT NULL default ' ',
  "AAABIRTO" varchar(1) NOT NULL default ' ',
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  "AAAAFODE" varchar(1) NOT NULL default ' ',
  "AAABLUWW" varchar(1) NOT NULL default ' ',
  "AAABJTEQ" varchar(6) NOT NULL default '000000',
  "AAABHCYR" varchar(30) NOT NULL default ' ',
  "AAABLDNU" varchar(4) NOT NULL default ' ',
  "AAABLBEI" varchar(6) NOT NULL default '000000',
  "AAABMLMY" varchar(6) NOT NULL default '000000',
  "AAABMNMM" varchar(6) NOT NULL default '000000',
  "AAABIANG" varchar(60) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVPS","AAAATVPT","AAABMREL","AAABJECZ","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNOF" (
  "AAAATVPS" varchar(30) NOT NULL default ' ',
  "AAAATVPT" varchar(6) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABKNBQ" varchar(1) NOT NULL default ' ',
  "AAABIRTT" varchar(8) NOT NULL default ' ',
  "AAAABNOM" varchar(12) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  "AAABKMZH" varchar(1) NOT NULL default ' ',
  "AAAABNOQ" varchar(60) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVPS","AAAATVPT","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNOW" (
  "AAAATVPS" varchar(30) NOT NULL default ' ',
  "AAAATVPT" varchar(6) NOT NULL default ' ',
  "AAABLNEE" varchar(30) NOT NULL default ' ',
  "AAABJHEY" varchar(26) NOT NULL default ' ',
  "AAABLNIW" varchar(21) NOT NULL default ' ',
  "AAAAUPPD" varchar(1) NOT NULL default ' ',
  "AAABMJWI" varchar(1) NOT NULL default ' ',
  "AAABMKXF" varchar(1) NOT NULL default ' ',
  "AAAABNPF" varchar(1) NOT NULL default ' ',
  "AAABMMVU" varchar(132) NOT NULL default ' ',
  "AAABLOZE" varchar(1) NOT NULL default ' ',
  "AAAAMDCZ" varchar(1) NOT NULL default ' ',
  "AAAAMDDA" varchar(1) NOT NULL default ' ',
  "AAAAMDDB" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVPS","AAAATVPT","AAABLNEE","AAABJHEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNPR" (
  "AAAATVPS" varchar(30) NOT NULL default ' ',
  "AAAATVPT" varchar(6) NOT NULL default ' ',
  "AAABLNEE" varchar(30) NOT NULL default ' ',
  "AAAABNQI" varchar(26) NOT NULL default ' ',
  "AAAAUPPD" varchar(1) NOT NULL default ' ',
  "AAABMJWI" varchar(1) NOT NULL default ' ',
  "AAABMMVU" varchar(132) NOT NULL default ' ',
  "AAABLOZE" varchar(1) NOT NULL default ' ',
  "AAAAMDCZ" varchar(1) NOT NULL default ' ',
  "AAAAMDDA" varchar(1) NOT NULL default ' ',
  "AAAAMDDB" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVPS","AAAATVPT","AAABLNEE","AAAABNQI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNQJ" (
  "AAABLUXP" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAABLUWW" varchar(8) NOT NULL default ' ',
  "AAABLUWX" varchar(8) NOT NULL default ' ',
  "AAABLUWY" varchar(8) NOT NULL default '00000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXP","AAABLUXQ","AAABLUWW","AAABLUWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNQU" (
  "AAAABNQZ" varchar(12) NOT NULL default ' ',
  "AAABIJSY" varchar(8) NOT NULL default '00000000',
  "AAABIJSZ" varchar(6) NOT NULL default '000000',
  "AAAAJMAB" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAAABNQZ","AAABIJSY","AAABIJSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNRC" (
  "AAABLUVB" varchar(10) NOT NULL default ' ',
  "AAABMONG" varchar(3) NOT NULL default ' ',
  "AAABMKFU" varchar(14) NOT NULL default ' ',
  "AAABLUVE" int(11) NOT NULL default '0',
  "AAABLUUU" varchar(8) NOT NULL default ' ',
  "AAABLUUV" varchar(8) NOT NULL default ' ',
  "AAABLUUW" varchar(1) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABLUUZ" int(11) NOT NULL default '0',
  "AAABMRCK" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUVB","AAABMONG","AAABMKFU","AAABLUVE"),
  KEY "AAAABNRS" ("AAABMKFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNRU" (
  "AAABLUVJ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNRX" (
  "AAABLUXP" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAABLUWW" varchar(8) NOT NULL default ' ',
  "AAABLUWX" varchar(8) NOT NULL default ' ',
  "AAABLUWY" varchar(8) NOT NULL default '00000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUVR" varchar(2) NOT NULL default ' ',
  "AAABLUVS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXP","AAABLUXQ","AAABLUWW","AAABLUWX","AAABLUWY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNSL" (
  "AAABLUUU" varchar(8) NOT NULL default ' ',
  "AAABMDXM" varchar(8) NOT NULL default ' ',
  "AAAABNSO" varchar(8) NOT NULL default ' ',
  "AAAABNSP" varchar(8) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUUU","AAABMDXM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNSU" (
  "AAAATVPT" varchar(6) NOT NULL default ' ',
  "AAABFGCN" varchar(30) NOT NULL default ' ',
  "AAABMIZS" varchar(30) NOT NULL default ' ',
  "AAABMMVU" varchar(4) NOT NULL default ' ',
  "AAABMLZG" int(11) NOT NULL default '0',
  "AAABMNMM" int(11) NOT NULL default '0',
  "AAABHWBK" int(11) NOT NULL default '0',
  "AAABLKHW" int(11) NOT NULL default '0',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVPT","AAABFGCN","AAABMIZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNTI" (
  "AAAATVPT" varchar(6) NOT NULL default ' ',
  "AAAABNTU" varchar(30) NOT NULL default ' ',
  "AAAABNTV" varchar(30) NOT NULL default ' ',
  "AAABCPJT" varchar(10) NOT NULL default ' ',
  "AAABLKHW" int(11) NOT NULL default '0',
  "AAABMJWH" varchar(1) NOT NULL default ' ',
  "AAABDNQB" varchar(1) NOT NULL default ' ',
  "AAAABNTQ" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVPT","AAAABNTU","AAAABNTV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNTW" (
  "AAAABNUY" varchar(10) NOT NULL default ' ',
  "AAAABNTY" varchar(10) NOT NULL default ' ',
  "AAABMNDU" varchar(40) NOT NULL default ' ',
  "AAABHHWW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAABNUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNUC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAABNUY" varchar(10) NOT NULL default ' ',
  "AAAABNUS" varchar(1) NOT NULL default ' ',
  "AAAAFNSE" varchar(8) NOT NULL default '00000000',
  "AAAABNUH" varchar(12) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAABNUY","AAAABNUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNUT" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAABNUY" varchar(10) NOT NULL default ' ',
  "AAABLPWW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAABNUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNUZ" (
  "AAABLUUU" varchar(8) NOT NULL default ' ',
  "AAABMDXM" varchar(8) NOT NULL default ' ',
  "AAABIMHJ" varchar(4) NOT NULL default '0000',
  "AAAABNVD" varchar(4) NOT NULL default '0000',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUUU","AAABMDXM","AAABIMHJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNVJ" (
  "AAAATVPS" varchar(30) NOT NULL default ' ',
  "AAABLUUU" varchar(8) NOT NULL default ' ',
  "AAABMDXM" varchar(8) NOT NULL default ' ',
  "AAABLMOZ" varchar(20) NOT NULL default ' ',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  "AAAAJWUY" varchar(60) NOT NULL default ' ',
  "AAABCCJN" varchar(20) NOT NULL default ' ',
  "AAAABNVS" varchar(1) NOT NULL default ' ',
  "AAABLAQG" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVPS","AAABLUUU","AAABMDXM","AAABLMOZ","AAABLLBG","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNWD" (
  "AAABLUUU" varchar(8) NOT NULL default ' ',
  "AAABMDXM" varchar(8) NOT NULL default ' ',
  "AAABLENL" varchar(8) NOT NULL default '00000000',
  "AAABLENM" varchar(2) NOT NULL default ' ',
  "AAABLENN" varchar(6) NOT NULL default ' ',
  "AAABLENO" varchar(6) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  "AAABLENR" varchar(80) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUUU","AAABMDXM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNWQ" (
  "AAABLUXP" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAABLUWM" varchar(32) NOT NULL default ' ',
  "AAABLUWN" varchar(32) NOT NULL default ' ',
  "AAABMOHM" varchar(3) NOT NULL default ' ',
  "AAABLUWE" varchar(80) NOT NULL default ' ',
  "AAABLUWF" varchar(1) NOT NULL default ' ',
  "AAABLUWG" varchar(10) NOT NULL default ' ',
  "AAABLUWH" varchar(3) NOT NULL default ' ',
  "AAABLUWI" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXP","AAABLUXQ","AAABLUWM","AAABLUWN","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNXH" (
  "AAABLUXP" varchar(10) NOT NULL default '0000000000',
  "AAABLUWW" varchar(8) NOT NULL default ' ',
  "AAABLUWX" varchar(8) NOT NULL default ' ',
  "AAABLUWY" varchar(8) NOT NULL default '00000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXP","AAABLUWW","AAABLUWX","AAABLUWY","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNXS" (
  "AAAAGEIF" varchar(2) NOT NULL default ' ',
  "AAABLUXP" varchar(13) NOT NULL default ' ',
  "AAABLFEA" varchar(8) NOT NULL default '00000000',
  "AAAAKZCT" varchar(6) NOT NULL default '000000',
  "AAAAGDTE" varchar(12) NOT NULL default ' ',
  "AAABJQCK" varchar(8) NOT NULL default '00000000',
  "AAABJQCL" varchar(6) NOT NULL default '000000',
  "AAABJQCM" varchar(12) NOT NULL default ' ',
  "AAAAGDTI" varchar(1) NOT NULL default ' ',
  "AAABLILS" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLAQG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAGEIF","AAABLUXP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNYH" (
  "AAABJTNV" varchar(30) NOT NULL default ' ',
  "AAAABNZS" varchar(1) NOT NULL default ' ',
  "AAABMNDU" varchar(40) NOT NULL default ' ',
  "AAABHHWW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABJTNV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNYN" (
  "AAABJTNV" varchar(30) NOT NULL default ' ',
  "AAAABNZS" varchar(1) NOT NULL default ' ',
  "AAABMNDU" varchar(40) NOT NULL default ' ',
  "AAABHHWW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABJTNV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNYT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAABNZK" varchar(10) NOT NULL default ' ',
  "AAAABNZL" varchar(10) NOT NULL default ' ',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAABOAT" varchar(32) NOT NULL default ' ',
  "AAABMNDU" varchar(40) NOT NULL default ' ',
  "AAAAVKAP" varchar(10) NOT NULL default ' ',
  "AAAABNZC" varchar(8) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABLMKH" varchar(8) NOT NULL default '00000000',
  "AAABLIRO" varchar(6) NOT NULL default '000000',
  "AAABKUQR" varchar(8) NOT NULL default '00000000',
  "AAABLWWO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUXQ","AAAABNZK","AAAABNZL","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABNZN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAABOAT" varchar(32) NOT NULL default ' ',
  "AAABJTNV" varchar(30) NOT NULL default ' ',
  "AAABMMRH" varchar(4) NOT NULL default '0000',
  "AAAABNZS" varchar(1) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAABOAT","AAABJTNV","AAABMMRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOAE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAABOAT" varchar(32) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABJEMU" varchar(30) NOT NULL default ' ',
  "AAAAGVMD" int(11) NOT NULL default '0',
  "AAAAGVME" int(11) NOT NULL default '0',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAABOAT","AAABMREL","AAABJEMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOAW" (
  "AAABLECD" varchar(2) NOT NULL default ' ',
  "AAABMCNO" varchar(5) NOT NULL default ' ',
  "AAABLUNJ" varchar(5) NOT NULL default ' ',
  "AAAABOBH" varchar(5) NOT NULL default ' ',
  "AAABMNRG" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABMCNO","AAABLUNJ","AAAABOBH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOBI" (
  "AAABLUXP" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAABLUXD" varchar(4) NOT NULL default ' ',
  "AAABLUXE" varchar(4) NOT NULL default ' ',
  "AAABLUXF" varchar(1) NOT NULL default ' ',
  "AAABLUXG" varchar(4) NOT NULL default ' ',
  "AAABLUXH" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXP","AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOBT" (
  "AAABLUXP" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXP","AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOBZ" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLUXP" varchar(10) NOT NULL default ' ',
  "AAABMONG" varchar(3) NOT NULL default ' ',
  "AAAABOCN" varchar(10) NOT NULL default '0000000000',
  "AAABLETY" varchar(8) NOT NULL default ' ',
  "AAAADNQZ" varchar(10) NOT NULL default '0000000000',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHDEA" varchar(2) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLUXP","AAABMONG","AAAABOCN","AAABLETY","AAAADNQZ","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOCR" (
  "AAABLUXU" varchar(20) NOT NULL default ' ',
  "AAABLUXT" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOCV" (
  "AAAATVPS" varchar(30) NOT NULL default ' ',
  "AAAATVPT" varchar(6) NOT NULL default ' ',
  "AAABLKHW" varchar(4) NOT NULL default '0000',
  "AAABGZHX" varchar(72) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVPS","AAAATVPT","AAABLKHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABODF" (
  "AAABJDRC" varchar(30) NOT NULL default ' ',
  "AAAATVPS" varchar(30) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABJDRC","AAAATVPS","AAABLUXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABODM" (
  "AAABLUUU" varchar(8) NOT NULL default ' ',
  "AAABLUUV" varchar(8) NOT NULL default ' ',
  "AAABMOIA" varchar(8) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUUU","AAABLUUV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABODU" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(22) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAAAUGRJ" varchar(10) NOT NULL default ' ',
  "AAAABODZ" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOEF" (
  "AAAATVPT" varchar(6) NOT NULL default ' ',
  "AAABMLPX" varchar(32) NOT NULL default ' ',
  "AAAATVPA" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVPT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOEK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAEVZG" varchar(32) NOT NULL default ' ',
  "AAAADNQZ" varchar(10) NOT NULL default ' ',
  "AAAABOEO" varchar(1) NOT NULL default ' ',
  "AAAABOEP" varchar(32) NOT NULL default ' ',
  "AAABCFHS" varchar(10) NOT NULL default ' ',
  "AAAAJMAB" varchar(10) NOT NULL default ' ',
  "AAABLGCM" varchar(15) NOT NULL default ' ',
  "AAAABOET" varchar(1) NOT NULL default ' ',
  "AAAABOEU" varchar(1) NOT NULL default ' ',
  "AAAABOEV" varchar(1) NOT NULL default ' ',
  "AAAABOEW" varchar(1) NOT NULL default ' ',
  "AAAABOEX" varchar(1) NOT NULL default ' ',
  "AAAABOEY" varchar(1) NOT NULL default ' ',
  "AAAABOEZ" varchar(1) NOT NULL default ' ',
  "AAAABOFA" varchar(1) NOT NULL default ' ',
  "AAAABOFB" varchar(1) NOT NULL default ' ',
  "AAAABOFC" varchar(1) NOT NULL default ' ',
  "AAAABOFD" varchar(1) NOT NULL default ' ',
  "AAAABOFE" varchar(1) NOT NULL default ' ',
  "AAAABOFF" varchar(1) NOT NULL default ' ',
  "AAAABOFG" varchar(1) NOT NULL default ' ',
  "AAAABOFH" varchar(1) NOT NULL default ' ',
  "AAAABOFI" varchar(1) NOT NULL default ' ',
  "AAAABOFJ" varchar(1) NOT NULL default ' ',
  "AAAABOFK" varchar(1) NOT NULL default ' ',
  "AAAABOFL" varchar(1) NOT NULL default ' ',
  "AAAABOFM" varchar(1) NOT NULL default ' ',
  "AAAABOFN" varchar(1) NOT NULL default ' ',
  "AAAABOFO" varchar(1) NOT NULL default ' ',
  "AAAABOFP" varchar(1) NOT NULL default ' ',
  "AAAABOFQ" varchar(1) NOT NULL default ' ',
  "AAAABOFR" varchar(1) NOT NULL default ' ',
  "AAAABOFS" varchar(1) NOT NULL default ' ',
  "AAAABOFT" varchar(1) NOT NULL default ' ',
  "AAAABOFU" varchar(1) NOT NULL default ' ',
  "AAAABOFV" varchar(1) NOT NULL default ' ',
  "AAAABOFW" varchar(1) NOT NULL default ' ',
  "AAABLJJO" varchar(3) NOT NULL default ' ',
  "AAAABOFY" varchar(30) NOT NULL default ' ',
  "AAAABOFZ" varchar(30) NOT NULL default ' ',
  "AAABMIRY" varchar(4) NOT NULL default ' ',
  "AAABJTNV" varchar(32) NOT NULL default ' ',
  "AAABJAEL" varchar(8) NOT NULL default '00000000',
  "AAABLOCX" varchar(6) NOT NULL default '000000',
  "AAAAEWCG" varchar(8) NOT NULL default '00000000',
  "AAAAEWCH" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAAAEVZG","AAAADNQZ"),
  KEY "AAAABOGJ" ("AAABMRIL","AAABLGCM","AAABJAEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOGN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLFYC" varchar(10) NOT NULL default ' ',
  "AAABLNYQ" varchar(4) NOT NULL default '0000',
  "AAABDEKG" varchar(2) NOT NULL default ' ',
  "AAABDEKF" varchar(10) NOT NULL default ' ',
  "AAABDEKH" varchar(2) NOT NULL default ' ',
  "AAABJFLQ" varchar(8) NOT NULL default '00000000',
  "AAABHFNY" varchar(6) NOT NULL default '000000',
  "AAAAJAKE" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLFYC","AAABLNYQ","AAABDEKG","AAABDEKF","AAABDEKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOHD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLFYC" varchar(10) NOT NULL default ' ',
  "AAABLNYQ" varchar(4) NOT NULL default '0000',
  "AAABEDRM" varchar(2) NOT NULL default ' ',
  "AAABLMHG" varchar(10) NOT NULL default ' ',
  "AAABCKEF" varchar(2) NOT NULL default ' ',
  "AAABJFLQ" varchar(8) NOT NULL default '00000000',
  "AAABHFNY" varchar(6) NOT NULL default '000000',
  "AAAAJAKE" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLFYC","AAABLNYQ","AAABEDRM","AAABLMHG","AAABCKEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOHT" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOIA" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOIH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOIQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOIZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOJI" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOJP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOJY" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOKF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOKX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOLG" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOLN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOLW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOMF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOMO" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOMV" (
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
CREATE TABLE "AAAABONG" (
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
CREATE TABLE "AAAABONR" (
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
CREATE TABLE "AAAABOOE" (
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
CREATE TABLE "AAAABOOR" (
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
CREATE TABLE "AAAABOPE" (
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
CREATE TABLE "AAAABOPP" (
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
CREATE TABLE "AAAABOQC" (
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
CREATE TABLE "AAAABOQN" (
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
CREATE TABLE "AAAABORA" (
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
CREATE TABLE "AAAABORN" (
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
CREATE TABLE "AAAABOSA" (
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
CREATE TABLE "AAAABOSL" (
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
CREATE TABLE "AAAABOSY" (
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
CREATE TABLE "AAAABOTL" (
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
CREATE TABLE "AAAABOTY" (
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
CREATE TABLE "AAAABOUJ" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOUT" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOVD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOVP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOWB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOWN" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOWX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOXJ" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOXT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOYR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOZD" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOZN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABOZZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABPAL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABPAX" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABPBH" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABPBR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDEKG" varchar(2) NOT NULL default ' ',
  "AAABDEKF" varchar(10) NOT NULL default ' ',
  "AAABDEKH" varchar(2) NOT NULL default ' ',
  "AAABEDRM" varchar(2) NOT NULL default ' ',
  "AAABLMHG" varchar(10) NOT NULL default ' ',
  "AAABCKEF" varchar(2) NOT NULL default ' ',
  "AAABCJWA" varchar(30) NOT NULL default ' ',
  "AAABCJTM" varchar(120) NOT NULL default ' ',
  "AAABFAAL" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDEKG","AAABDEKF","AAABDEKH","AAABEDRM","AAABLMHG","AAABCKEF","AAABCJWA","AAABCJTM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABPCL" (
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
  KEY "AAAABPCZ" ("AAABMRFO","AAABMLQA","AAABHZDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABPDD" (
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
  KEY "AAAABPDR" ("AAABMRFO","AAABMLQA","AAABHZDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABPDV" (
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
  KEY "AAAABPEO" ("AAABLLLF","AAABLLLJ"),
  KEY "AAAABPER" ("AAABLLLG"),
  KEY "AAAABPET" ("AAABLLLH","AAABLLLI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABPEW" (
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
CREATE TABLE "AAAABPFP" (
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
CREATE TABLE "AAAABPGK" (
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
CREATE TABLE "AAAABPHF" (
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
CREATE TABLE "AAAABPIA" (
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
CREATE TABLE "AAAABPIT" (
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
CREATE TABLE "AAAABPJO" (
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
CREATE TABLE "AAAABPKH" (
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
CREATE TABLE "AAAABPLC" (
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
CREATE TABLE "AAAABPLX" (
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
CREATE TABLE "AAAABPMS" (
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
CREATE TABLE "AAAABPNL" (
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
CREATE TABLE "AAAABPOG" (
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
CREATE TABLE "AAAABPPB" (
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
CREATE TABLE "AAAABPPW" (
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
  KEY "AAAABPQP" ("AAABLLLA","AAABLLLJ"),
  KEY "AAAABPQS" ("AAABLLLH","AAABLLLI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABPQV" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABPRC" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABPRJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABPRS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABPSB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABPSK" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABPSR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABPTA" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABPTH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABPTQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABPTZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABPUI" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABPUP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABPUY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABPVH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABPVQ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABPVX" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAABPWH" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABPWL" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAABPWV" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABPWZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAABPXL" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABPXP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAABPYB" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABPYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABPYR" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAABPZB" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABPZF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABPZR" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAABQAB" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABQAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABQAR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABQBD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABQBP" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAABQBZ" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABQCD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAABQCP" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABQCT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABQDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABQDR" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAABQEB" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABQEF" (
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
  UNIQUE KEY "AAAABQES" ("AAABLLNL"),
  KEY "AAAABQEU" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABQEW" (
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
  UNIQUE KEY "AAAABQFJ" ("AAABLLNL"),
  KEY "AAAABQFL" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABQFN" (
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
  UNIQUE KEY "AAAABQGC" ("AAABMRIL","AAABLLNL"),
  KEY "AAAABQGF" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABQGH" (
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
  UNIQUE KEY "AAAABQGW" ("AAABMRIL","AAABLLNL"),
  KEY "AAAABQGZ" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABQHB" (
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
CREATE TABLE "AAAABQHQ" (
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
  UNIQUE KEY "AAAABQID" ("AAABLLNL"),
  KEY "AAAABQIF" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABQIH" (
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
CREATE TABLE "AAAABQIW" (
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
  UNIQUE KEY "AAAABQJJ" ("AAABLLNL"),
  KEY "AAAABQJL" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABQJN" (
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
CREATE TABLE "AAAABQKC" (
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
CREATE TABLE "AAAABQKR" (
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
CREATE TABLE "AAAABQLG" (
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
  UNIQUE KEY "AAAABQLT" ("AAABLLNL"),
  KEY "AAAABQLV" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABQLX" (
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
  UNIQUE KEY "AAAABQMM" ("AAABMRIL","AAABLLNL"),
  KEY "AAAABQMP" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABQMR" (
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
CREATE TABLE "AAAABQNG" (
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
CREATE TABLE "AAAABQNV" (
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
  UNIQUE KEY "AAAABQOI" ("AAABLLNL"),
  KEY "AAAABQOK" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABQOM" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNN","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAABQOZ" ("AAABLLMD","AAABLLME","AAABLLND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABQPD" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNN","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAABQPQ" ("AAABLLMD","AAABLLME","AAABLLND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABQPU" (
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
  UNIQUE KEY "AAAABQQH" ("AAABLLNL"),
  KEY "AAAABQQJ" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABQQL" (
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
  UNIQUE KEY "AAAABQQY" ("AAABLLNL"),
  KEY "AAAABQRA" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABQRC" (
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
  UNIQUE KEY "AAAABQRR" ("AAABMRIL","AAABLLNL"),
  KEY "AAAABQRU" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABQRW" (
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
  UNIQUE KEY "AAAABQSL" ("AAABMRIL","AAABLLNL"),
  KEY "AAAABQSO" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABQSQ" (
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
CREATE TABLE "AAAABQTF" (
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
  UNIQUE KEY "AAAABQTS" ("AAABLLNL"),
  KEY "AAAABQTU" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABQTW" (
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
CREATE TABLE "AAAABQUL" (
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
  UNIQUE KEY "AAAABQUY" ("AAABLLNL"),
  KEY "AAAABQVA" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABQVC" (
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
CREATE TABLE "AAAABQVR" (
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
CREATE TABLE "AAAABQWG" (
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
CREATE TABLE "AAAABQWV" (
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
  UNIQUE KEY "AAAABQXI" ("AAABLLNL"),
  KEY "AAAABQXK" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABQXM" (
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
  UNIQUE KEY "AAAABQYB" ("AAABMRIL","AAABLLNL"),
  KEY "AAAABQYE" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABQYG" (
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
CREATE TABLE "AAAABQYV" (
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
CREATE TABLE "AAAABQZK" (
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
  UNIQUE KEY "AAAABQZX" ("AAABLLNL"),
  KEY "AAAABQZZ" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABRAB" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNI","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAABRAO" ("AAABLLMD","AAABLLME","AAABLLNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABRAS" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNI","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAABRBF" ("AAABLLMD","AAABLLME","AAABLLNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABRBJ" (
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
CREATE TABLE "AAAABRBV" (
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
CREATE TABLE "AAAABRCH" (
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
CREATE TABLE "AAAABRCV" (
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
CREATE TABLE "AAAABRDJ" (
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
CREATE TABLE "AAAABRDX" (
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
CREATE TABLE "AAAABREJ" (
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
CREATE TABLE "AAAABREX" (
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
CREATE TABLE "AAAABRFJ" (
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
CREATE TABLE "AAAABRFX" (
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
CREATE TABLE "AAAABRGL" (
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
CREATE TABLE "AAAABRGZ" (
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
CREATE TABLE "AAAABRHL" (
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
CREATE TABLE "AAAABRHZ" (
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
CREATE TABLE "AAAABRIN" (
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
CREATE TABLE "AAAABRJB" (
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
CREATE TABLE "AAAABRJN" (
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
  UNIQUE KEY "AAAABRKB" ("AAABLLNL"),
  KEY "AAAABRKD" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABRKF" (
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
  UNIQUE KEY "AAAABRKT" ("AAABLLNL"),
  KEY "AAAABRKV" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABRKX" (
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
  UNIQUE KEY "AAAABRLN" ("AAABMRIL","AAABLLNL"),
  KEY "AAAABRLQ" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABRLS" (
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
  UNIQUE KEY "AAAABRMI" ("AAABMRIL","AAABLLNL"),
  KEY "AAAABRML" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABRMN" (
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
  PRIMARY KEY ("AAABMRIL","AAABLLNN","AAABLLNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABRND" (
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
  UNIQUE KEY "AAAABRNR" ("AAABLLNL"),
  KEY "AAAABRNT" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABRNV" (
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
  PRIMARY KEY ("AAABMRIL","AAABLLNN","AAABLLNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABROL" (
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
  UNIQUE KEY "AAAABROZ" ("AAABLLNL"),
  KEY "AAAABRPB" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABRPD" (
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
  PRIMARY KEY ("AAABMRIL","AAABLLNN","AAABLLNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABRPT" (
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
  PRIMARY KEY ("AAABMRIL","AAABLLNN","AAABLLNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABRQJ" (
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
  PRIMARY KEY ("AAABMRIL","AAABLLNN","AAABLLNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABRQZ" (
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
  UNIQUE KEY "AAAABRRN" ("AAABLLNL"),
  KEY "AAAABRRP" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABRRR" (
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
  UNIQUE KEY "AAAABRSH" ("AAABMRIL","AAABLLNL"),
  KEY "AAAABRSK" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABRSM" (
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
  PRIMARY KEY ("AAABMRIL","AAABLLNN","AAABLLNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABRTC" (
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
  PRIMARY KEY ("AAABMRIL","AAABLLNN","AAABLLNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABRTS" (
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
  UNIQUE KEY "AAAABRUG" ("AAABLLNL"),
  KEY "AAAABRUI" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABRUK" (
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
CREATE TABLE "AAAABRVK" (
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
CREATE TABLE "AAAABRWK" (
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
  KEY "AAAABRXM" ("AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABRXO" (
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
  KEY "AAAABRYQ" ("AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABRYS" (
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
CREATE TABLE "AAAABRZU" (
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
  KEY "AAAABSAU" ("AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABSAW" (
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
CREATE TABLE "AAAABSBY" (
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
CREATE TABLE "AAAABSCY" (
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
CREATE TABLE "AAAABSEA" (
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
CREATE TABLE "AAAABSFC" (
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
CREATE TABLE "AAAABSGE" (
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
CREATE TABLE "AAAABSHE" (
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
  KEY "AAAABSIG" ("AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABSII" (
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
CREATE TABLE "AAAABSJK" (
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
CREATE TABLE "AAAABSKM" (
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
CREATE TABLE "AAAABSLM" (
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
  UNIQUE KEY "AAAABSMA" ("AAABLLNL"),
  KEY "AAAABSMC" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABSME" (
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
  UNIQUE KEY "AAAABSMS" ("AAABLLNL"),
  KEY "AAAABSMU" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABSMW" (
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
  UNIQUE KEY "AAAABSNM" ("AAABMRIL","AAABLLNL"),
  KEY "AAAABSNP" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABSNR" (
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
  UNIQUE KEY "AAAABSOH" ("AAABMRIL","AAABLLNL"),
  KEY "AAAABSOK" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABSOM" (
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
  PRIMARY KEY ("AAABMRIL","AAABLLNI","AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABSPC" (
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
  UNIQUE KEY "AAAABSPQ" ("AAABLLNL"),
  KEY "AAAABSPS" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABSPU" (
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
  PRIMARY KEY ("AAABMRIL","AAABLLNI","AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABSQK" (
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
  UNIQUE KEY "AAAABSQY" ("AAABLLNL"),
  KEY "AAAABSRA" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABSRC" (
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
  PRIMARY KEY ("AAABMRIL","AAABLLNI","AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABSRS" (
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
  PRIMARY KEY ("AAABMRIL","AAABLLNI","AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABSSI" (
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
  PRIMARY KEY ("AAABMRIL","AAABLLNI","AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABSSY" (
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
  UNIQUE KEY "AAAABSTM" ("AAABLLNL"),
  KEY "AAAABSTO" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABSTQ" (
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
  UNIQUE KEY "AAAABSUG" ("AAABMRIL","AAABLLNL"),
  KEY "AAAABSUJ" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABSUL" (
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
  PRIMARY KEY ("AAABMRIL","AAABLLNI","AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABSVB" (
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
  PRIMARY KEY ("AAABMRIL","AAABLLNI","AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABSVR" (
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
  UNIQUE KEY "AAAABSWF" ("AAABLLNL"),
  KEY "AAAABSWH" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABSWJ" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAABSWT" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABSWX" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAABSXH" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABSXL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAABSXX" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABSYB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAABSYN" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABSYR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABSZD" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAABSZN" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABSZR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABTAD" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAABTAN" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABTAR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABTBD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABTBP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABTCB" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAABTCL" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABTCP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAABTDB" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABTDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABTDR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABTED" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAABTEN" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABTER" (
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
  UNIQUE KEY "AAAABTFE" ("AAABLLNL"),
  KEY "AAAABTFG" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABTFI" (
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
  UNIQUE KEY "AAAABTFV" ("AAABLLNL"),
  KEY "AAAABTFX" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABTFZ" (
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
  UNIQUE KEY "AAAABTGO" ("AAABMRIL","AAABLLNL"),
  KEY "AAAABTGR" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABTGT" (
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
  UNIQUE KEY "AAAABTHI" ("AAABMRIL","AAABLLNL"),
  KEY "AAAABTHL" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABTHN" (
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
CREATE TABLE "AAAABTIC" (
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
  UNIQUE KEY "AAAABTIP" ("AAABLLNL"),
  KEY "AAAABTIR" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABTIT" (
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
CREATE TABLE "AAAABTJI" (
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
  UNIQUE KEY "AAAABTJV" ("AAABLLNL"),
  KEY "AAAABTJX" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAABTJZ" (
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

--enable_query_log
