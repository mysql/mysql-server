# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+
# |                             tbl06.inc                              |
# |                        Copyright 2006 by MySQL AB                  |
# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+

--disable_query_log
CREATE TABLE "AAAAEOQT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLENG" varchar(3) NOT NULL default ' ',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAAAEOQZ" varchar(1) NOT NULL default ' ',
  "AAAAEORA" varchar(1) NOT NULL default ' ',
  "AAAAEORB" varchar(1) NOT NULL default ' ',
  "AAAAEORC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLENG","AAABLTXG","AAABLXTG","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEORI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLENG" varchar(3) NOT NULL default ' ',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABLENG","AAABLTXG","AAABLXTG","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEORT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLENG" varchar(3) NOT NULL default ' ',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABLENG","AAABLTXG","AAABLXTG","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEOSE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLENG" varchar(3) NOT NULL default ' ',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAAAEOSK" varchar(8) NOT NULL default ' ',
  "AAAAEOSL" varchar(5) NOT NULL default ' ',
  "AAAAEOSM" varchar(1) NOT NULL default ' ',
  "AAAAEOSN" varchar(8) NOT NULL default ' ',
  "AAAAEOSO" varchar(5) NOT NULL default ' ',
  "AAAAEOSP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLENG","AAABLTXG","AAABLXTG","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEOSV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLENG" varchar(3) NOT NULL default ' ',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAAAQNPB" varchar(1) NOT NULL default ' ',
  "AAAAEOTC" varchar(1) NOT NULL default ' ',
  "AAAAEOTD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLENG","AAABLTXG","AAABLXTG","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEOTJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLENG" varchar(3) NOT NULL default ' ',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAABKQPE" varchar(2) NOT NULL default '00',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLENG","AAABLTXG","AAABLXTG","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEOTW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLENG" varchar(3) NOT NULL default ' ',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAAAEOUC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLENG","AAABLTXG","AAABLXTG","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEOUI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLENG" varchar(3) NOT NULL default ' ',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABLENG","AAABLTXG","AAABLXTG","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEOUT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLENG" varchar(3) NOT NULL default ' ',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABLENG","AAABLTXG","AAABLXTG","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEOVE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLENG" varchar(3) NOT NULL default ' ',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABLENG","AAABLTXG","AAABLXTG","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEOVP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLENG" varchar(3) NOT NULL default ' ',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLENG","AAABLTXG","AAABLXTG","AAABMLPO","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEOWG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLENG" varchar(3) NOT NULL default ' ',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAABDVOH" varchar(1) NOT NULL default ' ',
  "AAAAEOWN" varchar(1) NOT NULL default ' ',
  "AAAAEOWO" varchar(8) NOT NULL default '00000000',
  "AAAAEOWP" varchar(6) NOT NULL default '000000',
  "AAAAEOWQ" decimal(5,0) NOT NULL default '0',
  "AAAAEOWR" varchar(3) NOT NULL default ' ',
  "AAAAEOWS" varchar(1) NOT NULL default ' ',
  "AAAAEOWT" varchar(8) NOT NULL default '00000000',
  "AAAAEOWU" varchar(6) NOT NULL default '000000',
  "AAAAEOWV" decimal(5,0) NOT NULL default '0',
  "AAAAEOWW" varchar(3) NOT NULL default ' ',
  "AAAAEOWX" varchar(1) NOT NULL default ' ',
  "AAAAEOWY" varchar(8) NOT NULL default '00000000',
  "AAAAEOWZ" varchar(6) NOT NULL default '000000',
  "AAAAEOXA" decimal(5,0) NOT NULL default '0',
  "AAAAEOXB" varchar(3) NOT NULL default ' ',
  "AAAAEOXC" varchar(1) NOT NULL default ' ',
  "AAAAEOXD" varchar(8) NOT NULL default '00000000',
  "AAAAEOXE" varchar(6) NOT NULL default '000000',
  "AAAAEOXF" decimal(5,0) NOT NULL default '0',
  "AAAAEOXG" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLENG","AAABLTXG","AAABLXTG","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEOXM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLENG" varchar(3) NOT NULL default ' ',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAAAEOXS" varchar(1) NOT NULL default ' ',
  "AAAAEOXT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLENG","AAABLTXG","AAABLXTG","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEOXZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAPSFM" varchar(4) NOT NULL default ' ',
  "AAAAEOYC" varchar(2) NOT NULL default ' ',
  "AAAAEOYD" varchar(2) NOT NULL default ' ',
  "AAAAEOYE" varchar(1) NOT NULL default ' ',
  "AAAAEOYF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAPSFM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEOYI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAPSFM" varchar(4) NOT NULL default ' ',
  "AAAAEOYM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAPSFM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEOYQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHCXM" varchar(8) NOT NULL default ' ',
  "AAABJDAS" varchar(4) NOT NULL default ' ',
  "AAAAERHJ" varchar(1) NOT NULL default ' ',
  "AAAAEOYV" varchar(1) NOT NULL default ' ',
  "AAAAERHO" varchar(1) NOT NULL default ' ',
  "AAAAEOYX" varchar(8) NOT NULL default ' ',
  "AAAAERHT" varchar(1) NOT NULL default ' ',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  "AAABLTZM" varchar(8) NOT NULL default '00000000',
  "AAABLTZN" varchar(12) NOT NULL default ' ',
  "AAABLTYV" varchar(8) NOT NULL default '00000000',
  "AAABLTYW" varchar(12) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHCXM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEOZH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHCXM" varchar(8) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAPXCA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHCXM","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEOZP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCFHS" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLEPD" decimal(13,3) NOT NULL default '0.000',
  "AAABLEPE" decimal(13,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABCFHS","AAABMQII","AAABMJXV","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPAC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCFHS" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABLEPD" decimal(13,3) NOT NULL default '0.000',
  "AAABLEPE" decimal(13,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABCFHS","AAABMQII","AAABMJXV","AAABLXOB","AAABMCVK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPAR" (
  "AAABIJUH" varchar(3) NOT NULL default '000',
  "AAAAQZHL" varchar(22) NOT NULL default ' ',
  "AAABKQWU" varchar(1) NOT NULL default ' ',
  "AAABKQWS" varchar(8) NOT NULL default ' ',
  "AAABKQWT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABIJUH","AAAAQZHL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPAZ" (
  "AAABIJUH" varchar(3) NOT NULL default '000',
  "AAAAQZHL" varchar(22) NOT NULL default ' ',
  "AAABKQWU" varchar(1) NOT NULL default ' ',
  "AAABKQWS" varchar(8) NOT NULL default ' ',
  "AAABKQPD" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABIJUH","AAAAQZHL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPBH" (
  "AAABIJUH" varchar(3) NOT NULL default '000',
  "AAAAEPBS" varchar(22) NOT NULL default ' ',
  "AAABKWWJ" longblob,
  "AAAAPWCX" varchar(22) NOT NULL default ' ',
  "AAAAQZHL" varchar(22) NOT NULL default ' ',
  "AAABMKFU" decimal(21,7) NOT NULL default '0.0000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAEPBP" VARBINARY(255),
  "AAAAEPBU" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABIJUH","AAAAEPBS"),
  KEY "AAAAEPBT" ("AAAAEPBU"),
  KEY "AAAAEPBV" ("AAAAPWCX"),
  KEY "AAAAEPBX" ("AAAAQZHL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPBZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABJZMF" varchar(5) NOT NULL default ' ',
  "AAABLRLF" varchar(14) NOT NULL default ' ',
  "AAAAEQKU" varchar(7) NOT NULL default '0000000',
  "AAAAEQKV" decimal(15,2) NOT NULL default '0.00',
  "AAAAEQKW" decimal(15,2) NOT NULL default '0.00',
  "AAAAEQKX" varchar(5) NOT NULL default ' ',
  "AAAAEQKY" decimal(5,0) NOT NULL default '0',
  "AAAAEQKZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAABJZMF","AAABLRLF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPCO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJZMF" varchar(5) NOT NULL default ' ',
  "AAABJEUU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAABMLQA","AAABJZMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPCY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLUER" varchar(4) NOT NULL default ' ',
  "AAAAEPDT" varchar(3) NOT NULL default '000',
  "AAAAEPDD" varchar(1) NOT NULL default '0',
  "AAAAEPDE" varchar(1) NOT NULL default '0',
  "AAAAEPDF" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAABLUER","AAAAEPDT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPDK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLUER" varchar(4) NOT NULL default ' ',
  "AAAAEPDT" varchar(3) NOT NULL default '000',
  "AAAAEPDU" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAABLUER","AAAAEPDT","AAAAEPDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPDV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAEQHG" varchar(5) NOT NULL default ' ',
  "AAABLRLF" varchar(14) NOT NULL default ' ',
  "AAAAEPEA" varchar(7) NOT NULL default '0000000',
  "AAAAEPEB" varchar(4) NOT NULL default ' ',
  "AAAAEPEC" varchar(1) NOT NULL default ' ',
  "AAAAEPED" varchar(5) NOT NULL default ' ',
  "AAAAEPLD" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAAAEQHG","AAABLRLF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPEJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEQHG" varchar(5) NOT NULL default ' ',
  "AAABJEUU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAABMLQA","AAAAEQHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPET" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAEPYK" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAAAEPYK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPFA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEPYK" varchar(2) NOT NULL default ' ',
  "AAAAEPFF" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAABMLQA","AAAAEPYK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPFK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABFNVZ" varchar(10) NOT NULL default ' ',
  "AAABLDUY" varchar(4) NOT NULL default ' ',
  "AAABLRLF" varchar(14) NOT NULL default ' ',
  "AAAAEQMY" varchar(4) NOT NULL default '0000',
  "AAAAEQHN" varchar(5) NOT NULL default ' ',
  "AAAAEQHO" varchar(3) NOT NULL default ' ',
  "AAAAEQHP" varchar(5) NOT NULL default ' ',
  "AAAAEQHQ" varchar(3) NOT NULL default ' ',
  "AAAAEQHG" varchar(5) NOT NULL default ' ',
  "AAAAEQHH" decimal(16,8) NOT NULL default '0.00000000',
  "AAAAEQHI" decimal(16,8) NOT NULL default '0.00000000',
  "AAAAEQHJ" decimal(16,8) NOT NULL default '0.00000000',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAABFNVZ","AAABLDUY","AAABLRLF","AAAAEQMY","AAAAEQHN","AAAAEQHO","AAAAEQHP","AAAAEQHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPGJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABFNVZ" varchar(10) NOT NULL default ' ',
  "AAABLDUY" varchar(4) NOT NULL default ' ',
  "AAABLRLF" varchar(14) NOT NULL default ' ',
  "AAAAEQMY" varchar(4) NOT NULL default '0000',
  "AAAAEQIN" varchar(1) NOT NULL default ' ',
  "AAAAEQMV" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAABFNVZ","AAABLDUY","AAABLRLF","AAAAEQMY","AAAAEQIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPGZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABFNVZ" varchar(10) NOT NULL default ' ',
  "AAABLDUY" varchar(4) NOT NULL default ' ',
  "AAABLRLF" varchar(14) NOT NULL default ' ',
  "AAAAEQCL" varchar(3) NOT NULL default ' ',
  "AAAAEQCM" varchar(5) NOT NULL default ' ',
  "AAAAEQCN" decimal(5,0) NOT NULL default '0',
  "AAAAEQCO" varchar(3) NOT NULL default ' ',
  "AAAAEQCP" varchar(7) NOT NULL default '0000000',
  "AAAAEQCQ" varchar(7) NOT NULL default '0000000',
  "AAAAEQCR" varchar(5) NOT NULL default ' ',
  "AAAAEQCS" decimal(15,3) NOT NULL default '0.000',
  "AAAAEQCT" decimal(15,3) NOT NULL default '0.000',
  "AAAAEQCU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAABFNVZ","AAABLDUY","AAABLRLF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPHU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFNVZ" varchar(10) NOT NULL default ' ',
  "AAABLDUY" varchar(4) NOT NULL default ' ',
  "AAAAYWCI" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAABMLQA","AAABFNVZ","AAABLDUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPIG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAEPJR" varchar(5) NOT NULL default ' ',
  "AAABLRLF" varchar(14) NOT NULL default ' ',
  "AAAAEQDK" varchar(7) NOT NULL default '0000000',
  "AAAAEQDL" varchar(7) NOT NULL default '0000000',
  "AAAAEQDM" varchar(7) NOT NULL default '0000000',
  "AAAAEQDN" varchar(7) NOT NULL default '0000000',
  "AAAAEQDO" decimal(3,0) NOT NULL default '0',
  "AAAAEQDP" varchar(7) NOT NULL default '0000000',
  "AAAAEQDQ" decimal(3,0) NOT NULL default '0',
  "AAAAEQDR" decimal(3,0) NOT NULL default '0',
  "AAAAEQDS" varchar(1) NOT NULL default ' ',
  "AAABLRXN" varchar(8) NOT NULL default '00000000',
  "AAABHISZ" varchar(6) NOT NULL default '000000',
  "AAABLRXO" varchar(8) NOT NULL default '00000000',
  "AAABLKLU" varchar(6) NOT NULL default '000000',
  "AAAAEQDX" varchar(7) NOT NULL default '0000000',
  "AAAAEQDY" varchar(7) NOT NULL default '0000000',
  "AAAAEQDZ" varchar(2) NOT NULL default ' ',
  "AAAAEQEA" varchar(7) NOT NULL default '0000000',
  "AAAAEQEB" varchar(7) NOT NULL default '0000000',
  "AAAAEQEC" decimal(2,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAAAEPJR","AAABLRLF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPJI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEPJR" varchar(5) NOT NULL default ' ',
  "AAABJEUU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAABMLQA","AAAAEPJR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPJS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAEPKI" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAAAEPKI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPJZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEPKI" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAABMLQA","AAAAEPKI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPKJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAEPLD" varchar(5) NOT NULL default ' ',
  "AAABLRLF" varchar(14) NOT NULL default ' ',
  "AAAAEPKO" varchar(7) NOT NULL default '0000000',
  "AAAAEPKP" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAAAEPLD","AAABLRLF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPKU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEPLD" varchar(5) NOT NULL default ' ',
  "AAABJEUU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAABMLQA","AAAAEPLD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPLE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLRLF" varchar(14) NOT NULL default ' ',
  "AAAAEQKC" varchar(5) NOT NULL default ' ',
  "AAAAEQMD" varchar(5) NOT NULL default ' ',
  "AAAAEQME" decimal(5,0) NOT NULL default '0',
  "AAAAEQMF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAABLRLF","AAAAEQKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPLQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEQKC" varchar(5) NOT NULL default ' ',
  "AAABEOSE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAABMLQA","AAAAEQKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPMA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLRLF" varchar(14) NOT NULL default ' ',
  "AAAAEQKC" varchar(5) NOT NULL default ' ',
  "AAAAEPMO" varchar(5) NOT NULL default '00000',
  "AAAAEQJZ" varchar(2) NOT NULL default ' ',
  "AAAAEQKA" varchar(5) NOT NULL default ' ',
  "AAAAEQKB" varchar(18) NOT NULL default ' ',
  "AAAAEPMJ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAABLRLF","AAAAEQKC","AAAAEPMO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPMP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAEQJZ" varchar(2) NOT NULL default ' ',
  "AAAAEQKA" varchar(5) NOT NULL default ' ',
  "AAAAEQKB" varchar(18) NOT NULL default ' ',
  "AAAAEQMD" varchar(5) NOT NULL default ' ',
  "AAAAEQME" decimal(5,0) NOT NULL default '0',
  "AAAAEQMF" varchar(3) NOT NULL default ' ',
  "AAAAEPMY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAAAEQJZ","AAAAEQKA","AAAAEQKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPNE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEQJZ" varchar(2) NOT NULL default ' ',
  "AAAAEQKA" varchar(5) NOT NULL default ' ',
  "AAAAEQKB" varchar(18) NOT NULL default ' ',
  "AAAAEPNL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAABMLQA","AAAAEQJZ","AAAAEQKA","AAAAEQKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPNS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAEQJZ" varchar(2) NOT NULL default ' ',
  "AAAAEPNW" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAAAEQJZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPOA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEQJZ" varchar(2) NOT NULL default ' ',
  "AAABHBOM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAABMLQA","AAAAEQJZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPOK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAEQKA" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAAAEQKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPOR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEQKA" varchar(5) NOT NULL default ' ',
  "AAAAVNHH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAABMLQA","AAAAEQKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPPB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLUER" varchar(4) NOT NULL default ' ',
  "AAAAEQDM" varchar(7) NOT NULL default '0000000',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAABLUER","AAAAEQDM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPPK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLUER" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEQDM" varchar(7) NOT NULL default '0000000',
  "AAAAEPPQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAABLUER","AAABMLQA","AAAAEQDM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPPW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAEQMV" varchar(5) NOT NULL default ' ',
  "AAABLRLF" varchar(14) NOT NULL default ' ',
  "AAAAEPQB" varchar(7) NOT NULL default '0000000',
  "AAAAEPQC" decimal(2,0) NOT NULL default '0',
  "AAAAEPQD" decimal(1,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAAAEQMV","AAABLRLF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPQI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEQMV" varchar(5) NOT NULL default ' ',
  "AAABJEUU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAABMLQA","AAAAEQMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPQS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABAKKR" varchar(1) NOT NULL default ' ',
  "AAAAEPQV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPQX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAEQKP" varchar(5) NOT NULL default ' ',
  "AAABLRLF" varchar(14) NOT NULL default ' ',
  "AAAAEQLA" varchar(7) NOT NULL default '0000000',
  "AAAAEQLB" decimal(15,2) NOT NULL default '0.00',
  "AAAAEQLC" varchar(5) NOT NULL default ' ',
  "AAAAEQLD" decimal(5,0) NOT NULL default '0',
  "AAAAEQLE" varchar(3) NOT NULL default ' ',
  "AAAAEQLF" decimal(10,6) NOT NULL default '0.000000',
  "AAAAEQLG" decimal(10,6) NOT NULL default '0.000000',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAAAEQKP","AAABLRLF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPRN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEQKP" varchar(5) NOT NULL default ' ',
  "AAABJEUU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAABMLQA","AAAAEQKP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPRX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABILXP" varchar(10) NOT NULL default ' ',
  "AAABLDUY" varchar(4) NOT NULL default ' ',
  "AAABLRLF" varchar(14) NOT NULL default ' ',
  "AAABKKDV" varchar(7) NOT NULL default '0000000',
  "AAAAEQJY" varchar(5) NOT NULL default ' ',
  "AAAAEQJZ" varchar(2) NOT NULL default ' ',
  "AAAAEQKA" varchar(5) NOT NULL default ' ',
  "AAAAEQKB" varchar(18) NOT NULL default ' ',
  "AAAAEQKC" varchar(5) NOT NULL default ' ',
  "AAAAEQKD" varchar(3) NOT NULL default '000',
  "AAAAMQZI" varchar(10) NOT NULL default ' ',
  "AAAAKWZN" varchar(10) NOT NULL default ' ',
  "AAABKGND" VARBINARY(255),
  "AAAAEQKH" varchar(4) NOT NULL default ' ',
  "AAAAEQKI" decimal(15,2) NOT NULL default '0.00',
  "AAAAEQKJ" varchar(5) NOT NULL default ' ',
  "AAAAEQKK" decimal(5,0) NOT NULL default '0',
  "AAAAEQKL" varchar(3) NOT NULL default ' ',
  "AAAAEQKM" decimal(17,7) NOT NULL default '0.0000000',
  "AAAAEQKN" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAABILXP","AAABLDUY","AAABLRLF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPSZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABILXP" varchar(10) NOT NULL default ' ',
  "AAABLDUY" varchar(4) NOT NULL default ' ',
  "AAABLRLF" varchar(14) NOT NULL default ' ',
  "AAAAEQMY" varchar(4) NOT NULL default '0000',
  "AAAAEQHN" varchar(5) NOT NULL default ' ',
  "AAAAEQHO" varchar(3) NOT NULL default ' ',
  "AAAAEQHP" varchar(5) NOT NULL default ' ',
  "AAAAEQHQ" varchar(3) NOT NULL default ' ',
  "AAAAEQHG" varchar(5) NOT NULL default ' ',
  "AAAAEQHH" decimal(16,8) NOT NULL default '0.00000000',
  "AAAAEQHI" decimal(16,8) NOT NULL default '0.00000000',
  "AAAAEQHJ" decimal(16,8) NOT NULL default '0.00000000',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAABILXP","AAABLDUY","AAABLRLF","AAAAEQMY","AAAAEQHN","AAAAEQHO","AAAAEQHP","AAAAEQHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPTY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABFNVZ" varchar(10) NOT NULL default ' ',
  "AAABLDUY" varchar(4) NOT NULL default ' ',
  "AAABKKDW" varchar(5) NOT NULL default '00000',
  "AAABLRLF" varchar(14) NOT NULL default ' ',
  "AAABILXP" varchar(10) NOT NULL default ' ',
  "AAAAEQIN" varchar(1) NOT NULL default ' ',
  "AAAAKVHZ" varchar(1) NOT NULL default ' ',
  "AAAAEQIP" varchar(5) NOT NULL default '00000',
  "AAAAEQIQ" decimal(10,6) NOT NULL default '0.000000',
  "AAAAEQIR" decimal(10,6) NOT NULL default '0.000000',
  "AAAAEQIS" decimal(15,3) NOT NULL default '0.000',
  "AAAAEQIT" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAABFNVZ","AAABLDUY","AAABKKDW","AAABLRLF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPUT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABILXP" varchar(10) NOT NULL default ' ',
  "AAABLDUY" varchar(4) NOT NULL default ' ',
  "AAABLRLF" varchar(14) NOT NULL default ' ',
  "AAAAEQMY" varchar(4) NOT NULL default '0000',
  "AAAAEQMV" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAABILXP","AAABLDUY","AAABLRLF","AAAAEQMY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPVH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAEQJY" varchar(5) NOT NULL default ' ',
  "AAABLRLF" varchar(14) NOT NULL default ' ',
  "AAABJZMF" varchar(5) NOT NULL default ' ',
  "AAAAEQKP" varchar(5) NOT NULL default ' ',
  "AAAAEQKQ" varchar(5) NOT NULL default ' ',
  "AAAAEQKR" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAAAEQJY","AAABLRLF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPVU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEQJY" varchar(5) NOT NULL default ' ',
  "AAABJEUU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAABMLQA","AAAAEQJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPWE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABILXP" varchar(10) NOT NULL default ' ',
  "AAABLDUY" varchar(4) NOT NULL default ' ',
  "AAAAEPWK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQDU","AAABMLQA","AAABILXP","AAABLDUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPWQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLUER" varchar(4) NOT NULL default ' ',
  "AAAAEPXY" varchar(14) NOT NULL default ' ',
  "AAAAEWEE" VARBINARY(255) NOT NULL,
  "AAAAEPYJ" VARBINARY(255) NOT NULL,
  PRIMARY KEY ("AAABLUXQ","AAABLUER","AAAAEPXY","AAAAEWEE"(255),"AAAAEPYJ"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPXB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEPYQ" VARBINARY(255) NOT NULL,
  "AAABLUER" varchar(4) NOT NULL default ' ',
  "AAAAEPXY" varchar(14) NOT NULL default ' ',
  "AAAAEWEE" VARBINARY(255),
  "AAAAEPYI" varchar(3) NOT NULL default ' ',
  "AAAAEPYB" varchar(10) NOT NULL default ' ',
  "AAAAEPYL" varchar(14) NOT NULL default ' ',
  "AAAAEPXK" decimal(15,3) NOT NULL default '0.000',
  "AAAAEPXL" decimal(15,3) NOT NULL default '0.000',
  "AAAAEPXM" decimal(3,1) NOT NULL default '0.0',
  "AAABFNVZ" varchar(10) NOT NULL default ' ',
  "AAAAEQJS" VARBINARY(255),
  "AAAAEQBP" decimal(15,2) NOT NULL default '0.00',
  "AAAAEQCM" varchar(5) NOT NULL default ' ',
  "AAAAEQCN" decimal(5,0) NOT NULL default '0',
  "AAAAEQBL" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLUXQ","AAAAEPYQ"(255)),
  UNIQUE KEY "AAAAEPXV" ("AAABLUXQ","AAABLUER","AAAAEPXY","AAAAEWEE"(255),"AAAAEPYI","AAAAEPYB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPYC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEPYQ" VARBINARY(255) NOT NULL,
  "AAAAEPYR" varchar(10) NOT NULL default ' ',
  "AAAAEQMO" VARBINARY(255),
  "AAABMKAH" decimal(15,3) NOT NULL default '0.000',
  "AAAAEPYI" varchar(3) NOT NULL default ' ',
  "AAAAEPYJ" VARBINARY(255),
  "AAAAEPYK" varchar(2) NOT NULL default ' ',
  "AAAAEPYL" varchar(14) NOT NULL default ' ',
  "AAAAEPYM" varchar(14) NOT NULL default ' ',
  "AAABLEBK" varchar(14) NOT NULL default ' ',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEPYQ"(255),"AAAAEPYR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPYS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEPZB" VARBINARY(255) NOT NULL,
  "AAABLDUY" varchar(4) NOT NULL default ' ',
  "AAABFNVZ" varchar(10) NOT NULL default ' ',
  "AAAAEQJS" VARBINARY(255),
  "AAAAEQBI" VARBINARY(255),
  "AAAAEQDE" VARBINARY(255),
  PRIMARY KEY ("AAABLUXQ","AAAAEPZB"(255),"AAABLDUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPZD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEQJS" VARBINARY(255) NOT NULL,
  "AAABKKDW" varchar(5) NOT NULL default '00000',
  "AAABILXP" varchar(10) NOT NULL default ' ',
  "AAAAEQLI" VARBINARY(255),
  "AAAAEQLO" VARBINARY(255),
  "AAAAEQMX" VARBINARY(255),
  "AAAAEQIN" varchar(1) NOT NULL default ' ',
  "AAAAKVHZ" varchar(1) NOT NULL default ' ',
  "AAAAEQIP" varchar(5) NOT NULL default '00000',
  "AAAAEQIQ" decimal(10,6) NOT NULL default '0.000000',
  "AAAAEQIR" decimal(10,6) NOT NULL default '0.000000',
  "AAAAEQIS" decimal(15,3) NOT NULL default '0.000',
  "AAAAEQIT" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEQJS"(255),"AAABKKDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEPZV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEQBI" VARBINARY(255) NOT NULL,
  "AAAAEQMY" varchar(4) NOT NULL default '0000',
  "AAAAEQHN" varchar(5) NOT NULL default ' ',
  "AAAAEQHO" varchar(3) NOT NULL default ' ',
  "AAAAEQHP" varchar(5) NOT NULL default ' ',
  "AAAAEQHQ" varchar(3) NOT NULL default ' ',
  "AAAAEQHG" varchar(5) NOT NULL default ' ',
  "AAAAEQHH" decimal(16,8) NOT NULL default '0.00000000',
  "AAAAEQHI" decimal(16,8) NOT NULL default '0.00000000',
  "AAAAEQHJ" decimal(16,8) NOT NULL default '0.00000000',
  PRIMARY KEY ("AAABLUXQ","AAAAEQBI"(255),"AAAAEQMY","AAAAEQHN","AAAAEQHO","AAAAEQHP","AAAAEQHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQAO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEQBK" VARBINARY(255) NOT NULL,
  "AAAAEQMY" varchar(4) NOT NULL default '0000',
  "AAAAEQHV" decimal(16,8) NOT NULL default '0.00000000',
  "AAAAEQHW" varchar(5) NOT NULL default ' ',
  "AAAAEQHX" varchar(5) NOT NULL default ' ',
  "AAAAEQHY" decimal(16,8) NOT NULL default '0.00000000',
  "AAAAEQHZ" varchar(3) NOT NULL default ' ',
  "AAAAEQIA" decimal(16,8) NOT NULL default '0.00000000',
  "AAAAEQIB" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEQBK"(255),"AAAAEQMY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQBC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEQMO" VARBINARY(255) NOT NULL,
  "AAABLDUY" varchar(4) NOT NULL default ' ',
  "AAABFNVZ" varchar(10) NOT NULL default ' ',
  "AAAAEQJS" VARBINARY(255),
  "AAAAEQBI" VARBINARY(255),
  "AAAAEQDE" VARBINARY(255),
  "AAAAEQBK" VARBINARY(255),
  "AAAAEQBL" varchar(2) NOT NULL default '00',
  "AAAAEQLW" varchar(2) NOT NULL default ' ',
  "AAAAEQBN" varchar(8) NOT NULL default '00000000',
  "AAAAEQBO" decimal(15,2) NOT NULL default '0.00',
  "AAAAEQBP" decimal(15,2) NOT NULL default '0.00',
  "AAAAEQCM" varchar(5) NOT NULL default ' ',
  "AAAAEQCN" decimal(5,0) NOT NULL default '0',
  "AAAAEQCO" varchar(3) NOT NULL default ' ',
  "AAAAEQBT" decimal(15,3) NOT NULL default '0.000',
  "AAAAEQCL" varchar(3) NOT NULL default ' ',
  "AAAAEQCS" decimal(15,3) NOT NULL default '0.000',
  "AAAAEQCT" decimal(15,3) NOT NULL default '0.000',
  "AAAAEQBX" VARBINARY(255),
  "AAAAEQBY" VARBINARY(255),
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAEQGC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEQMO"(255),"AAABLDUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQCE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEQJS" VARBINARY(255) NOT NULL,
  "AAAAEQCH" decimal(15,2) NOT NULL default '0.00',
  "AAAAEQCI" varchar(5) NOT NULL default ' ',
  "AAAAEQCJ" decimal(5,0) NOT NULL default '0',
  "AAAAEQCK" varchar(3) NOT NULL default ' ',
  "AAAAEQCL" varchar(3) NOT NULL default ' ',
  "AAAAEQCM" varchar(5) NOT NULL default ' ',
  "AAAAEQCN" decimal(5,0) NOT NULL default '0',
  "AAAAEQCO" varchar(3) NOT NULL default ' ',
  "AAAAEQCP" varchar(7) NOT NULL default '0000000',
  "AAAAEQCQ" varchar(7) NOT NULL default '0000000',
  "AAAAEQCR" varchar(5) NOT NULL default ' ',
  "AAAAEQCS" decimal(15,3) NOT NULL default '0.000',
  "AAAAEQCT" decimal(15,3) NOT NULL default '0.000',
  "AAAAEQCU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEQJS"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQCX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEQDE" VARBINARY(255) NOT NULL,
  "AAAAEQMY" varchar(4) NOT NULL default '0000',
  "AAAAEQIN" varchar(1) NOT NULL default ' ',
  "AAAAEQMV" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEQDE"(255),"AAAAEQMY","AAAAEQIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQDH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEQEE" VARBINARY(255) NOT NULL,
  "AAAAEQDK" varchar(7) NOT NULL default '0000000',
  "AAAAEQDL" varchar(7) NOT NULL default '0000000',
  "AAAAEQDM" varchar(7) NOT NULL default '0000000',
  "AAAAEQDN" varchar(7) NOT NULL default '0000000',
  "AAAAEQDO" decimal(3,0) NOT NULL default '0',
  "AAAAEQDP" varchar(7) NOT NULL default '0000000',
  "AAAAEQDQ" decimal(3,0) NOT NULL default '0',
  "AAAAEQDR" decimal(3,0) NOT NULL default '0',
  "AAAAEQDS" varchar(1) NOT NULL default ' ',
  "AAABLRXN" varchar(8) NOT NULL default '00000000',
  "AAABHISZ" varchar(6) NOT NULL default '000000',
  "AAABLRXO" varchar(8) NOT NULL default '00000000',
  "AAABLKLU" varchar(6) NOT NULL default '000000',
  "AAAAEQDX" varchar(7) NOT NULL default '0000000',
  "AAAAEQDY" varchar(7) NOT NULL default '0000000',
  "AAAAEQDZ" varchar(2) NOT NULL default ' ',
  "AAAAEQEA" varchar(7) NOT NULL default '0000000',
  "AAAAEQEB" varchar(7) NOT NULL default '0000000',
  "AAAAEQEC" decimal(2,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAAAEQEE"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQEF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEQER" VARBINARY(255) NOT NULL,
  "AAAAFRVK" varchar(1) NOT NULL default ' ',
  "AAAAEQEJ" varchar(2) NOT NULL default '00',
  "AAAAEQEK" varchar(14) NOT NULL default ' ',
  "AAAAEQEL" varchar(14) NOT NULL default ' ',
  "AAAAEQEM" varchar(14) NOT NULL default ' ',
  "AAAAEQEN" varchar(14) NOT NULL default ' ',
  "AAABKGKR" varchar(8) NOT NULL default '00000000',
  "AAABKIUE" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEQER"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQES" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEQJZ" varchar(2) NOT NULL default ' ',
  "AAAAEQKA" varchar(5) NOT NULL default ' ',
  "AAAAEQKB" varchar(18) NOT NULL default ' ',
  "AAAANAHH" varchar(8) NOT NULL default '00000000',
  "AAAAEQGI" varchar(6) NOT NULL default '000000',
  "AAABLRLF" varchar(14) NOT NULL default ' ',
  "AAAAEQFW" decimal(15,2) NOT NULL default '0.00',
  "AAAAEQMD" varchar(5) NOT NULL default ' ',
  "AAAAEQME" decimal(5,0) NOT NULL default '0',
  "AAAAEQMF" varchar(3) NOT NULL default ' ',
  "AAAANAHC" varchar(1) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAEQGC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEQJZ","AAAAEQKA","AAAAEQKB","AAAANAHH","AAAAEQGI","AAABLRLF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQFO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEQJZ" varchar(2) NOT NULL default ' ',
  "AAAAEQKA" varchar(5) NOT NULL default ' ',
  "AAAAEQKB" varchar(18) NOT NULL default ' ',
  "AAAANAHH" varchar(8) NOT NULL default '00000000',
  "AAAAEQGI" varchar(6) NOT NULL default '000000',
  "AAABLRLF" varchar(14) NOT NULL default ' ',
  "AAAAEQFW" decimal(15,2) NOT NULL default '0.00',
  "AAAAEQMD" varchar(5) NOT NULL default ' ',
  "AAAAEQME" decimal(5,0) NOT NULL default '0',
  "AAAAEQMF" varchar(3) NOT NULL default ' ',
  "AAAANAHC" varchar(1) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAEQGC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEQJZ","AAAAEQKA","AAAAEQKB","AAAANAHH","AAAAEQGI","AAABLRLF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQGK" (
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
CREATE TABLE "AAAAEQGY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEQLO" VARBINARY(255) NOT NULL,
  "AAAAEQMY" varchar(4) NOT NULL default '0000',
  "AAAAEQHN" varchar(5) NOT NULL default ' ',
  "AAAAEQHO" varchar(3) NOT NULL default ' ',
  "AAAAEQHP" varchar(5) NOT NULL default ' ',
  "AAAAEQHQ" varchar(3) NOT NULL default ' ',
  "AAAAEQHG" varchar(5) NOT NULL default ' ',
  "AAAAEQHH" decimal(16,8) NOT NULL default '0.00000000',
  "AAAAEQHI" decimal(16,8) NOT NULL default '0.00000000',
  "AAAAEQHJ" decimal(16,8) NOT NULL default '0.00000000',
  PRIMARY KEY ("AAABLUXQ","AAAAEQLO"(255),"AAAAEQMY","AAAAEQHN","AAAAEQHO","AAAAEQHP","AAAAEQHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQHR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEQLS" VARBINARY(255) NOT NULL,
  "AAAAEQMY" varchar(4) NOT NULL default '0000',
  "AAAAEQHV" decimal(16,8) NOT NULL default '0.00000000',
  "AAAAEQHW" varchar(5) NOT NULL default ' ',
  "AAAAEQHX" varchar(5) NOT NULL default ' ',
  "AAAAEQHY" decimal(16,8) NOT NULL default '0.00000000',
  "AAAAEQHZ" varchar(3) NOT NULL default ' ',
  "AAAAEQIA" decimal(16,8) NOT NULL default '0.00000000',
  "AAAAEQIB" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEQLS"(255),"AAAAEQMY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQIF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEQJS" VARBINARY(255) NOT NULL,
  "AAABKKDW" varchar(5) NOT NULL default '00000',
  "AAABILXP" varchar(10) NOT NULL default ' ',
  "AAAAEQLI" VARBINARY(255),
  "AAAAEQLO" VARBINARY(255),
  "AAAAEQMX" VARBINARY(255),
  "AAAAEQIN" varchar(1) NOT NULL default ' ',
  "AAAAKVHZ" varchar(1) NOT NULL default ' ',
  "AAAAEQIP" varchar(5) NOT NULL default '00000',
  "AAAAEQIQ" decimal(10,6) NOT NULL default '0.000000',
  "AAAAEQIR" decimal(10,6) NOT NULL default '0.000000',
  "AAAAEQIS" decimal(15,3) NOT NULL default '0.000',
  "AAAAEQIT" varchar(3) NOT NULL default ' ',
  "AAAAEQLQ" decimal(15,3) NOT NULL default '0.000',
  "AAAAEQLR" varchar(3) NOT NULL default ' ',
  "AAAAEQLS" VARBINARY(255),
  "AAAAEQLT" VARBINARY(255),
  "AAAAEQLU" VARBINARY(255),
  "AAAAEQLV" varchar(2) NOT NULL default '00',
  "AAAAEQLW" varchar(2) NOT NULL default ' ',
  "AAAAEQLX" decimal(15,2) NOT NULL default '0.00',
  "AAAAEQLY" decimal(15,2) NOT NULL default '0.00',
  "AAAAEQLZ" varchar(5) NOT NULL default ' ',
  "AAAAEQMA" decimal(5,0) NOT NULL default '0',
  "AAAAEQMB" varchar(3) NOT NULL default ' ',
  "AAAAEQMC" decimal(15,2) NOT NULL default '0.00',
  "AAAAEQMD" varchar(5) NOT NULL default ' ',
  "AAAAEQME" decimal(5,0) NOT NULL default '0',
  "AAAAEQMF" varchar(3) NOT NULL default ' ',
  "AAAAEQMG" varchar(1) NOT NULL default '0',
  "AAAAEQMH" varchar(1) NOT NULL default ' ',
  "AAAAEQMI" decimal(15,2) NOT NULL default '0.00',
  "AAAAEQMJ" varchar(5) NOT NULL default ' ',
  "AAAAEQMK" decimal(5,0) NOT NULL default '0',
  "AAAAEQML" varchar(3) NOT NULL default ' ',
  "AAAAEQMM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEQJS"(255),"AAABKKDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQJU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEQLI" VARBINARY(255) NOT NULL,
  "AAABKKDV" varchar(7) NOT NULL default '0000000',
  "AAAAEQJY" varchar(5) NOT NULL default ' ',
  "AAAAEQJZ" varchar(2) NOT NULL default ' ',
  "AAAAEQKA" varchar(5) NOT NULL default ' ',
  "AAAAEQKB" varchar(18) NOT NULL default ' ',
  "AAAAEQKC" varchar(5) NOT NULL default ' ',
  "AAAAEQKD" varchar(3) NOT NULL default '000',
  "AAAAMQZI" varchar(10) NOT NULL default ' ',
  "AAAAKWZN" varchar(10) NOT NULL default ' ',
  "AAABKGND" VARBINARY(255),
  "AAAAEQKH" varchar(4) NOT NULL default ' ',
  "AAAAEQKI" decimal(15,2) NOT NULL default '0.00',
  "AAAAEQKJ" varchar(5) NOT NULL default ' ',
  "AAAAEQKK" decimal(5,0) NOT NULL default '0',
  "AAAAEQKL" varchar(3) NOT NULL default ' ',
  "AAAAEQKM" decimal(17,7) NOT NULL default '0.0000000',
  "AAAAEQKN" varchar(3) NOT NULL default ' ',
  "AAABJZMF" varchar(5) NOT NULL default ' ',
  "AAAAEQKP" varchar(5) NOT NULL default ' ',
  "AAAAEQKQ" varchar(5) NOT NULL default ' ',
  "AAAAEQKR" varchar(5) NOT NULL default ' ',
  "AAAAEQKS" VARBINARY(255),
  "AAAAEQKT" VARBINARY(255),
  "AAAAEQKU" varchar(7) NOT NULL default '0000000',
  "AAAAEQKV" decimal(15,2) NOT NULL default '0.00',
  "AAAAEQKW" decimal(15,2) NOT NULL default '0.00',
  "AAAAEQKX" varchar(5) NOT NULL default ' ',
  "AAAAEQKY" decimal(5,0) NOT NULL default '0',
  "AAAAEQKZ" varchar(3) NOT NULL default ' ',
  "AAAAEQLA" varchar(7) NOT NULL default '0000000',
  "AAAAEQLB" decimal(15,2) NOT NULL default '0.00',
  "AAAAEQLC" varchar(5) NOT NULL default ' ',
  "AAAAEQLD" decimal(5,0) NOT NULL default '0',
  "AAAAEQLE" varchar(3) NOT NULL default ' ',
  "AAAAEQLF" decimal(10,6) NOT NULL default '0.000000',
  "AAAAEQLG" decimal(10,6) NOT NULL default '0.000000',
  PRIMARY KEY ("AAABLUXQ","AAAAEQLI"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQLJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEQMO" VARBINARY(255) NOT NULL,
  "AAABLDUY" varchar(4) NOT NULL default ' ',
  "AAABKKDW" varchar(5) NOT NULL default '00000',
  "AAAAEQLO" VARBINARY(255),
  "AAAAEQMX" VARBINARY(255),
  "AAAAEQLQ" decimal(15,3) NOT NULL default '0.000',
  "AAAAEQLR" varchar(3) NOT NULL default ' ',
  "AAAAEQLS" VARBINARY(255),
  "AAAAEQLT" VARBINARY(255),
  "AAAAEQLU" VARBINARY(255),
  "AAAAEQLV" varchar(2) NOT NULL default '00',
  "AAAAEQLW" varchar(2) NOT NULL default ' ',
  "AAAAEQLX" decimal(15,2) NOT NULL default '0.00',
  "AAAAEQLY" decimal(15,2) NOT NULL default '0.00',
  "AAAAEQLZ" varchar(5) NOT NULL default ' ',
  "AAAAEQMA" decimal(5,0) NOT NULL default '0',
  "AAAAEQMB" varchar(3) NOT NULL default ' ',
  "AAAAEQMC" decimal(15,2) NOT NULL default '0.00',
  "AAAAEQMD" varchar(5) NOT NULL default ' ',
  "AAAAEQME" decimal(5,0) NOT NULL default '0',
  "AAAAEQMF" varchar(3) NOT NULL default ' ',
  "AAAAEQMG" varchar(1) NOT NULL default '0',
  "AAAAEQMH" varchar(1) NOT NULL default ' ',
  "AAAAEQMI" decimal(15,2) NOT NULL default '0.00',
  "AAAAEQMJ" varchar(5) NOT NULL default ' ',
  "AAAAEQMK" decimal(5,0) NOT NULL default '0',
  "AAAAEQML" varchar(3) NOT NULL default ' ',
  "AAAAEQMM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEQMO"(255),"AAABLDUY","AAABKKDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQMR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEQMX" VARBINARY(255) NOT NULL,
  "AAAAEQMY" varchar(4) NOT NULL default '0000',
  "AAAAEQMV" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEQMX"(255),"AAAAEQMY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQMZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAPROR" varchar(8) NOT NULL default ' ',
  "AAAAEQNC" varchar(1) NOT NULL default ' ',
  "AAAAEQND" varchar(1) NOT NULL default ' ',
  "AAAAEQNE" varchar(8) NOT NULL default ' ',
  "AAAAERHT" varchar(1) NOT NULL default ' ',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAPROR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQNJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAPROR" varchar(8) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAEQNN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAPROR","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQNR" (
  "AAABKWFM" varchar(8) NOT NULL default '00000000',
  "AAABLTLM" varchar(6) NOT NULL default '000000',
  "AAABFLQL" varchar(40) NOT NULL default ' ',
  "AAABKYTL" int(11) NOT NULL default '0',
  "AAAAEQNW" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABKWFM","AAABLTLM","AAABFLQL","AAABKYTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQOB" (
  "AAABJCYN" varchar(8) NOT NULL default ' ',
  "AAABJGDO" varchar(1) NOT NULL default ' ',
  "AAAAIYRV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJCYN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQOG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCYN" varchar(8) NOT NULL default ' ',
  "AAAAEQOJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJCYN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQOM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCYN" varchar(8) NOT NULL default ' ',
  "AAAATQRH" varchar(2) NOT NULL default '00',
  "AAAAEQOQ" varchar(3) NOT NULL default ' ',
  "AAAAQINK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJCYN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCYN" varchar(8) NOT NULL default ' ',
  "AAABJDAS" varchar(4) NOT NULL default ' ',
  "AAAAERHJ" varchar(1) NOT NULL default ' ',
  "AAAAEQOZ" varchar(1) NOT NULL default ' ',
  "AAAAEQPA" varchar(1) NOT NULL default ' ',
  "AAAAERHM" varchar(1) NOT NULL default ' ',
  "AAAAEQPC" varchar(1) NOT NULL default ' ',
  "AAAAERHN" varchar(4) NOT NULL default ' ',
  "AAAAEQPE" varchar(1) NOT NULL default ' ',
  "AAAAIYRV" varchar(1) NOT NULL default ' ',
  "AAAAEQPG" varchar(2) NOT NULL default '00',
  "AAABLOZE" varchar(18) NOT NULL default ' ',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABLXUH" varchar(10) NOT NULL default '0000000000',
  "AAAAEQPK" varchar(8) NOT NULL default ' ',
  "AAAAEQZT" varchar(1) NOT NULL default ' ',
  "AAAAERHT" varchar(1) NOT NULL default ' ',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  "AAABLTZM" varchar(8) NOT NULL default '00000000',
  "AAABLTZN" varchar(12) NOT NULL default ' ',
  "AAABLTYV" varchar(8) NOT NULL default '00000000',
  "AAABLTYW" varchar(12) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJCYN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQPV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCYN" varchar(8) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAPXCA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJCYN","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQQD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCYN" varchar(8) NOT NULL default ' ',
  "AAAAQRZF" varchar(8) NOT NULL default ' ',
  "AAAAQRYF" varchar(1) NOT NULL default ' ',
  "AAAAQRYG" varchar(1) NOT NULL default ' ',
  "AAAAQRYI" varchar(1) NOT NULL default ' ',
  "AAAAQRYJ" varchar(1) NOT NULL default ' ',
  "AAAAQRYK" varchar(8) NOT NULL default ' ',
  "AAAAEQQM" varchar(1) NOT NULL default ' ',
  "AAAAEQQN" varchar(1) NOT NULL default ' ',
  "AAAAEQQO" varchar(1) NOT NULL default ' ',
  "AAAAEQQP" varchar(1) NOT NULL default ' ',
  "AAAAQRYL" varchar(1) NOT NULL default ' ',
  "AAAAQRYM" varchar(30) NOT NULL default ' ',
  "AAAAQRYN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJCYN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQQV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAEQQX" varchar(1) NOT NULL default ' ',
  "AAAAEQQY" varchar(1) NOT NULL default ' ',
  "AAAAEQQZ" varchar(1) NOT NULL default ' ',
  "AAAAEQRA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQRC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAEQRE" varchar(1) NOT NULL default ' ',
  "AAAAEQRF" varchar(1) NOT NULL default ' ',
  "AAAAEQRG" varchar(1) NOT NULL default ' ',
  "AAAAEQRH" tinyint(3) unsigned NOT NULL default '0',
  "AAAAEQRI" tinyint(3) unsigned NOT NULL default '0',
  "AAABDEAL" varchar(25) NOT NULL default ' ',
  "AAAAEQRK" varchar(1) NOT NULL default ' ',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  "AAAAEQRM" varchar(1) NOT NULL default ' ',
  "AAAAEQRN" varchar(1) NOT NULL default ' ',
  "AAAAEQRO" int(11) NOT NULL default '0',
  "AAABEDND" varchar(20) NOT NULL default ' ',
  "AAABEDMU" varchar(20) NOT NULL default ' ',
  "AAABEDNB" varchar(20) NOT NULL default ' ',
  "AAABEDNC" varchar(20) NOT NULL default ' ',
  "AAAAEQRT" varchar(1) NOT NULL default ' ',
  "AAAAEQRU" varchar(1) NOT NULL default ' ',
  "AAAAEQRV" varchar(1) NOT NULL default ' ',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAAANALH" varchar(1) NOT NULL default ' ',
  "AAAAEQRY" varchar(30) NOT NULL default ' ',
  "AAABLTZM" varchar(8) NOT NULL default '00000000',
  "AAABLTZN" varchar(12) NOT NULL default ' ',
  "AAABLTYV" varchar(8) NOT NULL default '00000000',
  "AAABLTYW" varchar(12) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQSF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAEQTJ" varchar(8) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAEQTJ","AAABMLDW","AAABMLDX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQSP" (
  "AAAAEQTJ" varchar(8) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default ' ',
  "AAAAEQST" varchar(1) NOT NULL default ' ',
  "AAAAEQSU" varchar(1) NOT NULL default ' ',
  "AAAAEQSV" varchar(1) NOT NULL default ' ',
  "AAAAEQSW" varchar(1) NOT NULL default ' ',
  "AAAAEQSX" varchar(1) NOT NULL default ' ',
  "AAAAEQSY" varchar(1) NOT NULL default ' ',
  "AAAAEQSZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEQTJ","AAABMLDW","AAABMLDX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQTD" (
  "AAAAEQTJ" varchar(8) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAEQTI" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEQTJ","AAABMLDW","AAABMLDX","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQTN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAEQUP" varchar(8) NOT NULL default ' ',
  "AAABIGKK" varchar(8) NOT NULL default '00000000',
  "AAAAQIXJ" varchar(8) NOT NULL default ' ',
  "AAABJDAS" varchar(4) NOT NULL default ' ',
  "AAABLHPM" varchar(1) NOT NULL default ' ',
  "AAABMMVH" varchar(30) NOT NULL default ' ',
  "AAAATKJS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAEQUP","AAABIGKK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQTZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAEQUP" varchar(8) NOT NULL default ' ',
  "AAABIGKK" varchar(8) NOT NULL default '00000000',
  "AAABIMIT" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAEQUP","AAABIGKK","AAABIMIT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQUI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAEQUP" varchar(8) NOT NULL default ' ',
  "AAABIGKK" varchar(8) NOT NULL default '00000000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAEQUN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAEQUP","AAABIGKK","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQUS" (
  "AAABLHPM" varchar(1) NOT NULL default ' ',
  "AAABJDAS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHPM","AAABJDAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQUX" (
  "AAABLHPM" varchar(1) NOT NULL default ' ',
  "AAABJDAS" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAPXCA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHPM","AAABJDAS","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQVF" (
  "AAABJDAS" varchar(4) NOT NULL default ' ',
  "AAABLHPM" varchar(1) NOT NULL default ' ',
  "AAAAEQVI" varchar(40) NOT NULL default ' ',
  "AAAAEQVJ" varchar(4) NOT NULL default ' ',
  "AAAAEQVK" varchar(40) NOT NULL default ' ',
  "AAAAEQVL" varchar(4) NOT NULL default ' ',
  "AAAAEQVM" varchar(40) NOT NULL default ' ',
  "AAAAEQVN" varchar(4) NOT NULL default ' ',
  "AAAAEQVO" varchar(40) NOT NULL default ' ',
  "AAAAEQVP" varchar(4) NOT NULL default ' ',
  "AAAAEQVQ" varchar(40) NOT NULL default ' ',
  "AAAAEQVR" varchar(4) NOT NULL default ' ',
  "AAAAEQVS" varchar(40) NOT NULL default ' ',
  "AAAAEQVT" varchar(4) NOT NULL default ' ',
  "AAAAEQVU" varchar(1) NOT NULL default ' ',
  "AAAAEQWB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABJDAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQVX" (
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAAAEQWB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKGJ","AAAAEQWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQWC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAPIDQ" varchar(1) NOT NULL default ' ',
  "AAAAEQWF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAPIDQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQWI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAPIDQ" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAPIDQ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQWQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
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
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQXI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAEQXK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQXM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAPXIX" varchar(3) NOT NULL default ' ',
  "AAAAEQXP" varchar(1) NOT NULL default ' ',
  "AAAAEQXQ" varchar(1) NOT NULL default ' ',
  "AAAAEQXR" varchar(1) NOT NULL default ' ',
  "AAAAEQXS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAPXIX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQXV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAQINK" varchar(3) NOT NULL default ' ',
  "AAABKSQC" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQYC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREO" varchar(2) NOT NULL default ' ',
  "AAABMMRE" varchar(65) NOT NULL default ' ',
  "AAAAWTWC" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQYJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDNK" varchar(8) NOT NULL default ' ',
  "AAAAEQYM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDNK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQYP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDAS" varchar(4) NOT NULL default ' ',
  "AAABLHPM" varchar(1) NOT NULL default ' ',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDAS","AAABLHPM","AAABIMRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEQYY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDNK" varchar(8) NOT NULL default ' ',
  "AAAAERAB" varchar(8) NOT NULL default '00000000',
  "AAABBLCI" varchar(8) NOT NULL default ' ',
  "AAABBLCK" varchar(8) NOT NULL default ' ',
  "AAABJDMQ" varchar(1) NOT NULL default ' ',
  "AAABJDMT" varchar(1) NOT NULL default ' ',
  "AAAAEQZG" varchar(1) NOT NULL default ' ',
  "AAAAEQZH" varchar(1) NOT NULL default ' ',
  "AAAAEQZI" varchar(1) NOT NULL default ' ',
  "AAAAEQZJ" varchar(1) NOT NULL default ' ',
  "AAAAESPU" varchar(1) NOT NULL default ' ',
  "AAABJDMX" varchar(4) NOT NULL default ' ',
  "AAAAEQZM" varchar(1) NOT NULL default ' ',
  "AAAAEQZN" varchar(1) NOT NULL default ' ',
  "AAAAEQZO" varchar(1) NOT NULL default ' ',
  "AAABKNGX" varchar(1) NOT NULL default ' ',
  "AAAAERHT" varchar(1) NOT NULL default ' ',
  "AAAAEQZR" varchar(1) NOT NULL default ' ',
  "AAAAERHN" varchar(4) NOT NULL default ' ',
  "AAAAEQZT" varchar(1) NOT NULL default ' ',
  "AAABLTZM" varchar(8) NOT NULL default '00000000',
  "AAABLTZN" varchar(12) NOT NULL default ' ',
  "AAABLTYV" varchar(8) NOT NULL default '00000000',
  "AAABLTYW" varchar(12) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDNK","AAAAERAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERAC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDNK" varchar(8) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAPXCA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDNK","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERAK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATGNC" varchar(8) NOT NULL default ' ',
  "AAABLHPM" varchar(1) NOT NULL default ' ',
  "AAAAQIXJ" varchar(8) NOT NULL default ' ',
  "AAABJDAS" varchar(4) NOT NULL default ' ',
  "AAABGGZA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAATGNC","AAABLHPM","AAAAQIXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERAV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATGNC" varchar(8) NOT NULL default ' ',
  "AAAAPXHC" varchar(3) NOT NULL default ' ',
  "AAAAERBS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAATGNC","AAAAPXHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERBD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAATGNC" varchar(8) NOT NULL default ' ',
  "AAAAPXHC" varchar(3) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  "AAAAPPFE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAATGNC","AAAAPXHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERBO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKHX" varchar(8) NOT NULL default ' ',
  "AAABLKHY" varchar(8) NOT NULL default ' ',
  "AAAAERBS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLKHX","AAABLKHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERBW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABKCWA" varchar(1) NOT NULL default ' ',
  "AAABLEPD" decimal(13,3) NOT NULL default '0.000',
  "AAABLEPE" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERCL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDAS" varchar(4) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAAERCP" varchar(1) NOT NULL default ' ',
  "AAAAERCQ" varchar(1) NOT NULL default ' ',
  "AAAAERCR" varchar(10) NOT NULL default ' ',
  "AAAAERCS" varchar(10) NOT NULL default ' ',
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABLRFA" varchar(6) NOT NULL default ' ',
  "AAABLYCI" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERCY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATGNC" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAATGNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERDD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATGNC" varchar(8) NOT NULL default ' ',
  "AAABMQXN" varchar(1) NOT NULL default ' ',
  "AAAAERDH" varchar(1) NOT NULL default ' ',
  "AAABDPJQ" varchar(1) NOT NULL default ' ',
  "AAABDPJR" varchar(1) NOT NULL default ' ',
  "AAABDPJS" varchar(1) NOT NULL default ' ',
  "AAABDPJT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAATGNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERDO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAATGNC" varchar(8) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  "AAAAPPFE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAATGNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERDX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATGNC" varchar(8) NOT NULL default ' ',
  "AAABMMJT" varchar(3) NOT NULL default ' ',
  "AAAAGCTF" varchar(1) NOT NULL default ' ',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAATGNC","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEREG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAATGNC" varchar(8) NOT NULL default ' ',
  "AAABMMJT" varchar(3) NOT NULL default ' ',
  "AAABMRCK" varchar(20) NOT NULL default ' ',
  "AAAAPPFE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAATGNC","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERER" (
  "AAABIIVE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIIVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEREU" (
  "AAABIIVE" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABIIVE","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERFA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAERGT" varchar(8) NOT NULL default ' ',
  "AAAAERGU" varchar(8) NOT NULL default ' ',
  "AAABJDMQ" varchar(1) NOT NULL default ' ',
  "AAABJDMT" varchar(1) NOT NULL default ' ',
  "AAAAERFY" varchar(1) NOT NULL default ' ',
  "AAAAERFZ" varchar(1) NOT NULL default ' ',
  "AAABLTZM" varchar(8) NOT NULL default '00000000',
  "AAABLTZN" varchar(12) NOT NULL default ' ',
  "AAABLTYV" varchar(8) NOT NULL default '00000000',
  "AAABLTYW" varchar(12) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAERGT","AAAAERGU","AAABJDMQ","AAABJDMT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERFS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAERGT" varchar(8) NOT NULL default ' ',
  "AAAAERGU" varchar(8) NOT NULL default ' ',
  "AAABJDMQ" varchar(1) NOT NULL default ' ',
  "AAABJDMT" varchar(1) NOT NULL default ' ',
  "AAAAERFY" varchar(1) NOT NULL default ' ',
  "AAAAERFZ" varchar(1) NOT NULL default ' ',
  "AAABLTZM" varchar(8) NOT NULL default '00000000',
  "AAABLTZN" varchar(12) NOT NULL default ' ',
  "AAABLTYV" varchar(8) NOT NULL default '00000000',
  "AAABLTYW" varchar(12) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAERGT","AAAAERGU","AAABJDMQ","AAABJDMT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAERGT" varchar(8) NOT NULL default ' ',
  "AAAAERGU" varchar(8) NOT NULL default ' ',
  "AAABJDMQ" varchar(1) NOT NULL default ' ',
  "AAABJDMT" varchar(1) NOT NULL default ' ',
  "AAAAERGQ" varchar(40) NOT NULL default ' ',
  "AAAAERGR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAERGT","AAAAERGU","AAABJDMQ","AAABJDMT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERGX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAERHE" varchar(20) NOT NULL default ' ',
  "AAABLHMS" tinyint(3) unsigned NOT NULL default '0',
  "AAABFMOQ" varchar(15) NOT NULL default ' ',
  "AAAAERHC" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAERHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERHF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAPWRP" varchar(8) NOT NULL default ' ',
  "AAABJDAS" varchar(4) NOT NULL default ' ',
  "AAAAERHJ" varchar(1) NOT NULL default ' ',
  "AAAAERHK" varchar(1) NOT NULL default ' ',
  "AAAAERHL" varchar(1) NOT NULL default ' ',
  "AAAAERHM" varchar(1) NOT NULL default ' ',
  "AAAAERHN" varchar(4) NOT NULL default ' ',
  "AAAAERHO" varchar(1) NOT NULL default ' ',
  "AAABLOZE" varchar(18) NOT NULL default ' ',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABLXUH" varchar(10) NOT NULL default '0000000000',
  "AAAAERHS" varchar(8) NOT NULL default ' ',
  "AAAAERHT" varchar(1) NOT NULL default ' ',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  "AAABLTZM" varchar(8) NOT NULL default '00000000',
  "AAABLTZN" varchar(12) NOT NULL default ' ',
  "AAABLTYV" varchar(8) NOT NULL default '00000000',
  "AAABLTYW" varchar(12) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAPWRP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERIC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAPWRP" varchar(8) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAPXCA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAPWRP","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERIK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAERJH" varchar(22) NOT NULL default ' ',
  "AAAAERJI" varchar(7) NOT NULL default '0000000',
  "AAABMLEL" varchar(8) NOT NULL default '00000000',
  "AAABKQPD" varchar(8) NOT NULL default '00000000',
  "AAAAQJIJ" varchar(4) NOT NULL default ' ',
  "AAABKRRC" varchar(6) NOT NULL default ' ',
  "AAAAERIT" decimal(13,3) NOT NULL default '0.000',
  "AAAAERIU" decimal(13,3) NOT NULL default '0.000',
  "AAAAERIV" decimal(13,3) NOT NULL default '0.000',
  "AAAAERIW" decimal(13,3) NOT NULL default '0.000',
  "AAAAERIX" decimal(13,3) NOT NULL default '0.000',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAABKODR" decimal(5,3) NOT NULL default '0.000',
  "AAABLLXX" decimal(5,0) NOT NULL default '0',
  "AAABLLXY" decimal(5,0) NOT NULL default '0',
  "AAABKNRS" varchar(3) NOT NULL default ' ',
  "AAAAQJIS" varchar(1) NOT NULL default ' ',
  "AAAAERJE" decimal(13,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAAAERJH","AAAAERJI","AAABMLEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERJK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQPP" varchar(4) NOT NULL default ' ',
  "AAABJLRA" varchar(25) NOT NULL default ' ',
  "AAAAERKQ" varchar(8) NOT NULL default '00000000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAEXWR" varchar(10) NOT NULL default ' ',
  "AAAAKZTQ" varchar(2) NOT NULL default ' ',
  "AAAAEXWT" varchar(2) NOT NULL default ' ',
  "AAAAKZTR" varchar(15) NOT NULL default ' ',
  "AAAAEXWV" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCNV" varchar(5) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  "AAABMCZN" varchar(10) NOT NULL default ' ',
  "AAABMCZO" varchar(3) NOT NULL default '000',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABLXSX" varchar(8) NOT NULL default '00000000',
  "AAABMCXJ" varchar(12) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQPP","AAABJLRA","AAAAERKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERKR" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABEWKE" varchar(5) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMIZS" varchar(30) NOT NULL default ' ',
  "AAAAJWHF" varchar(1) NOT NULL default ' ',
  "AAABMRCU" varchar(10) NOT NULL default '0000000000',
  "AAAAJWGS" VARBINARY(255),
  "AAAAJWGT" varchar(132) NOT NULL default ' ',
  "AAAAJWGU" varchar(132) NOT NULL default ' ',
  "AAAAJWGV" varchar(2) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAAAJWGX" varchar(3) NOT NULL default '000',
  "AAAAJWGY" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABEWKE","AAABMREL","AAABMIZS","AAAAJWHF","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERLL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNLE" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABKQOK" varchar(4) NOT NULL default '0000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAERLS" varchar(8) NOT NULL default '00000000',
  "AAAAERLT" varchar(12) NOT NULL default ' ',
  "AAABKQFL" varchar(8) NOT NULL default '00000000',
  "AAAAERLV" varchar(6) NOT NULL default ' ',
  "AAAAERLW" varchar(6) NOT NULL default ' ',
  "AAAAERLX" varchar(6) NOT NULL default ' ',
  "AAAAERLY" varchar(6) NOT NULL default ' ',
  "AAABBELL" varchar(3) NOT NULL default '000',
  "AAAAERMA" varchar(8) NOT NULL default ' ',
  "AAAAERMB" varchar(8) NOT NULL default ' ',
  "AAAAERMC" varchar(8) NOT NULL default ' ',
  "AAABMDGQ" varchar(8) NOT NULL default ' ',
  "AAAAERME" varchar(40) NOT NULL default ' ',
  "AAAAESEZ" varchar(1) NOT NULL default ' ',
  "AAAAERMG" varchar(6) NOT NULL default ' ',
  "AAAAERMH" varchar(6) NOT NULL default ' ',
  "AAAAERMI" varchar(6) NOT NULL default ' ',
  "AAAAERMJ" varchar(6) NOT NULL default ' ',
  "AAAAERMK" varchar(1) NOT NULL default '0',
  "AAAAERML" varchar(1) NOT NULL default '0',
  "AAAAERMM" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKNLE","AAABLUNJ","AAABKQOK"),
  KEY "AAAAERMR" ("AAABMRIL","AAABLUNJ"),
  KEY "AAAAERMU" ("AAABKQFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERMW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNLE" varchar(2) NOT NULL default ' ',
  "AAABKQFL" varchar(8) NOT NULL default '00000000',
  "AAAAERNM" int(11) NOT NULL default '0',
  "AAAAERNN" int(11) NOT NULL default '0',
  "AAABGXHS" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABIJJL" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABKNLE","AAABKQFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERNI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNLE" varchar(2) NOT NULL default ' ',
  "AAABKQFL" varchar(8) NOT NULL default '00000000',
  "AAAAERNM" int(11) NOT NULL default '0',
  "AAAAERNN" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKNLE","AAABKQFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERNR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNLE" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAAAEROP" varchar(6) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAWQWH" varchar(4) NOT NULL default '0000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAERNZ" varchar(8) NOT NULL default '00000000',
  "AAAAEROA" varchar(12) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABKPUA" varchar(6) NOT NULL default ' ',
  "AAAAEROE" varchar(1) NOT NULL default ' ',
  "AAABEIIS" varchar(6) NOT NULL default ' ',
  "AAAAEROG" varchar(12) NOT NULL default ' ',
  "AAAAEROH" varchar(1) NOT NULL default ' ',
  "AAABDTDD" varchar(4) NOT NULL default ' ',
  "AAABLYNC" varchar(3) NOT NULL default ' ',
  "AAAAEROK" varchar(1) NOT NULL default ' ',
  "AAAAEROL" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKNLE","AAABLUNJ","AAAAEROP","AAABKFCZ","AAAAWQWH"),
  KEY "AAAAEROS" ("AAABMCRJ","AAABMCRK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEROV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGKZJ" varchar(8) NOT NULL default ' ',
  "AAAAQIXJ" varchar(8) NOT NULL default ' ',
  "AAABLHPM" varchar(1) NOT NULL default ' ',
  "AAABLROG" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGKZJ","AAAAQIXJ","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAREWW" (
  "AAAAERTR" varchar(30) NOT NULL default ' ',
  "AAAAERPH" varchar(2) NOT NULL default ' ',
  "AAAAERPI" varchar(2) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAAAERTR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERPO" (
  "AAAAERTR" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLP" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAAAERTR","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERPU" (
  "AAAAERTR" varchar(30) NOT NULL default ' ',
  "AAABGFJD" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAERTR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERPY" (
  "AAAAERSS" varchar(60) NOT NULL default ' ',
  "AAABIJSB" varchar(4) NOT NULL default '0000',
  "AAABJSDF" varchar(10) NOT NULL default '0000000000',
  "AAABLSBY" VARBINARY(255),
  PRIMARY KEY ("AAAAERSS","AAABIJSB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERQF" (
  "AAAAERTR" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default ' ',
  "AAAAERTG" varchar(8) NOT NULL default ' ',
  "AAABGETF" varchar(70) NOT NULL default ' ',
  "AAABGETG" varchar(70) NOT NULL default ' ',
  "AAAAERTJ" varchar(4) NOT NULL default ' ',
  "AAAAYWXT" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAAERTR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERQO" (
  "AAAAERSS" varchar(60) NOT NULL default ' ',
  "AAAAERQQ" varchar(60) NOT NULL default ' ',
  "AAABENKB" varchar(255) NOT NULL default ' ',
  "AAABGQLR" varchar(255) NOT NULL default ' ',
  "AAABHXBL" varchar(255) NOT NULL default ' ',
  "AAAAERQU" varchar(255) NOT NULL default ' ',
  "AAAAERQV" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAAERSS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERQX" (
  "AAAAERTR" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default ' ',
  "AAAAERTG" varchar(8) NOT NULL default ' ',
  "AAAAERRB" varchar(100) NOT NULL default ' ',
  "AAAAERRC" varchar(5) NOT NULL default ' ',
  "AAAAERRD" varchar(255) NOT NULL default ' ',
  "AAABDXKV" varchar(12) NOT NULL default ' ',
  "AAAATTKT" varchar(20) NOT NULL default ' ',
  "AAAAERRG" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAERTR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERRI" (
  "AAAAERTR" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default ' ',
  "AAAAERTG" varchar(8) NOT NULL default ' ',
  "AAABGETF" varchar(70) NOT NULL default ' ',
  "AAABGETG" varchar(70) NOT NULL default ' ',
  "AAAAERTJ" varchar(4) NOT NULL default ' ',
  "AAABGETW" varchar(64) NOT NULL default ' ',
  "AAAATTMI" varchar(5) NOT NULL default '00000',
  "AAAAERRR" varchar(5) NOT NULL default '00000',
  "AAAAERRS" varchar(70) NOT NULL default ' ',
  "AAAAERSJ" varchar(1) NOT NULL default ' ',
  "AAAAERSK" varchar(1) NOT NULL default ' ',
  "AAAAERSL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAERTR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERRX" (
  "AAAAERTR" varchar(30) NOT NULL default ' ',
  "AAAAERRZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAERTR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERSB" (
  "AAAAERTR" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default ' ',
  "AAAAERTG" varchar(8) NOT NULL default ' ',
  "AAAAERSF" varchar(30) NOT NULL default ' ',
  "AAABGETF" varchar(70) NOT NULL default ' ',
  "AAABGETG" varchar(70) NOT NULL default ' ',
  "AAAAERTJ" varchar(4) NOT NULL default ' ',
  "AAAAERSJ" varchar(1) NOT NULL default ' ',
  "AAAAERSK" varchar(1) NOT NULL default ' ',
  "AAAAERSL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAERTR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERSN" (
  "AAAAERTR" varchar(30) NOT NULL default ' ',
  "AAAAERSS" varchar(60) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAERTR","AAAAERSS","AAABLHGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERSU" (
  "AAAAERTR" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default ' ',
  "AAAAERTG" varchar(8) NOT NULL default ' ',
  "AAABGETF" varchar(70) NOT NULL default ' ',
  "AAABGETG" varchar(70) NOT NULL default ' ',
  "AAAAERTJ" varchar(4) NOT NULL default ' ',
  "AAABGFCP" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAERTR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERTD" (
  "AAAAERTR" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default ' ',
  "AAAAERTG" varchar(8) NOT NULL default ' ',
  "AAABGETF" varchar(70) NOT NULL default ' ',
  "AAABGETG" varchar(70) NOT NULL default ' ',
  "AAAAERTJ" varchar(4) NOT NULL default ' ',
  "AAABGETL" varchar(32) NOT NULL default ' ',
  "AAABGETM" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAERTR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERTN" (
  "AAAAERTR" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default ' ',
  "AAABHGII" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAAAERTR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERTS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNLE" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABMLEL" varchar(8) NOT NULL default '00000000',
  "AAABDERS" varchar(1) NOT NULL default ' ',
  "AAAAMNYA" varchar(2) NOT NULL default ' ',
  "AAAAMNYB" varchar(8) NOT NULL default '00000000',
  "AAABLRSP" varchar(1) NOT NULL default ' ',
  "AAABLRYP" varchar(8) NOT NULL default '00000000',
  "AAABMAWF" varchar(8) NOT NULL default '00000000',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABKMHA" varchar(12) NOT NULL default ' ',
  "AAABLVJK" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABFUGD" varchar(4) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABKNLP" varchar(1) NOT NULL default ' ',
  "AAAAERUL" varchar(4) NOT NULL default ' ',
  "AAABKRIO" varchar(10) NOT NULL default ' ',
  "AAAAERUN" varchar(3) NOT NULL default ' ',
  "AAABKNLO" varchar(4) NOT NULL default ' ',
  "AAAAMNYE" varchar(1) NOT NULL default ' ',
  "AAAAMNYF" varchar(4) NOT NULL default ' ',
  "AAAAMNYG" varchar(4) NOT NULL default ' ',
  "AAABCUGF" varchar(3) NOT NULL default ' ',
  "AAABKOCQ" varchar(7) NOT NULL default ' ',
  "AAAAMNYJ" varchar(1) NOT NULL default ' ',
  "AAABKOQD" varchar(2) NOT NULL default ' ',
  "AAAAMNYL" varchar(1) NOT NULL default ' ',
  "AAABKOQF" decimal(5,0) NOT NULL default '0',
  "AAABKOQE" varchar(3) NOT NULL default ' ',
  "AAAAMNYO" varchar(1) NOT NULL default ' ',
  "AAABKOQH" varchar(2) NOT NULL default ' ',
  "AAAAMNYQ" varchar(1) NOT NULL default ' ',
  "AAABKOQG" decimal(5,0) NOT NULL default '0',
  "AAABKOQI" varchar(3) NOT NULL default ' ',
  "AAAAMNYT" varchar(1) NOT NULL default ' ',
  "AAABKNMU" varchar(6) NOT NULL default ' ',
  "AAAAMNYV" varchar(1) NOT NULL default ' ',
  "AAABKNND" varchar(6) NOT NULL default ' ',
  "AAAAMNYX" varchar(1) NOT NULL default ' ',
  "AAABCUCW" varchar(6) NOT NULL default ' ',
  "AAABCUCY" varchar(6) NOT NULL default ' ',
  "AAABCUDA" varchar(6) NOT NULL default ' ',
  "AAABDOIZ" varchar(6) NOT NULL default ' ',
  "AAABDOJA" varchar(6) NOT NULL default ' ',
  "AAABCUDG" varchar(6) NOT NULL default ' ',
  "AAAAMNZE" varchar(3) NOT NULL default ' ',
  "AAAAMNZF" varchar(3) NOT NULL default ' ',
  "AAAAMNZG" varchar(3) NOT NULL default ' ',
  "AAAAMNZH" varchar(3) NOT NULL default ' ',
  "AAAAMNZI" varchar(3) NOT NULL default ' ',
  "AAAAMNZJ" varchar(3) NOT NULL default ' ',
  "AAAAMNZK" decimal(9,3) NOT NULL default '0.000',
  "AAAAMNZL" decimal(9,3) NOT NULL default '0.000',
  "AAAAMNZM" decimal(9,3) NOT NULL default '0.000',
  "AAAAMNZN" decimal(9,3) NOT NULL default '0.000',
  "AAAAMNZO" decimal(9,3) NOT NULL default '0.000',
  "AAAAMNZP" decimal(9,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABKNLE","AAABLUNJ","AAABMLEL"),
  KEY "AAAAERWF" ("AAABMRIL","AAAAMNYF","AAAAMNYG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERWJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAERZV" varchar(4) NOT NULL default '0000',
  "AAAAERZW" varchar(2) NOT NULL default '00',
  "AAABLQPP" varchar(4) NOT NULL default ' ',
  "AAAAERZY" varchar(5) NOT NULL default '00000',
  "AAABLXQP" varchar(8) NOT NULL default '00000000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAERWT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAERZV","AAAAERZW","AAABLQPP","AAAAERZY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERWZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAERZV" varchar(4) NOT NULL default '0000',
  "AAAAERZW" varchar(2) NOT NULL default '00',
  "AAABLQPP" varchar(4) NOT NULL default ' ',
  "AAAAERZY" varchar(5) NOT NULL default '00000',
  "AAAAERZZ" varchar(10) NOT NULL default '0000000000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAZOCZ" varchar(1) NOT NULL default ' ',
  "AAABJLRA" varchar(25) NOT NULL default ' ',
  "AAABMCNV" varchar(5) NOT NULL default ' ',
  "AAAAERZG" decimal(15,2) NOT NULL default '0.00',
  "AAAAERXN" decimal(15,2) NOT NULL default '0.00',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABKWII" varchar(8) NOT NULL default '00000000',
  "AAABLXQP" varchar(8) NOT NULL default '00000000',
  "AAAAIFUX" varchar(15) NOT NULL default ' ',
  "AAABHFBC" varchar(5) NOT NULL default ' ',
  "AAAAERZI" decimal(15,2) NOT NULL default '0.00',
  "AAAAERXV" decimal(15,2) NOT NULL default '0.00',
  "AAAAERXW" varchar(60) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAAAERXY" varchar(4) NOT NULL default ' ',
  "AAABDTFR" varchar(20) NOT NULL default ' ',
  "AAABMDGB" varchar(16) NOT NULL default ' ',
  "AAABMCNE" varchar(25) NOT NULL default ' ',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAERZV","AAAAERZW","AAABLQPP","AAAAERZY","AAAAERZZ"),
  KEY "AAAAERYJ" ("AAABLQPP","AAABJLRA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAERYM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAERZV" varchar(4) NOT NULL default '0000',
  "AAAAERZW" varchar(2) NOT NULL default '00',
  "AAABLQPP" varchar(4) NOT NULL default ' ',
  "AAAAERZY" varchar(5) NOT NULL default '00000',
  "AAAAERZZ" varchar(10) NOT NULL default '0000000000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCNJ" varchar(1) NOT NULL default ' ',
  "AAABJLRA" varchar(25) NOT NULL default ' ',
  "AAAAEXWB" varchar(1) NOT NULL default ' ',
  "AAABMCZN" varchar(10) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABKWII" varchar(8) NOT NULL default '00000000',
  "AAABMCNV" varchar(5) NOT NULL default ' ',
  "AAAAERZG" decimal(15,2) NOT NULL default '0.00',
  "AAABHFBC" varchar(5) NOT NULL default ' ',
  "AAAAERZI" decimal(15,2) NOT NULL default '0.00',
  "AAABDTFR" varchar(20) NOT NULL default ' ',
  "AAABMCMG" varchar(22) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(73) NOT NULL default ' ',
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABLSIN" varchar(3) NOT NULL default ' ',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAERZV","AAAAERZW","AAABLQPP","AAAAERZY","AAAAERZZ"),
  KEY "AAAAESAA" ("AAABLQPP","AAABJLRA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESAD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNLE" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAESAJ" varchar(8) NOT NULL default '00000000',
  "AAAAESAK" varchar(12) NOT NULL default ' ',
  "AAAAESAL" varchar(8) NOT NULL default '00000000',
  "AAAAESAM" varchar(12) NOT NULL default ' ',
  "AAAAESAN" varchar(8) NOT NULL default '00000000',
  "AAAAESAO" varchar(12) NOT NULL default ' ',
  "AAAAESAP" varchar(8) NOT NULL default '00000000',
  "AAAAESAQ" varchar(12) NOT NULL default ' ',
  "AAABHHST" varchar(8) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABEMIH" varchar(4) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  "AAABCUCW" varchar(6) NOT NULL default ' ',
  "AAABCUCY" varchar(6) NOT NULL default ' ',
  "AAABCUDA" varchar(6) NOT NULL default ' ',
  "AAABDOIZ" varchar(6) NOT NULL default ' ',
  "AAABDOJA" varchar(6) NOT NULL default ' ',
  "AAABCUDG" varchar(6) NOT NULL default ' ',
  "AAAAMNZE" varchar(3) NOT NULL default ' ',
  "AAAAMNZF" varchar(3) NOT NULL default ' ',
  "AAAAMNZG" varchar(3) NOT NULL default ' ',
  "AAAAMNZH" varchar(3) NOT NULL default ' ',
  "AAAAMNZI" varchar(3) NOT NULL default ' ',
  "AAAAMNZJ" varchar(3) NOT NULL default ' ',
  "AAAAMNZK" decimal(9,3) NOT NULL default '0.000',
  "AAAAMNZL" decimal(9,3) NOT NULL default '0.000',
  "AAAAMNZM" decimal(9,3) NOT NULL default '0.000',
  "AAAAMNZN" decimal(9,3) NOT NULL default '0.000',
  "AAAAMNZO" decimal(9,3) NOT NULL default '0.000',
  "AAAAMNZP" decimal(9,3) NOT NULL default '0.000',
  "AAABCUGF" varchar(3) NOT NULL default ' ',
  "AAABHYAL" varchar(10) NOT NULL default ' ',
  "AAABHOML" varchar(3) NOT NULL default ' ',
  "AAABKOCY" varchar(4) NOT NULL default ' ',
  "AAAAESBR" varchar(1) NOT NULL default ' ',
  "AAAAESBS" varchar(1) NOT NULL default ' ',
  "AAAAESBT" varchar(1) NOT NULL default ' ',
  "AAAAESBU" varchar(1) NOT NULL default ' ',
  "AAAAESBV" varchar(1) NOT NULL default ' ',
  "AAAAESBW" varchar(1) NOT NULL default ' ',
  "AAAAESBX" varchar(1) NOT NULL default ' ',
  "AAAAESBY" varchar(1) NOT NULL default ' ',
  "AAAAESBZ" varchar(1) NOT NULL default ' ',
  "AAABKKDS" varchar(1) NOT NULL default ' ',
  "AAAAPUJE" varchar(3) NOT NULL default ' ',
  "AAAAPUJF" varchar(3) NOT NULL default ' ',
  "AAAAPUJG" varchar(3) NOT NULL default ' ',
  "AAAAPUJH" varchar(3) NOT NULL default ' ',
  "AAAAPUJI" varchar(3) NOT NULL default ' ',
  "AAAAPUJJ" varchar(3) NOT NULL default ' ',
  "AAABKOCQ" varchar(7) NOT NULL default ' ',
  "AAABKODJ" decimal(3,0) NOT NULL default '0',
  "AAABKODK" varchar(4) NOT NULL default ' ',
  "AAABKODS" varchar(3) NOT NULL default ' ',
  "AAABKODM" varchar(2) NOT NULL default ' ',
  "AAABKODQ" varchar(2) NOT NULL default ' ',
  "AAABKOCM" varchar(4) NOT NULL default ' ',
  "AAABKQIT" varchar(3) NOT NULL default ' ',
  "AAABKQIV" varchar(3) NOT NULL default ' ',
  "AAABKQIX" varchar(3) NOT NULL default ' ',
  "AAABKQIZ" varchar(3) NOT NULL default ' ',
  "AAABKQJB" varchar(3) NOT NULL default ' ',
  "AAABKQJD" varchar(3) NOT NULL default ' ',
  "AAAAMNYJ" varchar(1) NOT NULL default ' ',
  "AAAAESCV" varchar(1) NOT NULL default ' ',
  "AAAAESCW" varchar(1) NOT NULL default ' ',
  "AAAAESCX" varchar(1) NOT NULL default ' ',
  "AAAAESCY" varchar(1) NOT NULL default ' ',
  "AAAAESCZ" varchar(1) NOT NULL default ' ',
  "AAAAESDA" varchar(1) NOT NULL default ' ',
  "AAAAESDB" varchar(6) NOT NULL default ' ',
  "AAAAESDC" varchar(6) NOT NULL default ' ',
  "AAAAESDD" varchar(6) NOT NULL default ' ',
  "AAABKQFL" varchar(8) NOT NULL default '00000000',
  "AAABCUJK" varchar(4) NOT NULL default ' ',
  "AAABKOMC" varchar(3) NOT NULL default ' ',
  "AAABKOMD" decimal(9,3) NOT NULL default '0.000',
  "AAABKOME" varchar(3) NOT NULL default ' ',
  "AAABKOMF" decimal(9,3) NOT NULL default '0.000',
  "AAAAESDK" varchar(1) NOT NULL default ' ',
  "AAABHHSU" varchar(10) NOT NULL default ' ',
  "AAABKMHD" varchar(3) NOT NULL default ' ',
  "AAABKMHE" varchar(9) NOT NULL default ' ',
  "AAAAESDO" decimal(3,1) NOT NULL default '0.0',
  "AAAAESDP" decimal(3,1) NOT NULL default '0.0',
  "AAAAESDQ" decimal(3,1) NOT NULL default '0.0',
  "AAABKODL" varchar(3) NOT NULL default '000',
  "AAABMAYH" varchar(4) NOT NULL default ' ',
  "AAAAESFV" varchar(8) NOT NULL default '00000000',
  "AAAAESDU" varchar(6) NOT NULL default ' ',
  "AAAAESDV" varchar(1) NOT NULL default ' ',
  "AAAAESDW" varchar(1) NOT NULL default ' ',
  "AAAAESDX" varchar(1) NOT NULL default ' ',
  "AAAAESDY" varchar(1) NOT NULL default ' ',
  "AAAAESDZ" varchar(1) NOT NULL default ' ',
  "AAAAESEA" varchar(1) NOT NULL default ' ',
  "AAAAESEB" varchar(4) NOT NULL default ' ',
  "AAAAESEC" varchar(4) NOT NULL default ' ',
  "AAAAESED" varchar(4) NOT NULL default ' ',
  "AAAAESEE" varchar(4) NOT NULL default ' ',
  "AAABDENC" varchar(4) NOT NULL default ' ',
  "AAAAESEG" varchar(1) NOT NULL default ' ',
  "AAAAESEH" varchar(1) NOT NULL default ' ',
  "AAABKRNW" varchar(10) NOT NULL default ' ',
  "AAABKOGP" varchar(6) NOT NULL default ' ',
  "AAAAZWBB" varchar(3) NOT NULL default ' ',
  "AAABKRRK" varchar(1) NOT NULL default ' ',
  "AAAAESFU" varchar(2) NOT NULL default ' ',
  "AAABLCNC" varchar(7) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAAAESEP" varchar(1) NOT NULL default ' ',
  "AAAAESEQ" varchar(1) NOT NULL default ' ',
  "AAAAESER" varchar(3) NOT NULL default ' ',
  "AAAAESES" varchar(6) NOT NULL default ' ',
  "AAAAESFT" varchar(2) NOT NULL default ' ',
  "AAAAESEU" varchar(3) NOT NULL default ' ',
  "AAAAVEIC" varchar(22) NOT NULL default ' ',
  "AAAAESEW" varchar(8) NOT NULL default ' ',
  "AAAAESFZ" varchar(4) NOT NULL default ' ',
  "AAAAESEY" varchar(1) NOT NULL default ' ',
  "AAAAESEZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNLE","AAABLUNJ"),
  KEY "AAAAESFD" ("AAABMRIL","AAABMCVI","AAABHHST"),
  KEY "AAAAESFH" ("AAABMRIL","AAABEMIH","AAABMCVI","AAABHHST"),
  KEY "AAAAESFM" ("AAABMRIL","AAABHOML","AAABMCVI","AAABHHST"),
  KEY "AAAAESFR" ("AAABMRIL","AAAAESFT","AAAAESFU","AAAAESFV"),
  KEY "AAAAESFW" ("AAABMRIL","AAABMCVI","AAAAESFZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESGA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNLE" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABMLPV" varchar(10) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABHOML" varchar(3) NOT NULL default ' ',
  "AAAAESGH" varchar(1) NOT NULL default ' ',
  "AAAASMYP" varchar(1) NOT NULL default ' ',
  "AAAAESGJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNLE","AAABLUNJ"),
  KEY "AAAAESGN" ("AAABMRIL","AAABMLPV","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESGR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDTCE" varchar(2) NOT NULL default ' ',
  "AAABDTCF" varchar(8) NOT NULL default '00000000',
  "AAAAESHM" varchar(2) NOT NULL default ' ',
  "AAAAESHN" varchar(8) NOT NULL default '00000000',
  "AAAAESHJ" varchar(2) NOT NULL default ' ',
  "AAAAESHK" varchar(8) NOT NULL default '00000000',
  "AAAAESGZ" varchar(2) NOT NULL default ' ',
  "AAAAESHA" varchar(8) NOT NULL default '00000000',
  "AAAAVHUU" varchar(5) NOT NULL default ' ',
  "AAAAVHUV" varchar(5) NOT NULL default ' ',
  "AAAAESHD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDTCE","AAABDTCF","AAAAESHM","AAAAESHN","AAAAESHJ","AAAAESHK"),
  KEY "AAAAESHL" ("AAAAESHM","AAAAESHN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVGY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNLE" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABKNLE","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESHV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIWUJ" varchar(32) NOT NULL default ' ',
  "AAABIWUM" varchar(1) NOT NULL default ' ',
  "AAABIWTZ" varchar(1) NOT NULL default ' ',
  "AAAAESJC" smallint(6) NOT NULL default '0',
  "AAAAESIB" blob,
  PRIMARY KEY ("AAABMRIL","AAABIWUJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESIE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIWUJ" varchar(32) NOT NULL default ' ',
  "AAABIWUK" varchar(32) NOT NULL default ' ',
  "AAABIWUM" varchar(1) NOT NULL default ' ',
  "AAAAESIJ" varchar(8) NOT NULL default ' ',
  "AAAAESIK" varchar(3) NOT NULL default '000',
  "AAAAESIL" varchar(5) NOT NULL default ' ',
  "AAAAESIM" decimal(13,2) NOT NULL default '0.00',
  "AAAAESIN" varchar(8) NOT NULL default '00000000',
  "AAAAESIO" decimal(9,5) NOT NULL default '0.00000',
  "AAAAESIP" double NOT NULL default '0',
  "AAAAESIQ" tinyint(3) unsigned NOT NULL default '0',
  "AAAAESIR" tinyint(3) unsigned NOT NULL default '0',
  "AAAAESIS" int(11) NOT NULL default '0',
  "AAAAESIT" varchar(1) NOT NULL default ' ',
  "AAAAESIU" varchar(6) NOT NULL default '000000',
  "AAAAESIV" decimal(2,0) NOT NULL default '0',
  "AAAAESIW" decimal(15,3) NOT NULL default '0.000',
  "AAAAESIX" VARBINARY(255),
  "AAAAESIY" varchar(6) NOT NULL default '000000',
  "AAAAESIZ" varchar(3) NOT NULL default ' ',
  "AAAAESJA" varchar(6) NOT NULL default ' ',
  "AAABIWTZ" varchar(1) NOT NULL default ' ',
  "AAAAESJC" smallint(6) NOT NULL default '0',
  "AAAAESJD" text,
  PRIMARY KEY ("AAABMRIL","AAABIWUJ","AAABIWUK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESJH" (
  "AAABLEUZ" varchar(12) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABIYEN" varchar(40) NOT NULL default ' ',
  "AAABIYEO" varchar(40) NOT NULL default ' ',
  "AAABIYJL" varchar(30) NOT NULL default ' ',
  "AAABLSAL" varchar(10) NOT NULL default ' ',
  "AAABEMDP" varchar(40) NOT NULL default ' ',
  "AAABIXXY" varchar(10) NOT NULL default ' ',
  "AAAATMFN" varchar(10) NOT NULL default ' ',
  "AAABGRXB" varchar(241) NOT NULL default ' ',
  "AAABIYDW" varchar(10) NOT NULL default ' ',
  "AAAAZCSW" varchar(12) NOT NULL default ' ',
  "AAAAZCSX" varchar(12) NOT NULL default ' ',
  "AAAAETIC" varchar(12) NOT NULL default ' ',
  "AAAAETID" varchar(12) NOT NULL default ' ',
  "AAAAETIE" varchar(12) NOT NULL default ' ',
  "AAAAVPNG" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLEUZ"),
  UNIQUE KEY "AAAAESKA" ("AAABLZKF"),
  KEY "AAAAESKC" ("AAAAZCSW"),
  KEY "AAAAESKE" ("AAAAZCSX"),
  KEY "AAAAESKG" ("AAAAETIC"),
  KEY "AAAAESKI" ("AAAAETID"),
  KEY "AAAAESKK" ("AAAAETIE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESKM" (
  "AAABLEUZ" varchar(12) NOT NULL default ' ',
  "AAABFLHW" varchar(10) NOT NULL default ' ',
  "AAAAESKW" varchar(3) NOT NULL default '000',
  "AAABLEPD" varchar(40) NOT NULL default ' ',
  "AAABLEPE" varchar(20) NOT NULL default ' ',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAAAVPNG" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLEUZ","AAABFLHW","AAAAESKW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESKX" (
  "AAABIYDW" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABIYDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESLA" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIYDW" varchar(10) NOT NULL default ' ',
  "AAAAESLD" varchar(10) NOT NULL default ' ',
  "AAAAESLE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIYDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESLH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESLM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABDOEA" varchar(22) NOT NULL default ' ',
  "AAABDOED" varchar(22) NOT NULL default ' ',
  "AAABJJLO" varchar(22) NOT NULL default ' ',
  "AAABJJLF" varchar(4) NOT NULL default ' ',
  "AAABDODY" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABDOEA","AAABDOED"),
  KEY "AAAAESLW" ("AAABJJLO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESLY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAESMD" varchar(4) NOT NULL default ' ',
  "AAAAESMB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAESMD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESME" (
  "AAAAESPC" varchar(3) NOT NULL default ' ',
  "AAAAESPD" varchar(4) NOT NULL default ' ',
  "AAAAESMW" varchar(5) NOT NULL default ' ',
  "AAABJFUZ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAAAESPC","AAAAESPD","AAAAESMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESMM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAESPC" varchar(3) NOT NULL default ' ',
  "AAAAESPD" varchar(4) NOT NULL default ' ',
  "AAAAESMW" varchar(5) NOT NULL default ' ',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABJFUZ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAESPC","AAAAESPD","AAAAESMW","AAABLKXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESMY" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAESND" varchar(2) NOT NULL default '00',
  "AAABIWTO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAESND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESNE" (
  "AAAAESPC" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAAAESPC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESNH" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAESPC" varchar(3) NOT NULL default ' ',
  "AAAAESOJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAESPC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESNN" (
  "AAAAESPC" varchar(3) NOT NULL default ' ',
  "AAAAESPD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAAESPC","AAAAESPD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESNS" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAESPC" varchar(3) NOT NULL default ' ',
  "AAAAESPD" varchar(4) NOT NULL default ' ',
  "AAAAESOZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAESPC","AAAAESPD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESOA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAESPC" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAESPC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESOF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAESPC" varchar(3) NOT NULL default ' ',
  "AAAAESOJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAAAESPC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESON" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAESPC" varchar(3) NOT NULL default ' ',
  "AAAAESPD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAESPC","AAAAESPD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESOU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAESPC" varchar(3) NOT NULL default ' ',
  "AAAAESPD" varchar(4) NOT NULL default ' ',
  "AAAAESOZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAAAESPC","AAAAESPD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESPE" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLTYE" varchar(4) NOT NULL default ' ',
  "AAABLTYF" varchar(14) NOT NULL default ' ',
  "AAABLTYG" varchar(20) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLXDT" varchar(14) NOT NULL default '00000000000000',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLTYE","AAABLTYF","AAABLTYG","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESPS" (
  "AAAAUBIJ" varchar(10) NOT NULL default ' ',
  "AAAAESPU" varchar(30) NOT NULL default ' ',
  "AAAAESPV" varchar(30) NOT NULL default ' ',
  "AAAAEWHK" varchar(30) NOT NULL default ' ',
  "AAAAEWHL" varchar(10) NOT NULL default ' ',
  "AAAAESPY" varchar(12) NOT NULL default ' ',
  "AAAAESPZ" varchar(12) NOT NULL default ' ',
  "AAAAESQA" varchar(30) NOT NULL default ' ',
  "AAAAESQB" varchar(24) NOT NULL default ' ',
  "AAAAVQTD" varchar(60) NOT NULL default ' ',
  "AAAAESQD" varchar(2) NOT NULL default ' ',
  "AAAAESQE" varchar(2) NOT NULL default ' ',
  "AAAAESQF" int(11) NOT NULL default '0',
  "AAAAESQG" int(11) NOT NULL default '0',
  "AAAAESQH" varchar(1) NOT NULL default ' ',
  "AAAAESQI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUBIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESQK" (
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABLEMJ" varchar(10) NOT NULL default ' ',
  "AAABAGFD" varchar(10) NOT NULL default ' ',
  "AAABHGWF" varchar(1) NOT NULL default ' ',
  "AAABLEML" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAATQHP","AAABLMNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESQT" (
  "AAAATQHO" varchar(1) NOT NULL default ' ',
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(48) NOT NULL default ' ',
  "AAAAUBIJ" varchar(10) NOT NULL default ' ',
  "AAAAEWTK" varchar(4) NOT NULL default ' ',
  "AAAAEWTL" varchar(4) NOT NULL default ' ',
  "AAAAEWTM" varchar(48) NOT NULL default ' ',
  "AAAAEWTN" varchar(3) NOT NULL default '000',
  "AAABCNCP" varchar(30) NOT NULL default ' ',
  "AAAAEWSU" varchar(10) NOT NULL default ' ',
  "AAAAEWSV" varchar(10) NOT NULL default '0000000000',
  "AAAAEWSW" varchar(4) NOT NULL default '0000',
  "AAAAEWSX" varchar(10) NOT NULL default ' ',
  "AAAAEWSY" varchar(10) NOT NULL default ' ',
  "AAAAESRK" varchar(12) NOT NULL default ' ',
  "AAAAESRL" varchar(8) NOT NULL default '00000000',
  "AAAAESRM" varchar(8) NOT NULL default '00000000',
  "AAAAESRN" varchar(12) NOT NULL default ' ',
  "AAABDSRO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATQHO","AAAATQHP","AAABLRST","AAABMKFH","AAABLFYD","AAAAUBIJ","AAAAEWTK","AAAAEWTL","AAAAEWTM","AAAAEWTN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESRZ" (
  "AAAATQHO" varchar(1) NOT NULL default ' ',
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(48) NOT NULL default ' ',
  "AAAAUBIJ" varchar(10) NOT NULL default ' ',
  "AAAAEWTK" varchar(4) NOT NULL default ' ',
  "AAAAEWTL" varchar(4) NOT NULL default ' ',
  "AAAAEWTM" varchar(48) NOT NULL default ' ',
  "AAAAEWTN" varchar(3) NOT NULL default '000',
  "AAAAETIF" varchar(8) NOT NULL default ' ',
  "AAAAETIG" varchar(3) NOT NULL default '000',
  "AAAAESTE" varchar(4) NOT NULL default ' ',
  "AAAAETIH" varchar(10) NOT NULL default ' ',
  "AAAAETII" varchar(10) NOT NULL default ' ',
  "AAAATUFG" varchar(1) NOT NULL default ' ',
  "AAAAESSQ" varchar(1) NOT NULL default ' ',
  "AAAAESSR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATQHO","AAAATQHP","AAABLRST","AAABMKFH","AAABLFYD","AAAAUBIJ","AAAAEWTK","AAAAEWTL","AAAAEWTM","AAAAEWTN","AAAAETIF","AAAAETIG","AAAAESTE","AAAAETIH","AAAAETII","AAAATUFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESTI" (
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAABLUVJ" varchar(32) NOT NULL default ' ',
  "AAAAESUI" varchar(8) NOT NULL default ' ',
  "AAABIJEW" varchar(2) NOT NULL default '00',
  "AAAAESUK" varchar(15) NOT NULL default ' ',
  "AAAAESUL" varchar(20) NOT NULL default ' ',
  "AAAAEXSN" varchar(10) NOT NULL default ' ',
  "AAAAEXSO" varchar(15) NOT NULL default ' ',
  "AAAAEXSP" varchar(3) NOT NULL default '000',
  "AAAAIXXH" varchar(10) NOT NULL default ' ',
  "AAAAEXSB" varchar(15) NOT NULL default ' ',
  "AAAAEXSC" varchar(3) NOT NULL default '000',
  "AAAAEXTD" varchar(10) NOT NULL default ' ',
  "AAAAEXTE" varchar(20) NOT NULL default ' ',
  "AAAAEXTF" varchar(4) NOT NULL default '0000',
  "AAAAEXTG" varchar(3) NOT NULL default '000',
  "AAAAEXTH" varchar(1) NOT NULL default ' ',
  "AAAAESUX" varchar(12) NOT NULL default '000000000000',
  "AAAAESUY" varchar(3) NOT NULL default ' ',
  "AAAAESUZ" varchar(8) NOT NULL default '00000000',
  "AAAAUQQT" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAAATQHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESUF" (
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAABLUVJ" varchar(32) NOT NULL default ' ',
  "AAAAESUI" varchar(8) NOT NULL default ' ',
  "AAABIJEW" varchar(2) NOT NULL default '00',
  "AAAAESUK" varchar(15) NOT NULL default ' ',
  "AAAAESUL" varchar(20) NOT NULL default ' ',
  "AAAAEXSN" varchar(10) NOT NULL default ' ',
  "AAAAEXSO" varchar(15) NOT NULL default ' ',
  "AAAAEXSP" varchar(3) NOT NULL default '000',
  "AAAAIXXH" varchar(10) NOT NULL default ' ',
  "AAAAEXSB" varchar(15) NOT NULL default ' ',
  "AAAAEXSC" varchar(3) NOT NULL default '000',
  "AAAAEXTD" varchar(10) NOT NULL default ' ',
  "AAAAEXTE" varchar(20) NOT NULL default ' ',
  "AAAAEXTF" varchar(4) NOT NULL default '0000',
  "AAAAEXTG" varchar(3) NOT NULL default '000',
  "AAAAEXTH" varchar(1) NOT NULL default ' ',
  "AAAAESUX" varchar(12) NOT NULL default '000000000000',
  "AAAAESUY" varchar(3) NOT NULL default ' ',
  "AAAAESUZ" varchar(8) NOT NULL default '00000000',
  "AAAAUQQT" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAAATQHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESVC" (
  "AAABFVDJ" varchar(30) NOT NULL default ' ',
  "AAAATQHO" varchar(1) NOT NULL default ' ',
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(48) NOT NULL default ' ',
  "AAAAUBIJ" varchar(10) NOT NULL default ' ',
  "AAAAEWTK" varchar(4) NOT NULL default ' ',
  "AAAAEWTL" varchar(4) NOT NULL default ' ',
  "AAAAEWTM" varchar(48) NOT NULL default ' ',
  "AAAAESWO" varchar(8) NOT NULL default '00000000',
  "AAAAESWP" varchar(10) NOT NULL default ' ',
  "AAAAEXVL" varchar(14) NOT NULL default '00000000000000',
  "AAABJAEN" varchar(12) NOT NULL default ' ',
  "AAAAVPNG" varchar(14) NOT NULL default '00000000000000',
  "AAAAVPNH" varchar(12) NOT NULL default ' ',
  "AAABANQK" varchar(10) NOT NULL default ' ',
  "AAAAESVU" varchar(12) NOT NULL default ' ',
  "AAAAESVV" varchar(12) NOT NULL default ' ',
  "AAAAESVW" varchar(1) NOT NULL default ' ',
  "AAAAESVX" varchar(14) NOT NULL default '00000000000000',
  "AAAAEVVM" varchar(10) NOT NULL default ' ',
  "AAAAESVZ" varchar(1) NOT NULL default ' ',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAAAETFZ" varchar(1) NOT NULL default ' ',
  "AAAAESWC" varchar(60) NOT NULL default ' ',
  "AAABGGFU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFVDJ","AAAATQHO","AAAATQHP","AAABLRST","AAABMKFH","AAABLFYD","AAAAUBIJ","AAAAEWTK","AAAAEWTL","AAAAEWTM","AAAAESWO","AAAAESWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESWQ" (
  "AAAATQHO" varchar(1) NOT NULL default ' ',
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(48) NOT NULL default ' ',
  "AAAAUBIJ" varchar(10) NOT NULL default ' ',
  "AAAAEWTK" varchar(4) NOT NULL default ' ',
  "AAAAEWTL" varchar(4) NOT NULL default ' ',
  "AAAAEWTM" varchar(48) NOT NULL default ' ',
  "AAAAEWTN" varchar(3) NOT NULL default '000',
  "AAAAETFG" varchar(3) NOT NULL default '000',
  "AAAAETJH" varchar(14) NOT NULL default '00000000000000',
  "AAAAETEV" varchar(255) NOT NULL default ' ',
  "AAAAETJI" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAAATQHO","AAAATQHP","AAABLRST","AAABMKFH","AAABLFYD","AAAAUBIJ","AAAAEWTK","AAAAEWTL","AAAAEWTM","AAAAEWTN","AAAAETFG","AAAAETJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESXR" (
  "AAAAUBIJ" varchar(10) NOT NULL default ' ',
  "AAAAETHE" varchar(10) NOT NULL default ' ',
  "AAAAEWTK" varchar(4) NOT NULL default ' ',
  "AAAAEWDJ" varchar(10) NOT NULL default ' ',
  "AAAAESXW" varchar(10) NOT NULL default ' ',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAAAESXY" varchar(1) NOT NULL default '0',
  "AAAAVQTD" varchar(60) NOT NULL default ' ',
  "AAAAEWST" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAAAUBIJ","AAAAETHE","AAAAEWTK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESYE" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUBIJ" varchar(10) NOT NULL default ' ',
  "AAAAETHE" varchar(10) NOT NULL default ' ',
  "AAAAEWTK" varchar(4) NOT NULL default ' ',
  "AAAAESYJ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAUBIJ","AAAAETHE","AAAAEWTK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESYO" (
  "AAAAUBIJ" varchar(10) NOT NULL default ' ',
  "AAAAETHE" varchar(10) NOT NULL default ' ',
  "AAAAEWTK" varchar(4) NOT NULL default ' ',
  "AAAAESYS" varchar(1) NOT NULL default '0',
  "AAABJDPL" varchar(8) NOT NULL default '00000000',
  "AAABDCRX" varchar(1) NOT NULL default ' ',
  "AAAAESYV" varchar(40) NOT NULL default ' ',
  "AAABFDQT" varchar(6) NOT NULL default '000000',
  "AAABFQWU" varchar(8) NOT NULL default '00000000',
  "AAAAESYY" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUBIJ","AAAAETHE","AAAAEWTK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESZC" (
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAAAESZE" varchar(12) NOT NULL default ' ',
  "AAABJDPL" varchar(8) NOT NULL default '00000000',
  "AAABIFVP" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLENH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESZI" (
  "AAAAUBIJ" varchar(10) NOT NULL default ' ',
  "AAAAEWHS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUBIJ","AAAAEWHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAESZN" (
  "AAAATQHO" varchar(1) NOT NULL default ' ',
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(48) NOT NULL default ' ',
  "AAAARVZQ" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAAATQHO","AAAATQHP","AAABLRST","AAABMKFH","AAABLFYD"),
  KEY "AAAAESZZ" ("AAAATQHO","AAAATQHP","AAAARVZQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETAD" (
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(48) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABEZHC" varchar(20) NOT NULL default ' ',
  "AAAAETHN" varchar(24) NOT NULL default ' ',
  "AAABFVDJ" varchar(30) NOT NULL default ' ',
  "AAAAQVTN" varchar(12) NOT NULL default ' ',
  "AAAAETHP" varchar(1) NOT NULL default ' ',
  "AAAAETHQ" varchar(12) NOT NULL default ' ',
  "AAAAETHR" varchar(12) NOT NULL default ' ',
  "AAAAETHS" varchar(12) NOT NULL default ' ',
  "AAAAETHT" varchar(1) NOT NULL default ' ',
  "AAAAETHU" varchar(1) NOT NULL default ' ',
  "AAAAETHV" varchar(1) NOT NULL default ' ',
  "AAABFVER" varchar(1) NOT NULL default ' ',
  "AAABCBJK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLRST","AAABMKFH","AAABLFYD"),
  KEY "AAAAETAY" ("AAABIRUN","AAABFVER"),
  KEY "AAAAETBB" ("AAAAETHN","AAABFVER"),
  KEY "AAAAETBE" ("AAAAQVTN","AAABFVER"),
  KEY "AAAAETBH" ("AAAAETHN","AAABIRUN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETBK" (
  "AAAATQHO" varchar(1) NOT NULL default ' ',
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAAAEWUD" varchar(1) NOT NULL default ' ',
  "AAAAEXVL" varchar(14) NOT NULL default '00000000000000',
  "AAABJAEN" varchar(12) NOT NULL default ' ',
  "AAAAVPNG" varchar(14) NOT NULL default '00000000000000',
  "AAAAVPNH" varchar(12) NOT NULL default ' ',
  "AAAAETBS" varchar(14) NOT NULL default '00000000000000',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAARUOH" varchar(1) NOT NULL default ' ',
  "AAAAETBV" varchar(1) NOT NULL default ' ',
  "AAAAETBW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATQHO","AAAATQHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETBZ" (
  "AAAATQHO" varchar(1) NOT NULL default ' ',
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(120) NOT NULL default ' ',
  "AAAAETCN" varchar(4) NOT NULL default ' ',
  "AAAAETCO" varchar(4) NOT NULL default ' ',
  "AAAAETCP" varchar(48) NOT NULL default ' ',
  PRIMARY KEY ("AAAATQHO","AAAATQHP","AAABLRST","AAABMKFH","AAABLFYD","AAAAETCN","AAAAETCO","AAAAETCP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETCQ" (
  "AAAATQHO" varchar(1) NOT NULL default ' ',
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAETCU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAATQHO","AAAATQHP","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETCY" (
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(48) NOT NULL default ' ',
  "AAAAETHP" varchar(1) NOT NULL default ' ',
  "AAABLEUZ" varchar(12) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLRST","AAABMKFH","AAABLFYD","AAAAETHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETDJ" (
  "AAAAUBIJ" varchar(10) NOT NULL default ' ',
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUBIJ","AAABLRST","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETDQ" (
  "AAAAETKT" varchar(10) NOT NULL default ' ',
  "AAAAUBIJ" varchar(10) NOT NULL default ' ',
  "AAAAEWQY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAAETKT","AAAAUBIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETDW" (
  "AAAAETKT" varchar(10) NOT NULL default ' ',
  "AAAAEXVL" varchar(14) NOT NULL default '00000000000000',
  "AAABJAEN" varchar(12) NOT NULL default ' ',
  "AAAAVPNG" varchar(14) NOT NULL default '00000000000000',
  "AAAAVPNH" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAAAETKT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETED" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAETKT" varchar(10) NOT NULL default ' ',
  "AAAAETEG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAETKT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETEJ" (
  "AAAATQHO" varchar(1) NOT NULL default ' ',
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(48) NOT NULL default ' ',
  "AAAAUBIJ" varchar(10) NOT NULL default ' ',
  "AAAAEWTK" varchar(4) NOT NULL default ' ',
  "AAAAEWTL" varchar(4) NOT NULL default ' ',
  "AAAAEWTM" varchar(48) NOT NULL default ' ',
  "AAAAEWTN" varchar(3) NOT NULL default '000',
  "AAAAETFG" varchar(3) NOT NULL default '000',
  "AAAAETEV" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAATQHO","AAAATQHP","AAABLRST","AAABMKFH","AAABLFYD","AAAAUBIJ","AAAAEWTK","AAAAEWTL","AAAAEWTM","AAAAEWTN","AAAAETFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETFH" (
  "AAAATQHO" varchar(1) NOT NULL default ' ',
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(48) NOT NULL default ' ',
  "AAAAUBIJ" varchar(10) NOT NULL default ' ',
  "AAAAEWTK" varchar(4) NOT NULL default ' ',
  "AAAAEWTL" varchar(4) NOT NULL default ' ',
  "AAAAEWTM" varchar(48) NOT NULL default ' ',
  "AAAAEWTN" varchar(3) NOT NULL default '000',
  "AAAAETHH" varchar(14) NOT NULL default '00000000000000',
  "AAAAETFT" varchar(1) NOT NULL default ' ',
  "AAAAETFU" varchar(1) NOT NULL default ' ',
  "AAAAETHI" varchar(1) NOT NULL default ' ',
  "AAAAETHJ" varchar(1) NOT NULL default ' ',
  "AAAAETHK" varchar(1) NOT NULL default ' ',
  "AAABKWQO" varchar(1) NOT NULL default ' ',
  "AAAAETFZ" varchar(1) NOT NULL default ' ',
  "AAAAETIK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATQHO","AAAATQHP","AAABLRST","AAABMKFH","AAABLFYD","AAAAUBIJ","AAAAEWTK","AAAAEWTL","AAAAEWTM","AAAAEWTN"),
  KEY "AAAAETGL" ("AAAATQHO","AAAATQHP","AAAAUBIJ","AAAAETHK","AAAAEWTK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETGR" (
  "AAAATQHO" varchar(1) NOT NULL default ' ',
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(48) NOT NULL default ' ',
  "AAAAUBIJ" varchar(10) NOT NULL default ' ',
  "AAAAEWTK" varchar(4) NOT NULL default ' ',
  "AAAAEWTL" varchar(4) NOT NULL default ' ',
  "AAAAEWTM" varchar(48) NOT NULL default ' ',
  "AAAAEWTN" varchar(3) NOT NULL default '000',
  "AAABFVDJ" varchar(30) NOT NULL default ' ',
  "AAAAEWTD" varchar(10) NOT NULL default ' ',
  "AAAAETHE" varchar(10) NOT NULL default ' ',
  "AAAAEWDJ" varchar(10) NOT NULL default ' ',
  "AAAAEWST" varchar(1) NOT NULL default '0',
  "AAAAETHH" varchar(14) NOT NULL default '00000000000000',
  "AAAAETHI" varchar(1) NOT NULL default ' ',
  "AAAAETHJ" varchar(1) NOT NULL default ' ',
  "AAAAETHK" varchar(1) NOT NULL default ' ',
  "AAABKWQO" varchar(1) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAAAETHN" varchar(24) NOT NULL default ' ',
  "AAAAQVTN" varchar(12) NOT NULL default ' ',
  "AAAAETHP" varchar(1) NOT NULL default ' ',
  "AAAAETHQ" varchar(12) NOT NULL default ' ',
  "AAAAETHR" varchar(12) NOT NULL default ' ',
  "AAAAETHS" varchar(12) NOT NULL default ' ',
  "AAAAETHT" varchar(1) NOT NULL default ' ',
  "AAAAETHU" varchar(1) NOT NULL default ' ',
  "AAAAETHV" varchar(1) NOT NULL default ' ',
  "AAABFVER" varchar(1) NOT NULL default ' ',
  "AAABCBJK" varchar(4) NOT NULL default ' ',
  "AAABLSAL" varchar(10) NOT NULL default ' ',
  "AAABGRXB" varchar(241) NOT NULL default ' ',
  "AAAAZCSW" varchar(12) NOT NULL default ' ',
  "AAAAZCSX" varchar(12) NOT NULL default ' ',
  "AAAAETIC" varchar(12) NOT NULL default ' ',
  "AAAAETID" varchar(12) NOT NULL default ' ',
  "AAAAETIE" varchar(12) NOT NULL default ' ',
  "AAAAETIF" varchar(8) NOT NULL default ' ',
  "AAAAETIG" varchar(3) NOT NULL default '000',
  "AAAAETIH" varchar(10) NOT NULL default ' ',
  "AAAAETII" varchar(10) NOT NULL default ' ',
  "AAAATUFG" varchar(1) NOT NULL default ' ',
  "AAAAETIK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATQHO","AAAATQHP","AAABLRST","AAABMKFH","AAABLFYD","AAAAUBIJ","AAAAEWTK","AAAAEWTL","AAAAEWTM","AAAAEWTN","AAABFVDJ","AAAAEWTD"),
  KEY "AAAAETIX" ("AAAATQHO","AAAATQHP","AAAAUBIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETJB" (
  "AAAATQHO" varchar(1) NOT NULL default ' ',
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAAAETOH" varchar(4) NOT NULL default '0000',
  "AAAAEXVL" varchar(14) NOT NULL default '00000000000000',
  "AAABJAEN" varchar(12) NOT NULL default ' ',
  "AAAAETJH" varchar(14) NOT NULL default '00000000000000',
  "AAAAETJI" varchar(14) NOT NULL default '00000000000000',
  "AAABIXJK" varchar(1) NOT NULL default ' ',
  "AAAAETJK" varchar(1) NOT NULL default ' ',
  "AAAAETKW" varchar(4) NOT NULL default '0000',
  "AAAAETJM" varchar(4) NOT NULL default '0000',
  "AAAAETKX" varchar(4) NOT NULL default '0000',
  "AAAAETJO" varchar(4) NOT NULL default '0000',
  "AAAAETJP" varchar(10) NOT NULL default '0000000000',
  "AAAARVZQ" varchar(10) NOT NULL default '0000000000',
  "AAABHFAO" varchar(1) NOT NULL default ' ',
  "AAAAETMP" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(10) NOT NULL default ' ',
  "AAAAETJU" varchar(4) NOT NULL default '0000',
  "AAAAETJV" varchar(4) NOT NULL default '0000',
  "AAAAETJW" varchar(32) NOT NULL default ' ',
  "AAAAETJX" varchar(8) NOT NULL default ' ',
  "AAABGCZX" varchar(20) NOT NULL default ' ',
  "AAABEDVF" varchar(20) NOT NULL default ' ',
  "AAAAETKA" varchar(20) NOT NULL default ' ',
  "AAAAETKB" varchar(1) NOT NULL default ' ',
  "AAAAETKC" varchar(14) NOT NULL default ' ',
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAAAETKE" varchar(1) NOT NULL default ' ',
  "AAABGAQM" varchar(1) NOT NULL default ' ',
  "AAAAETKG" varchar(1) NOT NULL default ' ',
  "AAAAETKH" varchar(1) NOT NULL default ' ',
  "AAAAETKI" varchar(12) NOT NULL default ' ',
  "AAAAETKJ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAATQHO","AAAATQHP","AAAAETOH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETKN" (
  "AAAATQHO" varchar(1) NOT NULL default ' ',
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAAAETOH" varchar(4) NOT NULL default '0000',
  "AAAAUBIJ" varchar(10) NOT NULL default ' ',
  "AAAAEWHL" varchar(10) NOT NULL default ' ',
  "AAAAETKT" varchar(10) NOT NULL default ' ',
  "AAAAEWQY" varchar(8) NOT NULL default '00000000',
  "AAAAETNQ" varchar(1) NOT NULL default ' ',
  "AAAAETKW" int(11) NOT NULL default '0',
  "AAAAETKX" int(11) NOT NULL default '0',
  "AAAAETKY" varchar(10) NOT NULL default '0000000000',
  "AAAAETKZ" varchar(10) NOT NULL default '0000000000',
  "AAAAETNS" int(11) NOT NULL default '0',
  "AAAAETMH" int(11) NOT NULL default '0',
  "AAAAETNU" varchar(10) NOT NULL default '0000000000',
  "AAAAETML" varchar(10) NOT NULL default '0000000000',
  "AAAAETLE" varchar(1) NOT NULL default ' ',
  "AAAAETLF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAATQHO","AAAATQHP","AAAAETOH","AAAAUBIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETLK" (
  "AAAATQHO" varchar(1) NOT NULL default ' ',
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAAAETOH" varchar(4) NOT NULL default '0000',
  "AAAAETMP" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(10) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAABEDVF" varchar(20) NOT NULL default ' ',
  "AAAARVZQ" varchar(10) NOT NULL default '0000000000',
  "AAAAETLU" varchar(10) NOT NULL default '0000000000',
  "AAAAETNQ" varchar(1) NOT NULL default ' ',
  "AAAAEXVL" varchar(14) NOT NULL default '00000000000000',
  "AAAAETLX" varchar(14) NOT NULL default '00000000000000',
  "AAAAETLY" varchar(14) NOT NULL default '00000000000000',
  "AAAAETLZ" varchar(1) NOT NULL default ' ',
  "AAABHFAO" varchar(1) NOT NULL default ' ',
  "AAAAEWHK" varchar(30) NOT NULL default ' ',
  "AAAAETMC" varchar(10) NOT NULL default ' ',
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(48) NOT NULL default ' ',
  "AAAAETNS" int(11) NOT NULL default '0',
  "AAAAETMH" int(11) NOT NULL default '0',
  "AAAAETNT" int(11) NOT NULL default '0',
  "AAAAETMJ" varchar(1) NOT NULL default '0',
  "AAABCROU" varchar(8) NOT NULL default ' ',
  "AAAAETML" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAAATQHO","AAAATQHP","AAAAETOH","AAAAETMP","AAABLUSV","AAABLTJH","AAABLTJI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETMT" (
  "AAAATQHO" varchar(1) NOT NULL default ' ',
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAAAETOH" varchar(4) NOT NULL default '0000',
  "AAAAUBIJ" varchar(10) NOT NULL default ' ',
  "AAAAEWQY" varchar(8) NOT NULL default '00000000',
  "AAABLQEI" varchar(1) NOT NULL default ' ',
  "AAABLQEJ" varchar(2) NOT NULL default ' ',
  "AAAAUNSO" varchar(4) NOT NULL default ' ',
  "AAAAUNSP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAATQHO","AAAATQHP","AAAAETOH","AAAAUBIJ","AAAAEWQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETNI" (
  "AAAATQHO" varchar(1) NOT NULL default ' ',
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAAAETOH" varchar(4) NOT NULL default '0000',
  "AAAAEWHS" varchar(10) NOT NULL default ' ',
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAAAEWHL" varchar(10) NOT NULL default ' ',
  "AAAAEWQY" varchar(8) NOT NULL default '00000000',
  "AAAAETNQ" varchar(1) NOT NULL default ' ',
  "AAAAETNR" varchar(10) NOT NULL default '0000000000',
  "AAAAETNS" int(11) NOT NULL default '0',
  "AAAAETNT" int(11) NOT NULL default '0',
  "AAAAETNU" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAAATQHO","AAAATQHP","AAAAETOH","AAAAEWHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETNZ" (
  "AAAATQHO" varchar(1) NOT NULL default ' ',
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAAAETOH" varchar(4) NOT NULL default '0000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAETOE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAATQHO","AAAATQHP","AAAAETOH","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETOJ" (
  "AAAATQHO" varchar(1) NOT NULL default ' ',
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAAAUBIJ" varchar(10) NOT NULL default ' ',
  "AAAAETON" varchar(2) NOT NULL default '00',
  "AAAAETOO" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAAATQHO","AAAATQHP","AAAAUBIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETOS" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUBIJ" varchar(10) NOT NULL default ' ',
  "AAAAEWCA" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAUBIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETOY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEVZT" varchar(1) NOT NULL default ' ',
  "AAAAHLFC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEVZT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETPE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCFHS" varchar(10) NOT NULL default ' ',
  "AAABLQTM" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  "AAAAETPJ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCFHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETPM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(34) NOT NULL default ' ',
  "AAABMLTD" varchar(5) NOT NULL default '00000',
  "AAAAMIZS" varchar(1) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABKPUA" varchar(6) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABKSTP" varchar(2) NOT NULL default ' ',
  "AAABMPTM" decimal(15,3) NOT NULL default '0.000',
  "AAAARZAY" varchar(3) NOT NULL default ' ',
  "AAABMBIU" decimal(15,2) NOT NULL default '0.00',
  "AAAAETQA" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABGVKJ" varchar(8) NOT NULL default '00000000',
  "AAAAMSGJ" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAEVTD" varchar(1) NOT NULL default ' ',
  "AAAAETQH" varchar(1) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABMLTD"),
  KEY "AAAAETQM" ("AAABLXTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETQO" (
  "AAABLUER" varchar(20) NOT NULL default ' ',
  "AAAAXLLM" varchar(30) NOT NULL default ' ',
  "AAAAETQR" varchar(30) NOT NULL default ' ',
  "AAAAETQS" varchar(30) NOT NULL default ' ',
  "AAAAETQT" varchar(1) NOT NULL default ' ',
  "AAABFPRE" varchar(20) NOT NULL default ' ',
  "AAAARZHW" varchar(20) NOT NULL default ' ',
  "AAAAEUTL" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER","AAAAXLLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETQZ" (
  "AAAAETRC" varchar(20) NOT NULL default ' ',
  "AAAAETRB" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAAETRC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETRD" (
  "AAABLUER" varchar(20) NOT NULL default ' ',
  "AAABMIRY" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER","AAABMIRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETRI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPPR" varchar(4) NOT NULL default ' ',
  "AAAAETRQ" varchar(10) NOT NULL default ' ',
  "AAAAETRM" varchar(10) NOT NULL default ' ',
  "AAABDMCO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPPR","AAAAETRQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETRR" (
  "AAABLUER" varchar(20) NOT NULL default ' ',
  "AAAAEVUN" varchar(10) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAAAEURX" varchar(2) NOT NULL default ' ',
  "AAAAWMFF" varchar(10) NOT NULL default ' ',
  "AAAAWSWJ" varchar(4) NOT NULL default ' ',
  "AAAAXLLM" varchar(30) NOT NULL default ' ',
  "AAABIMFT" varchar(32) NOT NULL default ' ',
  "AAAAEVII" varchar(10) NOT NULL default ' ',
  "AAAAETSX" varchar(10) NOT NULL default ' ',
  "AAAAEVAC" varchar(10) NOT NULL default ' ',
  "AAAAEVOC" varchar(10) NOT NULL default ' ',
  "AAAAEVCB" varchar(10) NOT NULL default ' ',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLUNJ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER","AAAAEVUN","AAABMPMH","AAAAEURX","AAAAWMFF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETSM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUER" varchar(20) NOT NULL default ' ',
  "AAAAEVUN" varchar(10) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAAAEURX" varchar(2) NOT NULL default ' ',
  "AAAAWMFF" varchar(10) NOT NULL default ' ',
  "AAAAWSWJ" varchar(4) NOT NULL default ' ',
  "AAAAXLLM" varchar(30) NOT NULL default ' ',
  "AAABIMFT" varchar(32) NOT NULL default ' ',
  "AAAAEVII" varchar(10) NOT NULL default ' ',
  "AAAAETSX" varchar(10) NOT NULL default ' ',
  "AAAAEVAC" varchar(10) NOT NULL default ' ',
  "AAAAEVOC" varchar(10) NOT NULL default ' ',
  "AAAAEVCB" varchar(10) NOT NULL default ' ',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLUNJ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUER","AAAAEVUN","AAABMPMH","AAAAEURX","AAAAWMFF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETTJ" (
  "AAABLUER" varchar(20) NOT NULL default ' ',
  "AAAAEVMC" varchar(20) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAAAETTN" varchar(10) NOT NULL default ' ',
  "AAAAEUZR" varchar(30) NOT NULL default ' ',
  "AAABIJOU" varchar(1) NOT NULL default ' ',
  "AAAAETUB" varchar(30) NOT NULL default ' ',
  "AAAAETTR" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETTT" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUER" varchar(20) NOT NULL default ' ',
  "AAABLPSU" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLUER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETTZ" (
  "AAAAEVMC" varchar(20) NOT NULL default ' ',
  "AAAAETUB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEVMC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETUD" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEVMC" varchar(20) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAEVMC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETUJ" (
  "AAAAETZO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAETZO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETUM" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAETZO" varchar(10) NOT NULL default ' ',
  "AAABLPSU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAETZO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETUS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLECD" varchar(20) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLECD","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETVD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABMKFH" varchar(20) NOT NULL default ' ',
  "AAAAETVQ" varchar(10) NOT NULL default '0000000000',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLUSV","AAABMKFH","AAAAETVQ","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETVS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAAAETWF" varchar(10) NOT NULL default '0000000000',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLUSV","AAABMKFH","AAAAETWF","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETWH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLUER" varchar(20) NOT NULL default ' ',
  "AAAAEUBI" varchar(30) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAAAEUZR" varchar(30) NOT NULL default ' ',
  "AAABFSPQ" varchar(100) NOT NULL default ' ',
  "AAAAEUAO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLUER","AAAAEUBI","AAABLZKF","AAAAEUZR","AAABFSPQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETWV" (
  "AAABLUER" varchar(20) NOT NULL default ' ',
  "AAAAEUBI" varchar(10) NOT NULL default ' ',
  "AAAAEUZR" varchar(30) NOT NULL default ' ',
  "AAAAEUAO" varchar(30) NOT NULL default ' ',
  "AAAAEUAP" varchar(30) NOT NULL default ' ',
  "AAABJSDF" int(11) NOT NULL default '0',
  "AAAAEUAR" longblob,
  PRIMARY KEY ("AAABLUER","AAAAEUBI","AAAAEUZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETXG" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUER" varchar(20) NOT NULL default ' ',
  "AAAAEUBI" varchar(10) NOT NULL default ' ',
  "AAAAEUZR" varchar(30) NOT NULL default ' ',
  "AAABLPSU" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLUER","AAAAEUBI","AAAAEUZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETXQ" (
  "AAAAWSWJ" varchar(4) NOT NULL default ' ',
  "AAABIJSC" varchar(30) NOT NULL default ' ',
  "AAAALTQS" varchar(70) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAAAETXV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAWSWJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETXX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAWSWJ" varchar(4) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAWSWJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETYD" (
  "AAABLUER" varchar(20) NOT NULL default ' ',
  "AAAAWMFF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER","AAAAWMFF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETYI" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUER" varchar(20) NOT NULL default ' ',
  "AAAAWMFF" varchar(10) NOT NULL default ' ',
  "AAABLPSU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLUER","AAAAWMFF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETYQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAEVAC" varchar(10) NOT NULL default ' ',
  "AAAAEVUN" varchar(10) NOT NULL default ' ',
  "AAABHBOY" varchar(10) NOT NULL default ' ',
  "AAAAETZO" varchar(10) NOT NULL default ' ',
  "AAAAEUAH" varchar(10) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAEVAC","AAAAEVUN","AAABHBOY","AAAAETZO","AAAAEUAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETZE" (
  "AAAAEVAC" varchar(10) NOT NULL default ' ',
  "AAAAEVUN" varchar(10) NOT NULL default ' ',
  "AAABHBOY" varchar(10) NOT NULL default ' ',
  "AAAAETZO" varchar(10) NOT NULL default ' ',
  "AAAAEUAH" varchar(10) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEVAC","AAAAEVUN","AAABHBOY","AAAAETZO","AAAAEUAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETZQ" (
  "AAABHBOY" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABHBOY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETZT" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHBOY" varchar(10) NOT NULL default ' ',
  "AAABLPSU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHBOY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAETZZ" (
  "AAAAEUAH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEUAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUAC" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEUAH" varchar(10) NOT NULL default ' ',
  "AAABLPSU" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAEUAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUAI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLUER" varchar(20) NOT NULL default ' ',
  "AAAAEUBI" varchar(10) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAAAEUZR" varchar(30) NOT NULL default ' ',
  "AAAAEUAO" varchar(30) NOT NULL default ' ',
  "AAAAEUAP" varchar(30) NOT NULL default ' ',
  "AAABJSDF" smallint(6) NOT NULL default '0',
  "AAAAEUAR" longblob,
  PRIMARY KEY ("AAABLUXQ","AAABLUER","AAAAEUBI","AAABLZKF","AAAAEUZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUAX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUER" varchar(20) NOT NULL default ' ',
  "AAAAEUBI" varchar(10) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAAAEUZR" varchar(30) NOT NULL default ' ',
  "AAABLPSU" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABLUER","AAAAEUBI","AAABLZKF","AAAAEUZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUBL" (
  "AAAAEVUN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEVUN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUBO" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEVUN" varchar(10) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAEVUN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUBU" (
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAAAEUBW" varchar(1) NOT NULL default ' ',
  "AAAAEUQO" varchar(20) NOT NULL default ' ',
  "AAAAEUQS" varchar(30) NOT NULL default ' ',
  "AAAAEUQT" varchar(1) NOT NULL default ' ',
  "AAAAEUQU" varchar(20) NOT NULL default ' ',
  "AAAAEUQV" varchar(20) NOT NULL default ' ',
  "AAAAEUQW" varchar(30) NOT NULL default ' ',
  "AAAAEUCD" varchar(20) NOT NULL default ' ',
  "AAAAEUQX" varchar(1) NOT NULL default ' ',
  "AAAAEUCF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUCH" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABLPSU" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUCN" (
  "AAABLUER" varchar(20) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABLMOZ" varchar(30) NOT NULL default ' ',
  "AAAAEUDI" varchar(20) NOT NULL default ' ',
  "AAABMJYX" varchar(30) NOT NULL default ' ',
  "AAABLTYA" varchar(30) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAAAEURX" varchar(2) NOT NULL default ' ',
  "AAAAEUCX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER","AAABMLDW","AAABMLDX","AAABLMOZ","AAAAEUDI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUDD" (
  "AAAAEUDI" varchar(20) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAAAEUTL" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEUDI","AAABMLDW","AAABMLDX"),
  KEY "AAAAEUDL" ("AAABMLDX","AAABMLDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUDO" (
  "AAABDUBG" varchar(20) NOT NULL default ' ',
  "AAAAEUDW" varchar(1) NOT NULL default ' ',
  "AAAAEUDX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDUBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUDT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDUBG" varchar(20) NOT NULL default ' ',
  "AAAAEUDW" varchar(1) NOT NULL default ' ',
  "AAAAEUDX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDUBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUEA" (
  "AAABDUBG" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABDUBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUED" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDUBG" varchar(20) NOT NULL default ' ',
  "AAABLPSU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABDUBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUEJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEUEN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEUEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUEO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEUEQ" varchar(1) NOT NULL default ' ',
  "AAAAEUER" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUET" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGMML" varchar(5) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABMLTD" varchar(7) NOT NULL default '0000000',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABCSNP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGMML","AAABKWSS","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUFE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEUFP" varchar(4) NOT NULL default ' ',
  "AAAAEUFQ" varchar(4) NOT NULL default ' ',
  "AAAAEUFR" varchar(6) NOT NULL default ' ',
  "AAAAEUFJ" varchar(2) NOT NULL default '00',
  "AAAAEUFK" varchar(4) NOT NULL default ' ',
  "AAAAEUFL" varchar(4) NOT NULL default ' ',
  "AAAAEUFM" varchar(6) NOT NULL default ' ',
  "AAAAEUFN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEUFP","AAAAEUFQ","AAAAEUFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUFS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEUFU" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUFW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEUGE" varchar(20) NOT NULL default ' ',
  "AAABHRNL" VARBINARY(255) NOT NULL,
  "AAAAEUHD" VARBINARY(255),
  "AAAAEUGS" varchar(1) NOT NULL default ' ',
  "AAAAEUGR" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEUGE","AAABHRNL"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUGG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABHRNL" VARBINARY(255) NOT NULL,
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLFZK","AAABHRNL"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUGO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHRNL" VARBINARY(255) NOT NULL,
  "AAAAEUGR" varchar(10) NOT NULL default ' ',
  "AAAAEUGS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABHRNL"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUGV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  "AAAAEUHD" VARBINARY(255) NOT NULL,
  PRIMARY KEY ("AAABLUXQ","AAABLFZK","AAABLPSU","AAAAEUHD"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAEUHG" varchar(255) NOT NULL default ' ',
  "AAAAEUHH" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUHJ" (
  "AAAAWSWJ" varchar(4) NOT NULL default ' ',
  "AAAAEUHL" varchar(4) NOT NULL default ' ',
  "AAAAEUHM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAWSWJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUHO" (
  "AAAAEVMC" varchar(20) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEVMC","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUHT" (
  "AAAAEVHZ" varchar(10) NOT NULL default ' ',
  "AAABGIFA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEVHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUHX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEVHZ" varchar(10) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAEVHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUID" (
  "AAABIMFT" varchar(32) NOT NULL default ' ',
  "AAAAEVUN" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAAEUJB" varchar(30) NOT NULL default ' ',
  "AAAAEUQO" varchar(20) NOT NULL default ' ',
  "AAAAEUIW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIMFT","AAAAEVUN","AAABMREF","AAAAEUJB","AAAAEUQO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUIP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMFT" varchar(32) NOT NULL default ' ',
  "AAAAEVUN" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAAEUJB" varchar(30) NOT NULL default ' ',
  "AAAAEUQO" varchar(20) NOT NULL default ' ',
  "AAAAEUIW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMFT","AAAAEVUN","AAABMREF","AAAAEUJB","AAAAEUQO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUJD" (
  "AAABIMFT" varchar(32) NOT NULL default ' ',
  "AAAAEVUN" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAAEUKB" varchar(30) NOT NULL default ' ',
  "AAAAEUQO" varchar(20) NOT NULL default ' ',
  "AAAAEUJW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIMFT","AAAAEVUN","AAABMREF","AAAAEUKB","AAAAEUQO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUJP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMFT" varchar(32) NOT NULL default ' ',
  "AAAAEVUN" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAAEUKB" varchar(30) NOT NULL default ' ',
  "AAAAEUQO" varchar(20) NOT NULL default ' ',
  "AAAAEUJW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMFT","AAAAEVUN","AAABMREF","AAAAEUKB","AAAAEUQO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUKD" (
  "AAABIMFT" varchar(32) NOT NULL default ' ',
  "AAAAXLLM" varchar(30) NOT NULL default ' ',
  "AAAAEUKG" varchar(1) NOT NULL default ' ',
  "AAAAEUKH" varchar(1) NOT NULL default ' ',
  "AAAAEUKI" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABIMFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUKK" (
  "AAABIMFT" varchar(32) NOT NULL default ' ',
  "AAAAEVUN" varchar(10) NOT NULL default ' ',
  "AAAAEURX" varchar(6) NOT NULL default '000000',
  "AAAAEVUP" varchar(10) NOT NULL default ' ',
  "AAAAEUQI" varchar(32) NOT NULL default ' ',
  "AAAASAJM" varchar(4) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLRJL" varchar(4) NOT NULL default ' ',
  "AAAAEUMG" varchar(1) NOT NULL default ' ',
  "AAAAEUMH" varchar(1) NOT NULL default ' ',
  "AAAAEUMI" varchar(30) NOT NULL default ' ',
  "AAABIJZM" varchar(1) NOT NULL default ' ',
  "AAABLBEK" varchar(1) NOT NULL default ' ',
  "AAAAEUQO" varchar(20) NOT NULL default ' ',
  "AAAAEUQP" varchar(1) NOT NULL default ' ',
  "AAABLNNZ" varchar(50) NOT NULL default ' ',
  "AAAAEUQR" varchar(1) NOT NULL default ' ',
  "AAAAEUQS" varchar(30) NOT NULL default ' ',
  "AAAAEUQT" varchar(1) NOT NULL default ' ',
  "AAAAEUQU" varchar(20) NOT NULL default ' ',
  "AAAAEUQV" varchar(20) NOT NULL default ' ',
  "AAAAEUQW" varchar(30) NOT NULL default ' ',
  "AAAAEUQX" varchar(1) NOT NULL default ' ',
  "AAAAEUQY" varchar(1) NOT NULL default ' ',
  "AAAAEUQZ" varchar(30) NOT NULL default ' ',
  "AAAAEURA" varchar(1) NOT NULL default ' ',
  "AAAAEURB" varchar(1) NOT NULL default ' ',
  "AAAAQRTG" varchar(1) NOT NULL default ' ',
  "AAAAEUNA" smallint(6) NOT NULL default '0',
  "AAAAEUNB" varchar(1) NOT NULL default ' ',
  "AAAAEUNC" varchar(1) NOT NULL default ' ',
  "AAAAEUND" smallint(6) NOT NULL default '0',
  "AAAAEUNE" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAABIMFT","AAAAEVUN","AAAAEURX","AAAAEVUP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEULW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMFT" varchar(32) NOT NULL default ' ',
  "AAAAEVUN" varchar(10) NOT NULL default ' ',
  "AAAAEURX" varchar(6) NOT NULL default '000000',
  "AAAAEVUP" varchar(10) NOT NULL default ' ',
  "AAAAEUQI" varchar(32) NOT NULL default ' ',
  "AAAASAJM" varchar(4) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLRJL" varchar(4) NOT NULL default ' ',
  "AAAAEUMG" varchar(1) NOT NULL default ' ',
  "AAAAEUMH" varchar(1) NOT NULL default ' ',
  "AAAAEUMI" varchar(30) NOT NULL default ' ',
  "AAABIJZM" varchar(1) NOT NULL default ' ',
  "AAABLBEK" varchar(1) NOT NULL default ' ',
  "AAAAEUQO" varchar(20) NOT NULL default ' ',
  "AAAAEUQP" varchar(1) NOT NULL default ' ',
  "AAABLNNZ" varchar(50) NOT NULL default ' ',
  "AAAAEUQR" varchar(1) NOT NULL default ' ',
  "AAAAEUQS" varchar(30) NOT NULL default ' ',
  "AAAAEUQT" varchar(1) NOT NULL default ' ',
  "AAAAEUQU" varchar(20) NOT NULL default ' ',
  "AAAAEUQV" varchar(20) NOT NULL default ' ',
  "AAAAEUQW" varchar(30) NOT NULL default ' ',
  "AAAAEUQX" varchar(1) NOT NULL default ' ',
  "AAAAEUQY" varchar(1) NOT NULL default ' ',
  "AAAAEUQZ" varchar(30) NOT NULL default ' ',
  "AAAAEURA" varchar(1) NOT NULL default ' ',
  "AAAAEURB" varchar(1) NOT NULL default ' ',
  "AAAAQRTG" varchar(1) NOT NULL default ' ',
  "AAAAEUNA" smallint(6) NOT NULL default '0',
  "AAAAEUNB" varchar(1) NOT NULL default ' ',
  "AAAAEUNC" varchar(1) NOT NULL default ' ',
  "AAAAEUND" smallint(6) NOT NULL default '0',
  "AAAAEUNE" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABIMFT","AAAAEVUN","AAAAEURX","AAAAEVUP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUNK" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIMFT" varchar(32) NOT NULL default ' ',
  "AAABLPSU" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIMFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUNQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAJOZG" varchar(4) NOT NULL default ' ',
  "AAABIMRZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAJOZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUNW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAJOZG" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEUOA" varchar(20) NOT NULL default ' ',
  "AAAAEUOB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAJOZG","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUOF" (
  "AAAAEURU" varchar(32) NOT NULL default ' ',
  "AAAAEURF" varchar(3) NOT NULL default ' ',
  "AAAAEVUN" varchar(10) NOT NULL default ' ',
  "AAAAEVUP" varchar(10) NOT NULL default ' ',
  "AAAAEURI" int(11) NOT NULL default '0',
  "AAABIMFT" varchar(32) NOT NULL default ' ',
  "AAAATQYA" int(11) NOT NULL default '0',
  "AAAAEURL" int(11) NOT NULL default '0',
  "AAAAEUQF" int(11) NOT NULL default '0',
  "AAAAEUQG" int(11) NOT NULL default '0',
  "AAAASAJM" varchar(4) NOT NULL default ' ',
  "AAAAEUQI" varchar(32) NOT NULL default ' ',
  "AAABLRJL" varchar(4) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAAEUQL" varchar(1) NOT NULL default ' ',
  "AAABIJZM" varchar(1) NOT NULL default ' ',
  "AAABLBEK" varchar(1) NOT NULL default ' ',
  "AAAAEUQO" varchar(20) NOT NULL default ' ',
  "AAAAEUQP" varchar(1) NOT NULL default ' ',
  "AAABLNNZ" varchar(50) NOT NULL default ' ',
  "AAAAEUQR" varchar(1) NOT NULL default ' ',
  "AAAAEUQS" varchar(30) NOT NULL default ' ',
  "AAAAEUQT" varchar(1) NOT NULL default ' ',
  "AAAAEUQU" varchar(20) NOT NULL default ' ',
  "AAAAEUQV" varchar(20) NOT NULL default ' ',
  "AAAAEUQW" varchar(30) NOT NULL default ' ',
  "AAAAEUQX" varchar(1) NOT NULL default ' ',
  "AAAAEUQY" varchar(1) NOT NULL default ' ',
  "AAAAEUQZ" varchar(30) NOT NULL default ' ',
  "AAAAEURA" varchar(1) NOT NULL default ' ',
  "AAAAEURB" varchar(1) NOT NULL default ' ',
  "AAAAQRTG" varchar(1) NOT NULL default ' ',
  "AAAAEUPM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEURU","AAAAEURF","AAAAEVUN","AAAAEVUP","AAAAEURI","AAABIMFT","AAAATQYA","AAAAEURL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUPV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAEURU" varchar(32) NOT NULL default ' ',
  "AAAAEURF" varchar(3) NOT NULL default ' ',
  "AAAAEVUN" varchar(10) NOT NULL default ' ',
  "AAAAEVUP" varchar(10) NOT NULL default ' ',
  "AAAAEURI" int(11) NOT NULL default '0',
  "AAABIMFT" varchar(32) NOT NULL default ' ',
  "AAAATQYA" int(11) NOT NULL default '0',
  "AAAAEURL" int(11) NOT NULL default '0',
  "AAAAEUQF" int(11) NOT NULL default '0',
  "AAAAEUQG" int(11) NOT NULL default '0',
  "AAAASAJM" varchar(4) NOT NULL default ' ',
  "AAAAEUQI" varchar(32) NOT NULL default ' ',
  "AAABLRJL" varchar(4) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAAEUQL" varchar(1) NOT NULL default ' ',
  "AAABIJZM" varchar(1) NOT NULL default ' ',
  "AAABLBEK" varchar(1) NOT NULL default ' ',
  "AAAAEUQO" varchar(20) NOT NULL default ' ',
  "AAAAEUQP" varchar(1) NOT NULL default ' ',
  "AAABLNNZ" varchar(50) NOT NULL default ' ',
  "AAAAEUQR" varchar(1) NOT NULL default ' ',
  "AAAAEUQS" varchar(30) NOT NULL default ' ',
  "AAAAEUQT" varchar(1) NOT NULL default ' ',
  "AAAAEUQU" varchar(20) NOT NULL default ' ',
  "AAAAEUQV" varchar(20) NOT NULL default ' ',
  "AAAAEUQW" varchar(30) NOT NULL default ' ',
  "AAAAEUQX" varchar(1) NOT NULL default ' ',
  "AAAAEUQY" varchar(1) NOT NULL default ' ',
  "AAAAEUQZ" varchar(30) NOT NULL default ' ',
  "AAAAEURA" varchar(1) NOT NULL default ' ',
  "AAAAEURB" varchar(1) NOT NULL default ' ',
  "AAAAQRTG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAEURU","AAAAEURF","AAAAEVUN","AAAAEVUP","AAAAEURI","AAABIMFT","AAAATQYA","AAAAEURL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEURM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAEURU" varchar(32) NOT NULL default ' ',
  "AAAAEVUN" varchar(10) NOT NULL default ' ',
  "AAAAEVUP" varchar(10) NOT NULL default ' ',
  "AAAAEURX" varchar(6) NOT NULL default '000000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAEURU","AAAAEVUN","AAAAEVUP","AAAAEURX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEURY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAEUSI" varchar(10) NOT NULL default ' ',
  "AAABLQKV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAEUSI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUSE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAEUSI" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAEUSI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUSJ" (
  "AAAAEVCA" varchar(32) NOT NULL default ' ',
  "AAAAEVUN" varchar(10) NOT NULL default ' ',
  "AAABLMLR" varchar(2) NOT NULL default '00',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABKNCB" varchar(1) NOT NULL default ' ',
  "AAAAEUTC" varchar(3) NOT NULL default ' ',
  "AAAAEUTL" varchar(20) NOT NULL default ' ',
  "AAAAEUTE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEVCA","AAAAEVUN","AAABLMLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUSV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAEVCA" varchar(32) NOT NULL default ' ',
  "AAAAEVUN" varchar(10) NOT NULL default ' ',
  "AAABLMLR" varchar(2) NOT NULL default '00',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABKNCB" varchar(1) NOT NULL default ' ',
  "AAAAEUTC" varchar(3) NOT NULL default ' ',
  "AAAAEUTL" varchar(20) NOT NULL default ' ',
  "AAAAEUTE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAEVCA","AAAAEVUN","AAABLMLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUTJ" (
  "AAAAEVCA" varchar(32) NOT NULL default ' ',
  "AAAAEUTL" varchar(20) NOT NULL default ' ',
  "AAAAEUTM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEVCA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUTO" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEVCA" varchar(32) NOT NULL default ' ',
  "AAABLPSU" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAEVCA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUTU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEUTZ" varchar(16) NOT NULL default ' ',
  "AAAAEUTX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEUTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUUA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBJZL" varchar(32) NOT NULL default ' ',
  "AAAAEUUD" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBJZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUUG" (
  "AAABMIRY" varchar(20) NOT NULL default ' ',
  "AAABLUEO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMIRY","AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUUL" (
  "AAABLUEO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUUO" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUEO" varchar(20) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUUU" (
  "AAABMIRY" varchar(20) NOT NULL default ' ',
  "AAAAEUUW" varchar(10) NOT NULL default ' ',
  "AAAAEUUX" varchar(1) NOT NULL default ' ',
  "AAAAEUUY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMIRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUVA" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMIRY" varchar(20) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMIRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUVG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLNRB" varchar(30) NOT NULL default ' ',
  "AAABMIRY" varchar(20) NOT NULL default ' ',
  "AAABLUEO" varchar(20) NOT NULL default ' ',
  "AAAAEUVL" varchar(4) NOT NULL default '0000',
  "AAAAEUVM" varchar(1) NOT NULL default ' ',
  "AAABLUER" varchar(20) NOT NULL default ' ',
  "AAAAEVUN" varchar(10) NOT NULL default ' ',
  "AAAAEUVP" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLNRB","AAABMIRY","AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLNRB" varchar(30) NOT NULL default ' ',
  "AAAAEUVX" varchar(255) NOT NULL default ' ',
  "AAAAEUVY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLNRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUWB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAQNPB" varchar(4) NOT NULL default ' ',
  "AAABIMRZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAQNPB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUWH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAQNPB" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEUWL" varchar(20) NOT NULL default ' ',
  "AAAAEUWM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAQNPB","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUWQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAAAEVJR" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLEHK","AAAAEVJR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUWX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAAAEVJR" varchar(3) NOT NULL default ' ',
  "AAAAEUXC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABLEHK","AAAAEVJR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUXH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAEUYS" varchar(10) NOT NULL default ' ',
  "AAAAEVUN" varchar(10) NOT NULL default ' ',
  "AAABLMLR" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABHWAZ" varchar(10) NOT NULL default ' ',
  "AAAAEVCA" varchar(32) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAEUYS","AAAAEVUN","AAABLMLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUXU" (
  "AAAAEUYS" varchar(10) NOT NULL default ' ',
  "AAAAEVUN" varchar(10) NOT NULL default ' ',
  "AAABLMLR" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABHWAZ" varchar(10) NOT NULL default ' ',
  "AAAAEVCA" varchar(32) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEUYS","AAAAEVUN","AAABLMLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUYF" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEUYS" varchar(10) NOT NULL default ' ',
  "AAABLPSU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAEUYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUYL" (
  "AAAAEVMC" varchar(20) NOT NULL default ' ',
  "AAAAEUYS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEVMC","AAAAEUYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUYQ" (
  "AAAAEUYS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEUYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUYT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEUZF" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEUZF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUYY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEUZF" varchar(14) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEUZC" varchar(12) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEUZF","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUZH" (
  "AAAAEVMC" varchar(20) NOT NULL default ' ',
  "AAAAEVAC" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEVMC","AAAAEVAC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUZM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUER" varchar(20) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAAAEVUN" varchar(10) NOT NULL default ' ',
  "AAAAEUZR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUER","AAABLZKF","AAAAEVUN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEUZW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLOKS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVAA" (
  "AAAAEVAC" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEVAC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVAD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJCNM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJCNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVAI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJCNM" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEVAM" varchar(20) NOT NULL default ' ',
  "AAAAEVAN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJCNM","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVAR" (
  "AAAAEVMC" varchar(20) NOT NULL default ' ',
  "AAAAEVCB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEVMC","AAAAEVCB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVAW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAEVCB" varchar(10) NOT NULL default ' ',
  "AAAAEVUN" varchar(10) NOT NULL default ' ',
  "AAABLMLR" varchar(6) NOT NULL default '000000',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABDDOJ" varchar(20) NOT NULL default ' ',
  "AAABHWAZ" varchar(10) NOT NULL default ' ',
  "AAAAEVCA" varchar(32) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAEVCB","AAAAEVUN","AAABLMLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVBK" (
  "AAAAEVCB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEVCB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVBN" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEVCB" varchar(10) NOT NULL default ' ',
  "AAABLPSU" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAEVCB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVBT" (
  "AAAAEVCB" varchar(10) NOT NULL default ' ',
  "AAAAEVUN" varchar(10) NOT NULL default ' ',
  "AAABLMLR" varchar(6) NOT NULL default '000000',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABDDOJ" varchar(20) NOT NULL default ' ',
  "AAABHWAZ" varchar(10) NOT NULL default ' ',
  "AAAAEVCA" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEVCB","AAAAEVUN","AAABLMLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVCE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIKRP" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIKRP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVCJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIKRP" varchar(14) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEVCN" varchar(12) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIKRP","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVCS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCGRI" varchar(3) NOT NULL default ' ',
  "AAABCGQN" varchar(3) NOT NULL default ' ',
  "AAABHILD" varchar(4) NOT NULL default ' ',
  "AAABCGRK" varchar(20) NOT NULL default ' ',
  "AAAAEWGW" varchar(1) NOT NULL default ' ',
  "AAABCGPJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCGRI","AAABCGQN","AAABHILD","AAABCGRK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVDF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCGRI" varchar(3) NOT NULL default ' ',
  "AAABCGQN" varchar(3) NOT NULL default ' ',
  "AAABHILD" varchar(4) NOT NULL default ' ',
  "AAABCGRK" varchar(20) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABCGRI","AAABCGQN","AAABHILD","AAABCGRK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVDT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABJFWS" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLEHK","AAABJFWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVEA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABJFWS" varchar(15) NOT NULL default ' ',
  "AAABMJVD" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLFZK","AAABLEHK","AAABJFWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVEK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCGRI" varchar(3) NOT NULL default ' ',
  "AAABCGQN" varchar(3) NOT NULL default ' ',
  "AAABHILD" varchar(4) NOT NULL default ' ',
  "AAABCGRK" varchar(20) NOT NULL default ' ',
  "AAABMRCU" varchar(1) NOT NULL default '0',
  "AAAAEVEY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCGRI","AAABCGQN","AAABHILD","AAABCGRK","AAABMRCU","AAAAEVEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVEZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCGRI" varchar(3) NOT NULL default ' ',
  "AAABCGQN" varchar(3) NOT NULL default ' ',
  "AAABHILD" varchar(4) NOT NULL default ' ',
  "AAABCGRK" varchar(20) NOT NULL default ' ',
  "AAABHBLT" varchar(4) NOT NULL default ' ',
  "AAABHBKP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCGRI","AAABCGQN","AAABHILD","AAABCGRK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVFM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCGRI" varchar(3) NOT NULL default ' ',
  "AAAAEWGV" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCGRI","AAAAEWGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVFT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCGRI" varchar(3) NOT NULL default ' ',
  "AAAAEWGV" varchar(16) NOT NULL default ' ',
  "AAABLELJ" varchar(60) NOT NULL default ' ',
  "AAABMJVE" varchar(60) NOT NULL default ' ',
  "AAABMJVF" varchar(60) NOT NULL default ' ',
  "AAABMJVG" varchar(60) NOT NULL default ' ',
  "AAABMJVH" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABCGRI","AAAAEWGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVGH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHILD" varchar(4) NOT NULL default ' ',
  "AAABCGNT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABHILD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVGN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEWVR" varchar(10) NOT NULL default ' ',
  "AAAAEWVS" varchar(4) NOT NULL default ' ',
  "AAAAEWVV" varchar(10) NOT NULL default ' ',
  "AAAAEWVW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEWVR","AAAAEWVS","AAAAEWVV","AAAAEWVW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVGY" (
  "AAAAEVMC" varchar(20) NOT NULL default ' ',
  "AAAAEVII" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEVMC","AAAAEVII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVHD" (
  "AAAAEVII" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEVII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVHG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAEVII" varchar(10) NOT NULL default ' ',
  "AAAAEVUN" varchar(10) NOT NULL default ' ',
  "AAABLMLR" varchar(2) NOT NULL default '00',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAAAEVHY" varchar(1) NOT NULL default ' ',
  "AAAAEVHZ" varchar(10) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAEVII","AAAAEVUN","AAABLMLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVHT" (
  "AAAAEVII" varchar(10) NOT NULL default ' ',
  "AAAAEVUN" varchar(10) NOT NULL default ' ',
  "AAABLMLR" varchar(2) NOT NULL default '00',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAAAEVHY" varchar(1) NOT NULL default ' ',
  "AAAAEVHZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEVII","AAAAEVUN","AAABLMLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVID" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEVII" varchar(10) NOT NULL default ' ',
  "AAABLPSU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAEVII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVIJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEVIV" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEVIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVIO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEVIV" varchar(14) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEVIS" varchar(12) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEVIV","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVIX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAAAEVJT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLEHK","AAAAEVJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVJE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAAAEVJT" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABLEHK","AAAAEVJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVJO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAAAEVJR" varchar(3) NOT NULL default ' ',
  "AAAAEVJS" varchar(1) NOT NULL default ' ',
  "AAAAEVJT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLEHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVJW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABMIRY" varchar(20) NOT NULL default ' ',
  "AAAAEVKH" varchar(20) NOT NULL default ' ',
  "AAAAEVKI" varchar(20) NOT NULL default ' ',
  "AAAAUNIX" varchar(40) NOT NULL default ' ',
  "AAAAEVKD" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLZKF","AAABMIRY","AAAAEVKH","AAAAEVKI","AAAAUNIX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVKK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEVKW" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEVKW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVKP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEVKW" varchar(6) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEVKT" varchar(20) NOT NULL default ' ',
  "AAAAEVKU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEVKW","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVKY" (
  "AAABHWAZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABHWAZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVLB" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHWAZ" varchar(10) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHWAZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVLH" (
  "AAABHWAZ" varchar(10) NOT NULL default ' ',
  "AAAAEVUN" varchar(10) NOT NULL default ' ',
  "AAABLMLR" varchar(2) NOT NULL default '00',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABHWAZ","AAAAEVUN","AAABLMLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVLP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHWAZ" varchar(10) NOT NULL default ' ',
  "AAAAEVUN" varchar(10) NOT NULL default ' ',
  "AAABLMLR" varchar(2) NOT NULL default '00',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHWAZ","AAAAEVUN","AAABLMLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVLZ" (
  "AAAAEVMC" varchar(20) NOT NULL default ' ',
  "AAAAEVOC" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEVMC","AAAAEVOC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVME" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAEVOC" varchar(10) NOT NULL default ' ',
  "AAAAEVUN" varchar(10) NOT NULL default ' ',
  "AAABLMLR" varchar(6) NOT NULL default '000000',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABDUBG" varchar(20) NOT NULL default ' ',
  "AAAAEVNI" varchar(200) NOT NULL default ' ',
  "AAAAEVNJ" varchar(200) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAEVOC","AAAAEVUN","AAABLMLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVMS" (
  "AAAAEVOC" varchar(10) NOT NULL default ' ',
  "AAAAEVMU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEVOC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVMW" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEVOC" varchar(10) NOT NULL default ' ',
  "AAABLPSU" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAEVOC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVNC" (
  "AAAAEVOC" varchar(10) NOT NULL default ' ',
  "AAAAEVUN" varchar(10) NOT NULL default ' ',
  "AAABLMLR" varchar(6) NOT NULL default '000000',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABDUBG" varchar(20) NOT NULL default ' ',
  "AAAAEVNI" varchar(200) NOT NULL default ' ',
  "AAAAEVNJ" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEVOC","AAAAEVUN","AAABLMLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVNN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUER" varchar(20) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAAAEVUN" varchar(10) NOT NULL default ' ',
  "AAAAEVOC" varchar(10) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAAAIVII" decimal(15,0) NOT NULL default '0',
  "AAABFOBU" varchar(12) NOT NULL default ' ',
  "AAABEAAI" varchar(1) NOT NULL default ' ',
  "AAABLMLR" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLUER","AAABLZKF","AAAAEVUN","AAAAEVOC","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVOE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(10) NOT NULL default ' ',
  "AAAAQNPB" varchar(4) NOT NULL default ' ',
  "AAAAEVOI" varchar(40) NOT NULL default ' ',
  "AAAAEVOJ" varchar(34) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVOM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(10) NOT NULL default ' ',
  "AAAAEXPL" varchar(10) NOT NULL default '0000000000',
  "AAAAJNNT" varchar(4) NOT NULL default ' ',
  "AAAAEVOR" varchar(40) NOT NULL default ' ',
  "AAAAEVOS" varchar(34) NOT NULL default ' ',
  "AAAAEVOT" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAAAEXPL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVOX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEVPE" VARBINARY(255) NOT NULL,
  "AAAAEVPI" VARBINARY(255) NOT NULL,
  "AAAAEVPB" varchar(2) NOT NULL default ' ',
  "AAAAEVPC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEVPE"(255),"AAAAEVPI"(255)),
  KEY "AAAAEVPG" ("AAABLUXQ","AAAAEVPI"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVPJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAABMBXY" varchar(10) NOT NULL default ' ',
  "AAAAQNPB" varchar(4) NOT NULL default ' ',
  "AAABGVKJ" varchar(8) NOT NULL default '00000000',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  "AAAAEVPQ" varchar(1) NOT NULL default ' ',
  "AAAAEVPR" varchar(10) NOT NULL default ' ',
  "AAAARKEO" varchar(10) NOT NULL default ' ',
  "AAABLILW" varchar(10) NOT NULL default ' ',
  "AAAAEVPU" varchar(1) NOT NULL default ' ',
  "AAABLEBK" decimal(15,0) NOT NULL default '0',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAABLEBM" decimal(15,0) NOT NULL default '0',
  "AAABLEBN" varchar(12) NOT NULL default ' ',
  "AAAAEVPZ" decimal(15,0) NOT NULL default '0',
  "AAAAEVQA" varchar(1) NOT NULL default ' ',
  "AAABIMRZ" varchar(10) NOT NULL default ' ',
  "AAAAEVQC" varchar(1) NOT NULL default ' ',
  "AAAAEVRB" varchar(40) NOT NULL default ' ',
  "AAAAEVQE" varchar(1) NOT NULL default ' ',
  "AAAAEVQF" decimal(15,0) NOT NULL default '0',
  "AAAAEVQG" decimal(15,0) NOT NULL default '0',
  "AAAAEVQH" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABLTVR"(255)),
  KEY "AAAAEVQK" ("AAABLUXQ","AAAAQNPB","AAABLEBM"),
  KEY "AAAAEVQO" ("AAAAEVRB"),
  KEY "AAAAEVQQ" ("AAABLUXQ","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVQT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAABLEBK" decimal(15,0) NOT NULL default '0',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAABLEBM" decimal(15,0) NOT NULL default '0',
  "AAABLEBN" varchar(12) NOT NULL default ' ',
  "AAABIMRZ" varchar(10) NOT NULL default ' ',
  "AAAAEVRB" varchar(40) NOT NULL default ' ',
  "AAABDNYG" VARBINARY(255),
  "AAABLTQF" VARBINARY(255),
  "AAAAEVRE" VARBINARY(255),
  "AAABHRVE" VARBINARY(255),
  "AAAAEVRG" varchar(10) NOT NULL default ' ',
  "AAAAEVRH" varchar(54) NOT NULL default ' ',
  "AAAAEVRI" varchar(40) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  "AAAAEVRK" varchar(1) NOT NULL default ' ',
  "AAAAJOZE" varchar(10) NOT NULL default '0000000000',
  "AAAAEVRM" varchar(10) NOT NULL default '0000000000',
  "AAAAJOZG" varchar(4) NOT NULL default ' ',
  "AAAATKIN" decimal(15,0) NOT NULL default '0',
  "AAAAEVRP" varchar(4) NOT NULL default ' ',
  "AAAAEVRQ" varchar(2) NOT NULL default ' ',
  "AAAAEVRR" varchar(4) NOT NULL default ' ',
  "AAAAEVRS" varchar(1) NOT NULL default ' ',
  "AAAAEVRT" varchar(10) NOT NULL default ' ',
  "AAAAEVRU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLTVR"(255)),
  KEY "AAAAEVRX" ("AAABLUXQ","AAABDNYG"(255)),
  KEY "AAAAEVSA" ("AAABLUXQ","AAAAJOZG"),
  KEY "AAAAEVSD" ("AAABLUXQ","AAABHRVE"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVSG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEWEE" VARBINARY(255) NOT NULL,
  "AAABLTVR" VARBINARY(255),
  "AAAARVID" varchar(1) NOT NULL default ' ',
  "AAABLQJP" varchar(1) NOT NULL default ' ',
  "AAAAEVSM" varchar(1) NOT NULL default ' ',
  "AAAAEVSN" varchar(1) NOT NULL default ' ',
  "AAAAEVSO" varchar(1) NOT NULL default ' ',
  "AAAAEVTK" varchar(8) NOT NULL default ' ',
  "AAAAEVTJ" varchar(32) NOT NULL default ' ',
  "AAABBYDZ" varchar(2) NOT NULL default ' ',
  "AAAAEVSS" varchar(2) NOT NULL default ' ',
  "AAAAEVST" varchar(1) NOT NULL default ' ',
  "AAAAEVSU" VARBINARY(255),
  "AAAAEVSV" varchar(10) NOT NULL default ' ',
  "AAAAEVSW" varchar(10) NOT NULL default ' ',
  "AAABHAEW" varchar(1) NOT NULL default ' ',
  "AAAAEVSY" varchar(1) NOT NULL default ' ',
  "AAAAEVSZ" varchar(1) NOT NULL default ' ',
  "AAAAEVTA" varchar(6) NOT NULL default ' ',
  "AAABKJMK" varchar(1) NOT NULL default ' ',
  "AAABJDQU" varchar(1) NOT NULL default ' ',
  "AAAAEVTD" varchar(1) NOT NULL default ' ',
  "AAAAEVTE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEWEE"(255)),
  KEY "AAAAEVTH" ("AAABLUXQ","AAAAEVTJ","AAAAEVTK"),
  KEY "AAAAEVTL" ("AAABLUXQ","AAABLTVR"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVTO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKQYU" varchar(12) NOT NULL default ' ',
  "AAAAEVUN" varchar(10) NOT NULL default ' ',
  "AAABIMFT" varchar(32) NOT NULL default ' ',
  "AAAAEVUP" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAAEVTV" varchar(6) NOT NULL default '000000',
  "AAAAEVTW" varchar(6) NOT NULL default '000000',
  "AAAAEVTX" varchar(6) NOT NULL default '000000',
  "AAAAEVTY" varchar(6) NOT NULL default '000000',
  "AAAAEVTZ" varchar(1) NOT NULL default ' ',
  "AAAAEVUA" varchar(1) NOT NULL default ' ',
  "AAAAEVUB" varchar(1) NOT NULL default ' ',
  "AAAAEVUC" varchar(1) NOT NULL default ' ',
  "AAAAEVUD" varchar(1) NOT NULL default ' ',
  "AAAAEVUE" varchar(1) NOT NULL default ' ',
  "AAAAEVUF" varchar(1) NOT NULL default ' ',
  "AAAAEVUG" varchar(1) NOT NULL default ' ',
  "AAAAEVUH" varchar(80) NOT NULL default ' ',
  "AAAAEVUI" varchar(80) NOT NULL default ' ',
  "AAAAIVII" decimal(15,0) NOT NULL default '0',
  "AAABFOBU" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKQYU","AAAAEVUN","AAABIMFT","AAAAEVUP","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVUR" (
  "AAABIFRO" varchar(8) NOT NULL default ' ',
  "AAAAVJTQ" varchar(12) NOT NULL default ' ',
  "AAABIYEO" varchar(24) NOT NULL default ' ',
  "AAABIYEN" varchar(15) NOT NULL default ' ',
  "AAABIYJL" varchar(6) NOT NULL default ' ',
  "AAAAGUBZ" varchar(6) NOT NULL default ' ',
  "AAAATMFN" varchar(7) NOT NULL default ' ',
  "AAAAGUCB" varchar(8) NOT NULL default ' ',
  "AAAAGUCC" varchar(30) NOT NULL default ' ',
  "AAABGYOH" varchar(12) NOT NULL default ' ',
  "AAAAGUCE" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABIFRO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVVE" (
  "AAAAEVVM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEVVM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVVH" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEVVM" varchar(10) NOT NULL default ' ',
  "AAAAEVVK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAEVVM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVVN" (
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  "AAABLUVJ" varchar(32) NOT NULL default ' ',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABIISW" varchar(10) NOT NULL default ' ',
  "AAABIYQL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAATQHP","AAABIITH","AAABIITI","AAABLUVJ","AAABLENH","AAABLMNC","AAABIISW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVWD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCFHS" varchar(10) NOT NULL default ' ',
  "AAABLGCM" varchar(15) NOT NULL default ' ',
  "AAAAEVWY" varchar(32) NOT NULL default ' ',
  "AAAAEWCT" varchar(30) NOT NULL default ' ',
  "AAAAEVXA" varchar(2) NOT NULL default '00',
  "AAAAEWIK" int(11) NOT NULL default '0',
  "AAAAEVWL" varchar(6) NOT NULL default '000000',
  "AAAAEVWM" varchar(6) NOT NULL default '000000',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(40) NOT NULL default ' ',
  "AAABMNNA" varchar(40) NOT NULL default ' ',
  "AAAAEWPC" varchar(1) NOT NULL default ' ',
  "AAAAEVWS" varchar(30) NOT NULL default ' ',
  "AAAAEVWT" varchar(30) NOT NULL default ' ',
  "AAABFZRW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCFHS","AAABLGCM","AAAAEVWY","AAAAEWCT","AAAAEVXA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVXB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLGCM" varchar(15) NOT NULL default ' ',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLGCM","AAABMNMH","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVXK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCFHS" varchar(10) NOT NULL default ' ',
  "AAABLGCM" varchar(15) NOT NULL default ' ',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default '000',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(40) NOT NULL default ' ',
  "AAABMNNA" varchar(40) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  "AAAAEWIN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCFHS","AAABLGCM","AAABMNMH","AAABMKXW","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVYD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAEWCU" varchar(30) NOT NULL default ' ',
  "AAAAEVYO" varchar(30) NOT NULL default ' ',
  "AAAAEVYI" varchar(30) NOT NULL default ' ',
  "AAAAEWKX" varchar(6) NOT NULL default '000000',
  "AAAAEWKY" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAAAEWCU","AAAAEVYO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVYP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLUNJ" varchar(32) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAEVZD" varchar(32) NOT NULL default ' ',
  "AAAAEVYU" varchar(32) NOT NULL default ' ',
  "AAAAEVZG" varchar(32) NOT NULL default ' ',
  "AAAAEVYW" varchar(128) NOT NULL default ' ',
  "AAAAEVYX" varchar(128) NOT NULL default ' ',
  "AAABJAEL" varchar(8) NOT NULL default '00000000',
  "AAABLOCX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUXQ","AAABLUNJ","AAABMREL","AAAAEVZD"),
  KEY "AAAAEVZE" ("AAABLUXQ","AAAAEVZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVZH" (
  "AAABLGCM" varchar(15) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAURLT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVZM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAALFZQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVZQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAHLFC" varchar(4) NOT NULL default ' ',
  "AAAAEVZT" varchar(1) NOT NULL default ' ',
  "AAAAEWAC" varchar(4) NOT NULL default ' ',
  "AAABLKXP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAHLFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEVZY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAHLFC" varchar(4) NOT NULL default ' ',
  "AAAAEWSB" varchar(6) NOT NULL default ' ',
  "AAAAEWAC" varchar(4) NOT NULL default ' ',
  "AAABLKXP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAHLFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWAG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEWAL" varchar(10) NOT NULL default ' ',
  "AAAAEWAS" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEWAL"),
  UNIQUE KEY "AAAAEWAM" ("AAABLUXQ","AAAAEWAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWAP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLSOF" varchar(10) NOT NULL default ' ',
  "AAAAEWAS" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLSOF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWAV" (
  "AAABGFKP" varchar(1) NOT NULL default ' ',
  "AAAAFNSL" varchar(30) NOT NULL default ' ',
  "AAAAEWBM" varchar(14) NOT NULL default '00000000000000',
  "AAAAEWQY" varchar(8) NOT NULL default '00000000',
  "AAAAEWBV" varchar(8) NOT NULL default '00000000',
  "AAAARTLU" varchar(1) NOT NULL default ' ',
  "AAAAEWBC" varchar(1) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABGFKP","AAAAFNSL","AAAAEWBM","AAAAEWQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWBO" (
  "AAABGFKP" varchar(1) NOT NULL default ' ',
  "AAAAEWBV" varchar(8) NOT NULL default '00000000',
  "AAAAEWQY" varchar(8) NOT NULL default '00000000',
  "AAABCXXG" varchar(30) NOT NULL default ' ',
  "AAABMKXX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGFKP","AAAAEWBV","AAAAEWQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWBX" (
  "AAABGFKP" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEWCA" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABGFKP","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWCD" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAEWCF" varchar(1) NOT NULL default ' ',
  "AAAAEWCG" varchar(8) NOT NULL default '00000000',
  "AAAAEWCH" varchar(6) NOT NULL default '000000',
  "AAAAEWCI" varchar(30) NOT NULL default ' ',
  "AAAAEWCJ" varchar(30) NOT NULL default ' ',
  "AAAAEWCK" varchar(30) NOT NULL default ' ',
  "AAAAEWCL" varchar(8) NOT NULL default '00000000',
  "AAAAEWCM" varchar(6) NOT NULL default '000000',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWCP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAEWCX" varchar(1) NOT NULL default ' ',
  "AAAAEWCT" varchar(30) NOT NULL default ' ',
  "AAAAEWCU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAAAEWCX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWCY" (
  "AAAAEWDJ" varchar(10) NOT NULL default ' ',
  "AAAAEWDA" varchar(1) NOT NULL default '0',
  "AAAAEWDB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEWDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWDD" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEWDJ" varchar(10) NOT NULL default ' ',
  "AAAAEWDG" varchar(60) NOT NULL default ' ',
  "AAAAEWDH" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAEWDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWDK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEWEE" VARBINARY(255) NOT NULL,
  "AAABCGLE" varchar(1) NOT NULL default ' ',
  "AAAAPOOG" varchar(10) NOT NULL default ' ',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAABLEBN" varchar(12) NOT NULL default ' ',
  "AAABKNAR" decimal(15,0) NOT NULL default '0',
  "AAABKNCM" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAAAEWEE"(255),"AAABCGLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWDW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEWEE" VARBINARY(255) NOT NULL,
  "AAABCGLE" varchar(1) NOT NULL default ' ',
  "AAABJDPL" decimal(15,0) NOT NULL default '0',
  "AAABLARI" decimal(15,0) NOT NULL default '0',
  "AAABFTRT" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEWEE"(255),"AAABCGLE","AAABJDPL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWEH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEWET" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEWET")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWEM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEWET" varchar(10) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAAAEWET")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWEU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEWFG" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEWFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWEZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEWFG" varchar(18) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAAAEWFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWFH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEWFT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEWFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWFM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEWFT" varchar(10) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAAAEWFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWFU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABDOGU" VARBINARY(255) NOT NULL,
  "AAABCGRI" varchar(3) NOT NULL default ' ',
  "AAABCGQN" varchar(3) NOT NULL default ' ',
  "AAABHILD" varchar(4) NOT NULL default ' ',
  "AAABCGRK" varchar(20) NOT NULL default ' ',
  "AAAAEWGV" varchar(16) NOT NULL default ' ',
  "AAAAEWGW" varchar(1) NOT NULL default ' ',
  "AAABLARI" decimal(15,0) NOT NULL default '0',
  "AAABJDPL" decimal(15,0) NOT NULL default '0',
  "AAABEEUX" varchar(12) NOT NULL default ' ',
  "AAABEEUL" varchar(1) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABDOGU"(255),"AAABCGRI","AAABCGQN","AAABHILD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWGN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABDOGU" VARBINARY(255) NOT NULL,
  "AAABJDPL" decimal(15,0) NOT NULL default '0',
  "AAABCGRI" varchar(3) NOT NULL default ' ',
  "AAABCGQN" varchar(3) NOT NULL default ' ',
  "AAABHILD" varchar(4) NOT NULL default ' ',
  "AAABCGRK" varchar(20) NOT NULL default ' ',
  "AAAAEWGV" varchar(16) NOT NULL default ' ',
  "AAAAEWGW" varchar(1) NOT NULL default ' ',
  "AAABLARI" decimal(15,0) NOT NULL default '0',
  "AAABEEUX" varchar(12) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABGQZW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABDOGU"(255),"AAABJDPL","AAABCGRI","AAABCGQN","AAABHILD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWHH" (
  "AAAAEWHS" varchar(10) NOT NULL default ' ',
  "AAAAEWHJ" varchar(30) NOT NULL default ' ',
  "AAAAEWHK" varchar(30) NOT NULL default ' ',
  "AAAAEWHL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEWHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWHN" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAEWHS" varchar(10) NOT NULL default ' ',
  "AAAAEWHQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAEWHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWVV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCFHS" varchar(10) NOT NULL default ' ',
  "AAABLGCM" varchar(15) NOT NULL default ' ',
  "AAAAEWHX" varchar(15) NOT NULL default ' ',
  "AAABDQNS" varchar(1) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  "AAAAEWIN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCFHS","AAABLGCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWIE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCFHS" varchar(10) NOT NULL default ' ',
  "AAABLGCM" varchar(15) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAEWIJ" varchar(30) NOT NULL default ' ',
  "AAAAEWIK" int(11) NOT NULL default '0',
  "AAAAEWIL" varchar(1) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  "AAAAEWIN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCFHS","AAABLGCM","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWIS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEWIY" varchar(10) NOT NULL default '0000000000',
  "AAAAEWJB" varchar(32) NOT NULL default ' ',
  "AAAAEWJF" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEWIY"),
  UNIQUE KEY "AAAAEWIZ" ("AAABLUXQ","AAAAEWJB","AAAAEWJF"),
  KEY "AAAAEWJD" ("AAABLUXQ","AAAAEWJF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWJG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJECS" varchar(64) NOT NULL default ' ',
  "AAAAEWJQ" varchar(32) NOT NULL default ' ',
  "AAAAEWJR" varchar(20) NOT NULL default ' ',
  "AAABCFHS" varchar(10) NOT NULL default ' ',
  "AAAAEWJM" varchar(255) NOT NULL default ' ',
  "AAAAEWJN" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJECS","AAAAEWJQ","AAAAEWJR","AAABCFHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWJT" (
  "AAABMALF" varchar(1) NOT NULL default '0',
  "AAABKCWA" varchar(1) NOT NULL default ' ',
  "AAABAHSI" int(11) NOT NULL default '0',
  "AAABAHSJ" int(11) NOT NULL default '0',
  "AAABAHSK" int(11) NOT NULL default '0',
  "AAABAHSL" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMALF","AAABKCWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWKC" (
  "AAABMALF" varchar(1) NOT NULL default '0',
  "AAABAHSI" int(11) NOT NULL default '0',
  "AAABAHSJ" int(11) NOT NULL default '0',
  "AAABAHSK" int(11) NOT NULL default '0',
  "AAABAHSL" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMALF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWKJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAEWKM" VARBINARY(255),
  "AAABLKIU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWKP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCFHS" varchar(10) NOT NULL default ' ',
  "AAABLGCM" varchar(15) NOT NULL default ' ',
  "AAABLOSB" varchar(10) NOT NULL default ' ',
  "AAABCKMA" varchar(30) NOT NULL default ' ',
  "AAAAEWKV" varchar(7) NOT NULL default ' ',
  "AAAAEWKW" varchar(30) NOT NULL default ' ',
  "AAAAEWKX" varchar(6) NOT NULL default '000000',
  "AAAAEWKY" varchar(6) NOT NULL default '000000',
  "AAAAEWKZ" varchar(30) NOT NULL default ' ',
  "AAAAEWLA" varchar(30) NOT NULL default ' ',
  "AAAAEWLB" varchar(30) NOT NULL default ' ',
  "AAAAEWLC" varchar(6) NOT NULL default '000000',
  "AAABJSDF" varchar(6) NOT NULL default '000000',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCFHS","AAABLGCM","AAABLOSB","AAABCKMA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWLK" (
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAAAEWLR" varchar(30) NOT NULL default ' ',
  "AAAARHPT" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLTVR","AAAAEWLR"),
  UNIQUE KEY "AAAAEWLQ" ("AAAAEWLR","AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWLT" (
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABLEMJ" varchar(10) NOT NULL default ' ',
  "AAABLUSV" varchar(2) NOT NULL default ' ',
  "AAABMKFH" varchar(60) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMMVU" varchar(1) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABKMZH" varchar(1) NOT NULL default ' ',
  "AAAAGGLC" varchar(5) NOT NULL default '00000',
  "AAAATVUE" varchar(1) NOT NULL default ' ',
  "AAAAWJTD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMNC","AAABLEMJ","AAABLUSV","AAABMKFH","AAABMLQA","AAABMMVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWML" (
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABLEMJ" varchar(10) NOT NULL default ' ',
  "AAABLUSV" varchar(2) NOT NULL default ' ',
  "AAABMKFH" varchar(60) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMMVU" varchar(1) NOT NULL default ' ',
  "AAAATVUS" varchar(4) NOT NULL default '0000',
  "AAABMMRH" varchar(6) NOT NULL default '000000',
  "AAABGEJG" varchar(2) NOT NULL default ' ',
  "AAABGEJH" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMNC","AAABLEMJ","AAABLUSV","AAABMKFH","AAABMLQA","AAABMMVU","AAAATVUS","AAABMMRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWNE" (
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABLEMJ" varchar(10) NOT NULL default ' ',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAABMRCK" varchar(73) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMNC","AAABLEMJ","AAABLSCE","AAABLTKC","AAABLXDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWNQ" (
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABLEMJ" varchar(10) NOT NULL default ' ',
  "AAAALOHH" varchar(40) NOT NULL default ' ',
  "AAABLUSV" varchar(1) NOT NULL default ' ',
  "AAABLAUD" varchar(8) NOT NULL default ' ',
  "AAABJZCI" varchar(255) NOT NULL default ' ',
  "AAABJSDF" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLMNC","AAABLEMJ","AAAALOHH","AAABLUSV","AAABLAUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWOD" (
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABLEMJ" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJTAQ" varchar(20) NOT NULL default ' ',
  "AAABGWQE" varchar(4) NOT NULL default ' ',
  "AAABJVWR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMNC","AAABLEMJ","AAABMRHS","AAABJTAQ","AAABGWQE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWOP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCFHS" varchar(10) NOT NULL default ' ',
  "AAABLGCM" varchar(15) NOT NULL default ' ',
  "AAAAEWPH" varchar(30) NOT NULL default ' ',
  "AAAAEWPI" varchar(30) NOT NULL default ' ',
  "AAAAEWPJ" varchar(30) NOT NULL default ' ',
  "AAABHDBW" varchar(30) NOT NULL default ' ',
  "AAABFRJK" varchar(6) NOT NULL default '000000',
  "AAABFRJL" varchar(6) NOT NULL default '000000',
  "AAAAEWOZ" varchar(6) NOT NULL default '000000',
  "AAABBJQN" varchar(6) NOT NULL default '000000',
  "AAABEJLD" varchar(6) NOT NULL default '000000',
  "AAAAEWPC" varchar(1) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCFHS","AAABLGCM","AAAAEWPH","AAAAEWPI","AAAAEWPJ","AAABHDBW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWPL" (
  "AAABHUPJ" varchar(32) NOT NULL default ' ',
  "AAABKNBT" varchar(8) NOT NULL default ' ',
  "AAAAEWPO" varchar(8) NOT NULL default ' ',
  "AAABLUFA" varchar(1) NOT NULL default ' ',
  "AAABGPVW" varchar(1) NOT NULL default ' ',
  "AAAAEWPR" varchar(14) NOT NULL default '00000000000000',
  "AAABIHMM" varchar(1) NOT NULL default ' ',
  "AAAAEWPT" varchar(14) NOT NULL default '00000000000000',
  "AAAAEWPU" varchar(14) NOT NULL default '00000000000000',
  "AAAAEXVL" varchar(14) NOT NULL default '00000000000000',
  "AAABCRXE" varchar(2) NOT NULL default '00',
  "AAABCRXD" varchar(3) NOT NULL default '000',
  "AAABCRXC" varchar(2) NOT NULL default '00',
  "AAABCRXB" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABHUPJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWQB" (
  "AAABHUPJ" varchar(32) NOT NULL default ' ',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAAAEWQY" varchar(8) NOT NULL default '00000000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAAEWQG" varchar(60) NOT NULL default ' ',
  "AAAAEWQH" varchar(60) NOT NULL default ' ',
  "AAAAEWQI" varchar(1) NOT NULL default ' ',
  "AAAAEWQJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHUPJ","AAABMNMH","AAAAEWQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWQN" (
  "AAABHUPJ" varchar(32) NOT NULL default ' ',
  "AAAAEWQY" varchar(8) NOT NULL default '00000000',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAABMNMV" varchar(3) NOT NULL default ' ',
  "AAABMNMW" int(11) NOT NULL default '0',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLQEJ" varchar(2) NOT NULL default ' ',
  "AAAAUNSO" varchar(60) NOT NULL default ' ',
  "AAAAUNSP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABHUPJ","AAAAEWQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWQZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCFHS" varchar(10) NOT NULL default ' ',
  "AAABLGCM" varchar(15) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAAAUYQA" varchar(1) NOT NULL default ' ',
  "AAAAEWRF" varchar(24) NOT NULL default ' ',
  "AAAAEWRG" varchar(24) NOT NULL default ' ',
  "AAAAEWRH" varchar(30) NOT NULL default ' ',
  "AAABHMNR" varchar(30) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  "AAAAEWRK" varchar(1) NOT NULL default ' ',
  "AAAAEWRL" varchar(1) NOT NULL default ' ',
  "AAAAEWRM" varchar(1) NOT NULL default ' ',
  "AAAAEWRN" varchar(1) NOT NULL default ' ',
  "AAAAEWRO" varchar(10) NOT NULL default ' ',
  "AAAAEWRP" varchar(8) NOT NULL default '00000000',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABIJJL" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABCFHS","AAABLGCM","AAABMODI","AAAAUYQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWRY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEWSE" varchar(3) NOT NULL default '000',
  "AAAAEWSB" varchar(6) NOT NULL default ' ',
  "AAAAHLFC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEWSE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWSF" (
  "AAABFVDJ" varchar(30) NOT NULL default ' ',
  "AAAAEWTD" varchar(10) NOT NULL default ' ',
  "AAAATQHO" varchar(1) NOT NULL default ' ',
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(48) NOT NULL default ' ',
  "AAAAUBIJ" varchar(10) NOT NULL default ' ',
  "AAAAEWTK" varchar(4) NOT NULL default ' ',
  "AAAAEWTL" varchar(4) NOT NULL default ' ',
  "AAAAEWTM" varchar(48) NOT NULL default ' ',
  "AAAAEWTN" varchar(3) NOT NULL default '000',
  "AAAAQXER" varchar(33) NOT NULL default ' ',
  "AAAAEWST" varchar(1) NOT NULL default '0',
  "AAAAEWSU" varchar(10) NOT NULL default ' ',
  "AAAAEWSV" varchar(10) NOT NULL default '0000000000',
  "AAAAEWSW" varchar(4) NOT NULL default '0000',
  "AAAAEWSX" varchar(10) NOT NULL default ' ',
  "AAAAEWSY" varchar(10) NOT NULL default ' ',
  "AAAAEWSZ" varchar(4) NOT NULL default '0000',
  "AAAAEWTA" varchar(3) NOT NULL default '000',
  "AAAAEWTB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFVDJ","AAAAEWTD","AAAATQHO","AAAATQHP","AAABLRST","AAABMKFH","AAABLFYD","AAAAUBIJ","AAAAEWTK","AAAAEWTL","AAAAEWTM","AAAAEWTN","AAAAQXER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWTP" (
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAABGXDL" varchar(1) NOT NULL default ' ',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  "AAABLEEW" varchar(30) NOT NULL default ' ',
  "AAABMKXF" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABILWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWTY" (
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(48) NOT NULL default ' ',
  "AAAAEWUD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABILWI","AAABLRST","AAABMKFH","AAABLFYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWUI" (
  "AAAAEWUQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEWUQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWUL" (
  "AAAAEWUQ" varchar(2) NOT NULL default ' ',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABFPQD" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAAAEWUQ","AAABMNMH","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWUT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCFHS" varchar(10) NOT NULL default ' ',
  "AAABLGCM" varchar(15) NOT NULL default ' ',
  "AAAAEWVG" varchar(1) NOT NULL default ' ',
  "AAABGGIZ" varchar(15) NOT NULL default ' ',
  "AAABLJJO" varchar(3) NOT NULL default ' ',
  "AAABMIRY" varchar(4) NOT NULL default ' ',
  "AAAAURLT" varchar(30) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCFHS","AAABLGCM","AAAAEWVG","AAABGGIZ","AAABLJJO","AAABMIRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWVK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEWVR" varchar(10) NOT NULL default ' ',
  "AAAAEWVS" varchar(4) NOT NULL default ' ',
  "AAAAEWVV" varchar(10) NOT NULL default ' ',
  "AAAAEWVW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEWVR","AAAAEWVS"),
  KEY "AAAAEWVT" ("AAABLUXQ","AAAAEWVV","AAAAEWVW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWVX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJJLO" varchar(22) NOT NULL default ' ',
  "AAABJJLP" varchar(4) NOT NULL default '0000',
  "AAABJJLF" varchar(4) NOT NULL default ' ',
  "AAABJJLG" varchar(10) NOT NULL default ' ',
  "AAABJJLX" varchar(10) NOT NULL default ' ',
  "AAABJJLY" varchar(70) NOT NULL default ' ',
  "AAABJJLZ" varchar(10) NOT NULL default ' ',
  "AAABMIRY" varchar(10) NOT NULL default ' ',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUXQ","AAABJJLO","AAABJJLP"),
  KEY "AAAAEWWM" ("AAABLUXQ","AAABMIRY","AAABMQIL","AAABMLTD"),
  KEY "AAAAEWWR" ("AAABLUXQ","AAABJJLX","AAABJJLY","AAABJJLZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWWW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAAAQNPB" varchar(4) NOT NULL default ' ',
  "AAABEOJJ" varchar(6) NOT NULL default ' ',
  "AAAAEWXB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHC","AAAAQNPB","AAABEOJJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWXG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABEOJJ" varchar(6) NOT NULL default ' ',
  "AAABEPCO" varchar(4) NOT NULL default ' ',
  "AAAALNVB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHC","AAABEOJJ","AAABEPCO","AAAALNVB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWXR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABEOJJ" varchar(6) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHC","AAABEOJJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWXZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAAAEWYG" varchar(1) NOT NULL default ' ',
  "AAABCCBQ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHC","AAAAEWYG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWYH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJCYE" VARBINARY(255) NOT NULL,
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAEXAP" varchar(32) NOT NULL default ' ',
  "AAAAEWZE" varchar(32) NOT NULL default ' ',
  "AAAAPVET" VARBINARY(255),
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAQNPB" varchar(4) NOT NULL default ' ',
  "AAAAEXAM" varchar(10) NOT NULL default ' ',
  "AAAAEXPL" varchar(10) NOT NULL default ' ',
  "AAABIMRZ" varchar(10) NOT NULL default ' ',
  "AAAAHJPF" varchar(8) NOT NULL default '00000000',
  "AAAAHJPG" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLUXQ","AAABJCYE"(255)),
  KEY "AAAAEWYY" ("AAABLUXQ","AAAAPVET"(255)),
  KEY "AAAAEWZB" ("AAABEZHC","AAAAEXAP","AAAAEWZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEWZF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJCYE" VARBINARY(255) NOT NULL,
  "AAAAEWZX" varchar(4) NOT NULL default ' ',
  "AAABEOJJ" varchar(6) NOT NULL default ' ',
  "AAABEPCO" varchar(4) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABMKKY" varchar(8) NOT NULL default '00000000',
  "AAABKGMO" decimal(15,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABMKAH" decimal(15,3) NOT NULL default '0.000',
  "AAABMKAI" varchar(3) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAEXAM" varchar(10) NOT NULL default ' ',
  "AAAAEXPL" varchar(10) NOT NULL default ' ',
  "AAABIMRZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJCYE"(255),"AAAAEWZX","AAABEOJJ","AAABEPCO","AAABMKFU","AAABMKKY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXAC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAQNPB" varchar(4) NOT NULL default ' ',
  "AAAAEXAM" varchar(10) NOT NULL default ' ',
  "AAAAEXPL" varchar(10) NOT NULL default ' ',
  "AAAAEXAP" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQDU","AAAAQNPB","AAAAEXAM","AAAAEXPL"),
  KEY "AAAAEXAO" ("AAAAEXAP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXAQ" (
  "AAAAQXER" varchar(8) NOT NULL default '00000000',
  "AAAAEXDY" varchar(8) NOT NULL default ' ',
  "AAABGGDB" varchar(30) NOT NULL default ' ',
  "AAABLOLZ" varchar(12) NOT NULL default ' ',
  "AAABFUGJ" varchar(8) NOT NULL default '00000000',
  "AAAAEXAW" varchar(6) NOT NULL default '000000',
  "AAABHFFQ" varchar(12) NOT NULL default ' ',
  "AAAAEXAY" varchar(3) NOT NULL default ' ',
  "AAABMMKX" varchar(3) NOT NULL default '000',
  "AAAAWDOE" varchar(1) NOT NULL default ' ',
  "AAAAGKFR" varchar(4) NOT NULL default '0000',
  "AAAAGKFT" varchar(4) NOT NULL default '0000',
  "AAAAGKFU" varchar(4) NOT NULL default '0000',
  "AAAAGKFV" varchar(4) NOT NULL default '0000',
  "AAAAEXBF" varchar(1) NOT NULL default ' ',
  "AAABMBXY" varchar(50) NOT NULL default ' ',
  "AAABLTWP" varchar(20) NOT NULL default ' ',
  "AAABIHTL" int(11) NOT NULL default '0',
  "AAABJYTM" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAAAQXER","AAAAEXDY","AAABGGDB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXBN" (
  "AAAAQXER" varchar(8) NOT NULL default '00000000',
  "AAAAEXDY" varchar(8) NOT NULL default ' ',
  "AAAAEXDZ" varchar(4) NOT NULL default '0000',
  "AAABGGDB" varchar(30) NOT NULL default ' ',
  "AAAAYWIV" varchar(4) NOT NULL default '0000',
  "AAABKSOV" varchar(6) NOT NULL default '000000',
  "AAABLQCR" varchar(30) NOT NULL default ' ',
  "AAAAEXCT" varchar(3) NOT NULL default ' ',
  "AAABMLBW" varchar(30) NOT NULL default ' ',
  "AAAAEXBX" varchar(20) NOT NULL default ' ',
  "AAAAEXBY" varchar(3) NOT NULL default '000',
  "AAAAEXBZ" varchar(20) NOT NULL default ' ',
  "AAAAEXCA" varchar(3) NOT NULL default '000',
  "AAAAEXCB" varchar(20) NOT NULL default ' ',
  "AAAAEXCC" varchar(3) NOT NULL default '000',
  "AAAAEXCD" varchar(50) NOT NULL default ' ',
  "AAAAEXCE" varchar(253) NOT NULL default ' ',
  "AAAAJVAY" varchar(8) NOT NULL default ' ',
  "AAABMRCU" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAAAQXER","AAAAEXDY","AAAAEXDZ"),
  KEY "AAAAEXCK" ("AAABGGDB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXCM" (
  "AAAAQXER" varchar(8) NOT NULL default '00000000',
  "AAAAEXDY" varchar(8) NOT NULL default ' ',
  "AAAAEXDZ" varchar(4) NOT NULL default '0000',
  "AAAAEXEA" varchar(4) NOT NULL default '0000',
  "AAAAEXEB" varchar(5) NOT NULL default '00000',
  "AAABMRFW" varchar(30) NOT NULL default ' ',
  "AAAAEXCT" varchar(3) NOT NULL default ' ',
  "AAABMRCU" varchar(5) NOT NULL default '00000',
  "AAABJLKW" varchar(2) NOT NULL default '00',
  "AAABLQCR" varchar(30) NOT NULL default ' ',
  "AAAAEXCX" varchar(4) NOT NULL default '0000',
  "AAAANRUW" varchar(1) NOT NULL default ' ',
  "AAAAEXCZ" varchar(3) NOT NULL default ' ',
  "AAABHXLS" varchar(30) NOT NULL default ' ',
  "AAAAEXDB" varchar(1) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABGIKF" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default '0000',
  "AAAAEXDF" varchar(30) NOT NULL default ' ',
  "AAAAEXDG" varchar(4) NOT NULL default ' ',
  "AAAAEXDH" varchar(4) NOT NULL default ' ',
  "AAABBVTW" varchar(3) NOT NULL default '000',
  "AAAAYWIV" varchar(4) NOT NULL default '0000',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default '000',
  "AAAAEXDM" varchar(3) NOT NULL default ' ',
  "AAAAEXDN" varchar(30) NOT NULL default ' ',
  "AAAAEXDO" varchar(1) NOT NULL default '0',
  "AAAAEXDP" varchar(3) NOT NULL default '000',
  "AAAAEXDQ" varchar(1) NOT NULL default '0',
  "AAAAEXDR" varchar(3) NOT NULL default '000',
  "AAAAEXDS" varchar(253) NOT NULL default ' ',
  "AAAAEXDT" varchar(253) NOT NULL default ' ',
  "AAAAEXDU" varchar(253) NOT NULL default ' ',
  "AAAAEXDV" varchar(253) NOT NULL default ' ',
  "AAAAEXDW" varchar(253) NOT NULL default ' ',
  PRIMARY KEY ("AAAAQXER","AAAAEXDY","AAAAEXDZ","AAAAEXEA","AAAAEXEB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXEC" (
  "AAAATQHO" varchar(1) NOT NULL default ' ',
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAAAEXMU" varchar(32) NOT NULL default ' ',
  "AAAATQGJ" varchar(8) NOT NULL default ' ',
  "AAAAEXGD" varchar(4) NOT NULL default '0000',
  "AAAAGLRR" varchar(10) NOT NULL default '0000000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAATQFT" varchar(4) NOT NULL default ' ',
  "AAAAEXVL" varchar(14) NOT NULL default '00000000000000',
  "AAAATQGQ" varchar(40) NOT NULL default ' ',
  "AAAAJVAY" varchar(40) NOT NULL default ' ',
  "AAABGBXR" varchar(100) NOT NULL default ' ',
  "AAAAQXER" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAATQHO","AAAATQHP","AAAAEXMU","AAAATQGJ","AAAAEXGD","AAAAGLRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXEW" (
  "AAAATQHO" varchar(1) NOT NULL default ' ',
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAAAEXMU" varchar(32) NOT NULL default ' ',
  "AAAATQGJ" varchar(8) NOT NULL default ' ',
  "AAAAQXER" varchar(8) NOT NULL default '00000000',
  "AAAAEXVL" varchar(14) NOT NULL default '00000000000000',
  "AAAAROPT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATQHO","AAAATQHP","AAAAEXMU","AAAATQGJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXFI" (
  "AAAATQHO" varchar(1) NOT NULL default ' ',
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAAAEXMU" varchar(32) NOT NULL default ' ',
  "AAAATQGJ" varchar(8) NOT NULL default ' ',
  "AAAAEXGD" varchar(4) NOT NULL default '0000',
  "AAAAGLRR" varchar(10) NOT NULL default '0000000000',
  "AAAAEXIY" varchar(4) NOT NULL default '0000',
  "AAABLXDP" varchar(1) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  "AAABJVWR" varchar(255) NOT NULL default ' ',
  "AAAAQXER" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAATQHO","AAAATQHP","AAAAEXMU","AAAATQGJ","AAAAEXGD","AAAAGLRR","AAAAEXIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXGG" (
  "AAABGGIZ" varchar(15) NOT NULL default ' ',
  "AAABLGCM" varchar(15) NOT NULL default ' ',
  "AAAAEXGU" varchar(1) NOT NULL default ' ',
  "AAAAEXGK" varchar(30) NOT NULL default ' ',
  "AAAAEXGL" varchar(30) NOT NULL default ' ',
  "AAAAEXGM" varchar(30) NOT NULL default ' ',
  "AAAAEXGN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGGIZ","AAABLGCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXGQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGGIZ" varchar(15) NOT NULL default ' ',
  "AAABLGCM" varchar(15) NOT NULL default ' ',
  "AAAAEXGU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGGIZ","AAABLGCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXGY" (
  "AAAATQHO" varchar(1) NOT NULL default ' ',
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAAATQGJ" varchar(10) NOT NULL default '0000000000',
  "AAABHEDX" varchar(8) NOT NULL default '00000000',
  "AAAAGLRR" varchar(10) NOT NULL default '0000000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAATQFT" varchar(4) NOT NULL default ' ',
  "AAAAEXVL" varchar(14) NOT NULL default '00000000000000',
  "AAAATQGQ" varchar(40) NOT NULL default ' ',
  "AAAAJVAY" varchar(40) NOT NULL default ' ',
  "AAAAEXMU" varchar(32) NOT NULL default ' ',
  "AAABGBXR" varchar(100) NOT NULL default ' ',
  "AAAAQXER" varchar(8) NOT NULL default '00000000',
  "AAAAEXHM" longtext,
  "AAABHWQS" longtext,
  PRIMARY KEY ("AAAATQHO","AAAATQHP","AAAATQGJ","AAABHEDX","AAAAGLRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXHT" (
  "AAAATQHO" varchar(1) NOT NULL default ' ',
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAAATQGJ" varchar(10) NOT NULL default '0000000000',
  "AAAAQXER" varchar(8) NOT NULL default '00000000',
  "AAAAEXVL" varchar(14) NOT NULL default '00000000000000',
  "AAAAROPT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATQHO","AAAATQHP","AAAATQGJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXID" (
  "AAAATQHO" varchar(1) NOT NULL default ' ',
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAAATQGJ" varchar(10) NOT NULL default '0000000000',
  "AAABHEDX" varchar(8) NOT NULL default '00000000',
  "AAAAGLRR" varchar(10) NOT NULL default '0000000000',
  "AAAAEXIY" varchar(4) NOT NULL default '0000',
  "AAABLXDP" varchar(1) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  "AAABJVWR" varchar(255) NOT NULL default ' ',
  "AAAAQXER" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAATQHO","AAAATQHP","AAAATQGJ","AAABHEDX","AAAAGLRR","AAAAEXIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXIZ" (
  "AAAAQXER" varchar(8) NOT NULL default '00000000',
  "AAABIGIZ" varchar(10) NOT NULL default '0000000000',
  "AAABHDYV" varchar(8) NOT NULL default '00000000',
  "AAAAGKKT" varchar(10) NOT NULL default '0000000000',
  "AAAAGOER" varchar(1) NOT NULL default ' ',
  "AAAAGMAV" varchar(8) NOT NULL default '00000000',
  "AAAAGKKP" varchar(10) NOT NULL default '0000000000',
  "AAAAGMAS" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAAQXER","AAABIGIZ","AAABHDYV","AAAAGKKT","AAAAGOER","AAAAGMAV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXJO" (
  "AAAAQXER" varchar(8) NOT NULL default '00000000',
  "AAABIGIZ" varchar(10) NOT NULL default '0000000000',
  "AAABCEDB" varchar(12) NOT NULL default ' ',
  "AAABLRTV" varchar(8) NOT NULL default '00000000',
  "AAABLRTW" varchar(6) NOT NULL default '000000',
  "AAABLEQN" varchar(8) NOT NULL default '00000000',
  "AAABLEQO" varchar(6) NOT NULL default '000000',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAAAGKGN" varchar(8) NOT NULL default '00000000',
  "AAAAGKFR" varchar(5) NOT NULL default '00000',
  "AAABBLFK" varchar(5) NOT NULL default '00000',
  "AAAAGKFT" varchar(5) NOT NULL default '00000',
  "AAAAGKFU" varchar(5) NOT NULL default '00000',
  "AAAAGKFV" varchar(5) NOT NULL default '00000',
  "AAABFIGO" varchar(1) NOT NULL default ' ',
  "AAAAGKGO" varchar(75) NOT NULL default ' ',
  "AAABHMNR" varchar(100) NOT NULL default ' ',
  "AAAAGLPS" varchar(1) NOT NULL default ' ',
  "AAAAGLPT" varchar(1) NOT NULL default ' ',
  "AAAAGLPU" varchar(1) NOT NULL default ' ',
  "AAAAGLPV" varchar(1) NOT NULL default ' ',
  "AAAAGLPW" varchar(1) NOT NULL default ' ',
  "AAAAQZRW" varchar(8) NOT NULL default '00000000',
  "AAAAGLPY" varchar(1) NOT NULL default ' ',
  "AAAAGLPZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAQXER","AAABIGIZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXKQ" (
  "AAAAQXER" varchar(8) NOT NULL default '00000000',
  "AAABIGIZ" varchar(10) NOT NULL default '0000000000',
  "AAABHDYV" varchar(8) NOT NULL default '00000000',
  "AAAAGKKT" varchar(10) NOT NULL default '0000000000',
  "AAAAGKHL" varchar(4) NOT NULL default '0000',
  "AAABBRJH" varchar(1) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAAALOKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAAQXER","AAABIGIZ","AAABHDYV","AAAAGKKT","AAAAGKHL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXLG" (
  "AAAAQXER" varchar(8) NOT NULL default '00000000',
  "AAABIGIZ" varchar(10) NOT NULL default '0000000000',
  "AAABHDYV" varchar(8) NOT NULL default '00000000',
  "AAAAGKKT" varchar(10) NOT NULL default '0000000000',
  "AAAAGKIJ" varchar(10) NOT NULL default '0000000000',
  "AAAAGKHR" varchar(1) NOT NULL default ' ',
  "AAAAGKHS" varchar(4) NOT NULL default '0000',
  "AAAAGKHT" varchar(1) NOT NULL default ' ',
  "AAAAGKHU" varchar(1) NOT NULL default ' ',
  "AAAAGLRG" varchar(4) NOT NULL default '0000',
  "AAABJTCX" varchar(30) NOT NULL default ' ',
  "AAABMKFH" varchar(30) NOT NULL default ' ',
  "AAABINZS" varchar(30) NOT NULL default ' ',
  "AAAAGLRK" varchar(30) NOT NULL default ' ',
  "AAABLQHQ" varchar(30) NOT NULL default ' ',
  "AAABHCQK" varchar(32) NOT NULL default ' ',
  "AAAAGLRN" varchar(8) NOT NULL default '00000000',
  "AAAAGLRO" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAAQXER","AAABIGIZ","AAABHDYV","AAAAGKKT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXMD" (
  "AAAAQXER" varchar(8) NOT NULL default '00000000',
  "AAABIGIZ" varchar(10) NOT NULL default '0000000000',
  "AAABHDYV" varchar(8) NOT NULL default '00000000',
  "AAAAGKKT" varchar(10) NOT NULL default '0000000000',
  "AAAAGKKU" varchar(2) NOT NULL default '00',
  "AAAAGKKP" varchar(10) NOT NULL default '0000000000',
  "AAAAGKKQ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAAQXER","AAABIGIZ","AAABHDYV","AAAAGKKT","AAAAGKKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXMQ" (
  "AAAAQXER" varchar(8) NOT NULL default '00000000',
  "AAABCEDB" varchar(12) NOT NULL default ' ',
  "AAAAEXVL" varchar(14) NOT NULL default '00000000000000',
  "AAAAEXMU" varchar(32) NOT NULL default ' ',
  "AAABGBXR" varchar(100) NOT NULL default ' ',
  "AAAAQWBJ" varchar(2) NOT NULL default ' ',
  "AAABGBXY" varchar(3) NOT NULL default '000',
  "AAABLIVG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAQXER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXNA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" VARBINARY(255) NOT NULL,
  "AAABKQEY" varchar(3) NOT NULL default '000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABKUQR" varchar(8) NOT NULL default '00000000',
  "AAABLWWO" varchar(6) NOT NULL default '000000',
  "AAAAKODK" varchar(20) NOT NULL default ' ',
  "AAAAKOCW" varchar(8) NOT NULL default ' ',
  "AAAAKOCX" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG"(255),"AAABKQEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXNN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" VARBINARY(255) NOT NULL,
  "AAABKQEQ" varchar(5) NOT NULL default ' ',
  "AAABKQEY" varchar(3) NOT NULL default '000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABKUQR" varchar(8) NOT NULL default '00000000',
  "AAABLWWO" varchar(6) NOT NULL default '000000',
  "AAAAKODK" varchar(20) NOT NULL default ' ',
  "AAABKRCX" varchar(1) NOT NULL default ' ',
  "AAABHESQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG"(255),"AAABKQEQ","AAABKQEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXOC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" VARBINARY(255) NOT NULL,
  "AAABKQEQ" varchar(5) NOT NULL default ' ',
  "AAABKRCX" varchar(1) NOT NULL default ' ',
  "AAABKQEY" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABLXTG"(255),"AAABKQEQ"),
  KEY "AAAAEXOL" ("AAABMRIL","AAABKQEQ","AAABKRCX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXOP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" VARBINARY(255) NOT NULL,
  "AAABLCGT" varchar(3) NOT NULL default ' ',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABKQEX" varchar(1) NOT NULL default ' ',
  "AAABKQEY" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABLXTG"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXOY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" VARBINARY(255) NOT NULL,
  PRIMARY KEY ("AAABMRIL","AAABLXTG"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXPD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLTZD" varchar(34) NOT NULL default ' ',
  "AAAAEXPO" varchar(10) NOT NULL default ' ',
  "AAAAMSTY" varchar(34) NOT NULL default ' ',
  "AAAAEXPQ" varchar(10) NOT NULL default ' ',
  "AAAAEXPJ" varchar(10) NOT NULL default ' ',
  "AAABIZBU" varchar(10) NOT NULL default ' ',
  "AAAAEXPL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLTZD","AAAAEXPO","AAAAMSTY","AAAAEXPQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXPR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEXQE" varchar(34) NOT NULL default ' ',
  "AAAAEXPZ" varchar(34) NOT NULL default ' ',
  "AAAAEXQA" varchar(10) NOT NULL default '0000000000',
  "AAAAEXQB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLUXQ","AAAAEXQE","AAAAEXPZ","AAAAEXQA","AAAAEXQB"),
  KEY "AAAAEXQC" ("AAABLUXQ","AAAAEXQE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXQF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAQNPB" varchar(4) NOT NULL default ' ',
  "AAAAJNNT" varchar(4) NOT NULL default ' ',
  "AAAAJOZD" varchar(14) NOT NULL default ' ',
  "AAAAJOZC" varchar(14) NOT NULL default ' ',
  "AAAAEXQL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAQNPB","AAAAJNNT","AAAAJOZD","AAAAJOZC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXQR" (
  "AAAAEXRF" varchar(30) NOT NULL default ' ',
  "AAABMDXM" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEXRF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXQV" (
  "AAABLEUE" varchar(20) NOT NULL default ' ',
  "AAAAEXRF" varchar(30) NOT NULL default ' ',
  "AAABLEMJ" varchar(10) NOT NULL default ' ',
  "AAABMDXM" varchar(50) NOT NULL default ' ',
  "AAABILFO" varchar(1) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLEUE","AAAAEXRF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXRG" (
  "AAABHRVE" varchar(30) NOT NULL default ' ',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABLEMJ" varchar(10) NOT NULL default ' ',
  "AAAAEXRK" varchar(10) NOT NULL default ' ',
  "AAAAEXRL" varchar(8) NOT NULL default '00000000',
  "AAABMDXM" varchar(50) NOT NULL default ' ',
  "AAABILFO" varchar(1) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABHRVE","AAABLMNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXRT" (
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXRW" (
  "AAABLEUE" varchar(20) NOT NULL default ' ',
  "AAAAEXUY" varchar(8) NOT NULL default ' ',
  "AAABLUVJ" varchar(32) NOT NULL default ' ',
  "AAAAIXXH" varchar(10) NOT NULL default ' ',
  "AAAAEXSB" varchar(15) NOT NULL default ' ',
  "AAAAEXSC" varchar(3) NOT NULL default '000',
  "AAABMDXM" varchar(50) NOT NULL default ' ',
  "AAABILFO" varchar(1) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLEUE","AAAAEXUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXSK" (
  "AAABLEUE" varchar(20) NOT NULL default ' ',
  "AAABLUVJ" varchar(32) NOT NULL default ' ',
  "AAAAEXSN" varchar(10) NOT NULL default ' ',
  "AAAAEXSO" varchar(15) NOT NULL default ' ',
  "AAAAEXSP" varchar(3) NOT NULL default '000',
  "AAABMDXM" varchar(50) NOT NULL default ' ',
  "AAABILFO" varchar(1) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLEUE","AAABLUVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXSX" (
  "AAABLEUE" varchar(20) NOT NULL default ' ',
  "AAABHFAP" varchar(20) NOT NULL default ' ',
  "AAABLETY" varchar(8) NOT NULL default ' ',
  "AAABLUVJ" varchar(32) NOT NULL default ' ',
  "AAABIJEW" varchar(2) NOT NULL default '00',
  "AAAAEXTD" varchar(4) NOT NULL default ' ',
  "AAAAEXTE" varchar(8) NOT NULL default '00000000',
  "AAAAEXTF" smallint(6) NOT NULL default '0',
  "AAAAEXTG" varchar(3) NOT NULL default '000',
  "AAAAEXTH" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(50) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABILFO" varchar(1) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLEUE","AAABHFAP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXTQ" (
  "AAAAEXTW" varchar(30) NOT NULL default ' ',
  "AAABMDXM" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEXTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXTU" (
  "AAABHRVE" varchar(30) NOT NULL default ' ',
  "AAAAEXTW" varchar(30) NOT NULL default ' ',
  "AAABMDXM" varchar(50) NOT NULL default ' ',
  "AAABILFO" varchar(1) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABHRVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXUD" (
  "AAABLEUE" varchar(20) NOT NULL default ' ',
  "AAABHRVE" varchar(30) NOT NULL default ' ',
  "AAABILFO" varchar(1) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLEUE","AAABHRVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXUM" (
  "AAABLEUE" varchar(20) NOT NULL default ' ',
  "AAABMDXM" varchar(50) NOT NULL default ' ',
  "AAABILFO" varchar(1) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLEUE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXUU" (
  "AAABLETY" varchar(8) NOT NULL default ' ',
  "AAABLEUE" varchar(20) NOT NULL default ' ',
  "AAABHRVE" varchar(30) NOT NULL default ' ',
  "AAAAEXUY" varchar(8) NOT NULL default ' ',
  "AAABMDXM" varchar(50) NOT NULL default ' ',
  "AAABILFO" varchar(1) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLETY","AAABLEUE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXVG" (
  "AAAATQHO" varchar(1) NOT NULL default ' ',
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAABHMRN" varchar(22) NOT NULL default ' ',
  "AAAAQXER" varchar(8) NOT NULL default '00000000',
  "AAAAEXVL" varchar(14) NOT NULL default '00000000000000',
  "AAAAROPT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATQHO","AAAATQHP","AAABHMRN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAANZQT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQPP" varchar(4) NOT NULL default ' ',
  "AAABJLRA" varchar(25) NOT NULL default ' ',
  "AAABJLQR" varchar(40) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABLKKE" varchar(2) NOT NULL default ' ',
  "AAAAEXVY" varchar(1) NOT NULL default ' ',
  "AAAAEXVZ" varchar(8) NOT NULL default '00000000',
  "AAAAEXWA" varchar(1) NOT NULL default ' ',
  "AAAAEXWB" varchar(1) NOT NULL default ' ',
  "AAABDTFR" varchar(20) NOT NULL default ' ',
  "AAAAEXWD" varchar(8) NOT NULL default '00000000',
  "AAAAEXWE" varchar(12) NOT NULL default ' ',
  "AAAAEXWF" decimal(15,2) NOT NULL default '0.00',
  "AAAAEXWG" decimal(15,2) NOT NULL default '0.00',
  "AAAAEXWH" varchar(1) NOT NULL default ' ',
  "AAAAEXWI" decimal(15,2) NOT NULL default '0.00',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(12) NOT NULL default ' ',
  "AAAAEXWL" varchar(2) NOT NULL default ' ',
  "AAAAEXWM" varchar(12) NOT NULL default ' ',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAAAEXWO" varchar(1) NOT NULL default ' ',
  "AAAAEXWP" varchar(1) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAEXWR" varchar(10) NOT NULL default ' ',
  "AAAAKZTQ" varchar(2) NOT NULL default ' ',
  "AAAAEXWT" varchar(2) NOT NULL default ' ',
  "AAAAKZTR" varchar(15) NOT NULL default ' ',
  "AAAAEXWV" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCNV" varchar(5) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  "AAABMCZN" varchar(10) NOT NULL default ' ',
  "AAABMCZO" varchar(3) NOT NULL default '000',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABLXSX" varchar(8) NOT NULL default '00000000',
  "AAABMCXJ" varchar(12) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQPP","AAABJLRA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXXR" (
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABLJIM" varchar(40) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM","AAABMLPV","AAABMNYT","AAABLJIM","AAABLVCU"),
  KEY "AAAAEXYC" ("AAABLJIM","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKJIF" varchar(4) NOT NULL default ' ',
  "AAAAEXYI" varchar(1) NOT NULL default ' ',
  "AAABETQU" varchar(1) NOT NULL default ' ',
  "AAAAEXYK" varchar(1) NOT NULL default ' ',
  "AAAAEXYL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKJIF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXYO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKJIF" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABARAV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKJIF","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXYW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCIM" varchar(10) NOT NULL default ' ',
  "AAABMCME" varchar(4) NOT NULL default '0000',
  "AAABMCMG" varchar(4) NOT NULL default '0000',
  "AAAAMOQF" decimal(13,3) NOT NULL default '0.000',
  "AAAAMOQG" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCIM","AAABMCME","AAABMCMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXZH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAEXZM" varchar(10) NOT NULL default '0000000000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAEXZM","AAABMPLT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXZO" (
  "AAAAEXZW" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEXZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXZR" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAEXZW" varchar(3) NOT NULL default ' ',
  "AAAAEXZU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAAEXZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEXZX" (
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAAAEXZZ" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYAB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNLE" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAEYAG" varchar(8) NOT NULL default '00000000',
  "AAAAEYAH" varchar(12) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  "AAAAEYAJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNLE","AAABLUNJ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYAO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABLEST" varchar(3) NOT NULL default ' ',
  "AAABMELA" varchar(1) NOT NULL default ' ',
  "AAAAEYAT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABLEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYAX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABLEST" varchar(3) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMCRJ","AAABLEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYBH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABLEST" varchar(3) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABGMOT" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABLEST","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYBS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNLE" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABIMRE" varchar(25) NOT NULL default ' ',
  "AAABIMRG" varchar(2) NOT NULL default ' ',
  "AAABIMRF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNLE","AAABLUNJ","AAABIMRD","AAABIMRE","AAABIMRG","AAABIMRF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYCH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABIMRE" varchar(25) NOT NULL default ' ',
  "AAABIMRG" varchar(2) NOT NULL default ' ',
  "AAABIMRF" varchar(3) NOT NULL default ' ',
  "AAABKNLE" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABIMRD","AAABIMRE","AAABIMRG","AAABIMRF","AAABKNLE","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYCW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNLE" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABMQVW" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNLE","AAABLUNJ","AAABMQVW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQVW" varchar(18) NOT NULL default ' ',
  "AAABKNLE" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMQVW","AAABKNLE","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYDO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNLE" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNLE","AAABLUNJ","AAABMLCZ","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYDZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABKNLE" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABMCVI","AAABKNLE","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYEK" (
  "AAABMREL" varchar(10) NOT NULL default ' ',
  "AAABLUSR" varchar(110) NOT NULL default ' ',
  "AAABLUSO" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABMREL","AAABLUSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYER" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNLE" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAAAEYFG" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNLE","AAABLUNJ","AAAAEYFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYFA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAEYFG" varchar(18) NOT NULL default ' ',
  "AAABKNLE" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAEYFG","AAABKNLE","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYFJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAAAEYFN" varchar(1) NOT NULL default ' ',
  "AAAAEYFO" varchar(1) NOT NULL default ' ',
  "AAAAEYFP" varchar(1) NOT NULL default ' ',
  "AAAAEYFQ" varchar(1) NOT NULL default ' ',
  "AAAAEYFR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTJH","AAABLTJI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYFV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHJOA" varchar(10) NOT NULL default ' ',
  "AAAAEYGD" varchar(10) NOT NULL default ' ',
  "AAAAEYFZ" varchar(10) NOT NULL default ' ',
  "AAABHIJR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHJOA","AAAAEYGD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYGE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLILW" varchar(10) NOT NULL default ' ',
  "AAABHJOA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQDU","AAABLILW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYGM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWBOP" varchar(12) NOT NULL default '000000000000',
  "AAAAEYHI" varchar(32) NOT NULL default ' ',
  "AAABMLPO" varchar(20) NOT NULL default '00000000000000000000',
  "AAABLLIO" varchar(10) NOT NULL default ' ',
  "AAABLUNJ" varchar(120) NOT NULL default ' ',
  "AAABIJEZ" varchar(20) NOT NULL default ' ',
  "AAABIJDD" varchar(3) NOT NULL default ' ',
  "AAABLSEB" varchar(32) NOT NULL default ' ',
  "AAABIJDF" varchar(32) NOT NULL default ' ',
  "AAABIJDG" varchar(4) NOT NULL default '0000',
  "AAABIJDH" varchar(6) NOT NULL default '000000',
  "AAABIJDI" varchar(6) NOT NULL default '000000',
  "AAABLQGL" varchar(1) NOT NULL default ' ',
  "AAABDNYT" varchar(1) NOT NULL default ' ',
  "AAABDNUG" varchar(1) NOT NULL default ' ',
  "AAABDNUF" varchar(1) NOT NULL default ' ',
  "AAAATPFH" varchar(1) NOT NULL default ' ',
  "AAAAEYHF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWBOP","AAAAEYHI","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYHK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAGBRZ" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAWBOP" varchar(12) NOT NULL default '000000000000',
  PRIMARY KEY ("AAABMRIL","AAAAGBRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYHQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUZUG" varchar(20) NOT NULL default ' ',
  "AAABMNDU" varchar(40) NOT NULL default ' ',
  "AAABECXR" varchar(4) NOT NULL default ' ',
  "AAAAEYHV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUZUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYHY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABLRYR" varchar(8) NOT NULL default '00000000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABMCQN" varchar(1) NOT NULL default ' ',
  "AAAAEYIF" varchar(1) NOT NULL default ' ',
  "AAAAEYIN" varchar(10) NOT NULL default ' ',
  "AAABKRIP" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZBM","AAABKOTY"),
  KEY "AAAAEYIL" ("AAABMRIL","AAAAEYIN","AAABLZBM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYIP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABFKRW" varchar(2) NOT NULL default ' ',
  "AAABLRYR" varchar(8) NOT NULL default '00000000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABLYLZ" varchar(8) NOT NULL default ' ',
  "AAAAEYIZ" varchar(1) NOT NULL default ' ',
  "AAAAEYJA" varchar(1) NOT NULL default ' ',
  "AAAAEYJB" varchar(2) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABFSXI" varchar(1) NOT NULL default ' ',
  "AAABJVUZ" varchar(3) NOT NULL default ' ',
  "AAAAVRYR" varchar(1) NOT NULL default ' ',
  "AAABLUQG" varchar(1) NOT NULL default ' ',
  "AAABMCWW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABKOTY","AAABMCJZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABFKRD" varchar(10) NOT NULL default ' ',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABLMLH" varchar(1) NOT NULL default ' ',
  "AAAAWCDK" varchar(3) NOT NULL default ' ',
  "AAAAPSER" varchar(10) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAAALEGV" varchar(18) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLLYO" varchar(10) NOT NULL default ' ',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAAAWUWQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABFKRD","AAABMCJZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYKH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABFJWM" varchar(1) NOT NULL default ' ',
  "AAABFJWQ" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABFULA" varchar(1) NOT NULL default ' ',
  "AAAAEYKS" varchar(20) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABMCXD" varchar(15) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABKRIM" varchar(10) NOT NULL default ' ',
  "AAABLRYR" varchar(8) NOT NULL default '00000000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABFJWN" varchar(1) NOT NULL default ' ',
  "AAABFJWO" varchar(1) NOT NULL default ' ',
  "AAABFJWP" varchar(1) NOT NULL default ' ',
  "AAABFJWR" varchar(1) NOT NULL default ' ',
  "AAABFJWS" varchar(1) NOT NULL default ' ',
  "AAABFJWT" varchar(2) NOT NULL default ' ',
  "AAABFSXI" varchar(1) NOT NULL default ' ',
  "AAAATHTC" varchar(12) NOT NULL default ' ',
  "AAAAEYLJ" varchar(10) NOT NULL default ' ',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAAAEYLM" varchar(6) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLVHD" varchar(15) NOT NULL default ' ',
  "AAABLZCP" varchar(35) NOT NULL default ' ',
  "AAABLZCR" varchar(35) NOT NULL default ' ',
  "AAABLVHG" varchar(35) NOT NULL default ' ',
  "AAABLVHH" varchar(35) NOT NULL default ' ',
  "AAABLVHI" varchar(35) NOT NULL default ' ',
  "AAABLVHJ" varchar(35) NOT NULL default ' ',
  "AAABLVHQ" varchar(35) NOT NULL default ' ',
  "AAABLVHK" varchar(10) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABLVHL" varchar(10) NOT NULL default ' ',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLVHY" varchar(15) NOT NULL default ' ',
  "AAABLXIZ" varchar(16) NOT NULL default ' ',
  "AAABLVHW" varchar(16) NOT NULL default ' ',
  "AAABLVHV" varchar(31) NOT NULL default ' ',
  "AAABLVHX" varchar(30) NOT NULL default ' ',
  "AAABLVHT" varchar(30) NOT NULL default ' ',
  "AAABLVHZ" varchar(14) NOT NULL default ' ',
  "AAAAEYMI" varchar(4) NOT NULL default ' ',
  "AAABFJUA" varchar(12) NOT NULL default ' ',
  "AAABKOSW" varchar(23) NOT NULL default ' ',
  "AAAAEYML" varchar(1) NOT NULL default ' ',
  "AAAANJPU" varchar(1) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABFPGJ" varchar(3) NOT NULL default ' ',
  "AAAAEYMP" varchar(3) NOT NULL default ' ',
  "AAAAEYNN" varchar(10) NOT NULL default ' ',
  "AAAAEYNO" varchar(10) NOT NULL default ' ',
  "AAABKRIP" varchar(16) NOT NULL default ' ',
  "AAAAEYMT" varchar(10) NOT NULL default ' ',
  "AAAAEYMU" varchar(10) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCWW" varchar(2) NOT NULL default ' ',
  "AAABMCXS" varchar(3) NOT NULL default ' ',
  "AAABKOKH" varchar(4) NOT NULL default ' ',
  "AAABKOWA" varchar(3) NOT NULL default ' ',
  "AAABKOWB" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABMCRK","AAABMCJZ"),
  KEY "AAAAEYNF" ("AAABMRIL","AAABMREX","AAABMCWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYNJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAAAEYNN" varchar(10) NOT NULL default ' ',
  "AAAAEYNO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABMCRK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYNS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  "AAAAHZDF" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCRJ","AAABMCRK","AAABMCJZ"),
  KEY "AAAAEYOG" ("AAABMRIL","AAABMRHS","AAAAHZDF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYOK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  "AAAAHZDF" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLZBM","AAABKOTY"),
  KEY "AAAAEYOW" ("AAABMRIL","AAABMRHS","AAAAHZDF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYPA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABFKRD" varchar(10) NOT NULL default ' ',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  "AAAAHZDF" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCRJ","AAABFKRD","AAABMCJZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYPO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABKPUA" varchar(6) NOT NULL default ' ',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABLYNC" varchar(3) NOT NULL default ' ',
  "AAABABFV" varchar(1) NOT NULL default ' ',
  "AAAAFADS" varchar(1) NOT NULL default ' ',
  "AAABLRYR" varchar(8) NOT NULL default '00000000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAAAVEBC" varchar(8) NOT NULL default ' ',
  "AAAAFADQ" varchar(3) NOT NULL default ' ',
  "AAABJRCP" decimal(5,2) NOT NULL default '0.00',
  "AAAAEYQC" varchar(10) NOT NULL default ' ',
  "AAAAEYQD" varchar(1) NOT NULL default ' ',
  "AAAAEYQE" varchar(1) NOT NULL default ' ',
  "AAAAEYQF" varchar(1) NOT NULL default ' ',
  "AAABMCXV" varchar(4) NOT NULL default ' ',
  "AAAAEYQH" varchar(1) NOT NULL default ' ',
  "AAABFULS" varchar(3) NOT NULL default ' ',
  "AAABFJXY" varchar(1) NOT NULL default ' ',
  "AAAAEYQK" varchar(3) NOT NULL default ' ',
  "AAAAEYQL" varchar(1) NOT NULL default ' ',
  "AAAAEYQM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABKPUA","AAABMCJZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYQR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABKPUA" varchar(6) NOT NULL default ' ',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  "AAAAHZDF" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCRJ","AAABKPUA","AAABMCJZ"),
  KEY "AAAAEYRF" ("AAABMRIL","AAABMRHS","AAAAHZDF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYRJ" (
  "AAAAEYRM" VARBINARY(255) NOT NULL,
  "AAAAEYRL" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEYRM"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYRN" (
  "AAAAEYSH" varchar(10) NOT NULL default ' ',
  "AAAAEYSI" varchar(2) NOT NULL default ' ',
  "AAAAEYSJ" varchar(10) NOT NULL default ' ',
  "AAAAEYSK" varchar(2) NOT NULL default ' ',
  "AAAAEYRZ" varchar(10) NOT NULL default ' ',
  "AAAAEYSA" varchar(2) NOT NULL default ' ',
  "AAAAEYRU" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAAAEYSH","AAAAEYSI","AAAAEYSJ","AAAAEYSK","AAAAEYRZ","AAAAEYSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYSB" (
  "AAAAEYSH" varchar(10) NOT NULL default ' ',
  "AAAAEYSI" varchar(2) NOT NULL default ' ',
  "AAAAEYSJ" varchar(10) NOT NULL default ' ',
  "AAAAEYSK" varchar(2) NOT NULL default ' ',
  "AAAAEYSG" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEYSH","AAAAEYSI","AAAAEYSJ","AAAAEYSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYSL" (
  "AAAAEYVF" varchar(10) NOT NULL default ' ',
  "AAAAEYVG" varchar(2) NOT NULL default ' ',
  "AAAAEYTG" varchar(32) NOT NULL default ' ',
  "AAAANART" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEYVF","AAAAEYVG","AAAAEYTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYST" (
  "AAAAEYTQ" varchar(32) NOT NULL default ' ',
  "AAAAEYTG" varchar(32) NOT NULL default ' ',
  "AAAAEYSZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEYTQ","AAAAEYTG","AAAAEYSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYTA" (
  "AAAAEYTQ" varchar(32) NOT NULL default ' ',
  "AAAAEYTC" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAAAEYTQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYTE" (
  "AAAAEYTG" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEYTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYTH" (
  "AAAAEYVF" varchar(10) NOT NULL default ' ',
  "AAAAEYVG" varchar(2) NOT NULL default ' ',
  "AAAANART" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEYVF","AAAAEYVG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYTN" (
  "AAAAEYVF" varchar(10) NOT NULL default ' ',
  "AAAAEYVG" varchar(2) NOT NULL default ' ',
  "AAAAEYTQ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEYVF","AAAAEYVG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYTT" (
  "AAAAEYVF" varchar(10) NOT NULL default ' ',
  "AAAAEYVG" varchar(2) NOT NULL default ' ',
  "AAABHMMH" varchar(108) NOT NULL default ' ',
  "AAAAEYTX" varchar(4) NOT NULL default ' ',
  "AAAAEYTY" VARBINARY(255),
  "AAAAEYTZ" varchar(14) NOT NULL default '00000000000000',
  "AAAAEYUA" varchar(10) NOT NULL default ' ',
  "AAAAEYUB" varchar(2) NOT NULL default ' ',
  "AAAAEYUC" varchar(4) NOT NULL default ' ',
  "AAAAEYUD" varchar(4) NOT NULL default ' ',
  "AAAAEYUE" VARBINARY(255),
  "AAAAEYUF" varchar(14) NOT NULL default '00000000000000',
  "AAAAEYUG" varchar(10) NOT NULL default ' ',
  "AAAAEYUH" varchar(2) NOT NULL default ' ',
  "AAAAEYUI" varchar(4) NOT NULL default ' ',
  "AAAAEYUJ" varchar(4) NOT NULL default ' ',
  "AAAAEYVI" VARBINARY(255),
  "AAAAEYVJ" VARBINARY(255),
  "AAAAEYUM" varchar(14) NOT NULL default '00000000000000',
  "AAAAEYUN" varchar(12) NOT NULL default ' ',
  "AAAAEYUO" varchar(255) NOT NULL default ' ',
  "AAAAPVIL" decimal(8,0) NOT NULL default '0',
  "AAAAEYUQ" VARBINARY(255),
  "AAAAEYUR" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAAAEYVF","AAAAEYVG","AAABHMMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYUV" (
  "AAAAEYVF" varchar(10) NOT NULL default ' ',
  "AAAAEYVG" varchar(2) NOT NULL default ' ',
  "AAABHMMH" varchar(108) NOT NULL default ' ',
  "AAAAEYVI" VARBINARY(255) NOT NULL,
  "AAAAEYVJ" VARBINARY(255) NOT NULL,
  "AAAAEYVK" VARBINARY(255) NOT NULL,
  "AAAARLNN" varchar(4) NOT NULL default ' ',
  "AAAAUMBT" varchar(4) NOT NULL default '0000',
  "AAAAUMBR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAAAEYVF","AAAAEYVG","AAABHMMH","AAAAEYVI"(255),"AAAAEYVJ"(255),"AAAAEYVK"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYVL" (
  "AAAAEZYH" varchar(32) NOT NULL default ' ',
  "AAABGBKQ" varchar(40) NOT NULL default ' ',
  "AAABMMJT" varchar(40) NOT NULL default ' ',
  "AAABGVZH" varchar(40) NOT NULL default ' ',
  "AAAAEYVQ" varchar(4) NOT NULL default ' ',
  "AAAAEYVR" varchar(40) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAAEZEE" int(11) NOT NULL default '0',
  "AAAAEZEF" int(11) NOT NULL default '0',
  "AAABBIKC" varchar(8) NOT NULL default ' ',
  "AAABCATV" varchar(40) NOT NULL default ' ',
  "AAABHQRQ" varchar(8) NOT NULL default ' ',
  "AAAAGACV" varchar(32) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAAAEYXO" int(11) NOT NULL default '0',
  "AAABLRTQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEZYH","AAABGBKQ","AAABMMJT"),
  KEY "AAAAEYWF" ("AAABHQRQ","AAABGVZH","AAABGBKQ"),
  KEY "AAAAEYWJ" ("AAABGBKQ","AAABMMJT"),
  KEY "AAAAEYWM" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYWO" (
  "AAAAEYYN" varchar(32) NOT NULL default ' ',
  "AAAAEYYG" varchar(32) NOT NULL default ' ',
  "AAAAEYYE" varchar(32) NOT NULL default ' ',
  "AAAAEYYO" varchar(40) NOT NULL default ' ',
  "AAAAEYWT" varchar(4) NOT NULL default ' ',
  "AAABMNMW" int(11) NOT NULL default '0',
  "AAAAEZYB" varchar(32) NOT NULL default ' ',
  "AAAAEZYC" varchar(32) NOT NULL default ' ',
  "AAAAEZYD" varchar(32) NOT NULL default ' ',
  "AAAAEYYQ" varchar(40) NOT NULL default ' ',
  "AAAAEYYP" varchar(4) NOT NULL default ' ',
  "AAAAEYYR" varchar(40) NOT NULL default ' ',
  "AAAAEYYS" varchar(40) NOT NULL default ' ',
  "AAAAEYYJ" varchar(40) NOT NULL default ' ',
  "AAAAEYYI" varchar(4) NOT NULL default ' ',
  "AAAAEYYK" varchar(40) NOT NULL default ' ',
  "AAABIHQW" varchar(40) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAAEZEE" varchar(4) NOT NULL default ' ',
  "AAAAEZEF" varchar(4) NOT NULL default ' ',
  "AAAAGACV" varchar(32) NOT NULL default ' ',
  "AAABBIKC" varchar(8) NOT NULL default ' ',
  "AAABCATV" varchar(40) NOT NULL default ' ',
  "AAABHQRQ" varchar(8) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAAAEYXO" int(11) NOT NULL default '0',
  "AAAAEYXP" varchar(8) NOT NULL default ' ',
  "AAAAEYXQ" varchar(8) NOT NULL default ' ',
  "AAABJSPH" varchar(40) NOT NULL default ' ',
  "AAAAEYXS" varchar(4) NOT NULL default ' ',
  "AAAAEYXT" varchar(32) NOT NULL default ' ',
  "AAAAEYXU" varchar(40) NOT NULL default ' ',
  "AAAAEYXV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEYYN","AAAAEYYG","AAAAEYYE"),
  KEY "AAAAEYXZ" ("AAAAEYYO","AAABBIKC","AAABCATV"),
  KEY "AAAAEYYD" ("AAAAEYYE"),
  KEY "AAAAEYYF" ("AAAAEYYG","AAAAEYYO","AAAAEYYI","AAAAEYYJ","AAAAEYYK","AAABIHQW"),
  KEY "AAAAEYYM" ("AAAAEYYN","AAAAEYYO","AAAAEYYP","AAAAEYYQ","AAAAEYYR","AAAAEYYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYYT" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHQRQ" varchar(8) NOT NULL default ' ',
  "AAABLOZE" varchar(40) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAAEZGO" varchar(4) NOT NULL default ' ',
  "AAAAEZGP" varchar(4) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAAAEZXA" varchar(4) NOT NULL default ' ',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABLRSS" varchar(12) NOT NULL default ' ',
  "AAABLRST" varchar(40) NOT NULL default ' ',
  "AAABLRSU" varchar(8) NOT NULL default '00000000',
  "AAABLRSV" varchar(8) NOT NULL default '00000000',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABHQRQ","AAABLOZE","AAABLHGN","AAAAEZGO","AAAAEZGP","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEYZR" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABJSPH" varchar(40) NOT NULL default ' ',
  "AAABHQRQ" varchar(8) NOT NULL default ' ',
  "AAABLOZE" varchar(40) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAAEZGO" varchar(4) NOT NULL default ' ',
  "AAAAEZGP" varchar(4) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAAAEZXA" varchar(4) NOT NULL default ' ',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABLRSU" varchar(8) NOT NULL default '00000000',
  "AAABLRSV" varchar(8) NOT NULL default '00000000',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABJSPH","AAABHQRQ","AAABLOZE","AAABLHGN","AAAAEZGO","AAAAEZGP","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZAP" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABLOZE" varchar(40) NOT NULL default ' ',
  "AAAAEZVR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZAV" (
  "AAAAEZYH" varchar(32) NOT NULL default ' ',
  "AAAAEZXY" varchar(32) NOT NULL default ' ',
  "AAABHQRQ" varchar(8) NOT NULL default ' ',
  "AAABLOZE" varchar(40) NOT NULL default ' ',
  "AAABJSPH" varchar(40) NOT NULL default ' ',
  "AAAAEZBB" varchar(40) NOT NULL default ' ',
  "AAAAEZBU" varchar(250) NOT NULL default ' ',
  "AAAAEZBD" varchar(250) NOT NULL default ' ',
  "AAAAEZBS" varchar(40) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABBIKC" varchar(8) NOT NULL default ' ',
  "AAAAEZBH" varchar(32) NOT NULL default ' ',
  "AAAAEZBI" varchar(32) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAAAGACV" varchar(32) NOT NULL default ' ',
  "AAABCATV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEZYH"),
  KEY "AAAAEZBN" ("AAABHQRQ","AAABLOZE","AAABJSPH"),
  KEY "AAAAEZBR" ("AAAAEZBS"),
  KEY "AAAAEZBT" ("AAAAEZBU"),
  KEY "AAAAEZBV" ("AAAAEZXY"),
  KEY "AAAAEZBX" ("AAABBIKC","AAABJSPH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZCA" (
  "AAAAEZYH" varchar(32) NOT NULL default ' ',
  "AAABLOZE" varchar(40) NOT NULL default ' ',
  "AAAAGGKS" varchar(40) NOT NULL default ' ',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAAAFEVQ" varchar(255) NOT NULL default ' ',
  "AAABBIKC" varchar(8) NOT NULL default ' ',
  "AAABCATV" varchar(40) NOT NULL default ' ',
  "AAABHQRQ" varchar(8) NOT NULL default ' ',
  "AAAAEZYB" varchar(32) NOT NULL default ' ',
  "AAAAEZYC" varchar(32) NOT NULL default ' ',
  "AAAAEZYD" varchar(32) NOT NULL default ' ',
  "AAAAGACV" varchar(32) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAAAEZDI" varchar(40) NOT NULL default ' ',
  "AAAAEZDJ" varchar(40) NOT NULL default ' ',
  "AAAAEZYE" varchar(80) NOT NULL default ' ',
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABLRTQ" varchar(4) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAAEZXU" varchar(8) NOT NULL default ' ',
  "AAAAEZCV" varchar(8) NOT NULL default ' ',
  "AAABJSPH" varchar(40) NOT NULL default ' ',
  "AAAAEZXS" varchar(4) NOT NULL default ' ',
  "AAAAEZXT" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEZYH"),
  KEY "AAAAEZDA" ("AAABLOZE","AAABLFYD"),
  KEY "AAAAEZDD" ("AAABLOZE","AAAAEZYB","AAAAEZYC"),
  KEY "AAAAEZDH" ("AAAAEZDI","AAAAEZDJ","AAAAEZYE","AAABLFYD"),
  KEY "AAAAEZDM" ("AAABHQRQ","AAABLOZE"),
  KEY "AAAAEZDP" ("AAABBIKC","AAABLOZE"),
  KEY "AAAAEZDS" ("AAABMOHM","AAABLRTQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZDV" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABCATV" varchar(40) NOT NULL default ' ',
  "AAAAEZDY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABCATV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZEB" (
  "AAABBIKC" varchar(40) NOT NULL default ' ',
  "AAABCATV" varchar(40) NOT NULL default ' ',
  "AAAAEZEE" varchar(4) NOT NULL default ' ',
  "AAAAEZEF" varchar(4) NOT NULL default ' ',
  "AAAAVARC" varchar(8) NOT NULL default ' ',
  "AAAAVASQ" varchar(40) NOT NULL default ' ',
  "AAAAVARE" varchar(3) NOT NULL default ' ',
  "AAAAVARF" varchar(10) NOT NULL default ' ',
  "AAAAVARG" varchar(3) NOT NULL default ' ',
  "AAAAVARH" varchar(10) NOT NULL default ' ',
  "AAAAVARI" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABBIKC","AAABCATV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZEP" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBIKC" varchar(40) NOT NULL default ' ',
  "AAAAEZES" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABBIKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZEV" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHQRQ" varchar(8) NOT NULL default ' ',
  "AAABLOZE" varchar(40) NOT NULL default ' ',
  "AAABGBKQ" varchar(40) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAAEZGO" varchar(4) NOT NULL default ' ',
  "AAAAEZGP" varchar(4) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAAAEZXA" varchar(4) NOT NULL default ' ',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABLRSS" varchar(12) NOT NULL default ' ',
  "AAABLRST" varchar(40) NOT NULL default ' ',
  "AAABLRSU" varchar(8) NOT NULL default '00000000',
  "AAABLRSV" varchar(8) NOT NULL default '00000000',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABHQRQ","AAABLOZE","AAABGBKQ","AAABLHGN","AAAAEZGO","AAAAEZGP","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZFV" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAAEZGL" varchar(32) NOT NULL default ' ',
  "AAABGBKQ" varchar(40) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAAEZGO" varchar(4) NOT NULL default ' ',
  "AAAAEZGP" varchar(4) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAAAEZXA" varchar(4) NOT NULL default ' ',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABLRSU" varchar(8) NOT NULL default '00000000',
  "AAABLRSV" varchar(8) NOT NULL default '00000000',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAAAEZGL","AAABGBKQ","AAABLHGN","AAAAEZGO","AAAAEZGP","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZGR" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABLOZE" varchar(40) NOT NULL default ' ',
  "AAABGBKQ" varchar(40) NOT NULL default ' ',
  "AAAAEZZQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABLOZE","AAABGBKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZGZ" (
  "AAAAVARC" varchar(8) NOT NULL default ' ',
  "AAAAVASQ" varchar(40) NOT NULL default ' ',
  "AAAAVARE" varchar(3) NOT NULL default ' ',
  "AAAAVARF" varchar(10) NOT NULL default ' ',
  "AAAAVARG" varchar(3) NOT NULL default ' ',
  "AAAAVASI" varchar(8) NOT NULL default ' ',
  "AAAAVASJ" varchar(40) NOT NULL default ' ',
  "AAAAVASK" varchar(10) NOT NULL default ' ',
  "AAAAVASM" varchar(8) NOT NULL default '00000000',
  "AAAAVASN" varchar(6) NOT NULL default '000000',
  "AAAAEZSL" varchar(3) NOT NULL default ' ',
  "AAABLJJL" varchar(40) NOT NULL default ' ',
  "AAAAEZSG" int(11) NOT NULL default '0',
  "AAABLRTV" varchar(8) NOT NULL default '00000000',
  "AAABLRTW" varchar(6) NOT NULL default '000000',
  "AAAAEZHP" varchar(40) NOT NULL default ' ',
  "AAAAEZHQ" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAAVARC","AAAAVASQ","AAAAVARE","AAAAVARF","AAAAVARG","AAAAVASI","AAAAVASJ","AAAAVASK","AAAAVASM","AAAAVASN","AAAAEZSL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZIC" (
  "AAABHQRQ" varchar(8) NOT NULL default ' ',
  "AAABLOZE" varchar(40) NOT NULL default ' ',
  "AAABGZSJ" varchar(40) NOT NULL default ' ',
  "AAAAEZIP" varchar(32) NOT NULL default ' ',
  "AAAAEZIH" varchar(8) NOT NULL default ' ',
  "AAAAEZII" varchar(40) NOT NULL default ' ',
  "AAAAEZIJ" varchar(46) NOT NULL default ' ',
  "AAAAEZIK" varchar(1) NOT NULL default ' ',
  "AAAAEZIL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHQRQ","AAABLOZE","AAABGZSJ","AAAAEZIP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZIQ" (
  "AAAAEZZS" varchar(32) NOT NULL default ' ',
  "AAABJSPH" varchar(40) NOT NULL default ' ',
  "AAABHQRQ" varchar(8) NOT NULL default ' ',
  "AAABLOZE" varchar(40) NOT NULL default ' ',
  "AAABGZSJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEZZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZIX" (
  "AAAAEZZS" varchar(32) NOT NULL default ' ',
  "AAABHQRQ" varchar(8) NOT NULL default ' ',
  "AAABLOZE" varchar(40) NOT NULL default ' ',
  "AAABGZSJ" varchar(40) NOT NULL default ' ',
  "AAABJSPH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEZZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZJE" (
  "AAAAEZZS" varchar(32) NOT NULL default ' ',
  "AAAAEZVK" varchar(8) NOT NULL default ' ',
  "AAAAEZVL" varchar(40) NOT NULL default ' ',
  "AAAAEZVF" varchar(4) NOT NULL default ' ',
  "AAAAEZVG" varchar(30) NOT NULL default ' ',
  "AAAAEZVH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEZZS","AAAAEZVK","AAAAEZVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZJO" (
  "AAAAEZKZ" varchar(40) NOT NULL default ' ',
  "AAAAEZLB" varchar(40) NOT NULL default ' ',
  "AAAAEZKO" varchar(40) NOT NULL default ' ',
  "AAAAEZKP" varchar(4) NOT NULL default ' ',
  "AAAAEZKQ" varchar(4) NOT NULL default ' ',
  "AAAAEZLC" varchar(40) NOT NULL default ' ',
  "AAAAEZKS" varchar(40) NOT NULL default ' ',
  "AAAAEZLD" smallint(6) NOT NULL default '0',
  "AAAAEZKU" smallint(6) NOT NULL default '0',
  "AAAAEZKV" varchar(8) NOT NULL default ' ',
  "AAAAEZKW" varchar(8) NOT NULL default ' ',
  "AAAAEZKX" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEZKZ","AAAAEZLB","AAAAEZKO"),
  KEY "AAAAEZKE" ("AAAAEZLB"),
  KEY "AAAAEZKG" ("AAAAEZKO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZKI" (
  "AAABJSPH" varchar(40) NOT NULL default ' ',
  "AAAAEZKZ" varchar(40) NOT NULL default ' ',
  "AAAAEZKL" varchar(40) NOT NULL default ' ',
  "AAAAEZLB" varchar(40) NOT NULL default ' ',
  "AAAAEZKN" varchar(40) NOT NULL default ' ',
  "AAAAEZKO" varchar(40) NOT NULL default ' ',
  "AAAAEZKP" varchar(4) NOT NULL default ' ',
  "AAAAEZKQ" varchar(4) NOT NULL default ' ',
  "AAAAEZLC" varchar(40) NOT NULL default ' ',
  "AAAAEZKS" varchar(40) NOT NULL default ' ',
  "AAAAEZLD" smallint(6) NOT NULL default '0',
  "AAAAEZKU" smallint(6) NOT NULL default '0',
  "AAAAEZKV" varchar(8) NOT NULL default ' ',
  "AAAAEZKW" varchar(8) NOT NULL default ' ',
  "AAAAEZKX" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABJSPH","AAAAEZKZ"),
  KEY "AAAAEZLA" ("AAAAEZLB","AAAAEZLC","AAAAEZLD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZLE" (
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABLMOZ" varchar(40) NOT NULL default ' ',
  "AAABLRPK" varchar(40) NOT NULL default ' ',
  "AAAAEZLI" varchar(1) NOT NULL default ' ',
  "AAAAEZLJ" varchar(40) NOT NULL default ' ',
  "AAAAEZLK" varchar(40) NOT NULL default ' ',
  "AAAAEZLL" varchar(3) NOT NULL default '000',
  "AAAAEZLM" varchar(8) NOT NULL default ' ',
  "AAAAEZLN" int(11) NOT NULL default '0',
  "AAABEBYS" varchar(12) NOT NULL default ' ',
  "AAABKWET" varchar(8) NOT NULL default '00000000',
  "AAABGCZV" varchar(6) NOT NULL default '000000',
  "AAAAEZLR" varchar(8) NOT NULL default ' ',
  "AAAAEZLS" varchar(40) NOT NULL default ' ',
  "AAAAEZLT" varchar(3) NOT NULL default ' ',
  "AAAAEZLU" varchar(10) NOT NULL default ' ',
  "AAAAEZLV" varchar(3) NOT NULL default ' ',
  "AAAAEZLW" varchar(10) NOT NULL default ' ',
  "AAAAEZLX" varchar(10) NOT NULL default ' ',
  "AAAAEZQZ" varchar(10) NOT NULL default ' ',
  "AAAAEZRA" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLENH","AAABLMOZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZMC" (
  "AAAAEZML" varchar(40) NOT NULL default ' ',
  "AAABLUVJ" varchar(100) NOT NULL default ' ',
  "AAABLGEF" varchar(20) NOT NULL default ' ',
  "AAABLYRW" varchar(255) NOT NULL default ' ',
  "AAAAEZMH" varchar(1) NOT NULL default ' ',
  "AAAAEZMI" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(255) NOT NULL default ' ',
  "AAAAVNMQ" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAAEZML")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZMM" (
  "AAAAEZRY" varchar(32) NOT NULL default ' ',
  "AAAAEZNK" varchar(32) NOT NULL default ' ',
  "AAAAEZMP" varchar(1) NOT NULL default ' ',
  "AAAAEZMQ" varchar(1) NOT NULL default ' ',
  "AAABLGEF" varchar(20) NOT NULL default ' ',
  "AAAAEZMS" varchar(255) NOT NULL default ' ',
  "AAAAEZMT" varchar(255) NOT NULL default ' ',
  "AAABEBYS" varchar(12) NOT NULL default ' ',
  "AAAAEZMV" varchar(40) NOT NULL default ' ',
  "AAABKWET" varchar(8) NOT NULL default '00000000',
  "AAABGCZV" varchar(6) NOT NULL default '000000',
  "AAAAEZQZ" varchar(10) NOT NULL default ' ',
  "AAAAEZRA" int(11) NOT NULL default '0',
  "AAAAEZNA" varchar(1) NOT NULL default ' ',
  "AAAAEZNB" varchar(1) NOT NULL default ' ',
  "AAAAEZNC" varchar(1) NOT NULL default ' ',
  "AAAAEZND" varchar(1) NOT NULL default ' ',
  "AAAAEZNE" varchar(1) NOT NULL default ' ',
  "AAAAEZNF" varchar(1) NOT NULL default ' ',
  "AAAAEZNG" varchar(1) NOT NULL default ' ',
  "AAAAEZNH" varchar(1) NOT NULL default ' ',
  "AAAAEZNI" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEZRY","AAAAEZNK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZNL" (
  "AAAAEZNY" varchar(7) NOT NULL default ' ',
  "AAAAEZNZ" varchar(32) NOT NULL default ' ',
  "AAABLTOK" varchar(32) NOT NULL default ' ',
  "AAABMLZS" varchar(4) NOT NULL default '0000',
  "AAABLTYA" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default ' ',
  "AAABLEHI" varchar(4) NOT NULL default ' ',
  "AAAAGACV" varchar(32) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAAEZNV" varchar(255) NOT NULL default ' ',
  "AAAAEZNW" decimal(15,0) NOT NULL default '0',
  "AAAAEZNX" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAAEZNY","AAAAEZNZ","AAABLTOK","AAABMLZS","AAABLTYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZOD" (
  "AAAAEZOG" varchar(30) NOT NULL default ' ',
  "AAAAEZOF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEZOG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZOH" (
  "AAAAEZOR" varchar(32) NOT NULL default ' ',
  "AAAAEZYM" varchar(1) NOT NULL default ' ',
  "AAAAEZYN" varchar(1) NOT NULL default ' ',
  "AAAAEZYO" varchar(1) NOT NULL default ' ',
  "AAAAEZYP" varchar(1) NOT NULL default ' ',
  "AAABJSLL" smallint(6) NOT NULL default '0',
  "AAAAEZOO" varchar(4) NOT NULL default ' ',
  "AAAAEZYR" varchar(8) NOT NULL default ' ',
  "AAAAEZYS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEZOR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZOS" (
  "AAAAEZPH" varchar(40) NOT NULL default ' ',
  "AAAAJLGT" varchar(1) NOT NULL default ' ',
  "AAABMORS" varchar(30) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAAAEZOX" varchar(40) NOT NULL default ' ',
  "AAAAEZOY" varchar(40) NOT NULL default ' ',
  "AAAAEZVG" varchar(30) NOT NULL default ' ',
  "AAAAEZVH" varchar(40) NOT NULL default ' ',
  "AAABLHAH" varchar(255) NOT NULL default ' ',
  "AAABKNGX" varchar(32) NOT NULL default ' ',
  "AAAAVLSN" varchar(1) NOT NULL default ' ',
  "AAABLMOZ" varchar(1) NOT NULL default ' ',
  "AAABFIGO" varchar(1) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAAEZPH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZPI" (
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABLRPK" varchar(40) NOT NULL default ' ',
  "AAABGFQD" varchar(32) NOT NULL default ' ',
  "AAAAEZQL" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVEJX" varchar(1) NOT NULL default ' ',
  "AAAAEZQN" varchar(1) NOT NULL default ' ',
  "AAAARKEK" int(11) NOT NULL default '0',
  "AAAAEZQP" varchar(1) NOT NULL default ' ',
  "AAAAEZQQ" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(10) NOT NULL default ' ',
  "AAABEBYS" varchar(12) NOT NULL default ' ',
  "AAABKWET" varchar(8) NOT NULL default '00000000',
  "AAABGCZV" varchar(6) NOT NULL default '000000',
  "AAAAEZQV" decimal(15,0) NOT NULL default '0',
  "AAAAEZQW" int(11) NOT NULL default '0',
  "AAAAEZQX" varchar(20) NOT NULL default ' ',
  "AAAAEZQY" varchar(32) NOT NULL default ' ',
  "AAAAEZQZ" varchar(10) NOT NULL default ' ',
  "AAAAEZRA" int(11) NOT NULL default '0',
  "AAAAEZRB" varchar(32) NOT NULL default ' ',
  "AAAAEZRC" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLENH","AAABLRPK","AAABGFQD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZQH" (
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABLRPK" varchar(40) NOT NULL default ' ',
  "AAABGFQD" varchar(32) NOT NULL default ' ',
  "AAAAEZQL" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVEJX" varchar(1) NOT NULL default ' ',
  "AAAAEZQN" varchar(1) NOT NULL default ' ',
  "AAAARKEK" int(11) NOT NULL default '0',
  "AAAAEZQP" varchar(1) NOT NULL default ' ',
  "AAAAEZQQ" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(10) NOT NULL default ' ',
  "AAABEBYS" varchar(12) NOT NULL default ' ',
  "AAABKWET" varchar(8) NOT NULL default '00000000',
  "AAABGCZV" varchar(6) NOT NULL default '000000',
  "AAAAEZQV" decimal(15,0) NOT NULL default '0',
  "AAAAEZQW" int(11) NOT NULL default '0',
  "AAAAEZQX" varchar(20) NOT NULL default ' ',
  "AAAAEZQY" varchar(32) NOT NULL default ' ',
  "AAAAEZQZ" varchar(10) NOT NULL default ' ',
  "AAAAEZRA" int(11) NOT NULL default '0',
  "AAAAEZRB" varchar(32) NOT NULL default ' ',
  "AAAAEZRC" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLENH","AAABLRPK","AAABGFQD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZRG" (
  "AAAAEZRY" varchar(32) NOT NULL default ' ',
  "AAAAEZRI" varchar(255) NOT NULL default ' ',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABMOHM" varchar(40) NOT NULL default ' ',
  "AAAAVASI" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEZRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZRN" (
  "AAAAEZRY" varchar(32) NOT NULL default ' ',
  "AAAAEZRZ" varchar(40) NOT NULL default ' ',
  "AAABLUVJ" varchar(32) NOT NULL default ' ',
  "AAABLGEF" varchar(20) NOT NULL default ' ',
  "AAAAEZRS" varchar(1) NOT NULL default ' ',
  "AAAAEZRT" varchar(1) NOT NULL default ' ',
  "AAAAEZRU" varchar(1) NOT NULL default ' ',
  "AAAAEZRV" varchar(12) NOT NULL default ' ',
  "AAAAEZRW" varchar(8) NOT NULL default '00000000',
  "AAAAEZRX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAAEZRY","AAAAEZRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZSA" (
  "AAABLENH" varchar(32) NOT NULL default ' ',
  "AAAAQWTG" varchar(40) NOT NULL default ' ',
  "AAAAEZSL" varchar(3) NOT NULL default ' ',
  "AAABJVQM" varchar(20) NOT NULL default ' ',
  "AAABLJJL" varchar(40) NOT NULL default ' ',
  "AAAAEZSG" int(11) NOT NULL default '0',
  "AAAAEZSH" varchar(12) NOT NULL default ' ',
  "AAAAEZSI" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLENH","AAAAQWTG","AAAAEZSL","AAABJVQM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZSN" (
  "AAAAEZSZ" varchar(40) NOT NULL default ' ',
  "AAAAEZTA" varchar(40) NOT NULL default ' ',
  "AAAAEZTB" varchar(40) NOT NULL default ' ',
  "AAAAEZSR" varchar(46) NOT NULL default ' ',
  "AAAAEZSS" varchar(120) NOT NULL default ' ',
  "AAAAEZST" varchar(32) NOT NULL default ' ',
  "AAAAEZSU" varchar(32) NOT NULL default ' ',
  "AAAAEZSV" varchar(1) NOT NULL default ' ',
  "AAAAEZSW" varchar(32) NOT NULL default ' ',
  "AAAAEZSX" varchar(1) NOT NULL default ' ',
  "AAAAEZSY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAEZSZ","AAAAEZTA","AAAAEZTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZTC" (
  "AAAAEZTS" varchar(1) NOT NULL default ' ',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABMPMH" varchar(40) NOT NULL default ' ',
  "AAABGZSJ" varchar(40) NOT NULL default ' ',
  "AAAAEZTH" varchar(32) NOT NULL default ' ',
  "AAABJSPH" varchar(40) NOT NULL default ' ',
  "AAABLUEO" varchar(40) NOT NULL default ' ',
  "AAABGFCP" varchar(32) NOT NULL default ' ',
  "AAAAEZTL" varchar(1) NOT NULL default ' ',
  "AAAAVLSN" varchar(1) NOT NULL default ' ',
  "AAABLMOZ" varchar(1) NOT NULL default ' ',
  "AAAAEZTO" varchar(1) NOT NULL default ' ',
  "AAAAEZTP" varchar(1) NOT NULL default ' ',
  "AAAAEZTQ" varchar(1) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAAEZTS","AAABLTVR","AAABMPMH","AAABGZSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZTW" (
  "AAAAEZUW" varchar(32) NOT NULL default ' ',
  "AAAAEZUX" varchar(32) NOT NULL default ' ',
  "AAAAEZUY" varchar(32) NOT NULL default ' ',
  "AAAAEZUA" varchar(40) NOT NULL default ' ',
  "AAAAEZUB" varchar(40) NOT NULL default ' ',
  "AAAAEZUC" varchar(40) NOT NULL default ' ',
  "AAAAEZUD" varchar(8) NOT NULL default ' ',
  "AAAAEZUE" varchar(40) NOT NULL default ' ',
  "AAAAEZUF" varchar(4) NOT NULL default ' ',
  "AAABMNMW" int(11) NOT NULL default '0',
  "AAAAEZYB" varchar(32) NOT NULL default ' ',
  "AAAAEZYC" varchar(32) NOT NULL default ' ',
  "AAAAEZUJ" varchar(8) NOT NULL default ' ',
  "AAAAEZUK" varchar(40) NOT NULL default ' ',
  "AAAAEZUL" varchar(40) NOT NULL default ' ',
  "AAAAEZUM" varchar(4) NOT NULL default ' ',
  "AAAAEZUN" varchar(80) NOT NULL default ' ',
  "AAAAEZUO" varchar(8) NOT NULL default ' ',
  "AAAAEZUP" varchar(40) NOT NULL default ' ',
  "AAAAEZUQ" varchar(40) NOT NULL default ' ',
  "AAAAEZUR" varchar(4) NOT NULL default ' ',
  "AAAAEZUS" varchar(80) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAAGACV" varchar(32) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAAEZUW","AAAAEZUX","AAAAEZUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZUZ" (
  "AAABHQRQ" varchar(8) NOT NULL default ' ',
  "AAABLOZE" varchar(40) NOT NULL default ' ',
  "AAAAEZVK" varchar(8) NOT NULL default ' ',
  "AAAAEZVL" varchar(40) NOT NULL default ' ',
  "AAAAEZVE" varchar(1) NOT NULL default ' ',
  "AAAAEZVF" varchar(4) NOT NULL default ' ',
  "AAAAEZVG" varchar(30) NOT NULL default ' ',
  "AAAAEZVH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABHQRQ","AAABLOZE","AAAAEZVK","AAAAEZVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZVM" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABJSPH" varchar(40) NOT NULL default ' ',
  "AAABHQRQ" varchar(8) NOT NULL default ' ',
  "AAABLOZE" varchar(40) NOT NULL default ' ',
  "AAAAEZVR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABJSPH","AAABHQRQ","AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZVW" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAAFYHK" varchar(32) NOT NULL default ' ',
  "AAAAEZWH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAAAEZWB" varchar(1) NOT NULL default ' ',
  "AAAAEZWC" varchar(8) NOT NULL default '00000000',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAAAFYHK","AAAAEZWH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZWJ" (
  "AAABMPMH" varchar(40) NOT NULL default ' ',
  "AAAAEZWL" varchar(1) NOT NULL default ' ',
  "AAAAEZWM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZWO" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABMPMH" varchar(40) NOT NULL default ' ',
  "AAABMDXM" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZWU" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAAEZYH" varchar(32) NOT NULL default ' ',
  "AAABGZSJ" varchar(40) NOT NULL default ' ',
  "AAAAEZXM" varchar(1) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAAAEZXA" varchar(4) NOT NULL default ' ',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABLRSS" varchar(12) NOT NULL default ' ',
  "AAABLRST" varchar(40) NOT NULL default ' ',
  "AAABLRSU" varchar(8) NOT NULL default '00000000',
  "AAABLRSV" varchar(8) NOT NULL default '00000000',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAAAEZYH","AAABGZSJ","AAAAEZXM","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZXO" (
  "AAAAEZYH" varchar(32) NOT NULL default ' ',
  "AAABJSPH" varchar(40) NOT NULL default ' ',
  "AAAAEZXR" varchar(40) NOT NULL default ' ',
  "AAAAEZXS" varchar(4) NOT NULL default ' ',
  "AAAAEZXT" varchar(32) NOT NULL default ' ',
  "AAAAEZXU" varchar(8) NOT NULL default ' ',
  "AAABGBRU" varchar(40) NOT NULL default ' ',
  "AAABLFYD" varchar(80) NOT NULL default ' ',
  "AAAAEZXX" varchar(32) NOT NULL default ' ',
  "AAAAEZXY" varchar(40) NOT NULL default ' ',
  "AAAAEZXZ" varchar(8) NOT NULL default ' ',
  "AAAAEZYA" varchar(40) NOT NULL default ' ',
  "AAAAEZYB" varchar(32) NOT NULL default ' ',
  "AAAAEZYC" varchar(32) NOT NULL default ' ',
  "AAAAEZYD" varchar(32) NOT NULL default ' ',
  "AAAAEZYE" varchar(132) NOT NULL default ' ',
  "AAAAGACV" varchar(32) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAAEZYH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZYI" (
  "AAABHQRQ" varchar(8) NOT NULL default ' ',
  "AAABLOZE" varchar(40) NOT NULL default ' ',
  "AAABGZSJ" varchar(40) NOT NULL default ' ',
  "AAAAEZYM" varchar(1) NOT NULL default ' ',
  "AAAAEZYN" varchar(1) NOT NULL default ' ',
  "AAAAEZYO" varchar(1) NOT NULL default ' ',
  "AAAAEZYP" varchar(1) NOT NULL default ' ',
  "AAABJSLL" smallint(6) NOT NULL default '0',
  "AAAAEZYR" varchar(8) NOT NULL default ' ',
  "AAAAEZYS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABHQRQ","AAABLOZE","AAABGZSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZYW" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAEZZD" varchar(8) NOT NULL default ' ',
  "AAABLOZE" varchar(40) NOT NULL default ' ',
  "AAABGZSJ" varchar(40) NOT NULL default ' ',
  "AAABLLLP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAAAEZZD","AAABLOZE","AAABGZSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZZG" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAEZZL" varchar(32) NOT NULL default ' ',
  "AAABLLLP" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAAAEZZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZZM" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAEZZS" varchar(32) NOT NULL default ' ',
  "AAABGBKQ" varchar(40) NOT NULL default ' ',
  "AAAAEZZQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAAAEZZS","AAABGBKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAEZZU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABIMRR" varchar(6) NOT NULL default '000000',
  "AAAAFABH" varchar(18) NOT NULL default ' ',
  "AAAAFABI" varchar(18) NOT NULL default ' ',
  "AAAAFAAV" varchar(4) NOT NULL default ' ',
  "AAAAFAAW" varchar(4) NOT NULL default ' ',
  "AAAAFAAX" varchar(15) NOT NULL default ' ',
  "AAAAFAAY" varchar(15) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABKMHA" varchar(12) NOT NULL default ' ',
  "AAAAFABA" varchar(8) NOT NULL default ' ',
  "AAABLVJK" varchar(8) NOT NULL default '00000000',
  "AAAAFABC" varchar(8) NOT NULL default '00000000',
  "AAABGKZH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQIL","AAABIMRR","AAAAFABH","AAAAFABI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFAAP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRJU" varchar(8) NOT NULL default '00000000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAAAFABH" varchar(18) NOT NULL default ' ',
  "AAAAFABI" varchar(18) NOT NULL default ' ',
  "AAAAFAAV" varchar(4) NOT NULL default ' ',
  "AAAAFAAW" varchar(4) NOT NULL default ' ',
  "AAAAFAAX" varchar(15) NOT NULL default ' ',
  "AAAAFAAY" varchar(15) NOT NULL default ' ',
  "AAABKMHA" varchar(12) NOT NULL default ' ',
  "AAAAFABA" varchar(8) NOT NULL default ' ',
  "AAABLVJK" varchar(8) NOT NULL default '00000000',
  "AAAAFABC" varchar(8) NOT NULL default '00000000',
  "AAABGKZH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRJU","AAABMCVI","AAAAFABH","AAAAFABI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFABJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFACH" varchar(12) NOT NULL default ' ',
  "AAABLKHW" varchar(3) NOT NULL default '000',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFACH","AAABLKHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFABR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFACH" varchar(12) NOT NULL default ' ',
  "AAABLKHW" varchar(3) NOT NULL default '000',
  "AAABFNHW" varchar(32) NOT NULL default ' ',
  "AAABEINZ" varchar(30) NOT NULL default ' ',
  "AAABMKXX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFACH","AAABLKHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFACB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFACH" varchar(12) NOT NULL default ' ',
  "AAABLKHW" varchar(3) NOT NULL default '000',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFACH","AAABLKHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFACJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABFSXI" varchar(1) NOT NULL default ' ',
  "AAAAFACR" varchar(3) NOT NULL default ' ',
  "AAABLYLZ" varchar(8) NOT NULL default ' ',
  "AAAAFACT" varchar(4) NOT NULL default ' ',
  "AAAAFACU" varchar(4) NOT NULL default ' ',
  "AAAAFACV" varchar(1) NOT NULL default ' ',
  "AAAAFACW" varchar(8) NOT NULL default ' ',
  "AAAAFACX" varchar(4) NOT NULL default ' ',
  "AAAAFACY" varchar(1) NOT NULL default ' ',
  "AAAAFACZ" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRXK","AAABMCRJ","AAABMDBB","AAABMCRK","AAABKOTY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFADG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABKPUA" varchar(6) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKOSW" varchar(23) NOT NULL default ' ',
  "AAABABFV" varchar(1) NOT NULL default ' ',
  "AAABLYNC" varchar(3) NOT NULL default ' ',
  "AAABJRCP" decimal(5,2) NOT NULL default '0.00',
  "AAAAFADQ" varchar(3) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAFADS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABMCRK","AAABKPUA","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFADY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFAGN" varchar(8) NOT NULL default ' ',
  "AAAAFAEQ" varchar(3) NOT NULL default ' ',
  "AAAAUKTF" varchar(40) NOT NULL default ' ',
  "AAAAFAED" varchar(30) NOT NULL default ' ',
  "AAAAFAEE" varchar(30) NOT NULL default ' ',
  "AAAAFAEF" varchar(40) NOT NULL default ' ',
  "AAAAFAEG" varchar(30) NOT NULL default ' ',
  "AAAAFAEH" varchar(30) NOT NULL default ' ',
  "AAAAFAEI" varchar(40) NOT NULL default ' ',
  "AAAAVPJI" varchar(40) NOT NULL default ' ',
  "AAABKPLY" varchar(10) NOT NULL default ' ',
  "AAAAFAEL" varchar(10) NOT NULL default ' ',
  "AAAAFAEM" varchar(1) NOT NULL default ' ',
  "AAAAFAEN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFAGN","AAAAFAEQ","AAAAUKTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFAES" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFAGN" varchar(8) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  "AAAAFAEW" varchar(1) NOT NULL default ' ',
  "AAAAFAEX" varchar(1) NOT NULL default ' ',
  "AAAAFAEY" varchar(1) NOT NULL default ' ',
  "AAAAFAEZ" varchar(1) NOT NULL default ' ',
  "AAAAFAFA" varchar(1) NOT NULL default ' ',
  "AAAAFAFB" varchar(1) NOT NULL default ' ',
  "AAAAFAFC" varchar(1) NOT NULL default ' ',
  "AAAAFAFD" varchar(1) NOT NULL default ' ',
  "AAAAFAFE" varchar(1) NOT NULL default ' ',
  "AAAAFAFF" varchar(1) NOT NULL default ' ',
  "AAAAFAFG" varchar(1) NOT NULL default ' ',
  "AAAAFAFH" varchar(1) NOT NULL default ' ',
  "AAAAFAFI" varchar(1) NOT NULL default ' ',
  "AAAAFAFJ" varchar(1) NOT NULL default ' ',
  "AAABLJBB" varchar(1) NOT NULL default ' ',
  "AAAAFAFL" varchar(1) NOT NULL default ' ',
  "AAAAFAFM" varchar(1) NOT NULL default ' ',
  "AAAAFAFN" varchar(1) NOT NULL default ' ',
  "AAAAFAFO" varchar(1) NOT NULL default ' ',
  "AAAAFAFP" varchar(1) NOT NULL default ' ',
  "AAAAFAFQ" varchar(1) NOT NULL default ' ',
  "AAAAFAFR" varchar(1) NOT NULL default ' ',
  "AAAAFAFS" varchar(1) NOT NULL default ' ',
  "AAAAFAFT" varchar(1) NOT NULL default ' ',
  "AAAAFAFU" varchar(1) NOT NULL default ' ',
  "AAAAFAFV" varchar(1) NOT NULL default ' ',
  "AAAAFAFW" varchar(1) NOT NULL default ' ',
  "AAAAFAFX" varchar(1) NOT NULL default ' ',
  "AAAAFAFY" varchar(1) NOT NULL default ' ',
  "AAAAFAFZ" varchar(3) NOT NULL default ' ',
  "AAAAFAGA" varchar(1) NOT NULL default ' ',
  "AAAAFAGB" varchar(1) NOT NULL default ' ',
  "AAAAFAGC" varchar(4) NOT NULL default ' ',
  "AAAAFAGD" varchar(2) NOT NULL default ' ',
  "AAAAFAGE" varchar(1) NOT NULL default ' ',
  "AAAAFAGF" varchar(1) NOT NULL default ' ',
  "AAAAVODP" varchar(1) NOT NULL default ' ',
  "AAAAFAGH" varchar(1) NOT NULL default ' ',
  "AAAAFAGI" varchar(1) NOT NULL default ' ',
  "AAAAFAGJ" varchar(10) NOT NULL default ' ',
  "AAABJOTY" varchar(10) NOT NULL default ' ',
  "AAAAFAGL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFAGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFAGO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAAAFAGR" varchar(40) NOT NULL default ' ',
  "AAAAFAGS" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMODI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFAGV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAAAFAGY" varchar(1) NOT NULL default ' ',
  "AAAAVXRE" varchar(1) NOT NULL default ' ',
  "AAABKRQO" varchar(1) NOT NULL default ' ',
  "AAABDPJP" varchar(1) NOT NULL default ' ',
  "AAAAFAHC" varchar(1) NOT NULL default ' ',
  "AAABKRQP" varchar(1) NOT NULL default ' ',
  "AAABDPJN" varchar(1) NOT NULL default ' ',
  "AAABKRPC" varchar(1) NOT NULL default ' ',
  "AAABHPNU" varchar(1) NOT NULL default ' ',
  "AAABKRTQ" varchar(2) NOT NULL default ' ',
  "AAABDPJE" varchar(1) NOT NULL default ' ',
  "AAABDPJG" varchar(1) NOT NULL default ' ',
  "AAABKRPW" varchar(1) NOT NULL default ' ',
  "AAABDPJC" varchar(1) NOT NULL default ' ',
  "AAABKRQJ" varchar(1) NOT NULL default ' ',
  "AAABDTOE" varchar(1) NOT NULL default ' ',
  "AAAAFAHO" varchar(1) NOT NULL default ' ',
  "AAABDTOK" varchar(1) NOT NULL default ' ',
  "AAAAFAHQ" varchar(1) NOT NULL default ' ',
  "AAAAFAHR" tinyint(3) unsigned NOT NULL default '0',
  "AAAAFAHS" varchar(1) NOT NULL default ' ',
  "AAAAFAHT" smallint(6) NOT NULL default '0',
  "AAAAFAHU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFAHX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCAB" varchar(20) NOT NULL default ' ',
  "AAABKMQV" varchar(2) NOT NULL default '00',
  "AAABLROG" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJCAB","AAABKMQV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFAIF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCAB" varchar(20) NOT NULL default ' ',
  "AAAAUZUG" varchar(20) NOT NULL default ' ',
  "AAAAFAIJ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABJCAB","AAAAUZUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFAIN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(20) NOT NULL default ' ',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFAIT" (
  "AAABJNHG" varchar(30) NOT NULL default ' ',
  "AAABKMJV" varchar(30) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAAFAIY" varchar(30) NOT NULL default ' ',
  "AAABIHMA" varchar(3) NOT NULL default ' ',
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABJNHG","AAABKMJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFAJD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFAJN" varchar(10) NOT NULL default ' ',
  "AAABHPYD" varchar(10) NOT NULL default ' ',
  "AAABGITH" varchar(2) NOT NULL default ' ',
  "AAABKRDV" varchar(4) NOT NULL default ' ',
  "AAABLHMG" varchar(2) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLURE" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFAJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFAJO" (
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAABMIRY" varchar(1) NOT NULL default ' ',
  "AAAAFAJR" varchar(1) NOT NULL default ' ',
  "AAAAFAJS" varchar(1) NOT NULL default ' ',
  "AAAAFAJT" varchar(1) NOT NULL default ' ',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMIRX","AAABMIRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFAJZ" (
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAABEBZN" varchar(3) NOT NULL default ' ',
  "AAABFWLC" varchar(8) NOT NULL default '00000000',
  "AAAAUPYD" varchar(6) NOT NULL default '000000',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABHZCZ" int(11) NOT NULL default '0',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABILWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFAKJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABILWI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAABILWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFAKQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAANAVQ" varchar(16) NOT NULL default ' ',
  "AAABKGFD" varchar(8) NOT NULL default '00000000',
  "AAAAHFQY" varchar(2) NOT NULL default ' ',
  "AAAAHFQZ" varchar(2) NOT NULL default ' ',
  "AAAAHFRA" varchar(2) NOT NULL default ' ',
  "AAAAHFRB" varchar(2) NOT NULL default ' ',
  "AAAAHFRC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAAANAVQ","AAABKGFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFALE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABJEXI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFALK" (
  "AAABGBCR" varchar(32) NOT NULL default ' ',
  "AAABGBCT" varchar(32) NOT NULL default ' ',
  "AAABKNBT" varchar(10) NOT NULL default ' ',
  "AAABGBCV" varchar(3) NOT NULL default ' ',
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAABHWUC" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABGBCR","AAABGBCT","AAABKNBT","AAABGBCV"),
  UNIQUE KEY "AAAAFALV" ("AAABILWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFALX" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAAAFAMA" varchar(1) NOT NULL default ' ',
  "AAAAFAMB" varchar(1) NOT NULL default ' ',
  "AAAAFAMC" varchar(1) NOT NULL default ' ',
  "AAAAFAMD" varchar(1) NOT NULL default ' ',
  "AAAAFAME" varchar(1) NOT NULL default ' ',
  "AAAAFAMF" varchar(1) NOT NULL default ' ',
  "AAAAFAMG" varchar(8) NOT NULL default '00000000',
  "AAAAFAMH" varchar(1) NOT NULL default ' ',
  "AAAAFAMI" varchar(1) NOT NULL default ' ',
  "AAABDFYR" varchar(3) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABHEYG" varchar(20) NOT NULL default ' ',
  "AAABJEXI" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFAMO" (
  "AAABGBCR" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABGBCR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFAMR" (
  "AAABGBCR" varchar(32) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHWUC" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABGBCR","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFAMX" (
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAABINFD" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAAAFANP" varchar(20) NOT NULL default ' ',
  "AAAAFANQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABILWI","AAABINFD","AAABLUXQ","AAAAFANP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFANH" (
  "AAAAFANP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFANP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFANK" (
  "AAAAFANP" varchar(20) NOT NULL default ' ',
  "AAAAFANQ" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHWUC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFANP","AAAAFANQ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFANS" (
  "AAAALTVY" varchar(20) NOT NULL default ' ',
  "AAAAFAOE" varchar(20) NOT NULL default ' ',
  "AAAAXVXJ" varchar(3) NOT NULL default ' ',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAAAFAOH" varchar(10) NOT NULL default ' ',
  "AAAAFANY" varchar(30) NOT NULL default ' ',
  "AAAALTVR" varchar(30) NOT NULL default ' ',
  "AAAAFAOA" varchar(30) NOT NULL default ' ',
  "AAAAFAPD" varchar(8) NOT NULL default '00000000',
  "AAAAFAPE" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAALTVY","AAAAFAOE","AAAAXVXJ","AAABLMNC","AAAAFAOH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFAOI" (
  "AAAAFAPF" varchar(20) NOT NULL default ' ',
  "AAAAFAPG" varchar(20) NOT NULL default ' ',
  "AAAAFAOL" varchar(1) NOT NULL default ' ',
  "AAAAFAPD" varchar(8) NOT NULL default '00000000',
  "AAAAFAPE" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAAFAPF","AAAAFAPG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFAOQ" (
  "AAAAFAPF" varchar(20) NOT NULL default ' ',
  "AAAAFAPG" varchar(20) NOT NULL default ' ',
  "AAABMLPO" varchar(10) NOT NULL default '0000000000',
  "AAAAFAOU" varchar(1) NOT NULL default ' ',
  "AAAAFAOV" varchar(3) NOT NULL default ' ',
  "AAAAFAOW" varchar(20) NOT NULL default ' ',
  "AAAAFAOX" varchar(6) NOT NULL default ' ',
  "AAAAFAOY" varchar(2) NOT NULL default ' ',
  "AAAAFAOZ" varchar(20) NOT NULL default ' ',
  "AAAAFAPA" varchar(20) NOT NULL default ' ',
  "AAAAFAPB" varchar(30) NOT NULL default ' ',
  "AAAAFAPC" varchar(30) NOT NULL default ' ',
  "AAAAFAPD" varchar(8) NOT NULL default '00000000',
  "AAAAFAPE" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAAFAPF","AAAAFAPG","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFAPI" (
  "AAAAPIZL" varchar(20) NOT NULL default ' ',
  "AAAAPIZK" varchar(20) NOT NULL default ' ',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABGCBE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAAPIZL","AAAAPIZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFAPP" (
  "AAABGIWK" varchar(5) NOT NULL default ' ',
  "AAABINFD" varchar(10) NOT NULL default ' ',
  "AAAAVNVP" varchar(10) NOT NULL default '0000000000',
  "AAABMLZS" varchar(6) NOT NULL default '000000',
  "AAAAVFGO" varchar(1) NOT NULL default '0',
  "AAABBKDT" varchar(1) NOT NULL default ' ',
  "AAABLUUW" varchar(1) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABGIWK","AAABINFD","AAAAVNVP","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFAQH" (
  "AAABMLPN" varchar(10) NOT NULL default ' ',
  "AAABINFD" varchar(10) NOT NULL default ' ',
  "AAAAVNVP" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMLPN","AAABINFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFAQN" (
  "AAABINFD" varchar(10) NOT NULL default ' ',
  "AAAAVNVP" varchar(10) NOT NULL default '0000000000',
  "AAABLFZO" varchar(60) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABHHSC" varchar(12) NOT NULL default ' ',
  "AAAAFAQT" varchar(10) NOT NULL default ' ',
  "AAAAFAQU" varchar(3) NOT NULL default '000',
  "AAAAFAQV" varchar(8) NOT NULL default '00000000',
  "AAAAFAQW" varchar(12) NOT NULL default ' ',
  "AAAAVNVD" varchar(1) NOT NULL default ' ',
  "AAAAWBOP" varchar(12) NOT NULL default '000000000000',
  PRIMARY KEY ("AAABINFD","AAAAVNVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFARB" (
  "AAABINFD" varchar(10) NOT NULL default ' ',
  "AAAAVNVP" varchar(10) NOT NULL default '0000000000',
  "AAABHEYG" varchar(20) NOT NULL default ' ',
  "AAABGXDK" varchar(1) NOT NULL default ' ',
  "AAABGCBE" varchar(60) NOT NULL default ' ',
  "AAABJEXI" varchar(20) NOT NULL default ' ',
  "AAABGBDL" varchar(1) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAABMLZS" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABINFD","AAAAVNVP","AAABHEYG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFARO" (
  "AAABINFD" varchar(10) NOT NULL default ' ',
  "AAAAVNVP" varchar(10) NOT NULL default '0000000000',
  "AAABMLZS" varchar(6) NOT NULL default '000000',
  "AAABLECQ" varchar(10) NOT NULL default ' ',
  "AAABEBZN" varchar(3) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAAAFARV" varchar(1) NOT NULL default ' ',
  "AAAAFARW" varchar(1) NOT NULL default ' ',
  "AAAAFARX" varchar(1) NOT NULL default ' ',
  "AAAAFARY" varchar(1) NOT NULL default ' ',
  "AAAAFARZ" varchar(1) NOT NULL default ' ',
  "AAAAFASA" varchar(1) NOT NULL default ' ',
  "AAAAFASB" varchar(1) NOT NULL default ' ',
  "AAABFVER" varchar(1) NOT NULL default ' ',
  "AAAAFASD" varchar(1) NOT NULL default ' ',
  "AAABLRTV" varchar(8) NOT NULL default '00000000',
  "AAABLRTW" varchar(6) NOT NULL default '000000',
  "AAABGBVR" varchar(32) NOT NULL default ' ',
  "AAAAFASH" varchar(1) NOT NULL default ' ',
  "AAABHEYE" varchar(4) NOT NULL default ' ',
  "AAABGHGS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFD","AAAAVNVP","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFASN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLROG" varchar(20) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLROG","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFASV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUZUG" varchar(20) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUZUG","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFATD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(20) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(20) NOT NULL default ' ',
  "AAABMDXM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFATM" (
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABLVJB" varchar(20) NOT NULL default ' ',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAHLZI" varchar(40) NOT NULL default ' ',
  "AAAAMTOD" varchar(40) NOT NULL default ' ',
  "AAAATKJP" varchar(60) NOT NULL default ' ',
  "AAABIFOK" varchar(4) NOT NULL default ' ',
  "AAABIFOL" varchar(30) NOT NULL default ' ',
  "AAABGAMR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNYT","AAABLVJB","AAABLSCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFATZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGIHK" varchar(30) NOT NULL default ' ',
  "AAABDRQC" varchar(50) NOT NULL default ' ',
  "AAABMLEL" varchar(4) NOT NULL default '0000',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABLXUH" varchar(10) NOT NULL default '0000000000',
  "AAABHSKS" varchar(1) NOT NULL default ' ',
  "AAABDUFW" varchar(1) NOT NULL default ' ',
  "AAABDUFY" varchar(1) NOT NULL default ' ',
  "AAABDUFZ" varchar(1) NOT NULL default ' ',
  "AAABDUFU" varchar(1) NOT NULL default ' ',
  "AAABDUFV" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAAMSFF" varchar(8) NOT NULL default '00000000',
  "AAAAFAUO" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAAAFAUQ" varchar(12) NOT NULL default ' ',
  "AAABLRYP" varchar(8) NOT NULL default '00000000',
  "AAABMAWF" varchar(8) NOT NULL default '00000000',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABDUFX" varchar(1) NOT NULL default ' ',
  "AAABKOGH" varchar(18) NOT NULL default '000000000000000000',
  "AAAAFAUW" varchar(1) NOT NULL default ' ',
  "AAAAFAUX" varchar(1) NOT NULL default ' ',
  "AAAAFAUY" varchar(1) NOT NULL default ' ',
  "AAAAFAUZ" varchar(1) NOT NULL default ' ',
  "AAAAFAVA" varchar(1) NOT NULL default ' ',
  "AAABANZZ" varchar(1) NOT NULL default ' ',
  "AAAAFAVC" varchar(1) NOT NULL default ' ',
  "AAAAFAVD" varchar(1) NOT NULL default ' ',
  "AAABMEKZ" varchar(10) NOT NULL default ' ',
  "AAAALKTT" varchar(1) NOT NULL default ' ',
  "AAAAVEAR" varchar(8) NOT NULL default '00000000',
  "AAAAVEAS" varchar(12) NOT NULL default ' ',
  "AAAAFAVI" varchar(4) NOT NULL default ' ',
  "AAAAFAVJ" varchar(4) NOT NULL default ' ',
  "AAAAFCRU" varchar(30) NOT NULL default ' ',
  "AAABLMFC" varchar(1) NOT NULL default ' ',
  "AAAAFAVM" varchar(1) NOT NULL default ' ',
  "AAABAKWX" varchar(18) NOT NULL default ' ',
  "AAAAFAVO" varchar(1) NOT NULL default ' ',
  "AAAAFAVP" varchar(1) NOT NULL default ' ',
  "AAAAFAVQ" varchar(1) NOT NULL default ' ',
  "AAAAFAVR" varchar(1) NOT NULL default ' ',
  "AAAAFAVS" varchar(1) NOT NULL default ' ',
  "AAAAFAVT" varchar(1) NOT NULL default ' ',
  "AAAAFAVU" varchar(1) NOT NULL default ' ',
  "AAAAFAVV" varchar(1) NOT NULL default ' ',
  "AAAAFAVW" varchar(1) NOT NULL default ' ',
  "AAAAFAVX" varchar(1) NOT NULL default ' ',
  "AAAAFAVY" varchar(1) NOT NULL default ' ',
  "AAAAFAVZ" varchar(1) NOT NULL default ' ',
  "AAAAFAWA" varchar(1) NOT NULL default ' ',
  "AAAAFAWB" varchar(1) NOT NULL default ' ',
  "AAAAFAWC" varchar(1) NOT NULL default ' ',
  "AAAAMTRU" varchar(1) NOT NULL default ' ',
  "AAAAFAWE" varchar(1) NOT NULL default ' ',
  "AAAAFAWF" varchar(1) NOT NULL default ' ',
  "AAABFDQR" varchar(1) NOT NULL default ' ',
  "AAAAFAWH" varchar(1) NOT NULL default ' ',
  "AAAAFAWI" varchar(1) NOT NULL default ' ',
  "AAAAFAWJ" varchar(1) NOT NULL default ' ',
  "AAAAFAWK" varchar(1) NOT NULL default ' ',
  "AAAAFAWL" varchar(1) NOT NULL default ' ',
  "AAAAFAWM" varchar(1) NOT NULL default ' ',
  "AAAAFAWN" varchar(1) NOT NULL default ' ',
  "AAAAFAWO" varchar(1) NOT NULL default ' ',
  "AAAAFAWP" varchar(1) NOT NULL default ' ',
  "AAAAFAWQ" varchar(1) NOT NULL default ' ',
  "AAAAFAWR" varchar(1) NOT NULL default ' ',
  "AAAAFAZA" varchar(18) NOT NULL default '000000000000000000',
  "AAAAFAWT" varchar(1) NOT NULL default ' ',
  "AAAAFAWU" varchar(1) NOT NULL default ' ',
  "AAAAFAWV" varchar(1) NOT NULL default ' ',
  "AAAAFAWW" varchar(1) NOT NULL default ' ',
  "AAAAFAWX" varchar(1) NOT NULL default ' ',
  "AAAAFAWY" varchar(1) NOT NULL default ' ',
  "AAAAFAWZ" varchar(1) NOT NULL default ' ',
  "AAAAFAXA" varchar(1) NOT NULL default ' ',
  "AAAAKSXB" varchar(1) NOT NULL default ' ',
  "AAAAFAXC" varchar(1) NOT NULL default ' ',
  "AAAAFAXD" varchar(1) NOT NULL default ' ',
  "AAAAFAXE" varchar(1) NOT NULL default ' ',
  "AAAAFAXF" varchar(1) NOT NULL default ' ',
  "AAAAFAXG" varchar(1) NOT NULL default ' ',
  "AAAAFAXH" varchar(1) NOT NULL default ' ',
  "AAAAFAXI" varchar(1) NOT NULL default ' ',
  "AAAAFAXJ" varchar(1) NOT NULL default ' ',
  "AAAAFAXK" varchar(1) NOT NULL default ' ',
  "AAAAFAXL" varchar(1) NOT NULL default ' ',
  "AAAAFAXM" varchar(1) NOT NULL default ' ',
  "AAAAFAXN" varchar(1) NOT NULL default ' ',
  "AAABGBRU" varchar(1) NOT NULL default ' ',
  "AAABKNBW" varchar(1) NOT NULL default ' ',
  "AAAAFAXQ" varchar(1) NOT NULL default ' ',
  "AAAAFAXR" varchar(1) NOT NULL default ' ',
  "AAAAFAXS" varchar(1) NOT NULL default ' ',
  "AAAAFAXT" varchar(1) NOT NULL default ' ',
  "AAAAFAXU" varchar(1) NOT NULL default ' ',
  "AAAAFAXV" varchar(20) NOT NULL default ' ',
  "AAAAFAXW" varchar(10) NOT NULL default ' ',
  "AAAAFAXX" varchar(10) NOT NULL default ' ',
  "AAAAFAXY" varchar(10) NOT NULL default ' ',
  "AAAAFAXZ" varchar(10) NOT NULL default ' ',
  "AAAAFAYA" varchar(10) NOT NULL default ' ',
  "AAAAFAYB" varchar(3) NOT NULL default ' ',
  "AAAAFAYC" varchar(3) NOT NULL default ' ',
  "AAAAFAYD" varchar(3) NOT NULL default ' ',
  "AAAAFAYE" varchar(3) NOT NULL default ' ',
  "AAAAFAYF" varchar(3) NOT NULL default ' ',
  "AAAAFAYG" varchar(1) NOT NULL default ' ',
  "AAAAFAYH" varchar(1) NOT NULL default ' ',
  "AAABMALF" varchar(2) NOT NULL default '00',
  "AAAAFAYJ" varchar(1) NOT NULL default ' ',
  "AAAAMPZX" varchar(1) NOT NULL default ' ',
  "AAAAFAYL" varchar(1) NOT NULL default ' ',
  "AAAAFAYM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGIHK","AAABDRQC","AAABMLEL"),
  KEY "AAAAFAYR" ("AAABKRQD"),
  KEY "AAAAFAYT" ("AAABLXUH"),
  KEY "AAAAFAYV" ("AAABAKWX"),
  KEY "AAAAFAYX" ("AAABKOGH"),
  KEY "AAAAFAYZ" ("AAAAFAZA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFAZB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFDCZ" varchar(10) NOT NULL default '0000000000',
  "AAAAFBEM" varchar(4) NOT NULL default '0000',
  "AAAAFBEH" smallint(6) NOT NULL default '0',
  "AAAAFBEI" text,
  PRIMARY KEY ("AAABMRIL","AAAAFDCZ","AAAAFBEM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFAZK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFDCZ" varchar(10) NOT NULL default '0000000000',
  "AAAATMCS" varchar(4) NOT NULL default '0000',
  "AAABKOGH" varchar(18) NOT NULL default '000000000000000000',
  "AAAAFAZY" varchar(4) NOT NULL default '0000',
  "AAAAFBLN" varchar(32) NOT NULL default ' ',
  "AAABGKZH" varchar(1) NOT NULL default ' ',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABLRYP" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAFDCZ","AAAATMCS","AAABKOGH","AAAAFAZY","AAAAFBLN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBAA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFDCZ" varchar(10) NOT NULL default '0000000000',
  "AAABDHCH" varchar(4) NOT NULL default '0000',
  "AAABKOGH" varchar(18) NOT NULL default '000000000000000000',
  "AAAAFBEM" varchar(4) NOT NULL default '0000',
  "AAAAFBAG" varchar(1) NOT NULL default ' ',
  "AAABGKZH" varchar(1) NOT NULL default ' ',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABLRYP" varchar(8) NOT NULL default '00000000',
  "AAAAFBEH" smallint(6) NOT NULL default '0',
  "AAAAFBEI" text,
  PRIMARY KEY ("AAABMRIL","AAAAFDCZ","AAABDHCH","AAABKOGH","AAAAFBEM"),
  KEY "AAAAFBAR" ("AAABMRIL","AAABKOGH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBAU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXUH" varchar(10) NOT NULL default '0000000000',
  "AAAAFBWX" varchar(30) NOT NULL default ' ',
  "AAAAFBWZ" varchar(4) NOT NULL default '0000',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABDHCH" varchar(4) NOT NULL default '0000',
  "AAABLRYP" varchar(8) NOT NULL default '00000000',
  "AAABMMRH" int(11) NOT NULL default '0',
  "AAAAFFSV" varchar(1) NOT NULL default ' ',
  "AAAAFBWA" int(11) NOT NULL default '0',
  "AAAAFBWC" int(11) NOT NULL default '0',
  "AAAAYXBJ" int(11) NOT NULL default '0',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  "AAAALUCS" varchar(10) NOT NULL default '0000000000',
  "AAAAFBBJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXUH","AAAAFBWX","AAAAFBWZ","AAABLXIY","AAABDHCH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBBQ" (
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAAAFBBS" varchar(4) NOT NULL default ' ',
  "AAAAPWZA" varchar(1) NOT NULL default ' ',
  "AAAAFBBU" varchar(1) NOT NULL default ' ',
  "AAAAFBBV" varchar(1) NOT NULL default ' ',
  "AAAAFBBW" varchar(1) NOT NULL default ' ',
  "AAAAFBBX" varchar(1) NOT NULL default ' ',
  "AAAAFBBY" varchar(1) NOT NULL default ' ',
  "AAAAFBBZ" varchar(1) NOT NULL default ' ',
  "AAAAFBCA" varchar(1) NOT NULL default ' ',
  "AAABGGJQ" varchar(1) NOT NULL default ' ',
  "AAAAFBCC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBCE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFDCZ" varchar(10) NOT NULL default '0000000000',
  "AAABDHCH" varchar(4) NOT NULL default '0000',
  "AAABDCWS" varchar(30) NOT NULL default ' ',
  "AAABDUAM" varchar(4) NOT NULL default ' ',
  "AAAAFBCK" varchar(1) NOT NULL default ' ',
  "AAAAFBCL" varchar(1) NOT NULL default ' ',
  "AAAAFBCM" varchar(4) NOT NULL default '0000',
  "AAAAFBCN" varchar(4) NOT NULL default '0000',
  "AAAAFBCO" varchar(1) NOT NULL default ' ',
  "AAABDVAH" varchar(1) NOT NULL default ' ',
  "AAAAFBCQ" varchar(8) NOT NULL default '00000000',
  "AAAAFBCR" varchar(12) NOT NULL default ' ',
  "AAAAFBCS" varchar(8) NOT NULL default '00000000',
  "AAAAFBCT" varchar(12) NOT NULL default ' ',
  "AAABDUCI" varchar(10) NOT NULL default ' ',
  "AAABDTZJ" varchar(3) NOT NULL default ' ',
  "AAABGKZH" varchar(1) NOT NULL default ' ',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABLRYP" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAFDCZ","AAABDHCH"),
  KEY "AAAAFBDC" ("AAABMRIL","AAABDCWS"),
  KEY "AAAAFBDF" ("AAABMRIL","AAABDUCI"),
  KEY "AAAAFBDI" ("AAABMRIL","AAABKRQD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBDL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFDCZ" varchar(10) NOT NULL default '0000000000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDHCH" varchar(4) NOT NULL default '0000',
  "AAAAFBDQ" varchar(30) NOT NULL default ' ',
  "AAAAFBDR" varchar(1) NOT NULL default ' ',
  "AAAAFBDS" varchar(1) NOT NULL default ' ',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABLRYP" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAFDCZ","AAABMRHS","AAABDHCH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBDZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFDCZ" varchar(10) NOT NULL default '0000000000',
  "AAABDHCH" varchar(4) NOT NULL default '0000',
  "AAAAFBEM" varchar(4) NOT NULL default '0000',
  "AAABGKZH" varchar(1) NOT NULL default ' ',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABLRYP" varchar(8) NOT NULL default '00000000',
  "AAAAFBEH" smallint(6) NOT NULL default '0',
  "AAAAFBEI" text,
  PRIMARY KEY ("AAABMRIL","AAAAFDCZ","AAABDHCH","AAAAFBEM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBEN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATLQT" varchar(10) NOT NULL default ' ',
  "AAABKOGH" varchar(18) NOT NULL default '000000000000000000',
  "AAAAFDCZ" varchar(10) NOT NULL default '0000000000',
  "AAABDHCH" varchar(4) NOT NULL default '0000',
  "AAAAFBET" varchar(4) NOT NULL default '0000',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABGKZH" varchar(1) NOT NULL default ' ',
  "AAABLRYP" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAATLQT","AAABKOGH","AAAAFDCZ","AAABDHCH"),
  KEY "AAAAFBFC" ("AAABMRIL","AAABKOGH"),
  KEY "AAAAFBFF" ("AAABMRIL","AAABKRQD"),
  KEY "AAAAFBFI" ("AAABMRIL","AAAAFDCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBFL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABIMRR" varchar(6) NOT NULL default '000000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLXUJ" varchar(1) NOT NULL default ' ',
  "AAABLXQQ" varchar(8) NOT NULL default ' ',
  "AAABKRTS" varchar(8) NOT NULL default '00000000',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABLXVN" varchar(12) NOT NULL default ' ',
  "AAABLRBI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTVR"(255)),
  KEY "AAAAFBGA" ("AAABMQIL","AAABIMRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBGD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXUJ" varchar(1) NOT NULL default ' ',
  "AAABLXQQ" varchar(8) NOT NULL default ' ',
  "AAABLXUK" varchar(8) NOT NULL default '00000000',
  "AAABLXUL" varchar(8) NOT NULL default '00000000',
  "AAAAFBGJ" varchar(10) NOT NULL default ' ',
  "AAAAFBGK" varchar(1) NOT NULL default ' ',
  "AAAAFBGL" varchar(1) NOT NULL default ' ',
  "AAAAFBGM" varchar(1) NOT NULL default ' ',
  "AAAAFBGN" varchar(1) NOT NULL default ' ',
  "AAAAZKHE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXUJ","AAABLXQQ","AAABLXUK","AAABLXUL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBGU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFBKP" varchar(10) NOT NULL default '0000000000',
  "AAAATMCS" varchar(4) NOT NULL default '0000',
  "AAABKOGH" varchar(18) NOT NULL default '000000000000000000',
  "AAAAFBGZ" varchar(6) NOT NULL default '000000',
  "AAAAFBHA" varchar(6) NOT NULL default '000000',
  "AAAAFBHB" varchar(6) NOT NULL default '000000',
  "AAABGKZH" varchar(1) NOT NULL default ' ',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABLRYP" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAFBKP","AAAATMCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBHI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFBKP" varchar(10) NOT NULL default '0000000000',
  "AAAATMCS" varchar(4) NOT NULL default '0000',
  "AAAAFBJN" varchar(10) NOT NULL default '0000000000',
  "AAAAFBHU" varchar(4) NOT NULL default '0000',
  "AAAAFBHO" smallint(6) NOT NULL default '0',
  "AAAAFBHP" text,
  PRIMARY KEY ("AAABMRIL","AAAAFBKP","AAAATMCS","AAAAFBJN","AAAAFBHU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBHV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFBKP" varchar(10) NOT NULL default '0000000000',
  "AAAATMCS" varchar(4) NOT NULL default '0000',
  "AAAAFBJM" varchar(6) NOT NULL default '000000',
  "AAAAFBIA" varchar(4) NOT NULL default ' ',
  "AAAATLNE" varchar(6) NOT NULL default '000000',
  "AAAATLNF" varchar(6) NOT NULL default '000000',
  "AAAAFBID" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAAAFBKP","AAAATMCS","AAAAFBJM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBII" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFBKP" varchar(10) NOT NULL default '0000000000',
  "AAAATMCS" varchar(4) NOT NULL default '0000',
  "AAAAFBJM" varchar(6) NOT NULL default '000000',
  "AAAAFBKS" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAAAFBKP","AAAATMCS","AAAAFBJM","AAAAFBKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBIT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFBKP" varchar(10) NOT NULL default '0000000000',
  "AAAATMCS" varchar(4) NOT NULL default '0000',
  "AAAAFBJE" varchar(6) NOT NULL default '000000',
  "AAABLLIO" varchar(30) NOT NULL default ' ',
  "AAABEYCZ" varchar(3) NOT NULL default ' ',
  "AAABJTNV" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFBKP","AAAATMCS","AAAAFBJE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBJF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFBKP" varchar(10) NOT NULL default '0000000000',
  "AAAATMCS" varchar(4) NOT NULL default '0000',
  "AAAAFBJS" varchar(10) NOT NULL default '0000000000',
  "AAAAFBJT" varchar(4) NOT NULL default '0000',
  "AAAATLOL" varchar(4) NOT NULL default '0000',
  "AAAAFBJM" varchar(6) NOT NULL default '000000',
  "AAAAFBJN" varchar(10) NOT NULL default '0000000000',
  "AAABMALF" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAFBKP","AAAATMCS","AAAAFBJS","AAAAFBJT","AAAATLOL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBJV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFBKP" varchar(10) NOT NULL default '0000000000',
  "AAAATMCS" varchar(4) NOT NULL default '0000',
  "AAAAFBKR" varchar(6) NOT NULL default '000000',
  "AAAAFBKA" varchar(6) NOT NULL default '000000',
  "AAAAFBKB" varchar(6) NOT NULL default '000000',
  "AAABFYLS" varchar(10) NOT NULL default '0000000000',
  "AAAAFBKD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFBKP","AAAATMCS","AAAAFBKR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBKI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFBKP" varchar(10) NOT NULL default '0000000000',
  "AAAATMCS" varchar(4) NOT NULL default '0000',
  "AAAAFBKR" varchar(6) NOT NULL default '000000',
  "AAAAFBKS" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAAAFBKP","AAAATMCS","AAAAFBKR","AAAAFBKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFDCZ" varchar(10) NOT NULL default '0000000000',
  "AAAATMCS" varchar(4) NOT NULL default '0000',
  "AAABKOGH" varchar(18) NOT NULL default '000000000000000000',
  "AAAAFBLN" varchar(32) NOT NULL default ' ',
  "AAABLLIO" varchar(30) NOT NULL default ' ',
  "AAABEYCZ" varchar(3) NOT NULL default ' ',
  "AAABJTNV" varchar(50) NOT NULL default ' ',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  "AAABAZFT" varchar(1) NOT NULL default ' ',
  "AAAATLQC" varchar(2) NOT NULL default ' ',
  "AAAAFBLF" varchar(4) NOT NULL default '0000',
  "AAABGKZH" varchar(1) NOT NULL default ' ',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABLRYP" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAFDCZ","AAAATMCS","AAABKOGH","AAAAFBLN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBLO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFBNO" VARBINARY(255) NOT NULL,
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABIMRR" varchar(6) NOT NULL default '000000',
  "AAABKRJU" varchar(8) NOT NULL default '00000000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABKQVZ" varchar(1) NOT NULL default ' ',
  "AAABKRTQ" varchar(2) NOT NULL default ' ',
  "AAABKQWF" varchar(4) NOT NULL default ' ',
  "AAABLFDD" varchar(1) NOT NULL default ' ',
  "AAABKWJY" varchar(1) NOT NULL default ' ',
  "AAABJQCL" decimal(15,0) NOT NULL default '0',
  "AAABJQCM" varchar(12) NOT NULL default ' ',
  "AAABEDIH" decimal(15,0) NOT NULL default '0',
  "AAAAFBNL" varchar(12) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFBNO"(255)),
  KEY "AAAAFBMI" ("AAABMRIL","AAABMQIL","AAABIMRR","AAABMLCZ","AAABMCVI"),
  KEY "AAAAFBMO" ("AAABMRIL","AAABKRJU","AAABMLCZ","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBMT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFBNO" VARBINARY(255) NOT NULL,
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABKQWF" varchar(4) NOT NULL default ' ',
  "AAABKQVZ" varchar(1) NOT NULL default ' ',
  "AAABKRTQ" varchar(2) NOT NULL default ' ',
  "AAABKQWU" varchar(1) NOT NULL default ' ',
  "AAABKQWS" varchar(8) NOT NULL default ' ',
  "AAAAMOXE" varchar(2) NOT NULL default ' ',
  "AAAAFBNE" varchar(1) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLFDD" varchar(1) NOT NULL default ' ',
  "AAABKWJY" varchar(1) NOT NULL default ' ',
  "AAABJQCL" decimal(15,0) NOT NULL default '0',
  "AAABJQCM" varchar(12) NOT NULL default ' ',
  "AAABEDIH" decimal(15,0) NOT NULL default '0',
  "AAAAFBNL" varchar(12) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFBNO"(255)),
  KEY "AAAAFBNP" ("AAABMRIL","AAABMLCZ","AAABMCVI","AAABKQWF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBNU" (
  "AAABHEHZ" varchar(4) NOT NULL default ' ',
  "AAABHEIC" varchar(60) NOT NULL default ' ',
  "AAAAFBQQ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEHZ","AAABHEIC","AAAAFBQQ"),
  KEY "AAAAFBOB" ("AAAAFBQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBOD" (
  "AAABHEHZ" varchar(4) NOT NULL default ' ',
  "AAABHEIC" varchar(60) NOT NULL default ' ',
  "AAAAFBOG" varchar(4) NOT NULL default ' ',
  "AAAAFBOH" varchar(4) NOT NULL default ' ',
  "AAAAFBOI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEHZ","AAABHEIC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBOL" (
  "AAABJJLG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABJJLG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBOO" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJJLG" varchar(4) NOT NULL default ' ',
  "AAABKZAW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABJJLG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBOU" (
  "AAABJJLG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABJJLG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBOX" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJJLG" varchar(4) NOT NULL default ' ',
  "AAABKZAW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABJJLG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBPD" (
  "AAABJJLG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABJJLG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBPG" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJJLG" varchar(4) NOT NULL default ' ',
  "AAABKZAW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABJJLG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBPM" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJEXI" varchar(3) NOT NULL default '000',
  "AAABHEIG" varchar(12) NOT NULL default ' ',
  "AAAAFBQC" varchar(1) NOT NULL default ' ',
  "AAAAFBQD" varchar(1) NOT NULL default ' ',
  "AAAAFBQE" varchar(2) NOT NULL default '00',
  "AAAAFBQF" varchar(1) NOT NULL default ' ',
  "AAAAFBQG" varchar(1) NOT NULL default ' ',
  "AAAAFBPV" varchar(1) NOT NULL default ' ',
  "AAAAFBPW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBPY" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJEXI" varchar(8) NOT NULL default ' ',
  "AAABHEIG" varchar(32) NOT NULL default ' ',
  "AAAAFBQC" varchar(1) NOT NULL default ' ',
  "AAAAFBQD" varchar(1) NOT NULL default ' ',
  "AAAAFBQE" varchar(2) NOT NULL default '00',
  "AAAAFBQF" varchar(1) NOT NULL default ' ',
  "AAAAFBQG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBQI" (
  "AAABHEHZ" varchar(4) NOT NULL default ' ',
  "AAABHEIC" varchar(60) NOT NULL default ' ',
  "AAAAFBRG" varchar(3) NOT NULL default ' ',
  "AAABLEHK" varchar(1) NOT NULL default ' ',
  "AAABGYNX" varchar(4) NOT NULL default ' ',
  "AAABIYCS" varchar(1) NOT NULL default '0',
  "AAABLETI" varchar(1) NOT NULL default '0',
  "AAAAFBQQ" varchar(1) NOT NULL default ' ',
  "AAAAFBQR" varchar(1) NOT NULL default ' ',
  "AAAAUUZZ" varchar(1) NOT NULL default ' ',
  "AAABJAAI" varchar(1) NOT NULL default ' ',
  "AAAAFBQU" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABHEHZ","AAABHEIC"),
  KEY "AAAAFBQX" ("AAABIYCS"),
  KEY "AAAAFBQZ" ("AAABLEHK"),
  KEY "AAAAFBRB" ("AAABLETI"),
  KEY "AAAAFBRD" ("AAAAUUZZ"),
  KEY "AAAAFBRF" ("AAAAFBRG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBRH" (
  "AAABHEHZ" varchar(4) NOT NULL default ' ',
  "AAABHEIC" varchar(60) NOT NULL default ' ',
  "AAABLYTE" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEHZ","AAABHEIC","AAABLYTE"),
  KEY "AAAAFBRO" ("AAABLYTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBRQ" (
  "AAABHEHZ" varchar(4) NOT NULL default ' ',
  "AAABHEIC" varchar(30) NOT NULL default ' ',
  "AAAATNRH" varchar(8) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  "AAAAFBRV" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABHEHZ","AAABHEIC","AAAATNRH"),
  KEY "AAAAFBRZ" ("AAAATNRH","AAABJASN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBSC" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHEHZ" varchar(4) NOT NULL default ' ',
  "AAABHEIC" varchar(30) NOT NULL default ' ',
  "AAAATNRH" varchar(8) NOT NULL default ' ',
  "AAABMECK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABHEHZ","AAABHEIC","AAAATNRH"),
  KEY "AAAAFBSM" ("AAABMRHS","AAAATNRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBSP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXUH" varchar(10) NOT NULL default '0000000000',
  "AAAAFBWX" varchar(30) NOT NULL default ' ',
  "AAAAFBWZ" varchar(4) NOT NULL default '0000',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABDHCH" varchar(4) NOT NULL default '0000',
  "AAABLRYP" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAAAWMZD" varchar(3) NOT NULL default '000',
  "AAAAWMZF" varchar(3) NOT NULL default '000',
  "AAAAWMZE" varchar(3) NOT NULL default '000',
  "AAAAWMZG" varchar(3) NOT NULL default '000',
  "AAAAFBTF" varchar(1) NOT NULL default ' ',
  "AAAAFBTG" varchar(1) NOT NULL default ' ',
  "AAABIGAW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXUH","AAAAFBWX","AAAAFBWZ","AAABLXIY","AAABDHCH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBTO" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAFBTQ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBTS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXUH" varchar(10) NOT NULL default '0000000000',
  "AAAAFBWX" varchar(30) NOT NULL default ' ',
  "AAAAFBWZ" varchar(4) NOT NULL default '0000',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDHCH" varchar(4) NOT NULL default '0000',
  "AAABLRYP" varchar(8) NOT NULL default '00000000',
  "AAAAZEDP" varchar(30) NOT NULL default ' ',
  "AAAALRVF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXUH","AAAAFBWX","AAAAFBWZ","AAABLXIY","AAABMLQA","AAABDHCH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBUK" (
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABIJPF" varchar(1) NOT NULL default ' ',
  "AAABCBLU" varchar(10) NOT NULL default ' ',
  "AAABCBLV" varchar(3) NOT NULL default ' ',
  "AAABIOAK" varchar(12) NOT NULL default ' ',
  "AAABIMEK" varchar(8) NOT NULL default '00000000',
  "AAABIMEL" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABIRUN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBUT" (
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABCBJK" varchar(4) NOT NULL default ' ',
  "AAABCBJI" varchar(10) NOT NULL default ' ',
  "AAABCWJS" varchar(3) NOT NULL default '000',
  "AAABCBLU" varchar(10) NOT NULL default ' ',
  "AAABCBLV" varchar(3) NOT NULL default ' ',
  "AAABIOAK" varchar(12) NOT NULL default ' ',
  "AAABIMEK" varchar(8) NOT NULL default '00000000',
  "AAABIMEL" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABIRUN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBVE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAAAFBVI" varchar(1) NOT NULL default ' ',
  "AAAAFBVJ" varchar(1) NOT NULL default ' ',
  "AAABLUVS" varchar(1) NOT NULL default ' ',
  "AAAAFBVL" varchar(1) NOT NULL default ' ',
  "AAAAFBVM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABLSOG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBVQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXUH" varchar(10) NOT NULL default '0000000000',
  "AAAAFBWX" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(40) NOT NULL default ' ',
  "AAAAFBWZ" varchar(4) NOT NULL default '0000',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABDHCH" varchar(4) NOT NULL default '0000',
  "AAABLRYP" varchar(8) NOT NULL default '00000000',
  "AAAAFBVZ" int(11) NOT NULL default '0',
  "AAAAFBWA" int(11) NOT NULL default '0',
  "AAABFSJW" int(11) NOT NULL default '0',
  "AAAAFBWC" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLXUH","AAAAFBWX","AAABKAFO","AAAAFBWZ","AAABLXIY","AAABDHCH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBWK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXUH" varchar(10) NOT NULL default '0000000000',
  "AAAAFBWX" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(40) NOT NULL default ' ',
  "AAAAFBWZ" varchar(4) NOT NULL default '0000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABDHCH" varchar(4) NOT NULL default '0000',
  "AAABLRYP" varchar(8) NOT NULL default '00000000',
  "AAAAFBWU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXUH","AAAAFBWX","AAABKAFO","AAAAFBWZ","AAABMLQA","AAABLXIY","AAABDHCH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBXD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDVAX" varchar(10) NOT NULL default '0000000000',
  "AAAAFBXG" varchar(1) NOT NULL default ' ',
  "AAAAFBXH" varchar(1) NOT NULL default ' ',
  "AAAAFBXI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDVAX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBXL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABIKRH" varchar(10) NOT NULL default '0000000000',
  "AAABIKRI" varchar(10) NOT NULL default '0000000000',
  "AAAAFBXQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBXT" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBYA" (
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
CREATE TABLE "AAAAFBYL" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBYV" (
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
  KEY "AAAAFBZJ" ("AAABMRFO","AAABMLQA","AAABHZDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFBZN" (
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
  KEY "AAAAFCAG" ("AAABLLLJ"),
  KEY "AAAAFCAI" ("AAABLLLH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCAK" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCAR" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAFCBB" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCBF" (
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
  UNIQUE KEY "AAAAFCBS" ("AAABLLNL"),
  KEY "AAAAFCBU" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCBW" (
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
  UNIQUE KEY "AAAAFCCJ" ("AAABLLNL"),
  KEY "AAAAFCCL" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCCN" (
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABLFYJ" varchar(32) NOT NULL default ' ',
  "AAABHZOI" varchar(32) NOT NULL default ' ',
  "AAABHZOJ" varchar(32) NOT NULL default ' ',
  "AAABHZOK" varchar(32) NOT NULL default ' ',
  "AAABLMUZ" varchar(4) NOT NULL default ' ',
  "AAABHZOM" varchar(1) NOT NULL default ' ',
  "AAABHZON" varchar(4) NOT NULL default ' ',
  "AAABHZOO" varchar(4) NOT NULL default ' ',
  "AAABHZOP" varchar(8) NOT NULL default '00000000',
  "AAABHZOQ" varchar(8) NOT NULL default '00000000',
  "AAABHZOR" varchar(1) NOT NULL default ' ',
  "AAABHZQH" varchar(1) NOT NULL default ' ',
  "AAABHZQI" varchar(1) NOT NULL default ' ',
  "AAABHZQJ" varchar(1) NOT NULL default ' ',
  "AAABHZOV" varchar(32) NOT NULL default ' ',
  "AAABHZOW" varchar(32) NOT NULL default ' ',
  "AAABHZOX" varchar(32) NOT NULL default ' ',
  "AAABHZOY" varchar(32) NOT NULL default ' ',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABHZQA" varchar(20) NOT NULL default ' ',
  "AAABHZQB" varchar(10) NOT NULL default ' ',
  "AAABHZQC" varchar(10) NOT NULL default ' ',
  "AAABHZPD" varchar(1) NOT NULL default ' ',
  "AAABHZQF" varchar(20) NOT NULL default ' ',
  "AAABHZQG" varchar(20) NOT NULL default ' ',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  "AAABHZPJ" varchar(4) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABHZPN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEY","AAABJAAI","AAABMRHK","AAABHZRB"),
  KEY "AAAAFCED" ("AAABJAAI"),
  KEY "AAAAFCEF" ("AAABMRHK","AAABHZRB"),
  KEY "AAAAFCEI" ("AAABLFYJ","AAABHZRB"),
  KEY "AAAAFCEL" ("AAABHZOJ"),
  KEY "AAAAFCEN" ("AAABHZOK","AAABHZOR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCEQ" (
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABJCXS" varchar(4) NOT NULL default ' ',
  "AAABHZQN" varchar(70) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABHZPY" varchar(10) NOT NULL default '0000000000',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABHZQA" varchar(20) NOT NULL default ' ',
  "AAABHZQB" varchar(10) NOT NULL default ' ',
  "AAABHZQC" varchar(10) NOT NULL default ' ',
  "AAABHZQD" varchar(1) NOT NULL default ' ',
  "AAABHZQE" varchar(1) NOT NULL default ' ',
  "AAABHZQF" varchar(20) NOT NULL default ' ',
  "AAABHZQG" varchar(20) NOT NULL default ' ',
  "AAABHZQH" varchar(1) NOT NULL default ' ',
  "AAABHZQI" varchar(1) NOT NULL default ' ',
  "AAABHZQJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHK","AAABHZRB","AAABJCXS","AAABHZQN"),
  KEY "AAAAFCFM" ("AAABJCXS","AAABHZQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCFP" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABHZQZ" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLGEY","AAABJAAI","AAABHZQZ","AAABMRHK","AAABHZRB"),
  KEY "AAAAFCGD" ("AAABMRHK","AAABHZRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCGG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" varchar(8) NOT NULL default '00000000',
  "AAABMLPV" varchar(25) NOT NULL default ' ',
  "AAAAVEEL" varchar(10) NOT NULL default ' ',
  "AAABJSTP" varchar(25) NOT NULL default ' ',
  "AAAATWEX" varchar(1) NOT NULL default ' ',
  "AAABIXVF" varchar(3) NOT NULL default '000',
  "AAAAVEEP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCGR" (
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
CREATE TABLE "AAAAFCHD" (
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
  UNIQUE KEY "AAAAFCHR" ("AAABLLNL"),
  KEY "AAAAFCHT" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCHV" (
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
  KEY "AAAAFCIV" ("AAABLLNH","AAABHZJH"),
  KEY "AAAAFCIY" ("AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCJA" (
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
  UNIQUE KEY "AAAAFCJO" ("AAABLLNL"),
  KEY "AAAAFCJQ" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCJS" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAFCKC" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCKG" (
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
  UNIQUE KEY "AAAAFCKT" ("AAABLLNL"),
  KEY "AAAAFCKV" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCKX" (
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
  UNIQUE KEY "AAAAFCLK" ("AAABLLNL"),
  KEY "AAAAFCLM" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCLO" (
  "AAABIZCB" varchar(20) NOT NULL default ' ',
  "AAABGNLC" varchar(20) NOT NULL default ' ',
  "AAAAFCLR" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABIZCB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCLT" (
  "AAABIZCB" varchar(20) NOT NULL default ' ',
  "AAAAUUZZ" varchar(1) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAAAFCOK" decimal(15,0) NOT NULL default '0',
  "AAAAFCOL" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABIZCB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCMC" (
  "AAABIZCB" varchar(20) NOT NULL default ' ',
  "AAABMIRY" varchar(1) NOT NULL default ' ',
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABGKUN" varchar(10) NOT NULL default ' ',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  "AAAAMZWW" varchar(1) NOT NULL default ' ',
  "AAAAFCMK" varchar(2) NOT NULL default '00',
  "AAAAFCML" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIZCB","AAABMIRY","AAABMIRX","AAABMDEP","AAABGKUN"),
  KEY "AAAAFCMR" ("AAABMIRX","AAABMIRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCMU" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIZCB" varchar(20) NOT NULL default ' ',
  "AAABMIRY" varchar(1) NOT NULL default ' ',
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABGKUN" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABIZCB","AAABMIRY","AAABMIRX","AAABMDEP","AAABGKUN"),
  KEY "AAAAFCNI" ("AAABIZCB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCNK" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIZCB" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABIZCB"),
  KEY "AAAAFCNQ" ("AAABIZCB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCNS" (
  "AAABJGYO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABJGYO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCNV" (
  "AAABIOBO" varchar(20) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABIOBO","AAABLEHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCOA" (
  "AAABIOBO" varchar(20) NOT NULL default ' ',
  "AAABGXIV" varchar(12) NOT NULL default ' ',
  "AAABLEHK" varchar(1) NOT NULL default ' ',
  "AAABIYCS" varchar(1) NOT NULL default '0',
  "AAABLETI" varchar(1) NOT NULL default '0',
  "AAABJAAI" varchar(1) NOT NULL default ' ',
  "AAAAFCOT" varchar(1) NOT NULL default ' ',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAAAFCOK" decimal(15,0) NOT NULL default '0',
  "AAAAFCOL" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABIOBO"),
  KEY "AAAAFCON" ("AAABIYCS","AAABLETI"),
  KEY "AAAAFCOQ" ("AAABLEHK"),
  KEY "AAAAFCOS" ("AAAAFCOT"),
  KEY "AAAAFCOU" ("AAABGXIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCOW" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIOBO" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABIOBO"),
  KEY "AAAAFCPC" ("AAABIOBO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCPE" (
  "AAABIOBO" varchar(20) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABKMWI" varchar(2) NOT NULL default '00',
  "AAABLENG" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABIOBO","AAABJAAI","AAABKMWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCPM" (
  "AAABIOBO" varchar(20) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABHGFF" varchar(4) NOT NULL default '0000',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABIOBO","AAABJAAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCPW" (
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMAMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCPZ" (
  "AAABIYCS" varchar(20) NOT NULL default ' ',
  "AAABLRQO" varchar(60) NOT NULL default ' ',
  "AAABLQJP" varchar(20) NOT NULL default ' ',
  "AAAAFCQN" varchar(20) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABIYCS"),
  KEY "AAAAFCQM" ("AAAAFCQN"),
  KEY "AAAAFCQO" ("AAABLQJP"),
  KEY "AAAAFCQQ" ("AAABLRQO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCQS" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIYCS" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABIYCS"),
  KEY "AAAAFCQY" ("AAABIYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCRA" (
  "AAABJZWV" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABJZWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCRD" (
  "AAABHWQS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABHWQS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCRG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAFCRO" varchar(22) NOT NULL default ' ',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABGIHK" varchar(30) NOT NULL default ' ',
  "AAABDRQC" varchar(50) NOT NULL default ' ',
  "AAAAFCRU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAFCRO"),
  UNIQUE KEY "AAAAFCRP" ("AAABLUXQ","AAABLTUX","AAABGIHK","AAABDRQC","AAAAFCRU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCRV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFCVA" varchar(10) NOT NULL default '0000000000',
  "AAABFRLS" varchar(30) NOT NULL default ' ',
  "AAAAFCTE" varchar(10) NOT NULL default ' ',
  "AAAAFCUS" varchar(1) NOT NULL default ' ',
  "AAAAFCSB" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFCVA"),
  KEY "AAAAFCSE" ("AAABMRIL","AAABFRLS"),
  KEY "AAAAFCSH" ("AAABMRIL","AAAAFCTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCSK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFCVA" varchar(10) NOT NULL default '0000000000',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABJFLQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAFCVA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCST" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFCTE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFCTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCSY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFCTE" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAFCTC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFCTE","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCTG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFCVA" varchar(10) NOT NULL default '0000000000',
  "AAAAFCYQ" varchar(4) NOT NULL default '0000',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAAAFCVA","AAAAFCYQ","AAABLUSC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCTP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFCVA" varchar(10) NOT NULL default '0000000000',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  "AAAAFCTT" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAAAFCVA","AAABLUSC"),
  KEY "AAAAFCTX" ("AAABMRIL","AAABLUSC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCUA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFCUS" varchar(1) NOT NULL default ' ',
  "AAAAFCUD" varchar(1) NOT NULL default ' ',
  "AAAAFCUE" varchar(1) NOT NULL default ' ',
  "AAAAFCUF" varchar(1) NOT NULL default ' ',
  "AAAAFCUG" varchar(1) NOT NULL default ' ',
  "AAABHJJK" varchar(1) NOT NULL default ' ',
  "AAABGKPS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFCUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCUL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAFCUS" varchar(1) NOT NULL default ' ',
  "AAAAFCUP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAFCUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCUT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFCVA" varchar(10) NOT NULL default '0000000000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAFCUX" varchar(40) NOT NULL default ' ',
  "AAAAFCVD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFCVA","AAABMRHS"),
  KEY "AAAAFCVC" ("AAAAFCVD","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCVF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFRLS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFRLS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCVK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFCVS" varchar(3) NOT NULL default ' ',
  "AAAAFCVT" varchar(18) NOT NULL default ' ',
  "AAABGKZP" varchar(4) NOT NULL default '0000',
  "AAAAFCVP" varchar(4) NOT NULL default '0000',
  "AAABKOGH" varchar(18) NOT NULL default '000000000000000000',
  PRIMARY KEY ("AAABMRIL","AAAAFCVS","AAAAFCVT","AAABGKZP"),
  KEY "AAAAFCVV" ("AAABKOGH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCVX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFDCE" varchar(10) NOT NULL default '0000000000',
  "AAAAFDCL" varchar(18) NOT NULL default ' ',
  "AAAAFCYH" varchar(10) NOT NULL default ' ',
  "AAAAFCZN" varchar(1) NOT NULL default ' ',
  "AAAAFCWD" varchar(3) NOT NULL default ' ',
  "AAAAFCWE" varchar(3) NOT NULL default ' ',
  "AAAAFCWF" varchar(1) NOT NULL default ' ',
  "AAAAFCWG" varchar(10) NOT NULL default ' ',
  "AAAAFCWU" varchar(16) NOT NULL default ' ',
  "AAAAFCWI" varchar(1) NOT NULL default ' ',
  "AAAAFCWJ" varchar(1) NOT NULL default ' ',
  "AAAAFCWK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFDCE"),
  KEY "AAAAFCWN" ("AAABMRIL","AAAAFDCL"),
  KEY "AAAAFCWQ" ("AAABMRIL","AAAAFCYH"),
  KEY "AAAAFCWT" ("AAAAFCWU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCWV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFDCE" varchar(10) NOT NULL default '0000000000',
  "AAAAFCWY" varchar(12) NOT NULL default ' ',
  "AAAAFCWZ" varchar(8) NOT NULL default '00000000',
  "AAAAFCXA" varchar(12) NOT NULL default ' ',
  "AAAAFCXB" varchar(8) NOT NULL default '00000000',
  "AAAAFCXC" varchar(12) NOT NULL default ' ',
  "AAAAFCXD" varchar(8) NOT NULL default '00000000',
  "AAAAFCXE" varchar(12) NOT NULL default ' ',
  "AAAAFCXF" varchar(12) NOT NULL default ' ',
  "AAAAFCXG" varchar(8) NOT NULL default '00000000',
  "AAAAFCXH" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAAAFDCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCXK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFDCE" varchar(10) NOT NULL default '0000000000',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  "AAAAFCXO" varchar(3) NOT NULL default '000',
  "AAAAFCXP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFDCE","AAABLUSC"),
  KEY "AAAAFCXT" ("AAABMRIL","AAABLUSC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCXW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFCYH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFCYH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCYB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFCYH" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAFCYF" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFCYH","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCYJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFDCE" varchar(10) NOT NULL default '0000000000',
  "AAAAFCYQ" varchar(4) NOT NULL default '0000',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAAAFDCE","AAAAFCYQ","AAABLUSC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCYS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFCZN" varchar(1) NOT NULL default ' ',
  "AAAAFCYV" varchar(1) NOT NULL default ' ',
  "AAAAFCYW" varchar(1) NOT NULL default ' ',
  "AAAAFCYX" varchar(1) NOT NULL default ' ',
  "AAAAFCYY" varchar(1) NOT NULL default ' ',
  "AAAAFCYZ" varchar(1) NOT NULL default ' ',
  "AAAAFCZA" varchar(1) NOT NULL default ' ',
  "AAAAFCZB" varchar(10) NOT NULL default ' ',
  "AAAAFCZC" varchar(1) NOT NULL default ' ',
  "AAAAFCZD" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFCZN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCZG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAFCZN" varchar(1) NOT NULL default ' ',
  "AAAAFCZK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAFCZN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFCZO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFDCE" varchar(10) NOT NULL default '0000000000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAYPVY" varchar(40) NOT NULL default ' ',
  "AAAAFCZY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFDCE","AAABMRHS"),
  KEY "AAAAFCZX" ("AAAAFCZY","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDAA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFDCE" varchar(10) NOT NULL default '0000000000',
  "AAAAFDCG" varchar(5) NOT NULL default '00000',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  "AAAAFDBH" varchar(3) NOT NULL default '000',
  "AAABBPIL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFDCE","AAAAFDCG","AAABLUSC","AAAAFDBH"),
  KEY "AAAAFDAM" ("AAABMRIL","AAAAFDCE","AAABLUSC","AAABBPIL","AAAAFDCG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDAS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFDCE" varchar(10) NOT NULL default '0000000000',
  "AAAAFDCG" varchar(5) NOT NULL default '00000',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  "AAAAFDBH" varchar(3) NOT NULL default '000',
  "AAAAUYSF" double NOT NULL default '0',
  "AAAAUYSG" double NOT NULL default '0',
  "AAAAFDBA" varchar(1) NOT NULL default ' ',
  "AAAAFDBB" varchar(3) NOT NULL default ' ',
  "AAAAFDBC" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFDCE","AAAAFDCG","AAABLUSC","AAAAFDBH"),
  KEY "AAAAFDBI" ("AAABMRIL","AAAAFDCE","AAABLUSC","AAAAUYSF","AAAAFDCG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDBO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFDCE" varchar(10) NOT NULL default '0000000000',
  "AAAAFDBY" varchar(5) NOT NULL default '00000',
  "AAAAFDBZ" varchar(4) NOT NULL default '0000',
  "AAAAFDCG" varchar(5) NOT NULL default '00000',
  "AAAAFDCB" varchar(12) NOT NULL default ' ',
  "AAAAFDBV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFDCE","AAAAFDBY","AAAAFDBZ"),
  KEY "AAAAFDCA" ("AAAAFDCB","AAABMRIL"),
  KEY "AAAAFDCD" ("AAAAFDCE","AAABMRIL","AAAAFDCG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDCH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFDCL" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFDCL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDCM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFDCZ" varchar(10) NOT NULL default '0000000000',
  "AAAATMCS" varchar(4) NOT NULL default '0000',
  "AAABLLIO" varchar(30) NOT NULL default ' ',
  "AAABEYCZ" varchar(3) NOT NULL default ' ',
  "AAABJTNV" varchar(50) NOT NULL default ' ',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  "AAABLUPE" varchar(30) NOT NULL default ' ',
  "AAABGKZH" varchar(1) NOT NULL default ' ',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABLRYP" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAFDCZ","AAAATMCS","AAABLLIO","AAABEYCZ","AAABJTNV","AAABLUSC","AAABLUPE"),
  KEY "AAAAFDDG" ("AAABMRIL","AAABLLIO","AAABJTNV"),
  KEY "AAAAFDDK" ("AAABMRIL","AAABLUSC","AAABLUPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDDO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDGIF" varchar(10) NOT NULL default ' ',
  "AAABEAMJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDGIF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDDW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGIF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDGIF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDEB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAFDPC" varchar(3) NOT NULL default ' ',
  "AAAAFDPD" varchar(3) NOT NULL default ' ',
  "AAAAFDEG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAFDPC","AAAAFDPD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDEL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFDPC" varchar(3) NOT NULL default ' ',
  "AAAAFDPD" varchar(3) NOT NULL default ' ',
  "AAAAFDEW" varchar(3) NOT NULL default ' ',
  "AAAAFDOX" varchar(3) NOT NULL default ' ',
  "AAAAFDOY" varchar(3) NOT NULL default ' ',
  "AAABMAYR" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFDPC","AAAAFDPD","AAAAFDEW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDEX" (
  "AAABHLQS" varchar(3) NOT NULL default ' ',
  "AAABMNRG" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABHLQS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDFB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGRRB" varchar(10) NOT NULL default ' ',
  "AAABDGVY" varchar(10) NOT NULL default ' ',
  "AAABEAOJ" varchar(10) NOT NULL default ' ',
  "AAABHLQS" varchar(3) NOT NULL default ' ',
  "AAABJCYJ" varchar(1) NOT NULL default ' ',
  "AAAAFDFI" varchar(240) NOT NULL default ' ',
  "AAAAFDFJ" varchar(3) NOT NULL default ' ',
  "AAAAFDFK" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGRRB","AAABDGVY","AAABEAOJ","AAABHLQS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDFR" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHLQS" varchar(3) NOT NULL default ' ',
  "AAAAFDFU" varchar(240) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHLQS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDFX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFDPC" varchar(3) NOT NULL default ' ',
  "AAAAFDPD" varchar(3) NOT NULL default ' ',
  "AAAAVKAP" varchar(4) NOT NULL default ' ',
  "AAABEAOJ" varchar(10) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
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
  PRIMARY KEY ("AAABMRIL","AAAAFDPC","AAAAFDPD","AAAAVKAP","AAABEAOJ","AAABLEHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDHI" (
  "AAABLESX" varchar(32) NOT NULL default ' ',
  "AAABLESY" varchar(64) NOT NULL default ' ',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABHESL" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLESX","AAABLESY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDHQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGRRB" varchar(10) NOT NULL default ' ',
  "AAABDGVY" varchar(10) NOT NULL default ' ',
  "AAABEAOJ" varchar(10) NOT NULL default ' ',
  "AAAAVKAP" varchar(4) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAAAFDHX" varchar(25) NOT NULL default ' ',
  "AAAAFDHY" varchar(2) NOT NULL default ' ',
  "AAAAFDHZ" varchar(25) NOT NULL default ' ',
  "AAAAFDIA" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGRRB","AAABDGVY","AAABEAOJ","AAAAVKAP","AAABLEHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDIH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFDPC" varchar(3) NOT NULL default ' ',
  "AAAAFDPD" varchar(3) NOT NULL default ' ',
  "AAAAVKAP" varchar(4) NOT NULL default ' ',
  "AAABEAOJ" varchar(10) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABMALF" varchar(4) NOT NULL default '0000',
  "AAAAFDPB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFDPC","AAAAFDPD","AAAAVKAP","AAABEAOJ","AAABLEHK","AAABMALF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDIX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGRRB" varchar(10) NOT NULL default ' ',
  "AAABDGVY" varchar(10) NOT NULL default ' ',
  "AAABEAOJ" varchar(10) NOT NULL default ' ',
  "AAAAFDPC" varchar(3) NOT NULL default ' ',
  "AAAAFDPD" varchar(3) NOT NULL default ' ',
  "AAAAURMM" varchar(10) NOT NULL default ' ',
  "AAABDHQT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGRRB","AAABDGVY","AAABEAOJ","AAAAFDPC","AAAAFDPD","AAAAURMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGRRB" varchar(10) NOT NULL default ' ',
  "AAABDGVY" varchar(10) NOT NULL default ' ',
  "AAABEAOJ" varchar(10) NOT NULL default ' ',
  "AAABMALF" varchar(4) NOT NULL default '0000',
  "AAAAFDPC" varchar(3) NOT NULL default ' ',
  "AAAAFDPD" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGRRB","AAABDGVY","AAABEAOJ","AAABMALF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDKA" (
  "AAABDGVY" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABDGVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDKD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGRRB" varchar(10) NOT NULL default ' ',
  "AAABDGVY" varchar(10) NOT NULL default ' ',
  "AAABEAOJ" varchar(10) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABDHFD" varchar(1) NOT NULL default ' ',
  "AAAAFDQU" varchar(1) NOT NULL default ' ',
  "AAABELAE" varchar(1) NOT NULL default ' ',
  "AAAAFDQW" varchar(1) NOT NULL default ' ',
  "AAAAFDQY" varchar(1) NOT NULL default ' ',
  "AAAAFDQX" varchar(1) NOT NULL default ' ',
  "AAAAFDQZ" varchar(1) NOT NULL default ' ',
  "AAAAFDRA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGRRB","AAABDGVY","AAABEAOJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDKV" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGRRB" varchar(10) NOT NULL default ' ',
  "AAAAFDKY" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABGRRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDLB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGRRB" varchar(10) NOT NULL default ' ',
  "AAABDGVY" varchar(10) NOT NULL default ' ',
  "AAABEAOJ" varchar(10) NOT NULL default ' ',
  "AAABMKFH" varchar(30) NOT NULL default ' ',
  "AAABHMMP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGRRB","AAABDGVY","AAABEAOJ","AAABMKFH","AAABHMMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDLO" (
  "AAABGRRB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABGRRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDLR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFDPB" varchar(10) NOT NULL default ' ',
  "AAAAFDOZ" varchar(12) NOT NULL default '000000000000',
  "AAAAMTZM" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAAAFDPB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDLY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAFDPB" varchar(10) NOT NULL default ' ',
  "AAAAFDMC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAFDPB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDMG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHPD" varchar(20) NOT NULL default '00000000000000000000',
  "AAABDHPE" varchar(20) NOT NULL default '00000000000000000000',
  "AAABDZWZ" varchar(8) NOT NULL default '00000000',
  "AAABDHON" varchar(8) NOT NULL default '00000000',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABLKLI" varchar(1) NOT NULL default ' ',
  "AAABDHOQ" varchar(1) NOT NULL default ' ',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABDHTT" varchar(8) NOT NULL default '00000000',
  "AAABDHTU" varchar(12) NOT NULL default ' ',
  "AAABDHOV" varchar(10) NOT NULL default ' ',
  "AAABDHOW" varchar(10) NOT NULL default ' ',
  "AAABDHOX" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAFDVE" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAFDMX" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAGXOE" varchar(20) NOT NULL default '00000000000000000000',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAAAFDRW" varchar(3) NOT NULL default ' ',
  "AAABGRRB" varchar(10) NOT NULL default ' ',
  "AAABDGVY" varchar(10) NOT NULL default ' ',
  "AAABEAOJ" varchar(10) NOT NULL default ' ',
  "AAAAVKAP" varchar(4) NOT NULL default ' ',
  "AAAAFDNF" varchar(12) NOT NULL default ' ',
  "AAABJCYJ" varchar(2) NOT NULL default ' ',
  "AAAAFDNH" varchar(4) NOT NULL default ' ',
  "AAAAFDRR" varchar(20) NOT NULL default ' ',
  "AAAAFDNJ" varchar(20) NOT NULL default ' ',
  "AAAAFDNK" varchar(20) NOT NULL default ' ',
  "AAAAFDNL" varchar(40) NOT NULL default ' ',
  "AAAAFDNM" varchar(3) NOT NULL default ' ',
  "AAAAFDNN" varchar(3) NOT NULL default ' ',
  "AAAAFDNO" varchar(40) NOT NULL default ' ',
  "AAAAFDNP" varchar(10) NOT NULL default ' ',
  "AAAAFDNQ" varchar(1) NOT NULL default ' ',
  "AAAAFDNR" varchar(20) NOT NULL default ' ',
  "AAAAFDNS" varchar(40) NOT NULL default ' ',
  "AAAAFDNT" varchar(40) NOT NULL default ' ',
  "AAAAFDNU" varchar(3) NOT NULL default ' ',
  "AAAAFDNV" varchar(3) NOT NULL default ' ',
  "AAAAFDNW" varchar(40) NOT NULL default ' ',
  "AAAAFDNX" varchar(10) NOT NULL default ' ',
  "AAAAFDNY" varchar(1) NOT NULL default ' ',
  "AAAAFDNZ" varchar(3) NOT NULL default ' ',
  "AAAAFDOA" varchar(1) NOT NULL default ' ',
  "AAAAFDOB" varchar(1) NOT NULL default ' ',
  "AAAAFDOC" varchar(1) NOT NULL default ' ',
  "AAAAFDOD" varchar(1) NOT NULL default ' ',
  "AAAAFDOE" varchar(12) NOT NULL default ' ',
  "AAABMMVO" varchar(12) NOT NULL default ' ',
  "AAAAGWYJ" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAGWYL" varchar(3) NOT NULL default ' ',
  "AAAAGWYM" varchar(5) NOT NULL default '00000',
  "AAABDGVH" varchar(10) NOT NULL default ' ',
  "AAABFMRU" varchar(2) NOT NULL default ' ',
  "AAABLTXI" varchar(5) NOT NULL default '00000',
  "AAAAGWYS" varchar(5) NOT NULL default '00000',
  "AAABBNPS" varchar(8) NOT NULL default '00000000',
  "AAABDHKC" varchar(8) NOT NULL default '00000000',
  "AAAAGWYV" varchar(14) NOT NULL default ' ',
  "AAAAGWYW" varchar(1) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAAAFDRS" varchar(132) NOT NULL default ' ',
  "AAAAFDOU" varchar(18) NOT NULL default ' ',
  "AAAAGWYP" varchar(3) NOT NULL default ' ',
  "AAAAGWZI" varchar(70) NOT NULL default ' ',
  "AAAAFDOX" varchar(3) NOT NULL default ' ',
  "AAAAFDOY" varchar(3) NOT NULL default ' ',
  "AAAAFDOZ" varchar(12) NOT NULL default '000000000000',
  "AAAAMTZM" varchar(3) NOT NULL default '000',
  "AAAAFDPB" varchar(10) NOT NULL default ' ',
  "AAAAFDPC" varchar(3) NOT NULL default ' ',
  "AAAAFDPD" varchar(3) NOT NULL default ' ',
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
  "AAABJVQV" int(11) NOT NULL default '0',
  "AAAAFDQD" varchar(17) NOT NULL default ' ',
  "AAAAGXOG" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAGXOH" varchar(5) NOT NULL default '00000',
  "AAABGBZZ" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAFDQH" varchar(32) NOT NULL default ' ',
  "AAAAFDQI" varchar(8) NOT NULL default ' ',
  "AAAAFDQJ" varchar(8) NOT NULL default '00000000',
  "AAABHESL" varchar(6) NOT NULL default '000000',
  "AAAAGNCT" varchar(6) NOT NULL default '000000',
  "AAAAFDQM" varchar(8) NOT NULL default '00000000',
  "AAAAFDQN" varchar(6) NOT NULL default '000000',
  "AAAAFDQO" varchar(12) NOT NULL default ' ',
  "AAAAFDQP" varchar(8) NOT NULL default '00000000',
  "AAAAFDQQ" varchar(6) NOT NULL default '000000',
  "AAAAFDQR" varchar(12) NOT NULL default ' ',
  "AAAAUNAQ" varchar(8) NOT NULL default '00000000',
  "AAAAUNAR" varchar(6) NOT NULL default '000000',
  "AAAAFDQU" varchar(1) NOT NULL default ' ',
  "AAABELAE" varchar(1) NOT NULL default ' ',
  "AAAAFDQW" varchar(1) NOT NULL default ' ',
  "AAAAFDQX" varchar(1) NOT NULL default ' ',
  "AAAAFDQY" varchar(1) NOT NULL default ' ',
  "AAAAFDQZ" varchar(1) NOT NULL default ' ',
  "AAAAFDRA" varchar(1) NOT NULL default ' ',
  "AAAAFDRB" varchar(4) NOT NULL default '0000',
  "AAAAFDRC" varchar(4) NOT NULL default '0000',
  "AAAAFDRD" varchar(3) NOT NULL default ' ',
  "AAAAFDRE" varchar(70) NOT NULL default ' ',
  "AAABDHFD" varchar(1) NOT NULL default ' ',
  "AAABLESX" varchar(32) NOT NULL default ' ',
  "AAABLESY" varchar(64) NOT NULL default ' ',
  "AAAAFDRI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAFDRM" ("AAABMRFO","AAABGBZZ","AAABMRIL"),
  KEY "AAAAFDRQ" ("AAAAFDRR","AAAAFDRS","AAABMRIL"),
  KEY "AAAAFDRU" ("AAABMMVO","AAAAFDRW","AAABMRIL"),
  KEY "AAAAFDRY" ("AAAAGXOE","AAABMRIL"),
  KEY "AAAAFDSB" ("AAABMRIL","AAABJFLO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDSE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHPD" varchar(20) NOT NULL default '00000000000000000000',
  "AAABDHPE" varchar(20) NOT NULL default '00000000000000000000',
  "AAABDZWZ" varchar(8) NOT NULL default '00000000',
  "AAABDHON" varchar(8) NOT NULL default '00000000',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABLKLI" varchar(1) NOT NULL default ' ',
  "AAABDHOQ" varchar(1) NOT NULL default ' ',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABDHTT" varchar(8) NOT NULL default '00000000',
  "AAABDHTU" varchar(12) NOT NULL default ' ',
  "AAABDHOV" varchar(10) NOT NULL default ' ',
  "AAABDHOW" varchar(10) NOT NULL default ' ',
  "AAABDHOX" varchar(20) NOT NULL default '00000000000000000000',
  "AAABMKFH" varchar(30) NOT NULL default ' ',
  "AAABHMMP" varchar(30) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  "AAABAARF" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  "AAABLZNN" varchar(10) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAABLBDX" varchar(70) NOT NULL default ' ',
  "AAABKEXE" varchar(1) NOT NULL default ' ',
  "AAAAFDTD" varchar(1) NOT NULL default ' ',
  "AAAAFDTE" varchar(1) NOT NULL default ' ',
  "AAAAFDTF" varchar(1) NOT NULL default ' ',
  "AAAAFDTG" varchar(1) NOT NULL default ' ',
  "AAABDFVN" varchar(30) NOT NULL default ' ',
  "AAAAFDTI" varchar(1) NOT NULL default ' ',
  "AAAAFDTJ" varchar(1) NOT NULL default ' ',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABIMRE" varchar(25) NOT NULL default ' ',
  "AAABIMRG" varchar(2) NOT NULL default ' ',
  "AAABIMRF" varchar(3) NOT NULL default ' ',
  "AAABDGNZ" varchar(10) NOT NULL default ' ',
  "AAABDGOA" varchar(10) NOT NULL default ' ',
  "AAAAFDTQ" varchar(40) NOT NULL default ' ',
  "AAABDGOB" varchar(10) NOT NULL default ' ',
  "AAAAFDTS" varchar(40) NOT NULL default ' ',
  "AAABDFXV" varchar(4) NOT NULL default '0000',
  "AAABLEJR" varchar(255) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABERXX" varchar(10) NOT NULL default ' ',
  "AAABMNMP" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAFDUB" ("AAABMRIL","AAABDHOX","AAABLKLI"),
  KEY "AAAAFDUF" ("AAABMRIL","AAABMKFH","AAABHMMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDUJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIGIZ" varchar(20) NOT NULL default '00000000000000000000',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABIMRE" varchar(25) NOT NULL default ' ',
  "AAABIMRG" varchar(2) NOT NULL default ' ',
  "AAABIMRF" varchar(3) NOT NULL default ' ',
  "AAABGRRB" varchar(10) NOT NULL default ' ',
  "AAABDGVY" varchar(10) NOT NULL default ' ',
  "AAABEAOJ" varchar(10) NOT NULL default ' ',
  "AAAAFDUT" varchar(8) NOT NULL default ' ',
  "AAAAFDUU" varchar(2) NOT NULL default ' ',
  "AAABMCWW" varchar(20) NOT NULL default ' ',
  "AAAAGXOR" varchar(20) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAAAFDUY" varchar(1) NOT NULL default ' ',
  "AAAAUNAQ" varchar(8) NOT NULL default '00000000',
  "AAAAUNAR" varchar(6) NOT NULL default '000000',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABHESL" varchar(6) NOT NULL default '000000',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAAAFDVE" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAGXOE" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAFDVG" varchar(3) NOT NULL default ' ',
  "AAAAFDVH" varchar(25) NOT NULL default ' ',
  "AAAAFDVI" varchar(2) NOT NULL default ' ',
  "AAAAFDVJ" varchar(3) NOT NULL default ' ',
  "AAAAFDVK" varchar(3) NOT NULL default ' ',
  "AAAAFDVL" varchar(25) NOT NULL default ' ',
  "AAAAFDVM" varchar(2) NOT NULL default ' ',
  "AAAAFDVN" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIGIZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDVQ" (
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABLUSK" smallint(6) NOT NULL default '0',
  "AAABLUSG" varchar(14) NOT NULL default ' ',
  "AAABLUSH" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABHPJU","AAABHPJV","AAABLUSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDWA" (
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABLUSK" smallint(6) NOT NULL default '0',
  "AAABLUSG" varchar(14) NOT NULL default ' ',
  "AAABLUSH" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABHPJU","AAABLUSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDWI" (
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABLUSK" smallint(6) NOT NULL default '0',
  "AAABLUSG" varchar(14) NOT NULL default ' ',
  "AAABLUSH" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABHPJU","AAABHPJV","AAABLUSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDWS" (
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABLUSK" smallint(6) NOT NULL default '0',
  "AAABLUSG" varchar(14) NOT NULL default ' ',
  "AAABLUSH" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABHPJU","AAABLUSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDXA" (
  "AAABHMXI" varchar(5) NOT NULL default '00000',
  "AAABHMXJ" varchar(5) NOT NULL default ' ',
  "AAABLUSK" smallint(6) NOT NULL default '0',
  "AAABLUSG" varchar(14) NOT NULL default ' ',
  "AAABLUSH" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABHMXI","AAABHMXJ","AAABLUSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDXK" (
  "AAABHJTB" varchar(3) NOT NULL default '000',
  "AAABLUSK" smallint(6) NOT NULL default '0',
  "AAABLUSG" varchar(14) NOT NULL default ' ',
  "AAABLUSH" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABHJTB","AAABLUSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDXS" (
  "AAABHPJU" varchar(4) NOT NULL default ' ',
  "AAABHPJV" varchar(4) NOT NULL default ' ',
  "AAABLUSK" smallint(6) NOT NULL default '0',
  "AAABLUSG" varchar(14) NOT NULL default ' ',
  "AAABLUSH" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABHPJU","AAABHPJV","AAABLUSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUEXS" (
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABILHK" varchar(10) NOT NULL default ' ',
  "AAABAGFD" varchar(10) NOT NULL default ' ',
  "AAABHGWF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDYI" (
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABHZPJ" varchar(10) NOT NULL default ' ',
  "AAABAGFD" varchar(10) NOT NULL default ' ',
  "AAABHGWF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMNC","AAABHZPJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDYP" (
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAAAFDYX" varchar(30) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  "AAABMGSU" varchar(6) NOT NULL default '000000',
  "AAABMGSS" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMNC","AAAAFDYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDYY" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABHGWF" varchar(1) NOT NULL default ' ',
  "AAABEXVV" varchar(1) NOT NULL default ' ',
  "AAAAIKYF" varchar(1) NOT NULL default ' ',
  "AAAAFDZE" varchar(1) NOT NULL default ' ',
  "AAAAFDZF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLMNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDZI" (
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAGFF" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMNC","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDZO" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPWW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFDZS" (
  "AAABDLWH" varchar(20) NOT NULL default ' ',
  "AAAAFDZU" varchar(30) NOT NULL default ' ',
  "AAAAFPHH" varchar(16) NOT NULL default ' ',
  "AAAAFDZW" varchar(30) NOT NULL default ' ',
  "AAAAFDZX" varchar(30) NOT NULL default ' ',
  "AAAAFDZY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABDLWH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEAA" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDLWH" varchar(20) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABDLWH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEAG" (
  "AAABDLWH" varchar(20) NOT NULL default ' ',
  "AAABECYT" varchar(10) NOT NULL default ' ',
  "AAABLSBE" varchar(30) NOT NULL default ' ',
  "AAABLBEK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABDLWH","AAABECYT"),
  KEY "AAAAFEAN" ("AAABECYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEAP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABIMRE" varchar(25) NOT NULL default ' ',
  "AAABIMRG" varchar(2) NOT NULL default ' ',
  "AAABIMRF" varchar(3) NOT NULL default ' ',
  "AAAAFEAV" varchar(1) NOT NULL default ' ',
  "AAAAFEAW" decimal(15,0) NOT NULL default '0',
  "AAAAFEAX" decimal(15,0) NOT NULL default '0',
  "AAAAFEAY" varchar(1) NOT NULL default ' ',
  "AAAAFEAZ" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABIMRD","AAABIMRE","AAABIMRG","AAABIMRF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEBF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABIMRE" varchar(25) NOT NULL default ' ',
  "AAABIMRG" varchar(2) NOT NULL default ' ',
  "AAABIMRF" varchar(3) NOT NULL default ' ',
  "AAABECYT" varchar(10) NOT NULL default ' ',
  "AAAATUZP" varchar(4) NOT NULL default '0000',
  "AAABEDNI" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTVR"),
  UNIQUE KEY "AAAAFEBR" ("AAABMRIL","AAABIMRD","AAABIMRE","AAABIMRG","AAABIMRF","AAABECYT","AAAATUZP","AAABEDNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFECA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(22) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFECL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDLWH" varchar(20) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAAFECQ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABDLWH","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFECV" (
  "AAABEUWM" varchar(10) NOT NULL default ' ',
  "AAAATQMS" varchar(10) NOT NULL default ' ',
  "AAAATQMT" varchar(10) NOT NULL default '0000000000',
  "AAABKNFD" varchar(4) NOT NULL default '0000',
  "AAABLELS" varchar(1) NOT NULL default ' ',
  "AAAAFEDB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEUWM","AAAATQMS","AAAATQMT","AAABKNFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEDG" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABEUWM" varchar(10) NOT NULL default ' ',
  "AAAATQMS" varchar(10) NOT NULL default ' ',
  "AAAATQMT" varchar(10) NOT NULL default '0000000000',
  "AAABKNFD" varchar(4) NOT NULL default '0000',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABEUWM","AAAATQMS","AAAATQMT","AAABKNFD","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEDV" (
  "AAABEUWM" varchar(10) NOT NULL default ' ',
  "AAAATQMS" varchar(10) NOT NULL default ' ',
  "AAAATQMT" varchar(10) NOT NULL default '0000000000',
  "AAABKNFD" varchar(4) NOT NULL default '0000',
  "AAAATVNH" varchar(1) NOT NULL default ' ',
  "AAAAFEEQ" varchar(10) NOT NULL default ' ',
  "AAAAFEER" varchar(10) NOT NULL default ' ',
  "AAAAFEES" varchar(10) NOT NULL default '0000000000',
  "AAAAFEEN" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABEUWM","AAAATQMS","AAAATQMT","AAABKNFD","AAAATVNH","AAAAFEEQ","AAAAFEER","AAAAFEES","AAAAFEEN"),
  KEY "AAAAFEEO" ("AAAATVNH","AAAAFEEQ","AAAAFEER","AAAAFEES")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEET" (
  "AAABEUWM" varchar(10) NOT NULL default ' ',
  "AAAATQMS" varchar(10) NOT NULL default ' ',
  "AAAATQMT" varchar(10) NOT NULL default '0000000000',
  "AAABKNFD" varchar(4) NOT NULL default '0000',
  "AAAAFEFE" varchar(10) NOT NULL default ' ',
  "AAAAFEFF" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABEUWM","AAAATQMS","AAAATQMT","AAABKNFD","AAAAFEFE","AAAAFEFF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEFG" (
  "AAABEUWM" varchar(10) NOT NULL default ' ',
  "AAAATQMS" varchar(10) NOT NULL default ' ',
  "AAAATQMT" varchar(10) NOT NULL default '0000000000',
  "AAABKNFD" varchar(4) NOT NULL default '0000',
  "AAAAFEFL" int(11) NOT NULL default '0',
  "AAAAFRVK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEUWM","AAAATQMS","AAAATQMT","AAABKNFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEFR" (
  "AAABEUWM" varchar(10) NOT NULL default ' ',
  "AAAATQMS" varchar(10) NOT NULL default ' ',
  "AAAATQMT" varchar(10) NOT NULL default '0000000000',
  "AAABKNFD" varchar(4) NOT NULL default '0000',
  "AAAAFEHW" varchar(10) NOT NULL default ' ',
  "AAAAFEHX" varchar(1) NOT NULL default ' ',
  "AAAAFEHY" varchar(6) NOT NULL default '000000',
  "AAAAFEHZ" varchar(10) NOT NULL default ' ',
  "AAAAFEIA" varchar(10) NOT NULL default '0000000000',
  "AAAAFEIB" varchar(10) NOT NULL default ' ',
  "AAAAFEIC" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABEUWM","AAAATQMS","AAAATQMT","AAABKNFD","AAAAFEHW","AAAAFEHX","AAAAFEHY","AAAAFEHZ","AAAAFEIA","AAAAFEIB","AAAAFEIC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEGO" (
  "AAABEUWM" varchar(10) NOT NULL default ' ',
  "AAAATQMS" varchar(10) NOT NULL default ' ',
  "AAAATQMT" varchar(10) NOT NULL default '0000000000',
  "AAABKNFD" varchar(4) NOT NULL default '0000',
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(120) NOT NULL default ' ',
  "AAAAFEGW" varchar(4) NOT NULL default ' ',
  "AAABFWNS" varchar(4) NOT NULL default ' ',
  "AAAALZYU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABEUWM","AAAATQMS","AAAATQMT","AAABKNFD","AAABLRST","AAABMKFH","AAABLFYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEHG" (
  "AAABEUWM" varchar(10) NOT NULL default ' ',
  "AAAATQMS" varchar(10) NOT NULL default ' ',
  "AAAATQMT" varchar(10) NOT NULL default '0000000000',
  "AAABKNFD" varchar(4) NOT NULL default '0000',
  "AAAAFEHW" varchar(10) NOT NULL default ' ',
  "AAAAFEHX" varchar(1) NOT NULL default ' ',
  "AAAAFEHY" varchar(6) NOT NULL default '000000',
  "AAAAFEHZ" varchar(10) NOT NULL default ' ',
  "AAAAFEIA" varchar(10) NOT NULL default '0000000000',
  "AAAAFEIB" varchar(10) NOT NULL default ' ',
  "AAAAFEIC" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABEUWM","AAAATQMS","AAAATQMT","AAABKNFD","AAAAFEHW","AAAAFEHX","AAAAFEHY","AAAAFEHZ","AAAAFEIA","AAAAFEIB","AAAAFEIC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEID" (
  "AAAATQMS" varchar(10) NOT NULL default ' ',
  "AAABLXDT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAATQMS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEIH" (
  "AAAATQMS" varchar(10) NOT NULL default ' ',
  "AAAAFEIN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAATQMS"),
  UNIQUE KEY "AAAAFEIL" ("AAAATQMS","AAAAFEIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEIO" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATQMS" varchar(10) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAATQMS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEIU" (
  "AAAATQMS" varchar(10) NOT NULL default ' ',
  "AAAATQMT" varchar(10) NOT NULL default '0000000000',
  "AAAAFEJB" varchar(10) NOT NULL default ' ',
  "AAAAFEJC" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAAATQMS","AAAATQMT","AAAAFEJB","AAAAFEJC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEJD" (
  "AAAATQMS" varchar(10) NOT NULL default ' ',
  "AAAATQMT" varchar(10) NOT NULL default '0000000000',
  "AAAAFEXH" varchar(20) NOT NULL default ' ',
  "AAAAVJPZ" varchar(1) NOT NULL default ' ',
  "AAAAFEKH" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAATQMS","AAAATQMT"),
  UNIQUE KEY "AAAAFEJL" ("AAAATQMS","AAAAFEXH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEJO" (
  "AAAATQMS" varchar(10) NOT NULL default ' ',
  "AAAATQMT" varchar(10) NOT NULL default '0000000000',
  "AAAAFEJY" varchar(10) NOT NULL default ' ',
  "AAAAFEJZ" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAAATQMS","AAAATQMT","AAAAFEJY","AAAAFEJZ"),
  KEY "AAAAFEJX" ("AAAAFEJY","AAAAFEJZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEKA" (
  "AAAATQMS" varchar(10) NOT NULL default ' ',
  "AAAAVJPZ" varchar(1) NOT NULL default ' ',
  "AAAAFEKH" varchar(6) NOT NULL default '000000',
  "AAAAFEKE" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAAATQMS","AAAAVJPZ","AAAAFEKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEKI" (
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
  KEY "AAAAFELO" ("AAAAURLT"),
  KEY "AAAAFELQ" ("AAAAURLV"),
  KEY "AAAAFELS" ("AAABKMVW","AAABKMVX"),
  KEY "AAAAFELV" ("AAABILWI"),
  KEY "AAAAFELX" ("AAAAVCLU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFELZ" (
  "AAAANABN" varchar(10) NOT NULL default '0000000000',
  "AAABKNFD" varchar(4) NOT NULL default '0000',
  "AAAANABP" varchar(10) NOT NULL default ' ',
  "AAAANABQ" varchar(10) NOT NULL default ' ',
  "AAAANABR" varchar(10) NOT NULL default '0000000000',
  "AAAANABS" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAAANABN","AAABKNFD","AAAANABP","AAAANABQ","AAAANABR","AAAANABS"),
  KEY "AAAAFEMM" ("AAAANABP","AAAANABQ","AAAANABR","AAAANABS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEMR" (
  "AAAANABN" varchar(10) NOT NULL default '0000000000',
  "AAABMLPO" varchar(10) NOT NULL default '0000000000',
  "AAABMMRH" varchar(42) NOT NULL default ' ',
  PRIMARY KEY ("AAAANABN","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEMX" (
  "AAAANABN" varchar(10) NOT NULL default '0000000000',
  "AAAAFEPU" varchar(1) NOT NULL default ' ',
  "AAAAIGHB" varchar(1) NOT NULL default ' ',
  "AAAAFENB" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAAANABN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEND" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAANABN" varchar(10) NOT NULL default '0000000000',
  "AAABKNFD" varchar(4) NOT NULL default '0000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAAANABN","AAABKNFD","AAABMLQA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFENQ" (
  "AAAANABN" varchar(10) NOT NULL default '0000000000',
  "AAABKNFD" varchar(4) NOT NULL default '0000',
  "AAAATQMS" varchar(10) NOT NULL default ' ',
  "AAAATQMT" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAAANABN","AAABKNFD","AAAATQMS","AAAATQMT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFENZ" (
  "AAAANABN" varchar(10) NOT NULL default '0000000000',
  "AAABKNFD" varchar(4) NOT NULL default '0000',
  "AAABLUSV" varchar(10) NOT NULL default ' ',
  "AAABMMJT" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAAANABN","AAABKNFD","AAABLUSV","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEOI" (
  "AAAANABN" varchar(10) NOT NULL default '0000000000',
  "AAABKNFD" varchar(4) NOT NULL default '0000',
  "AAAAFEOL" varchar(24) NOT NULL default ' ',
  "AAABEUWM" varchar(10) NOT NULL default ' ',
  "AAAAGAJY" varchar(10) NOT NULL default ' ',
  "AAAAFEOO" varchar(4) NOT NULL default '0000',
  "AAAAFRVK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAANABN","AAABKNFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEOS" (
  "AAAANABN" varchar(10) NOT NULL default '0000000000',
  "AAAAFEOU" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAAANABN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEOW" (
  "AAAANABN" varchar(10) NOT NULL default '0000000000',
  "AAABMLPO" varchar(10) NOT NULL default '0000000000',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLXDT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAANABN","AAABMLPO","AAABLENH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEPM" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAFEPU" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAAFEPU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEPS" (
  "AAAAFEPU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFEPU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEPV" (
  "AAAANABN" varchar(10) NOT NULL default '0000000000',
  "AAABKNFD" varchar(4) NOT NULL default '0000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPWW" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAAANABN","AAABKNFD","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEQD" (
  "AAAANABN" varchar(10) NOT NULL default '0000000000',
  "AAABKNFD" varchar(4) NOT NULL default '0000',
  "AAAAFEQK" varchar(10) NOT NULL default ' ',
  "AAAAFEQL" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAAANABN","AAABKNFD","AAAAFEQK","AAAAFEQL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEQM" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAIGHB" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAAIGHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEQS" (
  "AAAAIGHB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAIGHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEQV" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAAAMMXR" varchar(8) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAAATPFH" varchar(1) NOT NULL default ' ',
  "AAAAGIED" varchar(20) NOT NULL default ' ',
  "AAAAFERC" varchar(8) NOT NULL default '00000000',
  "AAAAFERD" varchar(6) NOT NULL default '000000',
  "AAAAFERE" varchar(8) NOT NULL default '00000000',
  "AAAAFERF" varchar(6) NOT NULL default '000000',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABILWI","AAAAMMXR","AAABLUMP"),
  KEY "AAAAFERT" ("AAAAGIED","AAABLUNI","AAAATPFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFERX" (
  "AAAATQMS" varchar(10) NOT NULL default ' ',
  "AAABMMVU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATQMS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFESB" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAAFETJ" varchar(20) NOT NULL default ' ',
  "AAABLENL" varchar(5) NOT NULL default '00000',
  "AAABLPJD" varchar(4) NOT NULL default ' ',
  "AAABLGCM" varchar(120) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAAAFETJ","AAABLENL","AAABLPJD","AAABLGCM","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFESQ" (
  "AAAAFETJ" varchar(20) NOT NULL default ' ',
  "AAABLENL" varchar(5) NOT NULL default '00000',
  "AAAATQMP" varchar(20) NOT NULL default ' ',
  "AAAATQMQ" varchar(8) NOT NULL default ' ',
  "AAABEUWM" varchar(10) NOT NULL default ' ',
  "AAAATQMS" varchar(10) NOT NULL default ' ',
  "AAAATQMT" varchar(10) NOT NULL default '0000000000',
  "AAAATQMZ" varchar(8) NOT NULL default '00000000',
  "AAAATQNA" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAAFETJ","AAABLENL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFETC" (
  "AAAAFETJ" varchar(20) NOT NULL default ' ',
  "AAABLENL" varchar(5) NOT NULL default '00000',
  "AAABLPJD" varchar(4) NOT NULL default ' ',
  "AAABLGCM" varchar(120) NOT NULL default ' ',
  "AAAATQMU" varchar(5) NOT NULL default '00000',
  "AAAATQMV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFETJ","AAABLENL","AAABLPJD","AAABLGCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFETN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFEXR" varchar(12) NOT NULL default ' ',
  "AAABDTOD" varchar(4) NOT NULL default ' ',
  "AAAAFETR" decimal(13,3) NOT NULL default '0.000',
  "AAAAFETS" varchar(1) NOT NULL default ' ',
  "AAAAFETT" varchar(8) NOT NULL default ' ',
  "AAAAFETU" varchar(1) NOT NULL default ' ',
  "AAAAFETV" varchar(1) NOT NULL default ' ',
  "AAAAFETW" varchar(1) NOT NULL default ' ',
  "AAAAFETX" varchar(1) NOT NULL default ' ',
  "AAAAFETY" varchar(1) NOT NULL default ' ',
  "AAAAFETZ" varchar(1) NOT NULL default ' ',
  "AAAAFEUA" varchar(1) NOT NULL default ' ',
  "AAAAFEUB" varchar(1) NOT NULL default ' ',
  "AAAAFEUC" varchar(1) NOT NULL default ' ',
  "AAAAFEUD" varchar(1) NOT NULL default ' ',
  "AAAAFEUE" varchar(1) NOT NULL default ' ',
  "AAAAFEUF" varchar(1) NOT NULL default ' ',
  "AAAAFEUG" varchar(1) NOT NULL default ' ',
  "AAAAFEUH" varchar(1) NOT NULL default ' ',
  "AAAAFEUI" varchar(1) NOT NULL default ' ',
  "AAAAFEUJ" varchar(1) NOT NULL default ' ',
  "AAAAFEUK" varchar(1) NOT NULL default ' ',
  "AAAAFEUL" varchar(1) NOT NULL default ' ',
  "AAAAFEUM" varchar(1) NOT NULL default ' ',
  "AAAAFEUN" varchar(1) NOT NULL default ' ',
  "AAAAFEUO" varchar(1) NOT NULL default ' ',
  "AAAAFEUP" varchar(1) NOT NULL default ' ',
  "AAAAFEUQ" varchar(1) NOT NULL default ' ',
  "AAAAFEUR" varchar(1) NOT NULL default ' ',
  "AAAAFEUS" varchar(1) NOT NULL default ' ',
  "AAAAFEUT" varchar(1) NOT NULL default ' ',
  "AAAAFEUU" varchar(1) NOT NULL default ' ',
  "AAAAFEUV" varchar(1) NOT NULL default ' ',
  "AAAAFEUW" varchar(1) NOT NULL default ' ',
  "AAAAFEUX" varchar(1) NOT NULL default ' ',
  "AAAAFEUY" varchar(1) NOT NULL default ' ',
  "AAAAFEUZ" varchar(1) NOT NULL default ' ',
  "AAAAFEVA" varchar(1) NOT NULL default ' ',
  "AAAAFEVB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFEXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEVE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOZE" varchar(1) NOT NULL default ' ',
  "AAAAFEXR" varchar(12) NOT NULL default ' ',
  "AAABMOIB" varchar(14) NOT NULL default ' ',
  "AAABHIOL" varchar(30) NOT NULL default ' ',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAAAHHTP" int(11) NOT NULL default '0',
  "AAABMNMV" varchar(3) NOT NULL default ' ',
  "AAABMNMW" int(11) NOT NULL default '0',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAAAFEVQ" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(45) NOT NULL default ' ',
  "AAABMNNA" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOZE","AAAAFEXR","AAABMOIB","AAABHIOL","AAABMNMH","AAAAHHTP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEWA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOZE" varchar(1) NOT NULL default ' ',
  "AAAAFEXR" varchar(12) NOT NULL default ' ',
  "AAABMOIB" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOZE","AAAAFEXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEWI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOZE" varchar(1) NOT NULL default ' ',
  "AAAAFEXR" varchar(12) NOT NULL default ' ',
  "AAABMOIB" varchar(14) NOT NULL default ' ',
  "AAAAFEWN" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLOZE","AAAAFEXR","AAABMOIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEWS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOZE" varchar(1) NOT NULL default ' ',
  "AAAAFEXR" varchar(12) NOT NULL default ' ',
  "AAABMOIB" varchar(14) NOT NULL default ' ',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOZE","AAAAFEXR","AAABMOIB","AAABMNJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEXE" (
  "AAAATQMS" varchar(10) NOT NULL default ' ',
  "AAAATQMT" varchar(10) NOT NULL default '0000000000',
  "AAAAFEXH" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAATQMS","AAAATQMT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEXK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOZE" varchar(1) NOT NULL default ' ',
  "AAAAFEXR" varchar(12) NOT NULL default ' ',
  "AAABDBZB" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOZE","AAAAFEXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEXS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFEXU" varchar(18) NOT NULL default ' ',
  "AAAAFEXV" varchar(3) NOT NULL default '000',
  "AAAAFEXW" varchar(3) NOT NULL default '000',
  "AAAAFEXX" varchar(2) NOT NULL default ' ',
  "AAAAFEXY" varchar(4) NOT NULL default ' ',
  "AAAAFEXZ" varchar(4) NOT NULL default ' ',
  "AAAAFEYA" varchar(4) NOT NULL default ' ',
  "AAAAFEYB" varchar(4) NOT NULL default ' ',
  "AAAAFEYC" varchar(4) NOT NULL default ' ',
  "AAAAFEYD" varchar(4) NOT NULL default ' ',
  "AAAAFEYE" varchar(4) NOT NULL default ' ',
  "AAAAFEYF" varchar(4) NOT NULL default ' ',
  "AAAAFEYG" varchar(4) NOT NULL default ' ',
  "AAAAFEYH" varchar(4) NOT NULL default ' ',
  "AAAAFEYI" varchar(4) NOT NULL default ' ',
  "AAAAFEYJ" varchar(4) NOT NULL default ' ',
  "AAAAFEYK" varchar(4) NOT NULL default ' ',
  "AAAAFEYL" varchar(4) NOT NULL default ' ',
  "AAAAFEYM" varchar(4) NOT NULL default ' ',
  "AAAAFEYN" varchar(4) NOT NULL default ' ',
  "AAAAFEYO" varchar(4) NOT NULL default ' ',
  "AAAAFEYP" varchar(4) NOT NULL default ' ',
  "AAAAFEYQ" varchar(4) NOT NULL default ' ',
  "AAAAFEYR" varchar(4) NOT NULL default ' ',
  "AAAAFEYS" varchar(4) NOT NULL default ' ',
  "AAAAFEYT" varchar(4) NOT NULL default ' ',
  "AAAAFEYU" varchar(4) NOT NULL default ' ',
  "AAAAFEYV" varchar(4) NOT NULL default ' ',
  "AAAAFEYW" varchar(255) NOT NULL default ' ',
  "AAAAFEYX" varchar(255) NOT NULL default ' ',
  "AAAAFEYY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFEZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAPVRK" varchar(4) NOT NULL default ' ',
  "AAABHYJM" varchar(2) NOT NULL default ' ',
  "AAABHYJN" varchar(2) NOT NULL default ' ',
  "AAAAFEZF" varchar(4) NOT NULL default ' ',
  "AAABLROG" varchar(4) NOT NULL default ' ',
  "AAAAFEZH" varchar(4) NOT NULL default ' ',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAABMAKT" varchar(10) NOT NULL default ' ',
  "AAAAFEZK" varchar(2) NOT NULL default ' ',
  "AAABDTND" varchar(6) NOT NULL default ' ',
  "AAAAFEZM" varchar(6) NOT NULL default ' ',
  "AAAAFEZN" varchar(6) NOT NULL default ' ',
  "AAAAPVRZ" varchar(4) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAAAFEZQ" varchar(5) NOT NULL default ' ',
  "AAAAFEZR" varchar(2) NOT NULL default ' ',
  "AAAAFEZS" varchar(2) NOT NULL default ' ',
  "AAAAFEZT" varchar(2) NOT NULL default ' ',
  "AAABFSQO" varchar(2) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAAAFEZX" varchar(4) NOT NULL default ' ',
  "AAAAFEZY" varchar(30) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAAAFFAA" varchar(4) NOT NULL default ' ',
  "AAAAUXJC" varchar(1) NOT NULL default ' ',
  "AAAAFFAC" varchar(1) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABHGHN" varchar(1) NOT NULL default ' ',
  "AAAAFFGL" varchar(2) NOT NULL default ' ',
  "AAAAFFAG" varchar(4) NOT NULL default ' ',
  "AAABHWDE" varchar(4) NOT NULL default ' ',
  "AAABLKXQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAPVRK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFAL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAPVRK" varchar(4) NOT NULL default ' ',
  "AAAAFFAP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAPVRK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFAT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAPVRK" varchar(4) NOT NULL default ' ',
  "AAAAFFMV" varchar(2) NOT NULL default ' ',
  "AAAAFFMW" varchar(2) NOT NULL default ' ',
  "AAAAFFNV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAPVRK","AAAAFFMV","AAAAFFMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFBD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAQIZV" varchar(8) NOT NULL default ' ',
  "AAAAFFBG" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAQIZV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFBJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFFBO" varchar(8) NOT NULL default ' ',
  "AAAAFFBM" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFFBO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFBP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAQIZV" varchar(8) NOT NULL default ' ',
  "AAAAFFBT" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAQIZV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFBX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJRKN" varchar(1) NOT NULL default ' ',
  "AAAAYOZS" varchar(1) NOT NULL default ' ',
  "AAAAFFCB" varchar(1) NOT NULL default ' ',
  "AAAAFFCC" varchar(1) NOT NULL default ' ',
  "AAABIJKF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJRKN","AAAAYOZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFCH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJRKN" varchar(1) NOT NULL default ' ',
  "AAAAYOZS" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAFFCM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJRKN","AAAAYOZS","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFCR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAQIZW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAQIZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFCW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAQIZW" varchar(2) NOT NULL default ' ',
  "AAAAFFDA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAQIZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFDE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAPVSE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAPVSE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFDJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAPVSE" varchar(10) NOT NULL default ' ',
  "AAAAFFDN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAPVSE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFDR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKNN" varchar(4) NOT NULL default ' ',
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABINFJ" varchar(40) NOT NULL default ' ',
  "AAABLTWM" varchar(40) NOT NULL default ' ',
  "AAABFLJO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLKNN","AAABMOHM","AAABINFJ","AAABLTWM","AAABFLJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFEE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKNN" varchar(4) NOT NULL default ' ',
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABINFJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLKNN","AAABMOHM","AAABINFJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFEN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUAB" varchar(4) NOT NULL default ' ',
  "AAABLUAC" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUAB","AAABLUAC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFEU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUAB" varchar(4) NOT NULL default ' ',
  "AAABLUAC" varchar(5) NOT NULL default ' ',
  "AAAAFFEZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUAB","AAABLUAC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFFE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAQJAR" varchar(4) NOT NULL default ' ',
  "AAAAQIZO" varchar(4) NOT NULL default ' ',
  "AAABJJXP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAQJAR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFFL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAQJAR" varchar(4) NOT NULL default ' ',
  "AAAAFFFP" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAQJAR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFFT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFFGL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFFGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFFY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAFFGL" varchar(2) NOT NULL default ' ',
  "AAAAFFGC" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAFFGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFGG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFFGL" varchar(2) NOT NULL default ' ',
  "AAAAQJAR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFFGL","AAAAQJAR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFGN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAPVRK" varchar(4) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAPVRK","AAABMQIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFGU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFFGW" varchar(3) NOT NULL default ' ',
  "AAAATQIT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFGZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFFHM" varchar(4) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFFHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFHF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAFFHM" varchar(4) NOT NULL default ' ',
  "AAAAFFHJ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAFFHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFHN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAFFHS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTKC","AAABLXDR","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFHX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZMXN" varchar(4) NOT NULL default ' ',
  "AAABEPUI" varchar(1) NOT NULL default ' ',
  "AAAAFFIB" varchar(1) NOT NULL default ' ',
  "AAAAFFIC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZMXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFIF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZMXN" varchar(4) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZMXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFIN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZMXN" varchar(4) NOT NULL default ' ',
  "AAAAHLMP" varchar(4) NOT NULL default '0000',
  "AAAAFFIR" varchar(4) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZMXN","AAAAHLMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHDLK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHDLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFJB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHDLK" varchar(4) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHDLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFJJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHDLK" varchar(4) NOT NULL default ' ',
  "AAAAFFJS" varchar(4) NOT NULL default ' ',
  "AAAAZMXN" varchar(4) NOT NULL default ' ',
  "AAAAFFJO" varchar(4) NOT NULL default ' ',
  "AAAAVULN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHDLK","AAAAFFJS","AAAAZMXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFJU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAPVRZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAPVRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFJZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAPVRZ" varchar(4) NOT NULL default ' ',
  "AAAAFFKD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAPVRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFKH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNLW" varchar(70) NOT NULL default ' ',
  "AAAAZMXN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFKN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFFMF" varchar(20) NOT NULL default ' ',
  "AAABHDLK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFFMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFFMF" varchar(20) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOQQ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFFMF","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFLB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFFMF" varchar(20) NOT NULL default ' ',
  "AAABHDLK" varchar(4) NOT NULL default ' ',
  "AAABFTFQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFFMF","AAABHDLK","AAABFTFQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFLK" (
  "AAAAFFLP" varchar(70) NOT NULL default ' ',
  "AAAAFFLM" varchar(70) NOT NULL default ' ',
  "AAAAFFLN" varchar(70) NOT NULL default ' ',
  "AAABMNLW" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFFLP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFLQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFFMF" varchar(20) NOT NULL default ' ',
  "AAABMNLW" varchar(70) NOT NULL default ' ',
  "AAAAZMXN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFFMF","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFLY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFFMF" varchar(20) NOT NULL default ' ',
  "AAABMNLW" varchar(70) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAFFMD" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFFMF","AAABMNLW","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFMI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAPVRK" varchar(4) NOT NULL default ' ',
  "AAAAFFMV" varchar(2) NOT NULL default ' ',
  "AAAAFFMW" varchar(2) NOT NULL default ' ',
  "AAAAFFNV" varchar(2) NOT NULL default ' ',
  "AAABHLSE" varchar(1) NOT NULL default ' ',
  "AAAAFFMP" varchar(30) NOT NULL default ' ',
  "AAAAFFMQ" varchar(1) NOT NULL default ' ',
  "AAAAFFMR" varchar(1) NOT NULL default ' ',
  "AAAAFFMS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAPVRK","AAAAFFMV","AAAAFFMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFMX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFFNV" varchar(2) NOT NULL default ' ',
  "AAAAFFNA" varchar(1) NOT NULL default ' ',
  "AAAAFFNB" varchar(1) NOT NULL default ' ',
  "AAAAFFNC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFFNV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFNF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAFFNV" varchar(2) NOT NULL default ' ',
  "AAABDDBR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAFFNV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFNN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFFNV" varchar(2) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABMLPO" varchar(6) NOT NULL default '000000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFFNV","AAABLHPM","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFNY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAAAFFOI" varchar(1) NOT NULL default ' ',
  "AAAAQIZV" varchar(8) NOT NULL default ' ',
  "AAABLBET" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLDW","AAABLXDR","AAAAFFOI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFOJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABHJRW" varchar(2) NOT NULL default ' ',
  "AAABMPMH" varchar(4) NOT NULL default '0000',
  "AAABFIGO" varchar(7) NOT NULL default ' ',
  "AAABMMVO" varchar(7) NOT NULL default ' ',
  "AAAAFFPA" varchar(8) NOT NULL default '00000000',
  "AAABLOZS" varchar(8) NOT NULL default '00000000',
  "AAABLMLR" varchar(3) NOT NULL default '000',
  "AAABMQBS" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABHJRW","AAABMPMH","AAABFIGO","AAABMMVO","AAAAFFPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFPB" (
  "AAAAFFPH" varchar(1) NOT NULL default ' ',
  "AAABMCXK" varchar(10) NOT NULL default '0000000000',
  "AAABMCXL" varchar(8) NOT NULL default '00000000',
  "AAABKQWL" varchar(12) NOT NULL default '000000000000',
  "AAABKQOJ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAAFFPH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFPI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKQFL" varchar(8) NOT NULL default '00000000',
  "AAABKRXK" varchar(2) NOT NULL default '00',
  "AAABMMVU" varchar(2) NOT NULL default ' ',
  "AAABHHSH" varchar(2) NOT NULL default ' ',
  "AAABJAMQ" varchar(10) NOT NULL default ' ',
  "AAABHHSJ" varchar(2) NOT NULL default ' ',
  "AAABHHSB" decimal(15,0) NOT NULL default '0',
  "AAABHHSC" varchar(12) NOT NULL default ' ',
  "AAABJQWM" decimal(15,0) NOT NULL default '0',
  "AAABLRQM" varchar(12) NOT NULL default ' ',
  "AAABHHRK" decimal(15,0) NOT NULL default '0',
  "AAABHHRL" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKQFL","AAABKRXK","AAABMMVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFQA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLXYU" varchar(100) NOT NULL default ' ',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABLXYU","AAABMCJZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFQN" (
  "AAABLHPU" varchar(40) NOT NULL default ' ',
  "AAABLJIM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHPU","AAABLJIM"),
  KEY "AAAAFFQS" ("AAABLJIM","AAABLHPU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFQV" (
  "AAABLHPU" varchar(40) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHPU","AAABMREL"),
  KEY "AAAAFFRA" ("AAABMREL","AAABLHPU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFRD" (
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABLWWP" varchar(4) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABCVWD" varchar(4) NOT NULL default ' ',
  "AAAATHNY" varchar(4) NOT NULL default ' ',
  "AAAAFFRJ" tinyint(3) unsigned NOT NULL default '0',
  "AAAAFFRK" tinyint(3) unsigned NOT NULL default '0',
  "AAAAFFRL" tinyint(3) unsigned NOT NULL default '0',
  "AAAAFFRM" tinyint(3) unsigned NOT NULL default '0',
  "AAAAFFRN" tinyint(3) unsigned NOT NULL default '0',
  "AAAAFFRO" tinyint(3) unsigned NOT NULL default '0',
  "AAAAFFRP" tinyint(3) unsigned NOT NULL default '0',
  "AAAAFFRQ" tinyint(3) unsigned NOT NULL default '0',
  "AAAAFFRR" tinyint(3) unsigned NOT NULL default '0',
  "AAAAFFRS" varchar(1) NOT NULL default ' ',
  "AAAAFFRT" varchar(1) NOT NULL default ' ',
  "AAABELXK" varchar(1) NOT NULL default ' ',
  "AAAAFFRV" varchar(132) NOT NULL default ' ',
  "AAAAFFRW" varchar(8) NOT NULL default '00000000',
  "AAAAFFRX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMNYT","AAABLWWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFSA" (
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMGHA" varchar(4) NOT NULL default '0000',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAAAFFSN" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNYT","AAABMGHA","AAABMNJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFSI" (
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMGHA" varchar(4) NOT NULL default '0000',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABLNLM" varchar(132) NOT NULL default ' ',
  "AAAAFFSN" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNYT","AAABMGHA","AAABMNJW","AAABLNLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFSS" (
  "AAAAFFSV" varchar(10) NOT NULL default ' ',
  "AAAAFFSU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFFSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFSW" (
  "AAABEZYX" varchar(1) NOT NULL default ' ',
  "AAAAFFTP" varchar(40) NOT NULL default ' ',
  "AAAAFFTK" varchar(1) NOT NULL default ' ',
  "AAAAFFTL" varchar(1) NOT NULL default ' ',
  "AAAAFFTM" varchar(4) NOT NULL default '0000',
  "AAAAFQFY" varchar(6) NOT NULL default '000000',
  "AAAARPGB" varchar(4) NOT NULL default ' ',
  "AAAARPGC" varchar(1) NOT NULL default ' ',
  "AAABLFWP" varchar(20) NOT NULL default ' ',
  "AAABLFZO" varchar(20) NOT NULL default ' ',
  "AAAAFFTH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEZYX","AAAAFFTP","AAAAFFTK","AAAAFFTL","AAAAFFTM","AAAAFQFY"),
  KEY "AAAAFFTO" ("AAAAFFTP","AAABEZYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFTR" (
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABMRFO" varchar(20) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAAAQLFL" int(11) NOT NULL default '0',
  "AAAAFFVE" int(11) NOT NULL default '0',
  "AAABLWWN" varchar(8) NOT NULL default ' ',
  "AAABLWWO" varchar(6) NOT NULL default ' ',
  "AAAAFFVH" int(11) NOT NULL default '0',
  "AAABJLFF" blob,
  PRIMARY KEY ("AAABLESS","AAABMRFO","AAABMOHM","AAAAQLFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFUF" (
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABKMZH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLESS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFUM" (
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAAAQLFL" int(11) NOT NULL default '0',
  "AAAAFFVE" int(11) NOT NULL default '0',
  "AAABLWWN" varchar(8) NOT NULL default ' ',
  "AAABLWWO" varchar(6) NOT NULL default ' ',
  "AAAAFFVH" int(11) NOT NULL default '0',
  "AAABJLFF" text,
  PRIMARY KEY ("AAABLESS","AAABINAW","AAABMOHM","AAAAQLFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFVA" (
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAAAQLFL" int(11) NOT NULL default '0',
  "AAAAFFVE" int(11) NOT NULL default '0',
  "AAABLWWN" varchar(8) NOT NULL default ' ',
  "AAABLWWO" varchar(6) NOT NULL default ' ',
  "AAAAFFVH" int(11) NOT NULL default '0',
  "AAABJLFF" text,
  PRIMARY KEY ("AAABLESS","AAABMOHM","AAAAQLFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFVM" (
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAARPFY" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLESS","AAABLSCE","AAAARPFY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFVU" (
  "AAABFRPG" varchar(5) NOT NULL default ' ',
  "AAAAFFVZ" varchar(1) NOT NULL default ' ',
  "AAAAFFVX" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABFRPG","AAAAFFVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFWA" (
  "AAAAZOEQ" varchar(12) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABHGYG" varchar(6) NOT NULL default '000000',
  "AAAAFHIT" varchar(8) NOT NULL default ' ',
  "AAABLRXL" varchar(2) NOT NULL default ' ',
  "AAAAFHIG" int(11) NOT NULL default '0',
  "AAAAFHIH" int(11) NOT NULL default '0',
  "AAAAFHII" int(11) NOT NULL default '0',
  "AAAAFHIJ" int(11) NOT NULL default '0',
  "AAAAFHIK" int(11) NOT NULL default '0',
  "AAAAFHIL" int(11) NOT NULL default '0',
  "AAAAFHIM" varchar(6) NOT NULL default ' ',
  "AAAAFHIN" int(11) NOT NULL default '0',
  "AAAAFHIO" int(11) NOT NULL default '0',
  "AAAAFFWP" varchar(1) NOT NULL default ' ',
  "AAAAFFWQ" varchar(44) NOT NULL default ' ',
  "AAAAFFWR" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAAAZOEQ","AAABHGYF","AAABHGYG","AAAAFHIT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFWW" (
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  "AAAAFGCR" int(11) NOT NULL default '0',
  "AAAAFGCS" int(11) NOT NULL default '0',
  "AAAAFGCT" int(11) NOT NULL default '0',
  "AAAAFFXB" int(11) NOT NULL default '0',
  "AAAAFFXC" int(11) NOT NULL default '0',
  "AAAAFGCU" int(11) NOT NULL default '0',
  "AAAAFGCW" int(11) NOT NULL default '0',
  "AAAAFGCX" int(11) NOT NULL default '0',
  "AAAAFGCY" int(11) NOT NULL default '0',
  "AAAAFGCZ" int(11) NOT NULL default '0',
  "AAAAFGDA" int(11) NOT NULL default '0',
  "AAAAFGDB" int(11) NOT NULL default '0',
  "AAAAFGDC" int(11) NOT NULL default '0',
  "AAAAFGDD" int(11) NOT NULL default '0',
  "AAAAFGDE" int(11) NOT NULL default '0',
  "AAAAFGDF" int(11) NOT NULL default '0',
  "AAAAFGDG" int(11) NOT NULL default '0',
  "AAAAFGDI" int(11) NOT NULL default '0',
  "AAAAFGDJ" int(11) NOT NULL default '0',
  "AAAAFGDK" int(11) NOT NULL default '0',
  "AAAAFGDL" int(11) NOT NULL default '0',
  "AAAAFGDM" int(11) NOT NULL default '0',
  "AAAAFGDN" int(11) NOT NULL default '0',
  "AAAAFJTM" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFXX" (
  "AAAAFJTO" varchar(8) NOT NULL default '00000000',
  "AAAAFJTP" varchar(6) NOT NULL default '000000',
  "AAAAFJTQ" varchar(8) NOT NULL default ' ',
  "AAABKUHD" varchar(8) NOT NULL default ' ',
  "AAABCHWX" smallint(6) NOT NULL default '0',
  "AAAAZOEQ" varchar(8) NOT NULL default ' ',
  "AAAAKFCZ" varchar(18) NOT NULL default ' ',
  "AAABFVMJ" varchar(8) NOT NULL default ' ',
  "AAAAVQFD" varchar(8) NOT NULL default ' ',
  "AAAAYGKR" smallint(6) NOT NULL default '0',
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAAAFGCH" varchar(36) NOT NULL default ' ',
  "AAAAFGCI" varchar(3) NOT NULL default ' ',
  "AAAAFGCJ" varchar(3) NOT NULL default ' ',
  "AAAAFJTM" varchar(3) NOT NULL default ' ',
  "AAAAFGCL" varchar(3) NOT NULL default ' ',
  "AAAAFGCM" varchar(40) NOT NULL default ' ',
  "AAAAFGCN" varchar(8) NOT NULL default ' ',
  "AAABLMHC" varchar(26) NOT NULL default ' ',
  "AAAAFGCP" varchar(26) NOT NULL default ' ',
  "AAAAFGCQ" varchar(26) NOT NULL default ' ',
  "AAAAFGCR" int(11) NOT NULL default '0',
  "AAAAFGCS" int(11) NOT NULL default '0',
  "AAAAFGCT" int(11) NOT NULL default '0',
  "AAAAFGCU" int(11) NOT NULL default '0',
  "AAAAFGCV" varchar(26) NOT NULL default ' ',
  "AAAAFGCW" int(11) NOT NULL default '0',
  "AAAAFGCX" int(11) NOT NULL default '0',
  "AAAAFGCY" int(11) NOT NULL default '0',
  "AAAAFGCZ" int(11) NOT NULL default '0',
  "AAAAFGDA" int(11) NOT NULL default '0',
  "AAAAFGDB" int(11) NOT NULL default '0',
  "AAAAFGDC" int(11) NOT NULL default '0',
  "AAAAFGDD" int(11) NOT NULL default '0',
  "AAAAFGDE" int(11) NOT NULL default '0',
  "AAAAFGDF" int(11) NOT NULL default '0',
  "AAAAFGDG" int(11) NOT NULL default '0',
  "AAAAFGDH" varchar(26) NOT NULL default ' ',
  "AAAAFGDI" int(11) NOT NULL default '0',
  "AAAAFGDJ" int(11) NOT NULL default '0',
  "AAAAFGDK" int(11) NOT NULL default '0',
  "AAAAFGDL" int(11) NOT NULL default '0',
  "AAAAFGDM" int(11) NOT NULL default '0',
  "AAAAFGDN" int(11) NOT NULL default '0',
  "AAAAFGDO" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAAAFJTO","AAAAFJTP","AAAAFJTQ","AAABKUHD","AAABCHWX","AAAAZOEQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFFZX" (
  "AAABLENF" varchar(14) NOT NULL default ' ',
  "AAABLENG" varchar(3) NOT NULL default ' ',
  "AAAAFGHY" varchar(44) NOT NULL default ' ',
  "AAAAFGAB" int(11) NOT NULL default '0',
  "AAAAFGAC" int(11) NOT NULL default '0',
  "AAAAFGAD" int(11) NOT NULL default '0',
  "AAABIYVQ" varchar(6) NOT NULL default ' ',
  "AAAAFGAF" int(11) NOT NULL default '0',
  "AAAAFGAG" int(11) NOT NULL default '0',
  "AAAAFGAH" varchar(9) NOT NULL default ' ',
  "AAAAFGAI" int(11) NOT NULL default '0',
  "AAAAFGAJ" int(11) NOT NULL default '0',
  "AAAAFGAK" int(11) NOT NULL default '0',
  "AAAAFGAL" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLENF","AAABLENG","AAAAFGHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFGAP" (
  "AAABLENF" varchar(14) NOT NULL default ' ',
  "AAABLENG" varchar(3) NOT NULL default ' ',
  "AAAAFGBM" varchar(1) NOT NULL default ' ',
  "AAAATPHA" varchar(44) NOT NULL default ' ',
  "AAAAFGAU" varchar(1) NOT NULL default ' ',
  "AAAAFGAV" varchar(1) NOT NULL default ' ',
  "AAAAFGAW" int(11) NOT NULL default '0',
  "AAAAFGBQ" int(11) NOT NULL default '0',
  "AAAAFGBR" varchar(255) NOT NULL default ' ',
  "AAABGHMU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLENF","AAABLENG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFGBC" (
  "AAABLENF" varchar(14) NOT NULL default ' ',
  "AAABLENG" varchar(3) NOT NULL default ' ',
  "AAABLUVE" int(11) NOT NULL default '0',
  "AAAAFGBG" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABLENF","AAABLENG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFGBJ" (
  "AAABLENF" varchar(14) NOT NULL default ' ',
  "AAABLENG" varchar(3) NOT NULL default ' ',
  "AAAAFGBM" varchar(1) NOT NULL default ' ',
  "AAAAFGHY" varchar(44) NOT NULL default ' ',
  "AAAATOVD" varchar(8) NOT NULL default ' ',
  "AAAAFGBP" varchar(2) NOT NULL default ' ',
  "AAAAFGBQ" int(11) NOT NULL default '0',
  "AAAAFGBR" varchar(255) NOT NULL default ' ',
  "AAABGHMU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLENF","AAABLENG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFGBV" (
  "AAAAFJTO" varchar(8) NOT NULL default '00000000',
  "AAAAFJTP" varchar(6) NOT NULL default '000000',
  "AAAAFJTQ" varchar(8) NOT NULL default ' ',
  "AAABKUHD" varchar(8) NOT NULL default ' ',
  "AAABCHWX" smallint(6) NOT NULL default '0',
  "AAAAZOEQ" varchar(8) NOT NULL default ' ',
  "AAAAKFCZ" varchar(18) NOT NULL default ' ',
  "AAABFVMJ" varchar(8) NOT NULL default ' ',
  "AAAAVQFD" varchar(8) NOT NULL default ' ',
  "AAAAYGKR" smallint(6) NOT NULL default '0',
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAAAFGCH" varchar(36) NOT NULL default ' ',
  "AAAAFGCI" varchar(3) NOT NULL default ' ',
  "AAAAFGCJ" varchar(3) NOT NULL default ' ',
  "AAAAFJTM" varchar(3) NOT NULL default ' ',
  "AAAAFGCL" varchar(3) NOT NULL default ' ',
  "AAAAFGCM" varchar(40) NOT NULL default ' ',
  "AAAAFGCN" varchar(8) NOT NULL default ' ',
  "AAABLMHC" varchar(26) NOT NULL default ' ',
  "AAAAFGCP" varchar(26) NOT NULL default ' ',
  "AAAAFGCQ" varchar(26) NOT NULL default ' ',
  "AAAAFGCR" int(11) NOT NULL default '0',
  "AAAAFGCS" int(11) NOT NULL default '0',
  "AAAAFGCT" int(11) NOT NULL default '0',
  "AAAAFGCU" int(11) NOT NULL default '0',
  "AAAAFGCV" varchar(26) NOT NULL default ' ',
  "AAAAFGCW" int(11) NOT NULL default '0',
  "AAAAFGCX" int(11) NOT NULL default '0',
  "AAAAFGCY" int(11) NOT NULL default '0',
  "AAAAFGCZ" int(11) NOT NULL default '0',
  "AAAAFGDA" int(11) NOT NULL default '0',
  "AAAAFGDB" int(11) NOT NULL default '0',
  "AAAAFGDC" int(11) NOT NULL default '0',
  "AAAAFGDD" int(11) NOT NULL default '0',
  "AAAAFGDE" int(11) NOT NULL default '0',
  "AAAAFGDF" int(11) NOT NULL default '0',
  "AAAAFGDG" int(11) NOT NULL default '0',
  "AAAAFGDH" varchar(26) NOT NULL default ' ',
  "AAAAFGDI" int(11) NOT NULL default '0',
  "AAAAFGDJ" int(11) NOT NULL default '0',
  "AAAAFGDK" int(11) NOT NULL default '0',
  "AAAAFGDL" int(11) NOT NULL default '0',
  "AAAAFGDM" int(11) NOT NULL default '0',
  "AAAAFGDN" int(11) NOT NULL default '0',
  "AAAAFGDO" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAAAFJTO","AAAAFJTP","AAAAFJTQ","AAABKUHD","AAABCHWX","AAAAZOEQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFGDV" (
  "AAABLENF" varchar(14) NOT NULL default ' ',
  "AAABLENG" varchar(3) NOT NULL default ' ',
  "AAAAFGDY" smallint(6) NOT NULL default '0',
  "AAABKNBI" text,
  PRIMARY KEY ("AAABLENF","AAABLENG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFGEC" (
  "AAABLENF" varchar(14) NOT NULL default ' ',
  "AAABLENG" varchar(3) NOT NULL default ' ',
  "AAABJLTX" int(11) NOT NULL default '0',
  "AAAAFHGW" int(11) NOT NULL default '0',
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  "AAABMLPV" varchar(31) NOT NULL default ' ',
  "AAAAYGKR" smallint(6) NOT NULL default '0',
  "AAABFWLV" varchar(8) NOT NULL default ' ',
  "AAAAFMPV" varchar(20) NOT NULL default ' ',
  "AAABLSBE" varchar(1) NOT NULL default ' ',
  "AAAAFGEN" varchar(54) NOT NULL default ' ',
  "AAAAFGEO" varchar(8) NOT NULL default ' ',
  "AAAAFGEP" smallint(6) NOT NULL default '0',
  "AAAAFGEQ" varchar(54) NOT NULL default ' ',
  "AAAAFGER" varchar(8) NOT NULL default ' ',
  "AAAAFGES" smallint(6) NOT NULL default '0',
  "AAAAFGET" varchar(54) NOT NULL default ' ',
  "AAAAFGEU" varchar(8) NOT NULL default ' ',
  "AAAAFGEV" smallint(6) NOT NULL default '0',
  "AAAAFGEW" varchar(54) NOT NULL default ' ',
  "AAAAFGEX" varchar(8) NOT NULL default ' ',
  "AAAAFGEY" smallint(6) NOT NULL default '0',
  "AAAAFGEZ" varchar(54) NOT NULL default ' ',
  "AAAAFGFA" varchar(8) NOT NULL default ' ',
  "AAAAFGFB" smallint(6) NOT NULL default '0',
  "AAAAFGFC" varchar(54) NOT NULL default ' ',
  "AAAAFGFD" varchar(8) NOT NULL default ' ',
  "AAAAFGFE" smallint(6) NOT NULL default '0',
  "AAAAFGFF" varchar(54) NOT NULL default ' ',
  "AAAAFGFG" varchar(8) NOT NULL default ' ',
  "AAAAFGFH" smallint(6) NOT NULL default '0',
  "AAAAFGFI" varchar(54) NOT NULL default ' ',
  "AAAAFGFJ" varchar(8) NOT NULL default ' ',
  "AAAAFGFK" smallint(6) NOT NULL default '0',
  "AAAAFGFL" varchar(54) NOT NULL default ' ',
  "AAAAFGFM" varchar(8) NOT NULL default ' ',
  "AAAAFGFN" smallint(6) NOT NULL default '0',
  "AAAAFGFO" varchar(54) NOT NULL default ' ',
  "AAAAFGFP" varchar(8) NOT NULL default ' ',
  "AAAAFGFQ" smallint(6) NOT NULL default '0',
  "AAAAFGFR" varchar(54) NOT NULL default ' ',
  "AAAAFGFS" varchar(8) NOT NULL default ' ',
  "AAAAFGFT" smallint(6) NOT NULL default '0',
  "AAAAFGFU" varchar(54) NOT NULL default ' ',
  "AAAAFGFV" varchar(8) NOT NULL default ' ',
  "AAAAFGFW" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAABLENF","AAABLENG","AAABJLTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFGGA" (
  "AAABLENF" varchar(14) NOT NULL default ' ',
  "AAABLENG" varchar(3) NOT NULL default ' ',
  "AAAAFHGW" int(11) NOT NULL default '0',
  "AAABLMLR" int(11) NOT NULL default '0',
  "AAAAFGGF" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLENF","AAABLENG","AAAAFHGW","AAABLMLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFGGK" (
  "AAABLENF" varchar(14) NOT NULL default ' ',
  "AAABLENG" varchar(3) NOT NULL default ' ',
  "AAABJLTX" int(11) NOT NULL default '0',
  "AAABLUVE" int(11) NOT NULL default '0',
  "AAABMRCK" varchar(254) NOT NULL default ' ',
  PRIMARY KEY ("AAABLENF","AAABLENG","AAABJLTX","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFGGU" (
  "AAABLENF" varchar(14) NOT NULL default ' ',
  "AAABLENG" varchar(3) NOT NULL default ' ',
  "AAAAFGGX" smallint(6) NOT NULL default '0',
  "AAAAFGGY" varchar(8) NOT NULL default ' ',
  "AAAAFGGZ" varchar(8) NOT NULL default ' ',
  "AAABFWLV" varchar(8) NOT NULL default ' ',
  "AAAAFGHB" varchar(8) NOT NULL default ' ',
  "AAAAFGHC" int(11) NOT NULL default '0',
  "AAAAVNMQ" smallint(6) NOT NULL default '0',
  "AAAAFGHE" varchar(1) NOT NULL default ' ',
  "AAAAFGHF" int(11) NOT NULL default '0',
  "AAAAFGHG" int(11) NOT NULL default '0',
  "AAABCKCB" smallint(6) NOT NULL default '0',
  "AAABHEYE" int(11) NOT NULL default '0',
  "AAAAFGHJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLENF","AAABLENG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFGHM" (
  "AAABFZUL" varchar(10) NOT NULL default ' ',
  "AAABFVMJ" varchar(8) NOT NULL default ' ',
  "AAAAVQFD" varchar(8) NOT NULL default ' ',
  "AAAAFGIK" int(11) NOT NULL default '0',
  "AAAAFGIL" int(11) NOT NULL default '0',
  "AAAAFGHS" varchar(12) NOT NULL default ' ',
  "AAABMKFU" varchar(26) NOT NULL default ' ',
  "AAAAFGHU" varchar(1) NOT NULL default ' ',
  "AAAAFGHV" int(11) NOT NULL default '0',
  "AAABLFZT" varchar(8) NOT NULL default ' ',
  "AAAAFGHX" varchar(1) NOT NULL default ' ',
  "AAAAFGHY" varchar(44) NOT NULL default ' ',
  "AAAAFGHZ" varchar(1) NOT NULL default ' ',
  "AAAAFGIA" varchar(2) NOT NULL default ' ',
  "AAAAFGIB" varchar(1) NOT NULL default ' ',
  "AAABLGEL" varchar(1) NOT NULL default ' ',
  "AAAAFGID" varchar(12) NOT NULL default ' ',
  "AAAAFGIE" varchar(8) NOT NULL default ' ',
  "AAABJSDF" smallint(6) NOT NULL default '0',
  "AAAAFGIG" text,
  PRIMARY KEY ("AAABFZUL","AAABFVMJ","AAAAVQFD","AAAAFGIK","AAAAFGIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFGIM" (
  "AAAAFJNY" varchar(8) NOT NULL default ' ',
  "AAAAFJRO" int(11) NOT NULL default '0',
  "AAAAFGXW" varchar(8) NOT NULL default '00000000',
  "AAAAFGXX" varchar(6) NOT NULL default '000000',
  "AAAAFJPW" int(11) NOT NULL default '0',
  "AAAAFIRX" decimal(16,0) NOT NULL default '0',
  "AAAAFIRY" decimal(16,0) NOT NULL default '0',
  "AAAAFIRZ" decimal(16,0) NOT NULL default '0',
  "AAAAFISA" decimal(16,0) NOT NULL default '0',
  "AAAAFISB" decimal(16,0) NOT NULL default '0',
  "AAAAFISC" decimal(16,0) NOT NULL default '0',
  "AAAAFISD" decimal(16,0) NOT NULL default '0',
  "AAAAFISE" decimal(16,0) NOT NULL default '0',
  "AAAAFISF" decimal(16,0) NOT NULL default '0',
  "AAAAFISG" decimal(16,0) NOT NULL default '0',
  "AAAAFISH" decimal(16,0) NOT NULL default '0',
  "AAAAFISI" decimal(16,0) NOT NULL default '0',
  "AAAAFISJ" decimal(16,0) NOT NULL default '0',
  "AAAAFISK" decimal(16,0) NOT NULL default '0',
  "AAAAFISL" decimal(16,0) NOT NULL default '0',
  "AAAAFISM" decimal(16,0) NOT NULL default '0',
  "AAAAFISN" decimal(16,0) NOT NULL default '0',
  "AAAAFISO" decimal(16,0) NOT NULL default '0',
  "AAAAFISP" int(11) NOT NULL default '0',
  "AAAAFISQ" int(11) NOT NULL default '0',
  "AAAAFISR" int(11) NOT NULL default '0',
  "AAAAFISS" int(11) NOT NULL default '0',
  "AAAAFIST" int(11) NOT NULL default '0',
  "AAAAFISU" int(11) NOT NULL default '0',
  "AAAAFISV" int(11) NOT NULL default '0',
  "AAAAFISW" int(11) NOT NULL default '0',
  "AAAAFISX" int(11) NOT NULL default '0',
  "AAAAFISY" int(11) NOT NULL default '0',
  "AAAAFISZ" int(11) NOT NULL default '0',
  "AAAAFITA" int(11) NOT NULL default '0',
  "AAAAFITB" decimal(16,0) NOT NULL default '0',
  "AAAAFITC" decimal(16,0) NOT NULL default '0',
  "AAAAFITD" decimal(16,0) NOT NULL default '0',
  "AAAAFITE" decimal(16,0) NOT NULL default '0',
  "AAAAFITF" decimal(16,0) NOT NULL default '0',
  "AAAAFITG" decimal(16,0) NOT NULL default '0',
  "AAAAFITH" int(11) NOT NULL default '0',
  "AAAAFITI" int(11) NOT NULL default '0',
  "AAAAFITJ" int(11) NOT NULL default '0',
  "AAAAFITK" int(11) NOT NULL default '0',
  "AAAAFITL" int(11) NOT NULL default '0',
  "AAAAFITM" int(11) NOT NULL default '0',
  "AAAAFITN" int(11) NOT NULL default '0',
  "AAAAFITO" decimal(16,0) NOT NULL default '0',
  "AAAAFITP" decimal(16,0) NOT NULL default '0',
  "AAAAFITQ" varchar(10) NOT NULL default ' ',
  "AAAAFITR" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAAFJNY","AAAAFJRO","AAAAFGXW","AAAAFGXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFGKR" (
  "AAAAFGKW" int(11) NOT NULL default '0',
  "AAAAFGKT" varchar(1) NOT NULL default ' ',
  "AAAAFGKU" varchar(1) NOT NULL default ' ',
  "AAAAFGKV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFGKW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFGKX" (
  "AAAAFGLK" varchar(8) NOT NULL default ' ',
  "AAAAFGLL" varchar(18) NOT NULL default ' ',
  "AAAAFJSD" varchar(8) NOT NULL default ' ',
  "AAAAFJSE" varchar(18) NOT NULL default ' ',
  "AAAAFJRT" varchar(18) NOT NULL default ' ',
  "AAAAFGLD" varchar(10) NOT NULL default ' ',
  "AAAAFGLE" int(11) NOT NULL default '0',
  "AAAAFGLF" varchar(3) NOT NULL default ' ',
  "AAAAFGLG" varchar(3) NOT NULL default ' ',
  "AAAAFGLH" varchar(3) NOT NULL default ' ',
  "AAAAFJRY" varchar(3) NOT NULL default ' ',
  "AAAAFJRZ" varchar(26) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFGLK","AAAAFGLL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFGLM" (
  "AAAAFJTO" varchar(8) NOT NULL default '00000000',
  "AAAAFJTP" varchar(6) NOT NULL default '000000',
  "AAAAFGMT" varchar(8) NOT NULL default ' ',
  "AAAAFGMU" varchar(18) NOT NULL default ' ',
  "AAAAFGMV" smallint(6) NOT NULL default '0',
  "AAAAZOEQ" varchar(8) NOT NULL default ' ',
  "AAABMLPV" varchar(18) NOT NULL default ' ',
  "AAAAFGLU" varchar(8) NOT NULL default ' ',
  "AAAAFNKK" varchar(18) NOT NULL default ' ',
  "AAAAFGLW" varchar(1) NOT NULL default ' ',
  "AAABEEPR" varchar(1) NOT NULL default ' ',
  "AAAAFGLY" varchar(8) NOT NULL default ' ',
  "AAAAFGLZ" varchar(1) NOT NULL default ' ',
  "AAAAFJTB" varchar(1) NOT NULL default ' ',
  "AAAAFJSQ" varchar(1) NOT NULL default ' ',
  "AAAAFGMC" smallint(6) NOT NULL default '0',
  "AAAAGEHC" smallint(6) NOT NULL default '0',
  "AAABFVMJ" varchar(8) NOT NULL default ' ',
  "AAAAFJSN" varchar(8) NOT NULL default ' ',
  "AAAAFGMG" smallint(6) NOT NULL default '0',
  "AAAAFJTD" varchar(8) NOT NULL default ' ',
  "AAAAFGMI" int(11) NOT NULL default '0',
  "AAAAPWCS" varchar(1) NOT NULL default ' ',
  "AAAAFJSU" smallint(6) NOT NULL default '0',
  "AAABLDBD" smallint(6) NOT NULL default '0',
  "AAABLJJO" varchar(26) NOT NULL default ' ',
  "AAABBHVC" int(11) NOT NULL default '0',
  "AAAAFNKT" int(11) NOT NULL default '0',
  "AAAAFJTM" int(11) NOT NULL default '0',
  "AAABKNCK" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFJTO","AAAAFJTP","AAAAFGMT","AAAAFGMU","AAAAFGMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFGMW" (
  "AAABLTJH" varchar(18) NOT NULL default ' ',
  "AAAAZOEQ" varchar(18) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAAAFGNF" int(11) NOT NULL default '0',
  "AAAAFGNB" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTJH","AAAAZOEQ","AAABMOHM","AAAAFGNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFGNG" (
  "AAABLTJH" varchar(18) NOT NULL default ' ',
  "AAAAZOEQ" varchar(18) NOT NULL default ' ',
  "AAAAFGNJ" varchar(1) NOT NULL default ' ',
  "AAAAFGNK" varchar(8) NOT NULL default '00000000',
  "AAABLRQM" varchar(18) NOT NULL default ' ',
  "AAAAQXJP" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLTJH","AAAAZOEQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFGNP" (
  "AAAAGLGD" varchar(18) NOT NULL default ' ',
  "AAABLTJH" varchar(18) NOT NULL default ' ',
  "AAABCOIC" varchar(7) NOT NULL default ' ',
  "AAAAFGNT" varchar(8) NOT NULL default '00000000',
  "AAAAFGNU" varchar(6) NOT NULL default '000000',
  "AAAAFGNV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAGLGD","AAABLTJH","AAABCOIC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFGNZ" (
  "AAAAFGOC" varchar(20) NOT NULL default ' ',
  "AAAAFGOB" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFGOC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFGOD" (
  "AAAAFGOZ" varchar(4) NOT NULL default ' ',
  "AAAAFGPA" varchar(8) NOT NULL default '00000000',
  "AAAAFGPB" varchar(6) NOT NULL default '000000',
  "AAAAFGPC" varchar(6) NOT NULL default ' ',
  "AAABFVMJ" varchar(8) NOT NULL default ' ',
  "AAAAVQFD" varchar(8) NOT NULL default ' ',
  "AAAAFGOK" varchar(3) NOT NULL default ' ',
  "AAAAFGOL" varchar(10) NOT NULL default ' ',
  "AAAAFGOM" int(11) NOT NULL default '0',
  "AAAAFGON" VARBINARY(255),
  "AAAAFGOO" int(11) NOT NULL default '0',
  "AAAAFGOP" varchar(128) NOT NULL default ' ',
  "AAAAFGOQ" varchar(128) NOT NULL default ' ',
  "AAAAFJQM" varchar(18) NOT NULL default ' ',
  "AAAAFJQN" varchar(32) NOT NULL default ' ',
  "AAAAFIRN" varchar(200) NOT NULL default ' ',
  "AAAAFIRO" varchar(18) NOT NULL default ' ',
  "AAAAFIRP" varchar(128) NOT NULL default ' ',
  "AAAAFIXU" varchar(128) NOT NULL default ' ',
  "AAAAFJPR" varchar(16) NOT NULL default ' ',
  "AAABJTDV" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFGOZ","AAAAFGPA","AAAAFGPB","AAAAFGPC","AAABFVMJ","AAAAVQFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFGPF" (
  "AAAAFHJM" varchar(4) NOT NULL default ' ',
  "AAAAFHJN" varchar(12) NOT NULL default ' ',
  "AAAAFHJO" varchar(8) NOT NULL default '00000000',
  "AAAAFHJP" varchar(6) NOT NULL default '000000',
  "AAAAFHJQ" varchar(6) NOT NULL default ' ',
  "AAAAFHJA" varchar(128) NOT NULL default ' ',
  "AAAAFHJB" varchar(8) NOT NULL default ' ',
  "AAAAFHJC" varchar(16) NOT NULL default ' ',
  "AAAAFHJD" varchar(128) NOT NULL default ' ',
  "AAAAFHJE" varchar(128) NOT NULL default ' ',
  "AAAAFHJF" varchar(32) NOT NULL default ' ',
  "AAAAFHJG" varchar(18) NOT NULL default ' ',
  "AAAAFHJH" varchar(8) NOT NULL default ' ',
  "AAAAFHJI" varchar(8) NOT NULL default ' ',
  "AAAAFHJJ" varchar(10) NOT NULL default '0000000000',
  "AAAAFHJK" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAAAFHJM","AAAAFHJN","AAAAFHJO","AAAAFHJP","AAAAFHJQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFGQB" (
  "AAAAFHLI" varchar(4) NOT NULL default ' ',
  "AAAAFHLJ" varchar(12) NOT NULL default ' ',
  "AAAAFHLK" varchar(8) NOT NULL default '00000000',
  "AAAAFHLL" varchar(6) NOT NULL default '000000',
  "AAAAFHLM" varchar(6) NOT NULL default ' ',
  "AAAAFHLN" int(11) NOT NULL default '0',
  "AAAAFHJY" varchar(20) NOT NULL default ' ',
  "AAAAFHJZ" varchar(16) NOT NULL default ' ',
  "AAAAFHKA" varchar(18) NOT NULL default ' ',
  "AAAAFHKB" varchar(10) NOT NULL default ' ',
  "AAAAFHKC" varchar(3) NOT NULL default ' ',
  "AAAAFHKD" varchar(128) NOT NULL default ' ',
  "AAAAFHKE" varchar(128) NOT NULL default ' ',
  "AAAAFHKF" varchar(8) NOT NULL default ' ',
  "AAAAFHKG" varchar(16) NOT NULL default ' ',
  "AAAAFHKH" varchar(128) NOT NULL default ' ',
  "AAAAFHKI" varchar(32) NOT NULL default ' ',
  "AAAAFHKJ" varchar(18) NOT NULL default ' ',
  "AAAAFHKK" varchar(12) NOT NULL default ' ',
  "AAAAFHKL" int(11) NOT NULL default '0',
  "AAAAFHKM" varchar(8) NOT NULL default ' ',
  "AAAAFHKN" varchar(3) NOT NULL default ' ',
  "AAAAFHKO" varchar(12) NOT NULL default ' ',
  "AAAAFHKP" VARBINARY(255),
  "AAAAFHKQ" varchar(128) NOT NULL default ' ',
  "AAAAFHKR" varchar(128) NOT NULL default ' ',
  "AAAAFHKS" varchar(8) NOT NULL default ' ',
  "AAAAFHKT" varchar(16) NOT NULL default ' ',
  "AAAAFHKU" varchar(128) NOT NULL default ' ',
  "AAAAFHKV" varchar(32) NOT NULL default ' ',
  "AAAAFHKW" varchar(18) NOT NULL default ' ',
  "AAAAFHKX" varchar(12) NOT NULL default ' ',
  "AAAAFHKY" int(11) NOT NULL default '0',
  "AAAAFHKZ" varchar(8) NOT NULL default ' ',
  "AAAAFHLA" varchar(6) NOT NULL default ' ',
  "AAAAFHLB" varchar(3) NOT NULL default ' ',
  "AAAAFHLC" varchar(12) NOT NULL default ' ',
  "AAAAFHLD" VARBINARY(255),
  "AAAAFHLE" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAAAFHLI","AAAAFHLJ","AAAAFHLK","AAAAFHLL","AAAAFHLM","AAAAFHLN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFGRV" (
  "AAAAFHLY" varchar(4) NOT NULL default ' ',
  "AAAAFHLZ" varchar(8) NOT NULL default '00000000',
  "AAAAFHMA" varchar(6) NOT NULL default '000000',
  "AAAAFHMB" varchar(6) NOT NULL default ' ',
  "AAAAFHLT" varchar(8) NOT NULL default ' ',
  "AAAAFHLU" varchar(8) NOT NULL default ' ',
  "AAAAFHLV" varchar(10) NOT NULL default ' ',
  "AAAAFHLW" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFHLY","AAAAFHLZ","AAAAFHMA","AAAAFHMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFGSI" (
  "AAAAFHMZ" varchar(4) NOT NULL default ' ',
  "AAAAFHNA" varchar(12) NOT NULL default ' ',
  "AAAAFHNB" varchar(8) NOT NULL default '00000000',
  "AAAAFHNC" varchar(6) NOT NULL default '000000',
  "AAAAFHND" varchar(6) NOT NULL default ' ',
  "AAAAFHNE" int(11) NOT NULL default '0',
  "AAAAFHMJ" varchar(1) NOT NULL default ' ',
  "AAAAFHMK" varchar(128) NOT NULL default ' ',
  "AAAAFHML" varchar(128) NOT NULL default ' ',
  "AAAAFHMM" varchar(8) NOT NULL default ' ',
  "AAAAFHMN" varchar(16) NOT NULL default ' ',
  "AAAAFHMO" varchar(128) NOT NULL default ' ',
  "AAAAFHMP" varchar(32) NOT NULL default ' ',
  "AAAAFHMQ" varchar(18) NOT NULL default ' ',
  "AAAAFHMR" varchar(12) NOT NULL default ' ',
  "AAAAFHMS" int(11) NOT NULL default '0',
  "AAAAFHMT" varchar(8) NOT NULL default ' ',
  "AAAAFHMU" varchar(3) NOT NULL default ' ',
  "AAAAFHMV" varchar(12) NOT NULL default ' ',
  "AAAAFHMW" VARBINARY(255),
  PRIMARY KEY ("AAAAFHMZ","AAAAFHNA","AAAAFHNB","AAAAFHNC","AAAAFHND","AAAAFHNE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFGTJ" (
  "AAAAFHOI" varchar(4) NOT NULL default ' ',
  "AAAAFHOJ" varchar(12) NOT NULL default ' ',
  "AAAAFHOK" varchar(8) NOT NULL default '00000000',
  "AAAAFHOL" varchar(6) NOT NULL default '000000',
  "AAAAFHOM" varchar(6) NOT NULL default ' ',
  "AAAAFHNL" varchar(128) NOT NULL default ' ',
  "AAAAFHNM" varchar(128) NOT NULL default ' ',
  "AAAAFHNN" varchar(8) NOT NULL default ' ',
  "AAAAFHNO" varchar(16) NOT NULL default ' ',
  "AAAAFHNP" varchar(128) NOT NULL default ' ',
  "AAAAFHNQ" varchar(32) NOT NULL default ' ',
  "AAAAFHNR" varchar(18) NOT NULL default ' ',
  "AAAAFHNS" varchar(8) NOT NULL default ' ',
  "AAAAFHNT" varchar(8) NOT NULL default ' ',
  "AAAAFHNU" varchar(10) NOT NULL default '0000000000',
  "AAAAFHNV" varchar(20) NOT NULL default ' ',
  "AAAAFHNW" varchar(16) NOT NULL default ' ',
  "AAAAFHNX" varchar(18) NOT NULL default ' ',
  "AAAAFHNY" varchar(10) NOT NULL default ' ',
  "AAAAFHNZ" varchar(3) NOT NULL default ' ',
  "AAAAFHOA" varchar(6) NOT NULL default ' ',
  "AAAAFHOB" varchar(3) NOT NULL default ' ',
  "AAAAFHOC" varchar(12) NOT NULL default ' ',
  "AAAAFHOD" VARBINARY(255),
  "AAAAFHOE" int(11) NOT NULL default '0',
  "AAAAFHOF" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAAAFHOI","AAAAFHOJ","AAAAFHOK","AAAAFHOL","AAAAFHOM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFGUP" (
  "AAAAFHPJ" varchar(4) NOT NULL default ' ',
  "AAAAFHPK" varchar(8) NOT NULL default '00000000',
  "AAAAFHPL" varchar(6) NOT NULL default '000000',
  "AAAAFHPM" varchar(6) NOT NULL default ' ',
  "AAAAFHOS" varchar(8) NOT NULL default ' ',
  "AAAAFHOT" varchar(8) NOT NULL default ' ',
  "AAAAFHOU" varchar(8) NOT NULL default ' ',
  "AAAAFHOV" varchar(5) NOT NULL default '00000',
  "AAAAFHOW" varchar(8) NOT NULL default ' ',
  "AAAAFHOX" varchar(16) NOT NULL default ' ',
  "AAAAFHOY" varchar(12) NOT NULL default ' ',
  "AAAAFHOZ" varchar(12) NOT NULL default ' ',
  "AAAAFHPA" varchar(8) NOT NULL default ' ',
  "AAAAFHPB" varchar(128) NOT NULL default ' ',
  "AAAAFHPC" varchar(128) NOT NULL default ' ',
  "AAAAFHPD" varchar(128) NOT NULL default ' ',
  "AAAAFHPE" varchar(32) NOT NULL default ' ',
  "AAAAFHPF" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFHPJ","AAAAFHPK","AAAAFHPL","AAAAFHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFGVM" (
  "AAAAFGVQ" varchar(50) NOT NULL default ' ',
  "AAABDTAQ" varchar(50) NOT NULL default ' ',
  "AAAAFGVP" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFGVQ","AAABDTAQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFGVS" (
  "AAABLTYE" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLTYE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFGVV" (
  "AAAAVQFA" varchar(50) NOT NULL default ' ',
  "AAAAFGXW" varchar(8) NOT NULL default '00000000',
  "AAAAFGXX" varchar(6) NOT NULL default '000000',
  "AAAAFGXY" varchar(30) NOT NULL default ' ',
  "AAABLUSV" int(11) NOT NULL default '0',
  "AAAAVBBC" int(11) NOT NULL default '0',
  "AAAAFGWC" int(11) NOT NULL default '0',
  "AAAAFGWD" int(11) NOT NULL default '0',
  "AAAAFGWE" int(11) NOT NULL default '0',
  "AAAAFGWF" int(11) NOT NULL default '0',
  "AAAAFGWG" int(11) NOT NULL default '0',
  "AAAAFGWH" int(11) NOT NULL default '0',
  "AAAAFGWI" int(11) NOT NULL default '0',
  "AAAAFGWJ" int(11) NOT NULL default '0',
  "AAAAFGWK" int(11) NOT NULL default '0',
  "AAAAFGWL" int(11) NOT NULL default '0',
  "AAAAFGWM" int(11) NOT NULL default '0',
  "AAAAFGWN" int(11) NOT NULL default '0',
  "AAAAFGWO" int(11) NOT NULL default '0',
  "AAAAFGWP" int(11) NOT NULL default '0',
  "AAAAFGWQ" int(11) NOT NULL default '0',
  "AAAAFGWR" int(11) NOT NULL default '0',
  "AAAAFGWS" int(11) NOT NULL default '0',
  "AAAAFGWT" int(11) NOT NULL default '0',
  "AAAAFGWU" int(11) NOT NULL default '0',
  "AAAAFGWV" int(11) NOT NULL default '0',
  "AAAAFGWW" int(11) NOT NULL default '0',
  "AAAAFGWX" int(11) NOT NULL default '0',
  "AAAAFGWY" int(11) NOT NULL default '0',
  "AAAAFGWZ" int(11) NOT NULL default '0',
  "AAAAFGXA" int(11) NOT NULL default '0',
  "AAAAFGXB" int(11) NOT NULL default '0',
  "AAAAFGXC" int(11) NOT NULL default '0',
  "AAAAFGXD" int(11) NOT NULL default '0',
  "AAAAFGXE" int(11) NOT NULL default '0',
  "AAAAFGXF" int(11) NOT NULL default '0',
  "AAAAFGXG" int(11) NOT NULL default '0',
  "AAAAFGXH" int(11) NOT NULL default '0',
  "AAAAFGXI" int(11) NOT NULL default '0',
  "AAAAFGXJ" int(11) NOT NULL default '0',
  "AAAAFGXK" int(11) NOT NULL default '0',
  "AAAAFGXL" int(11) NOT NULL default '0',
  "AAAAFGXM" int(11) NOT NULL default '0',
  "AAAAFGXN" int(11) NOT NULL default '0',
  "AAAAFGXO" int(11) NOT NULL default '0',
  "AAAAFGXP" int(11) NOT NULL default '0',
  "AAAAFGXQ" int(11) NOT NULL default '0',
  "AAAAVPZF" int(11) NOT NULL default '0',
  "AAAAFGXS" int(11) NOT NULL default '0',
  "AAAAFGXT" int(11) NOT NULL default '0',
  "AAAAFGXU" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAAVQFA","AAAAFGXW","AAAAFGXX","AAAAFGXY","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFGYA" (
  "AAABFVMJ" varchar(8) NOT NULL default ' ',
  "AAAAVQFD" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABFVMJ","AAAAVQFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFGYF" (
  "AAABMLPV" varchar(8) NOT NULL default ' ',
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAAAFGYI" longblob,
  PRIMARY KEY ("AAABMLPV","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFGYL" (
  "AAAAFJNY" varchar(8) NOT NULL default ' ',
  "AAABLEUZ" smallint(6) NOT NULL default '0',
  "AAAAFJRO" int(11) NOT NULL default '0',
  "AAAAFJRP" varchar(8) NOT NULL default ' ',
  "AAAAFJQW" int(11) NOT NULL default '0',
  "AAAAFJQX" int(11) NOT NULL default '0',
  "AAAAFJQY" int(11) NOT NULL default '0',
  "AAAAFJQZ" int(11) NOT NULL default '0',
  "AAAAFJRA" int(11) NOT NULL default '0',
  "AAAAFJRB" int(11) NOT NULL default '0',
  "AAAAFJRC" int(11) NOT NULL default '0',
  "AAAAFJRD" int(11) NOT NULL default '0',
  "AAAAFJRE" int(11) NOT NULL default '0',
  "AAAAFJRF" int(11) NOT NULL default '0',
  "AAAAFJRG" int(11) NOT NULL default '0',
  "AAAAFJRH" int(11) NOT NULL default '0',
  "AAAAFJRI" int(11) NOT NULL default '0',
  "AAAAFJRJ" int(11) NOT NULL default '0',
  "AAAAFJRK" int(11) NOT NULL default '0',
  "AAAAFJRL" varchar(10) NOT NULL default ' ',
  "AAAAFJRM" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAAFJNY","AAABLEUZ","AAAAFJRO","AAAAFJRP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFGZL" (
  "AAAAFJNY" varchar(8) NOT NULL default ' ',
  "AAAAFJRO" int(11) NOT NULL default '0',
  "AAAAFHAW" varchar(8) NOT NULL default ' ',
  "AAAAFHAI" decimal(10,0) NOT NULL default '0',
  "AAAAFHAJ" decimal(10,0) NOT NULL default '0',
  "AAAAFHAK" decimal(10,0) NOT NULL default '0',
  "AAAAFHAL" decimal(10,0) NOT NULL default '0',
  "AAAAFHAM" decimal(10,0) NOT NULL default '0',
  "AAAAFHAN" decimal(10,0) NOT NULL default '0',
  "AAAAFHAO" decimal(10,0) NOT NULL default '0',
  "AAAAFHAP" decimal(10,0) NOT NULL default '0',
  "AAAAFHAQ" decimal(10,0) NOT NULL default '0',
  "AAAAFHAR" decimal(10,0) NOT NULL default '0',
  "AAAAFHAS" decimal(10,0) NOT NULL default '0',
  PRIMARY KEY ("AAAAFJNY","AAAAFJRO","AAAAFHAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFHAD" (
  "AAAAFJNY" varchar(8) NOT NULL default ' ',
  "AAABLEUZ" smallint(6) NOT NULL default '0',
  "AAAAFJRO" int(11) NOT NULL default '0',
  "AAAAFHAW" varchar(8) NOT NULL default ' ',
  "AAAAFHAI" decimal(10,0) NOT NULL default '0',
  "AAAAFHAJ" decimal(10,0) NOT NULL default '0',
  "AAAAFHAK" decimal(10,0) NOT NULL default '0',
  "AAAAFHAL" decimal(10,0) NOT NULL default '0',
  "AAAAFHAM" decimal(10,0) NOT NULL default '0',
  "AAAAFHAN" decimal(10,0) NOT NULL default '0',
  "AAAAFHAO" decimal(10,0) NOT NULL default '0',
  "AAAAFHAP" decimal(10,0) NOT NULL default '0',
  "AAAAFHAQ" decimal(10,0) NOT NULL default '0',
  "AAAAFHAR" decimal(10,0) NOT NULL default '0',
  "AAAAFHAS" decimal(10,0) NOT NULL default '0',
  PRIMARY KEY ("AAAAFJNY","AAABLEUZ","AAAAFJRO","AAAAFHAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFHAX" (
  "AAAAFJNY" varchar(8) NOT NULL default ' ',
  "AAAAFHBT" varchar(8) NOT NULL default ' ',
  "AAAAFHBA" varchar(6) NOT NULL default ' ',
  "AAAAFHBB" varchar(6) NOT NULL default ' ',
  "AAAAFHBC" int(11) NOT NULL default '0',
  "AAAAFHBD" int(11) NOT NULL default '0',
  "AAAAFHBE" varchar(3) NOT NULL default ' ',
  "AAAAFHBF" varchar(3) NOT NULL default ' ',
  "AAAAFHBG" int(11) NOT NULL default '0',
  "AAAAFHBH" int(11) NOT NULL default '0',
  "AAAAFHBI" int(11) NOT NULL default '0',
  "AAAAFHBJ" int(11) NOT NULL default '0',
  "AAAAFHBK" int(11) NOT NULL default '0',
  "AAAAFHBL" int(11) NOT NULL default '0',
  "AAAAFHBM" int(11) NOT NULL default '0',
  "AAAAFHBN" int(11) NOT NULL default '0',
  "AAAAFHBO" varchar(8) NOT NULL default ' ',
  "AAAAFHBP" int(11) NOT NULL default '0',
  "AAAAFHBQ" int(11) NOT NULL default '0',
  "AAAAFHBR" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAAFJNY","AAAAFHBT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFHBU" (
  "AAAAFJNY" varchar(8) NOT NULL default ' ',
  "AAAAFHDL" varchar(8) NOT NULL default ' ',
  "AAAAFHBX" decimal(10,0) NOT NULL default '0',
  "AAAAFHBY" decimal(10,0) NOT NULL default '0',
  "AAAAFHBZ" decimal(10,0) NOT NULL default '0',
  "AAAAFHCA" decimal(10,0) NOT NULL default '0',
  "AAAAFHCB" decimal(10,0) NOT NULL default '0',
  "AAAAFHCC" decimal(10,0) NOT NULL default '0',
  "AAAAFHCD" decimal(10,0) NOT NULL default '0',
  "AAAAFHCE" decimal(10,0) NOT NULL default '0',
  "AAAAFHCF" decimal(10,0) NOT NULL default '0',
  "AAAAFHCG" decimal(10,0) NOT NULL default '0',
  "AAAAFHCH" decimal(10,0) NOT NULL default '0',
  "AAAAFHCI" decimal(10,0) NOT NULL default '0',
  "AAAAFHCJ" decimal(10,0) NOT NULL default '0',
  "AAAAFHCK" decimal(10,0) NOT NULL default '0',
  "AAAAFHCL" decimal(10,0) NOT NULL default '0',
  "AAAAFHCM" decimal(10,0) NOT NULL default '0',
  "AAAAFHCN" decimal(10,0) NOT NULL default '0',
  "AAAAFHCO" decimal(10,0) NOT NULL default '0',
  "AAAAFHCP" decimal(10,0) NOT NULL default '0',
  "AAAAFHCQ" decimal(10,0) NOT NULL default '0',
  "AAAAFHCR" decimal(10,0) NOT NULL default '0',
  "AAAAFHCS" decimal(10,0) NOT NULL default '0',
  "AAAAFHCT" decimal(10,0) NOT NULL default '0',
  "AAAAFHCU" decimal(10,0) NOT NULL default '0',
  "AAAAFHCV" decimal(10,0) NOT NULL default '0',
  "AAAAFHCW" decimal(10,0) NOT NULL default '0',
  "AAAAFHCX" decimal(10,0) NOT NULL default '0',
  "AAAAFHCY" decimal(10,0) NOT NULL default '0',
  "AAAAFHCZ" decimal(10,0) NOT NULL default '0',
  "AAAAFHDA" decimal(10,0) NOT NULL default '0',
  "AAAAFHDB" decimal(10,0) NOT NULL default '0',
  "AAAAFHDC" decimal(10,0) NOT NULL default '0',
  "AAAAFHDD" decimal(10,0) NOT NULL default '0',
  "AAAAFHDE" decimal(10,0) NOT NULL default '0',
  "AAAAFHDF" decimal(10,0) NOT NULL default '0',
  "AAAAFHDG" decimal(10,0) NOT NULL default '0',
  "AAAAFHDH" decimal(10,0) NOT NULL default '0',
  "AAAAFHDI" decimal(10,0) NOT NULL default '0',
  "AAAAFHDJ" decimal(10,0) NOT NULL default '0',
  PRIMARY KEY ("AAAAFJNY","AAAAFHDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFHDM" (
  "AAAAFJNY" varchar(8) NOT NULL default ' ',
  "AAAAFHGH" varchar(8) NOT NULL default ' ',
  "AAAAFHDP" int(11) NOT NULL default '0',
  "AAAAFHDQ" decimal(10,0) NOT NULL default '0',
  "AAAAFHDR" decimal(10,0) NOT NULL default '0',
  "AAAAFHDS" decimal(10,0) NOT NULL default '0',
  "AAAAFHDT" decimal(10,0) NOT NULL default '0',
  "AAAAFHDU" decimal(10,0) NOT NULL default '0',
  "AAAAFHDV" decimal(10,0) NOT NULL default '0',
  "AAAAFHDW" int(11) NOT NULL default '0',
  "AAAAFHDX" decimal(10,0) NOT NULL default '0',
  "AAAAFHDY" int(11) NOT NULL default '0',
  "AAAAFHDZ" decimal(10,0) NOT NULL default '0',
  "AAAAFHEA" decimal(10,0) NOT NULL default '0',
  "AAAAFHEB" decimal(10,0) NOT NULL default '0',
  "AAAAFHEC" decimal(10,0) NOT NULL default '0',
  "AAAAFHED" decimal(10,0) NOT NULL default '0',
  "AAAAFHEE" decimal(10,0) NOT NULL default '0',
  "AAAAFHEF" decimal(10,0) NOT NULL default '0',
  "AAAAFHEG" decimal(10,0) NOT NULL default '0',
  "AAAAFHEH" decimal(10,0) NOT NULL default '0',
  "AAAAFHEI" decimal(10,0) NOT NULL default '0',
  "AAAAFHEJ" decimal(10,0) NOT NULL default '0',
  "AAAAFHEK" decimal(10,0) NOT NULL default '0',
  "AAAAFHEL" decimal(10,0) NOT NULL default '0',
  "AAAAFHEM" decimal(10,0) NOT NULL default '0',
  "AAAAFHEN" decimal(10,0) NOT NULL default '0',
  "AAAAFHEO" decimal(10,0) NOT NULL default '0',
  "AAAAFHEP" decimal(10,0) NOT NULL default '0',
  "AAAAFHEQ" decimal(10,0) NOT NULL default '0',
  "AAAAFHER" decimal(10,0) NOT NULL default '0',
  "AAAAFHES" decimal(10,0) NOT NULL default '0',
  "AAAAFHET" decimal(10,0) NOT NULL default '0',
  "AAAAFHEU" decimal(10,0) NOT NULL default '0',
  "AAAAFHEV" decimal(10,0) NOT NULL default '0',
  "AAAAFHEW" decimal(10,0) NOT NULL default '0',
  "AAAAFHEX" decimal(10,0) NOT NULL default '0',
  "AAAAFHEY" decimal(10,0) NOT NULL default '0',
  "AAAAFHEZ" decimal(10,0) NOT NULL default '0',
  "AAAAFHFA" decimal(10,0) NOT NULL default '0',
  "AAAAFHFB" decimal(10,0) NOT NULL default '0',
  "AAAAFHFC" decimal(10,0) NOT NULL default '0',
  "AAAAFHFD" decimal(10,0) NOT NULL default '0',
  "AAAAFHFE" decimal(10,0) NOT NULL default '0',
  "AAAAFHFF" decimal(10,0) NOT NULL default '0',
  "AAAAFHFG" decimal(10,0) NOT NULL default '0',
  "AAAAFHFH" decimal(10,0) NOT NULL default '0',
  "AAAAFHFI" decimal(10,0) NOT NULL default '0',
  "AAAAFHFJ" decimal(10,0) NOT NULL default '0',
  "AAAAFHFK" decimal(10,0) NOT NULL default '0',
  "AAAAFHFL" decimal(10,0) NOT NULL default '0',
  "AAAAFHFM" decimal(10,0) NOT NULL default '0',
  "AAAAFHFN" decimal(10,0) NOT NULL default '0',
  "AAAAFHFO" decimal(10,0) NOT NULL default '0',
  "AAAAFHFP" decimal(10,0) NOT NULL default '0',
  "AAAAFHFQ" int(11) NOT NULL default '0',
  "AAAAFHFR" int(11) NOT NULL default '0',
  "AAAAFHFS" int(11) NOT NULL default '0',
  "AAAAFHFT" int(11) NOT NULL default '0',
  "AAAAFHFU" decimal(10,0) NOT NULL default '0',
  "AAAAFHFV" decimal(10,0) NOT NULL default '0',
  "AAAAFHFW" decimal(10,0) NOT NULL default '0',
  "AAAAFHFX" decimal(10,0) NOT NULL default '0',
  "AAAAFHFY" decimal(10,0) NOT NULL default '0',
  "AAAAFHFZ" decimal(10,0) NOT NULL default '0',
  "AAAAFHGA" decimal(10,0) NOT NULL default '0',
  "AAAAFHGB" decimal(10,0) NOT NULL default '0',
  "AAAAFHGC" decimal(10,0) NOT NULL default '0',
  "AAAAFHGD" decimal(10,0) NOT NULL default '0',
  "AAAAFHGE" decimal(10,0) NOT NULL default '0',
  "AAAAFHGF" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAAFJNY","AAAAFHGH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFHGI" (
  "AAAAFJNY" varchar(8) NOT NULL default ' ',
  "AAABLUVE" int(11) NOT NULL default '0',
  "AAAAFJRO" int(11) NOT NULL default '0',
  "AAABMRCK" varchar(254) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFJNY","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFHGP" (
  "AAAAFIWY" varchar(32) NOT NULL default ' ',
  "AAAAFHGR" varchar(8) NOT NULL default '00000000',
  "AAAAFHJP" varchar(6) NOT NULL default '000000',
  "AAABLEUZ" smallint(6) NOT NULL default '0',
  "AAABDRVP" smallint(6) NOT NULL default '0',
  "AAAAFJRO" int(11) NOT NULL default '0',
  "AAAAFHGW" int(11) NOT NULL default '0',
  "AAAAFHGX" VARBINARY(255),
  "AAAAFHGY" smallint(6) NOT NULL default '0',
  "AAAAFHGZ" smallint(6) NOT NULL default '0',
  "AAAAFHHA" int(11) NOT NULL default '0',
  "AAAAVQTC" int(11) NOT NULL default '0',
  "AAAAFHHC" smallint(6) NOT NULL default '0',
  "AAAAFHHD" int(11) NOT NULL default '0',
  "AAAAFJNY" varchar(8) NOT NULL default ' ',
  "AAAAFHHF" int(11) NOT NULL default '0',
  "AAAAFHHG" int(11) NOT NULL default '0',
  "AAAAFHHH" int(11) NOT NULL default '0',
  "AAAAFHHI" smallint(6) NOT NULL default '0',
  "AAABHFCH" text,
  PRIMARY KEY ("AAAAFIWY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFHHL" (
  "AAAAFJNY" varchar(8) NOT NULL default ' ',
  "AAABLUVE" int(11) NOT NULL default '0',
  "AAAAFHHU" varchar(1) NOT NULL default ' ',
  "AAAAFHHP" varchar(8) NOT NULL default '00000000',
  "AAAAFHHQ" varchar(6) NOT NULL default '000000',
  "AAABMRCK" varchar(254) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFJNY","AAABLUVE","AAAAFHHU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFHHV" (
  "AAAAFJNY" varchar(8) NOT NULL default ' ',
  "AAABLUVE" int(11) NOT NULL default '0',
  "AAABLEUZ" smallint(6) NOT NULL default '0',
  "AAAAFHHZ" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFJNY","AAABLUVE","AAABLEUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFHID" (
  "AAAAFJNY" varchar(8) NOT NULL default ' ',
  "AAAAFHIT" varchar(8) NOT NULL default ' ',
  "AAAAFHIG" int(11) NOT NULL default '0',
  "AAAAFHIH" int(11) NOT NULL default '0',
  "AAAAFHII" int(11) NOT NULL default '0',
  "AAAAFHIJ" int(11) NOT NULL default '0',
  "AAAAFHIK" int(11) NOT NULL default '0',
  "AAAAFHIL" int(11) NOT NULL default '0',
  "AAAAFHIM" varchar(6) NOT NULL default ' ',
  "AAAAFHIN" int(11) NOT NULL default '0',
  "AAAAFHIO" int(11) NOT NULL default '0',
  "AAAAFHIP" varchar(1) NOT NULL default ' ',
  "AAAAFHIQ" smallint(6) NOT NULL default '0',
  "AAAAFHIR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFJNY","AAAAFHIT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFHIU" (
  "AAAAFHJM" varchar(4) NOT NULL default ' ',
  "AAAAFHJN" varchar(12) NOT NULL default ' ',
  "AAAAFHJO" varchar(8) NOT NULL default '00000000',
  "AAAAFHJP" varchar(6) NOT NULL default '000000',
  "AAAAFHJQ" varchar(6) NOT NULL default ' ',
  "AAAAFHJA" varchar(128) NOT NULL default ' ',
  "AAAAFHJB" varchar(8) NOT NULL default ' ',
  "AAAAFHJC" varchar(16) NOT NULL default ' ',
  "AAAAFHJD" varchar(128) NOT NULL default ' ',
  "AAAAFHJE" varchar(128) NOT NULL default ' ',
  "AAAAFHJF" varchar(32) NOT NULL default ' ',
  "AAAAFHJG" varchar(18) NOT NULL default ' ',
  "AAAAFHJH" varchar(8) NOT NULL default ' ',
  "AAAAFHJI" varchar(8) NOT NULL default ' ',
  "AAAAFHJJ" varchar(10) NOT NULL default '0000000000',
  "AAAAFHJK" varchar(10) NOT NULL default '0000000000',
  "AAAAFIGL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFHJM","AAAAFHJN","AAAAFHJO","AAAAFHJP","AAAAFHJQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFHJR" (
  "AAAAFHLI" varchar(4) NOT NULL default ' ',
  "AAAAFHLJ" varchar(12) NOT NULL default ' ',
  "AAAAFHLK" varchar(8) NOT NULL default '00000000',
  "AAAAFHLL" varchar(6) NOT NULL default '000000',
  "AAAAFHLM" varchar(6) NOT NULL default ' ',
  "AAAAFHLN" int(11) NOT NULL default '0',
  "AAAAFHJY" varchar(20) NOT NULL default ' ',
  "AAAAFHJZ" varchar(16) NOT NULL default ' ',
  "AAAAFHKA" varchar(18) NOT NULL default ' ',
  "AAAAFHKB" varchar(10) NOT NULL default ' ',
  "AAAAFHKC" varchar(3) NOT NULL default ' ',
  "AAAAFHKD" varchar(128) NOT NULL default ' ',
  "AAAAFHKE" varchar(128) NOT NULL default ' ',
  "AAAAFHKF" varchar(8) NOT NULL default ' ',
  "AAAAFHKG" varchar(16) NOT NULL default ' ',
  "AAAAFHKH" varchar(128) NOT NULL default ' ',
  "AAAAFHKI" varchar(32) NOT NULL default ' ',
  "AAAAFHKJ" varchar(18) NOT NULL default ' ',
  "AAAAFHKK" varchar(12) NOT NULL default ' ',
  "AAAAFHKL" int(11) NOT NULL default '0',
  "AAAAFHKM" varchar(8) NOT NULL default ' ',
  "AAAAFHKN" varchar(3) NOT NULL default ' ',
  "AAAAFHKO" varchar(12) NOT NULL default ' ',
  "AAAAFHKP" VARBINARY(255),
  "AAAAFHKQ" varchar(128) NOT NULL default ' ',
  "AAAAFHKR" varchar(128) NOT NULL default ' ',
  "AAAAFHKS" varchar(8) NOT NULL default ' ',
  "AAAAFHKT" varchar(16) NOT NULL default ' ',
  "AAAAFHKU" varchar(128) NOT NULL default ' ',
  "AAAAFHKV" varchar(32) NOT NULL default ' ',
  "AAAAFHKW" varchar(18) NOT NULL default ' ',
  "AAAAFHKX" varchar(12) NOT NULL default ' ',
  "AAAAFHKY" int(11) NOT NULL default '0',
  "AAAAFHKZ" varchar(8) NOT NULL default ' ',
  "AAAAFHLA" varchar(6) NOT NULL default ' ',
  "AAAAFHLB" varchar(3) NOT NULL default ' ',
  "AAAAFHLC" varchar(12) NOT NULL default ' ',
  "AAAAFHLD" VARBINARY(255),
  "AAAAFHLE" int(11) NOT NULL default '0',
  "AAAAFHLF" int(11) NOT NULL default '0',
  "AAAAFHLG" int(11) NOT NULL default '0',
  "AAAAFIGL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFHLI","AAAAFHLJ","AAAAFHLK","AAAAFHLL","AAAAFHLM","AAAAFHLN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFHLO" (
  "AAAAFHLY" varchar(4) NOT NULL default ' ',
  "AAAAFHLZ" varchar(8) NOT NULL default '00000000',
  "AAAAFHMA" varchar(6) NOT NULL default '000000',
  "AAAAFHMB" varchar(6) NOT NULL default ' ',
  "AAAAFHLT" varchar(8) NOT NULL default ' ',
  "AAAAFHLU" varchar(8) NOT NULL default ' ',
  "AAAAFHLV" varchar(10) NOT NULL default ' ',
  "AAAAFHLW" varchar(10) NOT NULL default ' ',
  "AAAAFIGL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFHLY","AAAAFHLZ","AAAAFHMA","AAAAFHMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFHMC" (
  "AAAAFHMZ" varchar(4) NOT NULL default ' ',
  "AAAAFHNA" varchar(12) NOT NULL default ' ',
  "AAAAFHNB" varchar(8) NOT NULL default '00000000',
  "AAAAFHNC" varchar(6) NOT NULL default '000000',
  "AAAAFHND" varchar(6) NOT NULL default ' ',
  "AAAAFHNE" int(11) NOT NULL default '0',
  "AAAAFHMJ" varchar(1) NOT NULL default ' ',
  "AAAAFHMK" varchar(128) NOT NULL default ' ',
  "AAAAFHML" varchar(128) NOT NULL default ' ',
  "AAAAFHMM" varchar(8) NOT NULL default ' ',
  "AAAAFHMN" varchar(16) NOT NULL default ' ',
  "AAAAFHMO" varchar(128) NOT NULL default ' ',
  "AAAAFHMP" varchar(32) NOT NULL default ' ',
  "AAAAFHMQ" varchar(18) NOT NULL default ' ',
  "AAAAFHMR" varchar(12) NOT NULL default ' ',
  "AAAAFHMS" int(11) NOT NULL default '0',
  "AAAAFHMT" varchar(8) NOT NULL default ' ',
  "AAAAFHMU" varchar(3) NOT NULL default ' ',
  "AAAAFHMV" varchar(12) NOT NULL default ' ',
  "AAAAFHMW" VARBINARY(255),
  "AAAAFHMX" int(11) NOT NULL default '0',
  "AAAAFIGL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFHMZ","AAAAFHNA","AAAAFHNB","AAAAFHNC","AAAAFHND","AAAAFHNE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFHNF" (
  "AAAAFHOI" varchar(4) NOT NULL default ' ',
  "AAAAFHOJ" varchar(12) NOT NULL default ' ',
  "AAAAFHOK" varchar(8) NOT NULL default '00000000',
  "AAAAFHOL" varchar(6) NOT NULL default '000000',
  "AAAAFHOM" varchar(6) NOT NULL default ' ',
  "AAAAFHNL" varchar(128) NOT NULL default ' ',
  "AAAAFHNM" varchar(128) NOT NULL default ' ',
  "AAAAFHNN" varchar(8) NOT NULL default ' ',
  "AAAAFHNO" varchar(16) NOT NULL default ' ',
  "AAAAFHNP" varchar(128) NOT NULL default ' ',
  "AAAAFHNQ" varchar(32) NOT NULL default ' ',
  "AAAAFHNR" varchar(18) NOT NULL default ' ',
  "AAAAFHNS" varchar(8) NOT NULL default ' ',
  "AAAAFHNT" varchar(8) NOT NULL default ' ',
  "AAAAFHNU" varchar(10) NOT NULL default '0000000000',
  "AAAAFHNV" varchar(20) NOT NULL default ' ',
  "AAAAFHNW" varchar(16) NOT NULL default ' ',
  "AAAAFHNX" varchar(18) NOT NULL default ' ',
  "AAAAFHNY" varchar(10) NOT NULL default ' ',
  "AAAAFHNZ" varchar(3) NOT NULL default ' ',
  "AAAAFHOA" varchar(6) NOT NULL default ' ',
  "AAAAFHOB" varchar(3) NOT NULL default ' ',
  "AAAAFHOC" varchar(12) NOT NULL default ' ',
  "AAAAFHOD" VARBINARY(255),
  "AAAAFHOE" int(11) NOT NULL default '0',
  "AAAAFHOF" int(11) NOT NULL default '0',
  "AAAAFHOG" int(11) NOT NULL default '0',
  "AAAAFIGL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFHOI","AAAAFHOJ","AAAAFHOK","AAAAFHOL","AAAAFHOM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFHON" (
  "AAAAFHPJ" varchar(4) NOT NULL default ' ',
  "AAAAFHPK" varchar(8) NOT NULL default '00000000',
  "AAAAFHPL" varchar(6) NOT NULL default '000000',
  "AAAAFHPM" varchar(6) NOT NULL default ' ',
  "AAAAFHOS" varchar(8) NOT NULL default ' ',
  "AAAAFHOT" varchar(8) NOT NULL default ' ',
  "AAAAFHOU" varchar(8) NOT NULL default ' ',
  "AAAAFHOV" varchar(5) NOT NULL default '00000',
  "AAAAFHOW" varchar(8) NOT NULL default ' ',
  "AAAAFHOX" varchar(16) NOT NULL default ' ',
  "AAAAFHOY" varchar(12) NOT NULL default ' ',
  "AAAAFHOZ" varchar(12) NOT NULL default ' ',
  "AAAAFHPA" varchar(8) NOT NULL default ' ',
  "AAAAFHPB" varchar(128) NOT NULL default ' ',
  "AAAAFHPC" varchar(128) NOT NULL default ' ',
  "AAAAFHPD" varchar(128) NOT NULL default ' ',
  "AAAAFHPE" varchar(32) NOT NULL default ' ',
  "AAAAFHPF" varchar(18) NOT NULL default ' ',
  "AAAAFIGL" varchar(10) NOT NULL default ' ',
  "AAAAFHPH" varchar(12) NOT NULL default ' ',
  "AAAAFHPI" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAAFHPJ","AAAAFHPK","AAAAFHPL","AAAAFHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFHPN" (
  "AAAAFJNY" varchar(8) NOT NULL default ' ',
  "AAAAFHPP" smallint(6) NOT NULL default '0',
  "AAAAFHPQ" VARBINARY(255),
  "AAAAFHPR" VARBINARY(255),
  "AAAAFHPS" int(11) NOT NULL default '0',
  "AAAAFHPT" VARBINARY(255),
  "AAAAFHPU" VARBINARY(255),
  "AAAAFHPV" int(11) NOT NULL default '0',
  "AAAAFHPW" varchar(4) NOT NULL default ' ',
  "AAAAFHPX" int(11) NOT NULL default '0',
  "AAAAFHPY" int(11) NOT NULL default '0',
  "AAAAFHPZ" int(11) NOT NULL default '0',
  "AAAAFHQA" int(11) NOT NULL default '0',
  "AAAAFHQB" varchar(4) NOT NULL default ' ',
  "AAAAFHQC" varchar(128) NOT NULL default ' ',
  "AAAAFHQD" varchar(8) NOT NULL default ' ',
  "AAAAFHQE" varchar(8) NOT NULL default ' ',
  "AAAAFHQF" varchar(6) NOT NULL default ' ',
  "AAAAFHQG" smallint(6) NOT NULL default '0',
  "AAAAFHQH" int(11) NOT NULL default '0',
  "AAAAFHQI" int(11) NOT NULL default '0',
  "AAAAFHQJ" int(11) NOT NULL default '0',
  "AAAAFHQK" int(11) NOT NULL default '0',
  "AAAAFHQL" int(11) NOT NULL default '0',
  "AAAAFHQM" int(11) NOT NULL default '0',
  "AAAAFHQN" int(11) NOT NULL default '0',
  "AAAAFHQO" int(11) NOT NULL default '0',
  "AAAAFHQP" int(11) NOT NULL default '0',
  "AAAAFHQQ" int(11) NOT NULL default '0',
  "AAAAFHQR" int(11) NOT NULL default '0',
  "AAAAFHQS" int(11) NOT NULL default '0',
  "AAAAFHQT" int(11) NOT NULL default '0',
  "AAAAFHQU" int(11) NOT NULL default '0',
  "AAAAFHQV" int(11) NOT NULL default '0',
  "AAAAFHQW" int(11) NOT NULL default '0',
  "AAAAFHQX" decimal(10,0) NOT NULL default '0',
  "AAAAFHQY" decimal(10,0) NOT NULL default '0',
  "AAAAFHQZ" decimal(10,0) NOT NULL default '0',
  "AAAAFHRA" decimal(10,0) NOT NULL default '0',
  "AAAAFHRB" decimal(10,0) NOT NULL default '0',
  "AAAAFHRC" decimal(10,0) NOT NULL default '0',
  "AAAAFHRD" decimal(10,0) NOT NULL default '0',
  "AAAAFHRE" decimal(10,0) NOT NULL default '0',
  "AAAAFHRF" decimal(10,0) NOT NULL default '0',
  "AAAAFHRG" decimal(10,0) NOT NULL default '0',
  "AAAAFHRH" decimal(10,0) NOT NULL default '0',
  "AAAAFHRI" decimal(10,0) NOT NULL default '0',
  "AAAAFHRJ" decimal(10,0) NOT NULL default '0',
  "AAAAFHRK" decimal(10,0) NOT NULL default '0',
  "AAAAFHRL" decimal(10,0) NOT NULL default '0',
  "AAAAFHRM" decimal(10,0) NOT NULL default '0',
  "AAAAFHRN" decimal(10,0) NOT NULL default '0',
  "AAAAFHRO" decimal(10,0) NOT NULL default '0',
  "AAAAFHRP" decimal(10,0) NOT NULL default '0',
  "AAAAFHRQ" decimal(10,0) NOT NULL default '0',
  "AAAAFHRR" decimal(10,0) NOT NULL default '0',
  "AAAAFHRS" decimal(10,0) NOT NULL default '0',
  "AAAAFHRT" decimal(10,0) NOT NULL default '0',
  "AAAAFHRU" decimal(10,0) NOT NULL default '0',
  "AAAAFHRV" decimal(10,0) NOT NULL default '0',
  "AAAAFHRW" decimal(10,0) NOT NULL default '0',
  "AAAAFHRX" decimal(10,0) NOT NULL default '0',
  "AAAAFHRY" decimal(10,0) NOT NULL default '0',
  "AAAAFHRZ" decimal(10,0) NOT NULL default '0',
  "AAAAFHSA" decimal(10,0) NOT NULL default '0',
  "AAAAFHSB" decimal(10,0) NOT NULL default '0',
  "AAAAFHSC" decimal(10,0) NOT NULL default '0',
  "AAAAFHSD" decimal(10,0) NOT NULL default '0',
  "AAAAFHSE" decimal(10,0) NOT NULL default '0',
  "AAAAFHSF" decimal(10,0) NOT NULL default '0',
  "AAAAFHSG" decimal(10,0) NOT NULL default '0',
  "AAAAFHSH" decimal(10,0) NOT NULL default '0',
  "AAAAFHSI" decimal(10,0) NOT NULL default '0',
  "AAAAFHSJ" decimal(10,0) NOT NULL default '0',
  "AAAAFHSK" decimal(10,0) NOT NULL default '0',
  "AAAAFHSL" decimal(10,0) NOT NULL default '0',
  "AAAAFHSM" varchar(1) NOT NULL default ' ',
  "AAAAFHSN" varchar(1) NOT NULL default ' ',
  "AAAAFHSO" varchar(1) NOT NULL default ' ',
  "AAAAFHSP" varchar(1) NOT NULL default ' ',
  "AAAAFHSQ" varchar(1) NOT NULL default ' ',
  "AAAAFHSR" varchar(1) NOT NULL default ' ',
  "AAAAFHSS" varchar(1) NOT NULL default ' ',
  "AAAAFHST" varchar(1) NOT NULL default ' ',
  "AAAAFHSU" varchar(1) NOT NULL default ' ',
  "AAAAFHSV" varchar(1) NOT NULL default ' ',
  "AAAAFHSW" varchar(1) NOT NULL default ' ',
  "AAAAFHSX" varchar(1) NOT NULL default ' ',
  "AAAAFHSY" varchar(1) NOT NULL default ' ',
  "AAAAFHSZ" varchar(1) NOT NULL default ' ',
  "AAAAFHTA" varchar(1) NOT NULL default ' ',
  "AAAAFHTB" varchar(1) NOT NULL default ' ',
  "AAAAFHTC" varchar(1) NOT NULL default ' ',
  "AAAAFHTD" varchar(1) NOT NULL default ' ',
  "AAAAFHTE" varchar(1) NOT NULL default ' ',
  "AAAAFHTF" varchar(1) NOT NULL default ' ',
  "AAAAFHTG" varchar(1) NOT NULL default ' ',
  "AAAAFHTH" varchar(1) NOT NULL default ' ',
  "AAAAFHTI" varchar(1) NOT NULL default ' ',
  "AAAAFHTJ" varchar(1) NOT NULL default ' ',
  "AAAAFHTK" varchar(1) NOT NULL default ' ',
  "AAAAFHTL" varchar(1) NOT NULL default ' ',
  "AAAAFHTM" varchar(1) NOT NULL default ' ',
  "AAAAFHTN" varchar(1) NOT NULL default ' ',
  "AAAAFHTO" varchar(1) NOT NULL default ' ',
  "AAAAFHTP" varchar(1) NOT NULL default ' ',
  "AAAAFHTQ" varchar(1) NOT NULL default ' ',
  "AAAAFHTR" varchar(1) NOT NULL default ' ',
  "AAAAFHTS" varchar(1) NOT NULL default ' ',
  "AAAAFHTT" varchar(1) NOT NULL default ' ',
  "AAAAFHTU" varchar(1) NOT NULL default ' ',
  "AAAAFHTV" varchar(1) NOT NULL default ' ',
  "AAAAFHTW" int(11) NOT NULL default '0',
  "AAAAFHTX" int(11) NOT NULL default '0',
  "AAAAFHTY" varchar(8) NOT NULL default ' ',
  "AAAAFHTZ" int(11) NOT NULL default '0',
  "AAAAFHUA" int(11) NOT NULL default '0',
  "AAAAFHUB" int(11) NOT NULL default '0',
  "AAAAFHUC" int(11) NOT NULL default '0',
  "AAAAFHUD" int(11) NOT NULL default '0',
  "AAAAFHUE" int(11) NOT NULL default '0',
  "AAAAFHUF" int(11) NOT NULL default '0',
  "AAAAFHUG" int(11) NOT NULL default '0',
  "AAAAFHUH" int(11) NOT NULL default '0',
  "AAAAFHUI" int(11) NOT NULL default '0',
  "AAAAFHUJ" int(11) NOT NULL default '0',
  "AAAAFHUK" int(11) NOT NULL default '0',
  "AAAAFHUL" int(11) NOT NULL default '0',
  "AAAAFHUM" varchar(2) NOT NULL default ' ',
  "AAAAFHUN" smallint(6) NOT NULL default '0',
  "AAAAFHUO" varchar(4) NOT NULL default ' ',
  "AAAAFHUP" int(11) NOT NULL default '0',
  "AAAAFHUQ" int(11) NOT NULL default '0',
  "AAAAFHUR" int(11) NOT NULL default '0',
  "AAAAFHUS" int(11) NOT NULL default '0',
  "AAAAFHUT" int(11) NOT NULL default '0',
  "AAAAFHUU" int(11) NOT NULL default '0',
  "AAAAFHUV" int(11) NOT NULL default '0',
  "AAAAFHUW" int(11) NOT NULL default '0',
  "AAAAFHUX" int(11) NOT NULL default '0',
  "AAAAFHUY" int(11) NOT NULL default '0',
  "AAAAFHUZ" int(11) NOT NULL default '0',
  "AAAAFHVA" int(11) NOT NULL default '0',
  "AAAAFHVB" int(11) NOT NULL default '0',
  "AAAAFHVC" int(11) NOT NULL default '0',
  "AAAAFHVD" int(11) NOT NULL default '0',
  "AAAAFHVE" int(11) NOT NULL default '0',
  "AAAAFHVF" int(11) NOT NULL default '0',
  "AAAAFIYD" int(11) NOT NULL default '0',
  "AAAAFIYE" int(11) NOT NULL default '0',
  "AAAAFIYF" int(11) NOT NULL default '0',
  "AAAAFIYG" int(11) NOT NULL default '0',
  "AAAAFIYH" int(11) NOT NULL default '0',
  "AAAAFIYI" int(11) NOT NULL default '0',
  "AAAAFIYJ" int(11) NOT NULL default '0',
  "AAAAFIYK" int(11) NOT NULL default '0',
  "AAAAFIYL" int(11) NOT NULL default '0',
  "AAAAFIYM" int(11) NOT NULL default '0',
  "AAAAFIYN" decimal(10,0) NOT NULL default '0',
  "AAAAFIYO" decimal(10,0) NOT NULL default '0',
  "AAAAFIYP" decimal(10,0) NOT NULL default '0',
  "AAAAFIYQ" decimal(10,0) NOT NULL default '0',
  "AAAAFIYR" decimal(10,0) NOT NULL default '0',
  "AAAAFIYS" decimal(10,0) NOT NULL default '0',
  "AAAAFIYT" decimal(10,0) NOT NULL default '0',
  "AAAAFIYU" decimal(10,0) NOT NULL default '0',
  "AAAAFIYV" int(11) NOT NULL default '0',
  "AAAAFIUM" int(11) NOT NULL default '0',
  "AAAAFIUN" int(11) NOT NULL default '0',
  "AAAAFIUO" int(11) NOT NULL default '0',
  "AAAAFIUP" int(11) NOT NULL default '0',
  "AAAAFIUQ" int(11) NOT NULL default '0',
  "AAAAFIUR" int(11) NOT NULL default '0',
  "AAAAFIUS" int(11) NOT NULL default '0',
  "AAAAFIUT" int(11) NOT NULL default '0',
  "AAAAFIUU" int(11) NOT NULL default '0',
  "AAAAFIUV" int(11) NOT NULL default '0',
  "AAAAFIUW" int(11) NOT NULL default '0',
  "AAAAFIUX" int(11) NOT NULL default '0',
  "AAAAFIUY" int(11) NOT NULL default '0',
  "AAAAFIUZ" int(11) NOT NULL default '0',
  "AAAAFIVA" int(11) NOT NULL default '0',
  "AAAAFIVB" int(11) NOT NULL default '0',
  "AAAAFIVC" int(11) NOT NULL default '0',
  "AAAAFIVD" int(11) NOT NULL default '0',
  "AAAAFIVE" int(11) NOT NULL default '0',
  "AAAAFIVF" int(11) NOT NULL default '0',
  "AAAAFIVG" int(11) NOT NULL default '0',
  "AAAAFIVH" int(11) NOT NULL default '0',
  "AAAAFIVI" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAAFJNY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFHWX" (
  "AAAAFJNY" varchar(8) NOT NULL default ' ',
  "AAAAFIJC" int(11) NOT NULL default '0',
  "AAAAFIJD" int(11) NOT NULL default '0',
  "AAAAFIJE" int(11) NOT NULL default '0',
  "AAAAFIJF" int(11) NOT NULL default '0',
  "AAAAFIJG" int(11) NOT NULL default '0',
  "AAAAFIJH" int(11) NOT NULL default '0',
  "AAAAFIJI" int(11) NOT NULL default '0',
  "AAAAFIJJ" int(11) NOT NULL default '0',
  "AAAAFIJK" int(11) NOT NULL default '0',
  "AAAAFIJL" int(11) NOT NULL default '0',
  "AAAAFIJM" int(11) NOT NULL default '0',
  "AAAAFIJN" int(11) NOT NULL default '0',
  "AAAAFIJO" int(11) NOT NULL default '0',
  "AAAAFIJP" int(11) NOT NULL default '0',
  "AAAAFIJQ" int(11) NOT NULL default '0',
  "AAAAFIJR" int(11) NOT NULL default '0',
  "AAAAFIJS" int(11) NOT NULL default '0',
  "AAAAFIJT" int(11) NOT NULL default '0',
  "AAAAFIJU" int(11) NOT NULL default '0',
  "AAAAFIJV" int(11) NOT NULL default '0',
  "AAAAFIJW" int(11) NOT NULL default '0',
  "AAAAFIJX" int(11) NOT NULL default '0',
  "AAAAFIJY" int(11) NOT NULL default '0',
  "AAAAFIJZ" int(11) NOT NULL default '0',
  "AAAAFIKA" decimal(10,0) NOT NULL default '0',
  "AAAAFIKB" int(11) NOT NULL default '0',
  "AAAAFIKC" int(11) NOT NULL default '0',
  "AAAAFIKD" int(11) NOT NULL default '0',
  "AAAAFIKE" int(11) NOT NULL default '0',
  "AAAAFIKF" int(11) NOT NULL default '0',
  "AAAAFIKG" int(11) NOT NULL default '0',
  "AAAAFIKH" int(11) NOT NULL default '0',
  "AAAAFIKI" int(11) NOT NULL default '0',
  "AAAAFIKJ" int(11) NOT NULL default '0',
  "AAAAFIKK" int(11) NOT NULL default '0',
  "AAAAFIKL" int(11) NOT NULL default '0',
  "AAAAFIKM" int(11) NOT NULL default '0',
  "AAAAFIKN" int(11) NOT NULL default '0',
  "AAAAFIKO" int(11) NOT NULL default '0',
  "AAAAFIKP" int(11) NOT NULL default '0',
  "AAAAFIKQ" int(11) NOT NULL default '0',
  "AAAAFIKR" int(11) NOT NULL default '0',
  "AAAAFIKS" int(11) NOT NULL default '0',
  "AAAAFIKT" int(11) NOT NULL default '0',
  "AAAAFIKU" int(11) NOT NULL default '0',
  "AAAAFIKV" int(11) NOT NULL default '0',
  "AAAAFIKW" int(11) NOT NULL default '0',
  "AAAAFIKX" int(11) NOT NULL default '0',
  "AAAAFIKY" int(11) NOT NULL default '0',
  "AAAAFIKZ" int(11) NOT NULL default '0',
  "AAAAFILA" int(11) NOT NULL default '0',
  "AAAAFILB" int(11) NOT NULL default '0',
  "AAAAFILC" int(11) NOT NULL default '0',
  "AAAAFILD" int(11) NOT NULL default '0',
  "AAAAFILE" int(11) NOT NULL default '0',
  "AAAAFILF" int(11) NOT NULL default '0',
  "AAAAFILG" int(11) NOT NULL default '0',
  "AAAAFILH" int(11) NOT NULL default '0',
  "AAAAFILI" int(11) NOT NULL default '0',
  "AAAAFILJ" int(11) NOT NULL default '0',
  "AAAAFILK" int(11) NOT NULL default '0',
  "AAAAFILL" int(11) NOT NULL default '0',
  "AAAAFILM" varchar(1) NOT NULL default ' ',
  "AAAAFILN" varchar(3) NOT NULL default ' ',
  "AAAAFILO" int(11) NOT NULL default '0',
  "AAAAFILP" int(11) NOT NULL default '0',
  "AAAAFILQ" int(11) NOT NULL default '0',
  "AAAAFILR" int(11) NOT NULL default '0',
  "AAAAFILS" int(11) NOT NULL default '0',
  "AAAAFILT" int(11) NOT NULL default '0',
  "AAAAFILU" int(11) NOT NULL default '0',
  "AAAAFILV" int(11) NOT NULL default '0',
  "AAAAFILW" int(11) NOT NULL default '0',
  "AAAAFILX" int(11) NOT NULL default '0',
  "AAAAFILY" int(11) NOT NULL default '0',
  "AAAAFILZ" int(11) NOT NULL default '0',
  "AAAAFIMA" int(11) NOT NULL default '0',
  "AAAAFIMB" int(11) NOT NULL default '0',
  "AAAAFIMC" int(11) NOT NULL default '0',
  "AAAAFIMD" int(11) NOT NULL default '0',
  "AAAAFIME" int(11) NOT NULL default '0',
  "AAAAFIMF" int(11) NOT NULL default '0',
  "AAAAFIMG" int(11) NOT NULL default '0',
  "AAAAFIMH" int(11) NOT NULL default '0',
  "AAAAFIMI" int(11) NOT NULL default '0',
  "AAAAFIMJ" int(11) NOT NULL default '0',
  "AAAAFIMK" int(11) NOT NULL default '0',
  "AAAAFIML" int(11) NOT NULL default '0',
  "AAAAFIMM" int(11) NOT NULL default '0',
  "AAAAFIMN" int(11) NOT NULL default '0',
  "AAAAFIMO" int(11) NOT NULL default '0',
  "AAAAFIMP" int(11) NOT NULL default '0',
  "AAAAFIMQ" int(11) NOT NULL default '0',
  "AAAAFIMR" int(11) NOT NULL default '0',
  "AAAAFIMS" int(11) NOT NULL default '0',
  "AAAAFIMT" int(11) NOT NULL default '0',
  "AAAAFIMU" int(11) NOT NULL default '0',
  "AAAAFIMV" int(11) NOT NULL default '0',
  "AAAAFIMW" int(11) NOT NULL default '0',
  "AAAAFIMX" int(11) NOT NULL default '0',
  "AAAAFIMY" int(11) NOT NULL default '0',
  "AAAAFIMZ" int(11) NOT NULL default '0',
  "AAAAFINA" int(11) NOT NULL default '0',
  "AAAAFINB" int(11) NOT NULL default '0',
  "AAAAFINC" int(11) NOT NULL default '0',
  "AAAAFIND" int(11) NOT NULL default '0',
  "AAAAFINE" int(11) NOT NULL default '0',
  "AAAAFINF" int(11) NOT NULL default '0',
  "AAAAFING" varchar(6) NOT NULL default ' ',
  "AAAAFINH" varchar(6) NOT NULL default ' ',
  "AAAAFINI" varchar(6) NOT NULL default ' ',
  "AAAAFINJ" int(11) NOT NULL default '0',
  "AAAAFINK" int(11) NOT NULL default '0',
  "AAAAFIBI" smallint(6) NOT NULL default '0',
  "AAAAFIBJ" smallint(6) NOT NULL default '0',
  "AAAAFIBK" varchar(4) NOT NULL default ' ',
  "AAAAFIBL" int(11) NOT NULL default '0',
  "AAAAFIBM" int(11) NOT NULL default '0',
  "AAAAFIBN" int(11) NOT NULL default '0',
  "AAAAFIBO" int(11) NOT NULL default '0',
  "AAAAFIBP" int(11) NOT NULL default '0',
  "AAAAFIBQ" int(11) NOT NULL default '0',
  "AAAAFIBR" int(11) NOT NULL default '0',
  "AAAAFIBS" int(11) NOT NULL default '0',
  "AAAAFIBT" int(11) NOT NULL default '0',
  "AAAAFIBU" int(11) NOT NULL default '0',
  "AAAAFIBV" int(11) NOT NULL default '0',
  "AAAAFIBW" int(11) NOT NULL default '0',
  "AAAAFIBX" int(11) NOT NULL default '0',
  "AAAAFIBY" int(11) NOT NULL default '0',
  "AAAAFIBZ" int(11) NOT NULL default '0',
  "AAAAFICA" int(11) NOT NULL default '0',
  "AAAAFICB" int(11) NOT NULL default '0',
  "AAAAFICC" int(11) NOT NULL default '0',
  "AAAAFICD" int(11) NOT NULL default '0',
  "AAAAFICE" int(11) NOT NULL default '0',
  "AAAAFICF" int(11) NOT NULL default '0',
  "AAAAFICG" int(11) NOT NULL default '0',
  "AAAAFICH" int(11) NOT NULL default '0',
  "AAAAFICI" int(11) NOT NULL default '0',
  "AAAAFICJ" int(11) NOT NULL default '0',
  "AAAAFICK" int(11) NOT NULL default '0',
  "AAAAFICL" int(11) NOT NULL default '0',
  "AAAAFICM" int(11) NOT NULL default '0',
  "AAAAFICN" varchar(4) NOT NULL default ' ',
  "AAAAFICO" int(11) NOT NULL default '0',
  "AAAAFICP" int(11) NOT NULL default '0',
  "AAAAFICQ" int(11) NOT NULL default '0',
  "AAAAFICR" int(11) NOT NULL default '0',
  "AAAAFICS" int(11) NOT NULL default '0',
  "AAAAFICT" int(11) NOT NULL default '0',
  "AAAAFICU" int(11) NOT NULL default '0',
  "AAAAFICV" int(11) NOT NULL default '0',
  "AAAAFICW" int(11) NOT NULL default '0',
  "AAAAFICX" int(11) NOT NULL default '0',
  "AAAAFICY" int(11) NOT NULL default '0',
  "AAAAFICZ" int(11) NOT NULL default '0',
  "AAAAFIDA" int(11) NOT NULL default '0',
  "AAAAFIDB" int(11) NOT NULL default '0',
  "AAAAFIDC" int(11) NOT NULL default '0',
  "AAAAFIDD" int(11) NOT NULL default '0',
  "AAAAFIDE" int(11) NOT NULL default '0',
  "AAAAFIDF" int(11) NOT NULL default '0',
  "AAAAFIDG" int(11) NOT NULL default '0',
  "AAAAFIDH" int(11) NOT NULL default '0',
  "AAAAFIDI" int(11) NOT NULL default '0',
  "AAAAFIDJ" int(11) NOT NULL default '0',
  "AAAAFIDK" decimal(10,0) NOT NULL default '0',
  "AAAAFIDL" decimal(10,0) NOT NULL default '0',
  "AAAAFIDM" decimal(10,0) NOT NULL default '0',
  "AAAAFIDN" decimal(10,0) NOT NULL default '0',
  "AAAAFIDO" decimal(10,0) NOT NULL default '0',
  "AAAAFIDP" decimal(10,0) NOT NULL default '0',
  "AAAAFIDQ" int(11) NOT NULL default '0',
  "AAAAFINL" int(11) NOT NULL default '0',
  "AAAAFINM" int(11) NOT NULL default '0',
  "AAAAFINN" int(11) NOT NULL default '0',
  "AAAAFINO" int(11) NOT NULL default '0',
  "AAAAFINP" int(11) NOT NULL default '0',
  "AAAAFIQX" int(11) NOT NULL default '0',
  "AAAAFINQ" varchar(1) NOT NULL default ' ',
  "AAAAFINR" varchar(1) NOT NULL default ' ',
  "AAAAFINS" varchar(2) NOT NULL default ' ',
  "AAAAFINT" int(11) NOT NULL default '0',
  "AAAAFINU" int(11) NOT NULL default '0',
  "AAAAFINV" int(11) NOT NULL default '0',
  "AAAAFINW" int(11) NOT NULL default '0',
  "AAAAFINX" int(11) NOT NULL default '0',
  "AAAAFINY" int(11) NOT NULL default '0',
  "AAAAFINZ" int(11) NOT NULL default '0',
  "AAAAFIOA" int(11) NOT NULL default '0',
  "AAAAFIOB" int(11) NOT NULL default '0',
  "AAAAFIOC" int(11) NOT NULL default '0',
  "AAAAFIOD" int(11) NOT NULL default '0',
  "AAAAFIOE" int(11) NOT NULL default '0',
  "AAAAFIOF" int(11) NOT NULL default '0',
  "AAAAFIOG" int(11) NOT NULL default '0',
  "AAAAFIOH" int(11) NOT NULL default '0',
  "AAAAFIEP" int(11) NOT NULL default '0',
  "AAAAFIEQ" decimal(10,0) NOT NULL default '0',
  "AAAAFIER" decimal(10,0) NOT NULL default '0',
  "AAAAFIES" decimal(10,0) NOT NULL default '0',
  "AAAAFIET" decimal(10,0) NOT NULL default '0',
  "AAAAFIEU" decimal(10,0) NOT NULL default '0',
  "AAAAFIEV" decimal(10,0) NOT NULL default '0',
  "AAAAFIEW" decimal(10,0) NOT NULL default '0',
  "AAAAFIEX" decimal(10,0) NOT NULL default '0',
  "AAAAFIEY" decimal(10,0) NOT NULL default '0',
  "AAAAFIEZ" decimal(10,0) NOT NULL default '0',
  "AAAAFIFA" decimal(10,0) NOT NULL default '0',
  "AAAAFIFB" decimal(10,0) NOT NULL default '0',
  "AAAAFIFC" decimal(10,0) NOT NULL default '0',
  "AAAAFIFD" decimal(10,0) NOT NULL default '0',
  "AAAAFIFE" decimal(10,0) NOT NULL default '0',
  "AAAAFIFF" decimal(10,0) NOT NULL default '0',
  "AAAAFIFG" decimal(10,0) NOT NULL default '0',
  "AAAAFIFH" decimal(10,0) NOT NULL default '0',
  "AAAAFIFI" decimal(10,0) NOT NULL default '0',
  "AAAAFIFJ" int(11) NOT NULL default '0',
  "AAAAFIFK" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAAFJNY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFIFM" (
  "AAAAFJPB" varchar(4) NOT NULL default ' ',
  "AAAAFJPC" varchar(8) NOT NULL default '00000000',
  "AAAAFJPD" varchar(6) NOT NULL default '000000',
  "AAAAFJPE" varchar(6) NOT NULL default ' ',
  "AAAAFJPF" varchar(44) NOT NULL default ' ',
  "AAAAFJOI" varchar(8) NOT NULL default ' ',
  "AAAAFJOJ" varchar(8) NOT NULL default ' ',
  "AAAAFJOK" varchar(20) NOT NULL default ' ',
  "AAAAFJOL" smallint(6) NOT NULL default '0',
  "AAAAFJOM" smallint(6) NOT NULL default '0',
  "AAAAFJON" varchar(8) NOT NULL default ' ',
  "AAAAFJOO" varchar(8) NOT NULL default ' ',
  "AAAAFJOP" varchar(10) NOT NULL default '0000000000',
  "AAAAFJOQ" varchar(10) NOT NULL default '0000000000',
  "AAAAFJOR" int(11) NOT NULL default '0',
  "AAAAFJOS" int(11) NOT NULL default '0',
  "AAAAFJOT" varchar(10) NOT NULL default '0000000000',
  "AAAAFJOU" varchar(5) NOT NULL default '00000',
  "AAAAFJOV" varchar(5) NOT NULL default '00000',
  "AAAAFJOW" varchar(5) NOT NULL default '00000',
  "AAAAFJOX" varchar(5) NOT NULL default '00000',
  "AAAAFJOY" varchar(5) NOT NULL default '00000',
  "AAAAFJOZ" varchar(5) NOT NULL default '00000',
  "AAAAFJPA" varchar(5) NOT NULL default ' ',
  "AAAAFIGL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFJPB","AAAAFJPC","AAAAFJPD","AAAAFJPE","AAAAFJPF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFIGR" (
  "AAAAFJNY" varchar(8) NOT NULL default ' ',
  "AAABLEUZ" smallint(6) NOT NULL default '0',
  "AAAAFJOA" int(11) NOT NULL default '0',
  "AAAAFIGV" VARBINARY(255),
  "AAAAFIGW" int(11) NOT NULL default '0',
  "AAAAFIGX" int(11) NOT NULL default '0',
  "AAAAFIGY" varchar(30) NOT NULL default ' ',
  "AAAAFIGZ" VARBINARY(255),
  "AAAAFIHA" int(11) NOT NULL default '0',
  "AAAAFIHB" int(11) NOT NULL default '0',
  "AAAAFIHC" int(11) NOT NULL default '0',
  "AAAAFIHD" int(11) NOT NULL default '0',
  "AAAAFIHE" decimal(10,0) NOT NULL default '0',
  "AAAAFIHF" int(11) NOT NULL default '0',
  "AAAAFIHG" decimal(10,0) NOT NULL default '0',
  "AAAAFIHH" int(11) NOT NULL default '0',
  "AAAAFIHI" int(11) NOT NULL default '0',
  "AAAAFIHJ" int(11) NOT NULL default '0',
  "AAAAFIHK" int(11) NOT NULL default '0',
  "AAAAFIHL" int(11) NOT NULL default '0',
  "AAAAFIHM" decimal(16,0) NOT NULL default '0',
  "AAAAFIHN" smallint(6) NOT NULL default '0',
  "AAAAFIHO" varchar(60) NOT NULL default ' ',
  "AAAAFIHP" smallint(6) NOT NULL default '0',
  "AAAAFIHQ" varchar(128) NOT NULL default ' ',
  "AAAAFIHR" int(11) NOT NULL default '0',
  "AAAAFIHS" smallint(6) NOT NULL default '0',
  "AAAAFIHT" varchar(32) NOT NULL default ' ',
  "AAAAFIHU" smallint(6) NOT NULL default '0',
  "AAAAFIHV" varchar(128) NOT NULL default ' ',
  "AAAAFIHW" varchar(128) NOT NULL default ' ',
  "AAAAFIHX" varchar(128) NOT NULL default ' ',
  "AAAAFIHY" varchar(3) NOT NULL default ' ',
  "AAAAFIHZ" varchar(3) NOT NULL default ' ',
  "AAAAFIIA" varchar(4) NOT NULL default ' ',
  "AAAAFIIB" varchar(3) NOT NULL default ' ',
  "AAAAFIIC" varchar(3) NOT NULL default ' ',
  "AAAAFIID" varchar(5) NOT NULL default ' ',
  "AAAAFIIE" varchar(3) NOT NULL default ' ',
  "AAAAFIIF" decimal(16,0) NOT NULL default '0',
  "AAAAFIIG" decimal(16,0) NOT NULL default '0',
  "AAAAFIIH" decimal(16,0) NOT NULL default '0',
  "AAAAFIII" decimal(16,0) NOT NULL default '0',
  "AAAAFIIJ" decimal(16,0) NOT NULL default '0',
  "AAAAFIIK" decimal(16,0) NOT NULL default '0',
  "AAAAFIIL" decimal(16,0) NOT NULL default '0',
  "AAAAFIIM" decimal(16,0) NOT NULL default '0',
  "AAAAFIIN" int(11) NOT NULL default '0',
  "AAAAFIIO" int(11) NOT NULL default '0',
  "AAAAFIIP" varchar(128) NOT NULL default ' ',
  "AAAAFIIQ" varchar(128) NOT NULL default ' ',
  "AAAAFIIR" varchar(16) NOT NULL default ' ',
  "AAAAFIIS" int(11) NOT NULL default '0',
  "AAAAFJNV" smallint(6) NOT NULL default '0',
  "AAAAFJNW" text,
  PRIMARY KEY ("AAAAFJNY","AAABLEUZ","AAAAFJOA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFIIY" (
  "AAAAFJNY" varchar(8) NOT NULL default ' ',
  "AAABLEUZ" smallint(6) NOT NULL default '0',
  "AAAAFJRO" int(11) NOT NULL default '0',
  "AAAAFIJC" int(11) NOT NULL default '0',
  "AAAAFIJD" int(11) NOT NULL default '0',
  "AAAAFIJE" int(11) NOT NULL default '0',
  "AAAAFIJF" int(11) NOT NULL default '0',
  "AAAAFIJG" int(11) NOT NULL default '0',
  "AAAAFIJH" int(11) NOT NULL default '0',
  "AAAAFIJI" int(11) NOT NULL default '0',
  "AAAAFIJJ" int(11) NOT NULL default '0',
  "AAAAFIJK" int(11) NOT NULL default '0',
  "AAAAFIJL" int(11) NOT NULL default '0',
  "AAAAFIJM" int(11) NOT NULL default '0',
  "AAAAFIJN" int(11) NOT NULL default '0',
  "AAAAFIJO" int(11) NOT NULL default '0',
  "AAAAFIJP" int(11) NOT NULL default '0',
  "AAAAFIJQ" int(11) NOT NULL default '0',
  "AAAAFIJR" int(11) NOT NULL default '0',
  "AAAAFIJS" int(11) NOT NULL default '0',
  "AAAAFIJT" int(11) NOT NULL default '0',
  "AAAAFIJU" int(11) NOT NULL default '0',
  "AAAAFIJV" int(11) NOT NULL default '0',
  "AAAAFIJW" int(11) NOT NULL default '0',
  "AAAAFIJX" int(11) NOT NULL default '0',
  "AAAAFIJY" int(11) NOT NULL default '0',
  "AAAAFIJZ" int(11) NOT NULL default '0',
  "AAAAFIKA" decimal(10,0) NOT NULL default '0',
  "AAAAFIKB" int(11) NOT NULL default '0',
  "AAAAFIKC" int(11) NOT NULL default '0',
  "AAAAFIKD" int(11) NOT NULL default '0',
  "AAAAFIKE" int(11) NOT NULL default '0',
  "AAAAFIKF" int(11) NOT NULL default '0',
  "AAAAFIKG" int(11) NOT NULL default '0',
  "AAAAFIKH" int(11) NOT NULL default '0',
  "AAAAFIKI" int(11) NOT NULL default '0',
  "AAAAFIKJ" int(11) NOT NULL default '0',
  "AAAAFIKK" int(11) NOT NULL default '0',
  "AAAAFIKL" int(11) NOT NULL default '0',
  "AAAAFIKM" int(11) NOT NULL default '0',
  "AAAAFIKN" int(11) NOT NULL default '0',
  "AAAAFIKO" int(11) NOT NULL default '0',
  "AAAAFIKP" int(11) NOT NULL default '0',
  "AAAAFIKQ" int(11) NOT NULL default '0',
  "AAAAFIKR" int(11) NOT NULL default '0',
  "AAAAFIKS" int(11) NOT NULL default '0',
  "AAAAFIKT" int(11) NOT NULL default '0',
  "AAAAFIKU" int(11) NOT NULL default '0',
  "AAAAFIKV" int(11) NOT NULL default '0',
  "AAAAFIKW" int(11) NOT NULL default '0',
  "AAAAFIKX" int(11) NOT NULL default '0',
  "AAAAFIKY" int(11) NOT NULL default '0',
  "AAAAFIKZ" int(11) NOT NULL default '0',
  "AAAAFILA" int(11) NOT NULL default '0',
  "AAAAFILB" int(11) NOT NULL default '0',
  "AAAAFILC" int(11) NOT NULL default '0',
  "AAAAFILD" int(11) NOT NULL default '0',
  "AAAAFILE" int(11) NOT NULL default '0',
  "AAAAFILF" int(11) NOT NULL default '0',
  "AAAAFILG" int(11) NOT NULL default '0',
  "AAAAFILH" int(11) NOT NULL default '0',
  "AAAAFILI" int(11) NOT NULL default '0',
  "AAAAFILJ" int(11) NOT NULL default '0',
  "AAAAFILK" int(11) NOT NULL default '0',
  "AAAAFILL" int(11) NOT NULL default '0',
  "AAAAFILM" varchar(1) NOT NULL default ' ',
  "AAAAFILN" varchar(3) NOT NULL default ' ',
  "AAAAFILO" int(11) NOT NULL default '0',
  "AAAAFILP" int(11) NOT NULL default '0',
  "AAAAFILQ" int(11) NOT NULL default '0',
  "AAAAFILR" int(11) NOT NULL default '0',
  "AAAAFILS" int(11) NOT NULL default '0',
  "AAAAFILT" int(11) NOT NULL default '0',
  "AAAAFILU" int(11) NOT NULL default '0',
  "AAAAFILV" int(11) NOT NULL default '0',
  "AAAAFILW" int(11) NOT NULL default '0',
  "AAAAFILX" int(11) NOT NULL default '0',
  "AAAAFILY" int(11) NOT NULL default '0',
  "AAAAFILZ" int(11) NOT NULL default '0',
  "AAAAFIMA" int(11) NOT NULL default '0',
  "AAAAFIMB" int(11) NOT NULL default '0',
  "AAAAFIMC" int(11) NOT NULL default '0',
  "AAAAFIMD" int(11) NOT NULL default '0',
  "AAAAFIME" int(11) NOT NULL default '0',
  "AAAAFIMF" int(11) NOT NULL default '0',
  "AAAAFIMG" int(11) NOT NULL default '0',
  "AAAAFIMH" int(11) NOT NULL default '0',
  "AAAAFIMI" int(11) NOT NULL default '0',
  "AAAAFIMJ" int(11) NOT NULL default '0',
  "AAAAFIMK" int(11) NOT NULL default '0',
  "AAAAFIML" int(11) NOT NULL default '0',
  "AAAAFIMM" int(11) NOT NULL default '0',
  "AAAAFIMN" int(11) NOT NULL default '0',
  "AAAAFIMO" int(11) NOT NULL default '0',
  "AAAAFIMP" int(11) NOT NULL default '0',
  "AAAAFIMQ" int(11) NOT NULL default '0',
  "AAAAFIMR" int(11) NOT NULL default '0',
  "AAAAFIMS" int(11) NOT NULL default '0',
  "AAAAFIMT" int(11) NOT NULL default '0',
  "AAAAFIMU" int(11) NOT NULL default '0',
  "AAAAFIMV" int(11) NOT NULL default '0',
  "AAAAFIMW" int(11) NOT NULL default '0',
  "AAAAFIMX" int(11) NOT NULL default '0',
  "AAAAFIMY" int(11) NOT NULL default '0',
  "AAAAFIMZ" int(11) NOT NULL default '0',
  "AAAAFINA" int(11) NOT NULL default '0',
  "AAAAFINB" int(11) NOT NULL default '0',
  "AAAAFINC" int(11) NOT NULL default '0',
  "AAAAFIND" int(11) NOT NULL default '0',
  "AAAAFINE" int(11) NOT NULL default '0',
  "AAAAFINF" int(11) NOT NULL default '0',
  "AAAAFING" varchar(6) NOT NULL default ' ',
  "AAAAFINH" varchar(6) NOT NULL default ' ',
  "AAAAFINI" varchar(6) NOT NULL default ' ',
  "AAAAFINJ" int(11) NOT NULL default '0',
  "AAAAFINK" int(11) NOT NULL default '0',
  "AAAAFINL" int(11) NOT NULL default '0',
  "AAAAFINM" int(11) NOT NULL default '0',
  "AAAAFINN" int(11) NOT NULL default '0',
  "AAAAFINO" int(11) NOT NULL default '0',
  "AAAAFINP" int(11) NOT NULL default '0',
  "AAAAFINQ" varchar(1) NOT NULL default ' ',
  "AAAAFINR" varchar(1) NOT NULL default ' ',
  "AAAAFINS" varchar(2) NOT NULL default ' ',
  "AAAAFINT" int(11) NOT NULL default '0',
  "AAAAFINU" int(11) NOT NULL default '0',
  "AAAAFINV" int(11) NOT NULL default '0',
  "AAAAFINW" int(11) NOT NULL default '0',
  "AAAAFINX" int(11) NOT NULL default '0',
  "AAAAFINY" int(11) NOT NULL default '0',
  "AAAAFINZ" int(11) NOT NULL default '0',
  "AAAAFIOA" int(11) NOT NULL default '0',
  "AAAAFIOB" int(11) NOT NULL default '0',
  "AAAAFIOC" int(11) NOT NULL default '0',
  "AAAAFIOD" int(11) NOT NULL default '0',
  "AAAAFIOE" int(11) NOT NULL default '0',
  "AAAAFIOF" int(11) NOT NULL default '0',
  "AAAAFIOG" int(11) NOT NULL default '0',
  "AAAAFIOH" int(11) NOT NULL default '0',
  "AAAAFIOI" int(11) NOT NULL default '0',
  "AAAAFIOJ" int(11) NOT NULL default '0',
  "AAAAFIOK" int(11) NOT NULL default '0',
  "AAAAFIOL" int(11) NOT NULL default '0',
  "AAAAFIOM" int(11) NOT NULL default '0',
  "AAAAFION" int(11) NOT NULL default '0',
  "AAAAFIOO" int(11) NOT NULL default '0',
  "AAAAFIOP" int(11) NOT NULL default '0',
  "AAAAFIOQ" int(11) NOT NULL default '0',
  "AAAAFIOR" int(11) NOT NULL default '0',
  "AAAAFIOS" int(11) NOT NULL default '0',
  "AAAAFIOT" varchar(10) NOT NULL default ' ',
  "AAAAFIOU" varchar(10) NOT NULL default ' ',
  "AAAAFIOV" varchar(10) NOT NULL default ' ',
  "AAAAFIOW" varchar(22) NOT NULL default ' ',
  "AAAAFIOX" varchar(10) NOT NULL default ' ',
  "AAAAFIOY" smallint(6) NOT NULL default '0',
  "AAAAFIOZ" int(11) NOT NULL default '0',
  "AAAAFIPA" int(11) NOT NULL default '0',
  "AAAAFIPB" smallint(6) NOT NULL default '0',
  "AAAAFIPC" text,
  PRIMARY KEY ("AAAAFJNY","AAABLEUZ","AAAAFJRO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFIPG" (
  "AAABLEUZ" smallint(6) NOT NULL default '0',
  "AAAAFJRO" int(11) NOT NULL default '0',
  "AAABIGIV" smallint(6) NOT NULL default '0',
  "AAAAFIPZ" varchar(1) NOT NULL default ' ',
  "AAAAFIQA" varchar(36) NOT NULL default ' ',
  "AAAAUYTB" varchar(8) NOT NULL default ' ',
  "AAAAFJQD" varchar(6) NOT NULL default ' ',
  "AAAATQRH" varchar(20) NOT NULL default ' ',
  "AAAAFIPP" smallint(6) NOT NULL default '0',
  "AAAAFIPQ" smallint(6) NOT NULL default '0',
  "AAABLEQR" varchar(12) NOT NULL default ' ',
  "AAAAHZWC" varchar(10) NOT NULL default ' ',
  "AAAAFIPT" varchar(6) NOT NULL default ' ',
  "AAAAFIPU" varchar(22) NOT NULL default ' ',
  "AAAAFIPV" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEUZ","AAAAFJRO","AAABIGIV","AAAAFIPZ","AAAAFIQA","AAAAUYTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFIQC" (
  "AAAAFJNY" varchar(8) NOT NULL default ' ',
  "AAAAFJRO" int(11) NOT NULL default '0',
  "AAABJTDV" varchar(8) NOT NULL default ' ',
  "AAAAFJPK" varchar(4) NOT NULL default ' ',
  "AAAAFJPL" varchar(8) NOT NULL default '00000000',
  "AAAAFJPM" varchar(6) NOT NULL default '000000',
  "AAAAFJPN" varchar(6) NOT NULL default '000000',
  "AAABLTJH" varchar(8) NOT NULL default ' ',
  "AAAAFJPP" varchar(128) NOT NULL default ' ',
  "AAAAFJPQ" varchar(128) NOT NULL default ' ',
  "AAAAFJPR" varchar(16) NOT NULL default ' ',
  "AAAAFJPS" smallint(6) NOT NULL default '0',
  "AAAAFJPT" varchar(16) NOT NULL default ' ',
  "AAABHDEA" varchar(8) NOT NULL default ' ',
  "AAAAFJPV" int(11) NOT NULL default '0',
  "AAAAFJPW" int(11) NOT NULL default '0',
  "AAAAQPUE" int(11) NOT NULL default '0',
  "AAAAFJPY" int(11) NOT NULL default '0',
  "AAAAFJPZ" int(11) NOT NULL default '0',
  "AAAAFJQA" int(11) NOT NULL default '0',
  "AAAAFIQX" int(11) NOT NULL default '0',
  "AAABBTMB" int(11) NOT NULL default '0',
  "AAAAFJQC" int(11) NOT NULL default '0',
  "AAAAFJQD" varchar(6) NOT NULL default ' ',
  "AAABLOCY" varchar(16) NOT NULL default ' ',
  "AAAAFJQF" varchar(8) NOT NULL default ' ',
  "AAAAFJQG" varchar(8) NOT NULL default ' ',
  "AAAAFJQH" varchar(5) NOT NULL default ' ',
  "AAABCRRX" varchar(10) NOT NULL default '0000000000',
  "AAABHCRQ" varchar(10) NOT NULL default '0000000000',
  "AAAAFJQK" varchar(32) NOT NULL default ' ',
  "AAAAFJQL" int(11) NOT NULL default '0',
  "AAAAFJQM" varchar(18) NOT NULL default ' ',
  "AAAAFJQN" varchar(32) NOT NULL default ' ',
  "AAAAFJQO" int(11) NOT NULL default '0',
  "AAAAFJQP" int(11) NOT NULL default '0',
  "AAAAFIRN" varchar(200) NOT NULL default ' ',
  "AAAAFIRO" varchar(18) NOT NULL default ' ',
  "AAAAFIRP" varchar(128) NOT NULL default ' ',
  "AAAAFIXU" varchar(128) NOT NULL default ' ',
  "AAAAFIXS" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFJNY","AAAAFJRO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFIRU" (
  "AAAAFJNY" varchar(8) NOT NULL default ' ',
  "AAAAFJRO" int(11) NOT NULL default '0',
  "AAAAFIRX" decimal(16,0) NOT NULL default '0',
  "AAAAFIRY" decimal(16,0) NOT NULL default '0',
  "AAAAFIRZ" decimal(16,0) NOT NULL default '0',
  "AAAAFISA" decimal(16,0) NOT NULL default '0',
  "AAAAFISB" decimal(16,0) NOT NULL default '0',
  "AAAAFISC" decimal(16,0) NOT NULL default '0',
  "AAAAFISD" decimal(16,0) NOT NULL default '0',
  "AAAAFISE" decimal(16,0) NOT NULL default '0',
  "AAAAFISF" decimal(16,0) NOT NULL default '0',
  "AAAAFISG" decimal(16,0) NOT NULL default '0',
  "AAAAFISH" decimal(16,0) NOT NULL default '0',
  "AAAAFISI" decimal(16,0) NOT NULL default '0',
  "AAAAFISJ" decimal(16,0) NOT NULL default '0',
  "AAAAFISK" decimal(16,0) NOT NULL default '0',
  "AAAAFISL" decimal(16,0) NOT NULL default '0',
  "AAAAFISM" decimal(16,0) NOT NULL default '0',
  "AAAAFISN" decimal(16,0) NOT NULL default '0',
  "AAAAFISO" decimal(16,0) NOT NULL default '0',
  "AAAAFISP" int(11) NOT NULL default '0',
  "AAAAFISQ" int(11) NOT NULL default '0',
  "AAAAFISR" int(11) NOT NULL default '0',
  "AAAAFISS" int(11) NOT NULL default '0',
  "AAAAFIST" int(11) NOT NULL default '0',
  "AAAAFISU" int(11) NOT NULL default '0',
  "AAAAFISV" int(11) NOT NULL default '0',
  "AAAAFISW" int(11) NOT NULL default '0',
  "AAAAFISX" int(11) NOT NULL default '0',
  "AAAAFISY" int(11) NOT NULL default '0',
  "AAAAFISZ" int(11) NOT NULL default '0',
  "AAAAFITA" int(11) NOT NULL default '0',
  "AAAAFITB" decimal(16,0) NOT NULL default '0',
  "AAAAFITC" decimal(16,0) NOT NULL default '0',
  "AAAAFITD" decimal(16,0) NOT NULL default '0',
  "AAAAFITE" decimal(16,0) NOT NULL default '0',
  "AAAAFITF" decimal(16,0) NOT NULL default '0',
  "AAAAFITG" decimal(16,0) NOT NULL default '0',
  "AAAAFITH" int(11) NOT NULL default '0',
  "AAAAFITI" int(11) NOT NULL default '0',
  "AAAAFITJ" int(11) NOT NULL default '0',
  "AAAAFITK" int(11) NOT NULL default '0',
  "AAAAFITL" int(11) NOT NULL default '0',
  "AAAAFITM" int(11) NOT NULL default '0',
  "AAAAFITN" int(11) NOT NULL default '0',
  "AAAAFITO" decimal(16,0) NOT NULL default '0',
  "AAAAFITP" decimal(16,0) NOT NULL default '0',
  "AAAAFITQ" varchar(10) NOT NULL default ' ',
  "AAAAFITR" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAAFJNY","AAAAFJRO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFITU" (
  "AAAAFIVJ" varchar(4) NOT NULL default ' ',
  "AAAAFIVK" varchar(8) NOT NULL default ' ',
  "AAAAFIVL" varchar(8) NOT NULL default '00000000',
  "AAAAFIVM" varchar(6) NOT NULL default '000000',
  "AAAAFIVN" varchar(6) NOT NULL default ' ',
  "AAAAFIUA" varchar(8) NOT NULL default ' ',
  "AAAAFIUB" int(11) NOT NULL default '0',
  "AAAAFIUC" int(11) NOT NULL default '0',
  "AAAAFIUD" int(11) NOT NULL default '0',
  "AAAAFIUE" int(11) NOT NULL default '0',
  "AAAAFIUF" int(11) NOT NULL default '0',
  "AAAAFIUG" int(11) NOT NULL default '0',
  "AAAAFIUH" int(11) NOT NULL default '0',
  "AAAAFIUI" int(11) NOT NULL default '0',
  "AAAAFIUJ" int(11) NOT NULL default '0',
  "AAAAFIUK" int(11) NOT NULL default '0',
  "AAAAFIUL" int(11) NOT NULL default '0',
  "AAAAFIUM" int(11) NOT NULL default '0',
  "AAAAFIUN" int(11) NOT NULL default '0',
  "AAAAFIUO" int(11) NOT NULL default '0',
  "AAAAFIUP" int(11) NOT NULL default '0',
  "AAAAFIUQ" int(11) NOT NULL default '0',
  "AAAAFIUR" int(11) NOT NULL default '0',
  "AAAAFIUS" int(11) NOT NULL default '0',
  "AAAAFIUT" int(11) NOT NULL default '0',
  "AAAAFIUU" int(11) NOT NULL default '0',
  "AAAAFIUV" int(11) NOT NULL default '0',
  "AAAAFIUW" int(11) NOT NULL default '0',
  "AAAAFIUX" int(11) NOT NULL default '0',
  "AAAAFIUY" int(11) NOT NULL default '0',
  "AAAAFIUZ" int(11) NOT NULL default '0',
  "AAAAFIVA" int(11) NOT NULL default '0',
  "AAAAFIVB" int(11) NOT NULL default '0',
  "AAAAFIVC" int(11) NOT NULL default '0',
  "AAAAFIVD" int(11) NOT NULL default '0',
  "AAAAFIVE" int(11) NOT NULL default '0',
  "AAAAFIVF" int(11) NOT NULL default '0',
  "AAAAFIVG" int(11) NOT NULL default '0',
  "AAAAFIVH" int(11) NOT NULL default '0',
  "AAAAFIVI" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAAFIVJ","AAAAFIVK","AAAAFIVL","AAAAFIVM","AAAAFIVN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFIVO" (
  "AAAAFIWY" varchar(32) NOT NULL default ' ',
  "AAABLEUZ" smallint(6) NOT NULL default '0',
  "AAAAFIXA" varchar(8) NOT NULL default '00000000',
  "AAAAFIXB" varchar(6) NOT NULL default '000000',
  "AAAAFIXC" varchar(6) NOT NULL default ' ',
  "AAAAFIXD" varchar(8) NOT NULL default ' ',
  "AAAAFIXE" varchar(8) NOT NULL default ' ',
  "AAAAFIXF" smallint(6) NOT NULL default '0',
  "AAAAFIXG" varchar(8) NOT NULL default ' ',
  "AAAAFIVY" varchar(5) NOT NULL default ' ',
  "AAAAFIVZ" varchar(3) NOT NULL default ' ',
  "AAAAFIWA" decimal(8,2) NOT NULL default '0.00',
  "AAAAFIWB" decimal(8,2) NOT NULL default '0.00',
  "AAAAFIWC" decimal(8,2) NOT NULL default '0.00',
  "AAAAFIWD" decimal(4,2) NOT NULL default '0.00',
  "AAAAFIWE" decimal(4,2) NOT NULL default '0.00',
  "AAAAFIWF" decimal(4,2) NOT NULL default '0.00',
  "AAAAFIWG" varchar(8) NOT NULL default ' ',
  "AAAAFIWH" varchar(8) NOT NULL default ' ',
  "AAAAFIWI" smallint(6) NOT NULL default '0',
  "AAAAFIWJ" smallint(6) NOT NULL default '0',
  "AAAAFIWK" int(11) NOT NULL default '0',
  "AAAAFIWL" varchar(1) NOT NULL default ' ',
  "AAAAFIWM" int(11) NOT NULL default '0',
  "AAAAFIWN" int(11) NOT NULL default '0',
  "AAAAFIWO" int(11) NOT NULL default '0',
  "AAAAFIWP" int(11) NOT NULL default '0',
  "AAAAFIWQ" int(11) NOT NULL default '0',
  "AAAAFIWR" int(11) NOT NULL default '0',
  "AAAAFIWS" int(11) NOT NULL default '0',
  "AAAAFIWT" int(11) NOT NULL default '0',
  "AAAAFIWU" int(11) NOT NULL default '0',
  "AAAAFIWV" int(11) NOT NULL default '0',
  "AAAAFIWW" decimal(8,2) NOT NULL default '0.00',
  "AAAAFIZI" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAAAFIWY","AAABLEUZ","AAAAFIXA","AAAAFIXB","AAAAFIXC","AAAAFIXD","AAAAFIXE","AAAAFIXF","AAAAFIXG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFIXH" (
  "AAAAFJNY" varchar(8) NOT NULL default ' ',
  "AAAAFIXZ" varchar(12) NOT NULL default ' ',
  "AAAAFIYA" varchar(8) NOT NULL default ' ',
  "AAAAFIXL" smallint(6) NOT NULL default '0',
  "AAAAFIXM" varchar(3) NOT NULL default '000',
  "AAAAFIXN" varchar(1) NOT NULL default ' ',
  "AAAAFIXO" varchar(8) NOT NULL default ' ',
  "AAAAFIXP" varchar(8) NOT NULL default ' ',
  "AAAAFIXQ" varchar(8) NOT NULL default ' ',
  "AAAAFIXR" decimal(10,0) NOT NULL default '0',
  "AAAAFIXS" varchar(22) NOT NULL default ' ',
  "AAAAFIXT" varchar(2) NOT NULL default ' ',
  "AAAAFIXU" varchar(128) NOT NULL default ' ',
  "AAAAFIXV" varchar(32) NOT NULL default ' ',
  "AAAAFIXW" varchar(18) NOT NULL default ' ',
  "AAAAFIXX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFJNY","AAAAFIXZ","AAAAFIYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFIYB" (
  "AAAAFJNY" varchar(8) NOT NULL default ' ',
  "AAAAFIYD" int(11) NOT NULL default '0',
  "AAAAFIYE" int(11) NOT NULL default '0',
  "AAAAFIYF" int(11) NOT NULL default '0',
  "AAAAFIYG" int(11) NOT NULL default '0',
  "AAAAFIYH" int(11) NOT NULL default '0',
  "AAAAFIYI" int(11) NOT NULL default '0',
  "AAAAFIYJ" int(11) NOT NULL default '0',
  "AAAAFIYK" int(11) NOT NULL default '0',
  "AAAAFIYL" int(11) NOT NULL default '0',
  "AAAAFIYM" int(11) NOT NULL default '0',
  "AAAAFIYN" decimal(10,0) NOT NULL default '0',
  "AAAAFIYO" decimal(10,0) NOT NULL default '0',
  "AAAAFIYP" decimal(10,0) NOT NULL default '0',
  "AAAAFIYQ" decimal(10,0) NOT NULL default '0',
  "AAAAFIYR" decimal(10,0) NOT NULL default '0',
  "AAAAFIYS" decimal(10,0) NOT NULL default '0',
  "AAAAFIYT" decimal(10,0) NOT NULL default '0',
  "AAAAFIYU" decimal(10,0) NOT NULL default '0',
  "AAAAFIYV" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAAFJNY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFIYX" (
  "AAAAFJNY" varchar(8) NOT NULL default ' ',
  "AAAAFIYZ" varchar(4) NOT NULL default ' ',
  "AAAAFIZA" varchar(3) NOT NULL default ' ',
  "AAAAFIZB" smallint(6) NOT NULL default '0',
  "AAAAFIZC" int(11) NOT NULL default '0',
  "AAAAFIZD" smallint(6) NOT NULL default '0',
  "AAAAFIZE" smallint(6) NOT NULL default '0',
  "AAAAFIZF" smallint(6) NOT NULL default '0',
  "AAAAFIZG" smallint(6) NOT NULL default '0',
  "AAAAFIZH" smallint(6) NOT NULL default '0',
  "AAAAFIZI" smallint(6) NOT NULL default '0',
  "AAAAFIZJ" int(11) NOT NULL default '0',
  "AAAAFIZK" int(11) NOT NULL default '0',
  "AAAAFIZL" int(11) NOT NULL default '0',
  "AAAAFIZM" smallint(6) NOT NULL default '0',
  "AAAAFIZN" varchar(8) NOT NULL default ' ',
  "AAAAFIZO" varchar(3) NOT NULL default ' ',
  "AAAAFIZP" varchar(3) NOT NULL default ' ',
  "AAAAFIZQ" varchar(3) NOT NULL default ' ',
  "AAAAFIZR" varchar(8) NOT NULL default ' ',
  "AAAAFIZS" varchar(128) NOT NULL default ' ',
  "AAAAFIZT" varchar(4) NOT NULL default ' ',
  "AAAAFIZU" VARBINARY(255),
  "AAAAFIZV" int(11) NOT NULL default '0',
  "AAAAFIZW" varchar(3) NOT NULL default ' ',
  "AAAAFIZX" int(11) NOT NULL default '0',
  "AAAAFIZY" smallint(6) NOT NULL default '0',
  "AAAAFIZZ" smallint(6) NOT NULL default '0',
  "AAAAFJAA" int(11) NOT NULL default '0',
  "AAAAFJAB" varchar(10) NOT NULL default ' ',
  "AAAAFJAC" smallint(6) NOT NULL default '0',
  "AAAAFJAD" smallint(6) NOT NULL default '0',
  "AAAAFJAE" varchar(4) NOT NULL default ' ',
  "AAAAFJAF" smallint(6) NOT NULL default '0',
  "AAAAFJAG" smallint(6) NOT NULL default '0',
  "AAAAFJAH" varchar(8) NOT NULL default ' ',
  "AAAAFJAI" varchar(8) NOT NULL default ' ',
  "AAAAFJAJ" varchar(18) NOT NULL default ' ',
  "AAAAFJAK" varchar(4) NOT NULL default ' ',
  "AAAAFJAL" smallint(6) NOT NULL default '0',
  "AAAAFJAM" smallint(6) NOT NULL default '0',
  "AAAAFJAN" smallint(6) NOT NULL default '0',
  "AAAAFJAO" int(11) NOT NULL default '0',
  "AAAAFJAP" int(11) NOT NULL default '0',
  "AAAAFJAQ" int(11) NOT NULL default '0',
  "AAAAFJAR" varchar(3) NOT NULL default ' ',
  "AAAAFJAS" smallint(6) NOT NULL default '0',
  "AAAAFJAT" varchar(8) NOT NULL default ' ',
  "AAAAFJAU" int(11) NOT NULL default '0',
  "AAAAFJAV" int(11) NOT NULL default '0',
  "AAAAFJAW" int(11) NOT NULL default '0',
  "AAAAFJAX" int(11) NOT NULL default '0',
  "AAAAFJAY" int(11) NOT NULL default '0',
  "AAAAFJAZ" smallint(6) NOT NULL default '0',
  "AAAAFJBA" smallint(6) NOT NULL default '0',
  "AAAAFJBB" varchar(10) NOT NULL default ' ',
  "AAAAFJBC" varchar(10) NOT NULL default ' ',
  "AAAAFJBD" varchar(10) NOT NULL default ' ',
  "AAAAFJBE" varchar(3) NOT NULL default ' ',
  "AAAAFJBF" varchar(10) NOT NULL default ' ',
  "AAAAFJBG" varchar(3) NOT NULL default ' ',
  "AAAAFJBH" smallint(6) NOT NULL default '0',
  "AAAAFJBI" smallint(6) NOT NULL default '0',
  "AAAAFJBJ" varchar(4) NOT NULL default ' ',
  "AAAAFJBK" varchar(8) NOT NULL default ' ',
  "AAAAFJBL" smallint(6) NOT NULL default '0',
  "AAAAFJBM" smallint(6) NOT NULL default '0',
  "AAAAFJBN" int(11) NOT NULL default '0',
  "AAAAFJBO" varchar(3) NOT NULL default ' ',
  "AAAAFJBP" varchar(2) NOT NULL default ' ',
  "AAAAFJBQ" varchar(1) NOT NULL default ' ',
  "AAAAFJBR" varchar(1) NOT NULL default ' ',
  "AAAAFJBS" varchar(1) NOT NULL default ' ',
  "AAAAFJBT" varchar(8) NOT NULL default ' ',
  "AAAAFJBU" varchar(128) NOT NULL default ' ',
  "AAAAFJBV" varchar(3) NOT NULL default ' ',
  "AAAAFJBW" varchar(1) NOT NULL default ' ',
  "AAAAFJBX" varchar(9) NOT NULL default ' ',
  "AAAAFJBY" varchar(2) NOT NULL default ' ',
  "AAAAFJBZ" int(11) NOT NULL default '0',
  "AAAAFJCA" int(11) NOT NULL default '0',
  "AAAAFJCB" int(11) NOT NULL default '0',
  "AAAAFJCC" varchar(128) NOT NULL default ' ',
  "AAAAFJCD" varchar(8) NOT NULL default ' ',
  "AAAAFJCE" varchar(4) NOT NULL default ' ',
  "AAAAFJCF" int(11) NOT NULL default '0',
  "AAAAFJCG" int(11) NOT NULL default '0',
  "AAAAFJCH" int(11) NOT NULL default '0',
  "AAAAFJCI" varchar(4) NOT NULL default ' ',
  "AAAAFJCJ" varchar(4) NOT NULL default ' ',
  "AAAAFJCK" smallint(6) NOT NULL default '0',
  "AAAAFJCL" int(11) NOT NULL default '0',
  "AAAAFJCM" int(11) NOT NULL default '0',
  "AAAAFJCN" smallint(6) NOT NULL default '0',
  "AAAAFJCO" smallint(6) NOT NULL default '0',
  "AAAAFJCP" smallint(6) NOT NULL default '0',
  "AAAAFJCQ" smallint(6) NOT NULL default '0',
  "AAAAFJCR" smallint(6) NOT NULL default '0',
  "AAAAFJCS" smallint(6) NOT NULL default '0',
  "AAAAFJCT" smallint(6) NOT NULL default '0',
  "AAAAFJCU" smallint(6) NOT NULL default '0',
  "AAAAFJCV" smallint(6) NOT NULL default '0',
  "AAAAFJCW" varchar(2) NOT NULL default ' ',
  "AAAAFJCX" int(11) NOT NULL default '0',
  "AAAAFJCY" int(11) NOT NULL default '0',
  "AAAAFJCZ" smallint(6) NOT NULL default '0',
  "AAAAFJDA" smallint(6) NOT NULL default '0',
  "AAAAFJDB" smallint(6) NOT NULL default '0',
  "AAAAFJDC" smallint(6) NOT NULL default '0',
  "AAAAFJDD" smallint(6) NOT NULL default '0',
  "AAAAFJDE" smallint(6) NOT NULL default '0',
  "AAAAFJDF" varchar(2) NOT NULL default ' ',
  "AAAAFJDG" varchar(4) NOT NULL default ' ',
  "AAAAFJDH" smallint(6) NOT NULL default '0',
  "AAAAFJDI" int(11) NOT NULL default '0',
  "AAAAFJDJ" int(11) NOT NULL default '0',
  "AAAAFJDK" varchar(128) NOT NULL default ' ',
  "AAAAFJDL" varchar(128) NOT NULL default ' ',
  "AAAAFJDM" varchar(128) NOT NULL default ' ',
  "AAAAFJDN" smallint(6) NOT NULL default '0',
  "AAAAFJDO" smallint(6) NOT NULL default '0',
  "AAAAFJDP" smallint(6) NOT NULL default '0',
  "AAAAFJDQ" smallint(6) NOT NULL default '0',
  "AAAAFJDR" smallint(6) NOT NULL default '0',
  "AAAAFJDS" varchar(3) NOT NULL default '000',
  "AAAAFJDT" varchar(1) NOT NULL default ' ',
  "AAAAFJDU" smallint(6) NOT NULL default '0',
  "AAAAFJDV" smallint(6) NOT NULL default '0',
  "AAAAFJDW" smallint(6) NOT NULL default '0',
  "AAAAFJDX" smallint(6) NOT NULL default '0',
  "AAAAFJDY" smallint(6) NOT NULL default '0',
  "AAAAFJDZ" smallint(6) NOT NULL default '0',
  "AAAAFJEA" smallint(6) NOT NULL default '0',
  "AAAAFJEB" smallint(6) NOT NULL default '0',
  "AAAAFJEC" smallint(6) NOT NULL default '0',
  "AAAAFJED" smallint(6) NOT NULL default '0',
  "AAAAFJEE" smallint(6) NOT NULL default '0',
  "AAAAFJEF" smallint(6) NOT NULL default '0',
  "AAAAFJEG" smallint(6) NOT NULL default '0',
  "AAAAFJEH" smallint(6) NOT NULL default '0',
  "AAAAFJEI" int(11) NOT NULL default '0',
  "AAAAFJEJ" int(11) NOT NULL default '0',
  "AAAAFJEK" int(11) NOT NULL default '0',
  "AAAAFJEL" varchar(10) NOT NULL default '0000000000',
  "AAAAFJEM" smallint(6) NOT NULL default '0',
  "AAAAFJEN" smallint(6) NOT NULL default '0',
  "AAAAFJEO" smallint(6) NOT NULL default '0',
  "AAAAFJEP" smallint(6) NOT NULL default '0',
  "AAAAFJEQ" smallint(6) NOT NULL default '0',
  "AAAAFJER" smallint(6) NOT NULL default '0',
  "AAAAFJES" int(11) NOT NULL default '0',
  "AAAAFJET" int(11) NOT NULL default '0',
  "AAAAFJEU" int(11) NOT NULL default '0',
  "AAAAFJEV" int(11) NOT NULL default '0',
  "AAAAFJEW" int(11) NOT NULL default '0',
  "AAAAFJEX" smallint(6) NOT NULL default '0',
  "AAAAFJEY" smallint(6) NOT NULL default '0',
  "AAAAFJEZ" smallint(6) NOT NULL default '0',
  "AAAAFJFA" smallint(6) NOT NULL default '0',
  "AAAAFJFB" smallint(6) NOT NULL default '0',
  "AAAAFJFC" int(11) NOT NULL default '0',
  "AAAAFJFD" int(11) NOT NULL default '0',
  "AAAAFJFE" smallint(6) NOT NULL default '0',
  "AAAAFJFF" smallint(6) NOT NULL default '0',
  "AAAAFJFG" smallint(6) NOT NULL default '0',
  "AAAAFJFH" int(11) NOT NULL default '0',
  "AAAAFJFI" varchar(10) NOT NULL default ' ',
  "AAAAFJFJ" int(11) NOT NULL default '0',
  "AAAAFJFK" int(11) NOT NULL default '0',
  "AAAAFJFL" int(11) NOT NULL default '0',
  "AAAAFJFM" smallint(6) NOT NULL default '0',
  "AAAAFJFN" varchar(1) NOT NULL default ' ',
  "AAAAFJFO" varchar(128) NOT NULL default ' ',
  "AAAAFJFP" varchar(128) NOT NULL default ' ',
  "AAAAFJLE" varchar(17) NOT NULL default ' ',
  "AAAAFJFR" varchar(10) NOT NULL default ' ',
  "AAAAFJFS" varchar(9) NOT NULL default ' ',
  "AAAAFJFT" varchar(8) NOT NULL default ' ',
  "AAAAFJFU" int(11) NOT NULL default '0',
  "AAAAFJFV" int(11) NOT NULL default '0',
  "AAAAFJFW" varchar(3) NOT NULL default ' ',
  "AAAAFJFX" varchar(1) NOT NULL default '0',
  "AAAAFJFY" smallint(6) NOT NULL default '0',
  "AAAAFJFZ" smallint(6) NOT NULL default '0',
  "AAAAFJGA" smallint(6) NOT NULL default '0',
  "AAAAFJGB" int(11) NOT NULL default '0',
  "AAAAFJGC" varchar(4) NOT NULL default ' ',
  "AAAAFJGD" varchar(3) NOT NULL default ' ',
  "AAAAFJGE" varchar(1) NOT NULL default ' ',
  "AAAAFJGF" varchar(3) NOT NULL default ' ',
  "AAAAFJGG" int(11) NOT NULL default '0',
  "AAAAFJGH" int(11) NOT NULL default '0',
  "AAAAFJGI" smallint(6) NOT NULL default '0',
  "AAAAFJGJ" smallint(6) NOT NULL default '0',
  "AAAAFJGK" int(11) NOT NULL default '0',
  "AAAAFJGL" varchar(2) NOT NULL default ' ',
  "AAAAFJGM" varchar(5) NOT NULL default ' ',
  "AAAAFJGN" varchar(1) NOT NULL default ' ',
  "AAAAFJGO" smallint(6) NOT NULL default '0',
  "AAAAFJGP" smallint(6) NOT NULL default '0',
  "AAAAFJGQ" varchar(8) NOT NULL default ' ',
  "AAAAFJGR" varchar(3) NOT NULL default ' ',
  "AAAAFJGS" varchar(6) NOT NULL default ' ',
  "AAAAFJGT" smallint(6) NOT NULL default '0',
  "AAAAFJGU" int(11) NOT NULL default '0',
  "AAAAFJGV" int(11) NOT NULL default '0',
  "AAAAFJGW" int(11) NOT NULL default '0',
  "AAAAFJGX" smallint(6) NOT NULL default '0',
  "AAAAFJGY" varchar(3) NOT NULL default ' ',
  "AAAAFJGZ" smallint(6) NOT NULL default '0',
  "AAAAFJHA" int(11) NOT NULL default '0',
  "AAAAFJHB" varchar(3) NOT NULL default ' ',
  "AAAAFJHC" varchar(3) NOT NULL default ' ',
  "AAAAFJHD" varchar(3) NOT NULL default ' ',
  "AAAAFJHE" varchar(3) NOT NULL default ' ',
  "AAAAFJHF" varchar(3) NOT NULL default ' ',
  "AAAAFJHG" varchar(3) NOT NULL default ' ',
  "AAAAFJHH" varchar(3) NOT NULL default ' ',
  "AAAAFJHI" varchar(3) NOT NULL default ' ',
  "AAAAFJHJ" varchar(3) NOT NULL default ' ',
  "AAAAFJHK" varchar(3) NOT NULL default ' ',
  "AAAAFJHL" varchar(3) NOT NULL default ' ',
  "AAAAFJHM" varchar(3) NOT NULL default ' ',
  "AAAAFJHN" varchar(3) NOT NULL default ' ',
  "AAAAFJHO" varchar(3) NOT NULL default ' ',
  "AAAAFJHP" varchar(3) NOT NULL default ' ',
  "AAAAFJHQ" varchar(10) NOT NULL default ' ',
  "AAAAFJHR" smallint(6) NOT NULL default '0',
  "AAAAFJHS" varchar(8) NOT NULL default ' ',
  "AAAAFJHT" int(11) NOT NULL default '0',
  "AAAAFJHU" varchar(8) NOT NULL default ' ',
  "AAAAFJHV" varchar(8) NOT NULL default ' ',
  "AAAAFJHW" int(11) NOT NULL default '0',
  "AAAAFJHX" int(11) NOT NULL default '0',
  "AAAAFJHY" varchar(20) NOT NULL default ' ',
  "AAAAFJHZ" varchar(8) NOT NULL default ' ',
  "AAAAFJIA" int(11) NOT NULL default '0',
  "AAAAFJIB" int(11) NOT NULL default '0',
  "AAAAFJIC" varchar(8) NOT NULL default ' ',
  "AAAAFJID" varchar(8) NOT NULL default ' ',
  "AAAAFJIE" int(11) NOT NULL default '0',
  "AAAAFJIF" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAAFJNY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFJIH" (
  "AAAAFJNY" varchar(8) NOT NULL default ' ',
  "AAAAFJIJ" varchar(3) NOT NULL default ' ',
  "AAAAFJIK" smallint(6) NOT NULL default '0',
  "AAAAFJIL" smallint(6) NOT NULL default '0',
  "AAAAFJIM" int(11) NOT NULL default '0',
  "AAAAFJIN" int(11) NOT NULL default '0',
  "AAAAFJIO" varchar(8) NOT NULL default ' ',
  "AAAAFJIP" int(11) NOT NULL default '0',
  "AAAAFJIQ" int(11) NOT NULL default '0',
  "AAAAFJIR" int(11) NOT NULL default '0',
  "AAAAFJIS" varchar(10) NOT NULL default ' ',
  "AAAAFJIT" varchar(10) NOT NULL default ' ',
  "AAAAFJIU" varchar(6) NOT NULL default ' ',
  "AAAAFJIV" varchar(8) NOT NULL default ' ',
  "AAAAFJIW" varchar(8) NOT NULL default ' ',
  "AAAAFJIX" int(11) NOT NULL default '0',
  "AAAAFJIY" int(11) NOT NULL default '0',
  "AAAAFJIZ" varchar(8) NOT NULL default ' ',
  "AAAAFJJA" int(11) NOT NULL default '0',
  "AAAAFJJB" int(11) NOT NULL default '0',
  "AAAAFJJC" int(11) NOT NULL default '0',
  "AAAAFJJD" int(11) NOT NULL default '0',
  "AAAAFJJE" int(11) NOT NULL default '0',
  "AAAAFJJF" varchar(10) NOT NULL default ' ',
  "AAAAFJJG" varchar(3) NOT NULL default ' ',
  "AAAAFJJH" varchar(8) NOT NULL default ' ',
  "AAAAFJJI" varchar(8) NOT NULL default ' ',
  "AAAAFJJJ" varchar(8) NOT NULL default ' ',
  "AAAAFJJK" varchar(8) NOT NULL default ' ',
  "AAAAFJJL" varchar(3) NOT NULL default ' ',
  "AAAAFJJM" varchar(10) NOT NULL default ' ',
  "AAAAFJJN" varchar(10) NOT NULL default ' ',
  "AAAAFJJO" varchar(10) NOT NULL default ' ',
  "AAAAFJJP" varchar(10) NOT NULL default ' ',
  "AAAAFJJQ" smallint(6) NOT NULL default '0',
  "AAAAFJJR" smallint(6) NOT NULL default '0',
  "AAAAFJJS" varchar(3) NOT NULL default ' ',
  "AAAAFJJT" int(11) NOT NULL default '0',
  "AAAAFJJU" varchar(10) NOT NULL default ' ',
  "AAAAFJJV" varchar(50) NOT NULL default ' ',
  "AAAAFJJW" int(11) NOT NULL default '0',
  "AAAAFJJX" int(11) NOT NULL default '0',
  "AAAAFJJY" int(11) NOT NULL default '0',
  "AAAAFJJZ" varchar(30) NOT NULL default ' ',
  "AAAAFJKA" int(11) NOT NULL default '0',
  "AAAAFJKB" int(11) NOT NULL default '0',
  "AAAAFJKC" int(11) NOT NULL default '0',
  "AAAAFJKD" varchar(10) NOT NULL default ' ',
  "AAAAFJKE" varchar(20) NOT NULL default ' ',
  "AAAAFJKF" varchar(10) NOT NULL default ' ',
  "AAAAFJKG" varchar(20) NOT NULL default ' ',
  "AAAAFJKH" varchar(3) NOT NULL default ' ',
  "AAAAFJKI" varchar(10) NOT NULL default ' ',
  "AAAAFJKJ" varchar(10) NOT NULL default ' ',
  "AAAAFJKK" varchar(10) NOT NULL default ' ',
  "AAAAFJKL" varchar(10) NOT NULL default ' ',
  "AAAAFJKM" smallint(6) NOT NULL default '0',
  "AAAAFJKN" varchar(3) NOT NULL default ' ',
  "AAAAFJKO" smallint(6) NOT NULL default '0',
  "AAAAFJKP" varchar(10) NOT NULL default ' ',
  "AAAAFJKQ" varchar(3) NOT NULL default ' ',
  "AAAAFJKR" varchar(3) NOT NULL default ' ',
  "AAAAFJKS" varchar(3) NOT NULL default ' ',
  "AAAAFJKT" varchar(8) NOT NULL default ' ',
  "AAAAFJKU" varchar(10) NOT NULL default ' ',
  "AAAAFJKV" varchar(10) NOT NULL default ' ',
  "AAAAFJKW" smallint(6) NOT NULL default '0',
  "AAAAFJKX" varchar(14) NOT NULL default ' ',
  "AAAAFJKY" smallint(6) NOT NULL default '0',
  "AAAAFJKZ" varchar(10) NOT NULL default ' ',
  "AAAAFJLA" int(11) NOT NULL default '0',
  "AAAAFJLB" varchar(10) NOT NULL default ' ',
  "AAAAFJLC" int(11) NOT NULL default '0',
  "AAAAFJLD" varchar(3) NOT NULL default ' ',
  "AAAAFJLE" varchar(128) NOT NULL default ' ',
  "AAAAFJLF" int(11) NOT NULL default '0',
  "AAAAFJLG" int(11) NOT NULL default '0',
  "AAAAFJLH" int(11) NOT NULL default '0',
  "AAAAFJLI" int(11) NOT NULL default '0',
  "AAAAFJLJ" int(11) NOT NULL default '0',
  "AAAAFJLK" varchar(10) NOT NULL default ' ',
  "AAAAFJLL" int(11) NOT NULL default '0',
  "AAAAFJLM" int(11) NOT NULL default '0',
  "AAAAFJLN" int(11) NOT NULL default '0',
  "AAAAFJLO" int(11) NOT NULL default '0',
  "AAAAFJLP" varchar(53) NOT NULL default ' ',
  "AAAAFJLQ" varchar(10) NOT NULL default ' ',
  "AAAAFJLR" int(11) NOT NULL default '0',
  "AAAAFJLS" int(11) NOT NULL default '0',
  "AAAAFJLT" varchar(10) NOT NULL default ' ',
  "AAAAFJLU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFJNY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFJLW" (
  "AAAAFJMC" varchar(8) NOT NULL default ' ',
  "AAAAKFCZ" varchar(18) NOT NULL default ' ',
  "AAAAFJME" int(11) NOT NULL default '0',
  "AAAAFJMJ" varchar(1) NOT NULL default ' ',
  "AAAAFJMK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFJMC","AAAAKFCZ","AAAAFJME")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFJMF" (
  "AAABFVMJ" varchar(8) NOT NULL default ' ',
  "AAAAVQFD" varchar(8) NOT NULL default ' ',
  "AAAAFJMN" int(11) NOT NULL default '0',
  "AAAAFJMJ" varchar(1) NOT NULL default ' ',
  "AAAAFJMK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFVMJ","AAAAVQFD","AAAAFJMN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFJMO" (
  "AAABMODI" varchar(4) NOT NULL default ' ',
  "AAABKYPM" varchar(12) NOT NULL default ' ',
  "AAAARECB" varchar(22) NOT NULL default ' ',
  "AAAAFJMS" varchar(4) NOT NULL default ' ',
  "AAABGBXY" varchar(4) NOT NULL default ' ',
  "AAABGBYA" varchar(2) NOT NULL default ' ',
  "AAAAFJMV" varchar(4) NOT NULL default ' ',
  "AAAAFJMW" varchar(132) NOT NULL default ' ',
  "AAABGBXT" varchar(132) NOT NULL default ' ',
  "AAAAFJMY" varchar(132) NOT NULL default ' ',
  "AAAAFJMZ" varchar(3) NOT NULL default ' ',
  "AAAAFJNA" varchar(5) NOT NULL default '00000',
  "AAAAFJNB" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMODI","AAABKYPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFJNE" (
  "AAAAROLJ" varchar(8) NOT NULL default ' ',
  "AAAAJWXK" varchar(8) NOT NULL default ' ',
  "AAABJBGU" varchar(4) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABLUSV" varchar(2) NOT NULL default '00',
  "AAABHWXE" varchar(3) NOT NULL default ' ',
  "AAAAFJNL" varchar(8) NOT NULL default ' ',
  "AAABMMRH" varchar(254) NOT NULL default ' ',
  "AAAAFJNN" varchar(10) NOT NULL default ' ',
  "AAAAFJNO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAROLJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFJNQ" (
  "AAAAFJNY" varchar(8) NOT NULL default ' ',
  "AAABLEUZ" smallint(6) NOT NULL default '0',
  "AAAAFJOA" int(11) NOT NULL default '0',
  "AAAAFJOB" varchar(60) NOT NULL default ' ',
  "AAAAFJNV" int(11) NOT NULL default '0',
  "AAAAFJNW" longtext,
  "AAAAFJNX" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFJNY","AAABLEUZ","AAAAFJOA","AAAAFJOB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFJOC" (
  "AAAAFJPB" varchar(4) NOT NULL default ' ',
  "AAAAFJPC" varchar(8) NOT NULL default '00000000',
  "AAAAFJPD" varchar(6) NOT NULL default '000000',
  "AAAAFJPE" varchar(6) NOT NULL default ' ',
  "AAAAFJPF" varchar(44) NOT NULL default ' ',
  "AAAAFJOI" varchar(8) NOT NULL default ' ',
  "AAAAFJOJ" varchar(8) NOT NULL default ' ',
  "AAAAFJOK" varchar(20) NOT NULL default ' ',
  "AAAAFJOL" smallint(6) NOT NULL default '0',
  "AAAAFJOM" smallint(6) NOT NULL default '0',
  "AAAAFJON" varchar(8) NOT NULL default ' ',
  "AAAAFJOO" varchar(8) NOT NULL default ' ',
  "AAAAFJOP" varchar(10) NOT NULL default '0000000000',
  "AAAAFJOQ" varchar(10) NOT NULL default '0000000000',
  "AAAAFJOR" int(11) NOT NULL default '0',
  "AAAAFJOS" int(11) NOT NULL default '0',
  "AAAAFJOT" varchar(10) NOT NULL default '0000000000',
  "AAAAFJOU" varchar(5) NOT NULL default '00000',
  "AAAAFJOV" varchar(5) NOT NULL default '00000',
  "AAAAFJOW" varchar(5) NOT NULL default '00000',
  "AAAAFJOX" varchar(5) NOT NULL default '00000',
  "AAAAFJOY" varchar(5) NOT NULL default '00000',
  "AAAAFJOZ" varchar(5) NOT NULL default '00000',
  "AAAAFJPA" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFJPB","AAAAFJPC","AAAAFJPD","AAAAFJPE","AAAAFJPF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFJPG" (
  "AAAAFJRN" varchar(8) NOT NULL default ' ',
  "AAAAFJRO" int(11) NOT NULL default '0',
  "AAABJTDV" varchar(8) NOT NULL default ' ',
  "AAAAFJPK" varchar(4) NOT NULL default ' ',
  "AAAAFJPL" varchar(8) NOT NULL default '00000000',
  "AAAAFJPM" varchar(6) NOT NULL default '000000',
  "AAAAFJPN" varchar(6) NOT NULL default '000000',
  "AAABLTJH" varchar(8) NOT NULL default ' ',
  "AAAAFJPP" varchar(128) NOT NULL default ' ',
  "AAAAFJPQ" varchar(128) NOT NULL default ' ',
  "AAAAFJPR" varchar(16) NOT NULL default ' ',
  "AAAAFJPS" smallint(6) NOT NULL default '0',
  "AAAAFJPT" varchar(16) NOT NULL default ' ',
  "AAABHDEA" varchar(8) NOT NULL default ' ',
  "AAAAFJPV" int(11) NOT NULL default '0',
  "AAAAFJPW" int(11) NOT NULL default '0',
  "AAAAQPUE" int(11) NOT NULL default '0',
  "AAAAFJPY" int(11) NOT NULL default '0',
  "AAAAFJPZ" int(11) NOT NULL default '0',
  "AAAAFJQA" int(11) NOT NULL default '0',
  "AAABBTMB" int(11) NOT NULL default '0',
  "AAAAFJQC" int(11) NOT NULL default '0',
  "AAAAFJQD" varchar(6) NOT NULL default ' ',
  "AAABLOCY" varchar(16) NOT NULL default ' ',
  "AAAAFJQF" varchar(8) NOT NULL default ' ',
  "AAAAFJQG" varchar(8) NOT NULL default ' ',
  "AAAAFJQH" varchar(5) NOT NULL default ' ',
  "AAABCRRX" varchar(10) NOT NULL default '0000000000',
  "AAABHCRQ" varchar(10) NOT NULL default '0000000000',
  "AAAAFJQK" varchar(32) NOT NULL default ' ',
  "AAAAFJQL" int(11) NOT NULL default '0',
  "AAAAFJQM" varchar(18) NOT NULL default ' ',
  "AAAAFJQN" varchar(32) NOT NULL default ' ',
  "AAAAFJQO" int(11) NOT NULL default '0',
  "AAAAFJQP" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAAFJRN","AAAAFJRO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFJQS" (
  "AAAAFJRN" varchar(8) NOT NULL default ' ',
  "AAAAFJRO" int(11) NOT NULL default '0',
  "AAAAFJRP" varchar(8) NOT NULL default ' ',
  "AAAAFJQW" int(11) NOT NULL default '0',
  "AAAAFJQX" int(11) NOT NULL default '0',
  "AAAAFJQY" int(11) NOT NULL default '0',
  "AAAAFJQZ" int(11) NOT NULL default '0',
  "AAAAFJRA" int(11) NOT NULL default '0',
  "AAAAFJRB" int(11) NOT NULL default '0',
  "AAAAFJRC" int(11) NOT NULL default '0',
  "AAAAFJRD" int(11) NOT NULL default '0',
  "AAAAFJRE" int(11) NOT NULL default '0',
  "AAAAFJRF" int(11) NOT NULL default '0',
  "AAAAFJRG" int(11) NOT NULL default '0',
  "AAAAFJRH" int(11) NOT NULL default '0',
  "AAAAFJRI" int(11) NOT NULL default '0',
  "AAAAFJRJ" int(11) NOT NULL default '0',
  "AAAAFJRK" int(11) NOT NULL default '0',
  "AAAAFJRL" varchar(10) NOT NULL default ' ',
  "AAAAFJRM" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAAFJRN","AAAAFJRO","AAAAFJRP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--enable_query_log
