# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+
# |                             tbl30.inc                              |
# |                        Copyright 2006 by MySQL AB                  |
# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+

--disable_query_log
CREATE TABLE "AAABAGTL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAGTX" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAGTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGTQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAGTX" varchar(25) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAGTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGTY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAGUK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAGUK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGUD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAGUK" varchar(25) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAGUK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGUL" (
  "AAABAGUV" varchar(4) NOT NULL default '0000',
  "AAABAGUN" varchar(60) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABAGUV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGUR" (
  "AAABAGUV" varchar(4) NOT NULL default '0000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAGUU" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABAGUV","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGUX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAGVB" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAGVB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGVC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABALZE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABALZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGVH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDFU" varchar(3) NOT NULL default ' ',
  "AAABAIIS" varchar(30) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABAGVM" varchar(1) NOT NULL default ' ',
  "AAABAGVN" varchar(1) NOT NULL default ' ',
  "AAABAGVO" varchar(30) NOT NULL default ' ',
  "AAABAGVP" varchar(30) NOT NULL default ' ',
  "AAABAGVQ" varchar(30) NOT NULL default ' ',
  "AAABAGVR" varchar(30) NOT NULL default ' ',
  "AAABAGVS" varchar(30) NOT NULL default ' ',
  "AAABAGVT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGVW" (
  "AAABHWBH" varchar(15) NOT NULL default ' ',
  "AAABAMII" varchar(32) NOT NULL default ' ',
  "AAABANNN" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABHWBH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGWB" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHWBH" varchar(15) NOT NULL default ' ',
  "AAABHVRH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHWBH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGWH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABALZE" varchar(4) NOT NULL default ' ',
  "AAABILTS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABALZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGWP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJCDT" varchar(8) NOT NULL default ' ',
  "AAABAMRI" varchar(67) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABJCDT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGWX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCDT" varchar(8) NOT NULL default ' ',
  "AAABAGXQ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABJCDT","AAABAGXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGXE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCDT" varchar(8) NOT NULL default ' ',
  "AAABAGXQ" varchar(3) NOT NULL default '000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJDFT" varchar(1) NOT NULL default ' ',
  "AAABJDFU" varchar(3) NOT NULL default ' ',
  "AAABJDFW" varchar(2) NOT NULL default ' ',
  "AAABJDGB" varchar(2) NOT NULL default ' ',
  "AAABAGXN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJCDT","AAABAGXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGXR" (
  "AAABAGYB" varchar(8) NOT NULL default ' ',
  "AAABAGYC" varchar(2) NOT NULL default '00',
  "AAABAGYA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABAGYB","AAABAGYC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGXX" (
  "AAABAGYB" varchar(8) NOT NULL default ' ',
  "AAABAGYC" varchar(2) NOT NULL default '00',
  "AAABAGYA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABAGYB","AAABAGYC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGYD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAHAQ" varchar(4) NOT NULL default '0000',
  "AAABAGYM" varchar(2) NOT NULL default ' ',
  "AAABAGYN" varchar(2) NOT NULL default ' ',
  "AAABAGZI" varchar(8) NOT NULL default ' ',
  "AAABAGZJ" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAHAQ","AAABAGYM","AAABAGYN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGYO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAHAQ" varchar(4) NOT NULL default '0000',
  "AAABAHAR" varchar(2) NOT NULL default '00',
  "AAABAGYT" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAHAQ","AAABAHAR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGYY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAMZC" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAMZC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGZD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAMZC" varchar(8) NOT NULL default '00000000',
  "AAABAGZO" varchar(2) NOT NULL default ' ',
  "AAABAGZP" varchar(2) NOT NULL default ' ',
  "AAABAGZI" varchar(8) NOT NULL default ' ',
  "AAABAGZJ" varchar(8) NOT NULL default ' ',
  "AAABAGZK" varchar(20) NOT NULL default ' ',
  "AAABAGZL" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAMZC","AAABAGZO","AAABAGZP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGZQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAHAQ" varchar(4) NOT NULL default '0000',
  "AAABAHAR" varchar(2) NOT NULL default '00',
  "AAABAGZU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAHAQ","AAABAHAR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAGZY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAHAQ" varchar(4) NOT NULL default '0000',
  "AAABAHAR" varchar(2) NOT NULL default '00',
  "AAABAHAS" varchar(2) NOT NULL default '00',
  "AAABAHAE" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAHAQ","AAABAHAR","AAABAHAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHAK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAHAQ" varchar(4) NOT NULL default '0000',
  "AAABAHAR" varchar(2) NOT NULL default '00',
  "AAABAHAS" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABAHAQ","AAABAHAR","AAABAHAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHAT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMOZ" varchar(10) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABAHAX" varchar(2) NOT NULL default '00',
  "AAABAHAY" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABLMOZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHBB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLMOZ" varchar(10) NOT NULL default ' ',
  "AAABLPSU" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLMOZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHBJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABAHCE" varchar(2) NOT NULL default '00',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHPM","AAABAHCE","AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHBS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAHCE" varchar(2) NOT NULL default '00',
  "AAABAHBW" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAHCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHCA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAHCE" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABAHCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHCF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAHCT" varchar(4) NOT NULL default '0000',
  "AAABAHCI" decimal(3,0) NOT NULL default '0',
  "AAABAMJK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAHCT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHCM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAHCT" varchar(4) NOT NULL default '0000',
  "AAABAHCQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAHCT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHCU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABAHCX" varchar(4) NOT NULL default ' ',
  "AAABAHCY" varchar(4) NOT NULL default ' ',
  "AAABAHCZ" varchar(1) NOT NULL default ' ',
  "AAABAMFH" varchar(1) NOT NULL default ' ',
  "AAABAHDB" varchar(1) NOT NULL default ' ',
  "AAABAHDC" varchar(1) NOT NULL default ' ',
  "AAABAHDD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHDG" (
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABAHDI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABJFGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHDK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABAHDQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHJ","AAABLHPM","AAABJFGL","AAABJFHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHDW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDEQ" varchar(8) NOT NULL default ' ',
  "AAABHMRI" varchar(10) NOT NULL default ' ',
  "AAABAHVJ" varchar(2) NOT NULL default '00',
  "AAABHWBH" varchar(15) NOT NULL default ' ',
  "AAABHWBI" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDEQ","AAABHMRI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHEG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBDEQ" varchar(8) NOT NULL default ' ',
  "AAABHMRI" varchar(10) NOT NULL default ' ',
  "AAABFQPC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBDEQ","AAABHMRI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDEQ" varchar(8) NOT NULL default ' ',
  "AAABHMRI" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDEQ","AAABHMRI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHEX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABILZK" varchar(40) NOT NULL default ' ',
  "AAABLUHV" varchar(4) NOT NULL default ' ',
  "AAABAHFG" varchar(20) NOT NULL default ' ',
  "AAABAHFC" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABILZK","AAABLUHV","AAABAHFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHFH" (
  "AAABHVTW" varchar(12) NOT NULL default ' ',
  "AAABHVRO" varchar(1) NOT NULL default ' ',
  "AAABLFTX" varchar(4) NOT NULL default '0000',
  "AAABHVRP" varchar(1) NOT NULL default '0',
  "AAABHVRQ" varchar(30) NOT NULL default ' ',
  "AAABAHFN" varchar(1) NOT NULL default ' ',
  "AAABHVRR" varchar(5) NOT NULL default ' ',
  "AAABAMIE" varchar(6) NOT NULL default '000000',
  "AAABBKLY" varchar(6) NOT NULL default '000000',
  "AAABHVRS" varchar(30) NOT NULL default ' ',
  "AAABAHFS" varchar(30) NOT NULL default ' ',
  "AAABHVRU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHVTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHFV" (
  "AAABHVTW" varchar(12) NOT NULL default ' ',
  "AAABMREF" varchar(8) NOT NULL default '00000000',
  "AAABAHHO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABHVTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHGA" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHWBI" varchar(15) NOT NULL default ' ',
  "AAABHVTW" varchar(12) NOT NULL default ' ',
  "AAABHVTT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHWBI","AAABHVTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHGI" (
  "AAABHWBI" varchar(15) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABHWBI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHGM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHWBI" varchar(15) NOT NULL default ' ',
  "AAABHVTW" varchar(12) NOT NULL default ' ',
  "AAABHVTT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHWBI","AAABHVTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHGW" (
  "AAABHVTW" varchar(12) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABHVTW","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHHB" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHVTW" varchar(12) NOT NULL default ' ',
  "AAABHVTT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHVTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHHH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAHHX" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABAHHX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHHM" (
  "AAABAMII" varchar(32) NOT NULL default ' ',
  "AAABAHHO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABAMII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHHQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAHHX" varchar(4) NOT NULL default '0000',
  "AAABAHHU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAHHX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHHY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJJH" varchar(2) NOT NULL default '00',
  "AAABAHIB" decimal(5,2) NOT NULL default '0.00',
  "AAABAHIC" decimal(5,2) NOT NULL default '0.00',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHIG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDEQ" varchar(8) NOT NULL default ' ',
  "AAABHMRI" varchar(10) NOT NULL default ' ',
  "AAABAHYX" varchar(30) NOT NULL default ' ',
  "AAABHWBH" varchar(15) NOT NULL default ' ',
  "AAABAHYG" varchar(1) NOT NULL default ' ',
  "AAABHWBI" varchar(15) NOT NULL default ' ',
  "AAABAHYI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDEQ","AAABHMRI","AAABAHYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHIT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBDEQ" varchar(8) NOT NULL default ' ',
  "AAABHMRI" varchar(10) NOT NULL default ' ',
  "AAABAHYX" varchar(30) NOT NULL default ' ',
  "AAABAHYS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBDEQ","AAABHMRI","AAABAHYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHJF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDEQ" varchar(8) NOT NULL default ' ',
  "AAABHMRI" varchar(10) NOT NULL default ' ',
  "AAABAHYX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDEQ","AAABHMRI","AAABAHYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHJO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDEQ" varchar(8) NOT NULL default ' ',
  "AAABHMRI" varchar(10) NOT NULL default ' ',
  "AAABAHYX" varchar(30) NOT NULL default ' ',
  "AAABAHXT" varchar(30) NOT NULL default ' ',
  "AAABAHXU" varchar(2) NOT NULL default '00',
  "AAABAHXV" varchar(1) NOT NULL default ' ',
  "AAABAHXW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDEQ","AAABHMRI","AAABAHYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHKB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDSB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDSB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBDSB" varchar(10) NOT NULL default ' ',
  "AAABAHKK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBDSB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLDON" varchar(10) NOT NULL default ' ',
  "AAABAHKS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLDON")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLDON" varchar(10) NOT NULL default ' ',
  "AAABAHLF" varchar(2) NOT NULL default ' ',
  "AAABJCDT" varchar(8) NOT NULL default ' ',
  "AAABAHLH" varchar(2) NOT NULL default ' ',
  "AAABLQEI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLDON","AAABAHLF","AAABJCDT","AAABAHLH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHLI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDSB" varchar(10) NOT NULL default ' ',
  "AAABBDRP" varchar(4) NOT NULL default ' ',
  "AAABGCHJ" varchar(2) NOT NULL default '00',
  "AAABGCHK" varchar(4) NOT NULL default ' ',
  "AAABGCHL" varchar(1) NOT NULL default ' ',
  "AAABAHLP" varchar(1) NOT NULL default ' ',
  "AAABAHLQ" varchar(6) NOT NULL default '000000',
  "AAABAHLR" varchar(6) NOT NULL default '000000',
  "AAABAHLS" varchar(6) NOT NULL default '000000',
  "AAABAHLT" varchar(10) NOT NULL default ' ',
  "AAABAHLU" varchar(10) NOT NULL default ' ',
  "AAABAHLV" varchar(10) NOT NULL default ' ',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABGCHE" varchar(4) NOT NULL default ' ',
  "AAABMQCD" varchar(1) NOT NULL default ' ',
  "AAABGCHF" varchar(2) NOT NULL default '00',
  "AAABGCHI" varchar(2) NOT NULL default ' ',
  "AAABAHMB" varchar(4) NOT NULL default ' ',
  "AAABAHMC" varchar(4) NOT NULL default ' ',
  "AAABAHMD" varchar(4) NOT NULL default ' ',
  "AAABAHME" varchar(4) NOT NULL default ' ',
  "AAABLTQJ" varchar(1) NOT NULL default '0',
  "AAABGCIV" varchar(1) NOT NULL default ' ',
  "AAABGCIW" varchar(1) NOT NULL default ' ',
  "AAABAMXC" varchar(1) NOT NULL default ' ',
  "AAABAMXD" varchar(1) NOT NULL default ' ',
  "AAABAMXE" varchar(1) NOT NULL default ' ',
  "AAABLQVJ" varchar(4) NOT NULL default '0000',
  "AAABAHMM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDSB","AAABBDRP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHMQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAHNI" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAHNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHMV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAHNI" varchar(5) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAHNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHNB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAHNI" varchar(5) NOT NULL default ' ',
  "AAABMRCK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAHNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHNJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBIIQ" varchar(10) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABAHNN" varchar(1) NOT NULL default ' ',
  "AAABGLYJ" varchar(1) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  "AAABAHNQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBIIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHNT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBIIQ" varchar(10) NOT NULL default ' ',
  "AAABAHNX" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBIIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDSR" varchar(10) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABGCHZ" varchar(2) NOT NULL default '00',
  "AAABGCIA" varchar(4) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABAHOJ" varchar(1) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  "AAABAHOL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHOO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBDSR" varchar(10) NOT NULL default ' ',
  "AAABAHOS" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBDSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHOW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAHPE" varchar(10) NOT NULL default ' ',
  "AAABBDRE" varchar(10) NOT NULL default ' ',
  "AAABEAAI" varchar(1) NOT NULL default ' ',
  "AAABAHPB" varchar(1) NOT NULL default ' ',
  "AAABAHPC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAHPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHPF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABAHPI" varchar(30) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABAHPL" varchar(1) NOT NULL default ' ',
  "AAABLRCB" varchar(1) NOT NULL default ' ',
  "AAABAHPN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHPM"),
  KEY "AAABAHPQ" ("AAABMNYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHPS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAHTM" varchar(10) NOT NULL default ' ',
  "AAABAHPV" varchar(10) NOT NULL default ' ',
  "AAABAHPW" varchar(1) NOT NULL default ' ',
  "AAABAHPX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAHTM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHQA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLDON" varchar(10) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLDON")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHQG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDSL" varchar(10) NOT NULL default ' ',
  "AAABGCHF" varchar(2) NOT NULL default '00',
  "AAABLGKR" varchar(2) NOT NULL default ' ',
  "AAABAHQL" varchar(1) NOT NULL default ' ',
  "AAABAHQM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDSL","AAABGCHF","AAABLGKR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHQR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBDSB" varchar(10) NOT NULL default ' ',
  "AAABBDRP" varchar(4) NOT NULL default ' ',
  "AAABAHQW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBDSB","AAABBDRP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHRB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJDFT" varchar(1) NOT NULL default ' ',
  "AAABJDFU" varchar(3) NOT NULL default ' ',
  "AAABJDGB" varchar(2) NOT NULL default ' ',
  "AAABDTRC" varchar(40) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABJSLF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHPM","AAABJDFT","AAABJDFU","AAABJDGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHRP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAHTB" varchar(10) NOT NULL default ' ',
  "AAABLQVJ" varchar(2) NOT NULL default '00',
  "AAABAHRT" varchar(1) NOT NULL default ' ',
  "AAABAHRU" varchar(1) NOT NULL default ' ',
  "AAABAHRV" varchar(1) NOT NULL default ' ',
  "AAABAHRW" varchar(1) NOT NULL default ' ',
  "AAABAHRX" varchar(4) NOT NULL default ' ',
  "AAABAHRY" varchar(1) NOT NULL default ' ',
  "AAABAHRZ" varchar(1) NOT NULL default ' ',
  "AAABAHSA" varchar(1) NOT NULL default ' ',
  "AAABAHSB" varchar(1) NOT NULL default ' ',
  "AAABAHSC" varchar(1) NOT NULL default ' ',
  "AAABAHSD" varchar(1) NOT NULL default ' ',
  "AAABAHSE" varchar(1) NOT NULL default ' ',
  "AAABAHSF" varchar(1) NOT NULL default ' ',
  "AAABAHSG" varchar(1) NOT NULL default ' ',
  "AAABAHSH" varchar(1) NOT NULL default ' ',
  "AAABAHSI" varchar(10) NOT NULL default ' ',
  "AAABAHSJ" varchar(10) NOT NULL default ' ',
  "AAABAHSK" varchar(10) NOT NULL default ' ',
  "AAABAHSL" varchar(10) NOT NULL default ' ',
  "AAABAHSM" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABAHTB","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAHTB" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAHTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHSY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDSB" varchar(10) NOT NULL default ' ',
  "AAABAHTB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDSB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHTE" (
  "AAABAHTM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABAHTM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHTH" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAHTM" varchar(10) NOT NULL default ' ',
  "AAABAHTK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABAHTM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHTN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXFO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAXFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHTS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABAHTV" varchar(20) NOT NULL default ' ',
  "AAABAHTW" VARBINARY(255),
  "AAABAHTX" VARBINARY(255),
  "AAABAHTY" varchar(1) NOT NULL default ' ',
  "AAABAHTZ" varchar(1) NOT NULL default ' ',
  "AAABAMFH" varchar(1) NOT NULL default ' ',
  "AAABERRE" varchar(4) NOT NULL default '0000',
  "AAABANFV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHUF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABLWJT" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHUN" (
  "AAABANFV" varchar(4) NOT NULL default ' ',
  "AAABERRE" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABANFV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHUR" (
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABANFV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHPM","AAABJFGL","AAABJFHB","AAABANFV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHVA" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABANFV" varchar(4) NOT NULL default ' ',
  "AAABAHVD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABANFV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHVG" (
  "AAABBDEQ" varchar(8) NOT NULL default ' ',
  "AAABHMRI" varchar(10) NOT NULL default ' ',
  "AAABAHVJ" varchar(2) NOT NULL default '00',
  "AAABHWBH" varchar(15) NOT NULL default ' ',
  "AAABHWBI" varchar(15) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABBDEQ","AAABHMRI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHVP" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBDEQ" varchar(8) NOT NULL default ' ',
  "AAABHMRI" varchar(10) NOT NULL default ' ',
  "AAABFQPC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABBDEQ","AAABHMRI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHVX" (
  "AAABBDEQ" varchar(8) NOT NULL default ' ',
  "AAABLMVF" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABBDEQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHWC" (
  "AAABHVUJ" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABHVUJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHWF" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHVUJ" varchar(15) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHVUJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHWL" (
  "AAABAHXT" varchar(30) NOT NULL default ' ',
  "AAABLMVF" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABAHXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHWQ" (
  "AAABHMRI" varchar(10) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABHVRQ" varchar(30) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABHMRI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHWW" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHMRI" varchar(10) NOT NULL default ' ',
  "AAABFQPC" varchar(40) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHMRI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHXD" (
  "AAABAHYX" varchar(30) NOT NULL default ' ',
  "AAABAHXF" varchar(2) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABAHYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHXI" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAHYX" varchar(30) NOT NULL default ' ',
  "AAABAHYS" varchar(40) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABAHYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHXP" (
  "AAABBDEQ" varchar(8) NOT NULL default ' ',
  "AAABHMRI" varchar(10) NOT NULL default ' ',
  "AAABAHYX" varchar(30) NOT NULL default ' ',
  "AAABAHXT" varchar(30) NOT NULL default ' ',
  "AAABAHXU" varchar(2) NOT NULL default '00',
  "AAABAHXV" varchar(1) NOT NULL default ' ',
  "AAABAHXW" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABBDEQ","AAABHMRI","AAABAHYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHYB" (
  "AAABBDEQ" varchar(8) NOT NULL default ' ',
  "AAABHMRI" varchar(10) NOT NULL default ' ',
  "AAABAHYX" varchar(30) NOT NULL default ' ',
  "AAABHWBH" varchar(15) NOT NULL default ' ',
  "AAABAHYG" varchar(1) NOT NULL default ' ',
  "AAABHWBI" varchar(15) NOT NULL default ' ',
  "AAABAHYI" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABBDEQ","AAABHMRI","AAABAHYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHYN" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBDEQ" varchar(8) NOT NULL default ' ',
  "AAABHMRI" varchar(10) NOT NULL default ' ',
  "AAABAHYX" varchar(30) NOT NULL default ' ',
  "AAABAHYS" varchar(40) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABBDEQ","AAABHMRI","AAABAHYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHYY" (
  "AAABAHZI" varchar(2) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABAHZI","AAABMKGJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHZD" (
  "AAABAHZI" varchar(2) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAHZH" varchar(74) NOT NULL default ' ',
  PRIMARY KEY ("AAABAHZI","AAABMKGJ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHZL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAKXA" varchar(12) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABERJX" varchar(2) NOT NULL default '00',
  "AAABHVTT" varchar(2) NOT NULL default '00',
  "AAABFWJA" varchar(2) NOT NULL default '00',
  "AAABAICH" varchar(2) NOT NULL default '00',
  "AAABAICI" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABAKXA","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAHZX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAKXQ" varchar(12) NOT NULL default ' ',
  "AAABAKWK" varchar(12) NOT NULL default ' ',
  "AAABAIHV" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABAKXQ","AAABAKWK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIAF" (
  "AAABAKWC" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABAKWC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIAI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAKXI" varchar(12) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABAKWS" varchar(12) NOT NULL default ' ',
  "AAABAIAN" varchar(2) NOT NULL default '00',
  "AAABAIAO" varchar(2) NOT NULL default '00',
  "AAABAIAP" varchar(1) NOT NULL default ' ',
  "AAABAIHK" varchar(2) NOT NULL default '00',
  "AAABAIHL" varchar(2) NOT NULL default '00',
  "AAABAIHM" varchar(2) NOT NULL default '00',
  "AAABAIAT" varchar(1) NOT NULL default ' ',
  "AAABAIAU" varchar(1) NOT NULL default ' ',
  "AAABAIAV" varchar(1) NOT NULL default ' ',
  "AAABAIAW" varchar(1) NOT NULL default ' ',
  "AAABAIAX" varchar(1) NOT NULL default ' ',
  "AAABAIAY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAKXI","AAABLHPM","AAABAKWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIBD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAKWK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAKWK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIBI" (
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABAKWS" varchar(12) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABJDGB" varchar(2) NOT NULL default ' ',
  "AAABMKXF" varchar(12) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHPM","AAABAKWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIBT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAKXA" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAKXA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIBY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAKXI" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAKXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAICD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAKXA" varchar(12) NOT NULL default ' ',
  "AAABIAET" varchar(4) NOT NULL default ' ',
  "AAABAICH" varchar(2) NOT NULL default '00',
  "AAABAICI" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABAKXA","AAABIAET")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAICM" (
  "AAABAIDP" varchar(25) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABAIDP","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAICR" (
  "AAABAIDP" varchar(25) NOT NULL default ' ',
  "AAABAIGG" varchar(12) NOT NULL default ' ',
  "AAABALXJ" varchar(1) NOT NULL default ' ',
  "AAABAIDH" varchar(255) NOT NULL default ' ',
  "AAABAIDI" varchar(255) NOT NULL default ' ',
  "AAABAIDJ" varchar(255) NOT NULL default ' ',
  "AAABAIDK" varchar(255) NOT NULL default ' ',
  "AAABAIDL" varchar(30) NOT NULL default ' ',
  "AAABAIDM" tinyint(3) unsigned NOT NULL default '0',
  "AAABAIDN" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABAIDP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIDD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAIDP" varchar(25) NOT NULL default ' ',
  "AAABAIGG" varchar(12) NOT NULL default ' ',
  "AAABAIDH" varchar(255) NOT NULL default ' ',
  "AAABAIDI" varchar(255) NOT NULL default ' ',
  "AAABAIDJ" varchar(255) NOT NULL default ' ',
  "AAABAIDK" varchar(255) NOT NULL default ' ',
  "AAABAIDL" varchar(30) NOT NULL default ' ',
  "AAABAIDM" tinyint(3) unsigned NOT NULL default '0',
  "AAABAIDN" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABAIDP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIDQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIDW" (
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLTXO" varchar(30) NOT NULL default ' ',
  "AAABLLIO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQDU","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIED" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABHIOX" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMQDU","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIEL" (
  "AAABAIGZ" varchar(12) NOT NULL default ' ',
  "AAABAIFA" varchar(30) NOT NULL default ' ',
  "AAABHWBG" varchar(8) NOT NULL default ' ',
  "AAABHWBK" decimal(5,0) NOT NULL default '0',
  "AAABHWBH" varchar(15) NOT NULL default ' ',
  "AAABHWBI" varchar(15) NOT NULL default ' ',
  "AAABAIFF" varchar(1) NOT NULL default ' ',
  "AAABHVRQ" varchar(30) NOT NULL default ' ',
  "AAABAIFH" varchar(1) NOT NULL default ' ',
  "AAABHWBL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABAIGZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIEX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAIGZ" varchar(12) NOT NULL default ' ',
  "AAABAIFA" varchar(30) NOT NULL default ' ',
  "AAABHWBG" varchar(8) NOT NULL default ' ',
  "AAABHWBK" decimal(5,0) NOT NULL default '0',
  "AAABHWBH" varchar(15) NOT NULL default ' ',
  "AAABHWBI" varchar(15) NOT NULL default ' ',
  "AAABAIFF" varchar(1) NOT NULL default ' ',
  "AAABHVRQ" varchar(30) NOT NULL default ' ',
  "AAABAIFH" varchar(1) NOT NULL default ' ',
  "AAABHWBL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAIGZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIFL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAIGZ" varchar(12) NOT NULL default ' ',
  "AAABAIGX" varchar(67) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAIGZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIFT" (
  "AAABAIGG" varchar(12) NOT NULL default ' ',
  "AAABAIGZ" varchar(12) NOT NULL default ' ',
  "AAABAIGD" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABAIGG","AAABAIGZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIFZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAIGG" varchar(12) NOT NULL default ' ',
  "AAABAIGZ" varchar(12) NOT NULL default ' ',
  "AAABAIGD" varchar(2) NOT NULL default '00',
  "AAABMFSI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAIGG","AAABAIGZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIGI" (
  "AAABAIGZ" varchar(12) NOT NULL default ' ',
  "AAABAIGT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABAIGZ","AAABAIGT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIGN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAIGZ" varchar(12) NOT NULL default ' ',
  "AAABAIGT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAIGZ","AAABAIGT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIGU" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAIGZ" varchar(12) NOT NULL default ' ',
  "AAABAIGX" varchar(67) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABAIGZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIHA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAKXQ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAKXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIHF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAKXI" varchar(12) NOT NULL default ' ',
  "AAABIAET" varchar(4) NOT NULL default ' ',
  "AAABAKYE" varchar(12) NOT NULL default ' ',
  "AAABAIHK" varchar(2) NOT NULL default '00',
  "AAABAIHL" varchar(2) NOT NULL default '00',
  "AAABAIHM" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABAKXI","AAABIAET","AAABAKYE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIHR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAKWK" varchar(12) NOT NULL default ' ',
  "AAABHFSP" varchar(2) NOT NULL default ' ',
  "AAABAIHV" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABAKWK","AAABHFSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIHZ" (
  "AAABHFSP" varchar(2) NOT NULL default ' ',
  "AAABHSZC" varchar(1) NOT NULL default ' ',
  "AAABAIIC" varchar(17) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIIF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAKWC" varchar(12) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABAKXI" varchar(12) NOT NULL default ' ',
  "AAABAKXA" varchar(12) NOT NULL default ' ',
  "AAABAKXQ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAKWC","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIIP" (
  "AAABIAET" varchar(4) NOT NULL default ' ',
  "AAABAKYE" varchar(12) NOT NULL default ' ',
  "AAABAIIS" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIAET","AAABAKYE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIIX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAKXA" varchar(12) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABERJX" varchar(2) NOT NULL default '00',
  "AAABHVTT" varchar(2) NOT NULL default '00',
  "AAABFWJA" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABAKXA","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIJH" (
  "AAABAIXO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABAIXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIJK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAIKL" varchar(15) NOT NULL default ' ',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAIKL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIJQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAIKL" varchar(15) NOT NULL default ' ',
  "AAABAIXH" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABAIKL","AAABAIXH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIJX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAIKL" varchar(15) NOT NULL default ' ',
  "AAABAIXH" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABAIKL","AAABAIXH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIKE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAIKL" varchar(15) NOT NULL default ' ',
  "AAABAIKI" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAIKL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAIRV" varchar(8) NOT NULL default '00000000',
  "AAABAIRW" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAIRV","AAABAIRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAIRV" varchar(8) NOT NULL default '00000000',
  "AAABAIXH" varchar(1) NOT NULL default '0',
  "AAABAIXI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAIRV","AAABAIXH","AAABAIXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAILC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAIRV" varchar(8) NOT NULL default '00000000',
  "AAABAILI" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAIRV","AAABAILI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAILJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAIRV" varchar(8) NOT NULL default '00000000',
  "AAABAIYX" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAIRV","AAABAIYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAILQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAIRV" varchar(8) NOT NULL default '00000000',
  "AAABAIZC" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAIRV","AAABAIZC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAILX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAINM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAINM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIMC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAINM" varchar(4) NOT NULL default ' ',
  "AAABAIMG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAINM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIMK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAIRV" varchar(8) NOT NULL default '00000000',
  "AAABAIRW" varchar(8) NOT NULL default '00000000',
  "AAABAIMT" varchar(2) NOT NULL default ' ',
  "AAABAIMP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAIRV","AAABAIRW","AAABAIMT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIMU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAIRV" varchar(8) NOT NULL default '00000000',
  "AAABAIRW" varchar(8) NOT NULL default '00000000',
  "AAABAIND" varchar(2) NOT NULL default ' ',
  "AAABAIMZ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAIRV","AAABAIRW","AAABAIND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAINE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAIRV" varchar(8) NOT NULL default '00000000',
  "AAABAIRW" varchar(8) NOT NULL default '00000000',
  "AAABAINM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAIRV","AAABAIRW","AAABAINM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAINN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAIRV" varchar(8) NOT NULL default '00000000',
  "AAABAIRW" varchar(8) NOT NULL default '00000000',
  "AAABAISR" varchar(2) NOT NULL default ' ',
  "AAABAISE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAIRV","AAABAIRW","AAABAISR","AAABAISE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAINY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAIRV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAIRV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIOD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAIRV" varchar(8) NOT NULL default '00000000',
  "AAABAIOH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAIRV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIOL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAIRV" varchar(8) NOT NULL default '00000000',
  "AAABAIRW" varchar(8) NOT NULL default '00000000',
  "AAABAIOT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAIRV","AAABAIRW","AAABAIOT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAIRV" varchar(8) NOT NULL default '00000000',
  "AAABAIRW" varchar(8) NOT NULL default '00000000',
  "AAABAIPC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAIRV","AAABAIRW","AAABAIPC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAIRV" varchar(8) NOT NULL default '00000000',
  "AAABAIRW" varchar(8) NOT NULL default '00000000',
  "AAABAIWR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAIRV","AAABAIRW","AAABAIWR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIPM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAIRV" varchar(8) NOT NULL default '00000000',
  "AAABAIRW" varchar(8) NOT NULL default '00000000',
  "AAABAIXH" varchar(1) NOT NULL default '0',
  "AAABAIXI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAIRV","AAABAIRW","AAABAIXH","AAABAIXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIPX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAIRV" varchar(8) NOT NULL default '00000000',
  "AAABAIRW" varchar(8) NOT NULL default '00000000',
  "AAABAIQC" varchar(40) NOT NULL default ' ',
  "AAABAIQD" varchar(40) NOT NULL default ' ',
  "AAABAIQE" varchar(40) NOT NULL default ' ',
  "AAABAIQF" varchar(40) NOT NULL default ' ',
  "AAABAIQG" varchar(40) NOT NULL default ' ',
  "AAABAIQH" varchar(40) NOT NULL default ' ',
  "AAABAIQI" varchar(40) NOT NULL default ' ',
  "AAABAIQJ" varchar(40) NOT NULL default ' ',
  "AAABAIQK" varchar(40) NOT NULL default ' ',
  "AAABAIQL" varchar(40) NOT NULL default ' ',
  "AAABAIQM" varchar(40) NOT NULL default ' ',
  "AAABAIQN" varchar(40) NOT NULL default ' ',
  "AAABAIQO" varchar(40) NOT NULL default ' ',
  "AAABAIQP" varchar(40) NOT NULL default ' ',
  "AAABAIQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAIRV","AAABAIRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIQV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAIRV" varchar(8) NOT NULL default '00000000',
  "AAABAIRW" varchar(8) NOT NULL default '00000000',
  "AAABAIYS" varchar(2) NOT NULL default ' ',
  "AAABDMCO" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAIRV","AAABAIRW","AAABAIYS","AAABDMCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIRG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAIRV" varchar(8) NOT NULL default '00000000',
  "AAABAIRW" varchar(8) NOT NULL default '00000000',
  "AAABAIYX" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAIRV","AAABAIRW","AAABAIYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIRP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAIRV" varchar(8) NOT NULL default '00000000',
  "AAABAIRW" varchar(8) NOT NULL default '00000000',
  "AAABAIZC" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAIRV","AAABAIRW","AAABAIZC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIRY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAISR" varchar(2) NOT NULL default ' ',
  "AAABAISE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAISR","AAABAISE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAISF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAISR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAISR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAISK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAISR" varchar(2) NOT NULL default ' ',
  "AAABAISO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAISR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAISS" (
  "AAABGPJN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABGPJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAISV" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGPJN" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABGPJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAITB" (
  "AAABGPJN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABGPJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAITE" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGPJN" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABGPJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAITK" (
  "AAABGPJN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABGPJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAITN" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGPJN" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABGPJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAITT" (
  "AAABGPJN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGPJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAITW" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGPJN" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABGPJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIUC" (
  "AAABGPJN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABGPJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIUF" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGPJN" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABGPJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIUL" (
  "AAABGPJN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABGPJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIUO" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGPJN" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABGPJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIUU" (
  "AAABGPJN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABGPJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIUX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGPJN" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABGPJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIVD" (
  "AAABGPJN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABGPJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIVG" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGPJN" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABGPJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIVM" (
  "AAABGPJN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABGPJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIVP" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGPJN" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABGPJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIVV" (
  "AAABGPJN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABGPJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIVY" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGPJN" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABGPJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIWE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAIWR" varchar(2) NOT NULL default ' ',
  "AAABAIWH" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAIWR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIWK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAIWR" varchar(2) NOT NULL default ' ',
  "AAABLNRB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAIWR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIWS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAIXH" varchar(1) NOT NULL default '0',
  "AAABAIXI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAIXH","AAABAIXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIWZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAIXH" varchar(1) NOT NULL default '0',
  "AAABAIXI" varchar(1) NOT NULL default ' ',
  "AAABAIXE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAIXH","AAABAIXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIXJ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAIXO" varchar(4) NOT NULL default ' ',
  "AAABAIXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABAIXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIXP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAIYS" varchar(2) NOT NULL default ' ',
  "AAABDMCO" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAIYS","AAABDMCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIXW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAIYS" varchar(2) NOT NULL default ' ',
  "AAABDMCO" varchar(32) NOT NULL default ' ',
  "AAABAIYB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAIYS","AAABDMCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIYG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAIYS" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAIYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIYL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAIYS" varchar(2) NOT NULL default ' ',
  "AAABAIYP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAIYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIYT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAIYX" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAIYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIYY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAIZC" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAIZC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIZD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAJQL" varchar(2) NOT NULL default ' ',
  "AAABAIZV" varchar(4) NOT NULL default ' ',
  "AAABAIZH" varchar(1) NOT NULL default ' ',
  "AAABAIZI" varchar(1) NOT NULL default ' ',
  "AAABAIZJ" varchar(1) NOT NULL default ' ',
  "AAABAIZK" varchar(1) NOT NULL default ' ',
  "AAABAIZL" varchar(1) NOT NULL default ' ',
  "AAABAIZM" varchar(1) NOT NULL default ' ',
  "AAABAIZN" varchar(1) NOT NULL default ' ',
  "AAABAIZO" varchar(1) NOT NULL default ' ',
  "AAABAIZP" varchar(1) NOT NULL default ' ',
  "AAABAIZQ" varchar(1) NOT NULL default ' ',
  "AAABAIZR" varchar(1) NOT NULL default ' ',
  "AAABAIZS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAJQL","AAABAIZV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAIZW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABEIQX" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBOTQ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABEIQX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJAH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEIQX" varchar(4) NOT NULL default ' ',
  "AAABAJAK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEIQX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEIQX" varchar(4) NOT NULL default ' ',
  "AAABAJAQ" varchar(1) NOT NULL default ' ',
  "AAABAJAR" varchar(1) NOT NULL default ' ',
  "AAABAJAS" varchar(1) NOT NULL default ' ',
  "AAABAJAT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEIQX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJAW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEIQX" varchar(4) NOT NULL default ' ',
  "AAABAJBA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEIQX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJBE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAJQL" varchar(2) NOT NULL default ' ',
  "AAABAJCW" varchar(4) NOT NULL default ' ',
  "AAABAJCX" varchar(4) NOT NULL default ' ',
  "AAABAJBJ" varchar(1) NOT NULL default ' ',
  "AAABAJBK" varchar(1) NOT NULL default ' ',
  "AAABAJBL" varchar(1) NOT NULL default ' ',
  "AAABAJBM" varchar(1) NOT NULL default ' ',
  "AAABAJBN" varchar(1) NOT NULL default ' ',
  "AAABAJBO" varchar(1) NOT NULL default ' ',
  "AAABAJBP" varchar(1) NOT NULL default ' ',
  "AAABAJBQ" varchar(1) NOT NULL default ' ',
  "AAABAJBR" varchar(1) NOT NULL default ' ',
  "AAABAJBS" varchar(1) NOT NULL default ' ',
  "AAABAJBT" varchar(1) NOT NULL default ' ',
  "AAABAJBU" varchar(1) NOT NULL default ' ',
  "AAABAJBV" varchar(1) NOT NULL default ' ',
  "AAABAJBW" varchar(1) NOT NULL default ' ',
  "AAABAJBX" varchar(1) NOT NULL default ' ',
  "AAABAJBY" varchar(1) NOT NULL default ' ',
  "AAABAJBZ" varchar(1) NOT NULL default ' ',
  "AAABAJCA" varchar(1) NOT NULL default ' ',
  "AAABAJCB" varchar(1) NOT NULL default ' ',
  "AAABAJCC" varchar(1) NOT NULL default ' ',
  "AAABAJCD" varchar(1) NOT NULL default ' ',
  "AAABAJCE" varchar(1) NOT NULL default ' ',
  "AAABAJCF" varchar(1) NOT NULL default ' ',
  "AAABAJCG" varchar(1) NOT NULL default ' ',
  "AAABAJCH" varchar(1) NOT NULL default ' ',
  "AAABAJCI" varchar(1) NOT NULL default ' ',
  "AAABAJCJ" varchar(1) NOT NULL default ' ',
  "AAABAJCK" varchar(1) NOT NULL default ' ',
  "AAABAJCL" varchar(1) NOT NULL default ' ',
  "AAABAJCM" varchar(1) NOT NULL default ' ',
  "AAABAJCN" varchar(1) NOT NULL default ' ',
  "AAABAJCO" varchar(1) NOT NULL default ' ',
  "AAABAJCP" varchar(1) NOT NULL default ' ',
  "AAABAJCQ" varchar(1) NOT NULL default ' ',
  "AAABAJCR" varchar(1) NOT NULL default ' ',
  "AAABAJCS" varchar(1) NOT NULL default ' ',
  "AAABAJCT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAJQL","AAABAJCW","AAABAJCX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJCY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMYV" varchar(4) NOT NULL default ' ',
  "AAABAJDB" varchar(1) NOT NULL default ' ',
  "AAABAJDQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMYV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJDF" (
  "AAABMMYV" varchar(4) NOT NULL default ' ',
  "AAABMJWN" varchar(4) NOT NULL default ' ',
  "AAABEGDU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMYV","AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJDL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMYV" varchar(4) NOT NULL default ' ',
  "AAABMJWN" varchar(4) NOT NULL default ' ',
  "AAABLGDP" varchar(20) NOT NULL default ' ',
  "AAABAJDQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMYV","AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJDU" (
  "AAABMMYV" varchar(4) NOT NULL default ' ',
  "AAABAJDW" varchar(1) NOT NULL default ' ',
  "AAABAJDX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMYV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJDZ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMMYV" varchar(4) NOT NULL default ' ',
  "AAABAJEC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMMYV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJEF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAQFE" varchar(2) NOT NULL default ' ',
  "AAABLILW" varchar(3) NOT NULL default ' ',
  "AAABEIQX" varchar(4) NOT NULL default ' ',
  "AAABAJIE" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKPXV" varchar(2) NOT NULL default '00',
  "AAABAJEO" decimal(15,2) NOT NULL default '0.00',
  "AAABAJEP" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABAJER" decimal(15,2) NOT NULL default '0.00',
  "AAABGWOD" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABAQFE","AAABLILW","AAABEIQX","AAABAJIE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJEZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAJQL" varchar(2) NOT NULL default ' ',
  "AAABAJIE" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAJQL","AAABAJIE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJFG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAJQL" varchar(2) NOT NULL default ' ',
  "AAABAJIE" varchar(3) NOT NULL default ' ',
  "AAABAJFL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAJQL","AAABAJIE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJFQ" (
  "AAABMJWN" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJFV" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMJWN" varchar(4) NOT NULL default ' ',
  "AAABAJFY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJGB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAJQL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAJQL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJGG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAJQL" varchar(2) NOT NULL default ' ',
  "AAABHIHN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAJQL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJGO" (
  "AAABAJGT" varchar(2) NOT NULL default '00',
  "AAABAJGU" varchar(2) NOT NULL default '00',
  "AAABIWAM" int(11) NOT NULL default '0',
  "AAABGZRX" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABAJGT","AAABAJGU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJGV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAQFE" varchar(2) NOT NULL default ' ',
  "AAABLILW" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAQFE","AAABLILW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJHC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAQFE" varchar(2) NOT NULL default ' ',
  "AAABLILW" varchar(3) NOT NULL default ' ',
  "AAABAJHH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAQFE","AAABLILW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJHM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAQFE" varchar(2) NOT NULL default ' ',
  "AAABLILW" varchar(3) NOT NULL default ' ',
  "AAABEIQX" varchar(4) NOT NULL default ' ',
  "AAABAJIE" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHDK" decimal(15,2) NOT NULL default '0.00',
  "AAABKWVR" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABAQKE" decimal(15,2) NOT NULL default '0.00',
  "AAABMKAI" varchar(3) NOT NULL default ' ',
  "AAABIWAM" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABAQFE","AAABLILW","AAABEIQX","AAABAJIE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJIG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHWBH" varchar(15) NOT NULL default ' ',
  "AAABHVUU" varchar(15) NOT NULL default ' ',
  "AAABHVUV" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHWBH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJIN" (
  "AAABHWBH" varchar(15) NOT NULL default ' ',
  "AAABHVUU" varchar(15) NOT NULL default ' ',
  "AAABHVUV" varchar(15) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABHWBH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJIT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABBEKV" varchar(1) NOT NULL default ' ',
  "AAABAJIY" varchar(1) NOT NULL default ' ',
  "AAABAKDM" varchar(1) NOT NULL default ' ',
  "AAABAKDN" varchar(1) NOT NULL default ' ',
  "AAABAJJB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHPM","AAABJFHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJJF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHMRO" tinyint(3) unsigned NOT NULL default '0',
  "AAABAJKM" varchar(30) NOT NULL default ' ',
  "AAABAJKZ" varchar(20) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHMRO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAKFL" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABAKFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJJS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAKFL" varchar(10) NOT NULL default '0000000000',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAKFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJKA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAJKM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAJKM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJKF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAJKM" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAJKM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJKN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAJKZ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAJKZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJKS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAJKZ" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAJKZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJLA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAJWN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJLF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAJLW" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAJLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJLK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAJLW" varchar(12) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAJLQ" varchar(40) NOT NULL default ' ',
  "AAABAJLR" varchar(40) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  "AAABAJLT" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAJLW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJLY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAJMQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAJMQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJMD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMZU" varchar(14) NOT NULL default ' ',
  "AAABAJMQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMZU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJMJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAJMQ" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAJMQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJMR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAJMT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJMV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAJNB" varchar(14) NOT NULL default ' ',
  "AAABLSAX" varchar(1) NOT NULL default ' ',
  "AAABIMZU" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAJNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJNC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHMRO" tinyint(3) unsigned NOT NULL default '0',
  "AAABAKBW" varchar(30) NOT NULL default ' ',
  "AAABAJNG" varchar(1) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHMRO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJNK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAJNM" varchar(3) NOT NULL default '000',
  "AAABAJNN" varchar(1) NOT NULL default ' ',
  "AAABKWET" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJNQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJNU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAKBW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAKBW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJNZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAKBW" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAKBW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJOH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGJZC" varchar(10) NOT NULL default ' ',
  "AAABAJOK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGJZC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJON" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGJZC" varchar(10) NOT NULL default ' ',
  "AAABCRGY" varchar(10) NOT NULL default ' ',
  "AAABKZAW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABGJZC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJOW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAJOY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJPA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKPBA" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAKFU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAKFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJPM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMZU" varchar(14) NOT NULL default ' ',
  "AAABAKFU" varchar(30) NOT NULL default ' ',
  "AAABAKGE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMZU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJPT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAKFU" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAKFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJQB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMZU" varchar(14) NOT NULL default ' ',
  "AAABAJQE" varchar(1) NOT NULL default ' ',
  "AAABAJQF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMZU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJQI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMZU" varchar(14) NOT NULL default ' ',
  "AAABAJQL" varchar(2) NOT NULL default ' ',
  "AAABLILW" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMZU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJQP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMZU" varchar(14) NOT NULL default ' ',
  "AAABAJQS" varchar(4) NOT NULL default ' ',
  "AAABAJQT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMZU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJQW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMZU" varchar(14) NOT NULL default ' ',
  "AAABAJQZ" varchar(14) NOT NULL default ' ',
  "AAABAJRA" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMZU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJRD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAJRJ" varchar(14) NOT NULL default ' ',
  "AAABLSAX" varchar(1) NOT NULL default ' ',
  "AAABIMZU" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAJRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJRK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMRM" varchar(10) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMRM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJRQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMMRM" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMMRM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJRY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAJSP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAJSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJSD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAJSP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAJSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJSI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAJSP" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAJSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJSW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAJTF" varchar(14) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABAKIS" varchar(1) NOT NULL default ' ',
  "AAABAKIT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAJTF","AAABKFCV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJTI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAJTZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAJTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJTN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAJTZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAJTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJTS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAJTZ" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAJTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJUA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRXL" varchar(8) NOT NULL default ' ',
  "AAABAJUD" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJUG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLRXL" varchar(8) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLRXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJUO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAJVE" varchar(8) NOT NULL default ' ',
  "AAABAJUR" varchar(8) NOT NULL default ' ',
  "AAABAJUS" varchar(1) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAJVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJUX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAJVE" varchar(8) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAJVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJVF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAJVU" varchar(10) NOT NULL default ' ',
  "AAABAJVI" varchar(1) NOT NULL default ' ',
  "AAABAJVJ" varchar(1) NOT NULL default ' ',
  "AAABAJVK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAJVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJVN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAJVU" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAJVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJVV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAJWN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJWA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMZU" varchar(14) NOT NULL default ' ',
  "AAABAJWN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMZU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJWG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAJWN" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJWO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAJXA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAJXA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJWT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAJXA" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAJXA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJXB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAJXN" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAJXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJXG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAJXN" varchar(8) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAJXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJXO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBIKQ" varchar(5) NOT NULL default ' ',
  "AAABAJXR" varchar(1) NOT NULL default ' ',
  "AAABAJXS" varchar(1) NOT NULL default ' ',
  "AAABAJXT" varchar(1) NOT NULL default ' ',
  "AAABAJXU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBIKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJXX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAJYO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAJYO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJYC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAJYO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAJYO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJYH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAJYO" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAJYO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJYP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABAJYT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFGL","AAABJFHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJYX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBETZ" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAJZC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBETZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJZG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAWMP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAWMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJZL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAWMP" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAWMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJZT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBETZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBETZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAJZY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBETZ" varchar(3) NOT NULL default ' ',
  "AAABMRCK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBETZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKAG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAKBA" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAKBA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKAL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAKBA" varchar(3) NOT NULL default ' ',
  "AAABMRCK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAKBA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKAT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAKBA" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAKBA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKBC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMZU" varchar(14) NOT NULL default ' ',
  "AAABAKFD" varchar(30) NOT NULL default ' ',
  "AAABAKBG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMZU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKBJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKBO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAKBV" varchar(4) NOT NULL default ' ',
  "AAABAKBW" varchar(8) NOT NULL default ' ',
  "AAABHMRO" tinyint(3) unsigned NOT NULL default '0',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAKBV","AAABAKBW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKBX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAKFD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAKFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKCC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAKFD" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAKFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKCK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAKDF" varchar(3) NOT NULL default ' ',
  "AAABMRCK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAKDF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKCQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAKDF" varchar(3) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAKDF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKCY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAKDF" varchar(3) NOT NULL default ' ',
  "AAABAKDB" varchar(8) NOT NULL default '00000000',
  "AAABAKDC" varchar(3) NOT NULL default '000',
  "AAABAKDD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAKDF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKDG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABAKDK" varchar(1) NOT NULL default ' ',
  "AAABAKDL" varchar(1) NOT NULL default ' ',
  "AAABAKDM" varchar(1) NOT NULL default ' ',
  "AAABAKDN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHPM","AAABJFHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKDR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAKEJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAKEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKDW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMZU" varchar(14) NOT NULL default ' ',
  "AAABAKEJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMZU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKEC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAKEJ" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAKEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKEK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMJWG" varchar(6) NOT NULL default ' ',
  "AAABAKEN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMJWG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMJWG" varchar(6) NOT NULL default ' ',
  "AAABAKEU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMJWG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKEY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAKFD" varchar(30) NOT NULL default ' ',
  "AAABMJWG" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAKFD","AAABMJWG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKFF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNJ" varchar(1) NOT NULL default ' ',
  "AAABAKFL" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABMCNJ","AAABAKFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKFM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAKGE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAKGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKFR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMZU" varchar(14) NOT NULL default ' ',
  "AAABAKFU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMZU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKFX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAKGE" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAKGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKGF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKVKH" varchar(5) NOT NULL default ' ',
  "AAABGPST" varchar(1) NOT NULL default ' ',
  "AAABAKGJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKVKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKGM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKVKH" varchar(5) NOT NULL default ' ',
  "AAABAKGQ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABKVKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKGU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAWMP" varchar(2) NOT NULL default ' ',
  "AAABAWMH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAWMP","AAABAWMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKHB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAWMP" varchar(2) NOT NULL default ' ',
  "AAABAWMH" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAWMP","AAABAWMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKHL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGBL" varchar(2) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABIFTR" decimal(10,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABKGBL","AAABHVYP","AAABBEON","AAABBEOO","AAABBEOP","AAABGLBU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKIE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMZU" varchar(14) NOT NULL default ' ',
  "AAABAKIH" varchar(1) NOT NULL default ' ',
  "AAABKPMT" varchar(1) NOT NULL default '0',
  "AAABAKIJ" varchar(1) NOT NULL default ' ',
  "AAABIADW" varchar(1) NOT NULL default ' ',
  "AAABAKIL" varchar(1) NOT NULL default ' ',
  "AAABAKIM" varchar(14) NOT NULL default ' ',
  "AAABAKIN" varchar(14) NOT NULL default ' ',
  "AAABAKIO" varchar(14) NOT NULL default ' ',
  "AAABAKIP" varchar(14) NOT NULL default ' ',
  "AAABAKIQ" varchar(14) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABAKIS" varchar(1) NOT NULL default ' ',
  "AAABAKIT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMZU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAKJB" varchar(5) NOT NULL default ' ',
  "AAABAKIZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAKJB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKJC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBIKQ" varchar(5) NOT NULL default ' ',
  "AAABAKJF" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABBIKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKJI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSKS" varchar(8) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABAKJN" varchar(40) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJCDT" varchar(8) NOT NULL default ' ',
  "AAABAMFE" varchar(5) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSKS","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKJW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAKNA" varchar(8) NOT NULL default ' ',
  "AAABAKKA" varchar(40) NOT NULL default ' ',
  "AAABAKLA" varchar(8) NOT NULL default ' ',
  "AAABAKLB" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAKNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIKUE" varchar(8) NOT NULL default ' ',
  "AAABAKNA" varchar(8) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJHHP" varchar(14) NOT NULL default ' ',
  "AAABAKKO" varchar(8) NOT NULL default ' ',
  "AAABAKKP" varchar(8) NOT NULL default ' ',
  "AAABAKLE" varchar(8) NOT NULL default ' ',
  "AAABAKKR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIKUE","AAABAKNA","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAKLE" varchar(8) NOT NULL default ' ',
  "AAABAKKZ" varchar(40) NOT NULL default ' ',
  "AAABAKLA" varchar(8) NOT NULL default ' ',
  "AAABAKLB" varchar(8) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAKLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKLF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIKUE" varchar(8) NOT NULL default ' ',
  "AAABAKNA" varchar(8) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABAKLL" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABIKUE","AAABAKNA","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKLR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSKS" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKLW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSKS" varchar(8) NOT NULL default ' ',
  "AAABAKMA" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKME" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABIKUE" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKMK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIKUE" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIKUE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKMP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSKS" varchar(8) NOT NULL default ' ',
  "AAABAKNA" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSKS","AAABAKNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKMW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAKNA" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAKNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKNB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSKS" varchar(8) NOT NULL default ' ',
  "AAABAKNL" varchar(3) NOT NULL default '000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJCDT" varchar(8) NOT NULL default ' ',
  "AAABAMFE" varchar(5) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSKS","AAABAKNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKNM" (
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABAKNO" varchar(30) NOT NULL default ' ',
  "AAABAKNP" varchar(30) NOT NULL default ' ',
  "AAABAKNQ" varchar(30) NOT NULL default ' ',
  "AAABAKNR" varchar(30) NOT NULL default ' ',
  "AAABAKNS" varchar(40) NOT NULL default ' ',
  "AAABAKNT" varchar(30) NOT NULL default ' ',
  "AAABAKNU" varchar(4) NOT NULL default ' ',
  "AAABAKNV" varchar(30) NOT NULL default ' ',
  "AAABAKNW" varchar(30) NOT NULL default ' ',
  "AAABAKNX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABJFGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKNZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABKPDJ" varchar(8) NOT NULL default '00000000',
  "AAABHVXC" varchar(2) NOT NULL default ' ',
  "AAABBHFX" varchar(2) NOT NULL default ' ',
  "AAABAKOG" varchar(1) NOT NULL default ' ',
  "AAABJEZD" varchar(1) NOT NULL default ' ',
  "AAABAKOI" varchar(1) NOT NULL default ' ',
  "AAABAKOJ" varchar(1) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABAKOM" varchar(14) NOT NULL default ' ',
  "AAABAKON" varchar(8) NOT NULL default '00000000',
  "AAABAKOO" varchar(8) NOT NULL default '00000000',
  "AAABAKOP" varchar(8) NOT NULL default '00000000',
  "AAABAKOQ" varchar(8) NOT NULL default '00000000',
  "AAABAKOR" varchar(8) NOT NULL default '00000000',
  "AAABAKOS" varchar(8) NOT NULL default '00000000',
  "AAABAKOT" varchar(2) NOT NULL default ' ',
  "AAABAKOU" varchar(8) NOT NULL default '00000000',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABJFHJ"),
  KEY "AAABAKPA" ("AAABMRIL","AAABLUNJ","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKPE" (
  "AAABLILW" varchar(8) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABJDGB" varchar(2) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLILW","AAABLHPM","AAABJFGL","AAABJFHB","AAABJDGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKPQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLILW" varchar(8) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABJDGB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLILW","AAABLHPM","AAABJFGL","AAABJFHB","AAABJDGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKQD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAKQJ" varchar(4) NOT NULL default ' ',
  "AAABHYJM" varchar(2) NOT NULL default ' ',
  "AAABHYJN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAKQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKQK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAKQQ" varchar(1) NOT NULL default ' ',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAKQQ","AAABGCHD","AAABJFHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKQT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAKRP" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABFYUL" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABEPUS" varchar(1) NOT NULL default ' ',
  "AAABAMLT" varchar(3) NOT NULL default ' ',
  "AAABAKUD" varchar(10) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABAKRE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAKRP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKRI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAKRP" varchar(8) NOT NULL default ' ',
  "AAABLXIS" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAKRP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKRQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABKRXK" varchar(2) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABMCWV" varchar(12) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABLPWW" varchar(40) NOT NULL default ' ',
  "AAABAKSB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHJ","AAABKRXK","AAABKFCV","AAABKFCZ","AAABMCWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKSI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABKRXK" varchar(2) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABAKSQ" varchar(3) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABJFHJ","AAABKRXK","AAABKFCV","AAABKFCZ","AAABMDBB","AAABMCRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKTA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABLENL" varchar(10) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABAMNH" varchar(10) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABAKTJ" decimal(15,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABJZKE" varchar(8) NOT NULL default '00000000',
  "AAABJZKD" varchar(8) NOT NULL default '00000000',
  "AAABAMLB" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHJ","AAABLHPM","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKTS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHYMV" varchar(14) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAKTX" varchar(1) NOT NULL default ' ',
  "AAABAKTY" decimal(5,2) NOT NULL default '0.00',
  "AAABKPBA" varchar(5) NOT NULL default ' ',
  "AAABAKUA" decimal(13,2) NOT NULL default '0.00',
  "AAABLIXT" varchar(1) NOT NULL default ' ',
  "AAABAKUC" varchar(2) NOT NULL default '00',
  "AAABAKUD" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHYMV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKUH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABANDY" varchar(8) NOT NULL default ' ',
  "AAABAKUP" varchar(8) NOT NULL default ' ',
  "AAABAKUL" varchar(17) NOT NULL default ' ',
  "AAABANDW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABANDY","AAABAKUP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKUQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABKRXK" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABJFHJ","AAABKRXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKVA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABANDY" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABANDY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKVH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAKVT" varchar(2) NOT NULL default ' ',
  "AAABAKVK" varchar(1) NOT NULL default ' ',
  "AAABAKVL" varchar(1) NOT NULL default ' ',
  "AAABBENB" varchar(1) NOT NULL default ' ',
  "AAABAKVN" varchar(1) NOT NULL default ' ',
  "AAABAKVO" varchar(1) NOT NULL default ' ',
  "AAABAKVP" varchar(1) NOT NULL default ' ',
  "AAABAKVQ" varchar(1) NOT NULL default ' ',
  "AAABANDW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAKVT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKVU" (
  "AAABAKVW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABAKVW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKVX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAKWC" varchar(12) NOT NULL default ' ',
  "AAABLMOZ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABAKWC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKWD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAKWK" varchar(12) NOT NULL default ' ',
  "AAABAKWH" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAKWK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKWL" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABAKWS" varchar(12) NOT NULL default ' ',
  "AAABHIOX" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLHPM","AAABAKWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKWT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAKXA" varchar(12) NOT NULL default ' ',
  "AAABAKWX" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAKXA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKXB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAKXI" varchar(12) NOT NULL default ' ',
  "AAABIJYD" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAKXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKXJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAKXQ" varchar(12) NOT NULL default ' ',
  "AAABAKXN" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAKXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKXR" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHFSP" varchar(2) NOT NULL default ' ',
  "AAABLJJN" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHFSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKXX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIAET" varchar(4) NOT NULL default ' ',
  "AAABAKYE" varchar(12) NOT NULL default ' ',
  "AAABLPUZ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIAET","AAABAKYE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHVXC" varchar(4) NOT NULL default ' ',
  "AAABJBVB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHVXC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKYN" (
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABAMKR" varchar(30) NOT NULL default ' ',
  "AAABAMRT" varchar(4) NOT NULL default ' ',
  "AAABAMRU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHPM","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKYV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLOXS" varchar(4) NOT NULL default ' ',
  "AAABGGHA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLOXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKZD" (
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABAKZH" varchar(25) NOT NULL default ' ',
  "AAABAKZI" varchar(20) NOT NULL default ' ',
  "AAABAKZJ" varchar(30) NOT NULL default ' ',
  "AAABAKZK" varchar(30) NOT NULL default ' ',
  "AAABAKZL" varchar(30) NOT NULL default ' ',
  "AAABAKZM" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNLW","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAKZP" (
  "AAABLILW" varchar(4) NOT NULL default ' ',
  "AAABJCDT" varchar(8) NOT NULL default ' ',
  "AAABALDY" varchar(2) NOT NULL default '00',
  "AAABAKZT" varchar(2) NOT NULL default '00',
  "AAABHVRQ" varchar(30) NOT NULL default ' ',
  "AAABAKZV" varchar(15) NOT NULL default ' ',
  "AAABAKZW" varchar(30) NOT NULL default ' ',
  "AAABAKZX" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABLILW","AAABJCDT","AAABALDY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALAB" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLILW" varchar(4) NOT NULL default ' ',
  "AAABJCDT" varchar(8) NOT NULL default ' ',
  "AAABALDY" varchar(2) NOT NULL default '00',
  "AAABAMRI" varchar(67) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLILW","AAABJCDT","AAABALDY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALAL" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLILW" varchar(4) NOT NULL default ' ',
  "AAABJCDT" varchar(8) NOT NULL default ' ',
  "AAABALDY" varchar(2) NOT NULL default '00',
  "AAABAMRI" varchar(67) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLILW","AAABJCDT","AAABALDY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALAV" (
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABEPJZ" varchar(4) NOT NULL default ' ',
  "AAABALBM" varchar(1) NOT NULL default ' ',
  "AAABHVRQ" varchar(30) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  "AAABLHAH" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNLW","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALBI" (
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABALBK" varchar(1) NOT NULL default ' ',
  "AAABEPJZ" varchar(4) NOT NULL default ' ',
  "AAABALBM" varchar(1) NOT NULL default ' ',
  "AAABHVRQ" varchar(30) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  "AAABLHAH" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALBU" (
  "AAABLILW" varchar(4) NOT NULL default ' ',
  "AAABALCZ" varchar(20) NOT NULL default ' ',
  "AAABALDA" varchar(2) NOT NULL default '00',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABLRQQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLILW","AAABALCZ","AAABALDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALCE" (
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNLW","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALCM" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABLWJT" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALCS" (
  "AAABLILW" varchar(4) NOT NULL default ' ',
  "AAABALCZ" varchar(20) NOT NULL default ' ',
  "AAABALDA" varchar(2) NOT NULL default '00',
  "AAABALCW" varchar(1) NOT NULL default ' ',
  "AAABALCX" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLILW","AAABALCZ","AAABALDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALDB" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABLWJT" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALDH" (
  "AAABLILW" varchar(4) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABJCDT" varchar(8) NOT NULL default ' ',
  "AAABALDY" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLILW","AAABMNLW","AAABJCDT","AAABALDY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALDQ" (
  "AAABLILW" varchar(4) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABJCDT" varchar(8) NOT NULL default ' ',
  "AAABALDY" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLILW","AAABMNLW","AAABJCDT","AAABALDY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALDZ" (
  "AAABLILW" varchar(4) NOT NULL default ' ',
  "AAABALEB" varchar(8) NOT NULL default ' ',
  "AAABALEL" varchar(40) NOT NULL default ' ',
  "AAABALEM" varchar(40) NOT NULL default ' ',
  "AAABALEN" varchar(1) NOT NULL default ' ',
  "AAABALEO" varchar(1) NOT NULL default ' ',
  "AAABGKDC" varchar(1) NOT NULL default ' ',
  "AAABALEH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLILW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALEJ" (
  "AAABLILW" varchar(4) NOT NULL default ' ',
  "AAABALEL" varchar(40) NOT NULL default ' ',
  "AAABALEM" varchar(40) NOT NULL default ' ',
  "AAABALEN" varchar(1) NOT NULL default ' ',
  "AAABALEO" varchar(1) NOT NULL default ' ',
  "AAABGKDC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLILW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALER" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLILW" varchar(4) NOT NULL default ' ',
  "AAABALEU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLILW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALEX" (
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJXCE" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABJFGL","AAABJXCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALFC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABALFH" varchar(10) NOT NULL default ' ',
  "AAABALFI" varchar(4) NOT NULL default ' ',
  "AAABALFJ" varchar(8) NOT NULL default ' ',
  "AAABALFK" varchar(16) NOT NULL default ' ',
  "AAABALFL" varchar(8) NOT NULL default ' ',
  "AAABMIRW" varchar(16) NOT NULL default ' ',
  "AAABALFN" varchar(30) NOT NULL default ' ',
  "AAABALFO" varchar(30) NOT NULL default ' ',
  "AAABALFP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHPM","AAABJFGL","AAABJFHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALFU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABALZE" varchar(4) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABALZE","AAABLHPM","AAABJFGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALGD" (
  "AAABHBMG" varchar(4) NOT NULL default ' ',
  "AAABALGF" varchar(1) NOT NULL default ' ',
  "AAABALGG" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABHBMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALGJ" (
  "AAABLGEF" varchar(70) NOT NULL default ' ',
  "AAABHBMG" varchar(4) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALGO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABALYY" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABALYY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALGT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJCAA" varchar(8) NOT NULL default ' ',
  "AAABMRCK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABJCAA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALHB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLILW" varchar(8) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJCAA" varchar(8) NOT NULL default ' ',
  "AAABALXX" varchar(2) NOT NULL default '00',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABJCBT" varchar(4) NOT NULL default ' ',
  "AAABALYA" varchar(30) NOT NULL default ' ',
  "AAABALYB" varchar(30) NOT NULL default ' ',
  "AAABIKFK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLILW","AAABLHPM","AAABJCAA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALHQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABALYY" varchar(10) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJCDT" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABALYY","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALHY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABALYY" varchar(10) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABALIC" varchar(30) NOT NULL default ' ',
  "AAABALID" varchar(30) NOT NULL default ' ',
  "AAABKGJY" varchar(30) NOT NULL default ' ',
  "AAABALIF" varchar(30) NOT NULL default ' ',
  "AAABBXYA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABALYY","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALIK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDEQ" varchar(8) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABANNN" varchar(24) NOT NULL default ' ',
  "AAABIMHJ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDEQ","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALIT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDEQ" varchar(8) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABANNN" varchar(24) NOT NULL default ' ',
  "AAABIMHJ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDEQ","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALJC" (
  "AAABDUBG" varchar(40) NOT NULL default ' ',
  "AAABLFEA" varchar(8) NOT NULL default '00000000',
  "AAABFTXT" varchar(20) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  "AAABMMJT" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABDUBG","AAABLFEA","AAABFTXT","AAABMRCU"),
  KEY "AAABALJN" ("AAABFTXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALJP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHMMP" varchar(10) NOT NULL default ' ',
  "AAABKNBT" varchar(10) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLUXQ","AAABHMMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALJZ" (
  "AAABLGEF" varchar(70) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABDFYR" varchar(1) NOT NULL default ' ',
  "AAABJCDT" varchar(8) NOT NULL default ' ',
  "AAABALKQ" decimal(5,0) NOT NULL default '0',
  "AAABHWBH" varchar(15) NOT NULL default ' ',
  "AAABHWBI" varchar(15) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEF","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALKK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLGEF" varchar(70) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABDFYR" varchar(1) NOT NULL default ' ',
  "AAABJCDT" varchar(8) NOT NULL default ' ',
  "AAABALKQ" decimal(5,0) NOT NULL default '0',
  "AAABHWBH" varchar(15) NOT NULL default ' ',
  "AAABHWBI" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLGEF","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALKW" (
  "AAABHMMP" varchar(10) NOT NULL default ' ',
  "AAABIIUE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHMMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALLA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLILW" varchar(8) NOT NULL default ' ',
  "AAABHMMP" varchar(10) NOT NULL default ' ',
  "AAABALLE" varchar(10) NOT NULL default '0000000000',
  "AAABLMOC" varchar(8) NOT NULL default '00000000',
  "AAABLMOD" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLILW","AAABHMMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALLK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLILW" varchar(8) NOT NULL default ' ',
  "AAABHMMP" varchar(10) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLILW","AAABHMMP","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALLT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHMMP" varchar(10) NOT NULL default ' ',
  "AAABEINZ" varchar(30) NOT NULL default ' ',
  "AAABMKXX" varchar(30) NOT NULL default ' ',
  "AAABJJLX" varchar(10) NOT NULL default ' ',
  "AAABDBGB" varchar(1) NOT NULL default ' ',
  "AAABHNLS" varchar(1) NOT NULL default ' ',
  "AAABALMB" varchar(1) NOT NULL default ' ',
  "AAABALMC" varchar(1) NOT NULL default ' ',
  "AAABALMD" varchar(1) NOT NULL default ' ',
  "AAABALME" varchar(10) NOT NULL default ' ',
  "AAABLUWW" varchar(1) NOT NULL default ' ',
  "AAABJTEQ" varchar(6) NOT NULL default '000000',
  "AAABMLMY" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUXQ","AAABHMMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALMK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHMMP" varchar(10) NOT NULL default ' ',
  "AAABILTS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABHMMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALMS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLILW" varchar(8) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABJCDT" varchar(8) NOT NULL default ' ',
  "AAABMKCF" varchar(30) NOT NULL default ' ',
  "AAABALMY" varchar(1) NOT NULL default ' ',
  "AAABJSLF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLILW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALNC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLILW" varchar(8) NOT NULL default ' ',
  "AAABLPWW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABLILW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALNK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLILW" varchar(8) NOT NULL default ' ',
  "AAABHMMP" varchar(10) NOT NULL default ' ',
  "AAABJDFW" varchar(2) NOT NULL default '00',
  "AAABMFSJ" varchar(1) NOT NULL default ' ',
  "AAABALNQ" varchar(1) NOT NULL default ' ',
  "AAABMFSI" varchar(1) NOT NULL default ' ',
  "AAABALNS" varchar(1) NOT NULL default ' ',
  "AAABALNT" varchar(30) NOT NULL default ' ',
  "AAABMKCF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLILW","AAABHMMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALNY" (
  "AAABMPMH" varchar(4) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMPMH","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALOE" (
  "AAABLILW" varchar(8) NOT NULL default ' ',
  "AAABALXA" varchar(15) NOT NULL default ' ',
  "AAABALOH" varchar(8) NOT NULL default ' ',
  "AAABALWQ" varchar(20) NOT NULL default ' ',
  "AAABALWR" varchar(20) NOT NULL default ' ',
  "AAABALWS" varchar(20) NOT NULL default ' ',
  "AAABALWU" varchar(30) NOT NULL default ' ',
  "AAABKWRN" varchar(1) NOT NULL default ' ',
  "AAABALWW" varchar(1) NOT NULL default ' ',
  "AAABDKZZ" varchar(4) NOT NULL default ' ',
  "AAABALOP" varchar(4) NOT NULL default ' ',
  "AAABALOQ" varchar(30) NOT NULL default ' ',
  "AAABALOR" varchar(30) NOT NULL default ' ',
  "AAABJFHJ" varchar(20) NOT NULL default ' ',
  "AAABICGD" varchar(20) NOT NULL default ' ',
  "AAABALPA" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLILW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALOX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLILW" varchar(8) NOT NULL default ' ',
  "AAABALPA" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABLILW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALPD" (
  "AAABLILW" varchar(8) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLILW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALPH" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLILW" varchar(8) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLILW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALPN" (
  "AAABLILW" varchar(8) NOT NULL default ' ',
  "AAABHBMG" varchar(4) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABALXA" varchar(15) NOT NULL default ' ',
  "AAABHEYG" varchar(70) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLILW","AAABHBMG","AAABLHPM","AAABALXA","AAABHEYG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALPZ" (
  "AAABHVUJ" varchar(15) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABHVUJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALQD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLILW" varchar(8) NOT NULL default ' ',
  "AAABHBMG" varchar(4) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABALXA" varchar(15) NOT NULL default ' ',
  "AAABHEYG" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLILW","AAABHBMG","AAABLHPM","AAABALXA","AAABHEYG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALQQ" (
  "AAABLILW" varchar(8) NOT NULL default ' ',
  "AAABALRU" varchar(2) NOT NULL default ' ',
  "AAABALRV" varchar(2) NOT NULL default ' ',
  "AAABALRW" varchar(2) NOT NULL default ' ',
  "AAABALRX" varchar(2) NOT NULL default ' ',
  "AAABHEYG" varchar(70) NOT NULL default ' ',
  "AAABALRP" varchar(1) NOT NULL default ' ',
  "AAABALRQ" varchar(2) NOT NULL default ' ',
  "AAABALRR" varchar(2) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLILW","AAABALRU","AAABALRV","AAABALRW","AAABALRX","AAABHEYG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALRH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLILW" varchar(8) NOT NULL default ' ',
  "AAABALRU" varchar(2) NOT NULL default ' ',
  "AAABALRV" varchar(2) NOT NULL default ' ',
  "AAABALRW" varchar(2) NOT NULL default ' ',
  "AAABALRX" varchar(2) NOT NULL default ' ',
  "AAABHEYG" varchar(70) NOT NULL default ' ',
  "AAABALRP" varchar(1) NOT NULL default ' ',
  "AAABALRQ" varchar(2) NOT NULL default ' ',
  "AAABALRR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLILW","AAABALRU","AAABALRV","AAABALRW","AAABALRX","AAABHEYG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALRZ" (
  "AAABLILW" varchar(8) NOT NULL default ' ',
  "AAABLTXO" varchar(46) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLILW","AAABLTXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALSF" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLILW" varchar(8) NOT NULL default ' ',
  "AAABLTXO" varchar(46) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLILW","AAABLTXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALSN" (
  "AAABHBMG" varchar(4) NOT NULL default ' ',
  "AAABFPKP" varchar(30) NOT NULL default ' ',
  "AAABALUC" varchar(30) NOT NULL default ' ',
  "AAABJLOQ" varchar(30) NOT NULL default ' ',
  "AAABALUY" varchar(30) NOT NULL default ' ',
  "AAABLFTU" varchar(30) NOT NULL default ' ',
  "AAABALVA" varchar(30) NOT NULL default ' ',
  "AAABALSV" varchar(30) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABHBMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALSY" (
  "AAABHBMG" varchar(4) NOT NULL default ' ',
  "AAABALTO" varchar(30) NOT NULL default ' ',
  "AAABALTP" varchar(30) NOT NULL default ' ',
  "AAABALTQ" varchar(30) NOT NULL default ' ',
  "AAABALTR" varchar(30) NOT NULL default ' ',
  "AAABALTS" varchar(30) NOT NULL default ' ',
  "AAABALTT" varchar(30) NOT NULL default ' ',
  "AAABALTU" varchar(30) NOT NULL default ' ',
  "AAABALTV" varchar(30) NOT NULL default ' ',
  "AAABALTW" varchar(30) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABHBMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALTL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHBMG" varchar(4) NOT NULL default ' ',
  "AAABALTO" varchar(30) NOT NULL default ' ',
  "AAABALTP" varchar(30) NOT NULL default ' ',
  "AAABALTQ" varchar(30) NOT NULL default ' ',
  "AAABALTR" varchar(30) NOT NULL default ' ',
  "AAABALTS" varchar(30) NOT NULL default ' ',
  "AAABALTT" varchar(30) NOT NULL default ' ',
  "AAABALTU" varchar(30) NOT NULL default ' ',
  "AAABALTV" varchar(30) NOT NULL default ' ',
  "AAABALTW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHBMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALTZ" (
  "AAABHBMG" varchar(4) NOT NULL default ' ',
  "AAABFPKP" varchar(30) NOT NULL default ' ',
  "AAABALUC" varchar(30) NOT NULL default ' ',
  "AAABJLOQ" varchar(30) NOT NULL default ' ',
  "AAABALUY" varchar(30) NOT NULL default ' ',
  "AAABLFTU" varchar(30) NOT NULL default ' ',
  "AAABALUG" varchar(30) NOT NULL default ' ',
  "AAABALVA" varchar(30) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABHBMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALUK" (
  "AAABHBMG" varchar(4) NOT NULL default ' ',
  "AAABFPKP" varchar(30) NOT NULL default ' ',
  "AAABJLOQ" varchar(30) NOT NULL default ' ',
  "AAABALUY" varchar(30) NOT NULL default ' ',
  "AAABKWJY" varchar(30) NOT NULL default ' ',
  "AAABALVA" varchar(30) NOT NULL default ' ',
  "AAABLFTU" varchar(30) NOT NULL default ' ',
  "AAABALUS" varchar(30) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABHBMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALUV" (
  "AAABHBMG" varchar(4) NOT NULL default ' ',
  "AAABFPKP" varchar(30) NOT NULL default ' ',
  "AAABALUY" varchar(30) NOT NULL default ' ',
  "AAABKWJY" varchar(30) NOT NULL default ' ',
  "AAABALVA" varchar(30) NOT NULL default ' ',
  "AAABLFTU" varchar(30) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABHBMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALVE" (
  "AAABALVH" varchar(10) NOT NULL default '0000000000',
  "AAABALVK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABALVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALVI" (
  "AAABALVL" varchar(10) NOT NULL default '0000000000',
  "AAABALVK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABALVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALVM" (
  "AAABLILW" varchar(8) NOT NULL default ' ',
  "AAABHEYG" varchar(70) NOT NULL default ' ',
  "AAABALVZ" varchar(70) NOT NULL default ' ',
  "AAABALWA" varchar(70) NOT NULL default ' ',
  "AAABJCAA" varchar(8) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLILW","AAABHEYG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALVV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLILW" varchar(8) NOT NULL default ' ',
  "AAABHEYG" varchar(70) NOT NULL default ' ',
  "AAABALVZ" varchar(70) NOT NULL default ' ',
  "AAABALWA" varchar(70) NOT NULL default ' ',
  "AAABJCAA" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLILW","AAABHEYG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALWF" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLILW" varchar(8) NOT NULL default ' ',
  "AAABHEYG" varchar(70) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLILW","AAABHEYG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALWN" (
  "AAABLILW" varchar(8) NOT NULL default ' ',
  "AAABALXA" varchar(15) NOT NULL default ' ',
  "AAABALWQ" varchar(20) NOT NULL default ' ',
  "AAABALWR" varchar(20) NOT NULL default ' ',
  "AAABALWS" varchar(20) NOT NULL default ' ',
  "AAABALWT" varchar(30) NOT NULL default ' ',
  "AAABALWU" varchar(30) NOT NULL default ' ',
  "AAABKWRN" varchar(1) NOT NULL default ' ',
  "AAABALWW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLILW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALWY" (
  "AAABALXA" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABALXA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALXB" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLILW" varchar(8) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLILW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALXH" (
  "AAABJCAA" varchar(8) NOT NULL default ' ',
  "AAABALXJ" varchar(1) NOT NULL default ' ',
  "AAABALXK" varchar(1) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABJCAA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALXN" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJCAA" varchar(8) NOT NULL default ' ',
  "AAABMRCK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABJCAA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALXT" (
  "AAABLILW" varchar(8) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJCAA" varchar(8) NOT NULL default ' ',
  "AAABALXX" varchar(2) NOT NULL default '00',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABJCBT" varchar(4) NOT NULL default ' ',
  "AAABALYA" varchar(30) NOT NULL default ' ',
  "AAABALYB" varchar(30) NOT NULL default ' ',
  "AAABIKFK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLILW","AAABLHPM","AAABJCAA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALYG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABJCDT" varchar(8) NOT NULL default ' ',
  "AAABAMFE" varchar(5) NOT NULL default ' ',
  "AAABGTXN" varchar(1) NOT NULL default ' ',
  "AAABALYN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHPM","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALYR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABALYY" varchar(10) NOT NULL default ' ',
  "AAABKZAW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABALYY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALYZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABALZE" varchar(4) NOT NULL default ' ',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABALZE","AAABJFHJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALZG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABALZS" varchar(2) NOT NULL default '00',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABALZM" varchar(30) NOT NULL default ' ',
  "AAABFHMV" varchar(1) NOT NULL default ' ',
  "AAABALZO" varchar(1) NOT NULL default ' ',
  "AAABMLCO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHPM","AAABALZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABALZT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAMAD" varchar(2) NOT NULL default ' ',
  "AAABMBIU" varchar(4) NOT NULL default ' ',
  "AAABALZX" varchar(2) NOT NULL default '00',
  "AAABHCEC" varchar(2) NOT NULL default '00',
  "AAABALZZ" varchar(2) NOT NULL default '00',
  "AAABAMAA" varchar(2) NOT NULL default '00',
  "AAABFWJA" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABAMAD","AAABMBIU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMAM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABAMAY" varchar(1) NOT NULL default ' ',
  "AAABAMAQ" varchar(1) NOT NULL default ' ',
  "AAABAMAR" varchar(1) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABJFHB"),
  KEY "AAABAMAW" ("AAABMRIL","AAABAMAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMAZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABAMZN" varchar(2) NOT NULL default '00',
  "AAABMNMP" varchar(2) NOT NULL default '00',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABEAAI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHPM","AAABAMZN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMBK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABAMFR" varchar(2) NOT NULL default '00',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABAMBQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHPM","AAABAMFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMBU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMCC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABAMFR" varchar(2) NOT NULL default '00',
  "AAABLPWW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLHPM","AAABAMFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMCM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMCT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAMZN" varchar(4) NOT NULL default '0000',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAMZN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMDB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAMZN" varchar(4) NOT NULL default '0000',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAMZN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMDJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAMZN" varchar(4) NOT NULL default '0000',
  "AAABLPWW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAMZN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMDR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAMZN" varchar(4) NOT NULL default '0000',
  "AAABAMDU" varchar(40) NOT NULL default ' ',
  "AAABAMDV" varchar(4) NOT NULL default ' ',
  "AAABAMDW" varchar(4) NOT NULL default ' ',
  "AAABAMDX" varchar(40) NOT NULL default ' ',
  "AAABAMDY" varchar(4) NOT NULL default ' ',
  "AAABAMDZ" varchar(4) NOT NULL default ' ',
  "AAABLTXO" varchar(46) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAMZN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMED" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAMGD" varchar(2) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAMGD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMEJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAMGD" varchar(2) NOT NULL default ' ',
  "AAABLPWW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAMGD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMER" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRBY" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMEW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRBY" varchar(12) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABJCDT" varchar(8) NOT NULL default ' ',
  "AAABAMFE" varchar(5) NOT NULL default ' ',
  "AAABHWBK" decimal(5,0) NOT NULL default '0',
  "AAABKZWQ" varchar(1) NOT NULL default ' ',
  "AAABAMFH" varchar(1) NOT NULL default ' ',
  "AAABETOU" varchar(1) NOT NULL default ' ',
  "AAABAMFJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRBY","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMFN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAMGD" varchar(2) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABAMFR" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABAMGD","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMFV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAMGD" varchar(2) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABAMZN" varchar(4) NOT NULL default '0000',
  "AAABMNMP" varchar(2) NOT NULL default '00',
  "AAABEAAI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAMGD","AAABLHPM","AAABAMZN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMGG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKRBY" varchar(12) NOT NULL default ' ',
  "AAABJBGR" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABKRBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMGO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAMHK" varchar(5) NOT NULL default '00000',
  "AAABLQVJ" varchar(5) NOT NULL default '00000',
  "AAABIGOT" decimal(5,0) NOT NULL default '0',
  "AAABAMGT" decimal(5,0) NOT NULL default '0',
  "AAABAMGU" decimal(5,0) NOT NULL default '0',
  "AAABAMGV" decimal(5,0) NOT NULL default '0',
  "AAABAMGW" decimal(5,0) NOT NULL default '0',
  "AAABAMGX" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAMHK","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMHB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAMHK" varchar(5) NOT NULL default '00000',
  "AAABLQVJ" varchar(5) NOT NULL default '00000',
  "AAABCRGY" varchar(12) NOT NULL default ' ',
  "AAABLPWW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAMHK","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMHM" (
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default ' ',
  "AAABJDFW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHPM","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMHV" (
  "AAABAMII" varchar(32) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABJHEW" varchar(4) NOT NULL default ' ',
  "AAABLPSU" varchar(30) NOT NULL default ' ',
  "AAABLRJP" varchar(8) NOT NULL default '00000000',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABAMIE" varchar(3) NOT NULL default ' ',
  "AAABBKLY" varchar(3) NOT NULL default ' ',
  "AAABHVRR" varchar(5) NOT NULL default ' ',
  "AAABAMIH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABAMII","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMIK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAMIR" varchar(8) NOT NULL default ' ',
  "AAABAMIY" varchar(1) NOT NULL default ' ',
  "AAABBHNF" varchar(1) NOT NULL default ' ',
  "AAABMCRK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAMIR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMIS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAMIY" varchar(1) NOT NULL default ' ',
  "AAABAMIV" varchar(6) NOT NULL default '000000',
  "AAABAMIW" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABAMIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMIZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOPB" varchar(2) NOT NULL default ' ',
  "AAABAMJC" varchar(1) NOT NULL default ' ',
  "AAABFWOI" varchar(2) NOT NULL default ' ',
  "AAABAMJE" varchar(8) NOT NULL default ' ',
  "AAABAMJF" varchar(1) NOT NULL default ' ',
  "AAABAMJG" varchar(30) NOT NULL default ' ',
  "AAABAMJH" varchar(1) NOT NULL default ' ',
  "AAABAMJI" varchar(1) NOT NULL default ' ',
  "AAABAMJJ" varchar(1) NOT NULL default ' ',
  "AAABAMJK" varchar(1) NOT NULL default ' ',
  "AAABAMJL" varchar(8) NOT NULL default ' ',
  "AAABAMJM" varchar(30) NOT NULL default ' ',
  "AAABAMJN" varchar(8) NOT NULL default ' ',
  "AAABAMJO" varchar(30) NOT NULL default ' ',
  "AAABAMJP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOPB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMJS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFWOI" varchar(2) NOT NULL default ' ',
  "AAABAMJV" varchar(8) NOT NULL default ' ',
  "AAABJDFT" varchar(1) NOT NULL default ' ',
  "AAABJDFU" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFWOI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMKA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOPB" varchar(2) NOT NULL default ' ',
  "AAABAMKD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOPB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDSB" varchar(10) NOT NULL default ' ',
  "AAABAMPO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDSB","AAABAMPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMKN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAMPO" varchar(10) NOT NULL default ' ',
  "AAABAMKQ" varchar(2) NOT NULL default '00',
  "AAABAMKR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAMPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMKU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAMNH" varchar(10) NOT NULL default ' ',
  "AAABAMNI" varchar(3) NOT NULL default '000',
  "AAABAMNJ" varchar(3) NOT NULL default '000',
  "AAABKIFQ" varchar(8) NOT NULL default '00000000',
  "AAABJZKD" varchar(8) NOT NULL default '00000000',
  "AAABAMLB" varchar(12) NOT NULL default ' ',
  "AAABMPMH" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLLJR" varchar(10) NOT NULL default ' ',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABAMNM" varchar(2) NOT NULL default ' ',
  "AAABAMNN" varchar(8) NOT NULL default '00000000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJDGC" varchar(45) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABAMLL" varchar(8) NOT NULL default ' ',
  "AAABFYUL" decimal(16,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABMKAH" decimal(15,3) NOT NULL default '0.000',
  "AAABMKAI" varchar(3) NOT NULL default ' ',
  "AAABAMLQ" decimal(16,2) NOT NULL default '0.00',
  "AAABAMLR" decimal(16,2) NOT NULL default '0.00',
  "AAABEPUS" varchar(1) NOT NULL default ' ',
  "AAABAMLT" varchar(3) NOT NULL default ' ',
  "AAABAMLU" varchar(6) NOT NULL default ' ',
  "AAABAMLV" varchar(10) NOT NULL default ' ',
  "AAABAMLW" varchar(4) NOT NULL default ' ',
  "AAABAMLX" varchar(10) NOT NULL default ' ',
  "AAABAMLY" varchar(10) NOT NULL default ' ',
  "AAABKUGN" varchar(12) NOT NULL default ' ',
  "AAABAMMA" varchar(10) NOT NULL default ' ',
  "AAABAMMB" varchar(6) NOT NULL default '000000',
  "AAABAMMC" varchar(24) NOT NULL default ' ',
  "AAABAMMD" varchar(10) NOT NULL default ' ',
  "AAABAMME" varchar(10) NOT NULL default ' ',
  "AAABMJXA" varchar(10) NOT NULL default ' ',
  "AAABLEZR" varchar(10) NOT NULL default ' ',
  "AAABAMMH" varchar(35) NOT NULL default ' ',
  "AAABAMMI" varchar(8) NOT NULL default '00000000',
  "AAABMKFU" varchar(6) NOT NULL default '000000',
  "AAABAMMK" varchar(10) NOT NULL default ' ',
  "AAABIKEM" decimal(5,2) NOT NULL default '0.00',
  "AAABAMMM" varchar(5) NOT NULL default ' ',
  "AAABAMMN" varchar(1) NOT NULL default ' ',
  "AAABBRNJ" varchar(4) NOT NULL default ' ',
  "AAABAMMP" varchar(25) NOT NULL default ' ',
  "AAABAMMQ" varchar(8) NOT NULL default '00000000',
  "AAABAMMR" varchar(40) NOT NULL default ' ',
  "AAABAMMS" varchar(1) NOT NULL default ' ',
  "AAABAMMT" varchar(8) NOT NULL default '00000000',
  "AAABAMMU" varchar(6) NOT NULL default '000000',
  "AAABAMMV" varchar(1) NOT NULL default ' ',
  "AAABAMMW" varchar(10) NOT NULL default ' ',
  "AAABAMMX" varchar(3) NOT NULL default '000',
  "AAABAMMY" varchar(15) NOT NULL default ' ',
  "AAABAMMZ" varchar(1) NOT NULL default ' ',
  "AAABAMNA" varchar(8) NOT NULL default '00000000',
  "AAABANFZ" decimal(3,0) NOT NULL default '0',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABMCZQ" varchar(16) NOT NULL default ' ',
  "AAABKRUO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAMNH","AAABAMNI","AAABAMNJ"),
  KEY "AAABAMNK" ("AAABJFHJ","AAABAMNM","AAABAMNN"),
  KEY "AAABAMNO" ("AAABMPMH","AAABLLJR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMNR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSKN" varchar(12) NOT NULL default ' ',
  "AAABAMYT" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSKN","AAABAMYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMNY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSKN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSKN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMOD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSKN" varchar(12) NOT NULL default ' ',
  "AAABLPUZ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSKN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMOL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLVCU" varchar(10) NOT NULL default ' ',
  "AAABAMOO" varchar(4) NOT NULL default ' ',
  "AAABIFOL" varchar(30) NOT NULL default ' ',
  "AAABLTQJ" varchar(1) NOT NULL default '0',
  "AAABGCIV" varchar(1) NOT NULL default ' ',
  "AAABGCIW" varchar(1) NOT NULL default ' ',
  "AAABMNRG" varchar(1) NOT NULL default '0',
  "AAABAMXC" varchar(1) NOT NULL default ' ',
  "AAABAMXD" varchar(1) NOT NULL default ' ',
  "AAABAMXE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLVCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMOZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLVCU" varchar(10) NOT NULL default ' ',
  "AAABAMPD" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLVCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAMPO" varchar(10) NOT NULL default ' ',
  "AAABAMPL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAMPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMPP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNO" varchar(5) NOT NULL default ' ',
  "AAABBIKQ" varchar(5) NOT NULL default ' ',
  "AAABBIKG" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNO","AAABBIKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMPX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEBIH" varchar(4) NOT NULL default ' ',
  "AAABHZCZ" varchar(4) NOT NULL default ' ',
  "AAABLGEL" varchar(1) NOT NULL default ' ',
  "AAABHFFQ" varchar(30) NOT NULL default ' ',
  "AAABBGYT" varchar(20) NOT NULL default ' ',
  "AAABAMRC" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEBIH","AAABHZCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMQI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGYT" varchar(20) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABAMQM" varchar(40) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABMJWN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBGYT","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMQS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAMRC" varchar(20) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABAMQW" varchar(132) NOT NULL default ' ',
  "AAABAMQX" varchar(1) NOT NULL default ' ',
  "AAABAMQY" varchar(1) NOT NULL default ' ',
  "AAABAMQZ" varchar(1) NOT NULL default ' ',
  "AAABAMRA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAMRC","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMRE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABILLA" varchar(8) NOT NULL default ' ',
  "AAABDWNR" varchar(4) NOT NULL default ' ',
  "AAABAMRI" varchar(4) NOT NULL default ' ',
  "AAABAMRJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABILLA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMRM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABILLA" varchar(8) NOT NULL default ' ',
  "AAABFLNM" varchar(8) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABAMRR" varchar(1) NOT NULL default ' ',
  "AAABAMRS" varchar(12) NOT NULL default ' ',
  "AAABAMRT" varchar(4) NOT NULL default ' ',
  "AAABAMRU" varchar(20) NOT NULL default ' ',
  "AAABAMRV" varchar(8) NOT NULL default ' ',
  "AAABEBIH" varchar(4) NOT NULL default ' ',
  "AAABHZCZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABILLA","AAABFLNM","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMSC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABKLUK" varchar(3) NOT NULL default '000',
  "AAABMCNO" varchar(5) NOT NULL default ' ',
  "AAABBIKQ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP","AAABKLUK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMSL" (
  "AAABLILW" varchar(4) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABIKVA" varchar(12) NOT NULL default ' ',
  "AAABFZAB" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABLILW","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMSS" (
  "AAABLILW" varchar(4) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABIKVA" varchar(12) NOT NULL default ' ',
  "AAABFZAB" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABLILW","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMSZ" (
  "AAABLILW" varchar(4) NOT NULL default ' ',
  "AAABJCDT" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABLILW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMTD" (
  "AAABLILW" varchar(4) NOT NULL default ' ',
  "AAABJCDT" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABLILW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMTH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABANBG" varchar(8) NOT NULL default '00000000',
  "AAABAMTK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABANBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMTN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABANBG" varchar(8) NOT NULL default '00000000',
  "AAABAMTQ" decimal(15,3) NOT NULL default '0.000',
  "AAABAMTR" decimal(15,3) NOT NULL default '0.000',
  "AAABAMTS" decimal(12,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABANBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJDFT" varchar(1) NOT NULL default ' ',
  "AAABJDFU" varchar(3) NOT NULL default ' ',
  "AAABJDGB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMUD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABANBG" varchar(8) NOT NULL default '00000000',
  "AAABLAQG" varchar(4) NOT NULL default '0000',
  "AAABAMUH" varchar(20) NOT NULL default ' ',
  "AAABAMUI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABANBG","AAABLAQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMUM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDSB" varchar(10) NOT NULL default ' ',
  "AAABAMVP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDSB","AAABAMVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMUT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBDSB" varchar(10) NOT NULL default ' ',
  "AAABAMVP" varchar(10) NOT NULL default ' ',
  "AAABAMUY" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBDSB","AAABAMVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMVD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDSB" varchar(10) NOT NULL default ' ',
  "AAABAMVP" varchar(10) NOT NULL default ' ',
  "AAABAXDZ" varchar(3) NOT NULL default '000',
  "AAABLUVE" varchar(3) NOT NULL default '000',
  "AAABBOPO" varchar(4) NOT NULL default ' ',
  "AAABCUNV" varchar(6) NOT NULL default '000000',
  "AAABCUNW" varchar(6) NOT NULL default '000000',
  "AAABAMVM" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDSB","AAABAMVP","AAABAXDZ","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMVS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDSL" varchar(10) NOT NULL default ' ',
  "AAABLEUD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDSL","AAABLEUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMVZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABANBG" varchar(8) NOT NULL default '00000000',
  "AAABLAQG" varchar(4) NOT NULL default '0000',
  "AAABAMWT" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABANBG","AAABLAQG","AAABAMWT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMWI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABANBG" varchar(8) NOT NULL default '00000000',
  "AAABLAQG" varchar(4) NOT NULL default '0000',
  "AAABAMWT" varchar(6) NOT NULL default '000000',
  "AAABAMWO" varchar(79) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABANBG","AAABLAQG","AAABAMWT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMWU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDSL" varchar(10) NOT NULL default ' ',
  "AAABAMXH" varchar(10) NOT NULL default ' ',
  "AAABLTQJ" varchar(1) NOT NULL default '0',
  "AAABGCIV" varchar(1) NOT NULL default ' ',
  "AAABGCIW" varchar(1) NOT NULL default ' ',
  "AAABMNRG" varchar(1) NOT NULL default '0',
  "AAABAMXC" varchar(1) NOT NULL default ' ',
  "AAABAMXD" varchar(1) NOT NULL default ' ',
  "AAABAMXE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDSL","AAABAMXH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMXI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMCNO" varchar(5) NOT NULL default ' ',
  "AAABBIKQ" varchar(5) NOT NULL default ' ',
  "AAABHMSB" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMCNO","AAABBIKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMXS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABHHRZ" varchar(4) NOT NULL default ' ',
  "AAABLVJB" varchar(1) NOT NULL default ' ',
  "AAABAMXZ" varchar(1) NOT NULL default ' ',
  "AAABAMYA" varchar(1) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABLHPM","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMYH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAMYT" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAMYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMYM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAMYT" varchar(12) NOT NULL default ' ',
  "AAABILRJ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAMYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMYU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAMZC" varchar(8) NOT NULL default '00000000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAMYY" varchar(40) NOT NULL default ' ',
  "AAABAMYZ" varchar(40) NOT NULL default ' ',
  "AAABAMZA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAMZC","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMZE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABAMZN" varchar(2) NOT NULL default '00',
  "AAABLPWW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLHPM","AAABAMZN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMZO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLPWW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAMZW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABANBG" varchar(8) NOT NULL default '00000000',
  "AAABANAA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABANBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANAE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABANBG" varchar(8) NOT NULL default '00000000',
  "AAABLAQG" varchar(4) NOT NULL default '0000',
  "AAABANAJ" varchar(40) NOT NULL default ' ',
  "AAABANAK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABANBG","AAABLAQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANAP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHPM","AAABJFGL","AAABJFHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANAY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABANBG" varchar(8) NOT NULL default '00000000',
  "AAABLPWW" varchar(40) NOT NULL default ' ',
  "AAABANBD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABANBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANBH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABHHRZ" varchar(4) NOT NULL default ' ',
  "AAABIMUR" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLUVS","AAABLHPM","AAABHHRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANBT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABKRBY" varchar(12) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABHGAM" varchar(1) NOT NULL default ' ',
  "AAABIKFK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABKRBY","AAABKFCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANCF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABANCI" varchar(4) NOT NULL default '0000',
  "AAABANCJ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANCM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDFT" varchar(1) NOT NULL default ' ',
  "AAABJDFU" varchar(3) NOT NULL default ' ',
  "AAABANCQ" varchar(1) NOT NULL default ' ',
  "AAABANCR" varchar(1) NOT NULL default ' ',
  "AAABANCS" varchar(1) NOT NULL default ' ',
  "AAABANCT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDFT","AAABJDFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANCX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEIIS" varchar(16) NOT NULL default ' ',
  "AAABBQOI" varchar(8) NOT NULL default ' ',
  "AAABFNHW" varchar(30) NOT NULL default ' ',
  "AAABIJSB" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABEIIS","AAABBQOI","AAABFNHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANDH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLDNL" varchar(30) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABDTRC" varchar(40) NOT NULL default ' ',
  "AAABMNYU" varchar(30) NOT NULL default ' ',
  "AAABEGDU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLDNL","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANDR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABANDY" varchar(8) NOT NULL default ' ',
  "AAABANER" varchar(8) NOT NULL default ' ',
  "AAABEIIS" varchar(16) NOT NULL default ' ',
  "AAABANDW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABANDY","AAABANER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANEA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABANER" varchar(8) NOT NULL default ' ',
  "AAABANED" varchar(4) NOT NULL default ' ',
  "AAABANFF" varchar(4) NOT NULL default '0000',
  "AAABHUCO" varchar(1) NOT NULL default ' ',
  "AAABEGDU" varchar(1) NOT NULL default ' ',
  "AAABANEH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABANER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANEK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABANER" varchar(8) NOT NULL default ' ',
  "AAABANEO" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABANER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANES" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABANFF" varchar(4) NOT NULL default '0000',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABANFF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANEY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABANFF" varchar(4) NOT NULL default '0000',
  "AAABANFC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABANFF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANFG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABANFV" varchar(4) NOT NULL default ' ',
  "AAABANGD" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABANFV","AAABANGD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANFN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABJDGB" varchar(2) NOT NULL default ' ',
  "AAABJDGC" varchar(45) NOT NULL default ' ',
  "AAABEGFM" varchar(6) NOT NULL default '000000',
  "AAABANFV" varchar(4) NOT NULL default ' ',
  "AAABJFHC" varchar(1) NOT NULL default ' ',
  "AAABJDFW" varchar(2) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABANFZ" decimal(3,0) NOT NULL default '0',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABANGD" varchar(8) NOT NULL default ' ',
  "AAABEIIS" varchar(16) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABANGG" varchar(1) NOT NULL default '0',
  "AAABANGH" varchar(1) NOT NULL default ' ',
  "AAABANGI" varchar(1) NOT NULL default ' ',
  "AAABANGJ" varchar(1) NOT NULL default ' ',
  "AAABANGZ" varchar(2) NOT NULL default ' ',
  "AAABANGY" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHJ","AAABLHPM","AAABLUNJ","AAABJDGB","AAABJDGC","AAABEGFM"),
  KEY "AAABANGT" ("AAABJDGC","AAABJDGB","AAABJFHJ"),
  KEY "AAABANGX" ("AAABANGY","AAABANGZ","AAABJFHJ"),
  KEY "AAABANHB" ("AAABMDEN","AAABMRFQ","AAABJFHJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANHF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABANHL" varchar(4) NOT NULL default ' ',
  "AAABBGDI" varchar(1) NOT NULL default ' ',
  "AAABBGDJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABANHL","AAABBGDI","AAABBGDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANHO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGDI" varchar(1) NOT NULL default ' ',
  "AAABBGDJ" varchar(2) NOT NULL default ' ',
  "AAABANHS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBGDI","AAABBGDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANHW" (
  "AAABLGEF" varchar(8) NOT NULL default '00000000',
  "AAABLEHK" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEF","AAABLEHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANIB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABANKZ" varchar(4) NOT NULL default ' ',
  "AAABANMG" varchar(4) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABDCRX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABANKZ","AAABANMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANIK" (
  "AAABANMG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABANMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANIN" (
  "AAABANMG" varchar(4) NOT NULL default ' ',
  "AAABLGEF" varchar(8) NOT NULL default '00000000',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABANMG","AAABLGEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANIT" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABANMG" varchar(4) NOT NULL default ' ',
  "AAABLPSU" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABANMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANIZ" (
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABJFGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANJC" (
  "AAABLTQJ" varchar(8) NOT NULL default '00000000',
  "AAABLVJB" varchar(7) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANJG" (
  "AAABANKZ" varchar(4) NOT NULL default ' ',
  "AAABLGEF" varchar(8) NOT NULL default '00000000',
  "AAABLTQJ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABANKZ","AAABLGEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANJM" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLTQJ" varchar(8) NOT NULL default '00000000',
  "AAABMRCK" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLTQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANJS" (
  "AAABANKZ" varchar(4) NOT NULL default ' ',
  "AAABANJU" varchar(8) NOT NULL default '00000000',
  "AAABANJV" varchar(30) NOT NULL default ' ',
  "AAABANJW" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABANKZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANJY" (
  "AAABANKZ" varchar(4) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABLGEF" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABANKZ","AAABMCRV","AAABLGEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANKF" (
  "AAABLUSV" varchar(4) NOT NULL default '0000',
  "AAABMLPV" varchar(20) NOT NULL default ' ',
  "AAABMMYV" varchar(20) NOT NULL default ' ',
  "AAABANKJ" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANKL" (
  "AAABANKZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABANKZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANKO" (
  "AAABANKZ" varchar(4) NOT NULL default ' ',
  "AAABANMG" varchar(4) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABANKZ","AAABANMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANKU" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABANKZ" varchar(4) NOT NULL default ' ',
  "AAABLPSU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABANKZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANLA" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABANLD" varchar(4) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABANLF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANLH" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABANLO" varchar(2) NOT NULL default '00',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABANLM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP","AAABANLO","AAABHVYP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANLQ" (
  "AAABLGEF" varchar(8) NOT NULL default '00000000',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLUZL" varchar(30) NOT NULL default ' ',
  "AAABMKXW" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEF","AAABMOHM","AAABLUZL","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANLZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABANMG" varchar(4) NOT NULL default ' ',
  "AAABLGEF" varchar(8) NOT NULL default '00000000',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABDCRX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABANMG","AAABLGEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANMI" (
  "AAABANMM" varchar(8) NOT NULL default '00000000',
  "AAABANMN" varchar(8) NOT NULL default '00000000',
  "AAABANML" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABANMM","AAABANMN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANMO" (
  "AAABLGEF" varchar(8) NOT NULL default '00000000',
  "AAABBIWC" varchar(1) NOT NULL default ' ',
  "AAABIYJF" varchar(200) NOT NULL default ' ',
  "AAABMJWN" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANMU" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLGEF" varchar(8) NOT NULL default '00000000',
  "AAABMLPV" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLGEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANNA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABANNN" varchar(24) NOT NULL default ' ',
  "AAABFTXT" varchar(20) NOT NULL default ' ',
  "AAABANNF" varchar(14) NOT NULL default ' ',
  "AAABANNG" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABHVYP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANNJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABANOT" varchar(5) NOT NULL default ' ',
  "AAABANNN" varchar(24) NOT NULL default ' ',
  "AAABANNO" varchar(1) NOT NULL default ' ',
  "AAABANNP" varchar(1) NOT NULL default ' ',
  "AAABFTXT" varchar(20) NOT NULL default ' ',
  "AAABANNR" varchar(40) NOT NULL default ' ',
  "AAABFZAB" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABHVYP","AAABANOT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANNW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABANOT" varchar(5) NOT NULL default ' ',
  "AAABANOU" varchar(1) NOT NULL default ' ',
  "AAABANOV" varchar(3) NOT NULL default '000',
  "AAABMNMP" varchar(1) NOT NULL default '0',
  "AAABLENL" varchar(2) NOT NULL default ' ',
  "AAABANOE" varchar(30) NOT NULL default ' ',
  "AAABMLPV" varchar(70) NOT NULL default ' ',
  "AAABLWJT" varchar(60) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABANOI" varchar(3) NOT NULL default ' ',
  "AAABMNMM" varchar(2) NOT NULL default ' ',
  "AAABANOK" varchar(1) NOT NULL default ' ',
  "AAABLBEI" varchar(3) NOT NULL default ' ',
  "AAABANOM" varchar(70) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABJHEW" varchar(4) NOT NULL default ' ',
  "AAABLPSU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABHVYP","AAABANOT","AAABANOU","AAABANOV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANOW" (
  "AAABEJQB" varchar(12) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABANOZ" varchar(6) NOT NULL default '000000',
  "AAABANPA" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABEJQB","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANPD" (
  "AAABEJQB" varchar(12) NOT NULL default ' ',
  "AAABANPF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEJQB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABJDGB" varchar(2) NOT NULL default ' ',
  "AAABANSD" varchar(1) NOT NULL default ' ',
  "AAABEJQB" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHPM","AAABJFGL","AAABJFHB","AAABJDGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANPU" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEJQB" varchar(12) NOT NULL default ' ',
  "AAABANPX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABEJQB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANQA" (
  "AAABANQK" varchar(4) NOT NULL default ' ',
  "AAABANQL" varchar(4) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJBGU" varchar(4) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABLTIZ" varchar(6) NOT NULL default '000000',
  "AAABHLQU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABANQK","AAABANQL","AAABMMVH","AAABMKFH","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANQP" (
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABANQU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHPM","AAABJFGL","AAABJFHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANQY" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABHIOX" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANRE" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABAZVX" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABJFGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANRK" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABEDNV" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABJFGL","AAABJFHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANRS" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJDFU" varchar(3) NOT NULL default ' ',
  "AAABLPUZ" varchar(25) NOT NULL default ' ',
  "AAABANRW" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABJDFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANRZ" (
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABJDFU" varchar(3) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABANSD" varchar(1) NOT NULL default ' ',
  "AAABANSE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJFHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANSG" (
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANSJ" (
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABJFGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANSM" (
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABJFGL","AAABJFHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANSR" (
  "AAABJDFU" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABJDFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANSU" (
  "AAABANTL" varchar(3) NOT NULL default '000',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABMRIL" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABANTL"),
  KEY "AAABANSZ" ("AAABLENH","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANTC" (
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABANTL" varchar(3) NOT NULL default '000',
  "AAABANTM" varchar(5) NOT NULL default '00000',
  "AAABANTH" varchar(5) NOT NULL default '00000',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLENH","AAABLUXQ","AAABANTL","AAABANTM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANTN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABMDBL" varchar(1) NOT NULL default ' ',
  "AAABMNLX" varchar(2) NOT NULL default ' ',
  "AAABMNLY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANTW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABANVM" varchar(1) NOT NULL default ' ',
  "AAABANVG" varchar(1) NOT NULL default ' ',
  "AAABIEWH" varchar(2) NOT NULL default ' ',
  "AAABHYJN" varchar(2) NOT NULL default ' ',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABLKXQ" varchar(4) NOT NULL default ' ',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABANUH" varchar(1) NOT NULL default ' ',
  "AAABANUI" varchar(1) NOT NULL default ' ',
  "AAABANUJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABANVM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANUM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABANVM" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABANVM","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANUU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABANVG" varchar(1) NOT NULL default ' ',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABANUY" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABANVG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANVB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABANVG" varchar(1) NOT NULL default ' ',
  "AAABGPFI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABANVG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANVH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABANVK" varchar(1) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABANVM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANVP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABANVV" varchar(30) NOT NULL default ' ',
  "AAABANVS" varchar(1) NOT NULL default ' ',
  "AAABANVT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABANVV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANVW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABANZZ" varchar(4) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABBHCH" varchar(30) NOT NULL default ' ',
  "AAABANWD" varchar(5) NOT NULL default ' ',
  "AAABANWE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABANZZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANWI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABANZZ" varchar(4) NOT NULL default ' ',
  "AAABMAYK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABANZZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANWS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABANYW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABANYW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANWZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABANYW" varchar(4) NOT NULL default ' ',
  "AAABBENX" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABANYW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANXJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHVXC" varchar(2) NOT NULL default ' ',
  "AAABBHFX" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABANYW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABHVXC","AAABBHFX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANXW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABANZY" varchar(4) NOT NULL default ' ',
  "AAABANZZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABANZY","AAABANZZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABANZY" varchar(4) NOT NULL default ' ',
  "AAABANYN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABANZY","AAABANYN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANYO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABANZY" varchar(4) NOT NULL default ' ',
  "AAABANYW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABANZY","AAABANYW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANYX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABANZY" varchar(4) NOT NULL default ' ',
  "AAABANZB" tinyint(3) unsigned NOT NULL default '0',
  "AAABANZC" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABANZY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANZG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABANZY" varchar(4) NOT NULL default ' ',
  "AAABBAJI" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABANZY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABANZQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABANZY" varchar(4) NOT NULL default ' ',
  "AAABANZZ" varchar(4) NOT NULL default ' ',
  "AAABAOAW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABANZY","AAABANZZ","AAABAOAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOAB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAOAW" varchar(4) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABAOAG" varchar(1) NOT NULL default ' ',
  "AAABAOAH" varchar(8) NOT NULL default '00000000',
  "AAABAOAI" varchar(8) NOT NULL default '00000000',
  "AAABBHCH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAOAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAOAW" varchar(4) NOT NULL default ' ',
  "AAABAOAS" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABAOAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOAX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAOBG" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAOBD" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAOBG","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOBI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAOBO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAOBO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOBP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAODA" varchar(4) NOT NULL default ' ',
  "AAABAOBU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABAODA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOBZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABASGJ" varchar(4) NOT NULL default ' ',
  "AAABAODA" varchar(4) NOT NULL default ' ',
  "AAABAOCS" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMALF" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABASGJ","AAABAODA","AAABAOCS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOCO" (
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAOCS" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABHVYP","AAABAOCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOCT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABGCHE" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAODA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGCHD","AAABGCHE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAODG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAOHB" varchar(4) NOT NULL default ' ',
  "AAABJTTT" varchar(4) NOT NULL default ' ',
  "AAABAOJE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAOHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAODP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABJTTT" varchar(4) NOT NULL default ' ',
  "AAABARDX" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAOGD" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABJTTT","AAABARDX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOEC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABARDX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABARDX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOEJ" (
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAOGD" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABHVYP","AAABAOGD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOEO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABARDX" varchar(4) NOT NULL default ' ',
  "AAABAOGD" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABARDX","AAABAOGD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOEX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABARDX" varchar(4) NOT NULL default ' ',
  "AAABAOFH" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABARDX","AAABAOFH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOFJ" (
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABARDX" varchar(4) NOT NULL default ' ',
  "AAABAOGD" varchar(2) NOT NULL default '00',
  "AAABAOGE" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABHVYP","AAABARDX","AAABAOGD","AAABAOGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOFS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABARDX" varchar(4) NOT NULL default ' ',
  "AAABAOGD" varchar(2) NOT NULL default '00',
  "AAABAOGE" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABARDX","AAABAOGD","AAABAOGE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOGG" (
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAOIT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABHVYP","AAABAOIT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOGL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAOJE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAOJE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOGS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAOHB" varchar(4) NOT NULL default ' ',
  "AAABAOGX" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABAOHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOHC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABARDX" varchar(4) NOT NULL default ' ',
  "AAABAOHH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABARDX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOHM" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAOIT" varchar(4) NOT NULL default ' ',
  "AAABAOHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABHVYP","AAABAOIT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOHU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAOIT" varchar(4) NOT NULL default ' ',
  "AAABAOIE" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAOIT","AAABAOIE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOIG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAOJE" varchar(4) NOT NULL default ' ',
  "AAABAOIS" varchar(2) NOT NULL default '00',
  "AAABAOIT" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABIFTP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAOJE","AAABAOIS","AAABAOIT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOIV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAOJE" varchar(4) NOT NULL default ' ',
  "AAABAOJA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABAOJE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOJF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABJTTT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABJTTT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOJM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABJTTT" varchar(4) NOT NULL default ' ',
  "AAABBMWL" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABJTTT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOJW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJTTQ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJTTQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOKB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJTTQ" varchar(6) NOT NULL default ' ',
  "AAABAOMC" varchar(7) NOT NULL default ' ',
  "AAABAOKG" varchar(8) NOT NULL default '00000000',
  "AAABAOKH" varchar(4) NOT NULL default '0000',
  "AAABAOKI" varchar(2) NOT NULL default '00',
  "AAABILXD" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABJTTQ" varchar(6) NOT NULL default ' ',
  "AAABILTS" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABJTTQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOKU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEDEM" varchar(5) NOT NULL default ' ',
  "AAABEIXC" varchar(11) NOT NULL default ' ',
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEDEM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOLB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABEDEM" varchar(5) NOT NULL default ' ',
  "AAABAOLH" decimal(13,2) NOT NULL default '0.00',
  "AAABAOPI" decimal(8,5) NOT NULL default '0.00000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABAOLK" varchar(1) NOT NULL default ' ',
  "AAABAOLL" varchar(1) NOT NULL default ' ',
  "AAABAOLM" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOLQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAOMC" varchar(7) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAOMC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOLV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAOMC" varchar(7) NOT NULL default ' ',
  "AAABAOLZ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAOMC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOMD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABAOMM" varchar(4) NOT NULL default ' ',
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  "AAABAOMI" varchar(30) NOT NULL default ' ',
  "AAABAPIB" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABAOMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOMN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAOMU" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAOMS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAOMU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOMW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABAONA" varchar(1) NOT NULL default ' ',
  "AAABAONB" varchar(1) NOT NULL default ' ',
  "AAABAOZP" varchar(2) NOT NULL default ' ',
  "AAABAOND" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFGL","AAABJFHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAONH" (
  "AAABAPEQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABAPEQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAONK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKOTX" varchar(2) NOT NULL default ' ',
  "AAABAOYT" varchar(4) NOT NULL default '0000',
  "AAABAONO" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABKOTX","AAABAOYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAONS" (
  "AAABAOOD" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABAOOD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAONV" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAOOD" varchar(3) NOT NULL default '000',
  "AAABAONY" varchar(65) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABAOOD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOOB" (
  "AAABAPLG" varchar(2) NOT NULL default '00',
  "AAABAOOD" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABAPLG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOOF" (
  "AAABAOWD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABAOWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOOI" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAOWD" varchar(4) NOT NULL default ' ',
  "AAABAOOL" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABAOWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOOO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAOPD" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAOPD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOOW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAOPD" varchar(4) NOT NULL default ' ',
  "AAABAOPA" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAOPD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOPE" (
  "AAABAPEQ" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAOPI" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABAPEQ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOPL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAOPS" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAPLG" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABAOPS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOPU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGDI" varchar(1) NOT NULL default ' ',
  "AAABBGDJ" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAOTJ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABBGDI","AAABBGDJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOQF" (
  "AAABAPLG" varchar(2) NOT NULL default '00',
  "AAABAPFB" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAOQT" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABAPLG","AAABAPFB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOQO" (
  "AAABAOTJ" varchar(2) NOT NULL default '00',
  "AAABAPFB" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAOQT" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABAOTJ","AAABAPFB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOQX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAQKD" varchar(6) NOT NULL default ' ',
  "AAABAORA" varchar(1) NOT NULL default ' ',
  "AAABAORB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAQKD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAORE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAQKD" varchar(6) NOT NULL default ' ',
  "AAABAORI" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAQKD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAORM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAORY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAORY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAORR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAORY" varchar(4) NOT NULL default ' ',
  "AAABAORV" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAORY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAORZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAOSL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAOSL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOSE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAOSL" varchar(2) NOT NULL default ' ',
  "AAABAOSI" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAOSL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOSM" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAPEQ" varchar(2) NOT NULL default ' ',
  "AAABAOSP" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABAPEQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOSS" (
  "AAABAPLG" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABAPLG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOSV" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAPLG" varchar(2) NOT NULL default '00',
  "AAABILRJ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABAPLG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOTB" (
  "AAABAOTJ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABAOTJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOTE" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAOTJ" varchar(2) NOT NULL default '00',
  "AAABAOTH" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABAOTJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOTK" (
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAOTO" decimal(13,2) NOT NULL default '0.00',
  "AAABAOTP" decimal(13,2) NOT NULL default '0.00',
  "AAABAOTQ" decimal(5,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABKFCZ","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOTU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABEUXJ" varchar(1) NOT NULL default ' ',
  "AAABAOUB" varchar(1) NOT NULL default ' ',
  "AAABAOUC" varchar(1) NOT NULL default ' ',
  "AAABAPIA" varchar(2) NOT NULL default '00',
  "AAABDPCB" varchar(1) NOT NULL default '0',
  "AAABAOUF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHD","AAABJFHB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOUK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAQKD" varchar(6) NOT NULL default ' ',
  "AAABGLBT" varchar(10) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAQKD","AAABGLBT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOUU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAOVS" varchar(10) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABAPLR" varchar(15) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAOVS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOVD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAOVS" varchar(10) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAOVS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOVL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAOVS" varchar(10) NOT NULL default ' ',
  "AAABAOVP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAOVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOVT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAPEQ" varchar(2) NOT NULL default ' ',
  "AAABAPDR" varchar(2) NOT NULL default ' ',
  "AAABAOWB" varchar(1) NOT NULL default ' ',
  "AAABAOWC" varchar(2) NOT NULL default '00',
  "AAABAOWD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOWI" (
  "AAABAOXO" varchar(2) NOT NULL default '00',
  "AAABAOWK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABAOXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOWM" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAOXO" varchar(2) NOT NULL default '00',
  "AAABAOWP" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABAOXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOWS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAOXO" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOXD" (
  "AAABAOYT" varchar(4) NOT NULL default '0000',
  "AAABAOYU" varchar(2) NOT NULL default '00',
  "AAABAOXO" varchar(2) NOT NULL default '00',
  "AAABAOXH" varchar(4) NOT NULL default ' ',
  "AAABAOXI" decimal(5,2) NOT NULL default '0.00',
  "AAABAOXJ" decimal(13,2) NOT NULL default '0.00',
  "AAABAOXK" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABAOYT","AAABAOYU","AAABAOXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOXP" (
  "AAABAOYT" varchar(4) NOT NULL default '0000',
  "AAABAOYU" varchar(2) NOT NULL default '00',
  "AAABLUVE" varchar(3) NOT NULL default '000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABAOXU" decimal(13,2) NOT NULL default '0.00',
  "AAABAOXV" decimal(13,2) NOT NULL default '0.00',
  "AAABAOXW" decimal(13,2) NOT NULL default '0.00',
  "AAABAOXX" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABAOYT","AAABAOYU","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOYB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABEIXC" varchar(11) NOT NULL default ' ',
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOYL" (
  "AAABAOYT" varchar(4) NOT NULL default '0000',
  "AAABAOYU" varchar(2) NOT NULL default '00',
  "AAABLUVE" varchar(3) NOT NULL default '000',
  "AAABAOYP" decimal(13,2) NOT NULL default '0.00',
  "AAABAOYQ" decimal(13,2) NOT NULL default '0.00',
  "AAABAOYR" decimal(7,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABAOYT","AAABAOYU","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOYW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABAPLR" varchar(15) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABAPLR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOZI" (
  "AAABAPDR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABAPDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOZL" (
  "AAABAPAG" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAOZP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABAPAG","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAOZS" (
  "AAABAPAG" varchar(2) NOT NULL default ' ',
  "AAABAPDR" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABLUVE" varchar(3) NOT NULL default '000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAOZY" varchar(1) NOT NULL default ' ',
  "AAABAOZZ" decimal(13,2) NOT NULL default '0.00',
  "AAABAPAA" decimal(13,2) NOT NULL default '0.00',
  "AAABAPAB" varchar(4) NOT NULL default ' ',
  "AAABAPAC" varchar(4) NOT NULL default ' ',
  "AAABAPAD" varchar(4) NOT NULL default ' ',
  "AAABAPAE" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABAPAG","AAABAPDR","AAABKFCZ","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPAK" (
  "AAABAPEZ" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABAPEZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPAQ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAPEZ" varchar(2) NOT NULL default '00',
  "AAABAPAT" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABAPEZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPAW" (
  "AAABAPBB" varchar(5) NOT NULL default ' ',
  "AAABAPBC" varchar(5) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAPBA" decimal(25,0) NOT NULL default '0',
  PRIMARY KEY ("AAABAPBB","AAABAPBC","AAABKFCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPBE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAABGLBV" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAPBN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABBEOP","AAABGLBU","AAABGLBV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPBV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAPCH" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAPCH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPCA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAPCH" varchar(3) NOT NULL default ' ',
  "AAABAPCE" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAPCH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPCI" (
  "AAABAPCP" varchar(4) NOT NULL default '0000',
  "AAABLUVE" varchar(3) NOT NULL default ' ',
  "AAABBFMJ" varchar(4) NOT NULL default ' ',
  "AAABMORO" varchar(4) NOT NULL default ' ',
  "AAABMORP" varchar(4) NOT NULL default ' ',
  "AAABMORQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABAPCP","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPCR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHT" varchar(1) NOT NULL default ' ',
  "AAABGCHS" varchar(2) NOT NULL default '00',
  "AAABGCHU" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAPWT" varchar(4) NOT NULL default ' ',
  "AAABAPWU" varchar(4) NOT NULL default ' ',
  "AAABAPWV" varchar(4) NOT NULL default ' ',
  "AAABAPDC" varchar(4) NOT NULL default ' ',
  "AAABAPDD" varchar(4) NOT NULL default ' ',
  "AAABAPDE" varchar(4) NOT NULL default ' ',
  "AAABAPDF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHT","AAABGCHS","AAABGCHU","AAABKFCZ","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPDM" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAPDR" varchar(2) NOT NULL default ' ',
  "AAABAPDP" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABAPDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPDS" (
  "AAABAPFB" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAPDW" decimal(5,2) NOT NULL default '0.00',
  "AAABAPDX" decimal(5,2) NOT NULL default '0.00',
  "AAABAPDY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABAPFB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPEB" (
  "AAABAPFA" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABAPFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPEE" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAPFA" varchar(4) NOT NULL default '0000',
  "AAABAPEH" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABAPFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPEK" (
  "AAABAPEQ" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAPFA" varchar(4) NOT NULL default '0000',
  "AAABAPEP" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABAPEQ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPES" (
  "AAABAPEZ" varchar(2) NOT NULL default '00',
  "AAABAPFA" varchar(4) NOT NULL default '0000',
  "AAABAPFB" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAPEY" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABAPEZ","AAABAPFA","AAABAPFB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPFD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAPFN" varchar(2) NOT NULL default ' ',
  "AAABBCSI" varchar(1) NOT NULL default ' ',
  "AAABAWCO" varchar(1) NOT NULL default ' ',
  "AAABAPFQ" varchar(8) NOT NULL default '00000000',
  "AAABBBFS" varchar(4) NOT NULL default ' ',
  "AAABAPFK" varchar(8) NOT NULL default '00000000',
  "AAABKQGG" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAPFN","AAABBCSI","AAABAWCO","AAABAPFQ","AAABBBFS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPFS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAPFX" varchar(4) NOT NULL default ' ',
  "AAABAPFY" varchar(4) NOT NULL default ' ',
  "AAABAPFZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPGD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABAPJX" varchar(8) NOT NULL default '00000000',
  "AAABAPGH" varchar(11) NOT NULL default '00000000000',
  "AAABBCDI" varchar(40) NOT NULL default ' ',
  "AAABAPHL" varchar(30) NOT NULL default ' ',
  "AAABAPHM" varchar(5) NOT NULL default ' ',
  "AAABAPGL" varchar(4) NOT NULL default ' ',
  "AAABAPHO" varchar(10) NOT NULL default ' ',
  "AAABDROT" varchar(28) NOT NULL default ' ',
  "AAABAPHQ" varchar(4) NOT NULL default '0000',
  "AAABHTKL" varchar(2) NOT NULL default '00',
  "AAABAPGQ" varchar(9) NOT NULL default '000000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABAPJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPGU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABAPJX" varchar(8) NOT NULL default '00000000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABAPIL" varchar(1) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABBFVS" varchar(2) NOT NULL default ' ',
  "AAABAPHC" varchar(1) NOT NULL default ' ',
  "AAABAPHD" varchar(11) NOT NULL default '00000000000',
  "AAABAPHE" varchar(28) NOT NULL default ' ',
  "AAABAPHF" varchar(28) NOT NULL default ' ',
  "AAABAPHG" varchar(28) NOT NULL default ' ',
  "AAABAPIR" varchar(2) NOT NULL default ' ',
  "AAABAPHI" varchar(10) NOT NULL default '0000000000',
  "AAABAPHJ" varchar(8) NOT NULL default '00000000',
  "AAABAPHK" varchar(1) NOT NULL default ' ',
  "AAABAPHL" varchar(30) NOT NULL default ' ',
  "AAABAPHM" varchar(5) NOT NULL default ' ',
  "AAABAPHN" varchar(4) NOT NULL default ' ',
  "AAABAPHO" varchar(10) NOT NULL default ' ',
  "AAABDROT" varchar(28) NOT NULL default ' ',
  "AAABAPHQ" varchar(4) NOT NULL default '0000',
  "AAABHTKL" varchar(2) NOT NULL default '00',
  "AAABAPJG" varchar(3) NOT NULL default '000',
  "AAABBHFM" varchar(1) NOT NULL default '0',
  "AAABAPHU" varchar(1) NOT NULL default ' ',
  "AAABAPHV" varchar(8) NOT NULL default '00000000',
  "AAABAPHW" varchar(1) NOT NULL default '0',
  "AAABAPHX" varchar(2) NOT NULL default '00',
  "AAABAQKD" varchar(6) NOT NULL default '000000',
  "AAABAPHZ" varchar(8) NOT NULL default '00000000',
  "AAABAPIA" varchar(2) NOT NULL default '00',
  "AAABAPIB" varchar(6) NOT NULL default '000000',
  "AAABAPIC" varchar(11) NOT NULL default '00000000000',
  "AAABAPID" varchar(11) NOT NULL default '00000000000',
  "AAABAPIE" varchar(1) NOT NULL default ' ',
  "AAABAQKS" varchar(1) NOT NULL default ' ',
  "AAABAPIG" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABAPJX","AAABMCRV","AAABAPIL","AAABJFHB","AAABBFVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPIO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAPIT" varchar(2) NOT NULL default ' ',
  "AAABAPIR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAPIT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPIU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  "AAABHTKL" varchar(2) NOT NULL default '00',
  "AAABAYZR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAABMQNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPJD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABAPJG" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMAMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPJJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCXYD" varchar(1) NOT NULL default ' ',
  "AAABBHFM" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABCXYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPJP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABAPJX" varchar(8) NOT NULL default '00000000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABAPJU" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABAPJX","AAABMCRV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPJZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVXC" varchar(2) NOT NULL default ' ',
  "AAABBHFX" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAPKF" varchar(2) NOT NULL default ' ',
  "AAABAPKG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVXC","AAABBHFX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPKL" (
  "AAABAPKT" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABAPKT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPKO" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAPKT" varchar(2) NOT NULL default '00',
  "AAABAPKR" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABAPKT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPKU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAPNN" varchar(2) NOT NULL default ' ',
  "AAABBHDA" varchar(4) NOT NULL default ' ',
  "AAABAPLG" varchar(2) NOT NULL default '00',
  "AAABBHBN" varchar(4) NOT NULL default ' ',
  "AAABBHCM" varchar(4) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABEJKE" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABAPNN","AAABBHDA","AAABAPLG","AAABBHBN","AAABBHCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPLJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABAPLR" varchar(15) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABAPLR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPLT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAPMP" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAPMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPLY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAPMP" varchar(15) NOT NULL default ' ',
  "AAABAPMC" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAPMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPMG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAPMP" varchar(15) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAPML" decimal(13,2) NOT NULL default '0.00',
  "AAABLXTX" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAPMP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPMR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBHCM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBHCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPMW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBHCM" varchar(4) NOT NULL default ' ',
  "AAABBHBY" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBHCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPNE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAPNN" varchar(2) NOT NULL default ' ',
  "AAABBHCM" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHCH" varchar(30) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAPNN","AAABBHCM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPNQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBHDA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBHDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPNV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBHDA" varchar(4) NOT NULL default ' ',
  "AAABBHCX" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBHDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPOD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAPOS" varchar(2) NOT NULL default ' ',
  "AAABAPOG" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABAPOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPOJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAPOP" varchar(1) NOT NULL default ' ',
  "AAABAPOQ" varchar(1) NOT NULL default ' ',
  "AAABAPOR" varchar(1) NOT NULL default ' ',
  "AAABAPOS" varchar(2) NOT NULL default ' ',
  "AAABAPOT" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABGCHD","AAABJFHB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPOY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAPQB" varchar(4) NOT NULL default ' ',
  "AAABAPPJ" varchar(4) NOT NULL default ' ',
  "AAABAPPK" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABJFBZ" varchar(3) NOT NULL default '000',
  "AAABLLOQ" decimal(6,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABAPQB","AAABAPPJ","AAABAPPK","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPPM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAPQB" varchar(4) NOT NULL default ' ',
  "AAABJFBZ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABAPQB","AAABJFBZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPPT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAPQB" varchar(4) NOT NULL default ' ',
  "AAABJFBZ" varchar(3) NOT NULL default '000',
  "AAABKZAW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAPQB","AAABJFBZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPQD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGAMR" varchar(2) NOT NULL default ' ',
  "AAABAWWU" varchar(2) NOT NULL default '00',
  "AAABLUVE" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAYOA" varchar(5) NOT NULL default '00000',
  "AAABBGOQ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABGAMR","AAABAWWU","AAABLUVE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPQR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGAMR" varchar(2) NOT NULL default ' ',
  "AAABAWWU" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAPQX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGAMR","AAABAWWU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPRC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAABAQCY" varchar(4) NOT NULL default ' ',
  "AAABAQUV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPRL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAHO" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAPRQ" varchar(15) NOT NULL default ' ',
  "AAABAPRR" varchar(18) NOT NULL default ' ',
  "AAABAPRS" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABAPRU" varchar(2) NOT NULL default '00',
  "AAABAPRV" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABBAHO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPRZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAQIF" varchar(6) NOT NULL default '000000',
  "AAABAPSC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAQIF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPSF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGAMR" varchar(2) NOT NULL default ' ',
  "AAABBGOU" varchar(2) NOT NULL default '00',
  "AAABARCR" varchar(1) NOT NULL default ' ',
  "AAABAPSS" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAPSN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGAMR","AAABBGOU","AAABARCR","AAABAPSS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPSU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKCLT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKCLT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPSZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAQKS" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAQKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPTE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAQKS" varchar(2) NOT NULL default ' ',
  "AAABAPTI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAQKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPTM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKCLT" varchar(2) NOT NULL default ' ',
  "AAABAPTQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKCLT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPTU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAPVY" varchar(10) NOT NULL default ' ',
  "AAABAPTX" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAPVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPUA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAPVY" varchar(10) NOT NULL default ' ',
  "AAABAPUE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAPVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPUI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAPWJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAPWJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPUN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAPWJ" varchar(10) NOT NULL default ' ',
  "AAABAPUR" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAPWJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPUV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAPWA" varchar(10) NOT NULL default ' ',
  "AAABAPWJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAPWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPVB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAPWA" varchar(10) NOT NULL default ' ',
  "AAABAPVF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAPWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPVJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAPVY" varchar(10) NOT NULL default ' ',
  "AAABAPVZ" varchar(4) NOT NULL default ' ',
  "AAABAPWA" varchar(10) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAPVQ" varchar(4) NOT NULL default ' ',
  "AAABAPVR" varchar(4) NOT NULL default ' ',
  "AAABAPVS" varchar(4) NOT NULL default ' ',
  "AAABAPVT" varchar(4) NOT NULL default ' ',
  "AAABAPVU" varchar(4) NOT NULL default ' ',
  "AAABAPVV" varchar(4) NOT NULL default ' ',
  "AAABAPVW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAPVY","AAABAPVZ","AAABAPWA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPWC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAPWJ" varchar(10) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAPWJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPWL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHT" varchar(1) NOT NULL default ' ',
  "AAABGCHS" varchar(2) NOT NULL default '00',
  "AAABGCHU" varchar(2) NOT NULL default '00',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAPWT" varchar(4) NOT NULL default ' ',
  "AAABAPWU" varchar(4) NOT NULL default ' ',
  "AAABAPWV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHT","AAABGCHS","AAABGCHU","AAABLQVJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPXC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBHDA" varchar(4) NOT NULL default ' ',
  "AAABBHBN" varchar(4) NOT NULL default ' ',
  "AAABBHCM" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABAPXK" varchar(4) NOT NULL default ' ',
  "AAABEJKE" decimal(5,2) NOT NULL default '0.00',
  "AAABAZHE" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBHDA","AAABBHBN","AAABBHCM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPXS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAQNI" varchar(4) NOT NULL default ' ',
  "AAABAPYX" varchar(4) NOT NULL default '0000',
  "AAABAPZM" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHJD" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABAQNI","AAABAPYX","AAABAPZM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAQNI" varchar(4) NOT NULL default ' ',
  "AAABARBS" varchar(4) NOT NULL default ' ',
  "AAABAPZM" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAQNI","AAABARBS","AAABAPZM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPYR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAQNI" varchar(4) NOT NULL default ' ',
  "AAABARBS" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAPYX" varchar(4) NOT NULL default '0000',
  "AAABAPYY" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABAQNI","AAABARBS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPZD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRWE" varchar(4) NOT NULL default ' ',
  "AAABAPZM" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAPZJ" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLRWE","AAABAPZM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPZO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBHCM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBHCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAPZT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBHCM" varchar(4) NOT NULL default ' ',
  "AAABBHBY" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBHCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQAB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBHCM" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHCH" varchar(30) NOT NULL default ' ',
  "AAABBHCI" varchar(4) NOT NULL default ' ',
  "AAABBHCJ" varchar(4) NOT NULL default ' ',
  "AAABBHCK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBHCM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBHDA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBHDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQAS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBHDA" varchar(4) NOT NULL default ' ',
  "AAABBHCX" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBHDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQBA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAQBG" varchar(1) NOT NULL default ' ',
  "AAABAQBH" varchar(1) NOT NULL default ' ',
  "AAABAQBI" varchar(2) NOT NULL default '00',
  "AAABAQBJ" varchar(4) NOT NULL default ' ',
  "AAABAQBK" varchar(2) NOT NULL default ' ',
  "AAABAQBL" varchar(2) NOT NULL default '00',
  "AAABAQBM" varchar(1) NOT NULL default ' ',
  "AAABAQBN" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABGCHD","AAABJFHB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQBS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAQEH" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAQBX" varchar(7) NOT NULL default '0000000',
  PRIMARY KEY ("AAABMRIL","AAABAQEH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQCB" (
  "AAABAQEA" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABAQEA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQCE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBAEF" varchar(5) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHDK" decimal(8,6) NOT NULL default '0.000000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBAEF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQCP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAQCY" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLRWE" varchar(4) NOT NULL default ' ',
  "AAABAQHY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAQCY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQDA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAQDK" varchar(1) NOT NULL default '0',
  "AAABDYSH" varchar(1) NOT NULL default '0',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAQDG" decimal(9,2) NOT NULL default '0.00',
  "AAABAQDH" decimal(9,2) NOT NULL default '0.00',
  "AAABAQDI" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABAQDK","AAABDYSH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQDN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAQEH" varchar(4) NOT NULL default ' ',
  "AAABAQDR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAQEH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQDV" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAQEA" varchar(2) NOT NULL default ' ',
  "AAABAQDY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABAQEA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQEB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLRWE" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAQEH" varchar(4) NOT NULL default ' ',
  "AAABDYSH" varchar(1) NOT NULL default '0',
  "AAABAQEJ" varchar(14) NOT NULL default ' ',
  "AAABAQEK" varchar(6) NOT NULL default '000000',
  "AAABBAHO" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLRWE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLRWE" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAQNI" varchar(4) NOT NULL default ' ',
  "AAABAQEX" varchar(7) NOT NULL default '0000000',
  "AAABARBS" varchar(4) NOT NULL default ' ',
  "AAABAQHS" decimal(7,4) NOT NULL default '0.0000',
  "AAABIDWZ" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABAQFC" varchar(1) NOT NULL default ' ',
  "AAABAQFD" decimal(7,4) NOT NULL default '0.0000',
  "AAABAQFE" decimal(7,4) NOT NULL default '0.0000',
  "AAABHDGD" decimal(7,4) NOT NULL default '0.0000',
  "AAABAQFG" decimal(7,4) NOT NULL default '0.0000',
  "AAABAQFH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLRWE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQFM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLRWE" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAQFS" varchar(2) NOT NULL default '00',
  "AAABAQSC" varchar(4) NOT NULL default ' ',
  "AAABAQFU" varchar(1) NOT NULL default ' ',
  "AAABAQFV" varchar(2) NOT NULL default '00',
  "AAABAQFW" varchar(7) NOT NULL default '0000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLRWE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQGB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLRWE" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAQGH" varchar(4) NOT NULL default ' ',
  "AAABAQLN" varchar(4) NOT NULL default ' ',
  "AAABAQGJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLRWE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQGO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLRWE" varchar(4) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABAQGT" varchar(10) NOT NULL default '0000000000',
  "AAABAQGU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLRWE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQGY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLRWE" varchar(4) NOT NULL default ' ',
  "AAABAQHY" varchar(4) NOT NULL default ' ',
  "AAABAQHD" varchar(12) NOT NULL default ' ',
  "AAABIMEG" varchar(30) NOT NULL default ' ',
  "AAABAQMG" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLRWE","AAABAQHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQHK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLRWE" varchar(4) NOT NULL default ' ',
  "AAABAQHY" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABARBS" varchar(4) NOT NULL default ' ',
  "AAABAQHS" decimal(7,4) NOT NULL default '0.0000',
  "AAABAQHT" varchar(1) NOT NULL default ' ',
  "AAABAQHU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLRWE","AAABAQHY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQIA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLBT" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAQIF" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABGLBT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQIJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHT" varchar(1) NOT NULL default ' ',
  "AAABGCHS" varchar(2) NOT NULL default '00',
  "AAABAUNH" varchar(1) NOT NULL default ' ',
  "AAABBCIR" varchar(4) NOT NULL default ' ',
  "AAABAQJA" varchar(2) NOT NULL default '00',
  "AAABAQJB" varchar(1) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABAQJD" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABGCHT","AAABGCHS","AAABAUNH","AAABBCIR","AAABAQJA","AAABAQJB","AAABHVYL","AAABAQJD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQJF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGDJ" varchar(2) NOT NULL default ' ',
  "AAABBGDI" varchar(1) NOT NULL default ' ',
  "AAABAQJJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBGDJ","AAABBGDI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQJN" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAQJV" varchar(2) NOT NULL default ' ',
  "AAABAQJQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABAQJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQJT" (
  "AAABAQJV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABAQJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQJW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABLUVE" varchar(5) NOT NULL default '00000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLRWE" varchar(4) NOT NULL default ' ',
  "AAABAQKD" varchar(4) NOT NULL default ' ',
  "AAABAQKE" decimal(7,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABLUVE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQKJ" (
  "AAABAQKS" varchar(2) NOT NULL default ' ',
  "AAABAQKT" varchar(1) NOT NULL default ' ',
  "AAABEKTZ" varchar(1) NOT NULL default ' ',
  "AAABAQKV" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAQKQ" varchar(2) NOT NULL default '00',
  "AAABAQVK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABAQKS","AAABAQKT","AAABEKTZ","AAABAQKV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQKX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAQLN" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAQLC" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABAQLN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQLG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAQLN" varchar(4) NOT NULL default ' ',
  "AAABAQLK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAQLN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQLO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABCXYD" varchar(1) NOT NULL default ' ',
  "AAABBHFM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABCXYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQLW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCXYD" varchar(1) NOT NULL default ' ',
  "AAABBHFM" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABCXYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQMC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAQMI" varchar(4) NOT NULL default ' ',
  "AAABAQMF" varchar(18) NOT NULL default ' ',
  "AAABAQMG" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAQMI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQMJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABARAH" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAQMO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABARAH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQMS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAQNI" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAQMX" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABAQNI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQNB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAQNI" varchar(4) NOT NULL default ' ',
  "AAABAQNF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAQNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQNJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAQNV" varchar(4) NOT NULL default ' ',
  "AAABBCJA" varchar(1) NOT NULL default ' ',
  "AAABBCJB" varchar(2) NOT NULL default ' ',
  "AAABAQNY" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBCJL" varchar(6) NOT NULL default ' ',
  "AAABAQPA" varchar(6) NOT NULL default ' ',
  "AAABAQNT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAQNV","AAABBCJA","AAABBCJB","AAABAQNY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQOA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAQPA" varchar(6) NOT NULL default ' ',
  "AAABBBFS" varchar(4) NOT NULL default ' ',
  "AAABAQOM" varchar(2) NOT NULL default ' ',
  "AAABBCSI" varchar(1) NOT NULL default ' ',
  "AAABMCNO" varchar(2) NOT NULL default '00',
  "AAABAQOH" varchar(1) NOT NULL default ' ',
  "AAABAQOI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAQPA","AAABBBFS","AAABAQOM","AAABBCSI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQOO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAQPA" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAQPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQOT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAQPA" varchar(6) NOT NULL default ' ',
  "AAABAQOX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAQPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQPB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBCJT" varchar(2) NOT NULL default ' ',
  "AAABBCJR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBCJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBBCG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBBCG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQPM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBBCG" varchar(1) NOT NULL default ' ',
  "AAABASIW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBBCG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQPU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAQQD" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKWRL" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABAQQB" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABAQQD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQQF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAQQP" varchar(2) NOT NULL default '00',
  "AAABAQWR" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKWRL" decimal(15,2) NOT NULL default '0.00',
  "AAABAQWO" decimal(7,4) NOT NULL default '0.0000',
  "AAABAQWP" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABAQQP","AAABAQWR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQQS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAQRF" varchar(2) NOT NULL default ' ',
  "AAABAQQV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAQRF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAQRF" varchar(2) NOT NULL default ' ',
  "AAABAQRC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAQRF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQRG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHALW" varchar(3) NOT NULL default ' ',
  "AAABAQRJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHALW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQRM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAQSC" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAQRR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABAQSC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQRV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAQSC" varchar(4) NOT NULL default ' ',
  "AAABAQRZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAQSC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQSD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAQSJ" varchar(4) NOT NULL default ' ',
  "AAABAQSK" varchar(4) NOT NULL default ' ',
  "AAABAQSL" varchar(4) NOT NULL default ' ',
  "AAABAQSM" varchar(4) NOT NULL default ' ',
  "AAABAQSN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQSS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAQSY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQTD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHT" varchar(1) NOT NULL default ' ',
  "AAABGCHS" varchar(2) NOT NULL default '00',
  "AAABAUNH" varchar(1) NOT NULL default ' ',
  "AAABBCIR" varchar(4) NOT NULL default ' ',
  "AAABAQTJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHT","AAABGCHS","AAABAUNH","AAABBCIR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQTP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHT" varchar(1) NOT NULL default ' ',
  "AAABGCHS" varchar(2) NOT NULL default '00',
  "AAABGCHU" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAQTW" varchar(1) NOT NULL default ' ',
  "AAABBHNF" varchar(3) NOT NULL default '000',
  "AAABKRZV" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHT","AAABGCHS","AAABGCHU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQUE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAQYE" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAQYE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQUM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAQUV" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAQUR" decimal(7,4) NOT NULL default '0.0000',
  "AAABAQUS" decimal(7,4) NOT NULL default '0.0000',
  "AAABAQUT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAQUV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQUX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAQYE" varchar(2) NOT NULL default ' ',
  "AAABAQVB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAQYE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQVF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAVT" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAQVK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBAVT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQVO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAQYF" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAQYF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQVW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAQWF" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKWRL" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABAQWD" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABAQWF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQWH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAQWR" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKWRL" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABAQWO" decimal(7,4) NOT NULL default '0.0000',
  "AAABAQWP" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABAQWR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQWT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOHM" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAQWY" varchar(4) NOT NULL default ' ',
  "AAABAQWZ" varchar(4) NOT NULL default ' ',
  "AAABAQXA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOHM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQXE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAQYF" varchar(2) NOT NULL default ' ',
  "AAABAQXI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAQYF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQXM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVXC" varchar(2) NOT NULL default ' ',
  "AAABBHFX" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAQXS" varchar(1) NOT NULL default ' ',
  "AAABAQXT" varchar(2) NOT NULL default ' ',
  "AAABARAH" varchar(4) NOT NULL default ' ',
  "AAABBHKA" varchar(3) NOT NULL default '000',
  "AAABAQYP" varchar(3) NOT NULL default '000',
  "AAABAQYQ" varchar(1) NOT NULL default ' ',
  "AAABAQYR" varchar(1) NOT NULL default ' ',
  "AAABAQYT" varchar(1) NOT NULL default ' ',
  "AAABAQYA" varchar(1) NOT NULL default ' ',
  "AAABAQYB" varchar(1) NOT NULL default ' ',
  "AAABAQYC" varchar(1) NOT NULL default ' ',
  "AAABAQYD" varchar(1) NOT NULL default ' ',
  "AAABAQYE" varchar(2) NOT NULL default ' ',
  "AAABAQYF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVXC","AAABBHFX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAQYV" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAQYP" varchar(3) NOT NULL default '000',
  "AAABAQYQ" varchar(1) NOT NULL default ' ',
  "AAABAQYR" varchar(1) NOT NULL default ' ',
  "AAABAQYS" varchar(1) NOT NULL default ' ',
  "AAABAQYT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAQYV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQYX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBRWV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBRWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQZC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBRWV" varchar(2) NOT NULL default ' ',
  "AAABAQZG" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBRWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAQZK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAHO" varchar(8) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKWRL" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABAQZS" varchar(2) NOT NULL default '00',
  "AAABAQZT" varchar(2) NOT NULL default '00',
  "AAABAQZU" varchar(4) NOT NULL default ' ',
  "AAABAQZV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBAHO","AAABJFHB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARAA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABARAH" varchar(4) NOT NULL default ' ',
  "AAABARAE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABARAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARAI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABARBS" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABARAN" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABARBS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARAR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABIFOV" varchar(4) NOT NULL default ' ',
  "AAABARAV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABIFOV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARAZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIFOV" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABARBE" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABBRWV" varchar(2) NOT NULL default ' ',
  "AAABKPXV" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABIFOV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARBL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABARBS" varchar(4) NOT NULL default ' ',
  "AAABARBP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABARBS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARBT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABARCD" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKWRL" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABARCA" decimal(7,4) NOT NULL default '0.0000',
  "AAABBHOF" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABARCD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARCF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGAMR" varchar(2) NOT NULL default ' ',
  "AAABBGOU" varchar(2) NOT NULL default '00',
  "AAABARCR" varchar(1) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABARCN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGAMR","AAABBGOU","AAABARCR","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARCU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABARDD" varchar(4) NOT NULL default '0000',
  "AAABARDE" varchar(2) NOT NULL default '00',
  "AAABARDF" varchar(4) NOT NULL default '0000',
  "AAABARDG" varchar(2) NOT NULL default '00',
  "AAABARDA" decimal(9,6) NOT NULL default '0.000000',
  "AAABARDB" decimal(9,6) NOT NULL default '0.000000',
  PRIMARY KEY ("AAABMRIL","AAABARDD","AAABARDE","AAABARDF","AAABARDG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARDH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJTTT" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABIFTP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJTTT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARDQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABARDX" varchar(4) NOT NULL default ' ',
  "AAABARDY" varchar(4) NOT NULL default ' ',
  "AAABARDU" varchar(4) NOT NULL default ' ',
  "AAABARDV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABARDX","AAABARDY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARDZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABLUER" varchar(4) NOT NULL default ' ',
  "AAABGAMR" varchar(2) NOT NULL default ' ',
  "AAABARES" varchar(4) NOT NULL default ' ',
  "AAABARET" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABARGA" varchar(1) NOT NULL default ' ',
  "AAABAREJ" varchar(1) NOT NULL default ' ',
  "AAABAREK" varchar(1) NOT NULL default ' ',
  "AAABAREL" varchar(1) NOT NULL default ' ',
  "AAABAREM" varchar(1) NOT NULL default ' ',
  "AAABARFZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABLUER","AAABGAMR","AAABARES","AAABARET","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAREV" (
  "AAABLUER" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAREY" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUER" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLUER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARFE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABFTKZ" varchar(4) NOT NULL default ' ',
  "AAABMCXR" varchar(1) NOT NULL default ' ',
  "AAABARFJ" varchar(4) NOT NULL default ' ',
  "AAABARFK" varchar(1) NOT NULL default ' ',
  "AAABMKXW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABFTKZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARFP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABLUER" varchar(4) NOT NULL default ' ',
  "AAABARGE" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMKXW" varchar(1) NOT NULL default ' ',
  "AAABHDRZ" varchar(4) NOT NULL default ' ',
  "AAABARFY" varchar(4) NOT NULL default ' ',
  "AAABARFZ" varchar(1) NOT NULL default ' ',
  "AAABARGA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABLUER","AAABARGE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARGG" (
  "AAABLUEO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARGJ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUEO" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARGP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHDRZ" varchar(4) NOT NULL default ' ',
  "AAABFTKZ" varchar(4) NOT NULL default ' ',
  "AAABLUEO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABHDRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARGY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHDRZ" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHVYP","AAABHDRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARHI" (
  "AAABARHQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABARHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARHL" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABARHQ" varchar(1) NOT NULL default ' ',
  "AAABMOQQ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABARHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARHR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBFGT" varchar(4) NOT NULL default ' ',
  "AAABARJC" varchar(1) NOT NULL default ' ',
  "AAABARKO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBFGT","AAABARJC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARIB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFGT" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABARKO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFGT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARIK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBFGT" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABARKO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBFGT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARIV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFGT" varchar(4) NOT NULL default ' ',
  "AAABARJC" varchar(1) NOT NULL default ' ',
  "AAABARKO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFGT","AAABARJC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARJD" (
  "AAABBFGT" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABARKO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABBFGT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARJK" (
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBFGT" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABARKO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABHVYP","AAABBFGT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARJT" (
  "AAABBFGT" varchar(4) NOT NULL default ' ',
  "AAABARJV" varchar(1) NOT NULL default ' ',
  "AAABARKH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABBFGT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARJY" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBFGT" varchar(4) NOT NULL default ' ',
  "AAABARKB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABBFGT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARKE" (
  "AAABARKO" varchar(4) NOT NULL default ' ',
  "AAABARKG" varchar(30) NOT NULL default ' ',
  "AAABARKH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABARKO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARKJ" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABARKO" varchar(4) NOT NULL default ' ',
  "AAABARKM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABARKO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARKP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABARKT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABHVYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARKX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAABEPKB" varchar(20) NOT NULL default ' ',
  "AAABARWD" varchar(4) NOT NULL default ' ',
  "AAABARLD" varchar(4) NOT NULL default ' ',
  "AAABARLE" varchar(20) NOT NULL default ' ',
  "AAABARLF" varchar(4) NOT NULL default ' ',
  "AAABARLG" varchar(20) NOT NULL default ' ',
  "AAABARLH" varchar(4) NOT NULL default ' ',
  "AAABARLI" varchar(20) NOT NULL default ' ',
  "AAABARLJ" varchar(4) NOT NULL default ' ',
  "AAABARLK" varchar(20) NOT NULL default ' ',
  "AAABARLL" varchar(4) NOT NULL default ' ',
  "AAABARLM" varchar(20) NOT NULL default ' ',
  "AAABARNL" varchar(4) NOT NULL default ' ',
  "AAABARLO" varchar(20) NOT NULL default ' ',
  "AAABARSA" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARLT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABBFZW" varchar(4) NOT NULL default ' ',
  "AAABARTN" varchar(2) NOT NULL default ' ',
  "AAABEKQV" varchar(4) NOT NULL default '0000',
  "AAABARMH" varchar(1) NOT NULL default ' ',
  "AAABARMA" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFGL","AAABBFZW","AAABARTN","AAABEKQV","AAABARMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARMI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABBFZW" varchar(4) NOT NULL default ' ',
  "AAABARTN" varchar(2) NOT NULL default ' ',
  "AAABARNZ" varchar(2) NOT NULL default ' ',
  "AAABARMO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFGL","AAABBFZW","AAABARTN","AAABARNZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARMU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABARNC" varchar(4) NOT NULL default ' ',
  "AAABBFZW" varchar(4) NOT NULL default ' ',
  "AAABARMY" varchar(40) NOT NULL default ' ',
  "AAABARTN" varchar(2) NOT NULL default ' ',
  "AAABARNA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABARNC","AAABBFZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARNE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABARNL" varchar(4) NOT NULL default ' ',
  "AAABARTN" varchar(2) NOT NULL default ' ',
  "AAABARNI" varchar(40) NOT NULL default ' ',
  "AAABARNJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABARNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARNM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABBFZW" varchar(4) NOT NULL default ' ',
  "AAABARTN" varchar(2) NOT NULL default ' ',
  "AAABARNZ" varchar(2) NOT NULL default ' ',
  "AAABARNT" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABJFGL","AAABBFZW","AAABARTN","AAABARNZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAROA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABBFZW" varchar(4) NOT NULL default ' ',
  "AAABARTO" varchar(2) NOT NULL default ' ',
  "AAABARTN" varchar(2) NOT NULL default ' ',
  "AAABEPUY" varchar(8) NOT NULL default ' ',
  "AAABARQI" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABARQA" varchar(2) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAROL" varchar(1) NOT NULL default ' ',
  "AAABAROM" decimal(7,2) NOT NULL default '0.00',
  "AAABARON" decimal(13,2) NOT NULL default '0.00',
  "AAABAROO" varchar(5) NOT NULL default ' ',
  "AAABAROP" decimal(13,2) NOT NULL default '0.00',
  "AAABAROQ" varchar(5) NOT NULL default ' ',
  "AAABAROR" decimal(13,2) NOT NULL default '0.00',
  "AAABAROS" varchar(5) NOT NULL default ' ',
  "AAABAROT" varchar(1) NOT NULL default ' ',
  "AAABARPY" decimal(7,2) NOT NULL default '0.00',
  "AAABBABG" decimal(13,2) NOT NULL default '0.00',
  "AAABBVTV" varchar(5) NOT NULL default ' ',
  "AAABAROX" decimal(13,2) NOT NULL default '0.00',
  "AAABAROY" varchar(5) NOT NULL default ' ',
  "AAABAROZ" decimal(13,2) NOT NULL default '0.00',
  "AAABARPA" varchar(5) NOT NULL default ' ',
  "AAABARPB" varchar(1) NOT NULL default ' ',
  "AAABARPC" varchar(1) NOT NULL default ' ',
  "AAABARPD" varchar(4) NOT NULL default ' ',
  "AAABARPE" varchar(1) NOT NULL default ' ',
  "AAABARPF" varchar(1) NOT NULL default ' ',
  "AAABARPG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFGL","AAABBFZW","AAABARTO","AAABARTN","AAABEPUY","AAABARQI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARPP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPUY" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARPU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABARQA" varchar(2) NOT NULL default ' ',
  "AAABBFZW" varchar(4) NOT NULL default ' ',
  "AAABARPY" decimal(7,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABARQA","AAABBFZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARQC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPUY" varchar(8) NOT NULL default ' ',
  "AAABARQI" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPUY","AAABARQI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARQJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABBFZW" varchar(4) NOT NULL default ' ',
  "AAABARTN" varchar(2) NOT NULL default ' ',
  "AAABARTO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFGL","AAABBFZW","AAABARTN","AAABARTO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARQU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABBFZW" varchar(4) NOT NULL default ' ',
  "AAABARTN" varchar(2) NOT NULL default ' ',
  "AAABARQZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFGL","AAABBFZW","AAABARTN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARRE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABBFZW" varchar(4) NOT NULL default ' ',
  "AAABARTN" varchar(2) NOT NULL default ' ',
  "AAABARRJ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABJFGL","AAABBFZW","AAABARTN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARRO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABARSA" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABARRT" varchar(60) NOT NULL default ' ',
  "AAABARRU" decimal(13,2) NOT NULL default '0.00',
  "AAABKYSP" varchar(5) NOT NULL default ' ',
  "AAABARRW" decimal(13,2) NOT NULL default '0.00',
  "AAABARRX" varchar(5) NOT NULL default ' ',
  "AAABARRY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABARSA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARSC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAABBFZW" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAWCO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS","AAABBFZW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARSP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABBFZW" varchar(4) NOT NULL default ' ',
  "AAABARTN" varchar(2) NOT NULL default ' ',
  "AAABDKAV" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABJFGL","AAABBFZW","AAABARTN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARTB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABBFZW" varchar(4) NOT NULL default ' ',
  "AAABARTN" varchar(2) NOT NULL default ' ',
  "AAABARTO" varchar(2) NOT NULL default ' ',
  "AAABARTI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABJFGL","AAABBFZW","AAABARTN","AAABARTO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARTP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCQP" varchar(1) NOT NULL default ' ',
  "AAABBGAW" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBGAQ" decimal(15,2) NOT NULL default '0.00',
  "AAABBGAR" varchar(5) NOT NULL default ' ',
  "AAABBGDU" decimal(7,2) NOT NULL default '0.00',
  "AAABBGAT" decimal(13,2) NOT NULL default '0.00',
  "AAABBGAU" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCQP","AAABBGAW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARUE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABARXE" varchar(1) NOT NULL default ' ',
  "AAABMCQP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABARXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARUK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABARXQ" varchar(2) NOT NULL default ' ',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABARXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARUQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABARXQ" varchar(2) NOT NULL default ' ',
  "AAABARVL" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABARXS" decimal(13,2) NOT NULL default '0.00',
  "AAABARUX" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABARXQ","AAABARVL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARVC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABARVL" varchar(1) NOT NULL default ' ',
  "AAABARXE" varchar(1) NOT NULL default ' ',
  "AAABARVG" varchar(1) NOT NULL default ' ',
  "AAABARVH" varchar(1) NOT NULL default ' ',
  "AAABARVI" varchar(1) NOT NULL default ' ',
  "AAABARVJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABARVL","AAABARXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARVN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABARXD" varchar(1) NOT NULL default ' ',
  "AAABARXE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABARXD","AAABARXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABARWD" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABARXQ" varchar(2) NOT NULL default ' ',
  "AAABBFPK" varchar(20) NOT NULL default ' ',
  "AAABARWB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABARWD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARWF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABARXE" varchar(1) NOT NULL default ' ',
  "AAABARXA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABARXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARWN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABARXQ" varchar(2) NOT NULL default ' ',
  "AAABDKMH" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABARXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARWV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABARXD" varchar(1) NOT NULL default ' ',
  "AAABARXE" varchar(1) NOT NULL default ' ',
  "AAABARXA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABARXD","AAABARXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARXF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABLQVJ" varchar(5) NOT NULL default '00000',
  "AAABBFZW" varchar(1) NOT NULL default ' ',
  "AAABKOTX" varchar(2) NOT NULL default ' ',
  "AAABARXL" varchar(6) NOT NULL default ' ',
  "AAABARXM" varchar(8) NOT NULL default '00000000',
  "AAABARXN" varchar(8) NOT NULL default '00000000',
  "AAABIMEG" varchar(60) NOT NULL default ' ',
  "AAABARXP" varchar(18) NOT NULL default ' ',
  "AAABARXQ" varchar(2) NOT NULL default ' ',
  "AAABBGAQ" decimal(15,2) NOT NULL default '0.00',
  "AAABARXS" decimal(13,2) NOT NULL default '0.00',
  "AAABBGDU" decimal(7,2) NOT NULL default '0.00',
  "AAABBGAT" decimal(13,2) NOT NULL default '0.00',
  "AAABBGAF" decimal(15,2) NOT NULL default '0.00',
  "AAABARXW" decimal(15,2) NOT NULL default '0.00',
  "AAABARXX" decimal(15,2) NOT NULL default '0.00',
  "AAABARXY" decimal(15,2) NOT NULL default '0.00',
  "AAABARXZ" decimal(15,2) NOT NULL default '0.00',
  "AAABARYA" varchar(4) NOT NULL default ' ',
  "AAABBTUU" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABLQVJ","AAABBFZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARYG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABASAE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABASAE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARYL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABASAM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABASAM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARYQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABJDQB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABJDQB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARYX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDDT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDDT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARZC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABARZW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABARZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARZH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBDDT" varchar(2) NOT NULL default ' ',
  "AAABARZL" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBDDT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARZP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABARZW" varchar(2) NOT NULL default ' ',
  "AAABARZT" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABARZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABARZX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABASAE" varchar(2) NOT NULL default ' ',
  "AAABASAB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABASAE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABASAM" varchar(2) NOT NULL default ' ',
  "AAABASAJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABASAM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABJDQB" varchar(1) NOT NULL default ' ',
  "AAABASIV" varchar(10) NOT NULL default ' ',
  "AAABASIW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABJDQB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASAY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABASDM" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABASDM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASBD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABASDU" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABASDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASBI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABASGN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABASGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASBN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABASGM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABASGM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASBS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABASCO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABASCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASBX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABASCW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABASCW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASCC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIYJK" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIYJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASCH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABASCO" varchar(1) NOT NULL default ' ',
  "AAABASCL" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABASCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASCP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABASCW" varchar(4) NOT NULL default ' ',
  "AAABASCT" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABASCW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASCX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIYJK" varchar(2) NOT NULL default ' ',
  "AAABIMUR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABIYJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABASDM" varchar(8) NOT NULL default ' ',
  "AAABBFZT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABASDM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASDN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABASDU" varchar(2) NOT NULL default ' ',
  "AAABDDSS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABASDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASDV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABASGN" varchar(2) NOT NULL default ' ',
  "AAABASDZ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABASGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASED" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABASGM" varchar(4) NOT NULL default ' ',
  "AAABASEH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABASGM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASEL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABASHZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABASHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABASHR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABASHR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASEV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBATP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBATP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASFA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABASIQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABASIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASFF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABASIZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABASIZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASFK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRBI" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRBI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASFP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABASHJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABASHJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASFU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABASHB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABASHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASFZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABASGS" varchar(4) NOT NULL default ' ',
  "AAABASGT" varchar(20) NOT NULL default ' ',
  "AAABLRSU" varchar(8) NOT NULL default '00000000',
  "AAABLRSV" varchar(8) NOT NULL default '00000000',
  "AAABASGF" varchar(40) NOT NULL default ' ',
  "AAABASGG" varchar(20) NOT NULL default ' ',
  "AAABASHB" varchar(2) NOT NULL default ' ',
  "AAABASGI" varchar(40) NOT NULL default ' ',
  "AAABASGJ" varchar(10) NOT NULL default ' ',
  "AAABHDCP" varchar(20) NOT NULL default ' ',
  "AAABASGL" varchar(40) NOT NULL default ' ',
  "AAABASGM" varchar(4) NOT NULL default ' ',
  "AAABASGN" varchar(2) NOT NULL default ' ',
  "AAABASGO" varchar(40) NOT NULL default ' ',
  "AAABASGP" varchar(8) NOT NULL default '00000000',
  "AAABASGQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABASGS","AAABASGT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASGU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABASHB" varchar(2) NOT NULL default ' ',
  "AAABBACF" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABASHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASHC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABASHJ" varchar(2) NOT NULL default ' ',
  "AAABASHG" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABASHJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASHK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABASHR" varchar(2) NOT NULL default ' ',
  "AAABASHO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABASHR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASHS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABASHZ" varchar(1) NOT NULL default ' ',
  "AAABAYKI" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABASHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASIA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBATP" varchar(4) NOT NULL default ' ',
  "AAABBATM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBATP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASII" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABASIQ" varchar(2) NOT NULL default ' ',
  "AAABASIV" varchar(10) NOT NULL default ' ',
  "AAABASIW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABASIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASIR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABASIZ" varchar(1) NOT NULL default ' ',
  "AAABASIV" varchar(10) NOT NULL default ' ',
  "AAABASIW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABASIZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASJA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABLRBI" varchar(2) NOT NULL default ' ',
  "AAABLPWW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABLRBI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASJI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABHIPZ" varchar(3) NOT NULL default ' ',
  "AAABASJM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAABHIPZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASJQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBBAK" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHTE" varchar(4) NOT NULL default ' ',
  "AAABASJW" varchar(4) NOT NULL default ' ',
  "AAABKQXB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBBAK","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASKB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKQXB" varchar(2) NOT NULL default ' ',
  "AAABASKK" varchar(3) NOT NULL default '000',
  "AAABASKL" varchar(3) NOT NULL default '000',
  "AAABASKG" decimal(5,2) NOT NULL default '0.00',
  "AAABASKH" decimal(2,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKQXB","AAABASKK","AAABASKL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABASKY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABASKY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASKR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABASKY" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABASKY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASKZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABASLL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABASLL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASLE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABASLL" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABASLL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASLM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABASLY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABASLY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASLR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABASLY" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABASLY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASLZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABASML" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABASML")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASME" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABASML" varchar(20) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABASML")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASMM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAUGL" varchar(20) NOT NULL default ' ',
  "AAABASRH" varchar(10) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAUGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASMU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAUGL" varchar(20) NOT NULL default ' ',
  "AAABASOB" varchar(3) NOT NULL default ' ',
  "AAABMKAI" varchar(3) NOT NULL default ' ',
  "AAABLUZL" varchar(20) NOT NULL default ' ',
  "AAABAVCN" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABASND" decimal(10,5) NOT NULL default '0.00000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAUGL","AAABASOB","AAABMKAI","AAABLUZL","AAABAVCN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASNN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAUGL" varchar(20) NOT NULL default ' ',
  "AAABASOB" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABASNT" varchar(12) NOT NULL default ' ',
  "AAABASNU" varchar(75) NOT NULL default ' ',
  "AAABASNV" varchar(1) NOT NULL default ' ',
  "AAABASNW" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAUGL","AAABASOB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASOD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAUGL" varchar(20) NOT NULL default ' ',
  "AAABASOH" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAUGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASOL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABASQG" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABASQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASOQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABASQG" varchar(3) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABASOU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABASQG","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASOY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABASQG" varchar(3) NOT NULL default ' ',
  "AAABKRYU" varchar(8) NOT NULL default '00000000',
  "AAABAVRD" varchar(6) NOT NULL default '000000',
  "AAABASQD" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABASQG","AAABKRYU","AAABAVRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASPK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABASQG" varchar(3) NOT NULL default ' ',
  "AAABKRYU" varchar(8) NOT NULL default '00000000',
  "AAABASPP" varchar(2) NOT NULL default ' ',
  "AAABLVJB" varchar(6) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABASQG","AAABKRYU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABASQG" varchar(1) NOT NULL default ' ',
  "AAABAVRD" varchar(6) NOT NULL default '000000',
  "AAABASQD" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABASQG","AAABAVRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASQI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABASQU" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABASQU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASQN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABASQU" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABASQU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASQV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABASRH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABASRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASRA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABASRH" varchar(10) NOT NULL default ' ',
  "AAABASRE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABASRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASRI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABASRP" varchar(3) NOT NULL default '000',
  "AAABASRQ" varchar(5) NOT NULL default '00000',
  "AAABASRM" int(11) NOT NULL default '0',
  "AAABASRN" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABASRP","AAABASRQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASRR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABATHL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABATHL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASRW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABATHL" varchar(1) NOT NULL default ' ',
  "AAABASSA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABATHL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASSE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZDJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASSJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZDJ" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABASXZ" varchar(10) NOT NULL default ' ',
  "AAABASYA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZDJ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASSS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABASTO" varchar(2) NOT NULL default ' ',
  "AAABASTP" varchar(6) NOT NULL default ' ',
  "AAABASVD" varchar(1) NOT NULL default ' ',
  "AAABASUE" varchar(1) NOT NULL default ' ',
  "AAABASUR" varchar(1) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABASTO","AAABASTP","AAABASVD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASTF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABASTO" varchar(2) NOT NULL default ' ',
  "AAABASTP" varchar(6) NOT NULL default ' ',
  "AAABASVD" varchar(1) NOT NULL default ' ',
  "AAABASTR" varchar(1) NOT NULL default ' ',
  "AAABASTL" varchar(160) NOT NULL default ' ',
  "AAABASTM" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABASTO","AAABASTP","AAABASVD","AAABASTR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASTS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABASUE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABASUE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASTX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABASUE" varchar(1) NOT NULL default ' ',
  "AAABHACY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABASUE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASUF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABASUR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABASUR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASUK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABASUR" varchar(1) NOT NULL default ' ',
  "AAABASZA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABASUR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASUS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABASVD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABASVD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASUX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABASVD" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABASZN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABASVD","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASVF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVGI" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVGI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASVK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAVGI" varchar(2) NOT NULL default ' ',
  "AAABASVO" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAVGI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASVS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABAVGQ" varchar(6) NOT NULL default '000000',
  "AAABAVRD" varchar(6) NOT NULL default '000000',
  "AAABASVX" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABAVGQ","AAABAVRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASWC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVBV" varchar(3) NOT NULL default '000',
  "AAABMOHM" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVBV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASWI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAVBV" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAVBV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASWQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABAVWW" varchar(3) NOT NULL default ' ',
  "AAABAVWX" varchar(3) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAVRD" varchar(6) NOT NULL default '000000',
  "AAABASWX" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABAVWW","AAABAVWX","AAABKFCV","AAABAVRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASXE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABAVWW" varchar(3) NOT NULL default ' ',
  "AAABAVWX" varchar(3) NOT NULL default ' ',
  "AAABAVRD" varchar(6) NOT NULL default '000000',
  "AAABASXK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABAVWW","AAABAVWX","AAABAVRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASXQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZDJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASXV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZDJ" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABASXZ" varchar(10) NOT NULL default ' ',
  "AAABASYA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZDJ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASYE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABASZW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABASZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASYJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABASZW" varchar(1) NOT NULL default ' ',
  "AAABHACY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABASZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASYR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABASZX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABASZX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASYW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABASZX" varchar(1) NOT NULL default ' ',
  "AAABASZA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABASZX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASZE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABATAO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABATAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASZJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABATAO" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABASZN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABATAO","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABASZR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABATAM" varchar(2) NOT NULL default ' ',
  "AAABATAN" varchar(6) NOT NULL default ' ',
  "AAABATAO" varchar(1) NOT NULL default ' ',
  "AAABASZW" varchar(1) NOT NULL default ' ',
  "AAABASZX" varchar(1) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABATAM","AAABATAN","AAABATAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATAE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABATAM" varchar(2) NOT NULL default ' ',
  "AAABATAN" varchar(6) NOT NULL default ' ',
  "AAABATAO" varchar(1) NOT NULL default ' ',
  "AAABATAP" varchar(1) NOT NULL default ' ',
  "AAABATAK" varchar(160) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABATAM","AAABATAN","AAABATAO","AAABATAP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATAQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABATBB" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABATBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATAV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABATBB" varchar(8) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABATAZ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABATBB","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATBD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABATBO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABATBO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATBI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABATBO" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABATBM" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABATBO","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATBQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABATCB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABATCB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATBV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABATCB" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABATBZ" varchar(150) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABATCB","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATCD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABATCO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABATCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATCI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABATCO" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABATCM" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABATCO","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATCQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVHT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVHT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATCV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAVHT" varchar(2) NOT NULL default ' ',
  "AAABATCZ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAVHT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATDD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABAVIF" varchar(3) NOT NULL default '000',
  "AAABAVRD" varchar(6) NOT NULL default '000000',
  "AAABATDI" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABAVIF","AAABAVRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATDN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVWB" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABAVWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATDS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAVWB" varchar(2) NOT NULL default '00',
  "AAABATDW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAVWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATEA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVWB" varchar(2) NOT NULL default '00',
  "AAABAVJH" varchar(17) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABATEF" varchar(10) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABATEH" varchar(6) NOT NULL default ' ',
  "AAABJGWD" varchar(6) NOT NULL default ' ',
  "AAABATEJ" decimal(15,2) NOT NULL default '0.00',
  "AAABATEK" decimal(15,2) NOT NULL default '0.00',
  "AAABAVEL" varchar(5) NOT NULL default ' ',
  "AAABAVEO" varchar(5) NOT NULL default ' ',
  "AAABAVEM" decimal(5,0) NOT NULL default '0',
  "AAABAVEP" decimal(5,0) NOT NULL default '0',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABAVWS" decimal(15,0) NOT NULL default '0',
  "AAABAVWT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABAVWB","AAABAVJH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATEZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVWB" varchar(2) NOT NULL default '00',
  "AAABAVJH" varchar(17) NOT NULL default ' ',
  "AAABATZZ" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABATFZ" varchar(3) NOT NULL default '000',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABAVWS" decimal(15,0) NOT NULL default '0',
  "AAABAVWT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABAVWB","AAABAVJH","AAABATZZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATFS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVWB" varchar(2) NOT NULL default '00',
  "AAABAVJH" varchar(17) NOT NULL default ' ',
  "AAABATZZ" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABATFZ" varchar(3) NOT NULL default '000',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABAVWS" decimal(15,0) NOT NULL default '0',
  "AAABAVWT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABAVWB","AAABAVJH","AAABATZZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATGL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAVWB" varchar(2) NOT NULL default '00',
  "AAABAVJH" varchar(17) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABJHFY" varchar(75) NOT NULL default ' ',
  "AAABGGGH" varchar(12) NOT NULL default ' ',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABAVWS" decimal(15,0) NOT NULL default '0',
  "AAABAVWT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAVWB","AAABAVJH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATHD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVRC" varchar(12) NOT NULL default '000000000000',
  "AAABAVWB" varchar(2) NOT NULL default '00',
  "AAABAVJH" varchar(17) NOT NULL default ' ',
  "AAABAVJI" varchar(4) NOT NULL default '0000',
  "AAABATZZ" varchar(8) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABATHL" varchar(1) NOT NULL default ' ',
  "AAABATHM" varchar(1) NOT NULL default ' ',
  "AAABATHN" varchar(1) NOT NULL default ' ',
  "AAABATHO" varchar(1) NOT NULL default ' ',
  "AAABATHP" varchar(1) NOT NULL default ' ',
  "AAABATHQ" varchar(1) NOT NULL default ' ',
  "AAABATHR" varchar(1) NOT NULL default ' ',
  "AAABATHS" int(11) NOT NULL default '0',
  "AAABATHT" int(11) NOT NULL default '0',
  "AAABATHU" int(11) NOT NULL default '0',
  "AAABATHV" int(11) NOT NULL default '0',
  "AAABATHW" int(11) NOT NULL default '0',
  "AAABATHX" int(11) NOT NULL default '0',
  "AAABATHY" int(11) NOT NULL default '0',
  "AAABATHZ" varchar(1) NOT NULL default ' ',
  "AAABATIA" varchar(1) NOT NULL default ' ',
  "AAABATIB" varchar(1) NOT NULL default ' ',
  "AAABATIC" varchar(1) NOT NULL default ' ',
  "AAABATID" int(11) NOT NULL default '0',
  "AAABATIE" int(11) NOT NULL default '0',
  "AAABATIF" int(11) NOT NULL default '0',
  "AAABATIG" int(11) NOT NULL default '0',
  "AAABATIH" int(11) NOT NULL default '0',
  "AAABATII" int(11) NOT NULL default '0',
  "AAABATIJ" int(11) NOT NULL default '0',
  "AAABATIK" varchar(20) NOT NULL default ' ',
  "AAABATIL" varchar(20) NOT NULL default ' ',
  "AAABATIM" varchar(10) NOT NULL default ' ',
  "AAABLQDO" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAVRC","AAABAVWB","AAABAVJH","AAABAVJI","AAABATZZ"),
  KEY "AAABATIW" ("AAABMRIL","AAABMCRV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATIZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVRC" varchar(12) NOT NULL default '000000000000',
  "AAABAVWB" varchar(2) NOT NULL default '00',
  "AAABAVJH" varchar(17) NOT NULL default ' ',
  "AAABAVJI" varchar(4) NOT NULL default '0000',
  "AAABATZZ" varchar(8) NOT NULL default ' ',
  "AAABATKF" varchar(60) NOT NULL default ' ',
  "AAABMKAI" varchar(3) NOT NULL default ' ',
  "AAABATJI" varchar(60) NOT NULL default ' ',
  "AAABATKH" varchar(20) NOT NULL default ' ',
  "AAABATKI" varchar(20) NOT NULL default ' ',
  "AAABESRL" varchar(2) NOT NULL default ' ',
  "AAABATJM" varchar(1) NOT NULL default ' ',
  "AAABATJN" varchar(1) NOT NULL default ' ',
  "AAABAVVI" varchar(1) NOT NULL default ' ',
  "AAABATPO" varchar(8) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAVRC","AAABAVWB","AAABAVJH","AAABAVJI","AAABATZZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATJY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVRC" varchar(12) NOT NULL default '000000000000',
  "AAABAVWB" varchar(2) NOT NULL default '00',
  "AAABAVJH" varchar(17) NOT NULL default ' ',
  "AAABAVJI" varchar(4) NOT NULL default '0000',
  "AAABATZZ" varchar(8) NOT NULL default ' ',
  "AAABATKF" varchar(60) NOT NULL default ' ',
  "AAABMKAI" varchar(3) NOT NULL default ' ',
  "AAABATKH" varchar(20) NOT NULL default ' ',
  "AAABATKI" varchar(20) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAVRC","AAABAVWB","AAABAVJH","AAABAVJI","AAABATZZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATKR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVRC" varchar(12) NOT NULL default '000000000000',
  "AAABAVWB" varchar(2) NOT NULL default '00',
  "AAABAVJH" varchar(17) NOT NULL default ' ',
  "AAABAVJI" varchar(4) NOT NULL default '0000',
  "AAABATZZ" varchar(8) NOT NULL default ' ',
  "AAABATKY" decimal(8,2) NOT NULL default '0.00',
  "AAABATKZ" decimal(8,2) NOT NULL default '0.00',
  "AAABATLA" decimal(8,2) NOT NULL default '0.00',
  "AAABATLB" decimal(8,2) NOT NULL default '0.00',
  "AAABATLC" decimal(8,2) NOT NULL default '0.00',
  "AAABATLD" decimal(8,2) NOT NULL default '0.00',
  "AAABATLE" decimal(8,2) NOT NULL default '0.00',
  "AAABATLF" decimal(8,2) NOT NULL default '0.00',
  "AAABATLG" varchar(20) NOT NULL default ' ',
  "AAABATLH" varchar(20) NOT NULL default ' ',
  "AAABATLI" varchar(40) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAVRC","AAABAVWB","AAABAVJH","AAABAVJI","AAABATZZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATLR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABATMD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABATMD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATLW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABATMD" varchar(1) NOT NULL default ' ',
  "AAABATMA" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABATMD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATME" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVRC" varchar(12) NOT NULL default '000000000000',
  "AAABAVWB" varchar(2) NOT NULL default '00',
  "AAABAVJH" varchar(17) NOT NULL default ' ',
  "AAABAVJI" varchar(4) NOT NULL default '0000',
  "AAABAVRD" varchar(6) NOT NULL default '000000',
  "AAABATML" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVRC","AAABAVWB","AAABAVJH","AAABAVJI","AAABAVRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATMS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVBV" varchar(3) NOT NULL default '000',
  "AAABAVBW" varchar(2) NOT NULL default '00',
  "AAABAVBX" varchar(5) NOT NULL default '00000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAVBQ" varchar(2) NOT NULL default ' ',
  "AAABATNA" varchar(3) NOT NULL default '000',
  "AAABAVBO" varchar(2) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABATND" varchar(1) NOT NULL default ' ',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABMOHM" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVBV","AAABAVBW","AAABAVBX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATNP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAUQY" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAUQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATNU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAUQY" varchar(5) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAUQY","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATOC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAUIF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAUIF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATOH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAUIF" varchar(3) NOT NULL default ' ',
  "AAABATOL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAUIF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATOP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABATZZ" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABATPO" varchar(8) NOT NULL default ' ',
  "AAABATPP" varchar(6) NOT NULL default ' ',
  "AAABATYZ" varchar(3) NOT NULL default ' ',
  "AAABATPR" varchar(60) NOT NULL default ' ',
  "AAABATZA" varchar(20) NOT NULL default ' ',
  "AAABATZB" varchar(20) NOT NULL default ' ',
  "AAABATPU" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABAVWS" decimal(15,0) NOT NULL default '0',
  "AAABAVWT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABATZZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATPK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABATZZ" varchar(8) NOT NULL default ' ',
  "AAABATPZ" varchar(1) NOT NULL default ' ',
  "AAABATPO" varchar(8) NOT NULL default ' ',
  "AAABATPP" varchar(6) NOT NULL default ' ',
  "AAABATYZ" varchar(3) NOT NULL default ' ',
  "AAABATPR" varchar(60) NOT NULL default ' ',
  "AAABATPS" varchar(20) NOT NULL default ' ',
  "AAABATPT" varchar(20) NOT NULL default ' ',
  "AAABATPU" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABATZZ","AAABATPZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATQA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABATZZ" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABATZR" varchar(60) NOT NULL default ' ',
  "AAABGGGH" varchar(12) NOT NULL default ' ',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABAVWS" decimal(15,0) NOT NULL default '0',
  "AAABAVWT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABATZZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABATZZ" varchar(8) NOT NULL default ' ',
  "AAABATZR" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABATZZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVRC" varchar(12) NOT NULL default '000000000000',
  "AAABAVWB" varchar(2) NOT NULL default '00',
  "AAABAVJH" varchar(17) NOT NULL default ' ',
  "AAABAVJI" varchar(4) NOT NULL default '0000',
  "AAABATZZ" varchar(8) NOT NULL default ' ',
  "AAABAVRD" varchar(6) NOT NULL default '000000',
  "AAABATRG" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVRC","AAABAVWB","AAABAVJH","AAABAVJI","AAABATZZ","AAABAVRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATRO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABJLPG" varchar(16) NOT NULL default ' ',
  "AAABJTHJ" varchar(1) NOT NULL default ' ',
  "AAABAVYC" varchar(25) NOT NULL default ' ',
  "AAABAVYD" varchar(25) NOT NULL default ' ',
  "AAABAVYE" varchar(25) NOT NULL default ' ',
  "AAABAVYF" varchar(25) NOT NULL default ' ',
  "AAABDNYG" varchar(30) NOT NULL default ' ',
  "AAABAUZL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLESS","AAABJLPG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATSE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVKL" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVKL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATSJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAVKL" varchar(3) NOT NULL default ' ',
  "AAABATSN" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAVKL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATSR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABAVKL" varchar(3) NOT NULL default ' ',
  "AAABAVKM" varchar(8) NOT NULL default '00000000',
  "AAABAVRD" varchar(6) NOT NULL default '000000',
  "AAABAVKE" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABAVKL","AAABAVKM","AAABAVRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATTD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABATTJ" varchar(8) NOT NULL default '00000000',
  "AAABATTM" varchar(10) NOT NULL default ' ',
  "AAABATTN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABATTJ"),
  KEY "AAABATTK" ("AAABMRIL","AAABATTM","AAABATTN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATTO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABJLPG" varchar(16) NOT NULL default ' ',
  "AAABJTHJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABMRHS","AAABLESS","AAABJLPG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATUA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABJLPG" varchar(16) NOT NULL default ' ',
  "AAABAXKZ" varchar(2) NOT NULL default ' ',
  "AAABATUH" varchar(90) NOT NULL default ' ',
  "AAABATUI" varchar(90) NOT NULL default ' ',
  "AAABATUJ" varchar(90) NOT NULL default ' ',
  "AAABATUK" varchar(90) NOT NULL default ' ',
  "AAABATUL" varchar(90) NOT NULL default ' ',
  "AAABAVYC" varchar(25) NOT NULL default ' ',
  "AAABAVYD" varchar(25) NOT NULL default ' ',
  "AAABAVYE" varchar(25) NOT NULL default ' ',
  "AAABAVYF" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABMRHS","AAABLESS","AAABJLPG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATUV" (
  "AAABMKFH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATUY" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMKFH" varchar(2) NOT NULL default ' ',
  "AAABATVB" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATVE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVTA" varchar(12) NOT NULL default '000000000000',
  "AAABAVTB" varchar(12) NOT NULL default ' ',
  "AAABAVTC" varchar(4) NOT NULL default ' ',
  "AAABAWQE" varchar(7) NOT NULL default ' ',
  "AAABAVYS" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVTA","AAABAVTB","AAABAVTC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATVP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAVFD" varchar(10) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABKFCZ"),
  KEY "AAABATWA" ("AAABMRIL","AAABAVFD"),
  KEY "AAABATWD" ("AAABMRIL","AAABKFCV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATWH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABAVWW" varchar(3) NOT NULL default ' ',
  "AAABAVWX" varchar(3) NOT NULL default ' ',
  "AAABATWM" varchar(3) NOT NULL default ' ',
  "AAABATWN" varchar(1) NOT NULL default ' ',
  "AAABAVNT" varchar(8) NOT NULL default '00000000',
  "AAABATXA" varchar(8) NOT NULL default '00000000',
  "AAABAVSD" varchar(1) NOT NULL default ' ',
  "AAABATWR" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABAVWW","AAABAVWX"),
  KEY "AAABATWY" ("AAABMRIL","AAABATXA"),
  KEY "AAABATXB" ("AAABMRIL","AAABAVWW","AAABAVWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATXF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABAVWW" varchar(3) NOT NULL default ' ',
  "AAABAVWX" varchar(3) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABGGGH" varchar(12) NOT NULL default ' ',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABAVWS" decimal(15,0) NOT NULL default '0',
  "AAABAVWT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABAVWW","AAABAVWX","AAABKFCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATXW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABAVRD" varchar(6) NOT NULL default '000000',
  "AAABATYA" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABAVRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATYE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVWW" varchar(3) NOT NULL default ' ',
  "AAABEKKQ" varchar(2) NOT NULL default ' ',
  "AAABATYI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVWW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATYL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAVWW" varchar(3) NOT NULL default ' ',
  "AAABATYP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAVWW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATYT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABATZZ" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABATYY" varchar(6) NOT NULL default ' ',
  "AAABATYZ" varchar(3) NOT NULL default ' ',
  "AAABATZA" varchar(20) NOT NULL default ' ',
  "AAABATZB" varchar(20) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABAVWS" decimal(15,0) NOT NULL default '0',
  "AAABAVWT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABATZZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABATZL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABATZZ" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABATZR" varchar(60) NOT NULL default ' ',
  "AAABGGGH" varchar(12) NOT NULL default ' ',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABAVWS" decimal(15,0) NOT NULL default '0',
  "AAABAVWT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABATZZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUAB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVNL" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUAG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAVNL" varchar(20) NOT NULL default ' ',
  "AAABAUAK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAVNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUAO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVWW" varchar(3) NOT NULL default ' ',
  "AAABAVWX" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABAUDV" varchar(3) NOT NULL default '000',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABAVRD" varchar(3) NOT NULL default '000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMRCK" varchar(255) NOT NULL default ' ',
  "AAABGGGH" varchar(12) NOT NULL default ' ',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABAVWS" decimal(15,0) NOT NULL default '0',
  "AAABAVWT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABAVWW","AAABAVWX","AAABKFCZ","AAABAUDV","AAABLEHK","AAABAVRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUBK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVWW" varchar(3) NOT NULL default ' ',
  "AAABAVWX" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABAUDV" varchar(3) NOT NULL default '000',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABAVRD" varchar(3) NOT NULL default '000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMRCK" varchar(255) NOT NULL default ' ',
  "AAABGGGH" varchar(12) NOT NULL default ' ',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABAVWS" decimal(15,0) NOT NULL default '0',
  "AAABAVWT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABAVWW","AAABAVWX","AAABKFCZ","AAABAUDV","AAABLEHK","AAABAVRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUCG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVWW" varchar(3) NOT NULL default ' ',
  "AAABAVWX" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABAUDV" varchar(3) NOT NULL default '000',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABAVRD" varchar(3) NOT NULL default '000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMRCK" varchar(255) NOT NULL default ' ',
  "AAABGGGH" varchar(12) NOT NULL default ' ',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABAVWS" decimal(15,0) NOT NULL default '0',
  "AAABAVWT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABAVWW","AAABAVWX","AAABKFCZ","AAABAUDV","AAABLEHK","AAABAVRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUDC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVWW" varchar(3) NOT NULL default ' ',
  "AAABAVWX" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABAUDV" varchar(3) NOT NULL default '000',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABAVRD" varchar(3) NOT NULL default '000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMRCK" varchar(255) NOT NULL default ' ',
  "AAABGGGH" varchar(12) NOT NULL default ' ',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABAVWS" decimal(15,0) NOT NULL default '0',
  "AAABAVWT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABAVWW","AAABAVWX","AAABKFCZ","AAABAUDV","AAABLEHK","AAABAVRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUDY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVWW" varchar(3) NOT NULL default ' ',
  "AAABAVWX" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAVNL" varchar(20) NOT NULL default ' ',
  "AAABMOQQ" varchar(1) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABAUEI" varchar(1) NOT NULL default ' ',
  "AAABLBEK" varchar(1) NOT NULL default ' ',
  "AAABAVDA" varchar(1) NOT NULL default ' ',
  "AAABAUEL" varchar(1) NOT NULL default ' ',
  "AAABAUEM" varchar(1) NOT NULL default ' ',
  "AAABAUEN" varchar(1) NOT NULL default ' ',
  "AAABAUEO" varchar(1) NOT NULL default ' ',
  "AAABAUEP" varchar(1) NOT NULL default ' ',
  "AAABAUEQ" varchar(1) NOT NULL default ' ',
  "AAABAUER" varchar(1) NOT NULL default ' ',
  "AAABAUES" varchar(1) NOT NULL default ' ',
  "AAABAUET" varchar(1) NOT NULL default ' ',
  "AAABAVEK" decimal(15,2) NOT NULL default '0.00',
  "AAABAVEN" decimal(15,2) NOT NULL default '0.00',
  "AAABAVEL" varchar(5) NOT NULL default ' ',
  "AAABAVEO" varchar(5) NOT NULL default ' ',
  "AAABAVEM" decimal(5,0) NOT NULL default '0',
  "AAABAVEP" decimal(5,0) NOT NULL default '0',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABAUFB" varchar(1) NOT NULL default ' ',
  "AAABAUFC" varchar(1) NOT NULL default ' ',
  "AAABAUFD" varchar(1) NOT NULL default ' ',
  "AAABAUFE" varchar(2) NOT NULL default '00',
  "AAABAUFF" varchar(2) NOT NULL default '00',
  "AAABAUFG" varchar(1) NOT NULL default ' ',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABAVWS" decimal(15,0) NOT NULL default '0',
  "AAABAVWT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABAVWW","AAABAVWX","AAABKFCZ"),
  KEY "AAABAUFP" ("AAABMRFO","AAABAVDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUFS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVWW" varchar(3) NOT NULL default ' ',
  "AAABAVWX" varchar(3) NOT NULL default ' ',
  "AAABAUGL" varchar(20) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLXIS" varchar(75) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABAUKD" varchar(8) NOT NULL default '00000000',
  "AAABAUKE" varchar(8) NOT NULL default '00000000',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABAVWS" decimal(15,0) NOT NULL default '0',
  "AAABAVWT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABAVWW","AAABAVWX","AAABAUGL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUGN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVWW" varchar(3) NOT NULL default ' ',
  "AAABAVWX" varchar(3) NOT NULL default ' ',
  "AAABAVWB" varchar(2) NOT NULL default '00',
  "AAABAVJH" varchar(17) NOT NULL default ' ',
  "AAABAUHJ" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAUGW" varchar(3) NOT NULL default '000',
  "AAABAUGX" varchar(4) NOT NULL default '0000',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABAVWS" decimal(15,0) NOT NULL default '0',
  "AAABAVWT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABAVWW","AAABAVWX","AAABAVWB","AAABAVJH","AAABAUHJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUHL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVWW" varchar(3) NOT NULL default ' ',
  "AAABAVWX" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABAUIF" varchar(3) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAUHS" varchar(3) NOT NULL default '000',
  "AAABAUHT" varchar(4) NOT NULL default '0000',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABAUHW" varchar(3) NOT NULL default ' ',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABAVWS" decimal(15,0) NOT NULL default '0',
  "AAABAVWT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABAVWW","AAABAVWX","AAABKFCZ","AAABAUIF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUIG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVWW" varchar(3) NOT NULL default ' ',
  "AAABAVWX" varchar(3) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVWW","AAABAVWX","AAABKFCV","AAABLUNJ"),
  KEY "AAABAUIT" ("AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUIV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVWW" varchar(3) NOT NULL default ' ',
  "AAABAVWX" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABAVRD" varchar(3) NOT NULL default '000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMRCK" varchar(255) NOT NULL default ' ',
  "AAABGGGH" varchar(12) NOT NULL default ' ',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABAVWS" decimal(15,0) NOT NULL default '0',
  "AAABAVWT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABAVWW","AAABAVWX","AAABKFCZ","AAABAVRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVWW" varchar(3) NOT NULL default ' ',
  "AAABAVWX" varchar(3) NOT NULL default ' ',
  "AAABFODK" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABLUZL" varchar(40) NOT NULL default ' ',
  "AAABMECK" varchar(60) NOT NULL default ' ',
  "AAABLPSU" varchar(1) NOT NULL default ' ',
  "AAABAUJY" varchar(1) NOT NULL default ' ',
  "AAABAUJZ" varchar(1) NOT NULL default ' ',
  "AAABAUKA" varchar(1) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABAUKD" varchar(8) NOT NULL default '00000000',
  "AAABAUKE" varchar(8) NOT NULL default '00000000',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABAVWS" decimal(15,0) NOT NULL default '0',
  "AAABAVWT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABAVWW","AAABAVWX","AAABFODK","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVWW" varchar(3) NOT NULL default ' ',
  "AAABAVWX" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABILZK" varchar(40) NOT NULL default ' ',
  "AAABGGGH" varchar(12) NOT NULL default ' ',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABAVWS" decimal(15,0) NOT NULL default '0',
  "AAABAVWT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABAVWW","AAABAVWX","AAABMRHS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAULG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVRC" varchar(12) NOT NULL default '000000000000',
  "AAABAVWW" varchar(3) NOT NULL default ' ',
  "AAABAVWX" varchar(3) NOT NULL default ' ',
  "AAABAVRD" varchar(6) NOT NULL default '000000',
  "AAABAULM" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVRC","AAABAVWW","AAABAVWX","AAABAVRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAULS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAUNB" varchar(3) NOT NULL default '000',
  "AAABAUNC" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABAUNB","AAABAUNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--enable_query_log
