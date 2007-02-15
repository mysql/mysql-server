# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+
# |                             tbl32.inc                              |
# |                        Copyright 2006 by MySQL AB                  |
# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+

--disable_query_log
CREATE TABLE "AAABBLXI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFPVV" varchar(4) NOT NULL default ' ',
  "AAABLUDG" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABFPVV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBLXQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOZE" varchar(6) NOT NULL default ' ',
  "AAABBLXT" varchar(1) NOT NULL default ' ',
  "AAABBLXU" varchar(1) NOT NULL default ' ',
  "AAABBLXV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBLXY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLOZE" varchar(6) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBLYG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABBMCV" varchar(4) NOT NULL default ' ',
  "AAABBMCW" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABKQYU" varchar(12) NOT NULL default ' ',
  "AAABLEER" varchar(8) NOT NULL default '00000000',
  "AAABLEES" varchar(12) NOT NULL default ' ',
  "AAABBLYR" varchar(8) NOT NULL default '00000000',
  "AAABBLYS" varchar(12) NOT NULL default ' ',
  "AAABBLYT" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMKIQ","AAABBMCV","AAABBMCW","AAABMDBB","AAABKRXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBLZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABBMCV" varchar(4) NOT NULL default ' ',
  "AAABBMCW" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAABKPXV" varchar(3) NOT NULL default '000',
  "AAABBLZZ" varchar(4) NOT NULL default '0000',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABIJTI" varchar(8) NOT NULL default '00000000',
  "AAABFSUG" varchar(10) NOT NULL default ' ',
  "AAABFSUI" varchar(8) NOT NULL default '00000000',
  "AAABFSUF" varchar(1) NOT NULL default ' ',
  "AAABBLZO" varchar(30) NOT NULL default ' ',
  "AAABBLZP" varchar(10) NOT NULL default ' ',
  "AAABBLZQ" varchar(6) NOT NULL default ' ',
  "AAABBLZR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKIQ","AAABBMCV","AAABBMCW","AAABMDBB","AAABKRXK","AAABKPXV","AAABBLZZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMAA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABBMCV" varchar(4) NOT NULL default ' ',
  "AAABBMCW" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABJVQM" varchar(12) NOT NULL default ' ',
  "AAABHAPO" varchar(24) NOT NULL default ' ',
  "AAABHAPP" varchar(24) NOT NULL default ' ',
  "AAABIAEU" varchar(34) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKIQ","AAABBMCV","AAABBMCW","AAABMDBB","AAABKRXK","AAABMKXW"),
  KEY "AAABBMAT" ("AAABMRIL","AAABIAEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMAW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBMCV" varchar(4) NOT NULL default ' ',
  "AAABBMCW" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABCCUR" varchar(3) NOT NULL default '000',
  "AAABBMCQ" varchar(3) NOT NULL default '000',
  "AAABLMAE" decimal(5,2) NOT NULL default '0.00',
  "AAABBMCS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBMCV","AAABBMCW","AAABMDBB","AAABKRXK","AAABKOTY","AAABMKXW","AAABCCUR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMBQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABBMCV" varchar(4) NOT NULL default ' ',
  "AAABBMCW" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMKIQ","AAABBMCV","AAABBMCW","AAABMDBB","AAABKRXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMCG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKIQ" varchar(8) NOT NULL default ' ',
  "AAABBMCV" varchar(4) NOT NULL default ' ',
  "AAABBMCW" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABCCUR" varchar(3) NOT NULL default '000',
  "AAABBMCQ" varchar(3) NOT NULL default '000',
  "AAABLMAE" decimal(6,2) NOT NULL default '0.00',
  "AAABBMCS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKIQ","AAABBMCV","AAABBMCW","AAABMDBB","AAABKRXK","AAABKOTY","AAABMKXW","AAABCCUR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMDC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBMGC" varchar(30) NOT NULL default ' ',
  "AAABLJNI" varchar(24) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABLUMI" varchar(8) NOT NULL default '00000000',
  "AAABEDGS" varchar(1) NOT NULL default ' ',
  "AAABBMEJ" varchar(1) NOT NULL default ' ',
  "AAABBMEK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBMGC","AAABLJNI","AAABHZPH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMDP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBMGC" varchar(30) NOT NULL default ' ',
  "AAABLJNI" varchar(24) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABLUMI" varchar(8) NOT NULL default '00000000',
  "AAABEDGS" varchar(1) NOT NULL default ' ',
  "AAABBMEJ" varchar(1) NOT NULL default ' ',
  "AAABBMEK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBMGC","AAABLJNI","AAABHZPH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMEC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBMGC" varchar(30) NOT NULL default ' ',
  "AAABLJNI" varchar(24) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABLUMI" varchar(8) NOT NULL default '00000000',
  "AAABEDGS" varchar(1) NOT NULL default ' ',
  "AAABBMEJ" varchar(1) NOT NULL default ' ',
  "AAABBMEK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBMGC","AAABLJNI","AAABHZPH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMEP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBMGC" varchar(30) NOT NULL default ' ',
  "AAABLJNI" varchar(24) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABLUMI" varchar(8) NOT NULL default '00000000',
  "AAABLPWW" varchar(30) NOT NULL default ' ',
  "AAABMOQQ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBMGC","AAABLJNI","AAABHZPH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMFD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBMGC" varchar(30) NOT NULL default ' ',
  "AAABLJNI" varchar(24) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABLUMI" varchar(8) NOT NULL default '00000000',
  "AAABLPWW" varchar(30) NOT NULL default ' ',
  "AAABMOQQ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBMGC","AAABLJNI","AAABHZPH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMFR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBMGC" varchar(30) NOT NULL default ' ',
  "AAABLJNI" varchar(24) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABLUMI" varchar(8) NOT NULL default '00000000',
  "AAABLPWW" varchar(30) NOT NULL default ' ',
  "AAABMOQQ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBMGC","AAABLJNI","AAABHZPH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMGF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABIFOV" varchar(1) NOT NULL default ' ',
  "AAABILMX" varchar(1) NOT NULL default ' ',
  "AAABBMGK" varchar(4) NOT NULL default ' ',
  "AAABBMGL" varchar(4) NOT NULL default ' ',
  "AAABBMJX" varchar(1) NOT NULL default ' ',
  "AAABBMJY" varchar(1) NOT NULL default ' ',
  "AAABKRIX" varchar(1) NOT NULL default ' ',
  "AAABKRIY" varchar(1) NOT NULL default ' ',
  "AAABKRIZ" varchar(1) NOT NULL default ' ',
  "AAABJFGP" varchar(2) NOT NULL default ' ',
  "AAABBNAH" varchar(4) NOT NULL default ' ',
  "AAABEHHL" varchar(4) NOT NULL default '0000',
  "AAABEHHM" varchar(3) NOT NULL default '000',
  "AAABBNYT" varchar(3) NOT NULL default '000',
  "AAABEIKF" varchar(1) NOT NULL default ' ',
  "AAABBMJV" varchar(7) NOT NULL default ' ',
  "AAABBMJW" varchar(7) NOT NULL default ' ',
  "AAABBMGZ" varchar(1) NOT NULL default ' ',
  "AAABEXVV" varchar(1) NOT NULL default ' ',
  "AAABBMJZ" varchar(1) NOT NULL default ' ',
  "AAABBNHQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABETQU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMHF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDIAA" varchar(1) NOT NULL default ' ',
  "AAABKYZX" varchar(1) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDIAA","AAABKYZX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMHP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBMIH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBMIH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMHU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBMIH" varchar(1) NOT NULL default ' ',
  "AAABBMHY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBMIH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMIC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABBMIH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABETQU","AAABMQHY","AAABEPWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMIM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEIQJ" varchar(5) NOT NULL default ' ',
  "AAABEIPI" varchar(1) NOT NULL default ' ',
  "AAABGIEM" varchar(1) NOT NULL default ' ',
  "AAABEIPK" varchar(1) NOT NULL default ' ',
  "AAABEIPL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEIQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMIV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEIQJ" varchar(5) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEIQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMJD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEIQJ" varchar(5) NOT NULL default ' ',
  "AAABEIQK" varchar(1) NOT NULL default '0',
  "AAABLXTF" varchar(2) NOT NULL default '00',
  "AAABMKXW" varchar(10) NOT NULL default ' ',
  "AAABJSDF" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABEIQJ","AAABEIQK","AAABLXTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMJO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABBMJS" varchar(2) NOT NULL default ' ',
  "AAABBMJT" varchar(60) NOT NULL default ' ',
  "AAABBMJU" varchar(5) NOT NULL default ' ',
  "AAABBMJV" varchar(7) NOT NULL default ' ',
  "AAABBMJW" varchar(7) NOT NULL default ' ',
  "AAABBMJX" varchar(1) NOT NULL default ' ',
  "AAABBMJY" varchar(1) NOT NULL default ' ',
  "AAABBMJZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABETQU","AAABMQHX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMKD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABBMKG" varchar(30) NOT NULL default ' ',
  "AAABEGXK" varchar(30) NOT NULL default ' ',
  "AAABEGXJ" varchar(15) NOT NULL default ' ',
  "AAABIYJL" varchar(15) NOT NULL default ' ',
  "AAABLZCP" varchar(30) NOT NULL default ' ',
  "AAABLZCR" varchar(30) NOT NULL default ' ',
  "AAABEGXD" varchar(30) NOT NULL default ' ',
  "AAABEGXE" varchar(10) NOT NULL default ' ',
  "AAABBOOL" varchar(10) NOT NULL default ' ',
  "AAABJSTP" varchar(30) NOT NULL default ' ',
  "AAABHTLE" varchar(3) NOT NULL default ' ',
  "AAABBMKR" varchar(30) NOT NULL default ' ',
  "AAABKOUG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABETQU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMKV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABBMLA" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABETQU","AAABMQHY","AAABEPWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMLF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBMLH" varchar(12) NOT NULL default ' ',
  "AAABBMLI" varchar(8) NOT NULL default '00000000',
  "AAABBMLJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMLL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABBPPK" varchar(6) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABLQVJ" varchar(2) NOT NULL default ' ',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  "AAABJKTY" varchar(3) NOT NULL default ' ',
  "AAABBMLW" varchar(6) NOT NULL default ' ',
  "AAABBNZM" varchar(1) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABBMMA" decimal(7,4) NOT NULL default '0.0000',
  "AAABEKXK" decimal(15,2) NOT NULL default '0.00',
  "AAABBNZO" varchar(1) NOT NULL default ' ',
  "AAABBNZP" varchar(9) NOT NULL default '000000000',
  "AAABBNZQ" varchar(1) NOT NULL default ' ',
  "AAABEKXL" decimal(15,2) NOT NULL default '0.00',
  "AAABEKFJ" decimal(11,8) NOT NULL default '0.00000000',
  "AAABEKFK" decimal(11,8) NOT NULL default '0.00000000',
  "AAABEKFL" decimal(11,8) NOT NULL default '0.00000000',
  "AAABEKFM" decimal(11,8) NOT NULL default '0.00000000',
  "AAABBMMK" varchar(1) NOT NULL default ' ',
  "AAABEKEJ" decimal(15,2) NOT NULL default '0.00',
  "AAABEKEK" decimal(15,2) NOT NULL default '0.00',
  "AAABBMMN" varchar(1) NOT NULL default ' ',
  "AAABEIVN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHV","AAABMQHX","AAABETQU","AAABBPPK","AAABMQHY","AAABEPWA","AAABLQVJ"),
  KEY "AAABBMMX" ("AAABBPPK","AAABMQHX","AAABMQHV","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMNC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPWC" varchar(3) NOT NULL default ' ',
  "AAABBMQD" varchar(5) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABBNBF" varchar(1) NOT NULL default ' ',
  "AAABEPUY" varchar(2) NOT NULL default ' ',
  "AAABEJKH" varchar(1) NOT NULL default ' ',
  "AAABBNNA" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPWC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMNO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWC" varchar(3) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABLLJG" decimal(7,4) NOT NULL default '0.0000',
  "AAABBMNV" varchar(1) NOT NULL default ' ',
  "AAABBMNW" varchar(1) NOT NULL default ' ',
  "AAABBMNX" varchar(1) NOT NULL default ' ',
  "AAABBMNY" varchar(1) NOT NULL default ' ',
  "AAABBMNZ" varchar(1) NOT NULL default ' ',
  "AAABLRBI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHX","AAABMQHY","AAABEPWC","AAABETQU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMOG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEJKH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEJKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMOL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEJKH" varchar(1) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEJKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMOT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEJKH" varchar(1) NOT NULL default ' ',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABBMOX" varchar(3) NOT NULL default '000',
  "AAABBMOY" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABEJKH","AAABEPWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMPC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWC" varchar(3) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABLLJG" decimal(7,4) NOT NULL default '0.0000',
  "AAABEKFJ" decimal(11,8) NOT NULL default '0.00000000',
  "AAABEKFK" decimal(11,8) NOT NULL default '0.00000000',
  "AAABEKFL" decimal(11,8) NOT NULL default '0.00000000',
  "AAABEKFM" decimal(11,8) NOT NULL default '0.00000000',
  "AAABBMPO" decimal(11,8) NOT NULL default '0.00000000',
  "AAABBMPP" varchar(1) NOT NULL default ' ',
  "AAABJFGP" varchar(1) NOT NULL default ' ',
  "AAABBMPR" varchar(1) NOT NULL default ' ',
  "AAABBMPS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHX","AAABMQHY","AAABEPWC","AAABETQU","AAABEPWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMPZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEPWC" varchar(3) NOT NULL default ' ',
  "AAABBMQD" varchar(5) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABBNBF" varchar(1) NOT NULL default ' ',
  "AAABEPUY" varchar(2) NOT NULL default ' ',
  "AAABEJKH" varchar(1) NOT NULL default ' ',
  "AAABEHLM" varchar(2) NOT NULL default '00',
  "AAABBMQJ" varchar(6) NOT NULL default ' ',
  "AAABBMQK" varchar(1) NOT NULL default ' ',
  "AAABBMQL" varchar(1) NOT NULL default ' ',
  "AAABBMQM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHX","AAABEPWC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABBMSX" varchar(1) NOT NULL default ' ',
  "AAABBMSY" varchar(8) NOT NULL default '00000000',
  "AAABBMSZ" varchar(12) NOT NULL default ' ',
  "AAABBMRA" varchar(8) NOT NULL default '00000000',
  "AAABBMRB" varchar(3) NOT NULL default ' ',
  "AAABBMRC" varchar(1) NOT NULL default ' ',
  "AAABBMRD" varchar(8) NOT NULL default '00000000',
  "AAABBMRE" varchar(12) NOT NULL default ' ',
  "AAABBMRF" varchar(1) NOT NULL default ' ',
  "AAABBMRG" varchar(8) NOT NULL default '00000000',
  "AAABBMRH" varchar(12) NOT NULL default ' ',
  "AAABHPIC" varchar(8) NOT NULL default '00000000',
  "AAABBMRJ" varchar(1) NOT NULL default ' ',
  "AAABBMRK" varchar(8) NOT NULL default '00000000',
  "AAABBMRL" varchar(12) NOT NULL default ' ',
  "AAABBMRM" varchar(1) NOT NULL default ' ',
  "AAABBMRN" varchar(8) NOT NULL default '00000000',
  "AAABBMRO" varchar(12) NOT NULL default ' ',
  "AAABBMRP" varchar(8) NOT NULL default '00000000',
  "AAABBMRQ" varchar(1) NOT NULL default ' ',
  "AAABBMRR" varchar(8) NOT NULL default '00000000',
  "AAABBMRS" varchar(12) NOT NULL default ' ',
  "AAABBMRT" varchar(3) NOT NULL default ' ',
  "AAABBMRU" varchar(31) NOT NULL default ' ',
  "AAABBMRV" varchar(1) NOT NULL default ' ',
  "AAABKSPH" varchar(8) NOT NULL default '00000000',
  "AAABBMRX" varchar(6) NOT NULL default '000000',
  "AAABBMRY" varchar(6) NOT NULL default '000000',
  "AAABHPID" varchar(6) NOT NULL default '000000',
  "AAABBMSA" varchar(6) NOT NULL default '000000',
  "AAABBMSB" varchar(1) NOT NULL default ' ',
  "AAABJFLQ" varchar(8) NOT NULL default '00000000',
  "AAABHFNY" varchar(6) NOT NULL default '000000',
  "AAABBMSE" varchar(12) NOT NULL default ' ',
  "AAABBMSF" varchar(1) NOT NULL default ' ',
  "AAABBMSG" varchar(8) NOT NULL default '00000000',
  "AAABBMSH" varchar(6) NOT NULL default '000000',
  "AAABBMSI" varchar(12) NOT NULL default ' ',
  "AAABBMSJ" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMQHV","AAABMQHX","AAABETQU","AAABMQHY","AAABEPWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEPWC" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABBMSX" varchar(1) NOT NULL default ' ',
  "AAABBMSY" varchar(8) NOT NULL default '00000000',
  "AAABBMSZ" varchar(12) NOT NULL default ' ',
  "AAABBMTA" varchar(8) NOT NULL default '00000000',
  "AAABBMTB" varchar(8) NOT NULL default '00000000',
  "AAABBMTC" varchar(8) NOT NULL default '00000000',
  "AAABBMTD" varchar(8) NOT NULL default '00000000',
  "AAABBMTE" varchar(1) NOT NULL default ' ',
  "AAABBMTF" varchar(8) NOT NULL default '00000000',
  "AAABBMTG" varchar(12) NOT NULL default ' ',
  "AAABHUQZ" varchar(8) NOT NULL default '00000000',
  "AAABBMTI" varchar(3) NOT NULL default ' ',
  "AAABBMTJ" varchar(6) NOT NULL default '000000',
  "AAABBMTK" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMQHV","AAABMQHX","AAABEPWC","AAABMQHY","AAABEPWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMTR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  "AAABBMTU" varchar(1) NOT NULL default ' ',
  "AAABBMTV" varchar(1) NOT NULL default ' ',
  "AAABBMTW" varchar(1) NOT NULL default ' ',
  "AAABBMTX" varchar(1) NOT NULL default ' ',
  "AAABBMTY" varchar(1) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABEIWW" varchar(1) NOT NULL default ' ',
  "AAABHFND" varchar(1) NOT NULL default ' ',
  "AAABBMUC" varchar(1) NOT NULL default ' ',
  "AAABBMUD" varchar(1) NOT NULL default ' ',
  "AAABBMUE" varchar(1) NOT NULL default ' ',
  "AAABBMUF" varchar(1) NOT NULL default ' ',
  "AAABBMUG" varchar(1) NOT NULL default ' ',
  "AAABBMUH" varchar(60) NOT NULL default ' ',
  "AAABBMUI" varchar(10) NOT NULL default ' ',
  "AAABBMUJ" varchar(10) NOT NULL default ' ',
  "AAABBMUK" varchar(10) NOT NULL default ' ',
  "AAABBMUL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLIMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMUO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABELFZ" varchar(3) NOT NULL default ' ',
  "AAABBMUZ" varchar(1) NOT NULL default ' ',
  "AAABBMVA" varchar(10) NOT NULL default ' ',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  "AAABJKTY" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHX","AAABELFZ","AAABBMUZ","AAABBMVA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMVB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBMWL" varchar(1) NOT NULL default ' ',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  "AAABLUVE" varchar(1) NOT NULL default ' ',
  "AAABBMVH" varchar(8) NOT NULL default ' ',
  "AAABLUDG" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBMWL","AAABLIMF","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMVO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  "AAABLUDG" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLIMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMVW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFGP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFGP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMWB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJFGP" varchar(1) NOT NULL default ' ',
  "AAABBMWL" varchar(1) NOT NULL default ' ',
  "AAABLQVJ" varchar(2) NOT NULL default ' ',
  "AAABLUDG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABJFGP","AAABBMWL","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMWN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDER" varchar(3) NOT NULL default ' ',
  "AAABBMWQ" varchar(3) NOT NULL default ' ',
  "AAABBMWR" varchar(3) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABBMWT" varchar(1) NOT NULL default ' ',
  "AAABBMWU" varchar(75) NOT NULL default ' ',
  "AAABEGLU" varchar(1) NOT NULL default ' ',
  "AAABEGLW" varchar(1) NOT NULL default ' ',
  "AAABBMWX" varchar(1) NOT NULL default ' ',
  "AAABBMWY" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMXB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABMDER" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZBM","AAABMCSO","AAABMPLS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMXL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABLHRD" varchar(4) NOT NULL default ' ',
  "AAABMDER" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCFC","AAABLHRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMXT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMDER" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMDER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMYB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBMYP" varchar(3) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABBMYF" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBMYP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMYI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBMYP" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBMYP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMYQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBNEB" varchar(4) NOT NULL default ' ',
  "AAABLUVE" varchar(4) NOT NULL default ' ',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  "AAABBNKX" varchar(2) NOT NULL default ' ',
  "AAABLUDG" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBNEB","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBNEC" varchar(4) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default ' ',
  "AAABMMRH" varchar(1) NOT NULL default '0',
  "AAABBNIW" varchar(3) NOT NULL default '000',
  "AAABBMZH" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBNEC","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMZN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBNAH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBNAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMZS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBNAH" varchar(4) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBNAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNAA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBNAH" varchar(4) NOT NULL default ' ',
  "AAABLUVE" varchar(4) NOT NULL default ' ',
  "AAABMOIA" varchar(8) NOT NULL default ' ',
  "AAABBNEY" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBNAH","AAABLUVE","AAABMOIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNAK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBNBF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBNBF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNAP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBNBF" varchar(1) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBNBF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNAX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBNBF" varchar(1) NOT NULL default ' ',
  "AAABLUVE" varchar(4) NOT NULL default ' ',
  "AAABGPKK" varchar(1) NOT NULL default ' ',
  "AAABBNBC" varchar(39) NOT NULL default ' ',
  "AAABLSIN" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBNBF","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNBH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBNEC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBNEC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNBM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBNEC" varchar(4) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBNEC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNBU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBNEB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBNEB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNBZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBNEB" varchar(4) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBNEB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNCH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEGLU" varchar(1) NOT NULL default ' ',
  "AAABBOQQ" varchar(1) NOT NULL default ' ',
  "AAABBPJI" varchar(3) NOT NULL default ' ',
  "AAABBPJJ" varchar(3) NOT NULL default ' ',
  "AAABBNCO" varchar(3) NOT NULL default ' ',
  "AAABBNCP" varchar(3) NOT NULL default ' ',
  "AAABBNCQ" varchar(3) NOT NULL default ' ',
  "AAABBNCR" varchar(3) NOT NULL default ' ',
  "AAABBNCS" varchar(3) NOT NULL default ' ',
  "AAABBNCT" varchar(3) NOT NULL default ' ',
  "AAABBNCU" varchar(3) NOT NULL default ' ',
  "AAABBNCV" varchar(3) NOT NULL default ' ',
  "AAABBNCW" varchar(3) NOT NULL default ' ',
  "AAABDROT" varchar(1) NOT NULL default ' ',
  "AAABBNCY" varchar(3) NOT NULL default ' ',
  "AAABBNCZ" varchar(1) NOT NULL default ' ',
  "AAABBNDA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNDD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEPVS" varchar(3) NOT NULL default ' ',
  "AAABBNDI" varchar(1) NOT NULL default ' ',
  "AAABBNDJ" varchar(2) NOT NULL default '00',
  "AAABKRXK" varchar(4) NOT NULL default ' ',
  "AAABBNDL" varchar(8) NOT NULL default ' ',
  "AAABBNDM" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHV","AAABMQHX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNDQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMQHX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNDY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBNEY" varchar(12) NOT NULL default ' ',
  "AAABBNEB" varchar(4) NOT NULL default ' ',
  "AAABBNEC" varchar(4) NOT NULL default ' ',
  "AAABBNLL" varchar(1) NOT NULL default ' ',
  "AAABBNLM" varchar(1) NOT NULL default ' ',
  "AAABBNEF" varchar(1) NOT NULL default ' ',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABFRGS" varchar(1) NOT NULL default ' ',
  "AAABBNEI" varchar(3) NOT NULL default ' ',
  "AAABLDNL" varchar(4) NOT NULL default ' ',
  "AAABBNEK" varchar(1) NOT NULL default ' ',
  "AAABBNEL" varchar(1) NOT NULL default ' ',
  "AAABBNXD" varchar(1) NOT NULL default ' ',
  "AAABKYUV" varchar(2) NOT NULL default ' ',
  "AAABBNEO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBNEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNER" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBNEY" varchar(12) NOT NULL default ' ',
  "AAABBNNP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBNEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNEZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPWM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPWM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNFE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEPWM" varchar(10) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEPWM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNFM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPWN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNFR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEPWN" varchar(10) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEPWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNFZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPWO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNGE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEPWO" varchar(10) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEPWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNGM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBNHQ" varchar(4) NOT NULL default ' ',
  "AAABBNGT" varchar(12) NOT NULL default ' ',
  "AAABBNHC" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBNHQ","AAABBNGT","AAABBNHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNGV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBNHQ" varchar(4) NOT NULL default ' ',
  "AAABBNHC" varchar(12) NOT NULL default ' ',
  "AAABBNHD" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBNHQ","AAABBNHC","AAABBNHD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBNHQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBNHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNHJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBNHQ" varchar(4) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBNHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNHR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBNLR" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBNLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNHW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBNLR" varchar(12) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBNLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNIE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBNLK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBNLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNIJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBNLK" varchar(4) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBNLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNIR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBNLK" varchar(4) NOT NULL default ' ',
  "AAABLUVE" varchar(4) NOT NULL default ' ',
  "AAABMMRH" varchar(2) NOT NULL default '00',
  "AAABBNIW" varchar(4) NOT NULL default ' ',
  "AAABDBGB" varchar(2) NOT NULL default ' ',
  "AAABBNOM" varchar(6) NOT NULL default ' ',
  "AAABBNIZ" varchar(2) NOT NULL default ' ',
  "AAABLLBG" varchar(6) NOT NULL default ' ',
  "AAABBNJB" varchar(12) NOT NULL default ' ',
  "AAABBNMA" varchar(2) NOT NULL default '00',
  "AAABEJKE" varchar(1) NOT NULL default ' ',
  "AAABLOZV" varchar(1) NOT NULL default '0',
  "AAABBNJF" varchar(1) NOT NULL default ' ',
  "AAABMCWF" varchar(1) NOT NULL default ' ',
  "AAABBNJH" varchar(1) NOT NULL default ' ',
  "AAABKMGT" varchar(2) NOT NULL default ' ',
  "AAABLSIN" varchar(5) NOT NULL default ' ',
  "AAABBNJK" varchar(40) NOT NULL default ' ',
  "AAABIVOQ" varchar(1) NOT NULL default ' ',
  "AAABBNJM" varchar(3) NOT NULL default '000',
  "AAABBNJN" varchar(1) NOT NULL default ' ',
  "AAABBNJO" varchar(1) NOT NULL default ' ',
  "AAABBNJP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBNLK","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNJT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBNNS" varchar(12) NOT NULL default ' ',
  "AAABMMRH" varchar(1) NOT NULL default ' ',
  "AAABEGLB" varchar(79) NOT NULL default ' ',
  "AAABEGLC" varchar(79) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBNNS","AAABMMRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNKE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBNLI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBNLI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNKJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBNLI" varchar(4) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBNLI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNKR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBNLI" varchar(4) NOT NULL default ' ',
  "AAABLUVE" varchar(4) NOT NULL default ' ',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABBNKX" varchar(2) NOT NULL default ' ',
  "AAABGOWB" varchar(10) NOT NULL default ' ',
  "AAABBNKZ" varchar(10) NOT NULL default ' ',
  "AAABBNLA" varchar(1) NOT NULL default ' ',
  "AAABBNLB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBNLI","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNLF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBNNS" varchar(12) NOT NULL default ' ',
  "AAABBNLI" varchar(4) NOT NULL default ' ',
  "AAABBNLJ" varchar(4) NOT NULL default ' ',
  "AAABBNLK" varchar(4) NOT NULL default ' ',
  "AAABBNLL" varchar(1) NOT NULL default ' ',
  "AAABBNLM" varchar(1) NOT NULL default ' ',
  "AAABBNLN" varchar(1) NOT NULL default ' ',
  "AAABBNLO" varchar(5) NOT NULL default ' ',
  "AAABKYUV" varchar(2) NOT NULL default ' ',
  "AAABKOFE" decimal(3,0) NOT NULL default '0',
  "AAABBNLR" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBNNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNLU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPUY" varchar(2) NOT NULL default ' ',
  "AAABLUVE" varchar(4) NOT NULL default ' ',
  "AAABBNNS" varchar(12) NOT NULL default ' ',
  "AAABBNLZ" varchar(1) NOT NULL default '0',
  "AAABBNMA" varchar(40) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABBNQD" varchar(1) NOT NULL default ' ',
  "AAABJKTY" varchar(3) NOT NULL default ' ',
  "AAABBNME" varchar(1) NOT NULL default ' ',
  "AAABBNMF" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  "AAABBNMI" varchar(1) NOT NULL default ' ',
  "AAABDBGB" varchar(1) NOT NULL default ' ',
  "AAABBNMK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPUY","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNMO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBNNA" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBNNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNMT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBNNA" varchar(2) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBNNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNNB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  "AAABLUVE" varchar(4) NOT NULL default ' ',
  "AAABMOIA" varchar(8) NOT NULL default ' ',
  "AAABMLFD" varchar(14) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLIMF","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNNL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBNNS" varchar(12) NOT NULL default ' ',
  "AAABBNNP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBNNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNNT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBNOM" varchar(6) NOT NULL default ' ',
  "AAABLUDG" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBNOM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBNOM" varchar(6) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABBPAG" varchar(2) NOT NULL default '00',
  "AAABBOMR" varchar(3) NOT NULL default ' ',
  "AAABBOMS" varchar(3) NOT NULL default ' ',
  "AAABJFEC" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBNOM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNON" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPWC" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABBOMR" varchar(3) NOT NULL default '000',
  "AAABBOMS" varchar(3) NOT NULL default '000',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABICGD" varchar(8) NOT NULL default '00000000',
  "AAABICGE" varchar(6) NOT NULL default '000000',
  "AAABBNPT" varchar(1) NOT NULL default ' ',
  "AAABBNPU" varchar(1) NOT NULL default ' ',
  "AAABBNOY" varchar(1) NOT NULL default ' ',
  "AAABBNOZ" varchar(1) NOT NULL default ' ',
  "AAABKQPS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPWC","AAABMQHY","AAABBOMR","AAABBOMS","AAABMQHX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPWC" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABBOMR" varchar(3) NOT NULL default '000',
  "AAABBOMS" varchar(3) NOT NULL default '000',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABBNQD" varchar(1) NOT NULL default ' ',
  "AAABBNPQ" varchar(5) NOT NULL default ' ',
  "AAABBNPR" varchar(2) NOT NULL default ' ',
  "AAABBNPS" varchar(8) NOT NULL default '00000000',
  "AAABBNPT" varchar(1) NOT NULL default ' ',
  "AAABBNPU" varchar(1) NOT NULL default ' ',
  "AAABBNPV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPWC","AAABMQHY","AAABBOMR","AAABBOMS","AAABMQHX","AAABMQHW","AAABBNQD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNQE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPWC" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABBOMR" varchar(3) NOT NULL default '000',
  "AAABBOMS" varchar(3) NOT NULL default '000',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABLYCI" varchar(2) NOT NULL default ' ',
  "AAABLBFZ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPWC","AAABMQHY","AAABBOMR","AAABBOMS","AAABMQHX","AAABLYCI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNQU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  "AAABMDER" varchar(3) NOT NULL default ' ',
  "AAABBPPK" varchar(6) NOT NULL default ' ',
  "AAABELFT" varchar(10) NOT NULL default ' ',
  "AAABELFQ" varchar(10) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABEJKE" decimal(7,4) NOT NULL default '0.0000',
  "AAABBNRG" varchar(1) NOT NULL default ' ',
  "AAABBNRH" varchar(3) NOT NULL default ' ',
  "AAABBNRI" varchar(3) NOT NULL default ' ',
  "AAABBNRJ" varchar(1) NOT NULL default ' ',
  "AAABBNRK" varchar(1) NOT NULL default ' ',
  "AAABBNRL" varchar(1) NOT NULL default ' ',
  "AAABBNRM" varchar(10) NOT NULL default ' ',
  "AAABBNRN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHY","AAABEPWA","AAABMQHX","AAABLIMF","AAABMDER","AAABBPPK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNRV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABBNSZ" varchar(4) NOT NULL default ' ',
  "AAABBNSB" varchar(10) NOT NULL default ' ',
  "AAABBNSC" varchar(10) NOT NULL default ' ',
  "AAABBNSD" varchar(10) NOT NULL default ' ',
  "AAABBNSE" varchar(10) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABEJFS" varchar(1) NOT NULL default ' ',
  "AAABBNSH" varchar(1) NOT NULL default ' ',
  "AAABBNSI" varchar(1) NOT NULL default ' ',
  "AAABJKTY" varchar(1) NOT NULL default ' ',
  "AAABKOMQ" varchar(1) NOT NULL default ' ',
  "AAABBNSL" varchar(1) NOT NULL default ' ',
  "AAABBNSM" varchar(5) NOT NULL default ' ',
  "AAABBNSN" varchar(5) NOT NULL default ' ',
  "AAABBNSO" varchar(1) NOT NULL default ' ',
  "AAABBNSP" varchar(3) NOT NULL default ' ',
  "AAABBNSQ" varchar(3) NOT NULL default ' ',
  "AAABBNSR" varchar(3) NOT NULL default ' ',
  "AAABBNSS" varchar(3) NOT NULL default ' ',
  "AAABBNST" varchar(12) NOT NULL default ' ',
  "AAABBNSU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHY","AAABEPWA","AAABMQHX","AAABBNSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNTA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABBNTF" varchar(10) NOT NULL default ' ',
  "AAABBNTG" varchar(10) NOT NULL default ' ',
  "AAABBNTH" varchar(10) NOT NULL default ' ',
  "AAABBNTI" varchar(10) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABJKTY" varchar(3) NOT NULL default ' ',
  "AAABDJGM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHX","AAABLIMF","AAABMQHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNTQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBNWA" varchar(5) NOT NULL default ' ',
  "AAABJKTY" varchar(3) NOT NULL default ' ',
  "AAABEPWM" varchar(10) NOT NULL default ' ',
  "AAABEPWN" varchar(10) NOT NULL default ' ',
  "AAABEPWO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBNWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNTZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  "AAABBNWA" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLIMF","AAABBNWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNUG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBNWA" varchar(5) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBNWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNUO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABEPVE" varchar(10) NOT NULL default ' ',
  "AAABBNWA" varchar(5) NOT NULL default ' ',
  "AAABBPPK" varchar(6) NOT NULL default ' ',
  "AAABBNUX" decimal(15,2) NOT NULL default '0.00',
  "AAABJDMI" decimal(13,3) NOT NULL default '0.000',
  "AAABBNUZ" decimal(15,2) NOT NULL default '0.00',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABBNVB" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABMQHX","AAABMQHY","AAABEPWA","AAABETQU","AAABEPVE","AAABBNWA","AAABBPPK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNVK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABBPPK" varchar(6) NOT NULL default ' ',
  "AAABEPVE" varchar(10) NOT NULL default ' ',
  "AAABBNWA" varchar(5) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABEJQW" decimal(7,4) NOT NULL default '0.0000',
  "AAABLUII" varchar(1) NOT NULL default ' ',
  "AAABBNVV" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMQHX","AAABBPPK","AAABEPVE","AAABBNWA","AAABETQU","AAABMQHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNWD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEIQJ" varchar(5) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABMADG" varchar(1) NOT NULL default ' ',
  "AAABBNWI" varchar(2) NOT NULL default '00',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABBNWK" varchar(1) NOT NULL default ' ',
  "AAABBNWL" varchar(10) NOT NULL default ' ',
  "AAABBNWM" varchar(10) NOT NULL default ' ',
  "AAABBNWN" varchar(3) NOT NULL default ' ',
  "AAABBNWO" varchar(3) NOT NULL default ' ',
  "AAABBNWP" varchar(3) NOT NULL default ' ',
  "AAABBNWQ" varchar(3) NOT NULL default ' ',
  "AAABBNWR" varchar(3) NOT NULL default ' ',
  "AAABBNWS" varchar(3) NOT NULL default ' ',
  "AAABBNWT" varchar(3) NOT NULL default ' ',
  "AAABBNWU" varchar(3) NOT NULL default ' ',
  "AAABBNWV" varchar(10) NOT NULL default ' ',
  "AAABBNWW" varchar(10) NOT NULL default ' ',
  "AAABEIVN" varchar(1) NOT NULL default ' ',
  "AAABBNWY" varchar(1) NOT NULL default ' ',
  "AAABBNWZ" varchar(1) NOT NULL default ' ',
  "AAABBNXA" varchar(1) NOT NULL default '0',
  "AAABBNXB" varchar(1) NOT NULL default ' ',
  "AAABBNXC" varchar(1) NOT NULL default ' ',
  "AAABBNXD" varchar(12) NOT NULL default ' ',
  "AAABBNXE" varchar(12) NOT NULL default ' ',
  "AAABLSKC" varchar(7) NOT NULL default ' ',
  "AAABBNXG" varchar(10) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABIGGP" varchar(1) NOT NULL default ' ',
  "AAABBNXJ" varchar(8) NOT NULL default '00000000',
  "AAABBNXK" varchar(12) NOT NULL default ' ',
  "AAABBNXL" varchar(3) NOT NULL default ' ',
  "AAABBNXM" varchar(3) NOT NULL default ' ',
  "AAABBNXN" varchar(3) NOT NULL default ' ',
  "AAABBNXO" varchar(1) NOT NULL default ' ',
  "AAABBNXP" varchar(12) NOT NULL default ' ',
  "AAABBNXQ" varchar(12) NOT NULL default ' ',
  "AAABBNXR" varchar(12) NOT NULL default ' ',
  "AAABBNZM" varchar(1) NOT NULL default ' ',
  "AAABBNXT" varchar(3) NOT NULL default ' ',
  "AAABEKZP" varchar(1) NOT NULL default ' ',
  "AAABBNXV" varchar(12) NOT NULL default ' ',
  "AAABBNXW" varchar(12) NOT NULL default ' ',
  "AAABBNXX" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEIQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNYA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEIQJ" varchar(5) NOT NULL default ' ',
  "AAABMJVD" varchar(20) NOT NULL default ' ',
  "AAABMJVE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEIQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNYJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEPWC" varchar(3) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEIQJ" varchar(5) NOT NULL default ' ',
  "AAABIJAF" decimal(7,4) NOT NULL default '0.0000',
  "AAABEHHL" varchar(4) NOT NULL default '0000',
  "AAABEHHM" varchar(3) NOT NULL default '000',
  "AAABBNYT" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMQHX","AAABEPWC","AAABETQU","AAABMQHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBNYZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  "AAABLQVJ" varchar(2) NOT NULL default ' ',
  "AAABBNZI" varchar(1) NOT NULL default ' ',
  "AAABJKTY" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABBNZM" varchar(1) NOT NULL default ' ',
  "AAABMYBJ" decimal(15,2) NOT NULL default '0.00',
  "AAABBNZO" varchar(1) NOT NULL default ' ',
  "AAABBNZP" varchar(9) NOT NULL default '000000000',
  "AAABBNZQ" varchar(1) NOT NULL default ' ',
  "AAABBNZR" decimal(15,2) NOT NULL default '0.00',
  "AAABEIVN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHV","AAABMQHX","AAABETQU","AAABMQHY","AAABEPWA","AAABLIMF","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOAB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRXE" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABBPPK" varchar(6) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABLQVJ" varchar(2) NOT NULL default ' ',
  "AAABBOBP" varchar(1) NOT NULL default ' ',
  "AAABBOAL" decimal(15,2) NOT NULL default '0.00',
  "AAABBOIT" varchar(1) NOT NULL default ' ',
  "AAABBOIU" varchar(9) NOT NULL default '000000000',
  "AAABBOCY" varchar(1) NOT NULL default ' ',
  "AAABBOAP" decimal(15,2) NOT NULL default '0.00',
  "AAABBOBS" varchar(1) NOT NULL default ' ',
  "AAABBOAR" decimal(15,2) NOT NULL default '0.00',
  "AAABBOIV" varchar(1) NOT NULL default ' ',
  "AAABBOIW" varchar(9) NOT NULL default '000000000',
  "AAABBODB" varchar(1) NOT NULL default ' ',
  "AAABBOAV" decimal(15,2) NOT NULL default '0.00',
  "AAABBOBV" varchar(1) NOT NULL default ' ',
  "AAABBOAX" decimal(15,2) NOT NULL default '0.00',
  "AAABBOAY" varchar(1) NOT NULL default ' ',
  "AAABBOAZ" varchar(9) NOT NULL default '000000000',
  "AAABBOBA" varchar(1) NOT NULL default ' ',
  "AAABBOBB" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABKRXE","AAABMQHX","AAABBPPK","AAABETQU","AAABMQHY","AAABEPWA","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOBK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABBPPK" varchar(6) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABBOBP" varchar(1) NOT NULL default ' ',
  "AAABILQM" decimal(15,2) NOT NULL default '0.00',
  "AAABEGZH" decimal(15,2) NOT NULL default '0.00',
  "AAABBOBS" varchar(1) NOT NULL default ' ',
  "AAABILQN" decimal(15,2) NOT NULL default '0.00',
  "AAABEGZI" decimal(15,2) NOT NULL default '0.00',
  "AAABBOBV" varchar(1) NOT NULL default ' ',
  "AAABILQO" decimal(15,2) NOT NULL default '0.00',
  "AAABEHEK" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMQHX","AAABBPPK","AAABETQU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOCC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRXE" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  "AAABEPVK" varchar(4) NOT NULL default ' ',
  "AAABEPWC" varchar(3) NOT NULL default ' ',
  "AAABMAZB" varchar(1) NOT NULL default '0',
  "AAABBOCN" decimal(15,2) NOT NULL default '0.00',
  "AAABBOCO" decimal(15,2) NOT NULL default '0.00',
  "AAABBOCP" decimal(15,2) NOT NULL default '0.00',
  "AAABBOCQ" decimal(15,2) NOT NULL default '0.00',
  "AAABBOFR" varchar(1) NOT NULL default ' ',
  "AAABBOFS" decimal(7,4) NOT NULL default '0.0000',
  "AAABBOFT" varchar(3) NOT NULL default '000',
  "AAABBOEM" varchar(2) NOT NULL default '00',
  "AAABKCIS" varchar(10) NOT NULL default ' ',
  "AAABBOIT" varchar(1) NOT NULL default ' ',
  "AAABBOIU" varchar(9) NOT NULL default '000000000',
  "AAABBOCY" varchar(1) NOT NULL default ' ',
  "AAABBOIV" varchar(1) NOT NULL default ' ',
  "AAABBOIW" varchar(9) NOT NULL default '000000000',
  "AAABBODB" varchar(1) NOT NULL default ' ',
  "AAABLUDG" varchar(40) NOT NULL default ' ',
  "AAABBOEB" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABKRXE","AAABMQHX","AAABETQU","AAABMQHY","AAABEPWA","AAABLIMF","AAABEPVK","AAABEPWC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBODN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRXE" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEPWC" varchar(3) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABBPPK" varchar(6) NOT NULL default ' ',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  "AAABEPVK" varchar(4) NOT NULL default ' ',
  "AAABEHHL" varchar(4) NOT NULL default '0000',
  "AAABEHHM" varchar(2) NOT NULL default '00',
  "AAABEKQV" varchar(4) NOT NULL default '0000',
  "AAABEKQW" varchar(3) NOT NULL default '000',
  "AAABBOFT" varchar(3) NOT NULL default '000',
  "AAABBOEB" varchar(3) NOT NULL default '000',
  "AAABBOFR" varchar(1) NOT NULL default ' ',
  "AAABBOFS" decimal(7,4) NOT NULL default '0.0000',
  "AAABBOEE" decimal(15,2) NOT NULL default '0.00',
  "AAABBOEF" decimal(15,2) NOT NULL default '0.00',
  "AAABBOGR" decimal(15,2) NOT NULL default '0.00',
  "AAABBOEH" decimal(15,2) NOT NULL default '0.00',
  "AAABJCHR" decimal(15,2) NOT NULL default '0.00',
  "AAABBOEJ" decimal(15,2) NOT NULL default '0.00',
  "AAABBOGS" decimal(15,2) NOT NULL default '0.00',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABBOEM" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABKRXE","AAABMQHX","AAABEPWC","AAABETQU","AAABBPPK","AAABLIMF","AAABEPVK","AAABEHHL","AAABEHHM","AAABEKQV","AAABEKQW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOEZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABBPPK" varchar(6) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABBOIJ" decimal(15,2) NOT NULL default '0.00',
  "AAABBOIT" varchar(1) NOT NULL default ' ',
  "AAABBOIU" varchar(9) NOT NULL default '000000000',
  "AAABBOIG" decimal(15,2) NOT NULL default '0.00',
  "AAABBOIH" decimal(15,2) NOT NULL default '0.00',
  "AAABBOIV" varchar(1) NOT NULL default ' ',
  "AAABBOIW" varchar(9) NOT NULL default '000000000',
  "AAABBOIE" decimal(15,2) NOT NULL default '0.00',
  "AAABBOFR" varchar(1) NOT NULL default ' ',
  "AAABBOFS" decimal(7,4) NOT NULL default '0.0000',
  "AAABBOFT" varchar(3) NOT NULL default '000',
  "AAABBOFU" varchar(4) NOT NULL default '0000',
  "AAABBOFV" varchar(10) NOT NULL default ' ',
  "AAABBOFW" varchar(50) NOT NULL default ' ',
  "AAABBOIF" decimal(15,2) NOT NULL default '0.00',
  "AAABBOII" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMQHV","AAABMQHX","AAABETQU","AAABMQVT","AAABMQVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOGF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABEKQV" varchar(4) NOT NULL default '0000',
  "AAABBPPK" varchar(6) NOT NULL default ' ',
  "AAABBOGQ" decimal(15,2) NOT NULL default '0.00',
  "AAABBOGR" decimal(15,2) NOT NULL default '0.00',
  "AAABBOGS" decimal(15,2) NOT NULL default '0.00',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHV","AAABMQHX","AAABETQU","AAABMQHY","AAABEPWA","AAABMQVT","AAABMQVU","AAABEKQV","AAABBPPK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABBPPK" varchar(6) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABBOJF" varchar(4) NOT NULL default ' ',
  "AAABBOHM" varchar(4) NOT NULL default '0000',
  "AAABBOHN" varchar(3) NOT NULL default '000',
  "AAABBOHO" varchar(1) NOT NULL default ' ',
  "AAABBOHP" decimal(7,4) NOT NULL default '0.0000',
  "AAABBOHQ" varchar(3) NOT NULL default '000',
  "AAABBOHR" varchar(3) NOT NULL default '000',
  "AAABBOHS" varchar(10) NOT NULL default ' ',
  "AAABBOHT" varchar(10) NOT NULL default ' ',
  "AAABMQVU" varchar(12) NOT NULL default ' ',
  "AAABBOHV" varchar(4) NOT NULL default ' ',
  "AAABBOHW" varchar(4) NOT NULL default '0000',
  "AAABBOHX" varchar(3) NOT NULL default '000',
  "AAABBOHY" varchar(1) NOT NULL default ' ',
  "AAABBOHZ" decimal(7,4) NOT NULL default '0.0000',
  "AAABBOIA" varchar(3) NOT NULL default '000',
  "AAABBOIB" varchar(3) NOT NULL default '000',
  "AAABBOIC" varchar(10) NOT NULL default ' ',
  "AAABBOID" varchar(10) NOT NULL default ' ',
  "AAABBOIE" decimal(15,2) NOT NULL default '0.00',
  "AAABBOIF" decimal(15,2) NOT NULL default '0.00',
  "AAABBOIG" decimal(15,2) NOT NULL default '0.00',
  "AAABBOIH" decimal(15,2) NOT NULL default '0.00',
  "AAABBOII" decimal(15,2) NOT NULL default '0.00',
  "AAABBOIJ" decimal(15,2) NOT NULL default '0.00',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABBOIM" varchar(1) NOT NULL default ' ',
  "AAABBOIN" varchar(10) NOT NULL default ' ',
  "AAABBOIO" varchar(10) NOT NULL default ' ',
  "AAABBOIP" varchar(1) NOT NULL default ' ',
  "AAABBOIQ" varchar(4) NOT NULL default '0000',
  "AAABBOIR" varchar(3) NOT NULL default '000',
  "AAABBOIS" varchar(1) NOT NULL default ' ',
  "AAABBOIT" varchar(1) NOT NULL default ' ',
  "AAABBOIU" varchar(9) NOT NULL default '000000000',
  "AAABBOIV" varchar(1) NOT NULL default ' ',
  "AAABBOIW" varchar(9) NOT NULL default '000000000',
  "AAABBOIX" varchar(50) NOT NULL default ' ',
  "AAABBOIY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHX","AAABMQHV","AAABETQU","AAABBPPK","AAABMQVT","AAABBOJF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOJG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABBPPK" varchar(6) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABBOJQ" decimal(15,2) NOT NULL default '0.00',
  "AAABBOJR" decimal(15,2) NOT NULL default '0.00',
  "AAABBOJS" decimal(15,2) NOT NULL default '0.00',
  "AAABBOJT" decimal(15,2) NOT NULL default '0.00',
  "AAABBOJU" decimal(15,2) NOT NULL default '0.00',
  "AAABBOJV" decimal(15,2) NOT NULL default '0.00',
  "AAABBOJW" decimal(15,2) NOT NULL default '0.00',
  "AAABBOJX" decimal(15,2) NOT NULL default '0.00',
  "AAABBOJY" decimal(15,2) NOT NULL default '0.00',
  "AAABBOJZ" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMQHX","AAABMQHV","AAABETQU","AAABBPPK","AAABMQHY","AAABEPWA","AAABMQVT","AAABMQVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOKJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEIVN" varchar(1) NOT NULL default ' ',
  "AAABBPGY" varchar(1) NOT NULL default ' ',
  "AAABKYSQ" varchar(1) NOT NULL default ' ',
  "AAABBPGP" varchar(1) NOT NULL default ' ',
  "AAABBPGQ" varchar(1) NOT NULL default ' ',
  "AAABBOKS" varchar(1) NOT NULL default ' ',
  "AAABBOKT" varchar(1) NOT NULL default ' ',
  "AAABHNDA" varchar(1) NOT NULL default ' ',
  "AAABBPGR" varchar(1) NOT NULL default ' ',
  "AAABBOKW" varchar(1) NOT NULL default ' ',
  "AAABEIVT" varchar(1) NOT NULL default ' ',
  "AAABEIVU" varchar(1) NOT NULL default ' ',
  "AAABBPHF" varchar(2) NOT NULL default ' ',
  "AAABEJFS" varchar(1) NOT NULL default ' ',
  "AAABEIVY" varchar(1) NOT NULL default ' ',
  "AAABEIVW" varchar(1) NOT NULL default ' ',
  "AAABEIVX" varchar(1) NOT NULL default ' ',
  "AAABEIXP" varchar(1) NOT NULL default ' ',
  "AAABBOLF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQBR","AAABMQHX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOLJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABBOLM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOLP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMQBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOLX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMPMH" varchar(4) NOT NULL default '0000',
  "AAABFIGO" varchar(7) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMQHV","AAABMQHX","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOMI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPUY" varchar(2) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default ' ',
  "AAABMOIA" varchar(8) NOT NULL default ' ',
  "AAABBOMN" varchar(2) NOT NULL default ' ',
  "AAABLCOI" varchar(1) NOT NULL default '0',
  "AAABBOMP" varchar(1) NOT NULL default ' ',
  "AAABBOMQ" varchar(1) NOT NULL default '0',
  "AAABBOMR" varchar(3) NOT NULL default '000',
  "AAABBOMS" varchar(3) NOT NULL default '000',
  "AAABBOMT" varchar(25) NOT NULL default ' ',
  "AAABBOMU" varchar(1) NOT NULL default '0',
  "AAABEJFS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPUY","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOMZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPUY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBONE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEPUY" varchar(2) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEPUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBONM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABLTKC" varchar(2) NOT NULL default ' ',
  "AAABLSIN" varchar(5) NOT NULL default ' ',
  "AAABLUDG" varchar(68) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABLTKC","AAABLSIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBONW" (
  "AAABBOOA" varchar(2) NOT NULL default ' ',
  "AAABIOAE" varchar(12) NOT NULL default ' ',
  "AAABEIKL" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABBOOA","AAABIOAE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOOC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABLZCP" varchar(30) NOT NULL default ' ',
  "AAABHTLE" varchar(3) NOT NULL default ' ',
  "AAABLZCR" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEGXD" varchar(30) NOT NULL default ' ',
  "AAABEGXE" varchar(10) NOT NULL default ' ',
  "AAABBOOL" varchar(10) NOT NULL default ' ',
  "AAABJSTP" varchar(30) NOT NULL default ' ',
  "AAABJABF" varchar(5) NOT NULL default ' ',
  "AAABBOOO" varchar(4) NOT NULL default ' ',
  "AAABBPDH" varchar(1) NOT NULL default ' ',
  "AAABBOOQ" varchar(1) NOT NULL default ' ',
  "AAABMREO" varchar(30) NOT NULL default ' ',
  "AAABBOOS" varchar(30) NOT NULL default ' ',
  "AAABBOOT" varchar(4) NOT NULL default ' ',
  "AAABBOOU" varchar(3) NOT NULL default '000',
  "AAABBOOV" varchar(1) NOT NULL default ' ',
  "AAABBOOW" varchar(30) NOT NULL default ' ',
  "AAABBOOX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABETQU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOPA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMPVB" varchar(3) NOT NULL default '000',
  "AAABBOPG" varchar(5) NOT NULL default ' ',
  "AAABBOPH" varchar(5) NOT NULL default ' ',
  "AAABBOPI" varchar(5) NOT NULL default ' ',
  "AAABBOPJ" varchar(5) NOT NULL default ' ',
  "AAABLENG" varchar(5) NOT NULL default ' ',
  "AAABLOZV" varchar(1) NOT NULL default '0',
  "AAABBOPM" varchar(5) NOT NULL default ' ',
  "AAABBOPN" varchar(5) NOT NULL default ' ',
  "AAABBOPO" varchar(1) NOT NULL default '0',
  "AAABEGZY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHX","AAABETQU","AAABMQHY","AAABMPVB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOPV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABBOQC" varchar(4) NOT NULL default ' ',
  "AAABBOPZ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABETQU","AAABBOQC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOQD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABBPGQ" varchar(5) NOT NULL default ' ',
  "AAABLOZE" varchar(1) NOT NULL default ' ',
  "AAABMMVU" varchar(1) NOT NULL default ' ',
  "AAABBVTR" varchar(1) NOT NULL default ' ',
  "AAABBVTT" varchar(1) NOT NULL default ' ',
  "AAABBVTV" varchar(1) NOT NULL default ' ',
  "AAABBVTW" varchar(1) NOT NULL default ' ',
  "AAABJDMI" varchar(1) NOT NULL default ' ',
  "AAABBVTY" varchar(1) NOT NULL default ' ',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABBOQQ" varchar(1) NOT NULL default ' ',
  "AAABMNEA" varchar(1) NOT NULL default ' ',
  "AAABBPDH" varchar(1) NOT NULL default ' ',
  "AAABBOQT" varchar(1) NOT NULL default ' ',
  "AAABLMJR" varchar(2) NOT NULL default ' ',
  "AAABBOQV" varchar(1) NOT NULL default ' ',
  "AAABEXIM" varchar(2) NOT NULL default ' ',
  "AAABMCOH" varchar(2) NOT NULL default ' ',
  "AAABMCOI" varchar(2) NOT NULL default ' ',
  "AAABBOQZ" varchar(1) NOT NULL default ' ',
  "AAABBORA" varchar(1) NOT NULL default ' ',
  "AAABBORB" varchar(1) NOT NULL default ' ',
  "AAABEDGS" varchar(1) NOT NULL default ' ',
  "AAABFUKI" varchar(1) NOT NULL default ' ',
  "AAABBORE" varchar(1) NOT NULL default ' ',
  "AAABLECD" varchar(2) NOT NULL default ' ',
  "AAABIASH" varchar(3) NOT NULL default ' ',
  "AAABLTQC" varchar(3) NOT NULL default ' ',
  "AAABBORI" varchar(1) NOT NULL default ' ',
  "AAABFUJV" varchar(3) NOT NULL default ' ',
  "AAABBORK" varchar(2) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABIEIX" varchar(1) NOT NULL default '0',
  "AAABBORN" varchar(1) NOT NULL default ' ',
  "AAABBQBY" varchar(10) NOT NULL default ' ',
  "AAABBORP" varchar(1) NOT NULL default '0',
  "AAABBORQ" varchar(1) NOT NULL default ' ',
  "AAABBORR" varchar(1) NOT NULL default ' ',
  "AAABBORS" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBORV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABKQPS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOSC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABKQPS" varchar(1) NOT NULL default ' ',
  "AAABBOSH" varchar(30) NOT NULL default ' ',
  "AAABBOSI" varchar(30) NOT NULL default ' ',
  "AAABBOSJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOSM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMQHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOSU" (
  "AAABEXIM" varchar(2) NOT NULL default ' ',
  "AAABMCOH" varchar(2) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABJLFF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEXIM","AAABMCOH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOTB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABLMJR" varchar(2) NOT NULL default ' ',
  "AAABFUJV" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHV","AAABLMJR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOTJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABBOTQ" varchar(3) NOT NULL default ' ',
  "AAABGAJY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHV","AAABBOTQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOTR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABBPCX" varchar(1) NOT NULL default ' ',
  "AAABBPCY" varchar(1) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAABBPDH" varchar(1) NOT NULL default ' ',
  "AAABEXIH" varchar(4) NOT NULL default '0000',
  "AAABEXIK" varchar(5) NOT NULL default ' ',
  "AAABEXIL" varchar(5) NOT NULL default ' ',
  "AAABBOVZ" varchar(5) NOT NULL default ' ',
  "AAABBVTV" varchar(1) NOT NULL default ' ',
  "AAABBVTT" varchar(1) NOT NULL default ' ',
  "AAABBVTW" varchar(1) NOT NULL default ' ',
  "AAABEXIM" varchar(2) NOT NULL default ' ',
  "AAABMCOH" varchar(2) NOT NULL default ' ',
  "AAABMCOI" varchar(2) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABEXII" varchar(1) NOT NULL default ' ',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMQHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOUP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABBPCX" varchar(1) NOT NULL default ' ',
  "AAABBPCY" varchar(1) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAABBOUW" varchar(1) NOT NULL default ' ',
  "AAABBPDH" varchar(1) NOT NULL default ' ',
  "AAABBOUY" varchar(3) NOT NULL default '000',
  "AAABBOUZ" varchar(4) NOT NULL default '0000',
  "AAABBOVA" varchar(1) NOT NULL default ' ',
  "AAABEXIH" varchar(4) NOT NULL default '0000',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABEXIK" varchar(5) NOT NULL default ' ',
  "AAABEXIL" varchar(5) NOT NULL default ' ',
  "AAABBOVZ" varchar(5) NOT NULL default ' ',
  "AAABBVTV" varchar(1) NOT NULL default ' ',
  "AAABBVTT" varchar(1) NOT NULL default ' ',
  "AAABBVTW" varchar(1) NOT NULL default ' ',
  "AAABEXIM" varchar(2) NOT NULL default ' ',
  "AAABMCOH" varchar(2) NOT NULL default ' ',
  "AAABMCOI" varchar(2) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABETQU","AAABMQHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOVQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABEXIM" varchar(2) NOT NULL default ' ',
  "AAABMCOH" varchar(2) NOT NULL default ' ',
  "AAABMCOI" varchar(2) NOT NULL default ' ',
  "AAABEXIK" varchar(5) NOT NULL default ' ',
  "AAABEXIL" varchar(5) NOT NULL default ' ',
  "AAABBOVZ" varchar(5) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABBOWC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQDO","AAABMQHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOWG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABINIQ" varchar(30) NOT NULL default ' ',
  "AAABMMCV" varchar(30) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABINIQ","AAABMMCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOWO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABINIQ" varchar(30) NOT NULL default ' ',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABINIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOWU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABINIQ" varchar(30) NOT NULL default ' ',
  "AAABIANG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABINIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOXC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBPBC" varchar(5) NOT NULL default ' ',
  "AAABJDAN" varchar(1) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default ' ',
  "AAABBOXH" varchar(34) NOT NULL default ' ',
  "AAABBOXI" varchar(34) NOT NULL default ' ',
  "AAABBOXJ" varchar(34) NOT NULL default ' ',
  "AAABLMLH" varchar(1) NOT NULL default ' ',
  "AAABEJLV" varchar(1) NOT NULL default ' ',
  "AAABBOXM" varchar(1) NOT NULL default ' ',
  "AAABBOXN" varchar(1) NOT NULL default ' ',
  "AAABEIWW" varchar(1) NOT NULL default ' ',
  "AAABBOXP" varchar(10) NOT NULL default ' ',
  "AAABBOXQ" varchar(10) NOT NULL default ' ',
  "AAABBOXR" varchar(3) NOT NULL default ' ',
  "AAABBOXS" varchar(1) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABBOXU" varchar(10) NOT NULL default ' ',
  "AAABBOXV" varchar(10) NOT NULL default ' ',
  "AAABFJCO" varchar(3) NOT NULL default ' ',
  "AAABEPVK" varchar(10) NOT NULL default ' ',
  "AAABFUJV" varchar(3) NOT NULL default ' ',
  "AAABEISY" varchar(1) NOT NULL default ' ',
  "AAABEISZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBPBC","AAABJDAN","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBPBC" varchar(5) NOT NULL default ' ',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBPBC","AAABMKIQ","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOYO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABKYSP" varchar(5) NOT NULL default ' ',
  "AAABKYSQ" varchar(5) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABLVJB" varchar(3) NOT NULL default '000',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMQHX","AAABKYSP","AAABKYSQ","AAABMQHY","AAABLVJB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOZC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEIVG" varchar(1) NOT NULL default ' ',
  "AAABBOZF" varchar(1) NOT NULL default ' ',
  "AAABLUDG" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEIVG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOZJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBPBC" varchar(5) NOT NULL default ' ',
  "AAABBOZM" varchar(1) NOT NULL default ' ',
  "AAABBOZN" varchar(1) NOT NULL default ' ',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABBOZP" varchar(1) NOT NULL default ' ',
  "AAABBOZQ" varchar(1) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABFJCO" varchar(3) NOT NULL default ' ',
  "AAABBOZT" varchar(1) NOT NULL default ' ',
  "AAABKQPS" varchar(1) NOT NULL default ' ',
  "AAABBOZV" varchar(2) NOT NULL default ' ',
  "AAABBOZW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBPBC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBOZZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEIVG" varchar(1) NOT NULL default ' ',
  "AAABBPJJ" varchar(3) NOT NULL default ' ',
  "AAABBPAD" varchar(1) NOT NULL default ' ',
  "AAABBPAE" varchar(3) NOT NULL default ' ',
  "AAABBPAF" varchar(1) NOT NULL default ' ',
  "AAABBPAG" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABEIVG","AAABBPJJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPAK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGBL" varchar(3) NOT NULL default ' ',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABFLEJ" varchar(12) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABMMCV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKGBL","AAABMKIQ","AAABFLEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPAV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBPBC" varchar(5) NOT NULL default ' ',
  "AAABLUDG" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBPBC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPBD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIYCS" varchar(4) NOT NULL default ' ',
  "AAABBPIY" varchar(4) NOT NULL default ' ',
  "AAABBVTR" varchar(1) NOT NULL default ' ',
  "AAABBVTT" varchar(1) NOT NULL default ' ',
  "AAABBVTV" varchar(1) NOT NULL default ' ',
  "AAABJDMI" varchar(1) NOT NULL default ' ',
  "AAABBVTY" varchar(1) NOT NULL default ' ',
  "AAABBPDH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPBP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIYCS" varchar(4) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABBPDH" varchar(1) NOT NULL default ' ',
  "AAABGHKU" varchar(4) NOT NULL default ' ',
  "AAABBPCW" varchar(1) NOT NULL default ' ',
  "AAABBPCX" varchar(1) NOT NULL default ' ',
  "AAABBPCY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIYCS","AAABMQHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPCB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIYCS" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABBPDH" varchar(1) NOT NULL default ' ',
  "AAABGHKU" varchar(4) NOT NULL default ' ',
  "AAABBPCW" varchar(1) NOT NULL default ' ',
  "AAABBPCX" varchar(1) NOT NULL default ' ',
  "AAABBPCY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIYCS","AAABMREX","AAABMQHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPCP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIYCS" varchar(4) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABBPDH" varchar(1) NOT NULL default ' ',
  "AAABGHKU" varchar(4) NOT NULL default ' ',
  "AAABBPCW" varchar(1) NOT NULL default ' ',
  "AAABBPCX" varchar(1) NOT NULL default ' ',
  "AAABBPCY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIYCS","AAABETQU","AAABMQHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPDD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMMYV" varchar(2) NOT NULL default '00',
  "AAABBPDH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHV","AAABMMYV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPDL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABIYCS" varchar(4) NOT NULL default ' ',
  "AAABFUJV" varchar(1) NOT NULL default ' ',
  "AAABBPDQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHV","AAABIYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPDU" (
  "AAABIYCS" varchar(4) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABIYCS","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPEA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGHKU" varchar(4) NOT NULL default ' ',
  "AAABLOZE" varchar(1) NOT NULL default ' ',
  "AAABBPEE" varchar(30) NOT NULL default ' ',
  "AAABBPEF" varchar(30) NOT NULL default ' ',
  "AAABERMO" varchar(30) NOT NULL default ' ',
  "AAABBPEH" varchar(3) NOT NULL default ' ',
  "AAABBPEI" varchar(3) NOT NULL default ' ',
  "AAABBPEJ" varchar(30) NOT NULL default ' ',
  "AAABBPEK" varchar(30) NOT NULL default ' ',
  "AAABBPEL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGHKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPEO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMMYV" varchar(2) NOT NULL default '00',
  "AAABERMQ" varchar(30) NOT NULL default ' ',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABLWWN" varchar(8) NOT NULL default '00000000',
  "AAABLWWO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMQHV","AAABMMYV","AAABERMQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPFA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGHKU" varchar(4) NOT NULL default ' ',
  "AAABERMQ" varchar(30) NOT NULL default ' ',
  "AAABERMP" varchar(30) NOT NULL default ' ',
  "AAABERMM" varchar(30) NOT NULL default ' ',
  "AAABBPIP" varchar(4) NOT NULL default ' ',
  "AAABFUJV" varchar(3) NOT NULL default ' ',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABLWWN" varchar(8) NOT NULL default '00000000',
  "AAABLWWO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABGHKU","AAABERMQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPFO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABBPFR" varchar(1) NOT NULL default ' ',
  "AAABBPFS" varchar(1) NOT NULL default ' ',
  "AAABBPFT" varchar(1) NOT NULL default ' ',
  "AAABBPFU" varchar(1) NOT NULL default ' ',
  "AAABBPFV" varchar(1) NOT NULL default ' ',
  "AAABBPFW" varchar(1) NOT NULL default ' ',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABLWWN" varchar(8) NOT NULL default '00000000',
  "AAABLWWO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMQHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPGC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGHKU" varchar(4) NOT NULL default ' ',
  "AAABLUDG" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABGHKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABBPGY" varchar(1) NOT NULL default ' ',
  "AAABKYSQ" varchar(1) NOT NULL default ' ',
  "AAABBPGP" varchar(1) NOT NULL default ' ',
  "AAABBPGQ" varchar(1) NOT NULL default ' ',
  "AAABBPGR" varchar(1) NOT NULL default ' ',
  "AAABBPHF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPGV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABBPGY" varchar(1) NOT NULL default ' ',
  "AAABBPHF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPHC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABBPHF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPHI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMQBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPHQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMQBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPHY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUAM" varchar(11) NOT NULL default ' ',
  "AAABMMRE" varchar(254) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUAM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPIE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBPIP" varchar(4) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default ' ',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABERMQ" varchar(30) NOT NULL default ' ',
  "AAABBPIV" varchar(60) NOT NULL default ' ',
  "AAABBPIL" varchar(24) NOT NULL default ' ',
  "AAABLEHF" varchar(3) NOT NULL default ' ',
  "AAABBPIN" varchar(11) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBPIP","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPIR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBPIY" varchar(4) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default ' ',
  "AAABBPIV" varchar(60) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBPIY","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPJA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABFJZE" varchar(1) NOT NULL default ' ',
  "AAABFJZF" varchar(1) NOT NULL default ' ',
  "AAABFUKI" varchar(1) NOT NULL default ' ',
  "AAABBPJH" varchar(1) NOT NULL default ' ',
  "AAABBPJI" varchar(3) NOT NULL default ' ',
  "AAABBPJJ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHV","AAABMQHX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMQHV","AAABMQHX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPJX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMCRJ","AAABMQHV","AAABMQHX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPKJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABFJTK" varchar(1) NOT NULL default ' ',
  "AAABFJXQ" varchar(1) NOT NULL default ' ',
  "AAABFJXP" varchar(1) NOT NULL default ' ',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABHUQZ" varchar(8) NOT NULL default '00000000',
  "AAABLEST" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABMQHV","AAABMQHX","AAABMQHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPLA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMLCT" varchar(4) NOT NULL default ' ',
  "AAABLHRD" varchar(4) NOT NULL default ' ',
  "AAABBPOC" varchar(4) NOT NULL default ' ',
  "AAABBPLG" varchar(1) NOT NULL default ' ',
  "AAABFMED" varchar(10) NOT NULL default ' ',
  "AAABBPLI" varchar(10) NOT NULL default ' ',
  "AAABBPLJ" varchar(10) NOT NULL default ' ',
  "AAABBPLK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABMLCT","AAABLHRD","AAABBPOC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPLQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABHJRW" varchar(2) NOT NULL default ' ',
  "AAABMPMH" varchar(4) NOT NULL default '0000',
  "AAABFHKK" varchar(7) NOT NULL default ' ',
  "AAABBPLW" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABHJRW","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPMB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIYCS" varchar(4) NOT NULL default ' ',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABBPMF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIYCS","AAABMCFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPMJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABBPMS" varchar(10) NOT NULL default ' ',
  "AAABBPMN" varchar(10) NOT NULL default ' ',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABBPMS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPMT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABBPNC" varchar(6) NOT NULL default ' ',
  "AAABBPMY" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABLTXG","AAABBPNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPND" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBPPK" varchar(6) NOT NULL default ' ',
  "AAABBPNH" varchar(10) NOT NULL default ' ',
  "AAABBPNI" varchar(10) NOT NULL default ' ',
  "AAABCBKW" varchar(2) NOT NULL default ' ',
  "AAABCBKX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBPPK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPNO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABBPSM" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABBPSM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPNV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABBPOC" varchar(4) NOT NULL default ' ',
  "AAABLHRD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABBPOC","AAABLHRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPOE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABBPOM" varchar(1) NOT NULL default ' ',
  "AAABBPON" varchar(10) NOT NULL default ' ',
  "AAABBPOJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABBPOM","AAABBPON")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPOO" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQHM" varchar(18) NOT NULL default ' ',
  "AAABMQHN" varchar(18) NOT NULL default ' ',
  "AAABMQHO" varchar(18) NOT NULL default ' ',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMQHQ" varchar(3) NOT NULL default ' ',
  "AAABMQHR" varchar(1) NOT NULL default ' ',
  "AAABMQHS" varchar(3) NOT NULL default '000',
  "AAABBPTB" varchar(4) NOT NULL default ' ',
  "AAABBPTC" varchar(18) NOT NULL default ' ',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABBTSC" varchar(10) NOT NULL default ' ',
  "AAABBPPF" varchar(2) NOT NULL default '00',
  "AAABIBHM" varchar(16) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAABMCXV" varchar(4) NOT NULL default ' ',
  "AAABBPPK" varchar(6) NOT NULL default ' ',
  "AAABBPPL" varchar(10) NOT NULL default ' ',
  "AAABMQBS" varchar(4) NOT NULL default ' ',
  "AAABIAWX" varchar(2) NOT NULL default '00',
  "AAABBPPO" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMQDV" varchar(4) NOT NULL default ' ',
  "AAABBPPR" varchar(10) NOT NULL default ' ',
  "AAABBPPS" varchar(2) NOT NULL default '00',
  "AAABIBHQ" varchar(16) NOT NULL default ' ',
  "AAABJGHG" varchar(3) NOT NULL default ' ',
  "AAABMQDX" decimal(15,2) NOT NULL default '0.00',
  "AAABMQDY" decimal(15,2) NOT NULL default '0.00',
  "AAABMQDZ" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEA" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEB" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEC" decimal(15,2) NOT NULL default '0.00',
  "AAABMQED" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEE" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEF" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEG" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEH" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEI" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEJ" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEK" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEL" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEM" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEN" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEO" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEP" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEQ" decimal(15,2) NOT NULL default '0.00',
  "AAABMQER" decimal(15,2) NOT NULL default '0.00',
  "AAABMQES" decimal(15,2) NOT NULL default '0.00',
  "AAABMQET" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEU" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEV" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEW" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEX" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEY" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEZ" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFA" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFB" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFC" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFD" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFE" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFF" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFG" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFH" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFI" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFJ" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFK" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFL" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFM" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFN" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFO" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFP" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFQ" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFR" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFS" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFT" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFU" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFV" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFW" decimal(15,3) NOT NULL default '0.000',
  "AAABMQFX" decimal(15,3) NOT NULL default '0.000',
  "AAABMQFY" decimal(15,3) NOT NULL default '0.000',
  "AAABMQFZ" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGA" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGB" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGC" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGD" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGE" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGF" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGG" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGH" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGI" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGJ" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGK" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGL" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGM" decimal(15,3) NOT NULL default '0.000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBPSM" varchar(18) NOT NULL default ' ',
  "AAABBPSN" varchar(2) NOT NULL default ' ',
  "AAABJKTY" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMQHV","AAABMQHW","AAABMQHX","AAABMQHY","AAABMQHM","AAABMQHN","AAABMQHO","AAABMQHP","AAABMQHQ","AAABMQHR","AAABMQHS","AAABBPTB","AAABBPTC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPTD" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMCXV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMCRJ","AAABMCXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPTK" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMCXV" varchar(4) NOT NULL default ' ',
  "AAABLXIS" varchar(10) NOT NULL default ' ',
  "AAABGGHA" varchar(20) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMCRJ","AAABMLQA","AAABMCXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPTW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDJNS" varchar(3) NOT NULL default ' ',
  "AAABLXUA" varchar(4) NOT NULL default ' ',
  "AAABFSDE" varchar(2) NOT NULL default '00',
  "AAABFSDG" varchar(8) NOT NULL default '00000000',
  "AAABFSDH" varchar(7) NOT NULL default '0000000',
  "AAABFSDI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDJNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPUG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDJNS" varchar(3) NOT NULL default ' ',
  "AAABFSHS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDJNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPUO" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABBPVH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMQHV","AAABMQHY","AAABMCRJ","AAABMQDO","AAABMQCJ"),
  KEY "AAABBPVE" ("AAABMQHY","AAABMQDO","AAABBPVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPVI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABBPVL" varchar(1) NOT NULL default ' ',
  "AAABBPVM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPVP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMQBS" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABMQBS","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPWA" (
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPWD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBQBY" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBQBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPWI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBQBY" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBQBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPWQ" (
  "AAABBQDY" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABBQDY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPWT" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBQDY" varchar(5) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABBQDY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPWZ" (
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABKWJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPXC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABLEST" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKWJY","AAABLEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPXJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABLEST" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABKWJY","AAABLEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPXT" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABKWJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPXZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABLEST" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPYG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABLEST" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPYN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABLEST" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCMO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPYU" (
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABCNAB" varchar(1) NOT NULL default ' ',
  "AAABMCQQ" varchar(3) NOT NULL default ' ',
  "AAABBQDY" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMPWP","AAABCNAB","AAABMCQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPZC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFFWN" varchar(4) NOT NULL default ' ',
  "AAABBPZK" varchar(10) NOT NULL default ' ',
  "AAABBPZG" varchar(10) NOT NULL default ' ',
  "AAABBQDY" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFFWN","AAABBPZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPZL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBQBY" varchar(10) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABLEST" varchar(4) NOT NULL default ' ',
  "AAABBQDY" varchar(5) NOT NULL default ' ',
  "AAABKKAM" varchar(3) NOT NULL default ' ',
  "AAABBPZS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBQBY","AAABKWJY","AAABLEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBPZX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBQBY" varchar(10) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABLEST" varchar(4) NOT NULL default ' ',
  "AAABBQDY" varchar(5) NOT NULL default ' ',
  "AAABBQBN" varchar(1) NOT NULL default ' ',
  "AAABBQAE" varchar(1) NOT NULL default ' ',
  "AAABBQAF" varchar(1) NOT NULL default ' ',
  "AAABBQAG" varchar(1) NOT NULL default ' ',
  "AAABBQAH" varchar(1) NOT NULL default ' ',
  "AAABBQAI" varchar(1) NOT NULL default ' ',
  "AAABBQAJ" varchar(1) NOT NULL default ' ',
  "AAABBQFF" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBQBY","AAABKWJY","AAABLEST","AAABBQDY","AAABBQBN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQAR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBQBY" varchar(10) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABLEST" varchar(4) NOT NULL default ' ',
  "AAABBQDY" varchar(5) NOT NULL default ' ',
  "AAABBQBN" varchar(1) NOT NULL default ' ',
  "AAABBQBG" varchar(5) NOT NULL default ' ',
  "AAABBQAZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBQBY","AAABKWJY","AAABLEST","AAABBQDY","AAABBQBN","AAABBQBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQBH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBQBY" varchar(10) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABLEST" varchar(4) NOT NULL default ' ',
  "AAABBQDY" varchar(5) NOT NULL default ' ',
  "AAABBQBN" varchar(1) NOT NULL default ' ',
  "AAABBQBO" varchar(1) NOT NULL default ' ',
  "AAABBQBP" varchar(3) NOT NULL default ' ',
  "AAABBQBQ" varchar(1) NOT NULL default ' ',
  "AAABBQBR" varchar(1) NOT NULL default ' ',
  "AAABBQBS" varchar(3) NOT NULL default ' ',
  "AAABBQBT" varchar(1) NOT NULL default ' ',
  "AAABBQBU" varchar(1) NOT NULL default ' ',
  "AAABBQBV" varchar(1) NOT NULL default ' ',
  "AAABBQBW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBQBY","AAABKWJY","AAABLEST","AAABBQDY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQCC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABLEST" varchar(4) NOT NULL default ' ',
  "AAABBQDY" varchar(5) NOT NULL default ' ',
  "AAABBQCH" varchar(1) NOT NULL default ' ',
  "AAABBQCR" varchar(1) NOT NULL default ' ',
  "AAABBQCS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKWJY","AAABLEST","AAABBQDY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQCO" (
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABBQDY" varchar(5) NOT NULL default ' ',
  "AAABBQCR" varchar(1) NOT NULL default ' ',
  "AAABBQCS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKWJY","AAABBQDY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQCV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBQDY" varchar(5) NOT NULL default ' ',
  "AAABKKAM" varchar(3) NOT NULL default ' ',
  "AAABBQCZ" varchar(1) NOT NULL default ' ',
  "AAABBQDA" varchar(2) NOT NULL default ' ',
  "AAABBQDB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBQDY","AAABKKAM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBQDY" varchar(5) NOT NULL default ' ',
  "AAABKKAM" varchar(3) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBQDY","AAABKKAM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQDP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFFWN" varchar(4) NOT NULL default ' ',
  "AAABBQDY" varchar(5) NOT NULL default ' ',
  "AAABKKAM" varchar(3) NOT NULL default ' ',
  "AAABBQEA" varchar(10) NOT NULL default ' ',
  "AAABGZJM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFFWN","AAABBQDY","AAABKKAM","AAABBQEA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQEB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABBTUU" varchar(1) NOT NULL default ' ',
  "AAABBQEG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHV","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQEK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBQFF" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBQFF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQEP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBQFF" varchar(5) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBQFF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQEX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABBQFF" varchar(5) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHV","AAABBQFF","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQFH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABECDQ" varchar(1) NOT NULL default ' ',
  "AAABMREM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQFM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBVFS" varchar(4) NOT NULL default ' ',
  "AAABBRNW" varchar(4) NOT NULL default ' ',
  "AAABBQRR" varchar(2) NOT NULL default ' ',
  "AAABBQFR" varchar(5) NOT NULL default ' ',
  "AAABBQFS" varchar(5) NOT NULL default ' ',
  "AAABBQFT" varchar(5) NOT NULL default ' ',
  "AAABBQFU" varchar(5) NOT NULL default ' ',
  "AAABBQFV" varchar(5) NOT NULL default ' ',
  "AAABBQFW" varchar(5) NOT NULL default ' ',
  "AAABBQFX" varchar(5) NOT NULL default ' ',
  "AAABBQFY" varchar(5) NOT NULL default ' ',
  "AAABBQFZ" varchar(5) NOT NULL default ' ',
  "AAABBQGA" varchar(5) NOT NULL default ' ',
  "AAABBQGB" varchar(5) NOT NULL default ' ',
  "AAABBQGC" varchar(5) NOT NULL default ' ',
  "AAABBQGD" varchar(5) NOT NULL default ' ',
  "AAABBQGE" varchar(5) NOT NULL default ' ',
  "AAABBQGF" varchar(5) NOT NULL default ' ',
  "AAABBQGG" varchar(1) NOT NULL default ' ',
  "AAABBQGH" varchar(5) NOT NULL default ' ',
  "AAABBQGI" varchar(5) NOT NULL default ' ',
  "AAABBQGJ" varchar(5) NOT NULL default ' ',
  "AAABBQGK" varchar(5) NOT NULL default ' ',
  "AAABBQGL" varchar(5) NOT NULL default ' ',
  "AAABBQGM" varchar(5) NOT NULL default ' ',
  "AAABBQGN" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBVFS","AAABBRNW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQGR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBVFS" varchar(4) NOT NULL default ' ',
  "AAABBRNW" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOQQ" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBVFS","AAABBRNW","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQHB" (
  "AAABGCVC" varchar(40) NOT NULL default ' ',
  "AAABGYSC" varchar(30) NOT NULL default ' ',
  "AAABBQRH" varchar(5) NOT NULL default ' ',
  "AAABBQRR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABGCVC","AAABGYSC","AAABBQRH","AAABBQRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQHK" (
  "AAABGCVC" varchar(40) NOT NULL default ' ',
  "AAABGYSC" varchar(30) NOT NULL default ' ',
  "AAABBQRH" varchar(5) NOT NULL default ' ',
  "AAABBQRR" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOQQ" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABGCVC","AAABGYSC","AAABBQRH","AAABBQRR","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQHW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBVFS" varchar(4) NOT NULL default ' ',
  "AAABBRNW" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOQQ" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBVFS","AAABBRNW","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQIG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBTCU" varchar(4) NOT NULL default ' ',
  "AAABBQIJ" varchar(1) NOT NULL default ' ',
  "AAABBQIK" varchar(1) NOT NULL default ' ',
  "AAABBQQU" varchar(1) NOT NULL default ' ',
  "AAABBQQV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBTCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQIP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABBTCU" varchar(4) NOT NULL default ' ',
  "AAABBQIT" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABBTCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQIX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJFIO" varchar(4) NOT NULL default ' ',
  "AAABBQJB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJFIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQJF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJFIO" varchar(4) NOT NULL default ' ',
  "AAABJBVB" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABJFIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQJP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFIR" varchar(4) NOT NULL default ' ',
  "AAABBQRR" varchar(2) NOT NULL default ' ',
  "AAABCKMB" varchar(2) NOT NULL default '00',
  "AAABBQON" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFIR","AAABBQRR","AAABCKMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQJZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJFIO" varchar(4) NOT NULL default ' ',
  "AAABBTCU" varchar(4) NOT NULL default ' ',
  "AAABBQKE" varchar(2) NOT NULL default ' ',
  "AAABBQKF" varchar(2) NOT NULL default ' ',
  "AAABFFWW" varchar(2) NOT NULL default ' ',
  "AAABBQKH" varchar(8) NOT NULL default ' ',
  "AAABBQKI" varchar(10) NOT NULL default ' ',
  "AAABBQKJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJFIO","AAABBTCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJFIO" varchar(4) NOT NULL default ' ',
  "AAABBTCU" varchar(4) NOT NULL default ' ',
  "AAABKGKQ" varchar(2) NOT NULL default ' ',
  "AAABMCWW" varchar(2) NOT NULL default ' ',
  "AAABBQKV" varchar(2) NOT NULL default ' ',
  "AAABBQKW" varchar(1) NOT NULL default ' ',
  "AAABBQKX" varchar(2) NOT NULL default ' ',
  "AAABBQKY" varchar(1) NOT NULL default ' ',
  "AAABBQKZ" varchar(2) NOT NULL default ' ',
  "AAABBQLA" varchar(2) NOT NULL default ' ',
  "AAABBQLB" varchar(1) NOT NULL default ' ',
  "AAABBQLC" varchar(2) NOT NULL default ' ',
  "AAABBQLD" varchar(1) NOT NULL default ' ',
  "AAABBQLE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJFIO","AAABBTCU","AAABKGKQ","AAABMCWW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQLL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBQON" varchar(16) NOT NULL default ' ',
  "AAABBQRH" varchar(5) NOT NULL default ' ',
  "AAABBQLY" varchar(1) NOT NULL default ' ',
  "AAABBQOP" varchar(2) NOT NULL default '00',
  "AAABBQOI" varchar(8) NOT NULL default ' ',
  "AAABBQOJ" varchar(30) NOT NULL default ' ',
  "AAABILRT" varchar(1) NOT NULL default ' ',
  "AAABBQOL" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABBQON","AAABBQRH","AAABBQLY","AAABBQOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQMA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBQON" varchar(16) NOT NULL default ' ',
  "AAABBQMT" varchar(2) NOT NULL default '00',
  "AAABLMLR" varchar(2) NOT NULL default '00',
  "AAABBQMF" varchar(2) NOT NULL default '00',
  "AAABBSLU" varchar(2) NOT NULL default ' ',
  "AAABBQMH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBQON","AAABBQMT","AAABLMLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQMM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBQON" varchar(16) NOT NULL default ' ',
  "AAABBQMT" varchar(2) NOT NULL default '00',
  "AAABBQMQ" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBQON","AAABBQMT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQMU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBTCU" varchar(4) NOT NULL default ' ',
  "AAABKGKQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBTCU","AAABKGKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQNB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABBTCU" varchar(4) NOT NULL default ' ',
  "AAABKGKQ" varchar(2) NOT NULL default ' ',
  "AAABBQNG" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABBTCU","AAABKGKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQNL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBVFS" varchar(4) NOT NULL default ' ',
  "AAABBQRH" varchar(5) NOT NULL default ' ',
  "AAABBQRR" varchar(2) NOT NULL default ' ',
  "AAABBQNQ" varchar(1) NOT NULL default ' ',
  "AAABBVTW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBVFS","AAABBQRH","AAABBQRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQNW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBQON" varchar(16) NOT NULL default ' ',
  "AAABBQRR" varchar(2) NOT NULL default ' ',
  "AAABBVFS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBQON")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQOD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBQON" varchar(16) NOT NULL default ' ',
  "AAABBQRH" varchar(5) NOT NULL default ' ',
  "AAABBQOP" varchar(2) NOT NULL default '00',
  "AAABBQOI" varchar(8) NOT NULL default ' ',
  "AAABBQOJ" varchar(30) NOT NULL default ' ',
  "AAABILRT" varchar(1) NOT NULL default ' ',
  "AAABBQOL" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABBQON","AAABBQRH","AAABBQOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQOQ" (
  "AAABBQRR" varchar(2) NOT NULL default ' ',
  "AAABLDRF" varchar(2) NOT NULL default '00',
  "AAABBQPK" varchar(1) NOT NULL default ' ',
  "AAABGCVC" varchar(40) NOT NULL default ' ',
  "AAABGYSC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABBQRR","AAABLDRF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQOY" (
  "AAABBQRR" varchar(2) NOT NULL default ' ',
  "AAABLDRF" varchar(2) NOT NULL default '00',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABBQRR","AAABLDRF","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQPG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBVFS" varchar(4) NOT NULL default ' ',
  "AAABBQRR" varchar(2) NOT NULL default ' ',
  "AAABBQPK" varchar(1) NOT NULL default ' ',
  "AAABGCVC" varchar(40) NOT NULL default ' ',
  "AAABGYSC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBVFS","AAABBQRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQPQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABBVEU" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABBVEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBVEU" varchar(4) NOT NULL default ' ',
  "AAABBVFS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBVEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQQE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBTSQ" varchar(4) NOT NULL default ' ',
  "AAABBTSR" varchar(4) NOT NULL default ' ',
  "AAABBTSS" varchar(4) NOT NULL default ' ',
  "AAABBRNW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBTSQ","AAABBTSR","AAABBTSS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQQQ" (
  "AAABBTCU" varchar(4) NOT NULL default ' ',
  "AAABBQQS" varchar(1) NOT NULL default ' ',
  "AAABBQQT" varchar(1) NOT NULL default ' ',
  "AAABBQQU" varchar(1) NOT NULL default ' ',
  "AAABBQQV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABBTCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQQX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABBVFS" varchar(4) NOT NULL default ' ',
  "AAABBQRH" varchar(5) NOT NULL default ' ',
  "AAABBQRR" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABBVFS","AAABBQRH","AAABBQRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQRJ" (
  "AAABBQRR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABBQRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQRM" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABBQRR" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABBQRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQRS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABBQTF" varchar(1) NOT NULL default ' ',
  "AAABBQTG" varchar(1) NOT NULL default ' ',
  "AAABLXSU" varchar(1) NOT NULL default ' ',
  "AAABMBFP" varchar(1) NOT NULL default ' ',
  "AAABIBYF" varchar(1) NOT NULL default ' ',
  "AAABLXTE" varchar(1) NOT NULL default ' ',
  "AAABBQSB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCFC","AAABBQTF","AAABBQTG","AAABLXSU","AAABMBFP","AAABIBYF","AAABLXTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQSK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVX" varchar(4) NOT NULL default ' ',
  "AAABBUSX" varchar(2) NOT NULL default '00',
  "AAABBQSO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVX","AAABBUSX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQSS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABBQTF" varchar(1) NOT NULL default ' ',
  "AAABBQTG" varchar(1) NOT NULL default ' ',
  "AAABLXSU" varchar(1) NOT NULL default ' ',
  "AAABMBFP" varchar(1) NOT NULL default ' ',
  "AAABLXTE" varchar(1) NOT NULL default ' ',
  "AAABMCVX" varchar(4) NOT NULL default ' ',
  "AAABBUSX" varchar(2) NOT NULL default '00',
  "AAABBQTC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCFC","AAABBQTF","AAABBQTG","AAABLXSU","AAABMBFP","AAABLXTE","AAABMCVX","AAABBUSX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQTM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABCLWB" varchar(10) NOT NULL default ' ',
  "AAABCLWC" varchar(10) NOT NULL default ' ',
  "AAABBQUB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABCLWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJFJL" varchar(4) NOT NULL default ' ',
  "AAABCLWB" varchar(10) NOT NULL default ' ',
  "AAABCLWC" varchar(10) NOT NULL default ' ',
  "AAABBQUB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJFJL","AAABCLWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQUG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBQUZ" varchar(10) NOT NULL default ' ',
  "AAABBQUU" varchar(10) NOT NULL default ' ',
  "AAABBQUV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBQUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQUP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJFJL" varchar(4) NOT NULL default ' ',
  "AAABBQUZ" varchar(10) NOT NULL default ' ',
  "AAABBQUU" varchar(10) NOT NULL default ' ',
  "AAABBQUV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJFJL","AAABBQUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQVA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBRML" varchar(24) NOT NULL default ' ',
  "AAABBRMI" varchar(24) NOT NULL default ' ',
  "AAABBQVP" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBRML")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQVJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJFJL" varchar(4) NOT NULL default ' ',
  "AAABBRML" varchar(24) NOT NULL default ' ',
  "AAABBRMI" varchar(24) NOT NULL default ' ',
  "AAABBQVP" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJFJL","AAABBRML")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBQWN" varchar(12) NOT NULL default ' ',
  "AAABBQWI" varchar(12) NOT NULL default ' ',
  "AAABBQWJ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBQWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQWD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJFJL" varchar(4) NOT NULL default ' ',
  "AAABBQWN" varchar(12) NOT NULL default ' ',
  "AAABBQWI" varchar(12) NOT NULL default ' ',
  "AAABBQWJ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJFJL","AAABBQWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQWO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABCLWB" varchar(10) NOT NULL default ' ',
  "AAABCLWC" varchar(10) NOT NULL default ' ',
  "AAABBQWT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABCLWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQWX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJFJL" varchar(4) NOT NULL default ' ',
  "AAABBQXK" varchar(12) NOT NULL default ' ',
  "AAABBQXL" varchar(4) NOT NULL default ' ',
  "AAABBQXD" varchar(12) NOT NULL default ' ',
  "AAABBQXE" varchar(4) NOT NULL default ' ',
  "AAABBQXF" varchar(12) NOT NULL default ' ',
  "AAABBQXG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJFJL","AAABBQXK","AAABBQXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQXM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBQYP" varchar(12) NOT NULL default ' ',
  "AAABBQYQ" varchar(4) NOT NULL default ' ',
  "AAABBQYG" varchar(12) NOT NULL default ' ',
  "AAABBQYH" varchar(4) NOT NULL default ' ',
  "AAABBQYI" varchar(12) NOT NULL default ' ',
  "AAABBQYJ" varchar(4) NOT NULL default ' ',
  "AAABBQYL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBQYP","AAABBQYQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQYA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJFJL" varchar(4) NOT NULL default ' ',
  "AAABBQYP" varchar(12) NOT NULL default ' ',
  "AAABBQYQ" varchar(4) NOT NULL default ' ',
  "AAABBQYG" varchar(12) NOT NULL default ' ',
  "AAABBQYH" varchar(4) NOT NULL default ' ',
  "AAABBQYI" varchar(12) NOT NULL default ' ',
  "AAABBQYJ" varchar(4) NOT NULL default ' ',
  "AAABBQYK" varchar(8) NOT NULL default '00000000',
  "AAABBQYL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJFJL","AAABBQYP","AAABBQYQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQYR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJFJL" varchar(4) NOT NULL default ' ',
  "AAABBQZB" varchar(10) NOT NULL default ' ',
  "AAABBQYW" varchar(10) NOT NULL default ' ',
  "AAABBQYX" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJFJL","AAABBQZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQZC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBURT" varchar(4) NOT NULL default ' ',
  "AAABBQZG" decimal(9,6) NOT NULL default '0.000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBURT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQZK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBURT" varchar(4) NOT NULL default ' ',
  "AAABBQZP" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABBURT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBQZU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCWX" varchar(3) NOT NULL default ' ',
  "AAABBRAG" varchar(10) NOT NULL default ' ',
  "AAABBRAH" varchar(10) NOT NULL default ' ',
  "AAABBRAB" decimal(9,6) NOT NULL default '0.000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCWV","AAABMCWX","AAABBRAG","AAABBRAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRAI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBURS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBURS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRAP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBURS" varchar(4) NOT NULL default ' ',
  "AAABBRAU" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABBURS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRAZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBRDC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBRDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRBG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBRDC" varchar(4) NOT NULL default ' ',
  "AAABBRBL" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABBRDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRBQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBRDC" varchar(4) NOT NULL default ' ',
  "AAABGBBM" varchar(6) NOT NULL default ' ',
  "AAABBRJE" varchar(6) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABBRBX" varchar(1) NOT NULL default ' ',
  "AAABBRBY" varchar(1) NOT NULL default ' ',
  "AAABBRBZ" varchar(1) NOT NULL default ' ',
  "AAABBRJX" varchar(1) NOT NULL default ' ',
  "AAABBRCB" varchar(1) NOT NULL default ' ',
  "AAABBRCC" varchar(1) NOT NULL default '0',
  "AAABBRJY" varchar(1) NOT NULL default ' ',
  "AAABBRJW" varchar(1) NOT NULL default ' ',
  "AAABBRCF" varchar(1) NOT NULL default ' ',
  "AAABBRCG" varchar(8) NOT NULL default '00000000',
  "AAABBRCH" varchar(10) NOT NULL default ' ',
  "AAABBRJN" varchar(1) NOT NULL default ' ',
  "AAABBRJO" varchar(3) NOT NULL default ' ',
  "AAABBRJQ" varchar(1) NOT NULL default ' ',
  "AAABBRJP" varchar(1) NOT NULL default '0',
  "AAABBRCM" varchar(1) NOT NULL default ' ',
  "AAABGNXB" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBRDC","AAABGBBM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRCS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBRDC" varchar(4) NOT NULL default ' ',
  "AAABGBBM" varchar(6) NOT NULL default ' ',
  "AAABBRIF" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABBRDC","AAABGBBM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRDE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABBRDP" varchar(30) NOT NULL default ' ',
  "AAABMCRL" varchar(8) NOT NULL default '00000000',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCWV","AAABBRDP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRDQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMLFE" varchar(10) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABKOUD" varchar(10) NOT NULL default ' ',
  "AAABJBVB" varchar(35) NOT NULL default ' ',
  "AAABMDEP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMLFE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBREB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMLFE" varchar(10) NOT NULL default ' ',
  "AAABBTCU" varchar(4) NOT NULL default ' ',
  "AAABJRKN" varchar(10) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCWX" varchar(3) NOT NULL default ' ',
  "AAABMCRL" varchar(8) NOT NULL default '00000000',
  "AAABKYSP" varchar(5) NOT NULL default ' ',
  "AAABBRFD" varchar(6) NOT NULL default ' ',
  "AAABJWUT" decimal(13,2) NOT NULL default '0.00',
  "AAABBRKP" decimal(13,2) NOT NULL default '0.00',
  "AAABBREO" varchar(1) NOT NULL default ' ',
  "AAABJFJL" varchar(4) NOT NULL default ' ',
  "AAABBREQ" varchar(1) NOT NULL default ' ',
  "AAABBRER" varchar(1) NOT NULL default ' ',
  "AAABMCTK" varchar(4) NOT NULL default ' ',
  "AAABMCSW" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMLFE","AAABBTCU","AAABJRKN","AAABMCWV","AAABMCWX","AAABMCRL","AAABKYSP","AAABBRFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRFE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBRLS" varchar(10) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABKOUD" varchar(10) NOT NULL default ' ',
  "AAABBRIF" varchar(35) NOT NULL default ' ',
  "AAABBSPA" varchar(5) NOT NULL default ' ',
  "AAABBRFM" decimal(13,2) NOT NULL default '0.00',
  "AAABJRKN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBRLS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRFR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBRLS" varchar(10) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCWX" varchar(3) NOT NULL default ' ',
  "AAABBRGK" varchar(4) NOT NULL default '0000',
  "AAABBRLC" varchar(60) NOT NULL default ' ',
  "AAABJWUT" decimal(17,2) NOT NULL default '0.00',
  "AAABBRKP" decimal(17,2) NOT NULL default '0.00',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMCRL" varchar(8) NOT NULL default '00000000',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABBRJL" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBRLS","AAABMCWV","AAABMCWX","AAABBRGK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRGL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBRLS" varchar(10) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCWX" varchar(3) NOT NULL default ' ',
  "AAABJWUT" decimal(13,2) NOT NULL default '0.00',
  "AAABBRLM" decimal(13,2) NOT NULL default '0.00',
  "AAABBRLN" decimal(13,2) NOT NULL default '0.00',
  "AAABBRLO" varchar(5) NOT NULL default ' ',
  "AAABBRLP" decimal(9,5) NOT NULL default '0.00000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBRLS","AAABMCWV","AAABMCWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRHB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBRLS" varchar(10) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCWX" varchar(3) NOT NULL default ' ',
  "AAABGBBM" varchar(10) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABJWUT" decimal(17,2) NOT NULL default '0.00',
  "AAABBRKP" decimal(17,2) NOT NULL default '0.00',
  "AAABBRKQ" varchar(4) NOT NULL default '0000',
  "AAABBRKR" varchar(3) NOT NULL default '000',
  "AAABBRKS" decimal(13,2) NOT NULL default '0.00',
  "AAABIIWF" varchar(16) NOT NULL default ' ',
  "AAABMOAH" varchar(17) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABKWVK" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBRLS","AAABMCWV","AAABMCWX","AAABGBBM","AAABMQVT","AAABMQVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRIB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBRLS" varchar(10) NOT NULL default ' ',
  "AAABBRIF" varchar(35) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABKOUD" varchar(10) NOT NULL default ' ',
  "AAABJRKN" varchar(10) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABBSPA" varchar(5) NOT NULL default ' ',
  "AAABBRIL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBRLS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRIP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBRLS" varchar(10) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCWX" varchar(3) NOT NULL default ' ',
  "AAABBRKE" varchar(4) NOT NULL default '0000',
  "AAABBRLC" varchar(60) NOT NULL default ' ',
  "AAABBTHP" decimal(9,6) NOT NULL default '0.000000',
  "AAABBUYB" varchar(4) NOT NULL default ' ',
  "AAABBRIZ" decimal(17,2) NOT NULL default '0.00',
  "AAABBRJA" decimal(17,2) NOT NULL default '0.00',
  "AAABBUXH" varchar(4) NOT NULL default ' ',
  "AAABCNUZ" varchar(1) NOT NULL default ' ',
  "AAABBRJD" varchar(2) NOT NULL default '00',
  "AAABBRJE" varchar(6) NOT NULL default ' ',
  "AAABGBBM" varchar(6) NOT NULL default ' ',
  "AAABMOQQ" varchar(41) NOT NULL default ' ',
  "AAABBRJH" varchar(1) NOT NULL default ' ',
  "AAABMCRL" varchar(8) NOT NULL default '00000000',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABBRJL" varchar(1) NOT NULL default '0',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABBRJN" varchar(1) NOT NULL default ' ',
  "AAABBRJO" varchar(3) NOT NULL default ' ',
  "AAABBRJP" varchar(1) NOT NULL default '0',
  "AAABBRJQ" varchar(1) NOT NULL default ' ',
  "AAABBRJR" decimal(17,2) NOT NULL default '0.00',
  "AAABBRJS" decimal(17,2) NOT NULL default '0.00',
  "AAABBRJT" decimal(17,2) NOT NULL default '0.00',
  "AAABBRJU" decimal(17,2) NOT NULL default '0.00',
  "AAABBRJV" int(11) NOT NULL default '0',
  "AAABBRJW" varchar(1) NOT NULL default ' ',
  "AAABBRJX" varchar(1) NOT NULL default ' ',
  "AAABBRJY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBRLS","AAABMCWV","AAABMCWX","AAABBRKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRKF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBRLS" varchar(10) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCWX" varchar(3) NOT NULL default ' ',
  "AAABBRLC" varchar(60) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABJWUT" decimal(17,2) NOT NULL default '0.00',
  "AAABBRKP" decimal(17,2) NOT NULL default '0.00',
  "AAABBRKQ" varchar(4) NOT NULL default '0000',
  "AAABBRKR" varchar(3) NOT NULL default '000',
  "AAABBRKS" decimal(13,2) NOT NULL default '0.00',
  "AAABIIWF" varchar(16) NOT NULL default ' ',
  "AAABMOAH" varchar(17) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABKWVK" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBRLS","AAABMCWV","AAABMCWX","AAABBRLC","AAABMQVT","AAABMQVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRLF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBRLS" varchar(10) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCWX" varchar(3) NOT NULL default ' ',
  "AAABJWUT" decimal(13,2) NOT NULL default '0.00',
  "AAABBRLM" decimal(13,2) NOT NULL default '0.00',
  "AAABBRLN" decimal(13,2) NOT NULL default '0.00',
  "AAABBRLO" varchar(5) NOT NULL default ' ',
  "AAABBRLP" decimal(9,5) NOT NULL default '0.00000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBRLS","AAABMCWV","AAABMCWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRLV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABCLWB" varchar(10) NOT NULL default ' ',
  "AAABCLWC" varchar(10) NOT NULL default ' ',
  "AAABBRNW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABCLWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRME" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBRML" varchar(24) NOT NULL default ' ',
  "AAABBRMI" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBRML")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRMM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCWW" varchar(2) NOT NULL default ' ',
  "AAABBRNW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCWW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRMU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBRNB" varchar(4) NOT NULL default ' ',
  "AAABBRNW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBRNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRNC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBRNJ" varchar(4) NOT NULL default ' ',
  "AAABBRNW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBRNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRNK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBRNR" varchar(2) NOT NULL default ' ',
  "AAABBRNW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBRNR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRNS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBRNZ" varchar(4) NOT NULL default ' ',
  "AAABBRNW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBRNZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBROA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBTDA" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBTDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBROH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBTDA" varchar(2) NOT NULL default ' ',
  "AAABBROM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABBTDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBROR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBVFS" varchar(4) NOT NULL default ' ',
  "AAABJACE" varchar(5) NOT NULL default ' ',
  "AAABBSZA" varchar(2) NOT NULL default ' ',
  "AAABBRPF" varchar(2) NOT NULL default '00',
  "AAABBRPG" varchar(27) NOT NULL default ' ',
  "AAABBRPH" varchar(3) NOT NULL default '000',
  "AAABELZB" varchar(30) NOT NULL default ' ',
  "AAABLMNC" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBVFS","AAABJACE","AAABBSZA","AAABBRPF","AAABBRPG","AAABBRPH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRPI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRPN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLMNC" varchar(15) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLMNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRPV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(15) NOT NULL default ' ',
  "AAABBRQF" varchar(2) NOT NULL default '00',
  "AAABBRPZ" varchar(1) NOT NULL default ' ',
  "AAABHAGF" varchar(3) NOT NULL default '000',
  "AAABJSDF" varchar(3) NOT NULL default '000',
  "AAABBRQC" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABBRQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRQG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBVEH" varchar(2) NOT NULL default ' ',
  "AAABBRRA" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBVEH","AAABBRRA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRQP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBVEH" varchar(2) NOT NULL default ' ',
  "AAABBRRA" varchar(2) NOT NULL default ' ',
  "AAABBRQV" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABBVEH","AAABBRRA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRRB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBSLU" varchar(2) NOT NULL default ' ',
  "AAABBTDA" varchar(2) NOT NULL default ' ',
  "AAABBRRG" varchar(1) NOT NULL default ' ',
  "AAABKRJE" varchar(3) NOT NULL default ' ',
  "AAABKRJF" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBSLU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRRM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBRRP" decimal(17,2) NOT NULL default '0.00',
  "AAABBRRQ" decimal(17,2) NOT NULL default '0.00',
  "AAABBRRR" varchar(3) NOT NULL default ' ',
  "AAABBRRS" varchar(2) NOT NULL default ' ',
  "AAABBRRT" varchar(40) NOT NULL default ' ',
  "AAABBRRU" varchar(3) NOT NULL default ' ',
  "AAABBRYA" varchar(2) NOT NULL default ' ',
  "AAABBRYR" varchar(2) NOT NULL default ' ',
  "AAABBRRX" varchar(1) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRSB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMRQ" varchar(4) NOT NULL default ' ',
  "AAABBRSE" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMMRQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRSH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLOZE" varchar(3) NOT NULL default ' ',
  "AAABBTSM" varchar(3) NOT NULL default ' ',
  "AAABBRSM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLOZE" varchar(3) NOT NULL default ' ',
  "AAABKZAW" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRTA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLOZE" varchar(3) NOT NULL default ' ',
  "AAABIHQW" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLOZE","AAABIHQW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRTJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLOZE" varchar(3) NOT NULL default ' ',
  "AAABIHQW" varchar(5) NOT NULL default ' ',
  "AAABKZAW" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABLOZE","AAABIHQW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRTV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABININ" varchar(4) NOT NULL default ' ',
  "AAABKRJE" varchar(3) NOT NULL default ' ',
  "AAABKRJF" varchar(5) NOT NULL default ' ',
  "AAABBTSL" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABININ","AAABKRJE","AAABKRJF","AAABBTSL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRUG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABININ" varchar(4) NOT NULL default ' ',
  "AAABKRJE" varchar(3) NOT NULL default ' ',
  "AAABKRJF" varchar(5) NOT NULL default ' ',
  "AAABBTSL" varchar(24) NOT NULL default ' ',
  "AAABILRJ" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABININ","AAABKRJE","AAABKRJF","AAABBTSL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRUU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABININ" varchar(4) NOT NULL default ' ',
  "AAABBRWU" varchar(10) NOT NULL default ' ',
  "AAABBRWV" varchar(3) NOT NULL default ' ',
  "AAABBRWO" varchar(10) NOT NULL default ' ',
  "AAABKRJE" varchar(3) NOT NULL default ' ',
  "AAABKRJF" varchar(5) NOT NULL default ' ',
  "AAABBTSL" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABININ","AAABBRWU","AAABBRWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRVH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABININ" varchar(4) NOT NULL default ' ',
  "AAABKRJE" varchar(3) NOT NULL default ' ',
  "AAABBRWU" varchar(10) NOT NULL default ' ',
  "AAABBRWV" varchar(3) NOT NULL default ' ',
  "AAABBRWO" varchar(10) NOT NULL default ' ',
  "AAABBRVO" varchar(3) NOT NULL default ' ',
  "AAABKRJF" varchar(5) NOT NULL default ' ',
  "AAABBTSL" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABININ","AAABKRJE","AAABBRWU","AAABBRWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRVW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABININ" varchar(4) NOT NULL default ' ',
  "AAABKRJE" varchar(3) NOT NULL default ' ',
  "AAABKRJF" varchar(5) NOT NULL default ' ',
  "AAABBRWU" varchar(10) NOT NULL default ' ',
  "AAABBRWO" varchar(10) NOT NULL default ' ',
  "AAABBTSL" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABININ","AAABKRJE","AAABKRJF","AAABBRWU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRWJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABININ" varchar(4) NOT NULL default ' ',
  "AAABBRWU" varchar(10) NOT NULL default ' ',
  "AAABBRWV" varchar(3) NOT NULL default ' ',
  "AAABBRWO" varchar(10) NOT NULL default ' ',
  "AAABKRJE" varchar(3) NOT NULL default ' ',
  "AAABKRJF" varchar(5) NOT NULL default ' ',
  "AAABBTSL" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABININ","AAABBRWU","AAABBRWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRWW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBSLU" varchar(2) NOT NULL default ' ',
  "AAABBRXA" varchar(1) NOT NULL default ' ',
  "AAABBRXB" varchar(2) NOT NULL default ' ',
  "AAABBRXC" varchar(2) NOT NULL default ' ',
  "AAABBRXD" varchar(2) NOT NULL default ' ',
  "AAABBRXE" varchar(1) NOT NULL default ' ',
  "AAABBRXF" varchar(1) NOT NULL default ' ',
  "AAABBRXG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBSLU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRXK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBRYA" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBRYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRXR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBRYA" varchar(2) NOT NULL default ' ',
  "AAABBRXW" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABBRYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRYB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBRYR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBRYR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRYI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBRYR" varchar(2) NOT NULL default ' ',
  "AAABBRYN" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABBRYR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRYS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABEHHL" varchar(4) NOT NULL default '0000',
  "AAABLUTZ" varchar(2) NOT NULL default '00',
  "AAABKYSP" varchar(5) NOT NULL default ' ',
  "AAABJRKN" varchar(10) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABFVIF" varchar(1) NOT NULL default ' ',
  "AAABLMKH" varchar(8) NOT NULL default '00000000',
  "AAABLIRO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABEHHL","AAABLUTZ","AAABKYSP","AAABJRKN","AAABMCWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRZK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBSAX" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBSAX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBRZR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBSAX" varchar(8) NOT NULL default ' ',
  "AAABBRZW" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABBSAX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSAB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBSAY" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBSAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSAI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBSAY" varchar(3) NOT NULL default ' ',
  "AAABBSAN" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABBSAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSAS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBSBB" varchar(18) NOT NULL default ' ',
  "AAABBSAW" varchar(18) NOT NULL default ' ',
  "AAABBSAX" varchar(8) NOT NULL default ' ',
  "AAABBSAY" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBSBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSBC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABBTSN" varchar(1) NOT NULL default ' ',
  "AAABBSCM" varchar(1) NOT NULL default ' ',
  "AAABBSBI" varchar(1) NOT NULL default ' ',
  "AAABBSCH" varchar(1) NOT NULL default ' ',
  "AAABBSCI" varchar(2) NOT NULL default ' ',
  "AAABBSCJ" varchar(1) NOT NULL default ' ',
  "AAABBTXR" varchar(1) NOT NULL default ' ',
  "AAABBSCL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSBR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABBSCH" varchar(1) NOT NULL default ' ',
  "AAABBSCI" varchar(2) NOT NULL default ' ',
  "AAABBTXR" varchar(1) NOT NULL default ' ',
  "AAABBSCL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSCC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABBTSN" varchar(1) NOT NULL default ' ',
  "AAABBSCH" varchar(1) NOT NULL default ' ',
  "AAABBSCI" varchar(2) NOT NULL default ' ',
  "AAABBSCJ" varchar(1) NOT NULL default ' ',
  "AAABBTXR" varchar(1) NOT NULL default ' ',
  "AAABBSCL" varchar(1) NOT NULL default ' ',
  "AAABBSCM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSCQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABILRJ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSDA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABILRJ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSDK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABILRJ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSDU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABFNHW" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABFNHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSEB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBSII" varchar(6) NOT NULL default ' ',
  "AAABBSEF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBSII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSEJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBSII" varchar(6) NOT NULL default ' ',
  "AAABLDRF" varchar(2) NOT NULL default '00',
  "AAABHAHM" decimal(17,2) NOT NULL default '0.00',
  "AAABKWVR" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBSII","AAABLDRF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSEU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBUXV" varchar(6) NOT NULL default ' ',
  "AAABLDRF" varchar(2) NOT NULL default ' ',
  "AAABHHLQ" varchar(24) NOT NULL default ' ',
  "AAABBSFK" varchar(2) NOT NULL default ' ',
  "AAABBSHT" varchar(24) NOT NULL default ' ',
  "AAABBSFC" decimal(5,2) NOT NULL default '0.00',
  "AAABBSFD" varchar(6) NOT NULL default ' ',
  "AAABBSFE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBUXV","AAABLDRF","AAABHHLQ","AAABBSFK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSFL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBSGE" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBSGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSFS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBUXV" varchar(6) NOT NULL default ' ',
  "AAABBSGE" varchar(6) NOT NULL default ' ',
  "AAABKWVR" decimal(13,2) NOT NULL default '0.00',
  "AAABBSFY" varchar(1) NOT NULL default ' ',
  "AAABBSGR" decimal(9,4) NOT NULL default '0.0000',
  "AAABBSGS" decimal(9,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBUXV","AAABBSGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSGF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLNZQ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLNZQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSGM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBUXV" varchar(6) NOT NULL default ' ',
  "AAABLNZQ" varchar(6) NOT NULL default ' ',
  "AAABBSGR" decimal(9,4) NOT NULL default '0.0000',
  "AAABBSGS" decimal(9,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBUXV","AAABLNZQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSGX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBUXV" varchar(6) NOT NULL default ' ',
  "AAABLNZQ" varchar(6) NOT NULL default ' ',
  "AAABLDRF" varchar(2) NOT NULL default '00',
  "AAABHAHM" varchar(5) NOT NULL default '00000',
  "AAABKWVR" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBUXV","AAABLNZQ","AAABLDRF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSHK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBUXV" varchar(6) NOT NULL default ' ',
  "AAABLDRF" varchar(2) NOT NULL default '00',
  "AAABBSHP" varchar(24) NOT NULL default ' ',
  "AAABHTKN" varchar(24) NOT NULL default ' ',
  "AAABBSHR" varchar(24) NOT NULL default ' ',
  "AAABFUAM" varchar(6) NOT NULL default ' ',
  "AAABBSHT" varchar(24) NOT NULL default ' ',
  "AAABBSHU" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBUXV","AAABLDRF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBSII" varchar(6) NOT NULL default ' ',
  "AAABJEUU" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABBSII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSIJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBUXV" varchar(6) NOT NULL default ' ',
  "AAABMCWX" varchar(3) NOT NULL default ' ',
  "AAABBTHO" decimal(9,6) NOT NULL default '0.000000',
  "AAABBTHP" decimal(9,6) NOT NULL default '0.000000',
  "AAABBTHR" varchar(1) NOT NULL default ' ',
  "AAABBTHS" varchar(3) NOT NULL default '000',
  "AAABBTHT" varchar(4) NOT NULL default '0000',
  "AAABBTHU" varchar(1) NOT NULL default ' ',
  "AAABBTHV" varchar(3) NOT NULL default '000',
  "AAABBTHW" varchar(4) NOT NULL default '0000',
  "AAABBURT" varchar(4) NOT NULL default ' ',
  "AAABJFIP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBUXV","AAABMCWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSJC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBUXV" varchar(6) NOT NULL default ' ',
  "AAABMCWX" varchar(3) NOT NULL default ' ',
  "AAABBTMQ" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABBUXV","AAABMCWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSJO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBUXV" varchar(6) NOT NULL default ' ',
  "AAABMCWX" varchar(3) NOT NULL default ' ',
  "AAABJRKN" varchar(10) NOT NULL default ' ',
  "AAABJFJV" decimal(9,6) NOT NULL default '0.000000',
  "AAABBURG" varchar(1) NOT NULL default ' ',
  "AAABBURH" varchar(3) NOT NULL default '000',
  "AAABBURI" varchar(4) NOT NULL default '0000',
  "AAABBURJ" varchar(1) NOT NULL default ' ',
  "AAABBURK" varchar(3) NOT NULL default '000',
  "AAABBURL" varchar(4) NOT NULL default '0000',
  "AAABBURP" varchar(1) NOT NULL default ' ',
  "AAABMQNB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBUXV","AAABMCWX","AAABJRKN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSKI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJFIN" varchar(2) NOT NULL default ' ',
  "AAABBSKM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJFIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSKQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJFIN" varchar(2) NOT NULL default ' ',
  "AAABBSKV" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABJFIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSLA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBSLU" varchar(2) NOT NULL default ' ',
  "AAABBSLD" varchar(1) NOT NULL default ' ',
  "AAABBSLE" varchar(1) NOT NULL default ' ',
  "AAABBSLF" varchar(1) NOT NULL default ' ',
  "AAABBSLG" varchar(2) NOT NULL default ' ',
  "AAABBSLH" varchar(1) NOT NULL default ' ',
  "AAABBSLI" varchar(1) NOT NULL default ' ',
  "AAABBSLJ" varchar(1) NOT NULL default ' ',
  "AAABBSLK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBSLU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSLN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABBSLU" varchar(2) NOT NULL default ' ',
  "AAABJBVB" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABBSLU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSLV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBUAB" varchar(10) NOT NULL default ' ',
  "AAABBUYJ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBUAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSMD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJRKN" varchar(10) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABEHHL" varchar(4) NOT NULL default '0000',
  "AAABBSNQ" varchar(2) NOT NULL default '00',
  "AAABBSML" varchar(4) NOT NULL default '0000',
  "AAABBSMM" varchar(2) NOT NULL default '00',
  "AAABBVHN" decimal(13,2) NOT NULL default '0.00',
  "AAABBVHO" decimal(13,2) NOT NULL default '0.00',
  "AAABBSMP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJRKN","AAABMCWV","AAABMPWL","AAABEHHL","AAABBSNQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSMX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJRKN" varchar(10) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABEHHL" varchar(4) NOT NULL default '0000',
  "AAABBSNQ" varchar(2) NOT NULL default '00',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQCO" varchar(3) NOT NULL default '000',
  "AAABMCPO" varchar(10) NOT NULL default ' ',
  "AAABIAUZ" varchar(3) NOT NULL default '000',
  "AAABBSNJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJRKN","AAABMCWV","AAABMPWL","AAABEHHL","AAABBSNQ","AAABMQCJ","AAABMQCO"),
  KEY "AAABBSNT" ("AAABMREX","AAABMQCJ","AAABEHHL","AAABMQCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSNY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBSOE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBSOE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSOF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABBSPA" varchar(5) NOT NULL default ' ',
  "AAABBUAB" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCWV","AAABBSPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSOP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCWX" varchar(3) NOT NULL default ' ',
  "AAABBSPA" varchar(5) NOT NULL default ' ',
  "AAABBUAB" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCWV","AAABMCWX","AAABBSPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSPB" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABBSPE" varchar(1) NOT NULL default ' ',
  "AAABBSPF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSPH" (
  "AAABBSPT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABBSPT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSPK" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABBSPT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSPO" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBSPT" varchar(10) NOT NULL default ' ',
  "AAABILRJ" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABBSPT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSPU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBSPX" varchar(6) NOT NULL default ' ',
  "AAABBSPY" varchar(6) NOT NULL default ' ',
  "AAABBSPZ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSQC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVV" varchar(4) NOT NULL default ' ',
  "AAABMCVW" varchar(10) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABBSQH" varchar(2) NOT NULL default ' ',
  "AAABKRJU" varchar(8) NOT NULL default '00000000',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVV","AAABMCVW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSQN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBSQQ" varchar(1) NOT NULL default ' ',
  "AAABBSQR" varchar(1) NOT NULL default ' ',
  "AAABBSQS" varchar(1) NOT NULL default ' ',
  "AAABBSQT" varchar(1) NOT NULL default ' ',
  "AAABBSZM" decimal(17,2) NOT NULL default '0.00',
  "AAABBSQV" decimal(17,2) NOT NULL default '0.00',
  "AAABBSQW" decimal(17,2) NOT NULL default '0.00',
  "AAABBSQX" varchar(1) NOT NULL default ' ',
  "AAABBSQY" varchar(1) NOT NULL default ' ',
  "AAABHEHA" varchar(4) NOT NULL default '0000',
  "AAABLUTZ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSRD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBSRI" varchar(12) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBSRI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSRJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBTDZ" varchar(12) NOT NULL default ' ',
  "AAABJRKN" varchar(10) NOT NULL default ' ',
  "AAABBUBB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBTDZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSRS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBTDZ" varchar(12) NOT NULL default ' ',
  "AAABBTEA" varchar(30) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCWX" varchar(3) NOT NULL default ' ',
  "AAABBTCD" varchar(3) NOT NULL default ' ',
  "AAABBSSA" decimal(17,2) NOT NULL default '0.00',
  "AAABBSSB" decimal(17,2) NOT NULL default '0.00',
  "AAABBTHP" decimal(12,8) NOT NULL default '0.00000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBTDZ","AAABBTEA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSSH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBTDZ" varchar(12) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABBTEA" varchar(30) NOT NULL default ' ',
  "AAABBSSN" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBTDZ","AAABMRHS","AAABBTEA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSST" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBTCD" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBTCD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSTA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABBTCD" varchar(3) NOT NULL default ' ',
  "AAABBSTF" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMRHS","AAABBTCD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSTK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABIYUS" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABIYUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSTR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIYUS" varchar(3) NOT NULL default ' ',
  "AAABBSTW" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMRHS","AAABIYUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSUB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBTDZ" varchar(12) NOT NULL default ' ',
  "AAABBTEA" varchar(30) NOT NULL default ' ',
  "AAABBSZA" varchar(2) NOT NULL default ' ',
  "AAABIYUS" varchar(3) NOT NULL default ' ',
  "AAABBTCD" varchar(3) NOT NULL default ' ',
  "AAABDIIT" varchar(8) NOT NULL default '00000000',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABBTCJ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBTDZ","AAABBTEA","AAABBSZA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSUU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBTDZ" varchar(12) NOT NULL default ' ',
  "AAABBTEA" varchar(30) NOT NULL default ' ',
  "AAABBSZA" varchar(2) NOT NULL default ' ',
  "AAABLTWK" varchar(30) NOT NULL default ' ',
  "AAABIYUS" varchar(3) NOT NULL default ' ',
  "AAABBTCD" varchar(3) NOT NULL default ' ',
  "AAABDIIT" varchar(8) NOT NULL default '00000000',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABBTCJ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBTDZ","AAABBTEA","AAABBSZA","AAABLTWK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSVP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBTDZ" varchar(12) NOT NULL default ' ',
  "AAABBTEA" varchar(30) NOT NULL default ' ',
  "AAABBSZA" varchar(2) NOT NULL default ' ',
  "AAABLTWK" varchar(30) NOT NULL default ' ',
  "AAABKRJE" varchar(3) NOT NULL default ' ',
  "AAABIYUS" varchar(3) NOT NULL default ' ',
  "AAABBTCD" varchar(3) NOT NULL default ' ',
  "AAABDIIT" varchar(8) NOT NULL default '00000000',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABBTCJ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBTDZ","AAABBTEA","AAABBSZA","AAABLTWK","AAABKRJE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSWM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBTDZ" varchar(12) NOT NULL default ' ',
  "AAABBTEA" varchar(30) NOT NULL default ' ',
  "AAABBSZA" varchar(2) NOT NULL default ' ',
  "AAABLTWK" varchar(30) NOT NULL default ' ',
  "AAABKRJE" varchar(3) NOT NULL default ' ',
  "AAABKRJF" varchar(5) NOT NULL default ' ',
  "AAABIYUS" varchar(3) NOT NULL default ' ',
  "AAABBTCD" varchar(3) NOT NULL default ' ',
  "AAABDIIT" varchar(8) NOT NULL default '00000000',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABBTCJ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBTDZ","AAABBTEA","AAABBSZA","AAABLTWK","AAABKRJE","AAABKRJF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSXL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBTDZ" varchar(12) NOT NULL default ' ',
  "AAABBTEA" varchar(30) NOT NULL default ' ',
  "AAABBSZA" varchar(2) NOT NULL default ' ',
  "AAABKRJE" varchar(3) NOT NULL default ' ',
  "AAABIYUS" varchar(3) NOT NULL default ' ',
  "AAABBTCD" varchar(3) NOT NULL default ' ',
  "AAABDIIT" varchar(8) NOT NULL default '00000000',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABBTCJ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBTDZ","AAABBTEA","AAABBSZA","AAABKRJE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSYG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBTDZ" varchar(12) NOT NULL default ' ',
  "AAABBTEA" varchar(30) NOT NULL default ' ',
  "AAABBSZA" varchar(2) NOT NULL default ' ',
  "AAABKRJE" varchar(3) NOT NULL default ' ',
  "AAABKRJF" varchar(5) NOT NULL default ' ',
  "AAABIYUS" varchar(3) NOT NULL default ' ',
  "AAABBTCD" varchar(3) NOT NULL default ' ',
  "AAABDIIT" varchar(8) NOT NULL default '00000000',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABBTCJ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBTDZ","AAABBTEA","AAABBSZA","AAABKRJE","AAABKRJF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSZD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBTDZ" varchar(12) NOT NULL default ' ',
  "AAABBTEA" varchar(30) NOT NULL default ' ',
  "AAABLTWK" varchar(30) NOT NULL default ' ',
  "AAABBSZJ" varchar(1) NOT NULL default ' ',
  "AAABBSZK" decimal(17,2) NOT NULL default '0.00',
  "AAABBSZL" decimal(17,2) NOT NULL default '0.00',
  "AAABBSZM" decimal(17,2) NOT NULL default '0.00',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABKUQR" varchar(8) NOT NULL default '00000000',
  "AAABLWWO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBTDZ","AAABBTEA","AAABLTWK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBSZV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABIYUS" varchar(3) NOT NULL default ' ',
  "AAABBTCU" varchar(4) NOT NULL default ' ',
  "AAABKGKQ" varchar(2) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABIYUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABIYUS" varchar(3) NOT NULL default ' ',
  "AAABKRJE" varchar(3) NOT NULL default ' ',
  "AAABBTCU" varchar(4) NOT NULL default ' ',
  "AAABKGKQ" varchar(2) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABIYUS","AAABKRJE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTAR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABIYUS" varchar(3) NOT NULL default ' ',
  "AAABKRJE" varchar(3) NOT NULL default ' ',
  "AAABKRJF" varchar(5) NOT NULL default ' ',
  "AAABBTCU" varchar(4) NOT NULL default ' ',
  "AAABKGKQ" varchar(2) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABIYUS","AAABKRJE","AAABKRJF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTBF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBTDZ" varchar(12) NOT NULL default ' ',
  "AAABBTEA" varchar(30) NOT NULL default ' ',
  "AAABIYUS" varchar(3) NOT NULL default ' ',
  "AAABBTCD" varchar(3) NOT NULL default ' ',
  "AAABDIIT" varchar(8) NOT NULL default '00000000',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABBTCJ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBTDZ","AAABBTEA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTBW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBTDZ" varchar(12) NOT NULL default ' ',
  "AAABBTEA" varchar(30) NOT NULL default ' ',
  "AAABBTDA" varchar(2) NOT NULL default ' ',
  "AAABIYUS" varchar(3) NOT NULL default ' ',
  "AAABBTCD" varchar(3) NOT NULL default ' ',
  "AAABDIIT" varchar(8) NOT NULL default '00000000',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABBTCJ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBTDZ","AAABBTEA","AAABBTDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTCP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABIYUS" varchar(3) NOT NULL default ' ',
  "AAABBTDA" varchar(2) NOT NULL default ' ',
  "AAABBTCU" varchar(4) NOT NULL default ' ',
  "AAABKGKQ" varchar(2) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABIYUS","AAABBTDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTDB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBTDZ" varchar(12) NOT NULL default ' ',
  "AAABBTDK" varchar(30) NOT NULL default ' ',
  "AAABBTDL" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBTDZ","AAABBTDK","AAABBTDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTDM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSEB" varchar(16) NOT NULL default '0000000000000000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABBTDZ" varchar(12) NOT NULL default ' ',
  "AAABBTEA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSEB","AAABMREX","AAABMDBA","AAABMDBB","AAABBTDZ","AAABBTEA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTEB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCXS" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTEI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABEHHL" varchar(4) NOT NULL default '0000',
  "AAABMPVB" varchar(3) NOT NULL default '000',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKWJY","AAABEHHL","AAABMPVB","AAABMCWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTEV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCXS" varchar(3) NOT NULL default ' ',
  "AAABHMSB" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABMCXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTFF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABEHHL" varchar(4) NOT NULL default '0000',
  "AAABMPVB" varchar(3) NOT NULL default '000',
  "AAABLUVE" varchar(3) NOT NULL default '000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABLQCR" varchar(12) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABBTFR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKWJY","AAABEHHL","AAABMPVB","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTFY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABEHHL" varchar(4) NOT NULL default '0000',
  "AAABMPVB" varchar(3) NOT NULL default '000',
  "AAABLUVE" varchar(3) NOT NULL default '000',
  "AAABMJWN" varchar(8) NOT NULL default ' ',
  "AAABBWFO" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKWJY","AAABEHHL","AAABMPVB","AAABLUVE","AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTGO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABEHHL" varchar(4) NOT NULL default '0000',
  "AAABMPVB" varchar(3) NOT NULL default '000',
  "AAABLUVE" varchar(3) NOT NULL default '000',
  "AAABJYWC" varchar(1) NOT NULL default ' ',
  "AAABBWGQ" varchar(3) NOT NULL default '000',
  "AAABMNMZ" varchar(25) NOT NULL default ' ',
  "AAABMNNA" varchar(25) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIFAF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKWJY","AAABEHHL","AAABMPVB","AAABLUVE","AAABJYWC","AAABBWGQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTHJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCWX" varchar(3) NOT NULL default ' ',
  "AAABBTHO" decimal(9,6) NOT NULL default '0.000000',
  "AAABBTHP" decimal(9,6) NOT NULL default '0.000000',
  "AAABBTND" varchar(1) NOT NULL default ' ',
  "AAABBTHR" varchar(1) NOT NULL default ' ',
  "AAABBTHS" varchar(3) NOT NULL default '000',
  "AAABBTHT" varchar(4) NOT NULL default '0000',
  "AAABBTHU" varchar(1) NOT NULL default ' ',
  "AAABBTHV" varchar(3) NOT NULL default '000',
  "AAABBTHW" varchar(4) NOT NULL default '0000',
  "AAABBURT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCWV","AAABMCWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTIC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBTMH" varchar(10) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABBTLC" varchar(3) NOT NULL default ' ',
  "AAABIKHD" varchar(3) NOT NULL default '000',
  "AAABBXKE" varchar(4) NOT NULL default '0000',
  "AAABJFIT" varchar(1) NOT NULL default ' ',
  "AAABIKHE" varchar(3) NOT NULL default '000',
  "AAABBTIM" varchar(4) NOT NULL default '0000',
  "AAABHIIX" varchar(8) NOT NULL default '00000000',
  "AAABLOBA" varchar(8) NOT NULL default '00000000',
  "AAABBTLX" varchar(4) NOT NULL default '0000',
  "AAABBTLY" varchar(3) NOT NULL default '000',
  "AAABBTLZ" varchar(50) NOT NULL default ' ',
  "AAABBTIS" varchar(1) NOT NULL default ' ',
  "AAABBTIT" varchar(1) NOT NULL default ' ',
  "AAABBTIU" varchar(1) NOT NULL default ' ',
  "AAABBTMB" decimal(18,2) NOT NULL default '0.00',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABBTMD" varchar(12) NOT NULL default ' ',
  "AAABLFOX" varchar(1) NOT NULL default ' ',
  "AAABLIRO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBTMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTJD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBTMH" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMREX","AAABBTMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBTMH" varchar(10) NOT NULL default ' ',
  "AAABLKHW" varchar(2) NOT NULL default ' ',
  "AAABLTLR" varchar(10) NOT NULL default ' ',
  "AAABJFJL" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBTMH","AAABLKHW","AAABLTLR","AAABJFJL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTKB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTLR" varchar(10) NOT NULL default ' ',
  "AAABBTKE" varchar(10) NOT NULL default ' ',
  "AAABBTKF" varchar(1) NOT NULL default ' ',
  "AAABBTKG" varchar(1) NOT NULL default ' ',
  "AAABBTKH" varchar(40) NOT NULL default ' ',
  "AAABBTKI" varchar(20) NOT NULL default ' ',
  "AAABBTMB" decimal(18,2) NOT NULL default '0.00',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABBTMD" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLTLR" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBTMH" varchar(10) NOT NULL default ' ',
  "AAABBTLN" varchar(3) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABBTLC" varchar(3) NOT NULL default ' ',
  "AAABBTMB" decimal(18,2) NOT NULL default '0.00',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABBTMD" varchar(12) NOT NULL default ' ',
  "AAABLIRO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBTMH","AAABBTLN"),
  KEY "AAABBTLL" ("AAABMCWV","AAABBTLN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTLO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBTMH" varchar(10) NOT NULL default ' ',
  "AAABLKHW" varchar(2) NOT NULL default ' ',
  "AAABLTLR" varchar(10) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABHIIX" varchar(8) NOT NULL default '00000000',
  "AAABLOBA" varchar(8) NOT NULL default '00000000',
  "AAABBTLX" varchar(4) NOT NULL default '0000',
  "AAABBTLY" varchar(3) NOT NULL default '000',
  "AAABBTLZ" varchar(50) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAABBTMB" decimal(18,2) NOT NULL default '0.00',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABBTMD" varchar(12) NOT NULL default ' ',
  "AAABLIRO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBTMH","AAABLKHW","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTMK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCWX" varchar(3) NOT NULL default ' ',
  "AAABBTMQ" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABMCWV","AAABMCWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTMW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCXS" varchar(3) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABMCWX" varchar(3) NOT NULL default ' ',
  "AAABBTND" varchar(1) NOT NULL default ' ',
  "AAABBVHK" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCWV","AAABMCXS","AAABHZPH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTNK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKGBX" varchar(8) NOT NULL default ' ',
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABIAWX" varchar(2) NOT NULL default '00',
  "AAABBTOB" varchar(1) NOT NULL default ' ',
  "AAABBTNT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABICIU","AAABMREX","AAABKGBX","AAABLUAV","AAABKAFO","AAABIAWX","AAABBTOB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTOC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBTOW" varchar(1) NOT NULL default ' ',
  "AAABBTOX" varchar(1) NOT NULL default ' ',
  "AAABBTOY" varchar(1) NOT NULL default ' ',
  "AAABBTOZ" varchar(1) NOT NULL default ' ',
  "AAABFLVN" varchar(3) NOT NULL default '000',
  "AAABBTOJ" varchar(1) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABKGMO" varchar(3) NOT NULL default ' ',
  "AAABBTOM" varchar(1) NOT NULL default ' ',
  "AAABLLJG" varchar(3) NOT NULL default ' ',
  "AAABBTOO" varchar(2) NOT NULL default ' ',
  "AAABBTOP" varchar(1) NOT NULL default ' ',
  "AAABKGKQ" varchar(2) NOT NULL default ' ',
  "AAABBTOR" varchar(1) NOT NULL default ' ',
  "AAABBTOS" varchar(1) NOT NULL default ' ',
  "AAABBTOT" varchar(1) NOT NULL default ' ',
  "AAABBTOU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBTOW","AAABBTOX","AAABBTOY","AAABBTOZ","AAABFLVN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTPB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBTPH" varchar(2) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABMCRT" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBTPH","AAABMPLS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTPJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABFLEO" varchar(2) NOT NULL default ' ',
  "AAABIAWX" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABFLEO","AAABIAWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTPS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBTPX" varchar(3) NOT NULL default ' ',
  "AAABBTPV" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBTPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRT" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTQD" (
  "AAABKMUF" varchar(30) NOT NULL default ' ',
  "AAABFNSU" varchar(30) NOT NULL default ' ',
  "AAABBTQG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABKMUF","AAABFNSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTQJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABHXNQ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTQP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCWX" varchar(3) NOT NULL default ' ',
  "AAABJFJL" varchar(4) NOT NULL default ' ',
  "AAABBTQV" varchar(6) NOT NULL default ' ',
  "AAABBTQW" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCWV","AAABMCWX","AAABJFJL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTRC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUWW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUWW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTRH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUWW" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUWW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTRP" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABBTSU" varchar(18) NOT NULL default ' ',
  "AAABMPVB" varchar(3) NOT NULL default '000',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABBWRA" varchar(6) NOT NULL default ' ',
  "AAABBWRB" varchar(3) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAABIBNO" varchar(4) NOT NULL default ' ',
  "AAABBTTF" varchar(10) NOT NULL default ' ',
  "AAABBTTG" varchar(12) NOT NULL default ' ',
  "AAABBTTH" varchar(8) NOT NULL default '00000000',
  "AAABBTSC" varchar(10) NOT NULL default ' ',
  "AAABBTTI" varchar(12) NOT NULL default ' ',
  "AAABBTSE" varchar(4) NOT NULL default ' ',
  "AAABBTSF" varchar(3) NOT NULL default ' ',
  "AAABBTSG" varchar(2) NOT NULL default ' ',
  "AAABBWRC" varchar(10) NOT NULL default ' ',
  "AAABBTSI" varchar(4) NOT NULL default ' ',
  "AAABKRJE" varchar(3) NOT NULL default ' ',
  "AAABKRJF" varchar(5) NOT NULL default ' ',
  "AAABBTSL" varchar(24) NOT NULL default ' ',
  "AAABBTSM" varchar(3) NOT NULL default ' ',
  "AAABBTSN" varchar(1) NOT NULL default ' ',
  "AAABBTSO" varchar(1) NOT NULL default ' ',
  "AAABBTTA" varchar(22) NOT NULL default ' ',
  "AAABBTSQ" varchar(4) NOT NULL default ' ',
  "AAABBTSR" varchar(4) NOT NULL default ' ',
  "AAABBTSS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABBTSU"),
  KEY "AAABBTSV" ("AAABBWRA","AAABBTTF","AAABBTTG","AAABBTTH","AAABBTTA","AAABKRJF"),
  KEY "AAABBTTC" ("AAABBWRA","AAABMQHZ","AAABBTTF","AAABBTTG","AAABBTTH","AAABBTTI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTTJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCWW" varchar(2) NOT NULL default ' ',
  "AAABBVCM" varchar(1) NOT NULL default ' ',
  "AAABBVCN" varchar(1) NOT NULL default ' ',
  "AAABBVCP" varchar(2) NOT NULL default ' ',
  "AAABBTTQ" varchar(1) NOT NULL default ' ',
  "AAABBTTR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCWW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABFOQZ" varchar(32) NOT NULL default ' ',
  "AAABBTTZ" varchar(3) NOT NULL default '000',
  "AAABLLJG" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABKWJY","AAABFOQZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTUE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCWW" varchar(2) NOT NULL default ' ',
  "AAABILRJ" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABMCWW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTUO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLENG" varchar(4) NOT NULL default ' ',
  "AAABBTUY" varchar(10) NOT NULL default ' ',
  "AAABBTUT" varchar(10) NOT NULL default ' ',
  "AAABBTUU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLENG","AAABBTUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTUZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBTXR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBTXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTVG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABFLBM" varchar(4) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABBTVW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABFLBM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTVP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABFLBM" varchar(4) NOT NULL default ' ',
  "AAABBTWA" varchar(2) NOT NULL default ' ',
  "AAABBTVU" varchar(2) NOT NULL default ' ',
  "AAABBTVV" varchar(2) NOT NULL default ' ',
  "AAABBTVW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABFLBM","AAABBTWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTWB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABFLBM" varchar(4) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABMCRJ","AAABFLBM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTWL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBUXV" varchar(6) NOT NULL default ' ',
  "AAABBTXR" varchar(1) NOT NULL default ' ',
  "AAABBTWQ" decimal(11,6) NOT NULL default '0.000000',
  "AAABBTWR" decimal(11,6) NOT NULL default '0.000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBUXV","AAABBTXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTWW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBUXV" varchar(6) NOT NULL default ' ',
  "AAABBTXR" varchar(1) NOT NULL default ' ',
  "AAABLUWW" varchar(1) NOT NULL default ' ',
  "AAABIKEM" decimal(11,6) NOT NULL default '0.000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBUXV","AAABBTXR","AAABLUWW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTXI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBTXR" varchar(1) NOT NULL default ' ',
  "AAABBTXN" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABBTXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTXS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBUYJ" varchar(6) NOT NULL default ' ',
  "AAABMBTQ" varchar(16) NOT NULL default ' ',
  "AAABBTXX" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBUYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTYB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKXW" varchar(10) NOT NULL default ' ',
  "AAABBTYE" varchar(1) NOT NULL default ' ',
  "AAABBTYF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTYI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(6) NOT NULL default ' ',
  "AAABLUVE" varchar(2) NOT NULL default '00',
  "AAABBTYM" varchar(10) NOT NULL default ' ',
  "AAABBTYN" varchar(1) NOT NULL default ' ',
  "AAABBTYO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLPV","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTYS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBTYV" smallint(6) NOT NULL default '0',
  "AAABDHZV" varchar(1) NOT NULL default ' ',
  "AAABBTYX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBUYJ" varchar(6) NOT NULL default ' ',
  "AAABBTZF" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABBUYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTZK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQBZ" varchar(4) NOT NULL default ' ',
  "AAABLECD" varchar(2) NOT NULL default ' ',
  "AAABLUVE" varchar(2) NOT NULL default '00',
  "AAABMLPV" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMQBZ","AAABLECD","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBTZW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBUYJ" varchar(6) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABBUAB" varchar(5) NOT NULL default ' ',
  "AAABBUAC" varchar(5) NOT NULL default ' ',
  "AAABBUAD" varchar(10) NOT NULL default ' ',
  "AAABBUAE" varchar(10) NOT NULL default ' ',
  "AAABBUAF" decimal(9,6) NOT NULL default '0.000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBUYJ","AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUAK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJRKN" varchar(10) NOT NULL default ' ',
  "AAABJFJL" varchar(4) NOT NULL default ' ',
  "AAABBUAP" decimal(13,0) NOT NULL default '0',
  "AAABHCQW" varchar(1) NOT NULL default ' ',
  "AAABHESR" varchar(1) NOT NULL default ' ',
  "AAABBUAS" varchar(1) NOT NULL default ' ',
  "AAABJFIR" varchar(4) NOT NULL default ' ',
  "AAABFFWW" varchar(2) NOT NULL default ' ',
  "AAABBUAV" varchar(12) NOT NULL default ' ',
  "AAABBUNM" varchar(3) NOT NULL default ' ',
  "AAABBUAX" decimal(13,0) NOT NULL default '0',
  "AAABBUAY" decimal(13,0) NOT NULL default '0',
  "AAABIGSL" varchar(30) NOT NULL default ' ',
  "AAABEGXI" varchar(15) NOT NULL default ' ',
  "AAABBUBB" varchar(1) NOT NULL default ' ',
  "AAABBVHN" decimal(13,2) NOT NULL default '0.00',
  "AAABBVHO" decimal(13,2) NOT NULL default '0.00',
  "AAABBUYO" varchar(1) NOT NULL default ' ',
  "AAABBUBF" varchar(1) NOT NULL default ' ',
  "AAABBVHT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJRKN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUBK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJRKN" varchar(10) NOT NULL default ' ',
  "AAABMQNB" varchar(4) NOT NULL default ' ',
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJRKN","AAABMQNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUBU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJRKN" varchar(10) NOT NULL default ' ',
  "AAABMQNB" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABJRKN","AAABMQNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUCG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBUNM" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBUNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUCN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABININ" varchar(4) NOT NULL default ' ',
  "AAABBUJI" varchar(10) NOT NULL default ' ',
  "AAABBUCW" varchar(24) NOT NULL default ' ',
  "AAABBULQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABININ","AAABBUJI","AAABBUCW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUCX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABININ" varchar(4) NOT NULL default ' ',
  "AAABBUJI" varchar(10) NOT NULL default ' ',
  "AAABBUDB" varchar(1) NOT NULL default ' ',
  "AAABBUDC" varchar(1) NOT NULL default ' ',
  "AAABBUDD" varchar(1) NOT NULL default ' ',
  "AAABBUDE" varchar(1) NOT NULL default ' ',
  "AAABBUDF" varchar(1) NOT NULL default ' ',
  "AAABBUDG" varchar(1) NOT NULL default ' ',
  "AAABBUDH" varchar(1) NOT NULL default ' ',
  "AAABBUDI" varchar(1) NOT NULL default ' ',
  "AAABBUDJ" varchar(1) NOT NULL default ' ',
  "AAABBUDK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABININ","AAABBUJI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUDO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABININ" varchar(4) NOT NULL default ' ',
  "AAABBUMR" varchar(10) NOT NULL default ' ',
  "AAABBUEA" varchar(10) NOT NULL default ' ',
  "AAABGZIA" varchar(2) NOT NULL default '00',
  "AAABMJCM" varchar(1) NOT NULL default ' ',
  "AAABBUDV" varchar(10) NOT NULL default ' ',
  "AAABBUDW" decimal(10,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABLUXQ","AAABININ","AAABBUMR","AAABBUEA","AAABGZIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUEC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABININ" varchar(4) NOT NULL default ' ',
  "AAABBUJI" varchar(10) NOT NULL default ' ',
  "AAABBUEH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABININ","AAABBUJI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUEM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABININ" varchar(4) NOT NULL default ' ',
  "AAABBUJI" varchar(10) NOT NULL default ' ',
  "AAABBUEW" varchar(24) NOT NULL default ' ',
  "AAABBULQ" varchar(1) NOT NULL default ' ',
  "AAABBUES" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABININ","AAABBUJI","AAABBUEW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUEX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABININ" varchar(4) NOT NULL default ' ',
  "AAABBUJI" varchar(10) NOT NULL default ' ',
  "AAABBUFI" varchar(10) NOT NULL default ' ',
  "AAABJEXI" varchar(8) NOT NULL default '00000000',
  "AAABBUFK" varchar(12) NOT NULL default ' ',
  "AAABBUFL" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABININ","AAABBUJI","AAABBUFI","AAABJEXI","AAABBUFK","AAABBUFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUFM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABJTAQ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMOHM","AAABJTAQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUFV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABININ" varchar(4) NOT NULL default ' ',
  "AAABBUMR" varchar(10) NOT NULL default ' ',
  "AAABBUGT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABININ","AAABBUMR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUGF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABININ" varchar(4) NOT NULL default ' ',
  "AAABBUMR" varchar(10) NOT NULL default ' ',
  "AAABBUGJ" varchar(10) NOT NULL default ' ',
  "AAABIWPG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABININ","AAABBUMR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUGO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABININ" varchar(4) NOT NULL default ' ',
  "AAABBUMR" varchar(10) NOT NULL default ' ',
  "AAABBUGT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABININ","AAABBUMR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUGY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABININ" varchar(4) NOT NULL default ' ',
  "AAABBUXV" varchar(6) NOT NULL default ' ',
  "AAABBUMY" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABLUXQ","AAABININ","AAABBUXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUHG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABININ" varchar(4) NOT NULL default ' ',
  "AAABBUJI" varchar(10) NOT NULL default ' ',
  "AAABJTAQ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABININ","AAABBUJI","AAABJTAQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUHP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABININ" varchar(4) NOT NULL default ' ',
  "AAABJTAQ" varchar(10) NOT NULL default ' ',
  "AAABBUHU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABININ","AAABJTAQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUHZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABININ" varchar(4) NOT NULL default ' ',
  "AAABJTAQ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABININ","AAABJTAQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUIG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABININ" varchar(4) NOT NULL default ' ',
  "AAABBUMY" varchar(10) NOT NULL default '0000000000',
  "AAABBULV" varchar(10) NOT NULL default ' ',
  "AAABBUKA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABININ","AAABBUMY","AAABBULV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUIQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABININ" varchar(4) NOT NULL default ' ',
  "AAABBUXV" varchar(6) NOT NULL default ' ',
  "AAABBULV" varchar(10) NOT NULL default ' ',
  "AAABBUIW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABININ","AAABBUXV","AAABBULV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUJC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABININ" varchar(4) NOT NULL default ' ',
  "AAABBUXV" varchar(6) NOT NULL default ' ',
  "AAABBULV" varchar(10) NOT NULL default ' ',
  "AAABBUNC" varchar(6) NOT NULL default ' ',
  "AAABBUJI" varchar(10) NOT NULL default ' ',
  "AAABBUMR" varchar(10) NOT NULL default ' ',
  "AAABBUJK" varchar(10) NOT NULL default ' ',
  "AAABBUJL" varchar(10) NOT NULL default ' ',
  "AAABBUJM" varchar(10) NOT NULL default ' ',
  "AAABBUJN" varchar(2) NOT NULL default ' ',
  "AAABBUJO" varchar(2) NOT NULL default ' ',
  "AAABBUJP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABININ","AAABBUXV","AAABBULV","AAABBUNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUJV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABININ" varchar(4) NOT NULL default ' ',
  "AAABBUXV" varchar(6) NOT NULL default ' ',
  "AAABBULV" varchar(10) NOT NULL default ' ',
  "AAABBUKA" varchar(10) NOT NULL default ' ',
  "AAABBUKB" varchar(1) NOT NULL default ' ',
  "AAABBUKC" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABININ","AAABBUXV","AAABBULV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUKH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABJTAQ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMOHM","AAABJTAQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUKQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABININ" varchar(4) NOT NULL default ' ',
  "AAABBUMR" varchar(10) NOT NULL default ' ',
  "AAABCPAG" decimal(17,2) NOT NULL default '0.00',
  "AAABBUMB" decimal(17,2) NOT NULL default '0.00',
  "AAABKWVR" decimal(8,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABLUXQ","AAABININ","AAABBUMR","AAABCPAG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBULB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABJTAQ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMOHM","AAABJTAQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBULK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABININ" varchar(4) NOT NULL default ' ',
  "AAABBUXV" varchar(6) NOT NULL default ' ',
  "AAABBULU" varchar(3) NOT NULL default '000',
  "AAABBULV" varchar(10) NOT NULL default ' ',
  "AAABBULQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABININ","AAABBUXV","AAABBULU","AAABBULV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBULW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABININ" varchar(4) NOT NULL default ' ',
  "AAABBUMR" varchar(10) NOT NULL default ' ',
  "AAABCPAG" decimal(17,2) NOT NULL default '0.00',
  "AAABBUMB" decimal(17,2) NOT NULL default '0.00',
  "AAABKWVR" decimal(8,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABLUXQ","AAABININ","AAABBUMR","AAABCPAG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUMH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABININ" varchar(4) NOT NULL default ' ',
  "AAABBUMR" varchar(10) NOT NULL default ' ',
  "AAABBUMS" varchar(10) NOT NULL default '0000000000',
  "AAABBUMM" varchar(10) NOT NULL default '0000000000',
  "AAABBUMN" varchar(6) NOT NULL default ' ',
  "AAABBUMO" decimal(17,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABLUXQ","AAABININ","AAABBUMR","AAABBUMS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUMT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABININ" varchar(4) NOT NULL default ' ',
  "AAABBUXV" varchar(6) NOT NULL default ' ',
  "AAABBUNC" varchar(6) NOT NULL default ' ',
  "AAABBUMY" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABLUXQ","AAABININ","AAABBUXV","AAABBUNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUND" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBUNM" varchar(3) NOT NULL default ' ',
  "AAABBUNI" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABBUNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUNN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABDLQQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABDLQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUNU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBUXV" varchar(6) NOT NULL default ' ',
  "AAABDLQQ" varchar(1) NOT NULL default ' ',
  "AAABBVBM" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBUXV","AAABDLQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUOE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABDLQQ" varchar(1) NOT NULL default ' ',
  "AAABBUOJ" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABDLQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUOO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJEXI" varchar(8) NOT NULL default '00000000',
  "AAABBUOS" varchar(1) NOT NULL default ' ',
  "AAABBUOT" varchar(3) NOT NULL default '000',
  "AAABBUOU" varchar(4) NOT NULL default '0000',
  "AAABBUOV" varchar(1) NOT NULL default ' ',
  "AAABBUOW" varchar(3) NOT NULL default '000',
  "AAABBUOX" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJEXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUPB" (
  "AAABBUPJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABBUPJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUPE" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBUPJ" varchar(10) NOT NULL default ' ',
  "AAABILRJ" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABBUPJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUPK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBUYP" varchar(4) NOT NULL default ' ',
  "AAABMCWW" varchar(2) NOT NULL default ' ',
  "AAABBUQC" varchar(4) NOT NULL default ' ',
  "AAABBUPQ" varchar(30) NOT NULL default ' ',
  "AAABBUPR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBUYP","AAABMCWW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUPW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBUYP" varchar(4) NOT NULL default ' ',
  "AAABMCWW" varchar(2) NOT NULL default ' ',
  "AAABBUQH" varchar(4) NOT NULL default ' ',
  "AAABBUQC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBUYP","AAABMCWW","AAABBUQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUQI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBUYP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBUYP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUQP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBUYP" varchar(4) NOT NULL default ' ',
  "AAABIMUR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABBUYP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUQZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCWX" varchar(3) NOT NULL default ' ',
  "AAABJRKN" varchar(10) NOT NULL default ' ',
  "AAABJFJV" decimal(9,6) NOT NULL default '0.000000',
  "AAABBURG" varchar(1) NOT NULL default ' ',
  "AAABBURH" varchar(3) NOT NULL default '000',
  "AAABBURI" varchar(4) NOT NULL default '0000',
  "AAABBURJ" varchar(1) NOT NULL default ' ',
  "AAABBURK" varchar(3) NOT NULL default '000',
  "AAABBURL" varchar(4) NOT NULL default '0000',
  "AAABLUWW" varchar(1) NOT NULL default ' ',
  "AAABBURN" decimal(9,6) NOT NULL default '0.000000',
  "AAABBURO" decimal(9,6) NOT NULL default '0.000000',
  "AAABBURP" varchar(1) NOT NULL default ' ',
  "AAABBURQ" varchar(3) NOT NULL default '000',
  "AAABBURR" varchar(4) NOT NULL default '0000',
  "AAABBURS" varchar(4) NOT NULL default ' ',
  "AAABBURT" varchar(4) NOT NULL default ' ',
  "AAABMQNB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCWV","AAABMCWX","AAABJRKN"),
  KEY "AAABBUSA" ("AAABMRIL","AAABJRKN","AAABMREX","AAABMCWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUSF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBUSM" varchar(2) NOT NULL default ' ',
  "AAABMLPV" varchar(10) NOT NULL default ' ',
  "AAABJURT" varchar(4) NOT NULL default ' ',
  "AAABBUSK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBUSM","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUSO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVX" varchar(4) NOT NULL default ' ',
  "AAABBUSX" varchar(3) NOT NULL default '000',
  "AAABMCWW" varchar(2) NOT NULL default ' ',
  "AAABBUST" varchar(1) NOT NULL default ' ',
  "AAABBUSU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVX","AAABBUSX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUSY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBVFS" varchar(4) NOT NULL default ' ',
  "AAABBVFA" varchar(1) NOT NULL default ' ',
  "AAABBUTC" varchar(1) NOT NULL default ' ',
  "AAABBUTD" varchar(1) NOT NULL default ' ',
  "AAABBUTE" varchar(1) NOT NULL default ' ',
  "AAABBUTF" varchar(1) NOT NULL default ' ',
  "AAABBUTG" varchar(1) NOT NULL default ' ',
  "AAABBUTH" varchar(1) NOT NULL default ' ',
  "AAABBUTI" varchar(1) NOT NULL default ' ',
  "AAABBUTJ" varchar(1) NOT NULL default ' ',
  "AAABBUTK" varchar(1) NOT NULL default ' ',
  "AAABBUTL" varchar(1) NOT NULL default ' ',
  "AAABBVFO" varchar(1) NOT NULL default ' ',
  "AAABBUTN" varchar(1) NOT NULL default ' ',
  "AAABBUTO" varchar(1) NOT NULL default ' ',
  "AAABBUTP" varchar(15) NOT NULL default ' ',
  "AAABBUTQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBVFS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUTT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCWX" varchar(3) NOT NULL default ' ',
  "AAABMCWW" varchar(2) NOT NULL default ' ',
  "AAABBUTZ" varchar(10) NOT NULL default ' ',
  "AAABBUUA" varchar(8) NOT NULL default '00000000',
  "AAABBUUB" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCWV","AAABMCWX","AAABMCWW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUUH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABBVFS" varchar(4) NOT NULL default ' ',
  "AAABBUUL" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABBVFS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUUP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJFIM" varchar(2) NOT NULL default ' ',
  "AAABBUUT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJFIM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUUX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJFIM" varchar(2) NOT NULL default ' ',
  "AAABJARB" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABJFIM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUVH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBUXV" varchar(6) NOT NULL default ' ',
  "AAABLUUU" varchar(24) NOT NULL default ' ',
  "AAABBUVM" varchar(24) NOT NULL default ' ',
  "AAABBUVN" decimal(5,2) NOT NULL default '0.00',
  "AAABBUVO" decimal(5,2) NOT NULL default '0.00',
  "AAABBVFU" varchar(24) NOT NULL default ' ',
  "AAABBVFV" varchar(24) NOT NULL default ' ',
  "AAABBUVR" varchar(6) NOT NULL default ' ',
  "AAABBUVS" varchar(6) NOT NULL default ' ',
  "AAABBUVT" decimal(5,2) NOT NULL default '0.00',
  "AAABBVFW" varchar(24) NOT NULL default ' ',
  "AAABBUVV" decimal(5,2) NOT NULL default '0.00',
  "AAABBVFX" varchar(24) NOT NULL default ' ',
  "AAABBUVX" varchar(6) NOT NULL default ' ',
  "AAABBUVY" varchar(6) NOT NULL default ' ',
  "AAABBVFY" varchar(24) NOT NULL default ' ',
  "AAABBVGB" varchar(10) NOT NULL default ' ',
  "AAABBUWB" varchar(1) NOT NULL default ' ',
  "AAABBUWC" varchar(10) NOT NULL default ' ',
  "AAABFHOI" decimal(13,2) NOT NULL default '0.00',
  "AAABGOUC" varchar(1) NOT NULL default ' ',
  "AAABJFIN" varchar(2) NOT NULL default ' ',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABLTLE" varchar(8) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABMGSS" varchar(12) NOT NULL default ' ',
  "AAABMGSU" varchar(8) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  "AAABBVFZ" decimal(5,2) NOT NULL default '0.00',
  "AAABBUWN" varchar(1) NOT NULL default ' ',
  "AAABBUWO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBUXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUWS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBUXV" varchar(6) NOT NULL default ' ',
  "AAABBUWX" varchar(40) NOT NULL default ' ',
  "AAABMOQQ" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABBUXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUXD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABBUXH" varchar(4) NOT NULL default ' ',
  "AAABJFIO" varchar(4) NOT NULL default ' ',
  "AAABJFIP" varchar(1) NOT NULL default ' ',
  "AAABMJCM" varchar(10) NOT NULL default ' ',
  "AAABFFWW" varchar(2) NOT NULL default ' ',
  "AAABJFIM" varchar(2) NOT NULL default ' ',
  "AAABJFIT" varchar(1) NOT NULL default ' ',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABLTLE" varchar(8) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABMGSS" varchar(12) NOT NULL default ' ',
  "AAABMGSU" varchar(8) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  "AAABJFIR" varchar(4) NOT NULL default ' ',
  "AAABBUXV" varchar(6) NOT NULL default ' ',
  "AAABBUXW" varchar(1) NOT NULL default ' ',
  "AAABMCXD" varchar(15) NOT NULL default ' ',
  "AAABBUXY" varchar(4) NOT NULL default ' ',
  "AAABBUXZ" varchar(2) NOT NULL default ' ',
  "AAABBUYA" varchar(15) NOT NULL default ' ',
  "AAABBUYB" varchar(4) NOT NULL default ' ',
  "AAABJFIS" varchar(1) NOT NULL default '0',
  "AAABBUYD" decimal(5,2) NOT NULL default '0.00',
  "AAABBUYE" decimal(5,2) NOT NULL default '0.00',
  "AAABBUYF" varchar(10) NOT NULL default ' ',
  "AAABBUYG" varchar(8) NOT NULL default '00000000',
  "AAABBUYH" varchar(12) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABBUYJ" varchar(6) NOT NULL default ' ',
  "AAABBUYK" varchar(1) NOT NULL default ' ',
  "AAABGZSJ" varchar(24) NOT NULL default ' ',
  "AAABBVHN" decimal(13,2) NOT NULL default '0.00',
  "AAABBVHO" decimal(13,2) NOT NULL default '0.00',
  "AAABBUYO" varchar(1) NOT NULL default ' ',
  "AAABBUYP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBUYT" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABBUZZ" varchar(1) NOT NULL default ' ',
  "AAABMQCO" varchar(3) NOT NULL default '000',
  "AAABBUZV" varchar(5) NOT NULL default '00000',
  "AAABMPVB" varchar(3) NOT NULL default '000',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCV" varchar(10) NOT NULL default ' ',
  "AAABMQCW" varchar(4) NOT NULL default '0000',
  "AAABMQCX" varchar(1) NOT NULL default ' ',
  "AAABMQBZ" varchar(4) NOT NULL default ' ',
  "AAABBUZY" varchar(1) NOT NULL default ' ',
  "AAABBUZJ" varchar(12) NOT NULL default ' ',
  "AAABBUZK" varchar(8) NOT NULL default '00000000',
  "AAABBUZL" varchar(4) NOT NULL default '0000',
  "AAABBUZM" varchar(3) NOT NULL default '000',
  "AAABBUZN" varchar(5) NOT NULL default '00000',
  "AAABBUZO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMREX","AAABMQCJ","AAABMQHY","AAABBUZZ","AAABMQCO","AAABBUZV"),
  KEY "AAABBUZW" ("AAABMQHU","AAABBUZY","AAABBUZZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVAA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABBVAE" varchar(4) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVAK" (
  "AAABGZWJ" varchar(30) NOT NULL default ' ',
  "AAABBVBF" varchar(10) NOT NULL default ' ',
  "AAABGZVS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGZWJ","AAABBVBF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVAQ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGZWJ" varchar(30) NOT NULL default ' ',
  "AAABBVBF" varchar(10) NOT NULL default ' ',
  "AAABGZVZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABGZWJ","AAABBVBF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVAY" (
  "AAABGZWJ" varchar(30) NOT NULL default ' ',
  "AAABBVBF" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABGZWH" varchar(1) NOT NULL default ' ',
  "AAABHLPB" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABGZWJ","AAABBVBF","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVBH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABDLQQ" varchar(1) NOT NULL default ' ',
  "AAABBVBM" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCWV","AAABDLQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVBR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVBZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMOHX" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABMCWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVCJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCWW" varchar(2) NOT NULL default ' ',
  "AAABBVCM" varchar(1) NOT NULL default ' ',
  "AAABBVCN" varchar(1) NOT NULL default ' ',
  "AAABBVCO" varchar(1) NOT NULL default ' ',
  "AAABBVCP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCWW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVCS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCWW" varchar(2) NOT NULL default ' ',
  "AAABILRJ" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCWW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVDA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBVEH" varchar(2) NOT NULL default ' ',
  "AAABBVDE" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBVEH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVDI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABIBGJ" varchar(4) NOT NULL default ' ',
  "AAABBVDM" varchar(1) NOT NULL default ' ',
  "AAABBVDN" varchar(10) NOT NULL default ' ',
  "AAABBVDO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQDO","AAABIBGJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVDS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABBVDV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQDO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVDY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBVEH" varchar(2) NOT NULL default ' ',
  "AAABBVED" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABBVEH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVEI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBVEU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBVEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVEN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABBVEU" varchar(4) NOT NULL default ' ',
  "AAABEMDV" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABBVEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVEV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJFIT" varchar(1) NOT NULL default ' ',
  "AAABBVEZ" varchar(1) NOT NULL default ' ',
  "AAABBVFA" varchar(1) NOT NULL default ' ',
  "AAABBVFB" varchar(10) NOT NULL default ' ',
  "AAABBVFC" varchar(1) NOT NULL default '0',
  "AAABBVFD" varchar(6) NOT NULL default ' ',
  "AAABBVFE" varchar(3) NOT NULL default ' ',
  "AAABBVFF" varchar(2) NOT NULL default ' ',
  "AAABBVFG" varchar(2) NOT NULL default ' ',
  "AAABBVFH" varchar(1) NOT NULL default ' ',
  "AAABELKD" varchar(4) NOT NULL default '0000',
  "AAABBXTB" varchar(3) NOT NULL default '000',
  "AAABJAUV" varchar(4) NOT NULL default '0000',
  "AAABBXTD" varchar(3) NOT NULL default '000',
  "AAABBVFM" varchar(1) NOT NULL default ' ',
  "AAABEIVY" varchar(2) NOT NULL default ' ',
  "AAABBVFO" varchar(1) NOT NULL default ' ',
  "AAABBVFP" varchar(2) NOT NULL default ' ',
  "AAABMCXD" varchar(15) NOT NULL default ' ',
  "AAABBVFR" varchar(2) NOT NULL default ' ',
  "AAABBVFS" varchar(4) NOT NULL default ' ',
  "AAABBVFT" decimal(5,2) NOT NULL default '0.00',
  "AAABBVFU" varchar(24) NOT NULL default ' ',
  "AAABBVFV" varchar(24) NOT NULL default ' ',
  "AAABBVFW" varchar(24) NOT NULL default ' ',
  "AAABBVFX" varchar(24) NOT NULL default ' ',
  "AAABBVFY" varchar(24) NOT NULL default ' ',
  "AAABBVFZ" decimal(5,2) NOT NULL default '0.00',
  "AAABBVGA" decimal(5,2) NOT NULL default '0.00',
  "AAABBVGB" varchar(10) NOT NULL default ' ',
  "AAABJZUZ" varchar(1) NOT NULL default ' ',
  "AAABBVGD" varchar(1) NOT NULL default ' ',
  "AAABBVGE" varchar(1) NOT NULL default ' ',
  "AAABBVGF" varchar(2) NOT NULL default '00',
  "AAABBVGG" varchar(1) NOT NULL default ' ',
  "AAABBVGH" varchar(20) NOT NULL default ' ',
  "AAABBVGI" varchar(2) NOT NULL default '00',
  "AAABBVGJ" varchar(2) NOT NULL default '00',
  "AAABBVGK" varchar(2) NOT NULL default '00',
  "AAABDISW" varchar(1) NOT NULL default ' ',
  "AAABBVGM" varchar(1) NOT NULL default ' ',
  "AAABBVGN" varchar(5) NOT NULL default ' ',
  "AAABBVGO" varchar(30) NOT NULL default ' ',
  "AAABBVGP" varchar(30) NOT NULL default ' ',
  "AAABBVGQ" varchar(4) NOT NULL default '0000',
  "AAABBVGR" varchar(3) NOT NULL default '000',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABBVGT" varchar(10) NOT NULL default ' ',
  "AAABBVGU" varchar(8) NOT NULL default '00000000',
  "AAABBVGV" varchar(12) NOT NULL default ' ',
  "AAABBVGW" varchar(1) NOT NULL default ' ',
  "AAABBVGX" varchar(1) NOT NULL default ' ',
  "AAABBVGY" varchar(1) NOT NULL default ' ',
  "AAABBVGZ" varchar(1) NOT NULL default ' ',
  "AAABBVHA" varchar(1) NOT NULL default ' ',
  "AAABBVHB" varchar(1) NOT NULL default ' ',
  "AAABBVHC" varchar(1) NOT NULL default ' ',
  "AAABBVHD" varchar(1) NOT NULL default ' ',
  "AAABBVHE" varchar(1) NOT NULL default ' ',
  "AAABBVHF" varchar(1) NOT NULL default ' ',
  "AAABBVHG" varchar(1) NOT NULL default ' ',
  "AAABBVHH" varchar(10) NOT NULL default ' ',
  "AAABBVHI" varchar(2) NOT NULL default ' ',
  "AAABBVHJ" varchar(1) NOT NULL default ' ',
  "AAABBVHK" varchar(2) NOT NULL default ' ',
  "AAABBVHL" varchar(1) NOT NULL default ' ',
  "AAABBVHM" varchar(1) NOT NULL default ' ',
  "AAABBVHN" decimal(13,2) NOT NULL default '0.00',
  "AAABBVHO" decimal(13,2) NOT NULL default '0.00',
  "AAABBVHP" varchar(4) NOT NULL default ' ',
  "AAABBVHQ" varchar(4) NOT NULL default ' ',
  "AAABBVHR" varchar(1) NOT NULL default ' ',
  "AAABBVHS" varchar(1) NOT NULL default ' ',
  "AAABBVHT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVHW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMPMH" varchar(4) NOT NULL default '0000',
  "AAABDOOL" varchar(7) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVIF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMPMH" varchar(4) NOT NULL default '0000',
  "AAABFIGO" varchar(7) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVIO" (
  "AAABBVIV" varchar(4) NOT NULL default ' ',
  "AAABCFHW" varchar(40) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLMLR" varchar(4) NOT NULL default ' ',
  "AAABLVJB" varchar(72) NOT NULL default ' ',
  "AAABBVIU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABBVIV","AAABCFHW","AAABMOHM","AAABLMLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVIZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABKJVG" varchar(10) NOT NULL default ' ',
  "AAABMQCE" varchar(10) NOT NULL default ' ',
  "AAABMCOP" varchar(10) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQCD","AAABKJVG","AAABMQCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVJQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABKJVG" varchar(10) NOT NULL default ' ',
  "AAABMQCE" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(10) NOT NULL default '0000000000',
  "AAABMCOP" varchar(10) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMCPR" varchar(1) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  "AAABMCPW" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMCXM" varchar(8) NOT NULL default '00000000',
  "AAABMCRA" varchar(6) NOT NULL default ' ',
  "AAABMCPZ" decimal(13,2) NOT NULL default '0.00',
  "AAABMCXW" decimal(13,2) NOT NULL default '0.00',
  "AAABMCXX" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQA" decimal(13,2) NOT NULL default '0.00',
  "AAABMCTE" decimal(13,2) NOT NULL default '0.00',
  "AAABMCYI" decimal(13,2) NOT NULL default '0.00',
  "AAABMCYJ" decimal(13,2) NOT NULL default '0.00',
  "AAABMCTF" decimal(13,2) NOT NULL default '0.00',
  "AAABMCTY" varchar(1) NOT NULL default ' ',
  "AAABBVKZ" decimal(13,2) NOT NULL default '0.00',
  "AAABBVLA" decimal(13,2) NOT NULL default '0.00',
  "AAABBVLB" decimal(13,2) NOT NULL default '0.00',
  "AAABBVLC" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQU" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQV" decimal(13,2) NOT NULL default '0.00',
  "AAABMCYK" decimal(13,2) NOT NULL default '0.00',
  "AAABBVLG" decimal(13,2) NOT NULL default '0.00',
  "AAABBVLH" decimal(13,2) NOT NULL default '0.00',
  "AAABBVLI" decimal(13,2) NOT NULL default '0.00',
  "AAABMCPY" varchar(2) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABBVLM" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQCD","AAABKJVG","AAABMQCE","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVLS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABBVLX" varchar(30) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABLTIZ" varchar(6) NOT NULL default ' ',
  "AAABBVMA" varchar(12) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABIHQW","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVMG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABHFFR" varchar(1) NOT NULL default ' ',
  "AAABBVMN" varchar(1) NOT NULL default ' ',
  "AAABBVMO" varchar(1) NOT NULL default ' ',
  "AAABBVMP" varchar(1) NOT NULL default ' ',
  "AAABHFNV" varchar(132) NOT NULL default ' ',
  "AAABHUGJ" varchar(40) NOT NULL default ' ',
  "AAABHUGK" varchar(30) NOT NULL default ' ',
  "AAABBVMT" varchar(20) NOT NULL default ' ',
  "AAABIJKF" varchar(1) NOT NULL default ' ',
  "AAABMNMP" varchar(4) NOT NULL default '0000',
  "AAABBVMW" varchar(1) NOT NULL default ' ',
  "AAABBVMX" varchar(1) NOT NULL default ' ',
  "AAABIJAF" varchar(1) NOT NULL default ' ',
  "AAABBVMZ" varchar(2) NOT NULL default '00',
  "AAABBVNA" int(11) NOT NULL default '0',
  "AAABBVNB" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(32) NOT NULL default ' ',
  "AAABFPKP" varchar(1) NOT NULL default ' ',
  "AAABBVNE" varchar(4) NOT NULL default '0000',
  "AAABIIVF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABIHQW","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVNL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABBVOW" varchar(6) NOT NULL default '000000',
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  "AAABLBEI" varchar(6) NOT NULL default '000000',
  "AAABJTEQ" varchar(6) NOT NULL default '000000',
  "AAABMLMY" varchar(6) NOT NULL default '000000',
  "AAABMNMM" varchar(6) NOT NULL default '000000',
  "AAABLDNU" varchar(4) NOT NULL default ' ',
  "AAABLUWW" varchar(1) NOT NULL default ' ',
  "AAABBVNZ" varchar(20) NOT NULL default ' ',
  "AAABHVRR" varchar(5) NOT NULL default ' ',
  "AAABLAUS" varchar(55) NOT NULL default ' ',
  "AAABKGYO" varchar(10) NOT NULL default ' ',
  "AAABKGYP" varchar(20) NOT NULL default ' ',
  "AAABKGYQ" varchar(40) NOT NULL default ' ',
  "AAABEGEB" varchar(1) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABLTXS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABIHQW","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVON" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABBVPE" varchar(30) NOT NULL default ' ',
  "AAABHDJO" varchar(1) NOT NULL default ' ',
  "AAABBVOW" varchar(6) NOT NULL default '000000',
  "AAABJTEQ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABIHQW","AAABMREL","AAABMREF","AAABIHLK","AAABBVPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVPF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABBVPL" varchar(30) NOT NULL default ' ',
  "AAABDVXU" varchar(30) NOT NULL default ' ',
  "AAABHFNV" varchar(132) NOT NULL default ' ',
  "AAABHUGJ" varchar(40) NOT NULL default ' ',
  "AAABHUGK" varchar(30) NOT NULL default ' ',
  "AAABBVPQ" varchar(1) NOT NULL default ' ',
  "AAABGYQQ" varchar(4) NOT NULL default ' ',
  "AAABBVPS" varchar(1) NOT NULL default ' ',
  "AAABBVPT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABIHQW","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVPZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEPJZ" varchar(1) NOT NULL default '0',
  "AAABBVQP" varchar(1) NOT NULL default ' ',
  "AAABJZCI" varchar(2) NOT NULL default ' ',
  "AAABBVQR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABIHQW","AAABMREL","AAABMREF","AAABEPJZ","AAABBVQP","AAABJZCI","AAABBVQR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVQS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABBVRO" varchar(4) NOT NULL default '0000',
  "AAABMMJT" varchar(32) NOT NULL default ' ',
  "AAABFPKP" varchar(1) NOT NULL default ' ',
  "AAABKGYO" varchar(10) NOT NULL default ' ',
  "AAABKGYP" varchar(20) NOT NULL default ' ',
  "AAABKGYQ" varchar(40) NOT NULL default ' ',
  "AAABBVRE" varchar(1) NOT NULL default ' ',
  "AAABBVRF" varchar(1) NOT NULL default ' ',
  "AAABHUCM" varchar(1) NOT NULL default ' ',
  "AAABBVRH" varchar(1) NOT NULL default ' ',
  "AAABHUCO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABIHQW","AAABMREL","AAABMREF","AAABBVRO","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVRQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABBVSP" varchar(1) NOT NULL default ' ',
  "AAABBVSQ" varchar(10) NOT NULL default ' ',
  "AAABBVSR" varchar(1) NOT NULL default ' ',
  "AAABFNYO" varchar(10) NOT NULL default ' ',
  "AAABBVRZ" varchar(1) NOT NULL default ' ',
  "AAABBVSA" varchar(1) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLQRK" varchar(30) NOT NULL default ' ',
  "AAABLQRL" varchar(30) NOT NULL default ' ',
  "AAABHFNV" varchar(132) NOT NULL default ' ',
  "AAABHUGJ" varchar(40) NOT NULL default ' ',
  "AAABHUGK" varchar(30) NOT NULL default ' ',
  "AAABBVSH" varchar(30) NOT NULL default ' ',
  "AAABHUGL" varchar(30) NOT NULL default ' ',
  "AAABBVSJ" varchar(1) NOT NULL default ' ',
  "AAABMNMP" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABIHQW","AAABMREL","AAABBVSP","AAABBVSQ","AAABBVSR","AAABFNYO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVST" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABBVSY" varchar(4) NOT NULL default ' ',
  "AAABBVSZ" varchar(4) NOT NULL default ' ',
  "AAABBVTA" varchar(4) NOT NULL default ' ',
  "AAABBVTB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABIHQW","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVTG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUMK" varchar(8) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(3) NOT NULL default '000',
  "AAABFKGP" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  "AAABBVTO" varchar(1) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABEJFS" varchar(4) NOT NULL default ' ',
  "AAABBVTR" varchar(1) NOT NULL default ' ',
  "AAABBVTS" varchar(1) NOT NULL default ' ',
  "AAABBVTT" varchar(1) NOT NULL default ' ',
  "AAABBVTU" varchar(1) NOT NULL default ' ',
  "AAABBVTV" varchar(1) NOT NULL default ' ',
  "AAABBVTW" varchar(1) NOT NULL default ' ',
  "AAABJDMI" varchar(1) NOT NULL default ' ',
  "AAABBVTY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUMK","AAABIHQW","AAABMREL","AAABIHLK","AAABFKGP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVUF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABBVVA" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(32) NOT NULL default ' ',
  "AAABFRJP" varchar(1) NOT NULL default ' ',
  "AAABBVUN" varchar(4) NOT NULL default ' ',
  "AAABBVUO" varchar(4) NOT NULL default ' ',
  "AAABBVUP" varchar(4) NOT NULL default ' ',
  "AAABJTEQ" varchar(6) NOT NULL default '000000',
  "AAABMLMY" varchar(6) NOT NULL default '000000',
  "AAABLUWW" varchar(1) NOT NULL default ' ',
  "AAABLDNU" varchar(4) NOT NULL default ' ',
  "AAABBVUU" varchar(32) NOT NULL default ' ',
  "AAABBVUV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABIHQW","AAABMREL","AAABBVVA","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVVC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABBXUO" varchar(10) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCXS" varchar(3) NOT NULL default ' ',
  "AAABBXUR" varchar(4) NOT NULL default '0000',
  "AAABBXUS" varchar(8) NOT NULL default '00000000',
  "AAABIYVQ" decimal(15,3) NOT NULL default '0.000',
  "AAABBXQU" varchar(8) NOT NULL default '00000000',
  "AAABLUTZ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXUN","AAABBXUO","AAABMCWV","AAABMCXS","AAABBXUR","AAABBXUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVVW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLPSK" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLPSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVWD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLPSK" varchar(2) NOT NULL default ' ',
  "AAABBVWI" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMREX","AAABLPSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVWN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABBXUO" varchar(10) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCXS" varchar(3) NOT NULL default ' ',
  "AAABBXUR" varchar(4) NOT NULL default '0000',
  "AAABMPVB" varchar(3) NOT NULL default '000',
  "AAABBVWW" decimal(8,5) NOT NULL default '0.00000',
  "AAABBXIM" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXUN","AAABBXUO","AAABMCWV","AAABMCXS","AAABBXUR","AAABMPVB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVXG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCDD" varchar(4) NOT NULL default ' ',
  "AAABBVXJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCDD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVXM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXMT" varchar(10) NOT NULL default ' ',
  "AAABBXHM" varchar(2) NOT NULL default ' ',
  "AAABBXCZ" varchar(1) NOT NULL default ' ',
  "AAABLEAO" varchar(3) NOT NULL default ' ',
  "AAABBXDB" varchar(30) NOT NULL default ' ',
  "AAABBVXU" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXMT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVXY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBXOO" varchar(3) NOT NULL default '000',
  "AAABBXFR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBXOO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVYE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBXOO" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBXOO","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVYN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXMT" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXMT","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVYX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABBXUO" varchar(10) NOT NULL default ' ',
  "AAABBXAJ" varchar(6) NOT NULL default ' ',
  "AAABBXAK" varchar(3) NOT NULL default ' ',
  "AAABBXUR" varchar(4) NOT NULL default '0000',
  "AAABMPVB" varchar(3) NOT NULL default '000',
  "AAABJRKN" varchar(10) NOT NULL default ' ',
  "AAABMQHQ" varchar(3) NOT NULL default ' ',
  "AAABBVZI" decimal(15,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXUN","AAABBXUO","AAABBXAJ","AAABBXAK","AAABBXUR","AAABMPVB","AAABJRKN","AAABMQHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBVZT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBWAI" varchar(30) NOT NULL default ' ',
  "AAABBWAJ" varchar(30) NOT NULL default ' ',
  "AAABBVZY" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBWAI","AAABBWAJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWAE" (
  "AAABBWAI" varchar(30) NOT NULL default ' ',
  "AAABBWAJ" varchar(30) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABBWAI","AAABBWAJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWAK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBXEK" varchar(3) NOT NULL default ' ',
  "AAABBXEL" varchar(3) NOT NULL default ' ',
  "AAABBXEI" decimal(14,8) NOT NULL default '0.00000000',
  PRIMARY KEY ("AAABMRIL","AAABBXEK","AAABBXEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWAS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQCO" varchar(3) NOT NULL default '000',
  "AAABBXUR" varchar(4) NOT NULL default '0000',
  "AAABBXAC" varchar(10) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABMPVB" varchar(3) NOT NULL default '000',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABBWQY" varchar(10) NOT NULL default ' ',
  "AAABBWQZ" varchar(10) NOT NULL default ' ',
  "AAABBWRA" varchar(6) NOT NULL default ' ',
  "AAABBWRB" varchar(3) NOT NULL default ' ',
  "AAABBWBH" varchar(3) NOT NULL default '000',
  "AAABBWRC" varchar(10) NOT NULL default ' ',
  "AAABBWQA" varchar(10) NOT NULL default ' ',
  "AAABBWQB" varchar(10) NOT NULL default ' ',
  "AAABBWQC" varchar(6) NOT NULL default ' ',
  "AAABBWQD" varchar(3) NOT NULL default ' ',
  "AAABBWBN" varchar(3) NOT NULL default '000',
  "AAABBWQE" varchar(10) NOT NULL default ' ',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMPVO" decimal(17,2) NOT NULL default '0.00',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQCJ","AAABMQCO","AAABBXUR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWBZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABBXUO" varchar(10) NOT NULL default ' ',
  "AAABBXFA" varchar(3) NOT NULL default '000',
  "AAABBXKE" varchar(4) NOT NULL default '0000',
  "AAABMJLV" varchar(2) NOT NULL default '00',
  "AAABBXOO" varchar(3) NOT NULL default '000',
  "AAABBXMT" varchar(10) NOT NULL default ' ',
  "AAABBWCJ" varchar(3) NOT NULL default ' ',
  "AAABBWCK" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXUN","AAABBXUO","AAABBXFA","AAABBXKE","AAABMJLV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWCS" (
  "AAABBXFR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABBXFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWCV" (
  "AAABBXFR" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABBXFR","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWDC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBWDF" varchar(2) NOT NULL default ' ',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWDK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABBWEJ" varchar(3) NOT NULL default ' ',
  "AAABJRKN" varchar(10) NOT NULL default ' ',
  "AAABJFJV" decimal(9,6) NOT NULL default '0.000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXUN","AAABBWEJ","AAABJRKN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWDW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABBWEJ" varchar(3) NOT NULL default ' ',
  "AAABBWEK" varchar(4) NOT NULL default '0000',
  "AAABBWEL" varchar(3) NOT NULL default '000',
  "AAABLSDL" varchar(1) NOT NULL default ' ',
  "AAABJRKN" varchar(10) NOT NULL default ' ',
  "AAABJFJV" decimal(9,6) NOT NULL default '0.000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXUN","AAABBWEJ","AAABBWEK","AAABBWEL","AAABLSDL","AAABJRKN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWEO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABEHHL" varchar(4) NOT NULL default '0000',
  "AAABMPVB" varchar(3) NOT NULL default '000',
  "AAABLUVE" varchar(3) NOT NULL default '000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABLQCR" varchar(12) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKWJY","AAABEHHL","AAABMPVB","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWFG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABEHHL" varchar(4) NOT NULL default '0000',
  "AAABMPVB" varchar(3) NOT NULL default '000',
  "AAABLUVE" varchar(3) NOT NULL default '000',
  "AAABMJWN" varchar(8) NOT NULL default ' ',
  "AAABBWFO" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKWJY","AAABEHHL","AAABMPVB","AAABLUVE","AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWFW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABEHHL" varchar(4) NOT NULL default '0000',
  "AAABMPVB" varchar(3) NOT NULL default '000',
  "AAABLUVE" varchar(3) NOT NULL default '000',
  "AAABJYWC" varchar(2) NOT NULL default ' ',
  "AAABBWGQ" varchar(3) NOT NULL default '000',
  "AAABMNMZ" varchar(25) NOT NULL default ' ',
  "AAABMNNA" varchar(25) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIFAF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKWJY","AAABEHHL","AAABMPVB","AAABLUVE","AAABJYWC","AAABBWGQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWGR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXJT" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWGY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXJT" varchar(5) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMREX","AAABBXJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWHJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABBXUR" varchar(4) NOT NULL default '0000',
  "AAABLUTZ" varchar(2) NOT NULL default '00',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABBXUO" varchar(10) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCXS" varchar(3) NOT NULL default ' ',
  "AAABBXHN" varchar(2) NOT NULL default ' ',
  "AAABBXHK" decimal(17,5) NOT NULL default '0.00000',
  "AAABBXTP" decimal(12,7) NOT NULL default '0.0000000',
  "AAABBXHM" varchar(2) NOT NULL default ' ',
  "AAABBXHO" decimal(7,4) NOT NULL default '0.0000',
  "AAABBXHP" decimal(9,4) NOT NULL default '0.0000',
  "AAABBXIM" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMQHW","AAABMQHX","AAABBXUR","AAABLUTZ","AAABBXUN","AAABBXUO","AAABMCWV","AAABMCXS","AAABBXHN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWIM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWIU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBWML" varchar(20) NOT NULL default ' ',
  "AAABBWPG" varchar(10) NOT NULL default ' ',
  "AAABELKD" varchar(4) NOT NULL default '0000',
  "AAABBXTB" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABBWML","AAABBWPG","AAABELKD","AAABBXTB"),
  UNIQUE KEY "AAABBWJF" ("AAABMRIL","AAABBWPG","AAABELKD","AAABBXTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWJK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBWML" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBWML")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWJP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBWML" varchar(20) NOT NULL default ' ',
  "AAABBWJT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBWML")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWJX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHBRF" varchar(4) NOT NULL default '0000',
  "AAABBXUO" varchar(10) NOT NULL default ' ',
  "AAABBXUR" varchar(4) NOT NULL default '0000',
  "AAABBXUS" varchar(8) NOT NULL default '00000000',
  "AAABBWML" varchar(20) NOT NULL default ' ',
  "AAABBXVF" varchar(4) NOT NULL default ' ',
  "AAABBXSV" varchar(3) NOT NULL default ' ',
  "AAABBXSH" varchar(10) NOT NULL default ' ',
  "AAABBXRS" decimal(7,4) NOT NULL default '0.0000',
  "AAABIYVQ" decimal(15,3) NOT NULL default '0.000',
  "AAABLUTZ" varchar(2) NOT NULL default '00',
  "AAABBWMV" varchar(2) NOT NULL default ' ',
  "AAABMKFG" varchar(10) NOT NULL default ' ',
  "AAABLZCP" varchar(40) NOT NULL default ' ',
  "AAABBWLW" varchar(10) NOT NULL default ' ',
  "AAABBWLX" varchar(1) NOT NULL default ' ',
  "AAABBXTP" decimal(12,7) NOT NULL default '0.0000000',
  "AAABJGVW" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHBRF","AAABBXUO","AAABBXUR","AAABBXUS","AAABBWML","AAABBXVF","AAABBXSV","AAABBXSH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWLA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABBXUO" varchar(10) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCXS" varchar(3) NOT NULL default ' ',
  "AAABHBRF" varchar(4) NOT NULL default '0000',
  "AAABBXUR" varchar(4) NOT NULL default '0000',
  "AAABBXUS" varchar(8) NOT NULL default '00000000',
  "AAABBXSD" varchar(2) NOT NULL default ' ',
  "AAABJRKN" varchar(10) NOT NULL default ' ',
  "AAABBWML" varchar(20) NOT NULL default ' ',
  "AAABBXVF" varchar(4) NOT NULL default ' ',
  "AAABBXSV" varchar(3) NOT NULL default ' ',
  "AAABBXSH" varchar(10) NOT NULL default ' ',
  "AAABBXRS" decimal(7,4) NOT NULL default '0.0000',
  "AAABIYVQ" decimal(15,3) NOT NULL default '0.000',
  "AAABLUTZ" varchar(2) NOT NULL default '00',
  "AAABBXHN" varchar(2) NOT NULL default ' ',
  "AAABBWMV" varchar(2) NOT NULL default ' ',
  "AAABMKFG" varchar(10) NOT NULL default ' ',
  "AAABBWLW" varchar(10) NOT NULL default ' ',
  "AAABBWLX" varchar(1) NOT NULL default ' ',
  "AAABBXTP" decimal(12,7) NOT NULL default '0.0000000',
  "AAABJGVW" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXUN","AAABBXUO","AAABMCWV","AAABMCXS","AAABHBRF","AAABBXUR","AAABBXUS","AAABBXSD","AAABJRKN","AAABBWML","AAABBXVF","AAABBXSV","AAABBXSH"),
  KEY "AAABBWMP" ("AAABMRIL","AAABHBRF","AAABMREX","AAABBXUN","AAABBXUS","AAABBWMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWMW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBXSV" varchar(3) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBXSV","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWNF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBXSV" varchar(3) NOT NULL default ' ',
  "AAABBXSL" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABBXSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWNL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABELKD" varchar(4) NOT NULL default '0000',
  "AAABBXTB" varchar(3) NOT NULL default '000',
  "AAABJAUV" varchar(4) NOT NULL default '0000',
  "AAABBXTD" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWNU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABBXUO" varchar(10) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCXS" varchar(3) NOT NULL default ' ',
  "AAABBXKE" varchar(4) NOT NULL default '0000',
  "AAABMJLV" varchar(2) NOT NULL default '00',
  "AAABBXJT" varchar(5) NOT NULL default ' ',
  "AAABBXJU" varchar(1) NOT NULL default ' ',
  "AAABBXJV" varchar(1) NOT NULL default ' ',
  "AAABBXJW" varchar(10) NOT NULL default ' ',
  "AAABBXJX" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXUN","AAABBXUO","AAABMCWV","AAABMCXS","AAABBXKE","AAABMJLV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWOQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABBXUO" varchar(10) NOT NULL default ' ',
  "AAABBWPE" varchar(4) NOT NULL default '0000',
  "AAABLUTZ" varchar(2) NOT NULL default '00',
  "AAABBWPG" varchar(10) NOT NULL default ' ',
  "AAABBWOY" decimal(8,5) NOT NULL default '0.00000',
  "AAABBWOZ" decimal(8,5) NOT NULL default '0.00000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXUN","AAABBXUO","AAABBWPE","AAABLUTZ","AAABBWPG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQCO" varchar(3) NOT NULL default '000',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMQHQ" varchar(3) NOT NULL default ' ',
  "AAABMPVB" varchar(3) NOT NULL default '000',
  "AAABBWQY" varchar(10) NOT NULL default ' ',
  "AAABBWQZ" varchar(10) NOT NULL default ' ',
  "AAABBWRA" varchar(6) NOT NULL default ' ',
  "AAABBWRB" varchar(3) NOT NULL default ' ',
  "AAABBWRC" varchar(10) NOT NULL default ' ',
  "AAABBXOO" varchar(3) NOT NULL default '000',
  "AAABBWRD" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABBWQA" varchar(10) NOT NULL default ' ',
  "AAABBWQB" varchar(10) NOT NULL default ' ',
  "AAABBWQC" varchar(6) NOT NULL default ' ',
  "AAABBWQD" varchar(3) NOT NULL default ' ',
  "AAABBWQE" varchar(10) NOT NULL default ' ',
  "AAABMPVO" decimal(17,2) NOT NULL default '0.00',
  "AAABMPVR" decimal(15,3) NOT NULL default '0.000',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHW","AAABMQHX","AAABMQDO","AAABMQHY","AAABMQCJ","AAABMQCO"),
  KEY "AAABBWQT" ("AAABMRIL","AAABMQDO","AAABMQHY","AAABMPVB","AAABBWQY","AAABBWQZ","AAABBWRA","AAABBWRB","AAABBWRC","AAABBWRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWRE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABBXUO" varchar(10) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCXS" varchar(3) NOT NULL default ' ',
  "AAABBXUR" varchar(4) NOT NULL default '0000',
  "AAABBXUS" varchar(8) NOT NULL default '00000000',
  "AAABBXTP" decimal(12,7) NOT NULL default '0.0000000',
  "AAABLUTZ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXUN","AAABBXUO","AAABMCWV","AAABMCXS","AAABBXUR","AAABBXUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWRX" (
  "AAABBXCS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABBXCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWSA" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBXCS" varchar(10) NOT NULL default ' ',
  "AAABILRJ" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABBXCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWSG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBXUO" varchar(10) NOT NULL default ' ',
  "AAABLEAO" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBXUO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWSM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBXUO" varchar(10) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBXUO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWSV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABBXUO" varchar(10) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCXS" varchar(3) NOT NULL default ' ',
  "AAABBXUR" varchar(4) NOT NULL default '0000',
  "AAABBXUS" varchar(8) NOT NULL default '00000000',
  "AAABIYVQ" decimal(15,3) NOT NULL default '0.000',
  "AAABBXUJ" decimal(15,3) NOT NULL default '0.000',
  "AAABLUTZ" varchar(2) NOT NULL default '00',
  "AAABBXHP" decimal(9,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXUN","AAABBXUO","AAABMCWV","AAABMCXS","AAABBXUR","AAABBXUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWTQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWTX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  "AAABBXLX" varchar(3) NOT NULL default '000',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLUVS","AAABBXLX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWUH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLUVS","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWUR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABCVDR" varchar(8) NOT NULL default '00000000',
  "AAABBXIM" varchar(5) NOT NULL default ' ',
  "AAABBXIN" varchar(2) NOT NULL default ' ',
  "AAABBXIO" varchar(1) NOT NULL default ' ',
  "AAABBXIP" varchar(1) NOT NULL default ' ',
  "AAABBWVB" varchar(8) NOT NULL default '00000000',
  "AAABBWVC" varchar(8) NOT NULL default '00000000',
  "AAABBWVD" varchar(8) NOT NULL default '00000000',
  "AAABBWVE" varchar(1) NOT NULL default ' ',
  "AAABJOTX" varchar(8) NOT NULL default '00000000',
  "AAABGXSI" varchar(6) NOT NULL default '000000',
  "AAABBXIS" varchar(12) NOT NULL default ' ',
  "AAABJQCK" varchar(8) NOT NULL default '00000000',
  "AAABJQCL" varchar(6) NOT NULL default '000000',
  "AAABBXIV" varchar(12) NOT NULL default ' ',
  "AAABBWVL" varchar(10) NOT NULL default ' ',
  "AAABMJCM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXUN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWVQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXUN","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWWB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABBXUO" varchar(10) NOT NULL default ' ',
  "AAABBWWG" varchar(1) NOT NULL default ' ',
  "AAABBWWH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXUN","AAABBXUO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWWM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBXVF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBXVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWWR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBXVF" varchar(4) NOT NULL default ' ',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBXVF","AAABMNJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWXA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXMT" varchar(10) NOT NULL default ' ',
  "AAABBXMU" varchar(3) NOT NULL default '000',
  "AAABBXMN" decimal(17,5) NOT NULL default '0.00000',
  "AAABBXMO" decimal(8,5) NOT NULL default '0.00000',
  "AAABBXMP" decimal(8,5) NOT NULL default '0.00000',
  "AAABBXMQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXMT","AAABBXMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWXN" (
  "AAABLUZL" varchar(2) NOT NULL default ' ',
  "AAABBXMX" varchar(30) NOT NULL default ' ',
  "AAABBXMY" varchar(30) NOT NULL default ' ',
  "AAABBXMZ" varchar(30) NOT NULL default ' ',
  "AAABBXNA" varchar(30) NOT NULL default ' ',
  "AAABBXNB" varchar(30) NOT NULL default ' ',
  "AAABBXNC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWXW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLUZL" varchar(2) NOT NULL default ' ',
  "AAABBWYA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWYD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWYM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABBXUO" varchar(10) NOT NULL default ' ',
  "AAABBXOO" varchar(3) NOT NULL default '000',
  "AAABBXUR" varchar(4) NOT NULL default '0000',
  "AAABBXPS" varchar(2) NOT NULL default '00',
  "AAABBXPM" varchar(11) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXUN","AAABBXUO","AAABBXOO","AAABBXUR","AAABBXPS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWZC" (
  "AAABLUZL" varchar(2) NOT NULL default ' ',
  "AAABBXPE" varchar(2) NOT NULL default ' ',
  "AAABBXPC" varchar(2) NOT NULL default ' ',
  "AAABBWZG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUZL","AAABBXPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWZJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABBXUO" varchar(10) NOT NULL default ' ',
  "AAABBXUR" varchar(4) NOT NULL default '0000',
  "AAABBXPS" varchar(2) NOT NULL default '00',
  "AAABBXPM" varchar(11) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXUN","AAABBXUO","AAABBXUR","AAABBXPS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBWZX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQCO" varchar(3) NOT NULL default '000',
  "AAABBXUR" varchar(4) NOT NULL default '0000',
  "AAABBXAC" varchar(10) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMPVB" varchar(3) NOT NULL default '000',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABBXUO" varchar(10) NOT NULL default ' ',
  "AAABBXAJ" varchar(6) NOT NULL default ' ',
  "AAABBXAK" varchar(3) NOT NULL default ' ',
  "AAABJRKN" varchar(10) NOT NULL default ' ',
  "AAABLPSK" varchar(2) NOT NULL default ' ',
  "AAABMQHQ" varchar(3) NOT NULL default ' ',
  "AAABBXAO" decimal(15,3) NOT NULL default '0.000',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQCJ","AAABMQCO","AAABBXUR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXAX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHBRF" varchar(4) NOT NULL default '0000',
  "AAABLEAO" varchar(3) NOT NULL default ' ',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHBRF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXBF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHBRF" varchar(4) NOT NULL default '0000',
  "AAABBXUO" varchar(10) NOT NULL default ' ',
  "AAABBXUR" varchar(4) NOT NULL default '0000',
  "AAABBXUS" varchar(8) NOT NULL default '00000000',
  "AAABBXBL" decimal(8,5) NOT NULL default '0.00000',
  "AAABBXIM" varchar(5) NOT NULL default ' ',
  "AAABMPVB" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABHBRF","AAABBXUO","AAABBXUR","AAABBXUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXBT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABBXUO" varchar(10) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCXS" varchar(3) NOT NULL default ' ',
  "AAABHBRF" varchar(4) NOT NULL default '0000',
  "AAABBXCB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXUN","AAABBXUO","AAABMCWV","AAABMCXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXCI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHBRF" varchar(4) NOT NULL default '0000',
  "AAABBXCM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHBRF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXCQ" (
  "AAABHEIG" varchar(16) NOT NULL default ' ',
  "AAABBXCS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXCU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXMT" varchar(10) NOT NULL default ' ',
  "AAABBXHM" varchar(2) NOT NULL default ' ',
  "AAABBXCZ" varchar(1) NOT NULL default ' ',
  "AAABLEAO" varchar(3) NOT NULL default ' ',
  "AAABBXDB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXMT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBXOO" varchar(3) NOT NULL default '000',
  "AAABBXFR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBXOO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXDL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBXOO" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBXOO","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXDU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXMT" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXMT","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXEE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBXEK" varchar(3) NOT NULL default ' ',
  "AAABBXEL" varchar(3) NOT NULL default ' ',
  "AAABBXEI" decimal(14,8) NOT NULL default '0.00000000',
  PRIMARY KEY ("AAABMRIL","AAABBXEK","AAABBXEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXEM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABBXUO" varchar(10) NOT NULL default ' ',
  "AAABBXFA" varchar(3) NOT NULL default '000',
  "AAABBXKE" varchar(4) NOT NULL default '0000',
  "AAABMJLV" varchar(2) NOT NULL default '00',
  "AAABBXOO" varchar(3) NOT NULL default '000',
  "AAABBXMT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXUN","AAABBXUO","AAABBXFA","AAABBXKE","AAABMJLV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXFD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBXOO" varchar(3) NOT NULL default '000',
  "AAABBXFR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBXOO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXFJ" (
  "AAABBXFR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABBXFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXFM" (
  "AAABBXFR" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABBXFR","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXFT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABBXUO" varchar(10) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABBXGH" varchar(2) NOT NULL default '00',
  "AAABBXGI" varchar(3) NOT NULL default '000',
  "AAABBXGB" varchar(34) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXUN","AAABBXUO","AAABMQHX","AAABBXGH","AAABBXGI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXGJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXJT" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXGQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXJT" varchar(5) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMREX","AAABBXJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXHB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXUR" varchar(4) NOT NULL default '0000',
  "AAABLUTZ" varchar(2) NOT NULL default '00',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABBXUO" varchar(10) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCXS" varchar(3) NOT NULL default ' ',
  "AAABBXHK" decimal(17,5) NOT NULL default '0.00000',
  "AAABBXTP" decimal(12,7) NOT NULL default '0.0000000',
  "AAABBXHM" varchar(2) NOT NULL default ' ',
  "AAABBXHN" varchar(2) NOT NULL default ' ',
  "AAABBXHO" decimal(7,4) NOT NULL default '0.0000',
  "AAABBXHP" decimal(9,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXUR","AAABLUTZ","AAABBXUN","AAABBXUO","AAABMCWV","AAABMCXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXHY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXIG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABCVDR" varchar(8) NOT NULL default '00000000',
  "AAABBXIM" varchar(5) NOT NULL default ' ',
  "AAABBXIN" varchar(2) NOT NULL default ' ',
  "AAABBXIO" varchar(1) NOT NULL default ' ',
  "AAABBXIP" varchar(1) NOT NULL default ' ',
  "AAABJOTX" varchar(8) NOT NULL default '00000000',
  "AAABGXSI" varchar(6) NOT NULL default '000000',
  "AAABBXIS" varchar(12) NOT NULL default ' ',
  "AAABJQCK" varchar(8) NOT NULL default '00000000',
  "AAABJQCL" varchar(6) NOT NULL default '000000',
  "AAABBXIV" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXUN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXIZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXUN","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXJK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABBXUO" varchar(10) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCXS" varchar(3) NOT NULL default ' ',
  "AAABBXKE" varchar(4) NOT NULL default '0000',
  "AAABMJLV" varchar(2) NOT NULL default '00',
  "AAABBXJT" varchar(5) NOT NULL default ' ',
  "AAABBXJU" varchar(1) NOT NULL default ' ',
  "AAABBXJV" varchar(1) NOT NULL default ' ',
  "AAABBXJW" varchar(10) NOT NULL default ' ',
  "AAABBXJX" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXUN","AAABBXUO","AAABMCWV","AAABMCXS","AAABBXKE","AAABMJLV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABBXUO" varchar(10) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABBXKM" varchar(34) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXUN","AAABBXUO","AAABMQHX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXKS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBXUO" varchar(10) NOT NULL default ' ',
  "AAABLEAO" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBXUO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXKY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBXUO" varchar(10) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBXUO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXLH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXLO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  "AAABBXLX" varchar(3) NOT NULL default '000',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLUVS","AAABBXLX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXLY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLUVS","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXMI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXMT" varchar(10) NOT NULL default ' ',
  "AAABBXMU" varchar(3) NOT NULL default '000',
  "AAABBXMN" decimal(17,3) NOT NULL default '0.000',
  "AAABBXMO" decimal(8,5) NOT NULL default '0.00000',
  "AAABBXMP" decimal(8,5) NOT NULL default '0.00000',
  "AAABBXMQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXMT","AAABBXMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXMV" (
  "AAABLUZL" varchar(2) NOT NULL default ' ',
  "AAABBXMX" varchar(30) NOT NULL default ' ',
  "AAABBXMY" varchar(30) NOT NULL default ' ',
  "AAABBXMZ" varchar(30) NOT NULL default ' ',
  "AAABBXNA" varchar(30) NOT NULL default ' ',
  "AAABBXNB" varchar(30) NOT NULL default ' ',
  "AAABBXNC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXNE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABBXPM" varchar(11) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXUN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXNM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLUZL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXNS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABBXUO" varchar(10) NOT NULL default ' ',
  "AAABBXOO" varchar(3) NOT NULL default '000',
  "AAABBXUR" varchar(4) NOT NULL default '0000',
  "AAABBXPS" varchar(2) NOT NULL default '00',
  "AAABBXPM" varchar(11) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXUN","AAABBXUO","AAABBXOO","AAABBXUR","AAABBXPS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXOR" (
  "AAABBXOY" varchar(2) NOT NULL default ' ',
  "AAABBXOT" varchar(30) NOT NULL default ' ',
  "AAABBXOU" varchar(30) NOT NULL default ' ',
  "AAABBXOV" varchar(30) NOT NULL default ' ',
  "AAABBXOW" varchar(30) NOT NULL default ' ',
  "AAABBXOX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABBXOY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXOZ" (
  "AAABLUZL" varchar(2) NOT NULL default ' ',
  "AAABBXPE" varchar(2) NOT NULL default ' ',
  "AAABBXPC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUZL","AAABBXPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXPF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABBXUO" varchar(10) NOT NULL default ' ',
  "AAABBXUR" varchar(4) NOT NULL default '0000',
  "AAABBXPS" varchar(2) NOT NULL default '00',
  "AAABBXPM" varchar(11) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXUN","AAABBXUO","AAABBXUR","AAABBXPS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXPT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCXS" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXUN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXQC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXQK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABBXUO" varchar(10) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCXS" varchar(3) NOT NULL default ' ',
  "AAABBXUR" varchar(4) NOT NULL default '0000',
  "AAABBXUS" varchar(8) NOT NULL default '00000000',
  "AAABIYVQ" decimal(15,3) NOT NULL default '0.000',
  "AAABBXQU" varchar(8) NOT NULL default '00000000',
  "AAABLUTZ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXUN","AAABBXUO","AAABMCWV","AAABMCXS","AAABBXUR","AAABBXUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXRE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABBXUO" varchar(10) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCXS" varchar(3) NOT NULL default ' ',
  "AAABBXUR" varchar(4) NOT NULL default '0000',
  "AAABBXUS" varchar(8) NOT NULL default '00000000',
  "AAABBXSD" varchar(2) NOT NULL default ' ',
  "AAABJRKN" varchar(10) NOT NULL default ' ',
  "AAABBXVF" varchar(4) NOT NULL default ' ',
  "AAABBXSV" varchar(3) NOT NULL default ' ',
  "AAABBXSH" varchar(10) NOT NULL default ' ',
  "AAABBXRS" decimal(7,4) NOT NULL default '0.0000',
  "AAABIYVQ" decimal(15,3) NOT NULL default '0.000',
  "AAABLUTZ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXUN","AAABBXUO","AAABMCWV","AAABMCXS","AAABBXUR","AAABBXUS","AAABBXSD","AAABJRKN","AAABBXVF","AAABBXSV","AAABBXSH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXSI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBXSV" varchar(3) NOT NULL default ' ',
  "AAABBXSL" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABBXSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXSO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBXSV" varchar(3) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBXSV","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXSX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABELKD" varchar(4) NOT NULL default '0000',
  "AAABBXTB" varchar(3) NOT NULL default '000',
  "AAABJAUV" varchar(4) NOT NULL default '0000',
  "AAABBXTD" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXTG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABBXUO" varchar(10) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCXS" varchar(3) NOT NULL default ' ',
  "AAABBXUR" varchar(4) NOT NULL default '0000',
  "AAABBXUS" varchar(8) NOT NULL default '00000000',
  "AAABBXTP" decimal(12,7) NOT NULL default '0.0000000',
  "AAABLUTZ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXUN","AAABBXUO","AAABMCWV","AAABMCXS","AAABBXUR","AAABBXUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXTZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABBXUN" varchar(10) NOT NULL default ' ',
  "AAABBXUO" varchar(10) NOT NULL default ' ',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCXS" varchar(3) NOT NULL default ' ',
  "AAABBXUR" varchar(4) NOT NULL default '0000',
  "AAABBXUS" varchar(8) NOT NULL default '00000000',
  "AAABIYVQ" decimal(15,3) NOT NULL default '0.000',
  "AAABBXUJ" decimal(15,3) NOT NULL default '0.000',
  "AAABLUTZ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABBXUN","AAABBXUO","AAABMCWV","AAABMCXS","AAABBXUR","AAABBXUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXUT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBXVF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBXVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXUY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBXVF" varchar(4) NOT NULL default ' ',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBXVF","AAABMNJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXVH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBXWD" varchar(15) NOT NULL default ' ',
  "AAABCOUC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBXWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXVN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABBXWD" varchar(15) NOT NULL default ' ',
  "AAABEPBY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABBXWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXVV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBXWD" varchar(15) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABBXWA" varchar(10) NOT NULL default ' ',
  "AAABBXWB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBXWD","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXWF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBXXD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBXXD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXWK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBXXD" varchar(4) NOT NULL default ' ',
  "AAABBXWU" varchar(2) NOT NULL default '00',
  "AAABBXWV" varchar(2) NOT NULL default ' ',
  "AAABBXWP" varchar(2) NOT NULL default ' ',
  "AAABBXWQ" varchar(3) NOT NULL default ' ',
  "AAABBXWR" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBXXD","AAABBXWU","AAABBXWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXWW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABBXXD" varchar(4) NOT NULL default ' ',
  "AAABEPBY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABBXXD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXXE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAWY" varchar(30) NOT NULL default ' ',
  "AAABBZRM" varchar(30) NOT NULL default ' ',
  "AAABBXXI" varchar(12) NOT NULL default ' ',
  "AAABBXXJ" varchar(12) NOT NULL default ' ',
  "AAABBXXK" varchar(12) NOT NULL default ' ',
  "AAABBXXL" varchar(12) NOT NULL default ' ',
  "AAABBXXM" varchar(2) NOT NULL default '00',
  "AAABBZBV" smallint(6) NOT NULL default '0',
  "AAABBZBW" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABCAWY","AAABBZRM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXXS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCALH" varchar(2) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABCAWY" varchar(30) NOT NULL default ' ',
  "AAABBZRM" varchar(30) NOT NULL default ' ',
  "AAABBZRN" varchar(1) NOT NULL default ' ',
  "AAABLEUZ" varchar(30) NOT NULL default ' ',
  "AAABBXYA" varchar(1) NOT NULL default ' ',
  "AAABBXYB" varchar(1) NOT NULL default ' ',
  "AAABBXYC" varchar(1) NOT NULL default ' ',
  "AAABBXYD" varchar(1) NOT NULL default ' ',
  "AAABBXYE" varchar(1) NOT NULL default ' ',
  "AAABBXYF" varchar(1) NOT NULL default ' ',
  "AAABBXYG" varchar(1) NOT NULL default ' ',
  "AAABBXYH" varchar(1) NOT NULL default ' ',
  "AAABBXYI" varchar(1) NOT NULL default ' ',
  "AAABBXYJ" varchar(1) NOT NULL default ' ',
  "AAABBXYK" varchar(1) NOT NULL default ' ',
  "AAABBXYL" varchar(1) NOT NULL default ' ',
  "AAABBXYM" varchar(1) NOT NULL default ' ',
  "AAABBXYN" varchar(1) NOT NULL default ' ',
  "AAABBXYO" varchar(1) NOT NULL default ' ',
  "AAABBXYP" varchar(1) NOT NULL default ' ',
  "AAABBXYQ" varchar(1) NOT NULL default ' ',
  "AAABBXYR" varchar(1) NOT NULL default ' ',
  "AAABBXYS" varchar(1) NOT NULL default ' ',
  "AAABBXYT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCALH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXYW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCALH" varchar(2) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCALH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXZE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCALH" varchar(2) NOT NULL default ' ',
  "AAABCAFP" varchar(3) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABCAWY" varchar(30) NOT NULL default ' ',
  "AAABBZRM" varchar(30) NOT NULL default ' ',
  "AAABBZRN" varchar(1) NOT NULL default ' ',
  "AAABLEUZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCALH","AAABCAFP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBXZQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCALH" varchar(2) NOT NULL default ' ',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABLKIV" varchar(3) NOT NULL default ' ',
  "AAABGZSJ" varchar(6) NOT NULL default ' ',
  "AAABBZVT" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCALH","AAABCATV","AAABLKIV","AAABGZSJ","AAABBZVT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYAD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCALH" varchar(2) NOT NULL default ' ',
  "AAABLRXL" varchar(1) NOT NULL default ' ',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABCAFP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCALH","AAABLRXL","AAABCATV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBYEZ" varchar(6) NOT NULL default ' ',
  "AAABLVHQ" varchar(60) NOT NULL default ' ',
  "AAABLVHI" varchar(40) NOT NULL default ' ',
  "AAABLVHJ" varchar(40) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLVCU" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBYEZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYAY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCALH" varchar(2) NOT NULL default ' ',
  "AAABCALI" varchar(1) NOT NULL default ' ',
  "AAABLRXL" varchar(1) NOT NULL default ' ',
  "AAABCAKN" varchar(1) NOT NULL default ' ',
  "AAABCAFP" varchar(3) NOT NULL default ' ',
  "AAABBYBF" varchar(1) NOT NULL default ' ',
  "AAABBYBG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCALH","AAABCALI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYBK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCALH" varchar(2) NOT NULL default ' ',
  "AAABCALI" varchar(1) NOT NULL default ' ',
  "AAABBYBP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCALH","AAABCALI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYBU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAML" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAML")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYBZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAML" varchar(8) NOT NULL default ' ',
  "AAABBYCD" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAML")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYCH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCALH" varchar(2) NOT NULL default ' ',
  "AAABBYCK" varchar(30) NOT NULL default ' ',
  "AAABBYCL" varchar(30) NOT NULL default ' ',
  "AAABBYCM" varchar(30) NOT NULL default ' ',
  "AAABBYCN" varchar(20) NOT NULL default ' ',
  "AAABBYCO" varchar(20) NOT NULL default ' ',
  "AAABCAOU" varchar(20) NOT NULL default ' ',
  "AAABBYCQ" varchar(3) NOT NULL default '000',
  "AAABBYCR" varchar(4) NOT NULL default ' ',
  "AAABBYCS" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCALH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYCV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABCALH" varchar(2) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default ' ',
  "AAABLBGN" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABCALH","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBYDM" varchar(30) NOT NULL default ' ',
  "AAABLVJB" varchar(30) NOT NULL default ' ',
  "AAABCAXA" varchar(3) NOT NULL default ' ',
  "AAABBYDK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBYDM","AAABLVJB","AAABCAXA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYDP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCALH" varchar(2) NOT NULL default ' ',
  "AAABBYYG" varchar(1) NOT NULL default ' ',
  "AAABLKIV" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKGMO" decimal(10,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABBZHR" varchar(1) NOT NULL default ' ',
  "AAABBYDZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCALH","AAABBYYG","AAABLKIV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYEF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCALH" varchar(2) NOT NULL default ' ',
  "AAABBYYG" varchar(1) NOT NULL default ' ',
  "AAABLKIV" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCALH","AAABBYYG","AAABLKIV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYET" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBYEZ" varchar(6) NOT NULL default ' ',
  "AAABBYFA" varchar(6) NOT NULL default ' ',
  "AAABBYEX" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBYEZ","AAABBYFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYFB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  "AAABCAVN" varchar(1) NOT NULL default ' ',
  "AAABCALH" varchar(2) NOT NULL default ' ',
  "AAABBYGD" varchar(1) NOT NULL default ' ',
  "AAABLPSK" varchar(2) NOT NULL default ' ',
  "AAABBYFI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABCAVN","AAABCALH","AAABBYGD","AAABLPSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYFP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  "AAABCAVN" varchar(1) NOT NULL default ' ',
  "AAABCALH" varchar(2) NOT NULL default ' ',
  "AAABBYGD" varchar(1) NOT NULL default ' ',
  "AAABLPSK" varchar(2) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUVS","AAABCAVN","AAABCALH","AAABBYGD","AAABLPSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYGF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJXTS" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJXTS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJXTS" varchar(8) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJXTS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYGS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJXTS" varchar(8) NOT NULL default ' ',
  "AAABJZCI" varchar(8) NOT NULL default ' ',
  "AAABBYGW" varchar(8) NOT NULL default ' ',
  "AAABBYGX" varchar(8) NOT NULL default ' ',
  "AAABBYGY" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJXTS","AAABJZCI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYHC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJZCI" varchar(8) NOT NULL default ' ',
  "AAABBYHF" varchar(1) NOT NULL default ' ',
  "AAABBYHG" varchar(1) NOT NULL default ' ',
  "AAABBYHH" varchar(14) NOT NULL default ' ',
  "AAABBYHI" varchar(12) NOT NULL default ' ',
  "AAABBYHJ" varchar(14) NOT NULL default ' ',
  "AAABGLWX" varchar(40) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABBYHM" varchar(20) NOT NULL default ' ',
  "AAABLHAH" varchar(255) NOT NULL default ' ',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJZCI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYHR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJZCI" varchar(8) NOT NULL default ' ',
  "AAABMLPV" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJZCI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAXA" varchar(3) NOT NULL default ' ',
  "AAABCAXC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAXA","AAABCAXC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYIG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAXA" varchar(3) NOT NULL default ' ',
  "AAABCAXC" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAXA","AAABCAXC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYIQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCALH" varchar(2) NOT NULL default ' ',
  "AAABMKIQ" varchar(2) NOT NULL default '00',
  "AAABJXTS" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCALH","AAABMKIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYIY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCALH" varchar(2) NOT NULL default ' ',
  "AAABMKIQ" varchar(2) NOT NULL default '00',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCALH","AAABMKIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYJI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABBYYG" varchar(1) NOT NULL default ' ',
  "AAABLKIV" varchar(3) NOT NULL default ' ',
  "AAABBZVB" varchar(2) NOT NULL default ' ',
  "AAABBZVT" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBYTO" varchar(20) NOT NULL default ' ',
  "AAABBZNW" varchar(1) NOT NULL default ' ',
  "AAABFYUL" decimal(10,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCATV","AAABBYYG","AAABLKIV","AAABBZVB","AAABBZVT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYKC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABBYYG" varchar(1) NOT NULL default ' ',
  "AAABLKIV" varchar(3) NOT NULL default ' ',
  "AAABBZVB" varchar(2) NOT NULL default ' ',
  "AAABBZVT" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABFITZ" varchar(20) NOT NULL default ' ',
  "AAABBYRI" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABCATV","AAABBYYG","AAABLKIV","AAABBZVB","AAABBZVT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYKV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABBYYG" varchar(1) NOT NULL default ' ',
  "AAABLKIV" varchar(3) NOT NULL default ' ',
  "AAABBYSE" varchar(30) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBYTO" varchar(20) NOT NULL default ' ',
  "AAABBZVT" varchar(12) NOT NULL default ' ',
  "AAABBZNW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCATV","AAABBYYG","AAABLKIV","AAABBYSE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYLM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABBYYG" varchar(1) NOT NULL default ' ',
  "AAABLKIV" varchar(3) NOT NULL default ' ',
  "AAABBYSE" varchar(30) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABLPSU" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCATV","AAABBYYG","AAABLKIV","AAABBYSE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYMC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABCALR" varchar(1) NOT NULL default ' ',
  "AAABCATX" varchar(3) NOT NULL default ' ',
  "AAABCALU" varchar(1) NOT NULL default ' ',
  "AAABCATZ" varchar(3) NOT NULL default ' ',
  "AAABCALX" varchar(1) NOT NULL default ' ',
  "AAABBYPR" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBYOO" varchar(1) NOT NULL default ' ',
  "AAABBYOP" varchar(2) NOT NULL default ' ',
  "AAABCAML" varchar(8) NOT NULL default ' ',
  "AAABBYOR" varchar(1) NOT NULL default ' ',
  "AAABBZNW" varchar(1) NOT NULL default ' ',
  "AAABCALZ" varchar(1) NOT NULL default ' ',
  "AAABLOZE" varchar(2) NOT NULL default ' ',
  "AAABBZNX" varchar(4) NOT NULL default '0000',
  "AAABBYMV" decimal(10,2) NOT NULL default '0.00',
  "AAABFYUL" decimal(10,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABBZOE" varchar(1) NOT NULL default ' ',
  "AAABBZOF" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABCATV","AAABCALR","AAABCATX","AAABCALU","AAABCATZ","AAABCALX","AAABBYPR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYNJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABCALR" varchar(1) NOT NULL default ' ',
  "AAABCATX" varchar(3) NOT NULL default ' ',
  "AAABCALU" varchar(1) NOT NULL default ' ',
  "AAABCATZ" varchar(3) NOT NULL default ' ',
  "AAABCALX" varchar(1) NOT NULL default ' ',
  "AAABBYPR" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABFITZ" varchar(20) NOT NULL default ' ',
  "AAABBYRI" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCATV","AAABCALR","AAABCATX","AAABCALU","AAABCATZ","AAABCALX","AAABBYPR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYOG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABBYXF" varchar(3) NOT NULL default ' ',
  "AAABBYXH" varchar(3) NOT NULL default ' ',
  "AAABBYPR" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBYOO" varchar(1) NOT NULL default ' ',
  "AAABBYOP" varchar(2) NOT NULL default ' ',
  "AAABCAML" varchar(8) NOT NULL default ' ',
  "AAABBYOR" varchar(1) NOT NULL default ' ',
  "AAABBZNW" varchar(1) NOT NULL default ' ',
  "AAABCALZ" varchar(1) NOT NULL default ' ',
  "AAABFYUL" decimal(10,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCATV","AAABBYXF","AAABBYXH","AAABBYPR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYPC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABBYXF" varchar(3) NOT NULL default ' ',
  "AAABBYXH" varchar(3) NOT NULL default ' ',
  "AAABBYPR" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABFITZ" varchar(20) NOT NULL default ' ',
  "AAABBYRI" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCATV","AAABBYXF","AAABBYXH","AAABBYPR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYPT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABBYYG" varchar(1) NOT NULL default ' ',
  "AAABLKIV" varchar(3) NOT NULL default ' ',
  "AAABGZSJ" varchar(6) NOT NULL default ' ',
  "AAABBYYJ" varchar(1) NOT NULL default ' ',
  "AAABBYZF" varchar(1) NOT NULL default ' ',
  "AAABBZWN" varchar(1) NOT NULL default ' ',
  "AAABBZVT" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBYTO" varchar(20) NOT NULL default ' ',
  "AAABBZNW" varchar(1) NOT NULL default ' ',
  "AAABFYUL" decimal(10,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABBYTS" varchar(1) NOT NULL default ' ',
  "AAABBYTU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCATV","AAABBYYG","AAABLKIV","AAABGZSJ","AAABBYYJ","AAABBYZF","AAABBZWN","AAABBZVT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYQV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABBYYG" varchar(1) NOT NULL default ' ',
  "AAABLKIV" varchar(3) NOT NULL default ' ',
  "AAABGZSJ" varchar(6) NOT NULL default ' ',
  "AAABBYYJ" varchar(1) NOT NULL default ' ',
  "AAABBYZF" varchar(1) NOT NULL default ' ',
  "AAABBZWN" varchar(1) NOT NULL default ' ',
  "AAABBZVT" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABFITZ" varchar(20) NOT NULL default ' ',
  "AAABBYRI" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABCATV","AAABBYYG","AAABLKIV","AAABGZSJ","AAABBYYJ","AAABBYZF","AAABBZWN","AAABBZVT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYRU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBYYG" varchar(1) NOT NULL default ' ',
  "AAABLKIV" varchar(3) NOT NULL default ' ',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABGZSJ" varchar(6) NOT NULL default ' ',
  "AAABBZVT" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBYTO" varchar(20) NOT NULL default ' ',
  "AAABBYSE" varchar(30) NOT NULL default ' ',
  "AAABBZNW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBYYG","AAABLKIV","AAABCATV","AAABGZSJ","AAABBZVT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYSN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABBYYG" varchar(1) NOT NULL default ' ',
  "AAABLKIV" varchar(3) NOT NULL default ' ',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABGZSJ" varchar(6) NOT NULL default ' ',
  "AAABBZVT" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABLPSU" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABBYYG","AAABLKIV","AAABCATV","AAABGZSJ","AAABBZVT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYTF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCABB" varchar(8) NOT NULL default ' ',
  "AAABBYZF" varchar(1) NOT NULL default ' ',
  "AAABBYUD" varchar(1) NOT NULL default ' ',
  "AAABBZWN" varchar(1) NOT NULL default ' ',
  "AAABBZVT" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBYTO" varchar(20) NOT NULL default ' ',
  "AAABBZNW" varchar(1) NOT NULL default ' ',
  "AAABFYUL" decimal(10,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABBYTS" varchar(1) NOT NULL default ' ',
  "AAABBYTT" varchar(1) NOT NULL default ' ',
  "AAABBYTU" varchar(40) NOT NULL default ' ',
  "AAABBYTV" varchar(1) NOT NULL default ' ',
  "AAABBYTW" varchar(1) NOT NULL default ' ',
  "AAABBYTX" varchar(1) NOT NULL default ' ',
  "AAABBYTY" varchar(1) NOT NULL default ' ',
  "AAABBYTZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCABB","AAABBYZF","AAABBYUD","AAABBZWN","AAABBZVT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYUH" (
  "AAABBYUV" varchar(6) NOT NULL default '000000',
  "AAABBZOF" varchar(6) NOT NULL default '000000',
  "AAABBYWJ" varchar(1) NOT NULL default ' ',
  "AAABBYWG" varchar(6) NOT NULL default ' ',
  "AAABBYUM" varchar(3) NOT NULL default '000',
  "AAABBYUN" varchar(8) NOT NULL default '00000000',
  "AAABBYUO" varchar(8) NOT NULL default '00000000',
  "AAABBYUP" varchar(8) NOT NULL default '00000000',
  "AAABBYUQ" varchar(15) NOT NULL default ' ',
  "AAABLTLM" varchar(6) NOT NULL default '000000',
  "AAABLTLN" varchar(6) NOT NULL default '000000',
  "AAABBYUT" varchar(1) NOT NULL default ' ',
  "AAABBYUU" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABBYUV","AAABBZOF","AAABBYWJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYUY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABBYVP" varchar(1) NOT NULL default ' ',
  "AAABLKIV" varchar(3) NOT NULL default ' ',
  "AAABBZVB" varchar(2) NOT NULL default ' ',
  "AAABBZVT" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBYYV" varchar(20) NOT NULL default ' ',
  "AAABBZNW" varchar(1) NOT NULL default ' ',
  "AAABBZNZ" varchar(1) NOT NULL default ' ',
  "AAABBZOA" smallint(6) NOT NULL default '0',
  "AAABBZOB" decimal(10,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCATV","AAABBYVP","AAABLKIV","AAABBZVB","AAABBZVT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYVU" (
  "AAABBYWH" varchar(6) NOT NULL default '000000',
  "AAABBZOF" varchar(6) NOT NULL default '000000',
  "AAABBYWJ" varchar(1) NOT NULL default ' ',
  "AAABBYVY" varchar(1) NOT NULL default ' ',
  "AAABBYVZ" varchar(3) NOT NULL default ' ',
  "AAABBYWA" varchar(1) NOT NULL default ' ',
  "AAABBYWB" varchar(3) NOT NULL default ' ',
  "AAABBYWC" varchar(2) NOT NULL default ' ',
  "AAABBYWD" varchar(15) NOT NULL default ' ',
  "AAABBYWE" varchar(6) NOT NULL default '000000',
  "AAABBYWF" varchar(6) NOT NULL default '000000',
  "AAABBYWG" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABBYWH","AAABBZOF","AAABBYWJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYWK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABBYXF" varchar(3) NOT NULL default ' ',
  "AAABBYXG" varchar(1) NOT NULL default ' ',
  "AAABBYXH" varchar(3) NOT NULL default ' ',
  "AAABBYXI" varchar(1) NOT NULL default ' ',
  "AAABCALZ" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBZNV" varchar(1) NOT NULL default ' ',
  "AAABDROT" varchar(1) NOT NULL default ' ',
  "AAABBZNW" varchar(1) NOT NULL default ' ',
  "AAABBZNY" varchar(2) NOT NULL default ' ',
  "AAABBZNZ" varchar(1) NOT NULL default ' ',
  "AAABBZOA" smallint(6) NOT NULL default '0',
  "AAABBZOB" decimal(10,2) NOT NULL default '0.00',
  "AAABBZOC" varchar(1) NOT NULL default ' ',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCATV","AAABBYXF","AAABBYXG","AAABBYXH","AAABBYXI","AAABCALZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYXL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABBZVT" varchar(3) NOT NULL default ' ',
  "AAABBYYG" varchar(1) NOT NULL default ' ',
  "AAABLKIV" varchar(3) NOT NULL default ' ',
  "AAABGZSJ" varchar(6) NOT NULL default ' ',
  "AAABBYYJ" varchar(1) NOT NULL default ' ',
  "AAABBZWN" varchar(1) NOT NULL default ' ',
  "AAABBYZF" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBYYV" varchar(20) NOT NULL default ' ',
  "AAABBZNW" varchar(1) NOT NULL default ' ',
  "AAABBZNZ" varchar(1) NOT NULL default ' ',
  "AAABBZOA" smallint(6) NOT NULL default '0',
  "AAABBZOB" decimal(10,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCATV","AAABBZVT","AAABBYYG","AAABLKIV","AAABGZSJ","AAABBYYJ","AAABBZWN","AAABBYZF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYYN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCABB" varchar(8) NOT NULL default ' ',
  "AAABBZVT" varchar(3) NOT NULL default ' ',
  "AAABBZWN" varchar(1) NOT NULL default ' ',
  "AAABBYZF" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBYYV" varchar(20) NOT NULL default ' ',
  "AAABBZNW" varchar(1) NOT NULL default ' ',
  "AAABBZNZ" varchar(1) NOT NULL default ' ',
  "AAABBZOA" smallint(6) NOT NULL default '0',
  "AAABBZOB" decimal(10,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCABB","AAABBZVT","AAABBZWN","AAABBYZF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYZH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAXB" varchar(12) NOT NULL default ' ',
  "AAABBYZK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYZN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAXB" varchar(12) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBYZV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAXA" varchar(3) NOT NULL default ' ',
  "AAABCAXB" varchar(12) NOT NULL default ' ',
  "AAABLYTE" varchar(3) NOT NULL default ' ',
  "AAABIHOA" varchar(1) NOT NULL default ' ',
  "AAABBZAB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAXA","AAABCAXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAXD" varchar(2) NOT NULL default ' ',
  "AAABBZAP" varchar(1) NOT NULL default ' ',
  "AAABCAXA" varchar(3) NOT NULL default ' ',
  "AAABBZAK" varchar(12) NOT NULL default ' ',
  "AAABBZRM" varchar(30) NOT NULL default ' ',
  "AAABCAUI" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAXD","AAABBZAP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZAQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAXD" varchar(2) NOT NULL default ' ',
  "AAABCAXA" varchar(3) NOT NULL default ' ',
  "AAABCAXC" varchar(2) NOT NULL default ' ',
  "AAABCAXB" varchar(12) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAXD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZAZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAXD" varchar(2) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAXD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZBH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAXA" varchar(3) NOT NULL default ' ',
  "AAABCAXB" varchar(12) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAXA","AAABCAXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZBR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAXB" varchar(12) NOT NULL default ' ',
  "AAABLUFA" varchar(1) NOT NULL default ' ',
  "AAABBZBV" smallint(6) NOT NULL default '0',
  "AAABBZBW" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABCAXB","AAABLUFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZCA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZCF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZCN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  "AAABLRXL" varchar(1) NOT NULL default ' ',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABCARR" varchar(1) NOT NULL default ' ',
  "AAABCARS" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABLRXL","AAABCATV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZCY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  "AAABCAEE" varchar(1) NOT NULL default ' ',
  "AAABKXSJ" varchar(3) NOT NULL default ' ',
  "AAABCABB" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABCAAK" varchar(8) NOT NULL default ' ',
  "AAABMNRG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABCAEE","AAABKXSJ","AAABCABB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZDO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRXL" varchar(1) NOT NULL default ' ',
  "AAABCATU" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRXL","AAABCATU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZDV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLRXL" varchar(1) NOT NULL default ' ',
  "AAABCATU" varchar(8) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLRXL","AAABCATU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZEF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATU" varchar(8) NOT NULL default ' ',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABCATY" varchar(1) NOT NULL default ' ',
  "AAABCATZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCATU","AAABCATV","AAABCATY","AAABCATZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATU" varchar(8) NOT NULL default ' ',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABCATW" varchar(1) NOT NULL default ' ',
  "AAABCATX" varchar(3) NOT NULL default ' ',
  "AAABCATY" varchar(1) NOT NULL default ' ',
  "AAABCATZ" varchar(3) NOT NULL default ' ',
  "AAABCASR" varchar(1) NOT NULL default ' ',
  "AAABCASS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCATU","AAABCATV","AAABCATW","AAABCATX","AAABCATY","AAABCATZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZFH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATU" varchar(8) NOT NULL default ' ',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABCATY" varchar(1) NOT NULL default ' ',
  "AAABCATZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCATU","AAABCATV","AAABCATY","AAABCATZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZFS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  "AAABLRXL" varchar(1) NOT NULL default ' ',
  "AAABCATU" varchar(8) NOT NULL default ' ',
  "AAABCAEE" varchar(1) NOT NULL default ' ',
  "AAABBZIY" varchar(8) NOT NULL default ' ',
  "AAABMNRG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABLRXL","AAABCATU","AAABCAEE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZGF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNRG" varchar(1) NOT NULL default ' ',
  "AAABLTQJ" varchar(1) NOT NULL default '0',
  "AAABGCIV" varchar(1) NOT NULL default ' ',
  "AAABFPZD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNRG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZGN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMNRG" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMNRG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZGV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  "AAABCAEE" varchar(1) NOT NULL default ' ',
  "AAABLRXL" varchar(1) NOT NULL default ' ',
  "AAABCATU" varchar(8) NOT NULL default ' ',
  "AAABBZIY" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMNRG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABCAEE","AAABLRXL","AAABCATU","AAABBZIY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZHM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCALH" varchar(2) NOT NULL default ' ',
  "AAABBZIG" varchar(1) NOT NULL default ' ',
  "AAABBZIH" varchar(1) NOT NULL default ' ',
  "AAABBZHR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCALH","AAABBZIG","AAABBZIH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZHW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCALH" varchar(2) NOT NULL default ' ',
  "AAABBZIG" varchar(1) NOT NULL default ' ',
  "AAABBZIH" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCALH","AAABBZIG","AAABBZIH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZII" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRXL" varchar(1) NOT NULL default ' ',
  "AAABBZIY" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRXL","AAABBZIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZIP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLRXL" varchar(1) NOT NULL default ' ',
  "AAABBZIY" varchar(8) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLRXL","AAABBZIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZIZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBZJI" varchar(8) NOT NULL default ' ',
  "AAABBZKF" varchar(3) NOT NULL default '000',
  "AAABBZVB" varchar(2) NOT NULL default ' ',
  "AAABBZKA" decimal(10,2) NOT NULL default '0.00',
  "AAABBZKB" decimal(10,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBZJI","AAABBZKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZJK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBZJS" varchar(8) NOT NULL default ' ',
  "AAABBZJT" varchar(6) NOT NULL default '000000',
  "AAABBZJO" varchar(3) NOT NULL default ' ',
  "AAABBZJP" varchar(20) NOT NULL default ' ',
  "AAABBZJQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBZJS","AAABBZJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZJU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAAK" varchar(8) NOT NULL default ' ',
  "AAABBZKF" varchar(3) NOT NULL default '000',
  "AAABLRXL" varchar(1) NOT NULL default ' ',
  "AAABBZWM" varchar(2) NOT NULL default ' ',
  "AAABBZKA" decimal(10,2) NOT NULL default '0.00',
  "AAABBZKB" decimal(10,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAAK","AAABBZKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBZKM" varchar(8) NOT NULL default ' ',
  "AAABBZKN" varchar(6) NOT NULL default '000000',
  "AAABLOZE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBZKM","AAABBZKN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABCALR" varchar(1) NOT NULL default ' ',
  "AAABCATX" varchar(3) NOT NULL default ' ',
  "AAABCALU" varchar(1) NOT NULL default ' ',
  "AAABCATZ" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLJJO" varchar(1) NOT NULL default ' ',
  "AAABCALZ" varchar(1) NOT NULL default ' ',
  "AAABGYNY" varchar(1) NOT NULL default ' ',
  "AAABBZLB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABCATV","AAABCALR","AAABCATX","AAABCALU","AAABCATZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZLK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAXA" varchar(3) NOT NULL default ' ',
  "AAABCAXB" varchar(12) NOT NULL default ' ',
  "AAABCAXC" varchar(2) NOT NULL default ' ',
  "AAABBZMT" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABCAXA","AAABCAXB","AAABCAXC","AAABBZMT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZLV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAXA" varchar(3) NOT NULL default ' ',
  "AAABCAXB" varchar(12) NOT NULL default ' ',
  "AAABCAXC" varchar(2) NOT NULL default ' ',
  "AAABLUFA" varchar(1) NOT NULL default ' ',
  "AAABBZMT" varchar(5) NOT NULL default '00000',
  "AAABBZMU" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABCAXA","AAABCAXB","AAABCAXC","AAABLUFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZMI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAXA" varchar(3) NOT NULL default ' ',
  "AAABCAXB" varchar(12) NOT NULL default ' ',
  "AAABCAXC" varchar(2) NOT NULL default ' ',
  "AAABBZMT" varchar(5) NOT NULL default '00000',
  "AAABBZMU" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABCAXA","AAABCAXB","AAABCAXC","AAABBZMT","AAABBZMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZMV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAVN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAVN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZNA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAVN" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAVN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZNI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABCALR" varchar(1) NOT NULL default ' ',
  "AAABCATX" varchar(3) NOT NULL default ' ',
  "AAABCALU" varchar(1) NOT NULL default ' ',
  "AAABCATZ" varchar(3) NOT NULL default ' ',
  "AAABCALX" varchar(1) NOT NULL default ' ',
  "AAABBZON" varchar(1) NOT NULL default ' ',
  "AAABBZOO" varchar(2) NOT NULL default ' ',
  "AAABBZOP" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBZNV" varchar(1) NOT NULL default ' ',
  "AAABBZNW" varchar(1) NOT NULL default ' ',
  "AAABBZNX" varchar(4) NOT NULL default '0000',
  "AAABBZNY" varchar(2) NOT NULL default ' ',
  "AAABBZNZ" varchar(1) NOT NULL default ' ',
  "AAABBZOA" smallint(6) NOT NULL default '0',
  "AAABBZOB" decimal(10,2) NOT NULL default '0.00',
  "AAABBZOC" varchar(1) NOT NULL default ' ',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABBZOE" varchar(1) NOT NULL default ' ',
  "AAABBZOF" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABCATV","AAABCALR","AAABCATX","AAABCALU","AAABCATZ","AAABCALX","AAABBZON","AAABBZOO","AAABBZOP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZOR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  "AAABMRCU" varchar(2) NOT NULL default ' ',
  "AAABBZOV" varchar(2) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLRXL" varchar(20) NOT NULL default ' ',
  "AAABBZOY" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZPC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAWP" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABCAWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAWP" varchar(4) NOT NULL default '0000',
  "AAABCAWQ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABCAWP","AAABCAWQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZPO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAWP" varchar(4) NOT NULL default '0000',
  "AAABCAWQ" varchar(2) NOT NULL default '00',
  "AAABCAWR" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABCAWP","AAABCAWQ","AAABCAWR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZPX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAWP" varchar(4) NOT NULL default '0000',
  "AAABCAWQ" varchar(2) NOT NULL default '00',
  "AAABCAWR" varchar(2) NOT NULL default '00',
  "AAABBZQL" varchar(2) NOT NULL default '00',
  "AAABCALR" varchar(1) NOT NULL default ' ',
  "AAABCATX" varchar(3) NOT NULL default ' ',
  "AAABCALU" varchar(1) NOT NULL default ' ',
  "AAABCATZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAWP","AAABCAWQ","AAABCAWR","AAABBZQL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZQM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAWP" varchar(4) NOT NULL default '0000',
  "AAABCAWQ" varchar(2) NOT NULL default '00',
  "AAABCAWR" varchar(2) NOT NULL default '00',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAWP","AAABCAWQ","AAABCAWR","AAABCATV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZQX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAWP" varchar(4) NOT NULL default '0000',
  "AAABCAWQ" varchar(2) NOT NULL default '00',
  "AAABCAWR" varchar(2) NOT NULL default '00',
  "AAABCAMD" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAWP","AAABCAWQ","AAABCAWR","AAABCAMD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZRI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAXA" varchar(3) NOT NULL default ' ',
  "AAABCAWY" varchar(30) NOT NULL default ' ',
  "AAABBZRM" varchar(30) NOT NULL default ' ',
  "AAABBZRN" varchar(1) NOT NULL default ' ',
  "AAABLEUZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAXA","AAABCAWY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZRS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAXA" varchar(3) NOT NULL default ' ',
  "AAABCAXB" varchar(12) NOT NULL default ' ',
  "AAABCAXC" varchar(2) NOT NULL default ' ',
  "AAABCAWY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAXA","AAABCAXB","AAABCAXC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZSC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEEW" varchar(12) NOT NULL default ' ',
  "AAABMMJT" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLEEW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZSI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBZSR" varchar(2) NOT NULL default ' ',
  "AAABCALH" varchar(2) NOT NULL default ' ',
  "AAABBZSM" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(2) NOT NULL default ' ',
  "AAABMLPV" varchar(70) NOT NULL default ' ',
  "AAABMPMH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBZSR","AAABCALH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZST" (
  "AAABMLPV" varchar(12) NOT NULL default ' ',
  "AAABCAYW" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZSX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCALH" varchar(2) NOT NULL default ' ',
  "AAABBZTS" varchar(8) NOT NULL default ' ',
  "AAABLHAH" varchar(255) NOT NULL default ' ',
  "AAABBZTC" varchar(1) NOT NULL default ' ',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABBZTE" varchar(1) NOT NULL default ' ',
  "AAABBZTF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCALH","AAABBZTS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZTJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCALH" varchar(2) NOT NULL default ' ',
  "AAABBZTS" varchar(8) NOT NULL default ' ',
  "AAABBZTO" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCALH","AAABBZTS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZTT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBZVB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBZVB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZTY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABBZVB" varchar(2) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABBZVB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZUG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBZVT" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBZVT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZUL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABBZVT" varchar(3) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABBZVT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZUT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBZVB" varchar(2) NOT NULL default ' ',
  "AAABBZVC" varchar(1) NOT NULL default ' ',
  "AAABBZVD" varchar(1) NOT NULL default ' ',
  "AAABBZVE" varchar(1) NOT NULL default ' ',
  "AAABBZVF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBZVB","AAABBZVC","AAABBZVD","AAABBZVE","AAABBZVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZVG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBZVT" varchar(3) NOT NULL default ' ',
  "AAABDLQQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBZVT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZVM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABBZVT" varchar(3) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABBZVT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBZWM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBZWM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZVZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABBZWM" varchar(2) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABBZWM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZWH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBZWM" varchar(2) NOT NULL default ' ',
  "AAABBZWN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBZWM","AAABBZWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZWO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKXSJ" varchar(3) NOT NULL default ' ',
  "AAABBZXN" varchar(30) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABIYJJ" varchar(3) NOT NULL default ' ',
  "AAABCABI" varchar(2) NOT NULL default ' ',
  "AAABBZXR" varchar(3) NOT NULL default ' ',
  "AAABCALK" varchar(5) NOT NULL default ' ',
  "AAABBZWX" varchar(3) NOT NULL default ' ',
  "AAABBZWY" varchar(2) NOT NULL default ' ',
  "AAABBZWZ" varchar(1) NOT NULL default ' ',
  "AAABBZXA" varchar(1) NOT NULL default ' ',
  "AAABBZXB" varchar(1) NOT NULL default ' ',
  "AAABBZXC" varchar(1) NOT NULL default ' ',
  "AAABCALJ" varchar(1) NOT NULL default ' ',
  "AAABJSTM" varchar(7) NOT NULL default '0000000',
  "AAABBZXF" varchar(1) NOT NULL default ' ',
  "AAABJSTN" varchar(6) NOT NULL default '000000',
  "AAABBZXH" varchar(1) NOT NULL default ' ',
  "AAABFSWV" varchar(3) NOT NULL default '000',
  "AAABBZXJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKXSJ"),
  KEY "AAABBZXM" ("AAABBZXN"),
  KEY "AAABBZXO" ("AAABLEHK"),
  KEY "AAABBZXQ" ("AAABBZXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZXS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKXSJ" varchar(3) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKXSJ"),
  KEY "AAABBZYA" ("AAABMRIL","AAABMRHS","AAABMLPV"),
  KEY "AAABBZYE" ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZYG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCARJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCARJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZYL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRXL" varchar(1) NOT NULL default ' ',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRXL","AAABCATV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZYT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBZZM" varchar(1) NOT NULL default ' ',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABBZZO" varchar(3) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBZZM","AAABCATV","AAABBZZO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZZD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBZZM" varchar(1) NOT NULL default ' ',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABBZZO" varchar(3) NOT NULL default ' ',
  "AAABBZZP" varchar(1) NOT NULL default ' ',
  "AAABBZZQ" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABBZZM","AAABCATV","AAABBZZO","AAABBZZP","AAABBZZQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBZZR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCABB" varchar(8) NOT NULL default ' ',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABKXSJ" varchar(3) NOT NULL default ' ',
  "AAABGZSJ" varchar(6) NOT NULL default ' ',
  "AAABCAAS" varchar(3) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  "AAABBZZZ" varchar(60) NOT NULL default ' ',
  "AAABCAAA" varchar(60) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABCAAC" varchar(10) NOT NULL default ' ',
  "AAABCAAD" varchar(30) NOT NULL default ' ',
  "AAABCAAE" varchar(30) NOT NULL default ' ',
  "AAABCAAF" varchar(14) NOT NULL default ' ',
  "AAABIWXH" varchar(14) NOT NULL default ' ',
  "AAABCPHI" varchar(1) NOT NULL default ' ',
  "AAABILZU" varchar(1) NOT NULL default ' ',
  "AAABMNRG" varchar(1) NOT NULL default ' ',
  "AAABCAAK" varchar(8) NOT NULL default ' ',
  "AAABCAAT" varchar(1) NOT NULL default ' ',
  "AAABCAAU" varchar(1) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABLHAH" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCABB"),
  KEY "AAABCAAR" ("AAABCAAS","AAABCAAT","AAABCAAU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAAV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCABB" varchar(8) NOT NULL default ' ',
  "AAABCABC" varchar(3) NOT NULL default ' ',
  "AAABGZSJ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCABB","AAABCABC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCABD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCACP" varchar(1) NOT NULL default '0',
  "AAABIYJJ" varchar(3) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABCABI" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCACP","AAABIYJJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCABM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCACP" varchar(1) NOT NULL default '0',
  "AAABIYJJ" varchar(3) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCACP","AAABIYJJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCABW" (
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABCACH" varchar(1) NOT NULL default '0',
  "AAABCABZ" varchar(2) NOT NULL default '00',
  "AAABCACA" varchar(3) NOT NULL default '000',
  "AAABCACB" varchar(1) NOT NULL default ' ',
  "AAABCACC" varchar(1) NOT NULL default ' ',
  "AAABCACD" varchar(1) NOT NULL default ' ',
  "AAABCACE" varchar(1) NOT NULL default ' ',
  "AAABCACF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEHK","AAABCACH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCACI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCACP" varchar(1) NOT NULL default '0',
  "AAABIYJJ" varchar(3) NOT NULL default ' ',
  "AAABCACR" varchar(1) NOT NULL default '0',
  "AAABCACS" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCACP","AAABIYJJ","AAABCACR","AAABCACS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCACT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIYJJ" varchar(3) NOT NULL default ' ',
  "AAABKXSJ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIYJJ","AAABKXSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCADA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIYJJ" varchar(3) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIYJJ","AAABLEHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCADH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAABLRXL" varchar(1) NOT NULL default ' ',
  "AAABCAKN" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABCADO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAABLRXL","AAABCAKN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCADU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAABCAED" varchar(1) NOT NULL default ' ',
  "AAABCAEE" varchar(1) NOT NULL default ' ',
  "AAABCADZ" varchar(2) NOT NULL default ' ',
  "AAABCAEA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAABCAED","AAABCAEE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAEF" (
  "AAABCAMD" varchar(1) NOT NULL default ' ',
  "AAABCALZ" varchar(1) NOT NULL default ' ',
  "AAABGYNY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABCAMD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAEK" (
  "AAABCAMC" varchar(3) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABCAMD" varchar(1) NOT NULL default ' ',
  "AAABCALZ" varchar(1) NOT NULL default ' ',
  "AAABGYNY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABCAMC","AAABMOHM","AAABCAMD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAET" (
  "AAABCAME" varchar(4) NOT NULL default '0000',
  "AAABCAMC" varchar(3) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABCALR" varchar(1) NOT NULL default ' ',
  "AAABCATX" varchar(3) NOT NULL default ' ',
  "AAABCALT" varchar(1) NOT NULL default ' ',
  "AAABCALU" varchar(1) NOT NULL default ' ',
  "AAABCATZ" varchar(3) NOT NULL default ' ',
  "AAABCALW" varchar(1) NOT NULL default ' ',
  "AAABCALX" varchar(1) NOT NULL default ' ',
  "AAABCALY" varchar(6) NOT NULL default ' ',
  "AAABCAMD" varchar(1) NOT NULL default ' ',
  "AAABCALZ" varchar(1) NOT NULL default ' ',
  "AAABGYNY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABCAME")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAFJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAFP" varchar(3) NOT NULL default ' ',
  "AAABCAFQ" varchar(3) NOT NULL default ' ',
  "AAABCAFN" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAFP","AAABCAFQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAFR" (
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABCAMC" varchar(3) NOT NULL default ' ',
  "AAABCAFU" varchar(1) NOT NULL default ' ',
  "AAABCAFV" varchar(1) NOT NULL default ' ',
  "AAABCAFW" varchar(1) NOT NULL default ' ',
  "AAABCAFX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEHK","AAABCAMC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAGA" (
  "AAABLEEW" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABMCMG" smallint(6) NOT NULL default '0',
  "AAABMALF" smallint(6) NOT NULL default '0',
  "AAABCAGF" smallint(6) NOT NULL default '0',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLEEW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAGJ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLEEW" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABCAGN" varchar(70) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRHS","AAABLEEW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAGS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABCAIQ" varchar(1) NOT NULL default ' ',
  "AAABCAIR" varchar(2) NOT NULL default ' ',
  "AAABCAIS" smallint(6) NOT NULL default '0',
  "AAABCAGY" varchar(1) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABKFCZ","AAABCAIQ","AAABCAIR","AAABCAIS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAHF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABCAIQ" varchar(1) NOT NULL default ' ',
  "AAABCAIR" varchar(2) NOT NULL default ' ',
  "AAABCAIS" smallint(6) NOT NULL default '0',
  "AAABCAHM" varchar(70) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKFCZ","AAABCAIQ","AAABCAIR","AAABCAIS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAHU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAKE" int(11) NOT NULL default '0',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABCAHY" varchar(20) NOT NULL default ' ',
  "AAABCAHZ" varchar(20) NOT NULL default ' ',
  "AAABCAIA" varchar(4) NOT NULL default ' ',
  "AAABCAIB" varchar(3) NOT NULL default ' ',
  "AAABCAIC" varchar(3) NOT NULL default ' ',
  "AAABCAID" smallint(6) NOT NULL default '0',
  "AAABCAIE" smallint(6) NOT NULL default '0',
  "AAABCAIF" int(11) NOT NULL default '0',
  "AAABCAIG" varchar(1) NOT NULL default ' ',
  "AAABCAIH" smallint(6) NOT NULL default '0',
  "AAABCAII" int(11) NOT NULL default '0',
  "AAABCAJP" int(11) NOT NULL default '0',
  "AAABCAIK" int(11) NOT NULL default '0',
  "AAABCAJR" int(11) NOT NULL default '0',
  "AAABCAIM" varchar(3) NOT NULL default ' ',
  "AAABCAIN" int(11) NOT NULL default '0',
  "AAABCAIO" int(11) NOT NULL default '0',
  "AAABCAIP" varchar(1) NOT NULL default ' ',
  "AAABCAIQ" varchar(1) NOT NULL default ' ',
  "AAABCAIR" varchar(2) NOT NULL default ' ',
  "AAABCAIS" smallint(6) NOT NULL default '0',
  "AAABCAIT" varchar(1) NOT NULL default ' ',
  "AAABCAIU" varchar(1) NOT NULL default ' ',
  "AAABCAIV" int(11) NOT NULL default '0',
  "AAABCAIW" varchar(1) NOT NULL default ' ',
  "AAABCAIX" varchar(1) NOT NULL default ' ',
  "AAABCAIY" varchar(1) NOT NULL default ' ',
  "AAABCAIZ" varchar(1) NOT NULL default ' ',
  "AAABCAJA" varchar(1) NOT NULL default ' ',
  "AAABCAJB" varchar(1) NOT NULL default ' ',
  "AAABCAJC" varchar(1) NOT NULL default ' ',
  "AAABCAJD" varchar(1) NOT NULL default ' ',
  "AAABCAJE" varchar(1) NOT NULL default ' ',
  "AAABCAJF" varchar(1) NOT NULL default ' ',
  "AAABCAJG" varchar(1) NOT NULL default ' ',
  "AAABCAJH" varchar(1) NOT NULL default ' ',
  "AAABCAJI" varchar(1) NOT NULL default ' ',
  "AAABCAJJ" varchar(1) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABCAKE","AAABKFCZ"),
  KEY "AAABCAJO" ("AAABCAJP"),
  KEY "AAABCAJQ" ("AAABCAJR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAJS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAKA" varchar(20) NOT NULL default ' ',
  "AAABCAKE" int(11) NOT NULL default '0',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABCAJX" varchar(1) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABCAKA","AAABCAKE","AAABKFCZ"),
  KEY "AAABCAKD" ("AAABCAKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAKF" (
  "AAABLVJB" varchar(2) NOT NULL default ' ',
  "AAABCAKH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLVJB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAKJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAKP" varchar(2) NOT NULL default ' ',
  "AAABLRXL" varchar(1) NOT NULL default ' ',
  "AAABCAKN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAKP","AAABLRXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAKR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAXD" varchar(2) NOT NULL default ' ',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABCAKV" varchar(16) NOT NULL default ' ',
  "AAABGUEY" varchar(1) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAXD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCALA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCALH" varchar(2) NOT NULL default ' ',
  "AAABCALI" varchar(1) NOT NULL default ' ',
  "AAABCALJ" varchar(1) NOT NULL default ' ',
  "AAABCALK" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCALH","AAABCALI","AAABCALJ","AAABCALK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCALL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAMC" varchar(3) NOT NULL default ' ',
  "AAABCAMD" varchar(1) NOT NULL default ' ',
  "AAABCAME" varchar(4) NOT NULL default '0000',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABCALR" varchar(1) NOT NULL default ' ',
  "AAABCATX" varchar(3) NOT NULL default ' ',
  "AAABCALT" varchar(1) NOT NULL default ' ',
  "AAABCALU" varchar(1) NOT NULL default ' ',
  "AAABCATZ" varchar(3) NOT NULL default ' ',
  "AAABCALW" varchar(1) NOT NULL default ' ',
  "AAABCALX" varchar(1) NOT NULL default ' ',
  "AAABCALY" varchar(6) NOT NULL default ' ',
  "AAABCALZ" varchar(1) NOT NULL default ' ',
  "AAABGYNY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAMC","AAABCAMD","AAABCAME")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAMF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAXD" varchar(2) NOT NULL default ' ',
  "AAABCAML" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAXD","AAABCAML")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAMM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCANE" varchar(20) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLDON" varchar(6) NOT NULL default ' ',
  "AAABCAUG" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCANE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAMW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCANE" varchar(20) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABMLPV" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCANE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCANG" (
  "AAABCAOP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABCAOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCANJ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAOP" varchar(3) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABCAOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCANP" (
  "AAABCAOP" varchar(3) NOT NULL default ' ',
  "AAABMMJT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABCAOP","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCANU" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAOP" varchar(3) NOT NULL default ' ',
  "AAABMMJT" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABCAOP","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAOC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAOE" varchar(1) NOT NULL default ' ',
  "AAABCAOF" varchar(1) NOT NULL default ' ',
  "AAABCAOG" varchar(1) NOT NULL default ' ',
  "AAABCAOH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAOJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAOP" varchar(3) NOT NULL default ' ',
  "AAABCAXA" varchar(3) NOT NULL default ' ',
  "AAABMMJT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAOP","AAABCAXA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAOR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAOW" varchar(2) NOT NULL default ' ',
  "AAABCAOU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAOW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAOX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAXA" varchar(3) NOT NULL default ' ',
  "AAABHFND" varchar(3) NOT NULL default ' ',
  "AAABLKIV" varchar(3) NOT NULL default ' ',
  "AAABGZSJ" varchar(6) NOT NULL default ' ',
  "AAABJSTN" decimal(15,12) NOT NULL default '0.000000000000',
  "AAABJSTM" decimal(15,12) NOT NULL default '0.000000000000',
  "AAABJSTP" varchar(40) NOT NULL default ' ',
  "AAABKNIB" varchar(10) NOT NULL default ' ',
  "AAABJXDX" varchar(60) NOT NULL default ' ',
  "AAABCAPI" varchar(10) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABIYJJ" varchar(3) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  "AAABCAPM" varchar(40) NOT NULL default ' ',
  "AAABIYJL" varchar(40) NOT NULL default ' ',
  "AAABIWXH" varchar(40) NOT NULL default ' ',
  "AAABLHAH" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAXA","AAABHFND","AAABLKIV","AAABGZSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAPV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABKXSJ" varchar(3) NOT NULL default ' ',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABCAQA" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLPV"),
  KEY "AAABCAQD" ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAQF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABKXSJ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLPV","AAABKXSJ"),
  KEY "AAABCAQM" ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAQO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABKXSJ" varchar(3) NOT NULL default ' ',
  "AAABCAQS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLPV"),
  KEY "AAABCAQV" ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAQX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCARJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCARJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCARC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCARJ" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCARJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCARK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAXQ" varchar(2) NOT NULL default ' ',
  "AAABLRXL" varchar(1) NOT NULL default ' ',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABCARR" varchar(1) NOT NULL default ' ',
  "AAABCARS" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAXQ","AAABLRXL","AAABCATV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCARY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATU" varchar(8) NOT NULL default ' ',
  "AAABCATY" varchar(1) NOT NULL default ' ',
  "AAABCATZ" varchar(3) NOT NULL default ' ',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCATU","AAABCATY","AAABCATZ","AAABCATV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCASJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATU" varchar(8) NOT NULL default ' ',
  "AAABCATW" varchar(1) NOT NULL default ' ',
  "AAABCATX" varchar(3) NOT NULL default ' ',
  "AAABCATY" varchar(1) NOT NULL default ' ',
  "AAABCATZ" varchar(3) NOT NULL default ' ',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABCASR" varchar(1) NOT NULL default ' ',
  "AAABCASS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCATU","AAABCATW","AAABCATX","AAABCATY","AAABCATZ","AAABCATV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCATA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATU" varchar(8) NOT NULL default ' ',
  "AAABCATY" varchar(1) NOT NULL default ' ',
  "AAABCATZ" varchar(3) NOT NULL default ' ',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCATU","AAABCATY","AAABCATZ","AAABCATV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCATL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCATU" varchar(8) NOT NULL default ' ',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAABCATW" varchar(1) NOT NULL default ' ',
  "AAABCATX" varchar(3) NOT NULL default ' ',
  "AAABCATY" varchar(1) NOT NULL default ' ',
  "AAABCATZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCATU","AAABCATV","AAABCATW","AAABCATX","AAABCATY","AAABCATZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAUA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAXA" varchar(3) NOT NULL default ' ',
  "AAABCAUD" varchar(20) NOT NULL default ' ',
  "AAABKNCF" varchar(30) NOT NULL default ' ',
  "AAABCAUF" varchar(30) NOT NULL default ' ',
  "AAABCAUG" varchar(12) NOT NULL default ' ',
  "AAABCAUH" varchar(30) NOT NULL default ' ',
  "AAABCAUI" varchar(40) NOT NULL default ' ',
  "AAABCAUJ" varchar(32) NOT NULL default ' ',
  "AAABCAUK" tinyint(3) unsigned NOT NULL default '0',
  "AAABCAUL" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABCAXA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAUO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABCAVC" int(11) NOT NULL default '0',
  "AAABCAVD" int(11) NOT NULL default '0',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABLTIZ" varchar(6) NOT NULL default '000000',
  "AAABMRCU" varchar(4) NOT NULL default ' ',
  "AAABGLCK" smallint(6) NOT NULL default '0',
  "AAABCAUX" int(11) NOT NULL default '0',
  "AAABCAUY" int(11) NOT NULL default '0',
  "AAABCAUZ" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXIY","AAABCAVC","AAABCAVD","AAABMRFQ","AAABLTIZ","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAVH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAYS" varchar(2) NOT NULL default ' ',
  "AAABCAVN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAYS","AAABCAVN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAVO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAWP" varchar(4) NOT NULL default '0000',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAVW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAWP" varchar(4) NOT NULL default '0000',
  "AAABCAWQ" varchar(2) NOT NULL default '00',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAWP","AAABCAWQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAWG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAWP" varchar(4) NOT NULL default '0000',
  "AAABCAWQ" varchar(2) NOT NULL default '00',
  "AAABCAWR" varchar(2) NOT NULL default '00',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAWP","AAABCAWQ","AAABCAWR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAWS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAXA" varchar(3) NOT NULL default ' ',
  "AAABCAXB" varchar(12) NOT NULL default ' ',
  "AAABCAXC" varchar(2) NOT NULL default ' ',
  "AAABCAXD" varchar(2) NOT NULL default ' ',
  "AAABCAWY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAXA","AAABCAXB","AAABCAXC","AAABCAXD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAXE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAXQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAXJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAXQ" varchar(2) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAXR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAYD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAXW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAYD" varchar(2) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAYE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAYS" varchar(2) NOT NULL default ' ',
  "AAABLBEK" varchar(1) NOT NULL default ' ',
  "AAABCAYI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAYL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAYS" varchar(2) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAYT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(12) NOT NULL default ' ',
  "AAABCAYW" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAYZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAZU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAZU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAZE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCAZU" varchar(4) NOT NULL default ' ',
  "AAABMDXM" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCAZU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAZM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAZU" varchar(4) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABCAZQ" varchar(30) NOT NULL default ' ',
  "AAABCAZR" varchar(30) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAZU","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCAZW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABCBAH" varchar(4) NOT NULL default '0000',
  "AAABCBAI" varchar(3) NOT NULL default '000',
  "AAABCBAJ" varchar(2) NOT NULL default '00',
  "AAABCBHE" varchar(10) NOT NULL default ' ',
  "AAABCBHF" varchar(10) NOT NULL default ' ',
  "AAABCBAE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABCBAH","AAABCBAI","AAABCBAJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBAK" (
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABCBGD" varchar(4) NOT NULL default ' ',
  "AAABIJSC" varchar(10) NOT NULL default ' ',
  "AAABFNZI" varchar(40) NOT NULL default ' ',
  "AAABFNZJ" varchar(40) NOT NULL default ' ',
  "AAABFNZK" varchar(30) NOT NULL default ' ',
  "AAABFNZL" varchar(30) NOT NULL default ' ',
  "AAABFNZM" varchar(40) NOT NULL default ' ',
  "AAABFNZN" varchar(20) NOT NULL default ' ',
  "AAABFNZO" varchar(20) NOT NULL default ' ',
  "AAABFNZP" varchar(1) NOT NULL default ' ',
  "AAABFNZQ" varchar(1) NOT NULL default ' ',
  "AAABFNZR" varchar(1) NOT NULL default ' ',
  "AAABFNZS" varchar(1) NOT NULL default ' ',
  "AAABFNZT" varchar(1) NOT NULL default ' ',
  "AAABFNZU" varchar(1) NOT NULL default ' ',
  "AAABFNZV" varchar(1) NOT NULL default ' ',
  "AAABCBBD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHQV","AAABIHQW","AAABCBGD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBBH" (
  "AAABFOGY" varchar(16) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default ' ',
  "AAABLUWF" varchar(1) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABMNYU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFOGY","AAABLECD","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBBR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABCBGD" varchar(4) NOT NULL default ' ',
  "AAABCBFS" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(24) NOT NULL default ' ',
  "AAABFOBU" varchar(12) NOT NULL default ' ',
  "AAABFOBV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABCBGD","AAABCBFS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBCF" (
  "AAABFOAQ" varchar(10) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABFOAQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBCJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABFOBI" varchar(12) NOT NULL default ' ',
  "AAABFOBV" varchar(8) NOT NULL default '00000000',
  "AAABGOAH" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZKF","AAABFOBI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBCS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABCBGD" varchar(4) NOT NULL default ' ',
  "AAABCBFS" varchar(32) NOT NULL default ' ',
  "AAABFODZ" varchar(4) NOT NULL default '0000',
  "AAABLUEO" varchar(8) NOT NULL default ' ',
  "AAABFOBG" varchar(60) NOT NULL default ' ',
  "AAABFOBH" varchar(60) NOT NULL default ' ',
  "AAABFOBI" varchar(12) NOT NULL default ' ',
  "AAABFOBJ" varchar(4) NOT NULL default ' ',
  "AAABIMHJ" varchar(8) NOT NULL default ' ',
  "AAABFOBL" varchar(8) NOT NULL default ' ',
  "AAABFOBM" varchar(1) NOT NULL default ' ',
  "AAABFOBN" varchar(1) NOT NULL default ' ',
  "AAABFOBO" varchar(1) NOT NULL default ' ',
  "AAABFOBP" varchar(1) NOT NULL default ' ',
  "AAABFOBQ" varchar(1) NOT NULL default ' ',
  "AAABFOBR" varchar(1) NOT NULL default ' ',
  "AAABFOBS" varchar(1) NOT NULL default ' ',
  "AAABFOBT" varchar(1) NOT NULL default ' ',
  "AAABFOBU" varchar(12) NOT NULL default ' ',
  "AAABFOBV" varchar(8) NOT NULL default '00000000',
  "AAABFOBW" varchar(32) NOT NULL default ' ',
  "AAABFOBX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABCBGD","AAABCBFS","AAABFODZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBDY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABCBGD" varchar(4) NOT NULL default ' ',
  "AAABCBFS" varchar(32) NOT NULL default ' ',
  "AAABFODZ" varchar(4) NOT NULL default '0000',
  "AAABFODJ" varchar(1) NOT NULL default ' ',
  "AAABFODK" varchar(4) NOT NULL default '0000',
  "AAABKNCF" varchar(32) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABFOCQ" varchar(1) NOT NULL default ' ',
  "AAABFOCR" varchar(1) NOT NULL default ' ',
  "AAABFOCS" varchar(1) NOT NULL default ' ',
  "AAABFOCT" varchar(4) NOT NULL default '0000',
  "AAABFOCU" varchar(4) NOT NULL default '0000',
  "AAABFOCV" varchar(1) NOT NULL default ' ',
  "AAABJETU" varchar(60) NOT NULL default ' ',
  "AAABFOCX" varchar(1) NOT NULL default ' ',
  "AAABFOCY" varchar(1) NOT NULL default ' ',
  "AAABFOCZ" varchar(1) NOT NULL default ' ',
  "AAABFODA" varchar(1) NOT NULL default ' ',
  "AAABFODB" varchar(1) NOT NULL default ' ',
  "AAABMJCM" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABCBGD","AAABCBFS","AAABFODZ","AAABFODJ","AAABFODK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBFF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABCBGD" varchar(4) NOT NULL default ' ',
  "AAABCBFS" varchar(32) NOT NULL default ' ',
  "AAABFODZ" varchar(4) NOT NULL default '0000',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABCBGD","AAABCBFS","AAABFODZ","AAABMNJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBFV" (
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABCBGD" varchar(4) NOT NULL default ' ',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHQV","AAABIHQW","AAABCBGD","AAABMNJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBGF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
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
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBGX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABICIR" varchar(2) NOT NULL default '00',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABCBHE" varchar(10) NOT NULL default ' ',
  "AAABCBHF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBB","AAABICIR","AAABMQCA","AAABMQCB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBHM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDSZT" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABDSZT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBHR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDSZT" varchar(2) NOT NULL default '00',
  "AAABLPUZ" varchar(90) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDSZT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBHZ" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABHEIG" varchar(30) NOT NULL default ' ',
  "AAABCBJS" varchar(40) NOT NULL default ' ',
  "AAABCBJT" varchar(20) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABCBLU" varchar(10) NOT NULL default ' ',
  "AAABCBLV" varchar(3) NOT NULL default ' ',
  "AAABIOAK" varchar(12) NOT NULL default ' ',
  "AAABIMEK" varchar(8) NOT NULL default '00000000',
  "AAABIMEL" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMREL","AAABHEIG","AAABCBJS","AAABCBJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBIO" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABHEIG" varchar(30) NOT NULL default ' ',
  "AAABCBJS" varchar(40) NOT NULL default ' ',
  "AAABCBJT" varchar(20) NOT NULL default ' ',
  "AAABIJPF" varchar(1) NOT NULL default ' ',
  "AAABCBLU" varchar(10) NOT NULL default ' ',
  "AAABCBLV" varchar(3) NOT NULL default ' ',
  "AAABIOAK" varchar(12) NOT NULL default ' ',
  "AAABIMEK" varchar(8) NOT NULL default '00000000',
  "AAABIMEL" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMREL","AAABHEIG","AAABCBJS","AAABCBJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBJD" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABHEIG" varchar(30) NOT NULL default ' ',
  "AAABCBJS" varchar(40) NOT NULL default ' ',
  "AAABCBJT" varchar(20) NOT NULL default ' ',
  "AAABCBJI" varchar(10) NOT NULL default ' ',
  "AAABCBJJ" varchar(3) NOT NULL default '000',
  "AAABCBJK" varchar(4) NOT NULL default ' ',
  "AAABCBLU" varchar(10) NOT NULL default ' ',
  "AAABCBLV" varchar(3) NOT NULL default ' ',
  "AAABIOAK" varchar(12) NOT NULL default ' ',
  "AAABIMEK" varchar(8) NOT NULL default '00000000',
  "AAABIMEL" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMREL","AAABHEIG","AAABCBJS","AAABCBJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBJU" (
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABLWWP" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABCBJY" varchar(1) NOT NULL default ' ',
  "AAABCBJZ" varchar(1) NOT NULL default ' ',
  "AAABCBKA" varchar(1) NOT NULL default ' ',
  "AAABDEFZ" varchar(1) NOT NULL default ' ',
  "AAABKNBQ" varchar(1) NOT NULL default ' ',
  "AAABCBKD" varchar(1) NOT NULL default ' ',
  "AAABCBKE" varchar(30) NOT NULL default ' ',
  "AAABCBKF" varchar(30) NOT NULL default ' ',
  "AAABCBKG" varchar(30) NOT NULL default ' ',
  "AAABHCEC" varchar(5) NOT NULL default '00000',
  "AAABCBKI" varchar(1) NOT NULL default ' ',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNYT","AAABLWWP","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMCPR" varchar(1) NOT NULL default ' ',
  "AAABCBKW" varchar(2) NOT NULL default ' ',
  "AAABCBKX" varchar(2) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCSO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBLC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCBLM" varchar(4) NOT NULL default ' ',
  "AAABCBLN" varchar(4) NOT NULL default ' ',
  "AAABKOCM" varchar(4) NOT NULL default ' ',
  "AAABKRBW" varchar(1) NOT NULL default ' ',
  "AAABKOFR" varchar(1) NOT NULL default ' ',
  "AAABCBLJ" varchar(2) NOT NULL default ' ',
  "AAABCBLK" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCBLM","AAABCBLN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBLO" (
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABHEIG" varchar(30) NOT NULL default ' ',
  "AAABEPEV" varchar(30) NOT NULL default ' ',
  "AAABEPEY" varchar(30) NOT NULL default ' ',
  "AAABCBLU" varchar(10) NOT NULL default ' ',
  "AAABCBLV" varchar(3) NOT NULL default ' ',
  "AAABIOAK" varchar(12) NOT NULL default ' ',
  "AAABIMEK" varchar(8) NOT NULL default '00000000',
  "AAABIMEL" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMOHM","AAABMREL","AAABHEIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBMC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOIB" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBMH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOIB" varchar(14) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMOIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBMP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOIB" varchar(14) NOT NULL default ' ',
  "AAABCBMY" varchar(4) NOT NULL default ' ',
  "AAABCBMT" varchar(2) NOT NULL default '00',
  "AAABCBMU" varchar(2) NOT NULL default '00',
  "AAABCBMV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMOIB","AAABCBMY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBMZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIAXA" varchar(3) NOT NULL default ' ',
  "AAABCBRN" varchar(2) NOT NULL default ' ',
  "AAABCBND" varchar(1) NOT NULL default ' ',
  "AAABCDTJ" varchar(1) NOT NULL default ' ',
  "AAABCBNF" varchar(1) NOT NULL default ' ',
  "AAABCBNG" varchar(1) NOT NULL default ' ',
  "AAABCBNH" varchar(1) NOT NULL default ' ',
  "AAABCBNI" varchar(1) NOT NULL default ' ',
  "AAABCBNJ" varchar(1) NOT NULL default ' ',
  "AAABCBNK" varchar(3) NOT NULL default ' ',
  "AAABCBNL" varchar(1) NOT NULL default ' ',
  "AAABCBNM" varchar(1) NOT NULL default ' ',
  "AAABCBNN" varchar(3) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABCBNP" varchar(1) NOT NULL default ' ',
  "AAABCBNQ" varchar(3) NOT NULL default ' ',
  "AAABCBSP" varchar(2) NOT NULL default ' ',
  "AAABCBNS" varchar(1) NOT NULL default ' ',
  "AAABCBNT" varchar(1) NOT NULL default ' ',
  "AAABCBNU" varchar(1) NOT NULL default ' ',
  "AAABCBNV" varchar(1) NOT NULL default ' ',
  "AAABCBNW" varchar(1) NOT NULL default ' ',
  "AAABCBNX" varchar(1) NOT NULL default ' ',
  "AAABCBNY" varchar(1) NOT NULL default ' ',
  "AAABCBNZ" varchar(1) NOT NULL default ' ',
  "AAABCBOA" varchar(1) NOT NULL default ' ',
  "AAABCBOB" varchar(1) NOT NULL default ' ',
  "AAABCBOC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIAXA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBOF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAABIAXA" varchar(3) NOT NULL default ' ',
  "AAABIAWX" varchar(2) NOT NULL default '00',
  "AAABCBOK" varchar(1) NOT NULL default ' ',
  "AAABCBOL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABICIU","AAABIAXA","AAABIAWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBOQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABIAXA" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBOW" (
  "AAABCBPJ" varchar(20) NOT NULL default ' ',
  "AAABGRZB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABCBPJ","AAABGRZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBPB" (
  "AAABCBPJ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABCBPJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBPE" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCBPJ" varchar(20) NOT NULL default ' ',
  "AAABCBPH" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABCBPJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBPK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCBRN" varchar(2) NOT NULL default ' ',
  "AAABMGFB" varchar(1) NOT NULL default ' ',
  "AAABCBPO" varchar(1) NOT NULL default ' ',
  "AAABCBPP" varchar(3) NOT NULL default ' ',
  "AAABCBPQ" varchar(1) NOT NULL default ' ',
  "AAABCBPR" varchar(1) NOT NULL default ' ',
  "AAABIAVP" varchar(1) NOT NULL default '0',
  "AAABCBPT" varchar(1) NOT NULL default ' ',
  "AAABGNBZ" varchar(10) NOT NULL default ' ',
  "AAABCBPV" varchar(1) NOT NULL default ' ',
  "AAABCBPW" varchar(1) NOT NULL default ' ',
  "AAABCBPX" varchar(1) NOT NULL default ' ',
  "AAABCBPY" varchar(1) NOT NULL default ' ',
  "AAABCBPZ" varchar(1) NOT NULL default ' ',
  "AAABCBQA" varchar(1) NOT NULL default ' ',
  "AAABFIGO" varchar(7) NOT NULL default ' ',
  "AAABCBQC" varchar(1) NOT NULL default '0',
  "AAABFHKK" varchar(7) NOT NULL default ' ',
  "AAABCBQE" varchar(1) NOT NULL default '0',
  "AAABCBQF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCBRN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBQI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCBRN" varchar(2) NOT NULL default ' ',
  "AAABCBQM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCBRN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCBSC" varchar(4) NOT NULL default ' ',
  "AAABCBRN" varchar(2) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABCBQV" varchar(2) NOT NULL default '00',
  "AAABCBQW" varchar(2) NOT NULL default '00',
  "AAABCBQX" varchar(2) NOT NULL default '00',
  "AAABCBQY" varchar(2) NOT NULL default '00',
  "AAABCBQZ" varchar(2) NOT NULL default '00',
  "AAABCBRA" varchar(2) NOT NULL default '00',
  "AAABCBRB" varchar(2) NOT NULL default '00',
  "AAABCBRC" varchar(2) NOT NULL default '00',
  "AAABCBRD" varchar(2) NOT NULL default '00',
  "AAABCBRE" varchar(2) NOT NULL default '00',
  "AAABCBRF" varchar(2) NOT NULL default '00',
  "AAABCBRG" varchar(2) NOT NULL default '00',
  "AAABCBRH" varchar(2) NOT NULL default '00',
  "AAABCBRI" varchar(2) NOT NULL default '00',
  "AAABCBRJ" varchar(2) NOT NULL default '00',
  "AAABCBRK" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABCBSC","AAABCBRN","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBRP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCBSC" varchar(4) NOT NULL default ' ',
  "AAABCBRT" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCBSC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBRX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCBSC" varchar(4) NOT NULL default ' ',
  "AAABCBSA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCBSC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBSD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCBSP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCBSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBSI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCBSP" varchar(2) NOT NULL default ' ',
  "AAABCBSM" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCBSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIAXA" varchar(3) NOT NULL default ' ',
  "AAABCBSU" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABIAXA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBSY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAABIAXA" varchar(3) NOT NULL default ' ',
  "AAABIAWX" varchar(2) NOT NULL default '00',
  "AAABCBTD" varchar(1) NOT NULL default '0',
  "AAABCBTE" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABICIU","AAABIAXA","AAABIAWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBTJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCBTU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCBTU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBTO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCBTU" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCBTU","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBTW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABCBUC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPJD","AAABCBUC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBUD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCBUI" varchar(2) NOT NULL default ' ',
  "AAABGTHF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCBUI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBUJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGTHF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGTHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBUO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGTHF" varchar(4) NOT NULL default ' ',
  "AAABCBUS" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABGTHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBUW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGTHF" varchar(4) NOT NULL default ' ',
  "AAABCBVH" varchar(2) NOT NULL default ' ',
  "AAABCBVA" varchar(4) NOT NULL default ' ',
  "AAABCBVB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGTHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBVE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGTHF" varchar(4) NOT NULL default ' ',
  "AAABCBVH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGTHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBVK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABCBVO" varchar(4) NOT NULL default '0000',
  "AAABCBVP" varchar(4) NOT NULL default '0000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHC","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEZHC","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBWE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABCBWH" varchar(1) NOT NULL default ' ',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABCBWJ" varchar(30) NOT NULL default ' ',
  "AAABCBWK" varchar(30) NOT NULL default ' ',
  "AAABCBWL" varchar(30) NOT NULL default ' ',
  "AAABCBWM" varchar(30) NOT NULL default ' ',
  "AAABCBWN" varchar(30) NOT NULL default ' ',
  "AAABCBWO" varchar(30) NOT NULL default ' ',
  "AAABCBWP" varchar(30) NOT NULL default ' ',
  "AAABCBWQ" varchar(30) NOT NULL default ' ',
  "AAABCBWR" varchar(30) NOT NULL default ' ',
  "AAABCBWS" varchar(30) NOT NULL default ' ',
  "AAABCBWT" varchar(30) NOT NULL default ' ',
  "AAABCBWU" varchar(4) NOT NULL default ' ',
  "AAABLYCI" varchar(2) NOT NULL default ' ',
  "AAABCBWW" varchar(1) NOT NULL default ' ',
  "AAABCCFO" varchar(3) NOT NULL default ' ',
  "AAABCBWY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBXB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABCBXE" varchar(30) NOT NULL default ' ',
  "AAABCBXF" varchar(4) NOT NULL default ' ',
  "AAABCBXG" varchar(1) NOT NULL default ' ',
  "AAABCBXH" varchar(16) NOT NULL default ' ',
  "AAABCBXI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBXL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABEPBY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABEZHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBXT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABCBXX" varchar(1) NOT NULL default ' ',
  "AAABCBXY" varchar(1) NOT NULL default ' ',
  "AAABCBXZ" varchar(8) NOT NULL default '00000000',
  "AAABCBYA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHC","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBYE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMJCM" varchar(2) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMJCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMJCM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMJCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBYP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMJCM" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMJCM","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBYX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLLJG" decimal(8,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABEZHC","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBZF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABCBZV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHC","AAABCBZV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBZM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCBZV" varchar(4) NOT NULL default ' ',
  "AAABEPBY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHC","AAABMRHS","AAABCBZV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCBZW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABCCAM" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHC","AAABCCAM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCAD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCCAM" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHC","AAABMRHS","AAABCCAM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABMPMH" varchar(10) NOT NULL default ' ',
  "AAABFOMB" varchar(3) NOT NULL default '000',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABEZHC","AAABMPMH","AAABFOMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCAZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABCCBQ" varchar(10) NOT NULL default ' ',
  "AAABCCGQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHC","AAABCCBQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCBH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCCBQ" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHC","AAABMLQA","AAABCCBQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCBR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABEOJJ" varchar(6) NOT NULL default ' ',
  "AAABCCFB" varchar(1) NOT NULL default ' ',
  "AAABEPCK" varchar(1) NOT NULL default ' ',
  "AAABCCBX" varchar(1) NOT NULL default ' ',
  "AAABCCBY" varchar(1) NOT NULL default ' ',
  "AAABCCBZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHC","AAABEOJJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCCD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABEOJJ" varchar(6) NOT NULL default ' ',
  "AAABEPBY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEZHC","AAABEOJJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCCN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABEPCO" varchar(4) NOT NULL default ' ',
  "AAABEOJJ" varchar(6) NOT NULL default ' ',
  "AAABCCDA" varchar(6) NOT NULL default '000000',
  "AAABMJCM" varchar(2) NOT NULL default ' ',
  "AAABCCCU" varchar(6) NOT NULL default ' ',
  "AAABCCCV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHC","AAABEPCO","AAABEOJJ","AAABCCDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCDB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABEPCO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHC","AAABMREX","AAABEPCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCDK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABCCDN" varchar(40) NOT NULL default ' ',
  "AAABCCDO" varchar(40) NOT NULL default ' ',
  "AAABCCDP" varchar(40) NOT NULL default ' ',
  "AAABCCDQ" varchar(40) NOT NULL default ' ',
  "AAABCCDR" varchar(40) NOT NULL default ' ',
  "AAABCCDS" varchar(40) NOT NULL default ' ',
  "AAABCCDT" varchar(40) NOT NULL default ' ',
  "AAABCCDU" varchar(40) NOT NULL default ' ',
  "AAABCCDV" varchar(40) NOT NULL default ' ',
  "AAABCCDW" varchar(40) NOT NULL default ' ',
  "AAABCCDX" varchar(40) NOT NULL default ' ',
  "AAABCCDY" varchar(40) NOT NULL default ' ',
  "AAABCCDZ" varchar(40) NOT NULL default ' ',
  "AAABCCEA" varchar(40) NOT NULL default ' ',
  "AAABCCEB" varchar(40) NOT NULL default ' ',
  "AAABCCEC" varchar(40) NOT NULL default ' ',
  "AAABCCED" varchar(4) NOT NULL default '0000',
  "AAABCCEE" varchar(40) NOT NULL default ' ',
  "AAABCCEF" varchar(4) NOT NULL default '0000',
  "AAABCCEG" varchar(40) NOT NULL default ' ',
  "AAABCCEH" varchar(4) NOT NULL default '0000',
  "AAABCCEI" varchar(40) NOT NULL default ' ',
  "AAABCCEJ" varchar(4) NOT NULL default '0000',
  "AAABCCEK" varchar(40) NOT NULL default ' ',
  "AAABCCEL" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABEZHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCEO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABCCEV" varchar(30) NOT NULL default ' ',
  "AAABCCES" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHC","AAABCCEV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCEW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABEPCO" varchar(4) NOT NULL default ' ',
  "AAABEOJJ" varchar(6) NOT NULL default ' ',
  "AAABCCFB" varchar(1) NOT NULL default ' ',
  "AAABCCFC" varchar(10) NOT NULL default ' ',
  "AAABCCFD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHC","AAABEPCO","AAABEOJJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCFI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCCFO" varchar(3) NOT NULL default ' ',
  "AAABMPMH" varchar(10) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCCFO","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCFQ" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMPMH" varchar(10) NOT NULL default ' ',
  "AAABLISJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCFW" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMPMH" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABMPMH","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCGE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCCGQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCCGQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCGJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCCGQ" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABCCGQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCGR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABEPCO" varchar(4) NOT NULL default ' ',
  "AAABEOJJ" varchar(6) NOT NULL default ' ',
  "AAABJVHM" varchar(1) NOT NULL default ' ',
  "AAABCCGY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHC","AAABMREX","AAABEPCO","AAABEOJJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABCCIE" varchar(16) NOT NULL default ' ',
  "AAABMLTT" decimal(2,0) NOT NULL default '0',
  "AAABMIZS" varchar(30) NOT NULL default ' ',
  "AAABIJYD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHC","AAABCCIE","AAABMLTT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCHP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABCCIE" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHC","AAABCCIE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCHW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABCCIE" varchar(16) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHC","AAABCCIE","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCIG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABCCIX" varchar(10) NOT NULL default ' ',
  "AAABLDTB" varchar(30) NOT NULL default ' ',
  "AAABCCIL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHC","AAABCCIX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCIP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABCCIX" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHC","AAABCCIX","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCIZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(4) NOT NULL default '0000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABEPCO" varchar(4) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABFIGO" varchar(7) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMPMH","AAABMREX","AAABEPCO","AAABMNZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCJN" (
  "AAABKVFD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABKVFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCJQ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKVFD" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABKVFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCJW" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIYCS" varchar(4) NOT NULL default ' ',
  "AAABHALU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABIYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCKC" (
  "AAABCRMI" varchar(10) NOT NULL default ' ',
  "AAABKVFD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABCRMI","AAABKVFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCKH" (
  "AAABIYCS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABIYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCKK" (
  "AAABFRPG" varchar(5) NOT NULL default ' ',
  "AAABCCKQ" varchar(2) NOT NULL default ' ',
  "AAABCCKN" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABFRPG"),
  UNIQUE KEY "AAABCCKP" ("AAABCCKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCKR" (
  "AAABFRPG" varchar(5) NOT NULL default ' ',
  "AAABHBWQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFRPG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCKV" (
  "AAABCCKZ" varchar(8) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLUFA" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABCCKZ","AAABMDEN","AAABLUFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLISU" (
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAABKMUQ" varchar(10) NOT NULL default ' ',
  "AAABKNBT" varchar(10) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABHFBZ" varchar(1) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABHFPL" varchar(1) NOT NULL default ' ',
  "AAABHFCC" varchar(1) NOT NULL default ' ',
  "AAABHFCD" varchar(8) NOT NULL default ' ',
  "AAABKMZH" varchar(1) NOT NULL default ' ',
  "AAABHFCF" varchar(20) NOT NULL default ' ',
  "AAABCCLQ" varchar(1) NOT NULL default ' ',
  "AAABCCLR" varchar(1) NOT NULL default ' ',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABCCLT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLRST","AAABMKFH","AAABLFYD"),
  KEY "AAABCCLX" ("AAABLRQK"),
  KEY "AAABCCLZ" ("AAABIRUN","AAABLRST","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCMD" (
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABKCWA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABIRUN","AAABIITH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCML" (
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABHFPL" varchar(1) NOT NULL default ' ',
  "AAABKCWA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABLFYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCMV" (
  "AAABLQGL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCMY" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLQGL" varchar(4) NOT NULL default ' ',
  "AAABLWJT" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCNE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCCNP" VARBINARY(255) NOT NULL,
  "AAABCCNS" varchar(32) NOT NULL default ' ',
  "AAABCCNI" varchar(12) NOT NULL default ' ',
  "AAABCCNJ" varchar(12) NOT NULL default ' ',
  "AAABCCNK" decimal(13,3) NOT NULL default '0.000',
  "AAABCCNL" decimal(13,3) NOT NULL default '0.000',
  "AAABCCNM" varchar(3) NOT NULL default ' ',
  "AAABLVJB" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCCNP"(255)),
  UNIQUE KEY "AAABCCNQ" ("AAABMRIL","AAABCCNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCNT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCCNZ" varchar(1) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABFVKL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCCNZ","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCOB" (
  "AAABHXLS" varchar(30) NOT NULL default ' ',
  "AAABDHYW" varchar(30) NOT NULL default ' ',
  "AAABCCOI" varchar(30) NOT NULL default ' ',
  "AAABLQTM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHXLS","AAABDHYW","AAABCCOI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCOJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTKC","AAABLXDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCOQ" (
  "AAABCCPQ" varchar(30) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCCPT" varchar(30) NOT NULL default ' ',
  "AAABCCPB" varchar(30) NOT NULL default ' ',
  "AAABLLLP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABCCPQ","AAABLVCU","AAABMLQA","AAABCCPT","AAABCCPB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCPC" (
  "AAABCCPQ" varchar(30) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABCCPQ","AAABLVCU","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCPK" (
  "AAABCCPQ" varchar(30) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCCPT" varchar(30) NOT NULL default ' ',
  "AAABLLLP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABCCPQ","AAABLVCU","AAABMLQA","AAABCCPT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCPU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLECT" varchar(22) NOT NULL default ' ',
  "AAABLECU" varchar(22) NOT NULL default ' ',
  "AAABLECV" varchar(22) NOT NULL default ' ',
  "AAABLECJ" varchar(6) NOT NULL default ' ',
  "AAABMRGO" longtext NOT NULL,
  "AAABMRCK" longtext NOT NULL,
  "AAABLTQA" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(6) NOT NULL default ' ',
  "AAABLECO" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLECT","AAABLECU","AAABLECV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCQJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEQYU" varchar(1) NOT NULL default ' ',
  "AAABMNPV" varchar(1) NOT NULL default '0',
  "AAABDICO" varchar(1) NOT NULL default ' ',
  "AAABDICP" varchar(1) NOT NULL default ' ',
  "AAABDICQ" varchar(1) NOT NULL default ' ',
  "AAABDICR" varchar(1) NOT NULL default ' ',
  "AAABDICS" varchar(1) NOT NULL default ' ',
  "AAABDICT" varchar(1) NOT NULL default ' ',
  "AAABDICU" varchar(1) NOT NULL default ' ',
  "AAABDICV" varchar(1) NOT NULL default ' ',
  "AAABDICW" varchar(1) NOT NULL default ' ',
  "AAABLTXN" varchar(1) NOT NULL default ' ',
  "AAABDICY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEQYU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCRA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDIIT" varchar(5) NOT NULL default ' ',
  "AAABKRJY" varchar(24) NOT NULL default ' ',
  "AAABDIII" varchar(1) NOT NULL default ' ',
  "AAABDIIJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDIIT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCRI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDIIT" varchar(5) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDIIT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCRQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRFV" varchar(4) NOT NULL default ' ',
  "AAABKFFG" varchar(6) NOT NULL default ' ',
  "AAABCCRU" varchar(1) NOT NULL default ' ',
  "AAABCCRV" varchar(1) NOT NULL default ' ',
  "AAABKRHS" varchar(2) NOT NULL default ' ',
  "AAABCCRX" varchar(1) NOT NULL default ' ',
  "AAABCCRY" varchar(1) NOT NULL default ' ',
  "AAABCCRZ" varchar(1) NOT NULL default ' ',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABCCSB" varchar(1) NOT NULL default ' ',
  "AAABCCSC" varchar(1) NOT NULL default ' ',
  "AAABCCSD" varchar(1) NOT NULL default ' ',
  "AAABCCSE" varchar(1) NOT NULL default ' ',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABCCSG" varchar(1) NOT NULL default ' ',
  "AAABCDAD" varchar(6) NOT NULL default ' ',
  "AAABCCSI" varchar(1) NOT NULL default ' ',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABCCSK" varchar(1) NOT NULL default ' ',
  "AAABKOVB" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRFV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCSO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRFV" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKRFF" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRFV","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCSW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRFV" varchar(4) NOT NULL default ' ',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRFV","AAABLTXG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCTD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRFV" varchar(4) NOT NULL default ' ',
  "AAABCCTZ" varchar(10) NOT NULL default ' ',
  "AAABCCTH" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRFV","AAABCCTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCTL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRFV" varchar(4) NOT NULL default ' ',
  "AAABCCTZ" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKRFF" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRFV","AAABCCTZ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRFV" varchar(4) NOT NULL default ' ',
  "AAABLYAH" varchar(2) NOT NULL default '00',
  "AAABCCTZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRFV","AAABLYAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCUD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPUY" varchar(18) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABELKD" varchar(4) NOT NULL default '0000',
  "AAABCCUR" varchar(3) NOT NULL default '000',
  "AAABCCUK" decimal(11,8) NOT NULL default '0.00000000',
  "AAABMCSL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPUY","AAABETQU","AAABELKD","AAABCCUR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCUS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCCWO" varchar(4) NOT NULL default ' ',
  "AAABCCUV" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABCCWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCUY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCCWO" varchar(4) NOT NULL default ' ',
  "AAABCCVC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCCWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCVG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCCWO" varchar(4) NOT NULL default ' ',
  "AAABCCVW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCCWO","AAABCCVW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCVN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCCWO" varchar(4) NOT NULL default ' ',
  "AAABCCVW" varchar(20) NOT NULL default ' ',
  "AAABCCVS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCCWO","AAABCCVW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCVX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCCWE" varchar(2) NOT NULL default '00',
  "AAABCCWO" varchar(4) NOT NULL default ' ',
  "AAABCCWB" varchar(2) NOT NULL default '00',
  "AAABCCWC" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABCCWE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCWF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCCWO" varchar(4) NOT NULL default ' ',
  "AAABCCWP" varchar(3) NOT NULL default '000',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(40) NOT NULL default ' ',
  "AAABMNNA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCCWO","AAABCCWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCWQ" (
  "AAABFMOY" varchar(30) NOT NULL default ' ',
  "AAABCCWS" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABFMOY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCWU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABFMOY" varchar(30) NOT NULL default ' ',
  "AAABCCWX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABFMOY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCXA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCCXM" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCCXM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCXF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCCXM" varchar(3) NOT NULL default ' ',
  "AAABCCXJ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABCCXM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCXN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRIM" varchar(10) NOT NULL default ' ',
  "AAABCCXV" varchar(8) NOT NULL default ' ',
  "AAABCCXW" varchar(4) NOT NULL default '0000',
  "AAABCCXS" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRIM","AAABCCXV","AAABCCXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCCXX" (
  "AAABIHEH" varchar(4) NOT NULL default ' ',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABHFMC" varchar(1) NOT NULL default ' ',
  "AAABIIMJ" varchar(30) NOT NULL default ' ',
  "AAABHFME" varchar(1) NOT NULL default ' ',
  "AAABHFMF" varchar(30) NOT NULL default ' ',
  "AAABHFMG" varchar(30) NOT NULL default ' ',
  "AAABHFMH" varchar(1) NOT NULL default ' ',
  "AAABHFMI" varchar(18) NOT NULL default ' ',
  "AAABHFMJ" varchar(1) NOT NULL default ' ',
  "AAABHFMK" varchar(30) NOT NULL default ' ',
  "AAABHFML" varchar(30) NOT NULL default ' ',
  "AAABHFMM" varchar(30) NOT NULL default ' ',
  "AAABHFMN" varchar(6) NOT NULL default '000000',
  "AAABHFSY" varchar(30) NOT NULL default ' ',
  "AAABHFMP" varchar(20) NOT NULL default ' ',
  "AAABHFMQ" varchar(20) NOT NULL default ' ',
  "AAABHFMR" varchar(30) NOT NULL default ' ',
  "AAABHFMS" varchar(3) NOT NULL default ' ',
  "AAABHFMT" varchar(30) NOT NULL default ' ',
  "AAABHFMU" varchar(30) NOT NULL default ' ',
  "AAABHFMV" varchar(4) NOT NULL default ' ',
  "AAABHFMW" varchar(30) NOT NULL default ' ',
  "AAABHFMX" varchar(1) NOT NULL default ' ',
  "AAABHFMY" varchar(1) NOT NULL default ' ',
  "AAABHFMZ" varchar(1) NOT NULL default ' ',
  "AAABHFNA" varchar(1) NOT NULL default ' ',
  "AAABHFNB" varchar(1) NOT NULL default ' ',
  "AAABHFNC" varchar(1) NOT NULL default ' ',
  "AAABHFND" varchar(1) NOT NULL default ' ',
  "AAABHFNE" varchar(3) NOT NULL default ' ',
  "AAABHFNF" varchar(3) NOT NULL default ' ',
  "AAABHFNG" varchar(30) NOT NULL default ' ',
  "AAABHFNH" varchar(1) NOT NULL default ' ',
  "AAABHFNI" varchar(1) NOT NULL default ' ',
  "AAABLXJT" varchar(3) NOT NULL default ' ',
  "AAABJLRA" varchar(3) NOT NULL default '000',
  "AAABJFAA" varchar(5) NOT NULL default ' ',
  "AAABHFNM" varchar(30) NOT NULL default ' ',
  "AAABHFNN" varchar(30) NOT NULL default ' ',
  "AAABHFNO" varchar(30) NOT NULL default ' ',
  "AAABHFNP" varchar(1) NOT NULL default ' ',
  "AAABHFNQ" varchar(5) NOT NULL default ' ',
  "AAABHFNR" varchar(3) NOT NULL default ' ',
  "AAABHFNS" varchar(30) NOT NULL default ' ',
  "AAABHFNT" varchar(1) NOT NULL default ' ',
  "AAABHFNU" varchar(46) NOT NULL default ' ',
  "AAABHFNV" varchar(31) NOT NULL default ' ',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABJFLQ" varchar(8) NOT NULL default '00000000',
  "AAABHFNY" varchar(6) NOT NULL default '000000',
  "AAABMCSL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHEH","AAABIHHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDAA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCDHU" varchar(2) NOT NULL default ' ',
  "AAABCDAD" varchar(6) NOT NULL default ' ',
  "AAABCDAE" varchar(6) NOT NULL default ' ',
  "AAABCDAF" varchar(8) NOT NULL default ' ',
  "AAABCDAG" varchar(8) NOT NULL default ' ',
  "AAABKRFV" varchar(4) NOT NULL default ' ',
  "AAABIEWH" varchar(2) NOT NULL default ' ',
  "AAABECSY" varchar(1) NOT NULL default ' ',
  "AAABLKXQ" varchar(4) NOT NULL default ' ',
  "AAABCDAL" varchar(1) NOT NULL default ' ',
  "AAABCDAM" varchar(2) NOT NULL default '00',
  "AAABGOTW" varchar(2) NOT NULL default ' ',
  "AAABGOTX" varchar(2) NOT NULL default ' ',
  "AAABCDAP" varchar(2) NOT NULL default ' ',
  "AAABCDAQ" varchar(2) NOT NULL default ' ',
  "AAABKOVB" varchar(6) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABJDPM" varchar(7) NOT NULL default ' ',
  "AAABCDAU" varchar(1) NOT NULL default ' ',
  "AAABCDAV" varchar(1) NOT NULL default ' ',
  "AAABCDAW" varchar(1) NOT NULL default ' ',
  "AAABCDAX" varchar(8) NOT NULL default '00000000',
  "AAABLCNC" varchar(7) NOT NULL default ' ',
  "AAABLROG" varchar(4) NOT NULL default ' ',
  "AAABCDBA" varchar(4) NOT NULL default ' ',
  "AAABCDBB" varchar(4) NOT NULL default ' ',
  "AAABCDBC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCDHU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDBF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHATI" varchar(30) NOT NULL default ' ',
  "AAABCDBN" varchar(4) NOT NULL default ' ',
  "AAABGQUK" varchar(20) NOT NULL default ' ',
  "AAABFUCP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHATI","AAABCDBN","AAABGQUK","AAABFUCP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDBQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHATI" varchar(30) NOT NULL default ' ',
  "AAABGQUK" varchar(20) NOT NULL default ' ',
  "AAABFUCP" varchar(20) NOT NULL default ' ',
  "AAABCDCB" varchar(4) NOT NULL default ' ',
  "AAABCDBW" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABHATI","AAABGQUK","AAABFUCP","AAABCDCB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDCC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCDHU" varchar(2) NOT NULL default ' ',
  "AAABCDCJ" varchar(5) NOT NULL default ' ',
  "AAABDIIT" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCDHU","AAABCDCJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDCK" (
  "AAABMMKX" varchar(3) NOT NULL default '000',
  "AAABCDHU" varchar(2) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAABCDDE" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMKX","AAABCDHU","AAABLZNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDCS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCDHU" varchar(2) NOT NULL default ' ',
  "AAABCDCW" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCDHU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDDA" (
  "AAABMMKX" varchar(3) NOT NULL default '000',
  "AAABCDHU" varchar(2) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAABCDDE" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMKX","AAABCDHU","AAABLZNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDDI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCDHU" varchar(2) NOT NULL default ' ',
  "AAABCDDP" varchar(2) NOT NULL default '00',
  "AAABCDDM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCDHU","AAABCDDP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDDQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRFV" varchar(4) NOT NULL default ' ',
  "AAABFUWK" varchar(3) NOT NULL default ' ',
  "AAABCDGP" varchar(1) NOT NULL default ' ',
  "AAABCDDW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDBB","AAABKRFV","AAABFUWK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDEB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCDEO" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCDEF" varchar(1) NOT NULL default ' ',
  "AAABCDEG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCDEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDEJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCDEO" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCDEO","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKOVG" varchar(2) NOT NULL default ' ',
  "AAABCDET" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKOVG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDEW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKOVG" varchar(2) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABCDJB" decimal(15,2) NOT NULL default '0.00',
  "AAABCDFC" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABKOVG","AAABMCRJ","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDFH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKOVG" varchar(2) NOT NULL default ' ',
  "AAABECLU" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKOVG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDFP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEMCA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEMCA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDFU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEMCA" varchar(4) NOT NULL default ' ',
  "AAABFMTF" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABEMCA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDGC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCDGP" varchar(1) NOT NULL default ' ',
  "AAABCDGF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCDGP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDGI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCDGP" varchar(1) NOT NULL default ' ',
  "AAABCDGM" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCDGP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDGQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCDHF" varchar(1) NOT NULL default ' ',
  "AAABLCNF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCDHF","AAABLCNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDGX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCDHF" varchar(1) NOT NULL default ' ',
  "AAABLCNF" varchar(10) NOT NULL default ' ',
  "AAABEPBY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCDHF","AAABLCNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDHH" (
  "AAABIHEH" varchar(4) NOT NULL default ' ',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABHFOD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHEH","AAABIHHB","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDHP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCDHU" varchar(2) NOT NULL default ' ',
  "AAABLGEF" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCDHU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDHV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABKOVG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTXG","AAABKOVG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDIC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABIJAF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTXG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDII" (
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABIJAF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXG","AAABIJAF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDIN" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIJAF" varchar(4) NOT NULL default ' ',
  "AAABCDIQ" varchar(5) NOT NULL default ' ',
  "AAABCPGN" varchar(10) NOT NULL default ' ',
  "AAABCPGO" varchar(15) NOT NULL default ' ',
  "AAABGRZV" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABIJAF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABKOVG" varchar(2) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABCDJB" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLTXG","AAABKOVG","AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDJG" (
  "AAABFRPG" varchar(5) NOT NULL default ' ',
  "AAABHBDC" varchar(30) NOT NULL default ' ',
  "AAABCDJJ" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABFRPG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDJL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMMXA" varchar(10) NOT NULL default ' ',
  "AAABCDJP" varchar(1) NOT NULL default ' ',
  "AAABCDJQ" varchar(1) NOT NULL default ' ',
  "AAABCDJR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMMXA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDJV" (
  "AAABLKIV" varchar(20) NOT NULL default ' ',
  "AAABMMVU" varchar(5) NOT NULL default '00000',
  "AAABHGTL" varchar(5) NOT NULL default '00000',
  "AAABEPQQ" varchar(5) NOT NULL default '00000',
  "AAABCDKH" varchar(5) NOT NULL default '00000',
  "AAABCDKI" varchar(5) NOT NULL default '00000',
  "AAABLVGV" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABLKIV","AAABMMVU","AAABHGTL","AAABEPQQ","AAABCDKH","AAABCDKI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDKJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDKN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(10) NOT NULL default ' ',
  "AAABLROG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABLROG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDKU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLECD" varchar(10) NOT NULL default ' ',
  "AAABLROG" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLECD","AAABLROG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDLE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(10) NOT NULL default ' ',
  "AAABLROG" varchar(4) NOT NULL default ' ',
  "AAABMKIQ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABLROG","AAABMKIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDLN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLECD" varchar(10) NOT NULL default ' ',
  "AAABLROG" varchar(4) NOT NULL default ' ',
  "AAABMKIQ" varchar(2) NOT NULL default '00',
  "AAABMRCK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLECD","AAABLROG","AAABMKIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDLZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(10) NOT NULL default ' ',
  "AAABLROG" varchar(4) NOT NULL default ' ',
  "AAABMKIQ" varchar(2) NOT NULL default '00',
  "AAABCDMK" varchar(2) NOT NULL default '00',
  "AAABCDMF" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABLROG","AAABMKIQ","AAABCDMK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDML" (
  "AAABLECD" varchar(10) NOT NULL default ' ',
  "AAABCDMN" varchar(2) NOT NULL default '00',
  "AAABCDMO" varchar(2) NOT NULL default '00',
  "AAABIFOV" varchar(20) NOT NULL default ' ',
  "AAABCDMQ" varchar(26) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDMT" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLECD" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDMZ" (
  "AAABLECD" varchar(10) NOT NULL default ' ',
  "AAABCDNQ" varchar(4) NOT NULL default '0000',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABGMWN" varchar(4) NOT NULL default '0000',
  "AAABCDNE" varchar(1) NOT NULL default ' ',
  "AAABCDNF" varchar(1) NOT NULL default ' ',
  "AAABCDNG" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABCDNQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDNJ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLECD" varchar(10) NOT NULL default ' ',
  "AAABCDNQ" varchar(4) NOT NULL default '0000',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLECD","AAABCDNQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDNR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCDOD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCDOD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDNW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCDOD" varchar(4) NOT NULL default ' ',
  "AAABCDOA" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCDOD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDOE" (
  "AAABFRPG" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABFRPG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDOH" (
  "AAABLLIO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDOK" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLLIO" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLLIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDOQ" (
  "AAABLLIO" varchar(2) NOT NULL default ' ',
  "AAABCDPU" varchar(2) NOT NULL default ' ',
  "AAABCDPM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLIO","AAABCDPU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDOW" (
  "AAABLLIO" varchar(2) NOT NULL default ' ',
  "AAABCDPU" varchar(2) NOT NULL default ' ',
  "AAABCDPM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLIO","AAABCDPU","AAABCDPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDPD" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLLIO" varchar(2) NOT NULL default ' ',
  "AAABCDPU" varchar(2) NOT NULL default ' ',
  "AAABCDPM" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLLIO","AAABCDPU","AAABCDPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDPN" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLLIO" varchar(2) NOT NULL default ' ',
  "AAABCDPU" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLLIO","AAABCDPU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDPV" (
  "AAABFRPG" varchar(5) NOT NULL default ' ',
  "AAABHBWQ" varchar(30) NOT NULL default ' ',
  "AAABCDPY" varchar(4) NOT NULL default '0000',
  "AAABCDPZ" varchar(3) NOT NULL default '000',
  "AAABCDQA" varchar(2) NOT NULL default '00',
  "AAABCDQB" varchar(2) NOT NULL default '00',
  "AAABCDQC" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABFRPG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDQE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKLZ" varchar(30) NOT NULL default ' ',
  "AAABKQWU" varchar(1) NOT NULL default ' ',
  "AAABKQWS" varchar(8) NOT NULL default ' ',
  "AAABKQWT" varchar(2) NOT NULL default ' ',
  "AAABKNTD" varchar(7) NOT NULL default '0000000',
  "AAABMLEL" varchar(8) NOT NULL default '00000000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLRYP" varchar(8) NOT NULL default '00000000',
  "AAABKQOS" varchar(12) NOT NULL default ' ',
  "AAABLRSP" varchar(1) NOT NULL default ' ',
  "AAABLGTZ" varchar(1) NOT NULL default ' ',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABLVJK" varchar(8) NOT NULL default '00000000',
  "AAABKMHA" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLKLZ","AAABKQWU","AAABKQWS","AAABKQWT","AAABKNTD","AAABMLEL"),
  KEY "AAABCDRD" ("AAABKQWU","AAABKQWS","AAABKQWT","AAABLRSP"),
  KEY "AAABCDRI" ("AAABMRIL","AAABKRQD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDRL" (
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABIHQV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDRP" (
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDRT" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDRZ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIHQV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDSF" (
  "AAABHATI" varchar(30) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABCDSI" varchar(30) NOT NULL default ' ',
  "AAABCDSJ" varchar(30) NOT NULL default ' ',
  "AAABCDSK" varchar(30) NOT NULL default ' ',
  "AAABCDSL" varchar(30) NOT NULL default ' ',
  "AAABCDSM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHATI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDSO" (
  "AAABHATI" varchar(30) NOT NULL default ' ',
  "AAABFUCP" varchar(20) NOT NULL default ' ',
  "AAABGQUL" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABHATI","AAABFUCP","AAABGQUL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDSV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKOVB" varchar(6) NOT NULL default ' ',
  "AAABCDSY" varchar(1) NOT NULL default ' ',
  "AAABCDUA" varchar(8) NOT NULL default ' ',
  "AAABCDUB" varchar(1) NOT NULL default ' ',
  "AAABCDTB" varchar(1) NOT NULL default ' ',
  "AAABCDTC" varchar(8) NOT NULL default ' ',
  "AAABCDTD" varchar(1) NOT NULL default ' ',
  "AAABCDTE" varchar(1) NOT NULL default ' ',
  "AAABLIXT" varchar(1) NOT NULL default ' ',
  "AAABGLFM" varchar(1) NOT NULL default ' ',
  "AAABCDTH" varchar(1) NOT NULL default ' ',
  "AAABCDTI" varchar(3) NOT NULL default ' ',
  "AAABCDTJ" varchar(1) NOT NULL default ' ',
  "AAABCDTK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKOVB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDTN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKOVB" varchar(6) NOT NULL default ' ',
  "AAABCDTR" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKOVB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKOVB" varchar(6) NOT NULL default ' ',
  "AAABFKYS" varchar(2) NOT NULL default ' ',
  "AAABFKYT" varchar(3) NOT NULL default ' ',
  "AAABCDUA" varchar(8) NOT NULL default ' ',
  "AAABCDUB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKOVB","AAABFKYS","AAABFKYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDUG" (
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
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDUW" (
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
  KEY "AAABCDWM" ("AAABJAAI"),
  KEY "AAABCDWO" ("AAABHZOK","AAABHZOR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDWR" (
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
  PRIMARY KEY ("AAABMRHK","AAABHZRB","AAABJCXS","AAABHZQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDXN" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABHZQZ" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLGEY","AAABJAAI","AAABHZQZ","AAABMRHK","AAABHZRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDYB" (
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABMREL"),
  UNIQUE KEY "AAABCDYG" ("AAABMREL","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCDYJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIDX" varchar(4) NOT NULL default ' ',
  "AAABCDYM" varchar(1) NOT NULL default ' ',
  "AAABCEBE" varchar(1) NOT NULL default ' ',
  "AAABCDYO" varchar(1) NOT NULL default ' ',
  "AAABCEBF" varchar(1) NOT NULL default ' ',
  "AAABCDYQ" varchar(1) NOT NULL default ' ',
  "AAABCDYR" varchar(2) NOT NULL default ' ',
  "AAABCDYS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLIDX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--enable_query_log
