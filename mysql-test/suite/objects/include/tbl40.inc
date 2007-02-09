# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+
# |                             tbl40.inc                              |
# |                        Copyright 2006 by MySQL AB                  |
# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+

--disable_query_log
CREATE TABLE "AAABGOMK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGOMX" varchar(2) NOT NULL default ' ',
  "AAABGOMN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGOMX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOMQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABGOMX" varchar(2) NOT NULL default ' ',
  "AAABGOMU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABGOMX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOMY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGONS" varchar(1) NOT NULL default ' ',
  "AAABGONB" varchar(1) NOT NULL default ' ',
  "AAABGONC" varchar(1) NOT NULL default ' ',
  "AAABGOND" varchar(1) NOT NULL default ' ',
  "AAABGONE" varchar(1) NOT NULL default ' ',
  "AAABGONF" varchar(1) NOT NULL default ' ',
  "AAABGONG" varchar(1) NOT NULL default ' ',
  "AAABGONH" varchar(1) NOT NULL default ' ',
  "AAABGONI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGONS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGONL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABGONS" varchar(1) NOT NULL default ' ',
  "AAABGONP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABGONS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGONT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGOPA" varchar(2) NOT NULL default ' ',
  "AAABMIZS" varchar(20) NOT NULL default ' ',
  "AAABGONX" varchar(1) NOT NULL default ' ',
  "AAABGONY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGOPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMIZS" varchar(20) NOT NULL default ' ',
  "AAABMMCU" varchar(30) NOT NULL default ' ',
  "AAABMMCV" varchar(30) NOT NULL default ' ',
  "AAABGOOG" varchar(1) NOT NULL default ' ',
  "AAABGOOH" varchar(1) NOT NULL default ' ',
  "AAABGOOI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMIZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOOL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABMIZS" varchar(20) NOT NULL default ' ',
  "AAABKZXE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABINAW","AAABMIZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOOT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABGOPA" varchar(2) NOT NULL default ' ',
  "AAABKZXE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABINAW","AAABGOPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOPB" (
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABJYKA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOPF" (
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABKZXE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABINAW","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOPL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJJLG" varchar(1) NOT NULL default ' ',
  "AAABHLPB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABJJLG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOPR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABJJLG" varchar(1) NOT NULL default ' ',
  "AAABKZXE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABINAW","AAABJJLG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOPZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMEGE" varchar(4) NOT NULL default ' ',
  "AAABGOQD" varchar(1) NOT NULL default ' ',
  "AAABLRVC" varchar(1) NOT NULL default ' ',
  "AAABKWMJ" varchar(1) NOT NULL default ' ',
  "AAABLRVD" varchar(1) NOT NULL default ' ',
  "AAABLRVE" varchar(1) NOT NULL default ' ',
  "AAABGOQI" varchar(1) NOT NULL default ' ',
  "AAABGOQJ" varchar(1) NOT NULL default ' ',
  "AAABGOQK" varchar(1) NOT NULL default ' ',
  "AAABGOQL" varchar(1) NOT NULL default ' ',
  "AAABGOQM" varchar(1) NOT NULL default ' ',
  "AAABGOQN" varchar(1) NOT NULL default ' ',
  "AAABGOQO" varchar(1) NOT NULL default ' ',
  "AAABGOQP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP","AAABMEGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOQT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMEGE" varchar(4) NOT NULL default ' ',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABKZXE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP","AAABMEGE","AAABINAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGORD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGPNG" varchar(3) NOT NULL default ' ',
  "AAABGORG" varchar(1) NOT NULL default ' ',
  "AAABGORH" varchar(1) NOT NULL default ' ',
  "AAABGORI" varchar(1) NOT NULL default ' ',
  "AAABGORJ" varchar(1) NOT NULL default ' ',
  "AAABGORK" varchar(1) NOT NULL default ' ',
  "AAABGORL" varchar(1) NOT NULL default ' ',
  "AAABGORM" varchar(1) NOT NULL default ' ',
  "AAABGORN" varchar(1) NOT NULL default ' ',
  "AAABGORO" varchar(1) NOT NULL default ' ',
  "AAABGORP" varchar(1) NOT NULL default ' ',
  "AAABGORQ" varchar(4) NOT NULL default '0000',
  "AAABGORR" varchar(1) NOT NULL default ' ',
  "AAABGPNH" varchar(8) NOT NULL default ' ',
  "AAABGPNI" varchar(4) NOT NULL default ' ',
  "AAABGORU" varchar(8) NOT NULL default ' ',
  "AAABGORV" varchar(4) NOT NULL default ' ',
  "AAABGORW" varchar(8) NOT NULL default ' ',
  "AAABGORX" varchar(4) NOT NULL default ' ',
  "AAABGORY" varchar(30) NOT NULL default ' ',
  "AAABGORZ" varchar(1) NOT NULL default ' ',
  "AAABGOSA" varchar(30) NOT NULL default ' ',
  "AAABGOSB" varchar(1) NOT NULL default ' ',
  "AAABGOSC" varchar(30) NOT NULL default ' ',
  "AAABGOSD" varchar(1) NOT NULL default ' ',
  "AAABGOSE" varchar(30) NOT NULL default ' ',
  "AAABGOSF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGPNG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOSI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABGPNG" varchar(3) NOT NULL default ' ',
  "AAABKZXE" varchar(40) NOT NULL default ' ',
  "AAABGOSN" varchar(20) NOT NULL default ' ',
  "AAABGOSO" varchar(20) NOT NULL default ' ',
  "AAABGOSP" varchar(20) NOT NULL default ' ',
  "AAABGOSQ" varchar(20) NOT NULL default ' ',
  "AAABGOSR" varchar(20) NOT NULL default ' ',
  "AAABGOSS" varchar(20) NOT NULL default ' ',
  "AAABGOST" varchar(20) NOT NULL default ' ',
  "AAABGOSU" varchar(20) NOT NULL default ' ',
  "AAABGOSV" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABINAW","AAABGPNG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOSZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGPLQ" varchar(2) NOT NULL default ' ',
  "AAABKXRE" varchar(2) NOT NULL default ' ',
  "AAABGPDC" varchar(9) NOT NULL default ' ',
  "AAABGOTE" varchar(2) NOT NULL default ' ',
  "AAABGOTF" varchar(1) NOT NULL default ' ',
  "AAABGOTG" varchar(2) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABGOTI" varchar(4) NOT NULL default ' ',
  "AAABGOTJ" varchar(4) NOT NULL default ' ',
  "AAABGOTK" varchar(4) NOT NULL default ' ',
  "AAABLKXQ" varchar(4) NOT NULL default ' ',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABGOTN" varchar(8) NOT NULL default ' ',
  "AAABKNPA" varchar(2) NOT NULL default ' ',
  "AAABKMOY" varchar(4) NOT NULL default ' ',
  "AAABGOTQ" varchar(4) NOT NULL default ' ',
  "AAABGOTR" varchar(2) NOT NULL default ' ',
  "AAABGOTS" varchar(2) NOT NULL default ' ',
  "AAABGOTT" varchar(2) NOT NULL default ' ',
  "AAABGOTU" varchar(2) NOT NULL default ' ',
  "AAABGOTV" varchar(2) NOT NULL default ' ',
  "AAABGOTW" varchar(2) NOT NULL default ' ',
  "AAABGOTX" varchar(2) NOT NULL default ' ',
  "AAABGOTY" varchar(2) NOT NULL default ' ',
  "AAABGPFI" varchar(1) NOT NULL default ' ',
  "AAABGOWT" varchar(10) NOT NULL default ' ',
  "AAABGOUB" varchar(10) NOT NULL default ' ',
  "AAABGOUC" varchar(10) NOT NULL default ' ',
  "AAABGPFD" varchar(1) NOT NULL default ' ',
  "AAABGPFE" varchar(1) NOT NULL default ' ',
  "AAABGPFF" varchar(1) NOT NULL default ' ',
  "AAABGPFG" varchar(1) NOT NULL default ' ',
  "AAABGPFH" varchar(1) NOT NULL default ' ',
  "AAABGOUI" varchar(1) NOT NULL default ' ',
  "AAABLXSA" varchar(6) NOT NULL default ' ',
  "AAABGOUK" varchar(5) NOT NULL default ' ',
  "AAABGOUL" decimal(11,2) NOT NULL default '0.00',
  "AAABGOUM" varchar(30) NOT NULL default ' ',
  "AAABGOUN" varchar(4) NOT NULL default ' ',
  "AAABGOUO" varchar(2) NOT NULL default ' ',
  "AAABGOUP" varchar(1) NOT NULL default ' ',
  "AAABGOUQ" varchar(4) NOT NULL default ' ',
  "AAABGOUR" varchar(4) NOT NULL default ' ',
  "AAABGOUS" varchar(4) NOT NULL default ' ',
  "AAABGOUT" varchar(4) NOT NULL default ' ',
  "AAABGOUU" varchar(4) NOT NULL default ' ',
  "AAABGOUV" varchar(1) NOT NULL default ' ',
  "AAABGOUW" varchar(1) NOT NULL default ' ',
  "AAABGOUX" varchar(4) NOT NULL default ' ',
  "AAABGOUY" varchar(1) NOT NULL default ' ',
  "AAABGOUZ" varchar(1) NOT NULL default ' ',
  "AAABGOVA" varchar(1) NOT NULL default ' ',
  "AAABGOVB" varchar(1) NOT NULL default ' ',
  "AAABGOVC" varchar(1) NOT NULL default ' ',
  "AAABGOVD" varchar(1) NOT NULL default ' ',
  "AAABGOVE" varchar(1) NOT NULL default ' ',
  "AAABIZDJ" varchar(1) NOT NULL default ' ',
  "AAABGOVG" varchar(30) NOT NULL default ' ',
  "AAABGOVH" varchar(30) NOT NULL default ' ',
  "AAABJPEL" varchar(1) NOT NULL default ' ',
  "AAABKWRH" varchar(4) NOT NULL default '0000',
  "AAABLZNN" varchar(10) NOT NULL default ' ',
  "AAABLBDX" varchar(70) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAABGOVN" varchar(1) NOT NULL default ' ',
  "AAABHKKY" varchar(2) NOT NULL default ' ',
  "AAABGOVP" varchar(2) NOT NULL default ' ',
  "AAABGPFO" varchar(30) NOT NULL default ' ',
  "AAABGOVR" varchar(12) NOT NULL default ' ',
  "AAABGOVS" varchar(1) NOT NULL default ' ',
  "AAABGOVT" varchar(2) NOT NULL default ' ',
  "AAABGOVU" varchar(12) NOT NULL default ' ',
  "AAABGOVV" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGPLQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOVY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGOWT" varchar(10) NOT NULL default ' ',
  "AAABGOWB" varchar(1) NOT NULL default ' ',
  "AAABLUUU" varchar(1) NOT NULL default ' ',
  "AAABLOZE" varchar(1) NOT NULL default ' ',
  "AAABGOWE" decimal(9,0) NOT NULL default '0',
  "AAABGOWF" decimal(9,0) NOT NULL default '0',
  "AAABGOWG" decimal(9,0) NOT NULL default '0',
  "AAABGOWH" decimal(9,0) NOT NULL default '0',
  "AAABGOWI" decimal(9,0) NOT NULL default '0',
  "AAABGOWJ" decimal(9,0) NOT NULL default '0',
  "AAABGOWK" varchar(4) NOT NULL default '0000',
  "AAABGOWL" varchar(1) NOT NULL default ' ',
  "AAABGOWM" varchar(1) NOT NULL default ' ',
  "AAABGOWN" varchar(1) NOT NULL default ' ',
  "AAABGOWO" varchar(4) NOT NULL default '0000',
  "AAABLTUX" varchar(1) NOT NULL default ' ',
  "AAABGOWQ" varchar(1) NOT NULL default ' ',
  "AAABGOWR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGOWT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOWU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGPLQ" varchar(2) NOT NULL default ' ',
  "AAABGOWY" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGPLQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOXC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGOYB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGOYB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOXH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGOXT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGOXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOXM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGOXT" varchar(4) NOT NULL default ' ',
  "AAABGOXQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGOXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOXU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGOYB" varchar(4) NOT NULL default ' ',
  "AAABGOXY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGOYB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOYC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGOYH" varchar(2) NOT NULL default ' ',
  "AAABGOYI" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGOYH","AAABGOYI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOYJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGOYQ" varchar(1) NOT NULL default ' ',
  "AAABGOYR" varchar(8) NOT NULL default ' ',
  "AAABGOYS" varchar(4) NOT NULL default ' ',
  "AAABGOYT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGOYQ","AAABGOYR","AAABGOYS","AAABGOYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOYU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGPDB" varchar(4) NOT NULL default ' ',
  "AAABGOYX" varchar(1) NOT NULL default ' ',
  "AAABGOYY" varchar(1) NOT NULL default ' ',
  "AAABGOYZ" varchar(1) NOT NULL default ' ',
  "AAABGOZA" varchar(1) NOT NULL default ' ',
  "AAABGOZB" varchar(1) NOT NULL default ' ',
  "AAABGOZC" varchar(1) NOT NULL default ' ',
  "AAABGOZD" varchar(1) NOT NULL default ' ',
  "AAABGOZE" varchar(1) NOT NULL default ' ',
  "AAABGOZF" varchar(1) NOT NULL default ' ',
  "AAABGOZG" varchar(1) NOT NULL default ' ',
  "AAABGOZH" varchar(1) NOT NULL default ' ',
  "AAABGOZI" varchar(1) NOT NULL default ' ',
  "AAABGOZJ" varchar(1) NOT NULL default ' ',
  "AAABGOZK" varchar(1) NOT NULL default ' ',
  "AAABGOZL" varchar(1) NOT NULL default ' ',
  "AAABGOZM" varchar(1) NOT NULL default ' ',
  "AAABGOZN" varchar(1) NOT NULL default ' ',
  "AAABGOZO" varchar(1) NOT NULL default ' ',
  "AAABGOZP" varchar(1) NOT NULL default ' ',
  "AAABGOZQ" varchar(1) NOT NULL default ' ',
  "AAABGOZR" varchar(1) NOT NULL default ' ',
  "AAABGOZS" varchar(1) NOT NULL default ' ',
  "AAABGOZT" varchar(1) NOT NULL default ' ',
  "AAABGOZU" varchar(1) NOT NULL default ' ',
  "AAABGOZV" varchar(1) NOT NULL default ' ',
  "AAABGOZW" varchar(1) NOT NULL default ' ',
  "AAABGOZX" varchar(1) NOT NULL default ' ',
  "AAABGOZY" varchar(1) NOT NULL default ' ',
  "AAABGOZZ" varchar(1) NOT NULL default ' ',
  "AAABGPAA" varchar(1) NOT NULL default ' ',
  "AAABGPAB" varchar(1) NOT NULL default ' ',
  "AAABGPAC" varchar(1) NOT NULL default ' ',
  "AAABGPAD" varchar(1) NOT NULL default ' ',
  "AAABGPAE" varchar(1) NOT NULL default ' ',
  "AAABGPAF" varchar(1) NOT NULL default ' ',
  "AAABGPAG" varchar(1) NOT NULL default ' ',
  "AAABGPAH" varchar(1) NOT NULL default ' ',
  "AAABGPAI" varchar(1) NOT NULL default ' ',
  "AAABGPAJ" varchar(1) NOT NULL default ' ',
  "AAABGPAK" varchar(1) NOT NULL default ' ',
  "AAABGPAL" varchar(1) NOT NULL default ' ',
  "AAABGPAM" varchar(1) NOT NULL default ' ',
  "AAABGPAN" varchar(1) NOT NULL default ' ',
  "AAABGPAO" varchar(1) NOT NULL default ' ',
  "AAABGPAP" varchar(1) NOT NULL default ' ',
  "AAABGPAQ" varchar(1) NOT NULL default ' ',
  "AAABGPAR" varchar(1) NOT NULL default ' ',
  "AAABGPAS" varchar(1) NOT NULL default ' ',
  "AAABGPAT" varchar(1) NOT NULL default ' ',
  "AAABGPAU" varchar(1) NOT NULL default ' ',
  "AAABGPAV" varchar(1) NOT NULL default ' ',
  "AAABGPAW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGPDB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPAZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGPDB" varchar(4) NOT NULL default ' ',
  "AAABKZXE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGPDB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPBH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGPLQ" varchar(2) NOT NULL default ' ',
  "AAABHHXC" varchar(4) NOT NULL default ' ',
  "AAABKYVC" varchar(4) NOT NULL default ' ',
  "AAABHLPB" varchar(4) NOT NULL default '0000',
  "AAABMDBL" varchar(2) NOT NULL default ' ',
  "AAABMNUA" varchar(30) NOT NULL default ' ',
  "AAABGPBP" varchar(14) NOT NULL default ' ',
  "AAABGPBQ" varchar(30) NOT NULL default ' ',
  "AAABGPBR" varchar(1) NOT NULL default ' ',
  "AAABGPBS" varchar(1) NOT NULL default ' ',
  "AAABGPBT" varchar(1) NOT NULL default ' ',
  "AAABGPKR" varchar(30) NOT NULL default ' ',
  "AAABGPBV" varchar(1) NOT NULL default ' ',
  "AAABGPBW" varchar(8) NOT NULL default ' ',
  "AAABGPBX" varchar(4) NOT NULL default ' ',
  "AAABMQYE" varchar(4) NOT NULL default ' ',
  "AAABGPBZ" varchar(1) NOT NULL default ' ',
  "AAABGPCA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGPLQ","AAABHHXC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPCE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGPLQ" varchar(2) NOT NULL default ' ',
  "AAABHHXC" varchar(4) NOT NULL default ' ',
  "AAABGPCM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGPLQ","AAABHHXC","AAABGPCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPCN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGPLQ" varchar(2) NOT NULL default ' ',
  "AAABHHXC" varchar(4) NOT NULL default ' ',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABGPCS" varchar(30) NOT NULL default ' ',
  "AAABIFON" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGPLQ","AAABHHXC","AAABINAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPCY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGPEB" varchar(8) NOT NULL default ' ',
  "AAABGPDB" varchar(4) NOT NULL default ' ',
  "AAABGPDC" varchar(9) NOT NULL default ' ',
  "AAABGPDD" varchar(1) NOT NULL default ' ',
  "AAABGPDE" varchar(1) NOT NULL default ' ',
  "AAABGPDF" varchar(1) NOT NULL default ' ',
  "AAABGPDG" varchar(1) NOT NULL default ' ',
  "AAABGPDH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGPEB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPDK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLFLE" varchar(4) NOT NULL default ' ',
  "AAABHHST" varchar(8) NOT NULL default ' ',
  "AAABGPEB" varchar(8) NOT NULL default ' ',
  "AAABGPDP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLFLE","AAABHHST","AAABGPEB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPDU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGPEB" varchar(8) NOT NULL default ' ',
  "AAABKZXE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGPEB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPEC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGPLQ" varchar(2) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLXSX" varchar(8) NOT NULL default '00000000',
  "AAABMCXJ" varchar(12) NOT NULL default ' ',
  "AAABMCXL" varchar(8) NOT NULL default '00000000',
  "AAABKROQ" varchar(10) NOT NULL default ' ',
  "AAABKROR" varchar(6) NOT NULL default '000000',
  "AAABMCXE" varchar(8) NOT NULL default ' ',
  "AAABMCXI" varchar(12) NOT NULL default ' ',
  "AAABMCXN" varchar(10) NOT NULL default '0000000000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABGPEV" varchar(4) NOT NULL default ' ',
  "AAABGPEW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGPLQ","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPFA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKXRE" varchar(2) NOT NULL default ' ',
  "AAABGPFD" varchar(1) NOT NULL default ' ',
  "AAABGPFE" varchar(1) NOT NULL default ' ',
  "AAABGPFF" varchar(1) NOT NULL default ' ',
  "AAABGPFG" varchar(1) NOT NULL default ' ',
  "AAABGPFH" varchar(1) NOT NULL default ' ',
  "AAABGPFI" varchar(1) NOT NULL default ' ',
  "AAABKNPA" varchar(2) NOT NULL default ' ',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABGPFM" varchar(3) NOT NULL default ' ',
  "AAABGPFN" varchar(3) NOT NULL default ' ',
  "AAABGPFO" varchar(30) NOT NULL default ' ',
  "AAABGPFP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKXRE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPFS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKXRE" varchar(2) NOT NULL default ' ',
  "AAABGPFW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKXRE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPGA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGPGZ" varchar(6) NOT NULL default ' ',
  "AAABGPGD" varchar(1) NOT NULL default ' ',
  "AAABGPGE" varchar(1) NOT NULL default ' ',
  "AAABGPGF" varchar(1) NOT NULL default ' ',
  "AAABGPGG" varchar(1) NOT NULL default ' ',
  "AAABGPGH" varchar(1) NOT NULL default ' ',
  "AAABGPGI" varchar(1) NOT NULL default ' ',
  "AAABGPGJ" varchar(1) NOT NULL default ' ',
  "AAABGPGK" varchar(1) NOT NULL default ' ',
  "AAABGPGL" varchar(1) NOT NULL default ' ',
  "AAABGPGM" varchar(1) NOT NULL default ' ',
  "AAABGPGN" varchar(1) NOT NULL default ' ',
  "AAABGPGO" varchar(2) NOT NULL default ' ',
  "AAABGPGP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGPGZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPGS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABGPGZ" varchar(6) NOT NULL default ' ',
  "AAABGPGW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABGPGZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPHA" (
  "AAABGPHE" varchar(2) NOT NULL default '00',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  "AAABGPHD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABGPHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPHF" (
  "AAABGPHI" varchar(5) NOT NULL default ' ',
  "AAABGPHH" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABGPHI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPHJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDAN" varchar(8) NOT NULL default ' ',
  "AAABLFLE" varchar(4) NOT NULL default ' ',
  "AAABGPHN" varchar(1) NOT NULL default ' ',
  "AAABGPHO" varchar(1) NOT NULL default ' ',
  "AAABGPHP" varchar(1) NOT NULL default ' ',
  "AAABGPHQ" varchar(2) NOT NULL default ' ',
  "AAABGPHR" varchar(2) NOT NULL default ' ',
  "AAABLMVF" varchar(3) NOT NULL default ' ',
  "AAABGPHT" varchar(3) NOT NULL default ' ',
  "AAABGPHU" varchar(4) NOT NULL default '0000',
  "AAABGPHV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDAN","AAABLFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPHZ" (
  "AAABGPJN" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABGPJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPIC" (
  "AAABGPJN" varchar(3) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABGPJN","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPII" (
  "AAABGPJN" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABGPJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPIL" (
  "AAABGPJN" varchar(3) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABGPJN","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPIR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABGPJB" varchar(1) NOT NULL default ' ',
  "AAABGPIV" varchar(1) NOT NULL default ' ',
  "AAABGPIW" varchar(1) NOT NULL default ' ',
  "AAABGPIX" varchar(1) NOT NULL default ' ',
  "AAABGPIY" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMCFC","AAABGPJB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPJC" (
  "AAABJSCJ" varchar(2) NOT NULL default ' ',
  "AAABLYAG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABJSCJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPJG" (
  "AAABGPJN" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABGPJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPJJ" (
  "AAABGPJN" varchar(3) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABGPJN","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPJP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLFLE" varchar(4) NOT NULL default ' ',
  "AAABGPJS" varchar(4) NOT NULL default '0000',
  "AAABGPJT" varchar(4) NOT NULL default '0000',
  "AAABGPJU" varchar(4) NOT NULL default '0000',
  "AAABGPJV" varchar(4) NOT NULL default '0000',
  "AAABGPJW" varchar(4) NOT NULL default '0000',
  "AAABGPJX" varchar(4) NOT NULL default '0000',
  "AAABLWIE" varchar(4) NOT NULL default '0000',
  "AAABGPJZ" varchar(4) NOT NULL default '0000',
  "AAABGPKA" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABLFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPKD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLFLE" varchar(4) NOT NULL default ' ',
  "AAABGPKK" varchar(1) NOT NULL default ' ',
  "AAABGPKH" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABLFLE","AAABGPKK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPKL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGPLQ" varchar(2) NOT NULL default ' ',
  "AAABGPQT" varchar(20) NOT NULL default ' ',
  "AAABMDBL" varchar(1) NOT NULL default ' ',
  "AAABKXRE" varchar(2) NOT NULL default ' ',
  "AAABGPKR" varchar(30) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABGPKT" varchar(3) NOT NULL default ' ',
  "AAABGPKU" varchar(3) NOT NULL default ' ',
  "AAABGPKV" varchar(3) NOT NULL default ' ',
  "AAABGPKW" varchar(3) NOT NULL default ' ',
  "AAABGPKX" varchar(3) NOT NULL default ' ',
  "AAABGPKY" varchar(2) NOT NULL default '00',
  "AAABGPKZ" varchar(4) NOT NULL default ' ',
  "AAABGPLA" varchar(4) NOT NULL default ' ',
  "AAABGPLB" varchar(4) NOT NULL default ' ',
  "AAABGPLC" varchar(4) NOT NULL default ' ',
  "AAABGPLD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGPLQ","AAABGPQT","AAABMDBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPLI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGPLQ" varchar(2) NOT NULL default ' ',
  "AAABGPLR" varchar(20) NOT NULL default ' ',
  "AAABMDBL" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGPLQ","AAABGPLR","AAABMDBL","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPLU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGPLW" varchar(1) NOT NULL default ' ',
  "AAABGPLX" decimal(3,0) NOT NULL default '0',
  "AAABGPLY" decimal(3,0) NOT NULL default '0',
  "AAABGPLZ" varchar(1) NOT NULL default ' ',
  "AAABGPMA" varchar(1) NOT NULL default ' ',
  "AAABGPMB" varchar(1) NOT NULL default ' ',
  "AAABGPMC" varchar(1) NOT NULL default ' ',
  "AAABGPMD" varchar(1) NOT NULL default ' ',
  "AAABGPME" varchar(4) NOT NULL default '0000',
  "AAABGPMF" decimal(3,0) NOT NULL default '0',
  "AAABGPMG" varchar(30) NOT NULL default ' ',
  "AAABGPMH" varchar(4) NOT NULL default '0000',
  "AAABGPMI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPMK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABGPMN" varchar(2) NOT NULL default ' ',
  "AAABLQHF" varchar(2) NOT NULL default ' ',
  "AAABGPMP" varchar(1) NOT NULL default ' ',
  "AAABGPMQ" varchar(1) NOT NULL default ' ',
  "AAABGPMR" varchar(1) NOT NULL default ' ',
  "AAABGPMS" decimal(5,2) NOT NULL default '0.00',
  "AAABGPMT" varchar(1) NOT NULL default ' ',
  "AAABGPMU" tinyint(3) unsigned NOT NULL default '0',
  "AAABGPMV" tinyint(3) unsigned NOT NULL default '0',
  "AAABGPMW" varchar(1) NOT NULL default ' ',
  "AAABGPMX" varchar(1) NOT NULL default ' ',
  "AAABGPMY" varchar(4) NOT NULL default ' ',
  "AAABGPMZ" varchar(10) NOT NULL default ' ',
  "AAABGPNA" varchar(10) NOT NULL default ' ',
  "AAABGPNB" varchar(4) NOT NULL default ' ',
  "AAABGPNC" varchar(6) NOT NULL default '000000',
  "AAABGPND" varchar(6) NOT NULL default '000000',
  "AAABGPNE" smallint(6) NOT NULL default '0',
  "AAABGPNF" varchar(1) NOT NULL default ' ',
  "AAABGPNG" varchar(3) NOT NULL default ' ',
  "AAABGPNH" varchar(8) NOT NULL default ' ',
  "AAABGPNI" varchar(4) NOT NULL default ' ',
  "AAABGPNJ" varchar(8) NOT NULL default ' ',
  "AAABGPNK" varchar(4) NOT NULL default ' ',
  "AAABGPNL" varchar(1) NOT NULL default ' ',
  "AAABGPNM" varchar(8) NOT NULL default ' ',
  "AAABGPNN" varchar(8) NOT NULL default ' ',
  "AAABGPNO" varchar(8) NOT NULL default ' ',
  "AAABGPNP" varchar(8) NOT NULL default ' ',
  "AAABGPNQ" varchar(1) NOT NULL default ' ',
  "AAABGPNR" varchar(4) NOT NULL default ' ',
  "AAABGPNS" varchar(4) NOT NULL default ' ',
  "AAABGPNT" varchar(4) NOT NULL default '0000',
  "AAABGPNU" varchar(4) NOT NULL default '0000',
  "AAABGPNV" varchar(4) NOT NULL default '0000',
  "AAABGPNW" varchar(1) NOT NULL default ' ',
  "AAABGPNX" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPOA" (
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default '0000',
  "AAABJHGJ" varchar(1) NOT NULL default ' ',
  "AAABJHGK" varchar(1) NOT NULL default ' ',
  "AAABGPOF" decimal(2,0) NOT NULL default '0',
  "AAABGPOG" decimal(2,0) NOT NULL default '0',
  "AAABGPOH" decimal(2,0) NOT NULL default '0',
  "AAABGPOI" decimal(2,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMNLV","AAABMGHE","AAABJHGJ","AAABJHGK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPON" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGPPO" varchar(15) NOT NULL default ' ',
  "AAABIJSB" int(11) NOT NULL default '0',
  "AAABGPOR" varchar(60) NOT NULL default ' ',
  "AAABGPOS" varchar(60) NOT NULL default ' ',
  "AAABGPOT" varchar(15) NOT NULL default ' ',
  "AAABGPOU" varchar(15) NOT NULL default ' ',
  "AAABGPOV" varchar(1) NOT NULL default ' ',
  "AAABGPOW" varchar(1) NOT NULL default ' ',
  "AAABGPOX" varchar(1) NOT NULL default ' ',
  "AAABGPOY" varchar(1) NOT NULL default ' ',
  "AAABGPOZ" varchar(1) NOT NULL default ' ',
  "AAABGPPA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABGPPO","AAABIJSB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPPE" (
  "AAABHMLH" varchar(15) NOT NULL default ' ',
  "AAABGPPM" int(11) NOT NULL default '0',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABGPPO" varchar(15) NOT NULL default ' ',
  "AAABIJSB" int(11) NOT NULL default '0',
  "AAABGPPK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABHMLH","AAABGPPM","AAABMRFQ","AAABGPPO","AAABIJSB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPPQ" (
  "AAABKXRE" varchar(2) NOT NULL default ' ',
  "AAABIJSB" varchar(3) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABKXRE","AAABIJSB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPPX" (
  "AAABKXRE" varchar(2) NOT NULL default ' ',
  "AAABIJSB" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABKXRE","AAABIJSB","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPQF" (
  "AAABKXRE" varchar(2) NOT NULL default ' ',
  "AAABGPQT" varchar(20) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABGPQJ" varchar(50) NOT NULL default ' ',
  "AAABGPQK" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABKXRE","AAABGPQT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPQN" (
  "AAABKXRE" varchar(2) NOT NULL default ' ',
  "AAABGPQT" varchar(20) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABKXRE","AAABGPQT","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPQV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGQNX" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGQNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPRA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGQNX" varchar(12) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGPRE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGQNX","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPRI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABLAXF" varchar(8) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABGVKO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCPX","AAABLAXF","AAABJGWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPRS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGQQI" varchar(4) NOT NULL default ' ',
  "AAABGQUU" varchar(16) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABGQNX" varchar(12) NOT NULL default ' ',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABLPPS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGQQI","AAABGQUU","AAABJGWB","AAABGQNX","AAABLPQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPSG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGQNX" varchar(12) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWD","AAABGQNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPSP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABGPST" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKZWL","AAABJGWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPSX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGPTL" varchar(2) NOT NULL default '00',
  "AAABIYDX" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGPTL","AAABIYDX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPTE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGPTL" varchar(2) NOT NULL default '00',
  "AAABIYDX" varchar(8) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGPTL","AAABIYDX","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPTO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABLAXF" varchar(8) NOT NULL default ' ',
  "AAABGQNX" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLCPX","AAABLAXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPTY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGQUU" varchar(16) NOT NULL default ' ',
  "AAABGQQI" varchar(4) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABGQNX" varchar(12) NOT NULL default ' ',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABGQSJ" varchar(5) NOT NULL default ' ',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  "AAABLADC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGQUU","AAABGQQI","AAABJGWB","AAABGQNX","AAABLPQY","AAABGQSJ","AAABKZWL","AAABLADC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPUR" (
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABGPUW" varchar(10) NOT NULL default ' ',
  "AAABGPUU" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABLZKF","AAABGPUW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPUX" (
  "AAABGRUN" varchar(10) NOT NULL default ' ',
  "AAABKIZK" varchar(10) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  "AAABGPVE" varchar(2) NOT NULL default ' ',
  "AAABLEQO" varchar(6) NOT NULL default '000000',
  "AAABLEQN" varchar(8) NOT NULL default '00000000',
  "AAABGPVH" int(11) NOT NULL default '0',
  "AAABILHK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABGRUN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPVK" (
  "AAABGRUN" varchar(10) NOT NULL default ' ',
  "AAABHFAO" varchar(2) NOT NULL default ' ',
  "AAABMKFH" varchar(70) NOT NULL default ' ',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABGRUN","AAABHFAO","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPVT" (
  "AAABKIZK" varchar(10) NOT NULL default ' ',
  "AAABGPVV" varchar(60) NOT NULL default ' ',
  "AAABGPVW" varchar(1) NOT NULL default ' ',
  "AAABGPVX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKIZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPVZ" (
  "AAABGRUN" varchar(10) NOT NULL default ' ',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  "AAABILHK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABGRUN","AAABMNJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPWG" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQCG" varchar(18) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMQHQ" varchar(3) NOT NULL default ' ',
  "AAABMQHR" varchar(1) NOT NULL default ' ',
  "AAABMPVB" varchar(3) NOT NULL default '000',
  "AAABMQCL" varchar(1) NOT NULL default ' ',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQCO" varchar(6) NOT NULL default ' ',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMPVO" decimal(17,2) NOT NULL default '0.00',
  "AAABMPVP" decimal(17,2) NOT NULL default '0.00',
  "AAABMPVQ" decimal(17,2) NOT NULL default '0.00',
  "AAABMPVR" decimal(15,3) NOT NULL default '0.000',
  "AAABMPVS" decimal(15,3) NOT NULL default '0.000',
  "AAABMQHE" varchar(3) NOT NULL default ' ',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABMQBS" varchar(4) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMQBU" varchar(8) NOT NULL default '00000000',
  "AAABMQCV" varchar(10) NOT NULL default ' ',
  "AAABMQCW" varchar(4) NOT NULL default '0000',
  "AAABMQDA" varchar(6) NOT NULL default '000000',
  "AAABMQCX" varchar(1) NOT NULL default ' ',
  "AAABMQBZ" varchar(4) NOT NULL default ' ',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABMQCE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMQCG"),
  UNIQUE KEY "AAABGPXU" ("AAABMQHU","AAABMQCJ","AAABMQHY","AAABMQCL","AAABMQDO","AAABMQHV","AAABMQCO"),
  KEY "AAABGPYC" ("AAABMQHV","AAABMQCV","AAABMQCD","AAABMQCE"),
  KEY "AAABGPYH" ("AAABMQHV","AAABMQDO","AAABMQHY","AAABMQHZ","AAABMPVB"),
  KEY "AAABGPYN" ("AAABMQHU","AAABMQCV","AAABMQCW","AAABMQCX","AAABMQDO","AAABMQHV","AAABMQDA"),
  KEY "AAABGPYV" ("AAABMQHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPYX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(18) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG"),
  UNIQUE KEY "AAABGPZD" ("AAABMRIL","AAABMQDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPZG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(18) NOT NULL default ' ',
  "AAABGPZJ" varchar(8) NOT NULL default '00000000',
  "AAABLGWN" varchar(8) NOT NULL default '00000000',
  "AAABGPZL" varchar(8) NOT NULL default '00000000',
  "AAABLGWT" varchar(8) NOT NULL default '00000000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABGPZS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG"),
  UNIQUE KEY "AAABGPZR" ("AAABGPZS","AAABMREX","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGPZV" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQCG" varchar(18) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMQHQ" varchar(3) NOT NULL default ' ',
  "AAABMQHR" varchar(1) NOT NULL default ' ',
  "AAABMQHS" varchar(3) NOT NULL default '000',
  "AAABMQCL" varchar(1) NOT NULL default ' ',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQCO" varchar(6) NOT NULL default ' ',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMQDX" decimal(17,2) NOT NULL default '0.00',
  "AAABMQDY" decimal(17,2) NOT NULL default '0.00',
  "AAABMQDZ" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEA" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEB" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEC" decimal(17,2) NOT NULL default '0.00',
  "AAABMQED" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEE" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEF" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEG" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEH" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEI" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEJ" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEK" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEL" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEM" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEN" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEO" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEP" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEQ" decimal(17,2) NOT NULL default '0.00',
  "AAABMQER" decimal(17,2) NOT NULL default '0.00',
  "AAABMQES" decimal(17,2) NOT NULL default '0.00',
  "AAABMQET" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEU" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEV" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEW" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEX" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEY" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEZ" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFA" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFB" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFC" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFD" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFE" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFF" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFG" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFH" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFI" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFJ" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFK" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFL" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFM" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFN" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFO" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFP" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFQ" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFR" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFS" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFT" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFU" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFV" decimal(17,2) NOT NULL default '0.00',
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
  "AAABMQGN" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGO" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGP" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGQ" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGR" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGS" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGT" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGU" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGV" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGW" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGX" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGY" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGZ" decimal(15,3) NOT NULL default '0.000',
  "AAABMQHA" decimal(15,3) NOT NULL default '0.000',
  "AAABMQHB" decimal(15,3) NOT NULL default '0.000',
  "AAABMQHC" decimal(15,3) NOT NULL default '0.000',
  "AAABMQHD" decimal(15,3) NOT NULL default '0.000',
  "AAABMQHE" varchar(3) NOT NULL default ' ',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABMQBS" varchar(4) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMQBU" varchar(8) NOT NULL default '00000000',
  "AAABMQCV" varchar(10) NOT NULL default ' ',
  "AAABMQCW" varchar(4) NOT NULL default '0000',
  "AAABMQDA" varchar(6) NOT NULL default '000000',
  "AAABMQCX" varchar(1) NOT NULL default ' ',
  "AAABMQBZ" varchar(4) NOT NULL default ' ',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABMQCE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMQCG"),
  UNIQUE KEY "AAABGQEL" ("AAABMQHU","AAABMQCJ","AAABMQHY","AAABMQCL","AAABMQDO","AAABMQHV","AAABMQCO","AAABMQHS"),
  KEY "AAABGQEU" ("AAABMQHV","AAABMQDO","AAABMQHY","AAABMQHZ"),
  KEY "AAABGQEZ" ("AAABMQHU","AAABMQCV","AAABMQCW","AAABMQCX","AAABMQDO","AAABMQHV","AAABMQDA"),
  KEY "AAABGQFH" ("AAABMQHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQFJ" (
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
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMQDX" decimal(17,2) NOT NULL default '0.00',
  "AAABMQDY" decimal(17,2) NOT NULL default '0.00',
  "AAABMQDZ" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEA" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEB" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEC" decimal(17,2) NOT NULL default '0.00',
  "AAABMQED" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEE" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEF" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEG" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEH" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEI" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEJ" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEK" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEL" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEM" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEN" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEO" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEP" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEQ" decimal(17,2) NOT NULL default '0.00',
  "AAABMQER" decimal(17,2) NOT NULL default '0.00',
  "AAABMQES" decimal(17,2) NOT NULL default '0.00',
  "AAABMQET" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEU" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEV" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEW" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEX" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEY" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEZ" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFA" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFB" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFC" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFD" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFE" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFF" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFG" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFH" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFI" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFJ" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFK" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFL" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFM" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFN" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFO" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFP" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFQ" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFR" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFS" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFT" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFU" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFV" decimal(17,2) NOT NULL default '0.00',
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
  "AAABMQGN" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGO" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGP" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGQ" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGR" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGS" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGT" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGU" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGV" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGW" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGX" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGY" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGZ" decimal(15,3) NOT NULL default '0.000',
  "AAABMQHA" decimal(15,3) NOT NULL default '0.000',
  "AAABMQHB" decimal(15,3) NOT NULL default '0.000',
  "AAABMQHC" decimal(15,3) NOT NULL default '0.000',
  "AAABMQHD" decimal(15,3) NOT NULL default '0.000',
  "AAABMQHE" varchar(3) NOT NULL default ' ',
  "AAABMQHF" varchar(4) NOT NULL default ' ',
  "AAABMQHG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMQHV","AAABMQHW","AAABMQHX","AAABMQHY","AAABMQHM","AAABMQHN","AAABMQHO","AAABMQHP","AAABMQHQ","AAABMQHR","AAABMQHS"),
  KEY "AAABGQJV" ("AAABMQHV","AAABMQDO","AAABMQHZ"),
  KEY "AAABGQJZ" ("AAABMQHV","AAABMQDO"),
  KEY "AAABGQKC" ("AAABMQHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQKE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHKGP" varchar(32) NOT NULL default ' ',
  "AAABGXLE" varchar(10) NOT NULL default '0000000000',
  "AAABGQLW" varchar(32) NOT NULL default ' ',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  "AAABLADC" varchar(1) NOT NULL default ' ',
  "AAABGQNX" varchar(12) NOT NULL default ' ',
  "AAABGQUU" varchar(16) NOT NULL default ' ',
  "AAABGQPM" tinyint(3) unsigned NOT NULL default '0',
  "AAABGQKO" varchar(2) NOT NULL default ' ',
  "AAABLPPS" varchar(10) NOT NULL default ' ',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGYZF" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEP" varchar(5) NOT NULL default ' ',
  "AAABGQKU" int(11) NOT NULL default '0',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABGQKW" varchar(10) NOT NULL default ' ',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABGQKY" varchar(6) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABGZEW" varchar(8) NOT NULL default '00000000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMMRQ" varchar(4) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAABMCZD" varchar(2) NOT NULL default ' ',
  "AAABMCTH" varchar(1) NOT NULL default ' ',
  "AAABLADA" varchar(4) NOT NULL default ' ',
  "AAABGZEU" varchar(5) NOT NULL default ' ',
  "AAABMCUB" varchar(3) NOT NULL default ' ',
  "AAABMCUC" varchar(3) NOT NULL default ' ',
  "AAABGQLK" varchar(18) NOT NULL default ' ',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABJKTY" varchar(3) NOT NULL default ' ',
  "AAABMCRA" varchar(6) NOT NULL default ' ',
  "AAABGQLQ" varchar(10) NOT NULL default ' ',
  "AAABGQLR" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHKGP","AAABGXLE"),
  KEY "AAABGQLV" ("AAABGQLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQLX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABLAXK" VARBINARY(255),
  "AAABGYMA" VARBINARY(255),
  "AAABGWEK" varchar(4) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABKJVG" varchar(10) NOT NULL default ' ',
  "AAABMQCE" varchar(10) NOT NULL default ' ',
  "AAABGQMG" varchar(8) NOT NULL default '00000000',
  "AAABGQMH" varchar(2) NOT NULL default '00',
  "AAABGQMI" varchar(4) NOT NULL default '0000',
  "AAABGQNO" varchar(10) NOT NULL default ' ',
  "AAABGQNP" varchar(10) NOT NULL default ' ',
  "AAABGQML" varchar(8) NOT NULL default '00000000',
  "AAABGQMM" varchar(2) NOT NULL default '00',
  "AAABGQMN" varchar(4) NOT NULL default '0000',
  "AAABGROP" varchar(1) NOT NULL default ' ',
  "AAABGQMP" tinyint(3) unsigned NOT NULL default '0',
  "AAABGQMQ" varchar(2) NOT NULL default ' ',
  "AAABGZIZ" varchar(8) NOT NULL default '00000000',
  "AAABGTHF" varchar(4) NOT NULL default ' ',
  "AAABGQMT" varchar(1) NOT NULL default ' ',
  "AAABGQMU" varchar(1) NOT NULL default ' ',
  "AAABGXSL" VARBINARY(255),
  "AAABLAXL" varchar(4) NOT NULL default '0000',
  "AAABGVFO" varchar(13) NOT NULL default ' ',
  "AAABMCNE" varchar(25) NOT NULL default ' ',
  "AAABMDGB" varchar(16) NOT NULL default ' ',
  "AAABGQNA" varchar(2) NOT NULL default ' ',
  "AAABGXMD" varchar(2) NOT NULL default ' ',
  "AAABGQNC" varchar(32) NOT NULL default ' ',
  "AAABGQND" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"),
  KEY "AAABGQNG" ("AAABMQCE","AAABKJVG","AAABMRIL"),
  KEY "AAABGQNK" ("AAABLAXK"(255)),
  KEY "AAABGQNM" ("AAABMRIL","AAABGQNO","AAABGQNP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQNQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABGQOC" varchar(32) NOT NULL default ' ',
  "AAABGQNU" varchar(32) NOT NULL default ' ',
  "AAABGQNV" varchar(10) NOT NULL default ' ',
  "AAABGQNW" tinyint(3) unsigned NOT NULL default '0',
  "AAABGQNX" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"),
  KEY "AAABGQOA" ("AAABMRIL","AAABGQOC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQOD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLAXK" VARBINARY(255) NOT NULL,
  "AAABLAXL" varchar(4) NOT NULL default '0000',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGYJY" varchar(3) NOT NULL default ' ',
  "AAABLIMM" varchar(10) NOT NULL default ' ',
  "AAABJGWF" varchar(13) NOT NULL default ' ',
  "AAABJGWE" varchar(10) NOT NULL default ' ',
  "AAABJGWH" varchar(10) NOT NULL default ' ',
  "AAABGWDW" varchar(10) NOT NULL default ' ',
  "AAABGVFO" varchar(13) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLAXK"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQOS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABGVKO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWB","AAABGVKO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQOZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABGVKO" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGQPE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWB","AAABGVKO","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQPJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGQUU" varchar(16) NOT NULL default ' ',
  "AAABGQPM" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABGQUU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQPP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGQQI" varchar(4) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABGQUU" varchar(16) NOT NULL default ' ',
  "AAABLPPS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGQQI","AAABJGWB","AAABGQUU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQPZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGQQI" varchar(4) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABGQUU" varchar(16) NOT NULL default ' ',
  "AAABGVKO" varchar(10) NOT NULL default ' ',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABLPPS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGQQI","AAABJGWB","AAABGQUU","AAABGVKO","AAABGZEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQQN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGQUU" varchar(16) NOT NULL default ' ',
  "AAABGQQQ" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGQUU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQQT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGQUU" varchar(16) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGQQX" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGQUU","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQRB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  "AAABLADC" varchar(1) NOT NULL default ' ',
  "AAABGQSJ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKZWL","AAABLADC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQRJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABLADC" varchar(1) NOT NULL default ' ',
  "AAABGQSJ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKZWL","AAABJGWB","AAABLADC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQRT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGQSJ" varchar(5) NOT NULL default ' ',
  "AAABINNI" varchar(2) NOT NULL default ' ',
  "AAABGQRX" varchar(2) NOT NULL default ' ',
  "AAABGQRY" varchar(16) NOT NULL default ' ',
  "AAABGQRZ" varchar(2) NOT NULL default ' ',
  "AAABGQSA" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGQSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQSD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGQSJ" varchar(5) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGQSH" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGQSJ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQSL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGQSW" varchar(32) NOT NULL default ' ',
  "AAABGVLV" varchar(4) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABGVGY" varchar(32) NOT NULL default ' ',
  "AAABGVGZ" varchar(8) NOT NULL default '00000000',
  "AAABLAXK" VARBINARY(255),
  "AAABGQST" VARBINARY(255),
  "AAABGQSU" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAABGQSW"),
  UNIQUE KEY "AAABGQSX" ("AAABMRIL","AAABGVLV","AAABJGWB","AAABGVGY","AAABGVGZ","AAABLAXK"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQTE" (
  "AAABGQTP" varchar(30) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAABGQTH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABGQTP","AAABLVCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQTK" (
  "AAABGQTP" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAABLLLP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABGQTP","AAABMLQA","AAABLVCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQTS" (
  "AAABHATI" varchar(30) NOT NULL default ' ',
  "AAABGQUK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABHATI","AAABGQUK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQTX" (
  "AAABHATI" varchar(30) NOT NULL default ' ',
  "AAABGQUK" varchar(20) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGQUB" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABHATI","AAABGQUK","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQUF" (
  "AAABHATI" varchar(30) NOT NULL default ' ',
  "AAABGQUK" varchar(20) NOT NULL default ' ',
  "AAABGQUL" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABHATI","AAABGQUK","AAABGQUL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQUM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGQUU" varchar(16) NOT NULL default ' ',
  "AAABGTIB" varchar(8) NOT NULL default ' ',
  "AAABGTIA" varchar(8) NOT NULL default ' ',
  "AAABGQUR" varchar(8) NOT NULL default ' ',
  "AAABGQUS" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGQUU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQUV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIZKK" VARBINARY(255) NOT NULL,
  "AAABKSXJ" int(11) NOT NULL default '0',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  "AAABGQVA" varchar(1) NOT NULL default '0',
  "AAABGQVB" varchar(16) NOT NULL default ' ',
  "AAABGQVC" decimal(21,2) NOT NULL default '0.00',
  "AAABGQVD" varchar(5) NOT NULL default ' ',
  "AAABGYZF" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEP" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIZKK"(255),"AAABKSXJ"),
  KEY "AAABGQVJ" ("AAABIZKK"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQVL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" VARBINARY(255) NOT NULL,
  "AAABGVKJ" varchar(8) NOT NULL default '00000000',
  "AAABGQVP" varchar(12) NOT NULL default ' ',
  "AAABGQVQ" varchar(12) NOT NULL default ' ',
  "AAABGQVR" varchar(1) NOT NULL default ' ',
  "AAABLAXK" VARBINARY(255),
  "AAABGZIS" varchar(32) NOT NULL default ' ',
  "AAABGZIR" varchar(32) NOT NULL default ' ',
  "AAABGQWA" varchar(32) NOT NULL default ' ',
  "AAABGQVW" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"(255)),
  KEY "AAABGQVZ" ("AAABGQWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQWB" (
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAABLQGL" varchar(1) NOT NULL default ' ',
  "AAABHEYE" varchar(4) NOT NULL default ' ',
  "AAABLUSG" varchar(14) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  "AAABGQXP" varchar(10) NOT NULL default ' ',
  "AAABLEJH" varchar(120) NOT NULL default ' ',
  "AAABIOWN" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAABILWI","AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQWO" (
  "AAABLQGL" varchar(1) NOT NULL default ' ',
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAABHNTB" varchar(6) NOT NULL default '000000',
  "AAABHEYE" varchar(4) NOT NULL default ' ',
  "AAABLUSG" varchar(14) NOT NULL default ' ',
  "AAABLEJH" varchar(120) NOT NULL default ' ',
  "AAABGQWW" varchar(20) NOT NULL default ' ',
  "AAABGQWX" varchar(255) NOT NULL default ' ',
  "AAABGQWY" varchar(255) NOT NULL default ' ',
  "AAABGQWZ" varchar(255) NOT NULL default ' ',
  "AAABGQXA" varchar(255) NOT NULL default ' ',
  "AAABKAHC" varchar(11) NOT NULL default ' ',
  "AAABLUVJ" varchar(32) NOT NULL default ' ',
  "AAABGQXD" varchar(1) NOT NULL default ' ',
  "AAABGQXE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLQGL","AAABILWI","AAABLUXQ","AAABHNTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQXJ" (
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAABLQGL" varchar(1) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAABHEYE" varchar(4) NOT NULL default ' ',
  "AAABLUSG" varchar(14) NOT NULL default ' ',
  "AAABGQXP" varchar(10) NOT NULL default ' ',
  "AAABLEJH" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAABILWI","AAABLQGL","AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQXU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLADL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLADL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQXZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCOG" varchar(1) NOT NULL default ' ',
  "AAABLADL" varchar(4) NOT NULL default ' ',
  "AAABGQYD" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCOG","AAABLADL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQYH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGQZW" varchar(8) NOT NULL default ' ',
  "AAABGQYK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGQZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQYN" (
  "AAABLUSV" varchar(10) NOT NULL default ' ',
  "AAABLUUW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQYR" (
  "AAABLVJB" varchar(4) NOT NULL default ' ',
  "AAABLECD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLVJB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQYV" (
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABLVJB" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  "AAABGQYZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNJW","AAABLVJB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQZC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABLAXK" VARBINARY(255),
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGQZH" varchar(13) NOT NULL default ' ',
  "AAABGRNU" varchar(1) NOT NULL default ' ',
  "AAABLAXL" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"),
  UNIQUE KEY "AAABGQZM" ("AAABMRIL","AAABLAXK"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGQZP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGRAE" varchar(32) NOT NULL default ' ',
  "AAABGRAH" varchar(32) NOT NULL default ' ',
  "AAABKSXJ" int(11) NOT NULL default '0',
  "AAABLADS" varchar(10) NOT NULL default ' ',
  "AAABGYJY" varchar(3) NOT NULL default ' ',
  "AAABGQZW" varchar(8) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABJGWE" varchar(10) NOT NULL default ' ',
  "AAABJGWF" varchar(13) NOT NULL default ' ',
  "AAABGZEW" varchar(8) NOT NULL default '00000000',
  "AAABGZER" varchar(5) NOT NULL default ' ',
  "AAABGZES" decimal(21,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABGRAE"),
  KEY "AAABGRAF" ("AAABMRIL","AAABGRAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRAI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABLDAZ" varchar(3) NOT NULL default '000',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABJODP" varchar(5) NOT NULL default ' ',
  "AAABGRAP" varchar(1) NOT NULL default '0',
  "AAABGRAQ" varchar(8) NOT NULL default '00000000',
  "AAABGRAR" varchar(8) NOT NULL default '00000000',
  "AAABGRAS" varchar(8) NOT NULL default '00000000',
  "AAABGRAT" varchar(8) NOT NULL default '00000000',
  "AAABGRAU" varchar(8) NOT NULL default '00000000',
  "AAABLZDP" varchar(12) NOT NULL default ' ',
  "AAABLCIO" varchar(8) NOT NULL default '00000000',
  "AAABLCIP" varchar(6) NOT NULL default '000000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABJLTO" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMEFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRBD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKZWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRBI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGRBM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKZWL","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRBQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABLDCD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKZWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRBX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABLDCD" varchar(4) NOT NULL default ' ',
  "AAABJLZK" varchar(5) NOT NULL default '00000',
  "AAABMDHP" varchar(1) NOT NULL default ' ',
  "AAABKZQK" varchar(4) NOT NULL default ' ',
  "AAABLAXH" varchar(4) NOT NULL default ' ',
  "AAABKZPO" varchar(4) NOT NULL default ' ',
  "AAABJOMP" varchar(4) NOT NULL default ' ',
  "AAABJOMQ" varchar(1) NOT NULL default ' ',
  "AAABMDHQ" varchar(1) NOT NULL default ' ',
  "AAABLDAF" varchar(8) NOT NULL default '00000000',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  "AAABLCBJ" varchar(4) NOT NULL default ' ',
  "AAABJOMV" varchar(4) NOT NULL default '0000',
  "AAABMDYP" varchar(8) NOT NULL default '00000000',
  "AAABMDYS" varchar(2) NOT NULL default '00',
  "AAABLAXJ" varchar(2) NOT NULL default '00',
  "AAABLAAD" varchar(1) NOT NULL default ' ',
  "AAABMEBC" varchar(4) NOT NULL default ' ',
  "AAABLABK" varchar(1) NOT NULL default ' ',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAABMDZP" varchar(1) NOT NULL default '0',
  "AAABMPTH" varchar(1) NOT NULL default '0',
  "AAABMPTI" varchar(6) NOT NULL default '000000',
  "AAABMDZE" decimal(10,7) NOT NULL default '0.0000000',
  "AAABMDZB" varchar(8) NOT NULL default '00000000',
  "AAABMDZM" varchar(4) NOT NULL default ' ',
  "AAABMEAL" varchar(1) NOT NULL default ' ',
  "AAABMDZT" varchar(8) NOT NULL default '00000000',
  "AAABMEFR" varchar(1) NOT NULL default '0',
  "AAABMDZV" varchar(1) NOT NULL default ' ',
  "AAABMEFM" varchar(8) NOT NULL default '00000000',
  "AAABMDHW" varchar(1) NOT NULL default '0',
  "AAABMDHX" varchar(1) NOT NULL default ' ',
  "AAABMEFN" varchar(8) NOT NULL default '00000000',
  "AAABMDHZ" varchar(1) NOT NULL default '0',
  "AAABMDIA" varchar(1) NOT NULL default ' ',
  "AAABMPTJ" varchar(6) NOT NULL default '000000',
  "AAABMEAA" varchar(8) NOT NULL default '00000000',
  "AAABMDIJ" varchar(8) NOT NULL default '00000000',
  "AAABMDIZ" varchar(8) NOT NULL default '00000000',
  "AAABMDJA" varchar(1) NOT NULL default '0',
  "AAABMDJB" varchar(1) NOT NULL default ' ',
  "AAABMDIK" varchar(8) NOT NULL default '00000000',
  "AAABMDIL" varchar(1) NOT NULL default '0',
  "AAABMDIM" varchar(1) NOT NULL default ' ',
  "AAABLADP" varchar(8) NOT NULL default '00000000',
  "AAABLAYD" varchar(5) NOT NULL default ' ',
  "AAABMDHS" decimal(13,2) NOT NULL default '0.00',
  "AAABLBYH" varchar(5) NOT NULL default ' ',
  "AAABMDHR" decimal(13,2) NOT NULL default '0.00',
  "AAABLAAI" varchar(5) NOT NULL default ' ',
  "AAABLAAO" decimal(13,2) NOT NULL default '0.00',
  "AAABLBXX" decimal(13,2) NOT NULL default '0.00',
  "AAABMDZN" decimal(13,2) NOT NULL default '0.00',
  "AAABLCQU" decimal(10,7) NOT NULL default '0.0000000',
  "AAABMEFK" varchar(1) NOT NULL default '0',
  "AAABMDIW" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMEFT","AAABLDCD","AAABJLZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGREL" (
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABLJIM" varchar(40) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOIA"),
  KEY "AAABGREQ" ("AAABMLDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRES" (
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV","AAABLSCE"),
  KEY "AAABGREY" ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRFA" (
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV","AAABLSCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRFG" (
  "AAABLAXL" varchar(4) NOT NULL default '0000',
  "AAABGRFI" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLAXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRFK" (
  "AAABLAXL" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGRFN" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLAXL","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRFQ" (
  "AAABGRFT" varchar(3) NOT NULL default '000',
  "AAABGRFS" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABGRFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRFU" (
  "AAABGRGA" varchar(3) NOT NULL default '000',
  "AAABJPFP" varchar(4) NOT NULL default ' ',
  "AAABJPFO" varchar(4) NOT NULL default ' ',
  "AAABGRFY" varchar(32) NOT NULL default ' ',
  "AAABGRFZ" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABGRGA","AAABJPFP","AAABJPFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRGD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGRLZ" varchar(32) NOT NULL default ' ',
  "AAABKSXJ" int(11) NOT NULL default '0',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  "AAABGZIY" varchar(18) NOT NULL default ' ',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABGYJY" varchar(3) NOT NULL default ' ',
  "AAABGTBZ" varchar(3) NOT NULL default ' ',
  "AAABJMEG" varchar(4) NOT NULL default '0000',
  "AAABJGWC" varchar(4) NOT NULL default '0000',
  "AAABJGWF" varchar(13) NOT NULL default ' ',
  "AAABJGWE" varchar(10) NOT NULL default ' ',
  "AAABJGWH" varchar(10) NOT NULL default ' ',
  "AAABLIMM" varchar(10) NOT NULL default ' ',
  "AAABGRGT" varchar(12) NOT NULL default ' ',
  "AAABJGWI" varchar(13) NOT NULL default ' ',
  "AAABGVFO" varchar(13) NOT NULL default ' ',
  "AAABGWDW" varchar(10) NOT NULL default ' ',
  "AAABKZRY" VARBINARY(255),
  "AAABGSLP" varchar(1) NOT NULL default ' ',
  "AAABGZKC" decimal(22,6) NOT NULL default '0.000000',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEN" decimal(21,2) NOT NULL default '0.00',
  "AAABKZVA" varchar(4) NOT NULL default ' ',
  "AAABGZEP" varchar(5) NOT NULL default ' ',
  "AAABGYZF" decimal(21,2) NOT NULL default '0.00',
  "AAABGWAK" varchar(5) NOT NULL default ' ',
  "AAABGVQU" decimal(21,2) NOT NULL default '0.00',
  "AAABGRHK" varchar(4) NOT NULL default ' ',
  "AAABGRHL" varchar(12) NOT NULL default ' ',
  "AAABGXTR" varchar(1) NOT NULL default ' ',
  "AAABGXTQ" varchar(4) NOT NULL default '0000',
  "AAABIDTM" varchar(1) NOT NULL default '0',
  "AAABGVPH" double NOT NULL default '0',
  "AAABGZER" varchar(5) NOT NULL default ' ',
  "AAABGZES" decimal(21,2) NOT NULL default '0.00',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABGZEU" varchar(5) NOT NULL default ' ',
  "AAABGZEV" varchar(5) NOT NULL default ' ',
  "AAABGZEW" varchar(8) NOT NULL default '00000000',
  "AAABGVPU" varchar(8) NOT NULL default '00000000',
  "AAABLADA" varchar(4) NOT NULL default ' ',
  "AAABLADS" varchar(10) NOT NULL default ' ',
  "AAABLADC" varchar(1) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAABMCZD" varchar(2) NOT NULL default ' ',
  "AAABLADQ" varchar(1) NOT NULL default ' ',
  "AAABLADR" varchar(1) NOT NULL default ' ',
  "AAABKWDD" varchar(1) NOT NULL default ' ',
  "AAABKWDE" varchar(1) NOT NULL default ' ',
  "AAABLCFI" varchar(10) NOT NULL default ' ',
  "AAABKTRY" varchar(20) NOT NULL default ' ',
  "AAABJQXA" varchar(8) NOT NULL default '00000000',
  "AAABGVPT" varchar(1) NOT NULL default '0',
  "AAABGZIW" varchar(8) NOT NULL default '00000000',
  "AAABGZIX" varchar(1) NOT NULL default ' ',
  "AAABGVPV" varchar(8) NOT NULL default '00000000',
  "AAABGVPW" varchar(8) NOT NULL default '00000000',
  "AAABMCUC" varchar(3) NOT NULL default ' ',
  "AAABMCUB" varchar(3) NOT NULL default ' ',
  "AAABKWDG" varchar(20) NOT NULL default ' ',
  "AAABKWDH" varchar(50) NOT NULL default ' ',
  "AAABKXOZ" varchar(5) NOT NULL default '00000',
  "AAABLABR" varchar(16) NOT NULL default ' ',
  "AAABKXKU" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABGRLZ","AAABKSXJ"),
  KEY "AAABGRIY" ("AAABMRIL","AAABJGWD","AAABJGWB","AAABJGWF"),
  KEY "AAABGRJD" ("AAABMRIL","AAABJGWD","AAABJGWB","AAABJGWI"),
  KEY "AAABGRJI" ("AAABMRIL","AAABJGWD","AAABJGWB","AAABGVFO"),
  KEY "AAABGRJN" ("AAABMRIL","AAABJGWD","AAABJGWB","AAABJGWF","AAABGWDW"),
  KEY "AAABGRJT" ("AAABMRIL","AAABJGWF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRJW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGWBG" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABLAXK" VARBINARY(255) NOT NULL,
  PRIMARY KEY ("AAABMRIL","AAABGWBG","AAABJGWD","AAABJGWB","AAABLAXK"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRKH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGWBG" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABLAXK" VARBINARY(255) NOT NULL,
  PRIMARY KEY ("AAABMRIL","AAABGWBG","AAABJGWD","AAABJGWB","AAABLAXK"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRKS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGRLZ" varchar(32) NOT NULL default ' ',
  "AAABKSXJ" int(11) NOT NULL default '0',
  "AAABMDYP" varchar(8) NOT NULL default '00000000',
  "AAABMDZE" decimal(10,7) NOT NULL default '0.0000000',
  "AAABKZIQ" varchar(1) NOT NULL default '0',
  "AAABMDHZ" varchar(1) NOT NULL default '0',
  "AAABMDIL" varchar(1) NOT NULL default '0',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  "AAABKZPO" varchar(4) NOT NULL default ' ',
  "AAABMDIA" varchar(1) NOT NULL default ' ',
  "AAABMDIM" varchar(1) NOT NULL default ' ',
  "AAABMDHX" varchar(1) NOT NULL default ' ',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAABLCWJ" varchar(2) NOT NULL default '00',
  "AAABLACV" varchar(8) NOT NULL default '00000000',
  "AAABLADO" varchar(25) NOT NULL default ' ',
  "AAABLADN" varchar(16) NOT NULL default ' ',
  "AAABLADM" varchar(18) NOT NULL default ' ',
  "AAABLABR" varchar(20) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABLCQU" decimal(10,7) NOT NULL default '0.0000000',
  "AAABMEFK" varchar(1) NOT NULL default '0',
  "AAABLABC" varchar(10) NOT NULL default ' ',
  "AAABLCCM" varchar(8) NOT NULL default '00000000',
  "AAABLACJ" varchar(10) NOT NULL default ' ',
  "AAABLADP" varchar(8) NOT NULL default '00000000',
  "AAABKZRV" varchar(4) NOT NULL default '0000',
  "AAABKZUD" varchar(10) NOT NULL default ' ',
  "AAABKZUE" varchar(10) NOT NULL default ' ',
  "AAABMDHQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGRLZ","AAABKSXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRMB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABGZIR" varchar(32) NOT NULL default ' ',
  "AAABGZIS" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default ' ',
  "AAABKNGX" tinyint(3) unsigned NOT NULL default '0',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABLAXK" VARBINARY(255),
  "AAABKSXM" varchar(60) NOT NULL default ' ',
  "AAABLAXL" varchar(4) NOT NULL default '0000',
  "AAABGWBG" varchar(3) NOT NULL default '000',
  "AAABGVMC" varchar(3) NOT NULL default '000',
  "AAABGRNU" varchar(1) NOT NULL default ' ',
  "AAABGRMQ" tinyint(3) unsigned NOT NULL default '0',
  "AAABGZKF" varchar(8) NOT NULL default '00000000',
  "AAABJOTX" varchar(8) NOT NULL default '00000000',
  "AAABGXSI" varchar(6) NOT NULL default '000000',
  "AAABGXSL" VARBINARY(255),
  "AAABGXMD" varchar(2) NOT NULL default ' ',
  "AAABGZIZ" varchar(8) NOT NULL default '00000000',
  "AAABGWBR" varchar(8) NOT NULL default '00000000',
  "AAABGWBS" varchar(2) NOT NULL default '00',
  "AAABGWBT" varchar(8) NOT NULL default '00000000',
  "AAABGWBU" varchar(2) NOT NULL default '00',
  "AAABGVFO" varchar(13) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"),
  UNIQUE KEY "AAABGRNE" ("AAABMRIL","AAABLAXK"(255)),
  KEY "AAABGRNH" ("AAABMRIL","AAABKSXM"),
  KEY "AAABGRNK" ("AAABJGWD","AAABJGWB","AAABGRNU","AAABGZKF","AAABLAXL","AAABGVMC"),
  KEY "AAABGRNR" ("AAABMRIL","AAABJGWD","AAABGRNU","AAABGVFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRNW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABGRNZ" varchar(32) NOT NULL default ' ',
  "AAABGROM" varchar(32) NOT NULL default ' ',
  "AAABGZIR" varchar(32) NOT NULL default ' ',
  "AAABGZIS" varchar(32) NOT NULL default ' ',
  "AAABGYMA" VARBINARY(255),
  "AAABGROP" varchar(1) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"),
  UNIQUE KEY "AAABGROI" ("AAABMRIL","AAABGYMA"(255)),
  KEY "AAABGROL" ("AAABGROM","AAABMRIL"),
  KEY "AAABGROO" ("AAABGROP"),
  KEY "AAABGROQ" ("AAABMRIL","AAABGZIS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGROT" (
  "AAABMBXY" VARBINARY(255) NOT NULL,
  "AAABGRPF" VARBINARY(255) NOT NULL,
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABMMJT" longtext,
  "AAABLDNU" longtext,
  "AAABGROZ" varchar(60) NOT NULL default ' ',
  "AAABGRPA" varchar(120) NOT NULL default ' ',
  "AAABGRPB" varchar(1) NOT NULL default ' ',
  "AAABGRPC" longtext,
  "AAABGRPD" VARBINARY(255),
  PRIMARY KEY ("AAABMBXY"(255),"AAABGRPF"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRPG" (
  "AAABMBXY" VARBINARY(255) NOT NULL,
  "AAABLRQQ" varchar(10) NOT NULL default ' ',
  "AAABJSPH" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY"(255),"AAABLRQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRPM" (
  "AAABGRPU" varchar(60) NOT NULL default ' ',
  "AAABGRPV" varchar(120) NOT NULL default ' ',
  "AAABMBXY" VARBINARY(255) NOT NULL,
  "AAABGRPQ" varchar(60) NOT NULL default ' ',
  "AAABGRPR" varchar(60) NOT NULL default ' ',
  "AAABGRPS" varchar(60) NOT NULL default ' ',
  "AAABGRPT" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABGRPU","AAABGRPV","AAABMBXY"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRPX" (
  "AAABMBXY" VARBINARY(255) NOT NULL,
  "AAABGRQH" VARBINARY(255) NOT NULL,
  "AAABGRQI" smallint(6) NOT NULL default '0',
  "AAABGRQB" varchar(1) NOT NULL default ' ',
  "AAABGRQC" VARBINARY(255),
  "AAABGRQD" VARBINARY(255),
  "AAABGRQE" VARBINARY(255),
  "AAABGRQF" VARBINARY(255),
  PRIMARY KEY ("AAABMBXY"(255),"AAABGRQH"(255),"AAABGRQI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRQJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHKHZ" varchar(4) NOT NULL default ' ',
  "AAABGRZB" varchar(30) NOT NULL default ' ',
  "AAABGRQN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHKHZ","AAABGRZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRQR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHKIM" varchar(4) NOT NULL default ' ',
  "AAABGRZB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHKIM","AAABGRZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGRRM" varchar(2) NOT NULL default ' ',
  "AAABGRRB" varchar(1) NOT NULL default ' ',
  "AAABGRRC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGRRM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRRF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGRRM" varchar(2) NOT NULL default ' ',
  "AAABGRRJ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGRRM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRRN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCMO" varchar(4) NOT NULL default ' ',
  "AAABGRRZ" varchar(10) NOT NULL default ' ',
  "AAABLOAV" varchar(10) NOT NULL default ' ',
  "AAABIJKF" varchar(40) NOT NULL default ' ',
  "AAABGRRT" decimal(13,2) NOT NULL default '0.00',
  "AAABKYSQ" varchar(5) NOT NULL default ' ',
  "AAABGRRV" int(11) NOT NULL default '0',
  "AAABGRRW" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMCMO","AAABGRRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRSA" (
  "AAABLUAV" varchar(45) NOT NULL default ' ',
  "AAABGRSQ" varchar(6) NOT NULL default '000000',
  "AAABLUSV" varchar(6) NOT NULL default '000000',
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABLTQF" varchar(6) NOT NULL default '000000',
  "AAABLTQG" varchar(6) NOT NULL default '000000',
  "AAABGRSI" varchar(6) NOT NULL default '000000',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  "AAABLTQE" varchar(1) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABLQGL" varchar(1) NOT NULL default ' ',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABLJIM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUAV","AAABGRSQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRSR" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABJSPH" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRSV" (
  "AAABHMRI" varchar(32) NOT NULL default ' ',
  "AAABLTQF" varchar(32) NOT NULL default ' ',
  "AAABMRCK" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABHMRI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRTA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGRYC" varchar(11) NOT NULL default ' ',
  "AAABGYQM" varchar(30) NOT NULL default ' ',
  "AAABGRZB" varchar(30) NOT NULL default ' ',
  "AAABGRTF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGRYC","AAABGYQM","AAABGRZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRTK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGRYC" varchar(11) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGRYC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRTP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGRYC" varchar(11) NOT NULL default ' ',
  "AAABGRTT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGRYC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRTX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGRYF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGRYF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRUC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGRYF" varchar(4) NOT NULL default ' ',
  "AAABGRUG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGRYF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRUK" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABGRUP" varchar(4) NOT NULL default ' ',
  "AAABGRUN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABGRUP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRUQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABGRUX" varchar(10) NOT NULL default ' ',
  "AAABGRUU" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABGRUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRUY" (
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABGRVK" varchar(30) NOT NULL default ' ',
  "AAABHFPL" varchar(1) NOT NULL default ' ',
  "AAABGRVE" varchar(1) NOT NULL default ' ',
  "AAABIYEO" varchar(12) NOT NULL default ' ',
  "AAABHIYE" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMKFH","AAABMREL","AAABMREF","AAABGRVK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRVL" (
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABGRVN" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRVP" (
  "AAABHEAF" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEAF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRVS" (
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABGRWO" varchar(40) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABGRVX" varchar(1) NOT NULL default ' ',
  "AAABGRVY" varchar(3) NOT NULL default ' ',
  "AAABGRVZ" varchar(1) NOT NULL default ' ',
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABLEER" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLRST","AAABMKFH","AAABGRWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRWG" (
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABGRWO" varchar(40) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKZAW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLRST","AAABMKFH","AAABGRWO","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRWQ" (
  "AAABLOZE" varchar(6) NOT NULL default ' ',
  "AAABHEAF" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAABHEAF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRWV" (
  "AAABHEGP" varchar(10) NOT NULL default ' ',
  "AAABHEAF" varchar(12) NOT NULL default ' ',
  "AAABGRWY" varchar(12) NOT NULL default ' ',
  "AAABLCCC" varchar(2) NOT NULL default '00',
  "AAABININ" varchar(35) NOT NULL default ' ',
  "AAABGRXB" varchar(241) NOT NULL default ' ',
  "AAABIYJL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEGP","AAABHEAF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRXF" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHEGP" varchar(10) NOT NULL default ' ',
  "AAABHEAF" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHEGP","AAABHEAF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRXN" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHEAF" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHEAF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRXT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGRYN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGRYN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRXY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGRYN" varchar(4) NOT NULL default ' ',
  "AAABGRYF" varchar(4) NOT NULL default ' ',
  "AAABGRYC" varchar(11) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGRYN","AAABGRYF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRYG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGRYN" varchar(4) NOT NULL default ' ',
  "AAABGRYK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGRYN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRYO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGRZJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGRZJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRYT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGRZJ" varchar(4) NOT NULL default ' ',
  "AAABGYQM" varchar(10) NOT NULL default ' ',
  "AAABGRZB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGRZJ","AAABGYQM","AAABGRZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRZC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGRZJ" varchar(4) NOT NULL default ' ',
  "AAABGRZG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGRZJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRZK" (
  "AAABHIXN" varchar(32) NOT NULL default ' ',
  "AAABHATI" varchar(32) NOT NULL default ' ',
  "AAABGRZN" varchar(40) NOT NULL default ' ',
  "AAABGRZO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHIXN","AAABHATI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRZR" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIBEW" varchar(3) NOT NULL default '000',
  "AAABGRZU" varchar(4) NOT NULL default ' ',
  "AAABGRZV" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABIBEW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGRZY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABKZZG" varchar(13) NOT NULL default ' ',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZKH","AAABKZZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSAG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGXYC" varchar(32) NOT NULL default ' ',
  "AAABKZZG" varchar(13) NOT NULL default ' ',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABGXYC","AAABKZZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSAP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGXXT" varchar(32) NOT NULL default ' ',
  "AAABKZZG" varchar(13) NOT NULL default ' ',
  "AAABGSQT" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGXXT","AAABKZZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSAY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABKZZG" varchar(13) NOT NULL default ' ',
  "AAABGSQT" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZKH","AAABKZZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSBH" (
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
CREATE TABLE "AAABGSCD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGSIS" varchar(8) NOT NULL default ' ',
  "AAABGSIT" varchar(4) NOT NULL default ' ',
  "AAABGSIU" varchar(8) NOT NULL default ' ',
  "AAABGSIV" varchar(4) NOT NULL default ' ',
  "AAABGSHN" varchar(24) NOT NULL default ' ',
  "AAABGSHP" varchar(24) NOT NULL default '000000000000000000000000',
  "AAABGSGE" varchar(24) NOT NULL default ' ',
  "AAABHCQK" varchar(32) NOT NULL default ' ',
  "AAABGSHG" varchar(1) NOT NULL default ' ',
  "AAABGSHX" varchar(8) NOT NULL default ' ',
  "AAABGSGH" varchar(24) NOT NULL default '000000000000000000000000',
  "AAABGSGI" varchar(12) NOT NULL default ' ',
  "AAABGSGJ" varchar(30) NOT NULL default ' ',
  "AAABGSGK" varchar(8) NOT NULL default '00000000',
  "AAABGSGL" varchar(6) NOT NULL default '000000',
  "AAABGSEZ" varchar(24) NOT NULL default ' ',
  "AAABGSGM" varchar(8) NOT NULL default ' ',
  "AAABGSFB" varchar(1) NOT NULL default ' ',
  "AAABGSFC" varchar(8) NOT NULL default '00000000',
  "AAABGSFD" varchar(6) NOT NULL default '000000',
  "AAABGSFE" varchar(4) NOT NULL default '0000',
  "AAABGSGN" varchar(1) NOT NULL default ' ',
  "AAABGSGO" varchar(73) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGSIS","AAABGSIT","AAABGSIU","AAABGSIV","AAABGSHN","AAABGSHP"),
  KEY "AAABGSDJ" ("AAABMRIL","AAABGSHN","AAABGSHP"),
  KEY "AAABGSDN" ("AAABMRIL","AAABGSEZ"),
  KEY "AAABGSDQ" ("AAABMRIL","AAABGSHP"),
  KEY "AAABGSDT" ("AAABMRIL","AAABGSHN","AAABGSIS","AAABGSHP","AAABGSHX"),
  KEY "AAABGSDZ" ("AAABGSIV","AAABGSIU","AAABMRIL","AAABGSIT","AAABGSIS"),
  KEY "AAABGSEF" ("AAABGSHX","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSEI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGSIS" varchar(8) NOT NULL default ' ',
  "AAABGSIT" varchar(4) NOT NULL default ' ',
  "AAABGSIU" varchar(8) NOT NULL default ' ',
  "AAABGSIV" varchar(4) NOT NULL default ' ',
  "AAABGSHN" varchar(24) NOT NULL default ' ',
  "AAABGSHP" varchar(24) NOT NULL default '000000000000000000000000',
  "AAABGSGE" varchar(24) NOT NULL default ' ',
  "AAABHCQK" varchar(32) NOT NULL default ' ',
  "AAABGSHG" varchar(1) NOT NULL default ' ',
  "AAABGSHX" varchar(8) NOT NULL default ' ',
  "AAABGSGH" varchar(24) NOT NULL default '000000000000000000000000',
  "AAABGSGI" varchar(12) NOT NULL default ' ',
  "AAABGSGJ" varchar(30) NOT NULL default ' ',
  "AAABGSGK" varchar(8) NOT NULL default '00000000',
  "AAABGSGL" varchar(6) NOT NULL default '000000',
  "AAABGSEZ" varchar(24) NOT NULL default ' ',
  "AAABGSGM" varchar(8) NOT NULL default ' ',
  "AAABGSFB" varchar(1) NOT NULL default ' ',
  "AAABGSFC" varchar(8) NOT NULL default '00000000',
  "AAABGSFD" varchar(6) NOT NULL default '000000',
  "AAABGSFE" varchar(4) NOT NULL default '0000',
  "AAABGSGN" varchar(1) NOT NULL default ' ',
  "AAABGSGO" varchar(73) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGSIS","AAABGSIT","AAABGSIU","AAABGSIV","AAABGSHN","AAABGSHP"),
  KEY "AAABGSFO" ("AAABMRIL","AAABGSHN","AAABGSHP"),
  KEY "AAABGSFS" ("AAABMRIL","AAABGSHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSFV" (
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
  KEY "AAABGSGX" ("AAABMRIL","AAABGSHN","AAABHCQK","AAABGSHP"),
  KEY "AAABGSHC" ("AAABMRIL","AAABGSHP"),
  KEY "AAABGSHF" ("AAABGSHG","AAABMRIL","AAABHCQK","AAABGSHN"),
  KEY "AAABGSHK" ("AAABMRIL","AAABHCQK","AAABGSHN","AAABGSIS","AAABGSHP"),
  KEY "AAABGSHQ" ("AAABGSIV","AAABGSIU","AAABMRIL","AAABGSIT","AAABGSIS"),
  KEY "AAABGSHW" ("AAABGSHX","AAABMRIL","AAABHCQK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSIA" (
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
  KEY "AAABGSIY" ("AAABGSIZ","AAABGSJA"),
  KEY "AAABGSJB" ("AAABGSJC"),
  KEY "AAABGSJD" ("AAABGSJE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSJF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKZWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSJK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABGSJN" varchar(8) NOT NULL default ' ',
  "AAABGSJO" varchar(8) NOT NULL default ' ',
  "AAABGSJP" varchar(8) NOT NULL default ' ',
  "AAABGSJQ" varchar(8) NOT NULL default ' ',
  "AAABGSJR" varchar(8) NOT NULL default ' ',
  "AAABGSJS" varchar(8) NOT NULL default ' ',
  "AAABGSJT" varchar(8) NOT NULL default ' ',
  "AAABGSJU" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCAK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSJX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  "AAABGVPJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKZWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSKD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABLAXK" VARBINARY(255),
  "AAABLAXL" varchar(4) NOT NULL default '0000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABGZKF" varchar(8) NOT NULL default '00000000',
  "AAABJOTX" varchar(8) NOT NULL default '00000000',
  "AAABGXSI" varchar(6) NOT NULL default '000000',
  "AAABKWVR" decimal(15,6) NOT NULL default '0.000000',
  "AAABGXSL" VARBINARY(255),
  "AAABGYZR" varchar(12) NOT NULL default ' ',
  "AAABGYZS" varchar(8) NOT NULL default '00000000',
  "AAABGYZT" varchar(6) NOT NULL default '000000',
  "AAABGSOI" varchar(12) NOT NULL default ' ',
  "AAABGXME" varchar(8) NOT NULL default '00000000',
  "AAABGSOK" varchar(6) NOT NULL default '000000',
  "AAABGSKU" varchar(1) NOT NULL default ' ',
  "AAABGSLE" varchar(32) NOT NULL default ' ',
  "AAABGSKW" varchar(32) NOT NULL default ' ',
  "AAABGSLV" varchar(32) NOT NULL default ' ',
  "AAABGSLR" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"),
  KEY "AAABGSLB" ("AAABLAXK"(255)),
  KEY "AAABGSLD" ("AAABGSLE"),
  KEY "AAABGSLF" ("AAABMRIL","AAABGSLV","AAABLAXL","AAABGZKF","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSLL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABGSLO" VARBINARY(255),
  "AAABGSLP" varchar(1) NOT NULL default ' ',
  "AAABGSLV" varchar(32) NOT NULL default ' ',
  "AAABGSLR" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"),
  KEY "AAABGSLU" ("AAABGSLV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSLW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGXUV" varchar(13) NOT NULL default ' ',
  "AAABGWDW" varchar(10) NOT NULL default ' ',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGXWX" varchar(1) NOT NULL default ' ',
  "AAABGSME" varchar(1) NOT NULL default ' ',
  "AAABGZGM" varchar(10) NOT NULL default ' ',
  "AAABIWPG" varchar(2) NOT NULL default ' ',
  "AAABGVMC" varchar(3) NOT NULL default '000',
  "AAABGVKO" varchar(8) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABKXML" varchar(8) NOT NULL default '00000000',
  "AAABKXMM" varchar(6) NOT NULL default '000000',
  "AAABKXMN" varchar(12) NOT NULL default ' ',
  "AAABKXMO" varchar(8) NOT NULL default '00000000',
  "AAABKXMP" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"),
  KEY "AAABGSMS" ("AAABJGWD","AAABGXUV","AAABGWDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSMW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGXYC" varchar(32) NOT NULL default ' ',
  "AAABGZJF" VARBINARY(255),
  "AAABGYYS" int(11) NOT NULL default '0',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  "AAABKZVA" varchar(4) NOT NULL default ' ',
  "AAABGVPJ" varchar(4) NOT NULL default ' ',
  "AAABGSNE" varchar(3) NOT NULL default ' ',
  "AAABGZKC" decimal(22,6) NOT NULL default '0.000000',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  "AAABGZKF" varchar(8) NOT NULL default '00000000',
  "AAABGSNN" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGXYC"),
  KEY "AAABGSNM" ("AAABGSNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSNO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGSPA" varchar(32) NOT NULL default ' ',
  "AAABLAXK" VARBINARY(255),
  "AAABLAXL" varchar(4) NOT NULL default '0000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGSQL" varchar(32) NOT NULL default ' ',
  "AAABGZKF" varchar(8) NOT NULL default '00000000',
  "AAABGZEW" varchar(8) NOT NULL default '00000000',
  "AAABGVPU" varchar(8) NOT NULL default '00000000',
  "AAABGVKJ" varchar(8) NOT NULL default '00000000',
  "AAABGZIY" varchar(18) NOT NULL default ' ',
  "AAABGZEU" varchar(5) NOT NULL default ' ',
  "AAABGZEV" varchar(5) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABGVFO" varchar(13) NOT NULL default ' ',
  "AAABGYZR" varchar(12) NOT NULL default ' ',
  "AAABGYZS" varchar(8) NOT NULL default '00000000',
  "AAABGYZT" varchar(6) NOT NULL default '000000',
  "AAABGSOH" varchar(2) NOT NULL default ' ',
  "AAABGSOI" varchar(12) NOT NULL default ' ',
  "AAABGXME" varchar(8) NOT NULL default '00000000',
  "AAABGSOK" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABGSPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSON" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGSPA" varchar(32) NOT NULL default ' ',
  "AAABKSXJ" int(11) NOT NULL default '0',
  "AAABGSOR" int(11) NOT NULL default '0',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEP" varchar(5) NOT NULL default ' ',
  "AAABGYZF" decimal(21,2) NOT NULL default '0.00',
  "AAABGZES" decimal(21,2) NOT NULL default '0.00',
  "AAABGZER" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGSPA","AAABKSXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSPC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGSPZ" varchar(10) NOT NULL default ' ',
  "AAABGSPG" varchar(30) NOT NULL default ' ',
  "AAABGSPH" varchar(15) NOT NULL default ' ',
  "AAABGSPI" varchar(15) NOT NULL default ' ',
  "AAABGSPJ" varchar(10) NOT NULL default ' ',
  "AAABGZEU" varchar(5) NOT NULL default ' ',
  "AAABGZEV" varchar(5) NOT NULL default ' ',
  "AAABHRZY" varchar(4) NOT NULL default ' ',
  "AAABKXGH" varchar(10) NOT NULL default ' ',
  "AAABLZDP" varchar(12) NOT NULL default ' ',
  "AAABLCIO" varchar(8) NOT NULL default '00000000',
  "AAABLCIP" varchar(6) NOT NULL default '000000',
  "AAABLCIQ" varchar(10) NOT NULL default ' ',
  "AAABLCIS" varchar(12) NOT NULL default ' ',
  "AAABLCIT" varchar(8) NOT NULL default '00000000',
  "AAABLCIU" varchar(6) NOT NULL default '000000',
  "AAABLCIV" varchar(10) NOT NULL default ' ',
  "AAABGSPW" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWD","AAABGSPZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSQA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGXXT" varchar(32) NOT NULL default ' ',
  "AAABGZKF" varchar(8) NOT NULL default '00000000',
  "AAABGZKC" decimal(22,6) NOT NULL default '0.000000',
  "AAABGSQT" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGSQL" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGXXT"),
  KEY "AAABGSQK" ("AAABGSQL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSQM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABGZKF" varchar(8) NOT NULL default '00000000',
  "AAABLAXL" varchar(4) NOT NULL default '0000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABGZKC" decimal(22,6) NOT NULL default '0.000000',
  "AAABGSQT" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGSRI" VARBINARY(255),
  "AAABGXUA" varchar(32) NOT NULL default ' ',
  "AAABGXTU" varchar(32) NOT NULL default ' ',
  "AAABGSRF" varchar(32) NOT NULL default ' ',
  "AAABGSQZ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"),
  KEY "AAABGSRC" ("AAABGXUA"),
  KEY "AAABGSRE" ("AAABGSRF"),
  KEY "AAABGSRG" ("AAABMRIL","AAABGSRI"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSRJ" (
  "AAABGSRS" varchar(20) NOT NULL default ' ',
  "AAABMNMP" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABGSRS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSRN" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGSRS" varchar(20) NOT NULL default ' ',
  "AAABHWUC" varchar(43) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABGSRS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSRT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGSSK" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGSRX" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGSSK","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSSB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGSSK" varchar(4) NOT NULL default ' ',
  "AAABJGTZ" varchar(4) NOT NULL default ' ',
  "AAABGTFO" varchar(2) NOT NULL default ' ',
  "AAABGTFP" varchar(1) NOT NULL default ' ',
  "AAABGTFQ" varchar(1) NOT NULL default '0',
  "AAABGTFR" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABGSSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSSL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJMEG" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABJMEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJMEG" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGSSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJMEG","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSSY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGYJY" varchar(3) NOT NULL default ' ',
  "AAABGTBZ" varchar(3) NOT NULL default ' ',
  "AAABGSTT" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWD","AAABGYJY","AAABGTBZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSTI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGSTT" varchar(3) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGSTT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSTO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGSTT" varchar(3) NOT NULL default ' ',
  "AAABJMEG" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABGSTT","AAABJMEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSUB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABGWEK" varchar(4) NOT NULL default ' ',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABJGWC" varchar(4) NOT NULL default '0000',
  "AAABLAOA" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABJGWB","AAABGWEK","AAABLCAK","AAABJGWC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSUN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGWEK" varchar(4) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABGSUR" varchar(1) NOT NULL default ' ',
  "AAABGSUS" varchar(1) NOT NULL default ' ',
  "AAABGSUT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGWEK","AAABJGWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSUX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUZL" varchar(4) NOT NULL default '0000',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUZL","AAABKZWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSVE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  "AAABKZVA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKZWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSVK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGTBC" varchar(4) NOT NULL default ' ',
  "AAABJGTZ" varchar(4) NOT NULL default ' ',
  "AAABKYLX" varchar(3) NOT NULL default ' ',
  "AAABJGWK" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGTBC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSVS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGTBC" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGSVW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGTBC","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSWA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGTSR" varchar(3) NOT NULL default ' ',
  "AAABHEYG" varchar(20) NOT NULL default ' ',
  "AAABGSWE" varchar(3) NOT NULL default '000',
  "AAABGSWF" varchar(10) NOT NULL default ' ',
  "AAABGSWG" varchar(10) NOT NULL default ' ',
  "AAABGSWH" varchar(10) NOT NULL default ' ',
  "AAABLAPE" varchar(2) NOT NULL default ' ',
  "AAABGSWJ" varchar(20) NOT NULL default ' ',
  "AAABGSWK" varchar(1) NOT NULL default ' ',
  "AAABLORS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSWN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABJGWJ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABJGWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSWT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGSWV" varchar(4) NOT NULL default '0000',
  "AAABGSWW" varchar(4) NOT NULL default '0000',
  "AAABGSWX" varchar(4) NOT NULL default '0000',
  "AAABGSWY" varchar(4) NOT NULL default '0000',
  "AAABGSWZ" varchar(4) NOT NULL default '0000',
  "AAABGSXA" varchar(4) NOT NULL default '0000',
  "AAABGSXB" varchar(4) NOT NULL default '0000',
  "AAABGSXC" varchar(4) NOT NULL default '0000',
  "AAABGSXD" varchar(4) NOT NULL default '0000',
  "AAABGSXE" varchar(4) NOT NULL default '0000',
  "AAABGSXF" varchar(4) NOT NULL default '0000',
  "AAABGSXG" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSXI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGYJY" varchar(3) NOT NULL default ' ',
  "AAABGSXS" varchar(3) NOT NULL default ' ',
  "AAABGSXT" varchar(1) NOT NULL default ' ',
  "AAABGWEX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWD","AAABGYJY","AAABGSXS","AAABGSXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSXU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGSXZ" varchar(10) NOT NULL default ' ',
  "AAABJMEG" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABGSXZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSYA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGYJY" varchar(3) NOT NULL default ' ',
  "AAABGSYF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKZWL","AAABJGWD","AAABGYJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGWBG" varchar(3) NOT NULL default '000',
  "AAABGVMC" varchar(3) NOT NULL default '000',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABGWEK" varchar(4) NOT NULL default ' ',
  "AAABHIJR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGWBG","AAABGVMC","AAABJGWB","AAABGWEK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSYW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGWBG" varchar(3) NOT NULL default '000',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABGWEK" varchar(4) NOT NULL default ' ',
  "AAABHIJR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGWBG","AAABJGWB","AAABGWEK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSZG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  "AAABGSZK" varchar(1) NOT NULL default ' ',
  "AAABGTAB" varchar(3) NOT NULL default ' ',
  "AAABKZWQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWB","AAABKZWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSZQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGTAB" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGTAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGSZV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGTAB" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGSZZ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGTAB","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTAD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGWEX" varchar(4) NOT NULL default ' ',
  "AAABGTFO" varchar(2) NOT NULL default ' ',
  "AAABGTFP" varchar(1) NOT NULL default ' ',
  "AAABGTAI" varchar(1) NOT NULL default '0',
  "AAABJGVG" varchar(4) NOT NULL default ' ',
  "AAABLAOA" varchar(3) NOT NULL default '000',
  "AAABGTFR" varchar(3) NOT NULL default '000',
  "AAABGTAM" varchar(2) NOT NULL default ' ',
  "AAABGTAN" varchar(3) NOT NULL default '000',
  "AAABGTAO" varchar(4) NOT NULL default ' ',
  "AAABGTAP" varchar(1) NOT NULL default ' ',
  "AAABGTAQ" varchar(3) NOT NULL default '000',
  "AAABGTAR" varchar(4) NOT NULL default ' ',
  "AAABGTAS" varchar(1) NOT NULL default ' ',
  "AAABGTAT" varchar(3) NOT NULL default '000',
  "AAABGTAU" varchar(4) NOT NULL default ' ',
  "AAABGTAV" varchar(1) NOT NULL default ' ',
  "AAABGTAW" varchar(3) NOT NULL default '000',
  "AAABGTAX" varchar(4) NOT NULL default ' ',
  "AAABGTAY" varchar(1) NOT NULL default ' ',
  "AAABGTAZ" varchar(3) NOT NULL default '000',
  "AAABGTBA" varchar(4) NOT NULL default ' ',
  "AAABGTBB" varchar(1) NOT NULL default ' ',
  "AAABGTBC" varchar(4) NOT NULL default ' ',
  "AAABGTBD" varchar(1) NOT NULL default ' ',
  "AAABGTBE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGWEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTBH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABGWEK" varchar(4) NOT NULL default ' ',
  "AAABGVMC" varchar(3) NOT NULL default '000',
  "AAABGYJY" varchar(3) NOT NULL default ' ',
  "AAABJGWC" varchar(4) NOT NULL default '0000',
  "AAABGTBZ" varchar(3) NOT NULL default ' ',
  "AAABLIMM" varchar(10) NOT NULL default ' ',
  "AAABJGWH" varchar(10) NOT NULL default ' ',
  "AAABJGWE" varchar(10) NOT NULL default ' ',
  "AAABGWEX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWB","AAABGWEK","AAABGVMC","AAABGYJY","AAABJGWC","AAABGTBZ","AAABLIMM","AAABJGWH","AAABJGWE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTCD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGWEX" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGVLK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGWEX","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTCL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGWBG" varchar(3) NOT NULL default '000',
  "AAABGVMC" varchar(3) NOT NULL default '000',
  "AAABGYJY" varchar(3) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABGWEK" varchar(4) NOT NULL default ' ',
  "AAABHIJR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGWBG","AAABGVMC","AAABGYJY","AAABJGWB","AAABGWEK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTCZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABJGWC" varchar(4) NOT NULL default '0000',
  "AAABGVMC" varchar(3) NOT NULL default '000',
  "AAABGYJY" varchar(3) NOT NULL default ' ',
  "AAABJGWF" varchar(13) NOT NULL default ' ',
  "AAABGTFA" varchar(4) NOT NULL default ' ',
  "AAABGTET" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWB","AAABJGWC","AAABGVMC","AAABGYJY","AAABJGWF","AAABGTFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTDP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABJGWC" varchar(4) NOT NULL default '0000',
  "AAABGTET" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWB","AAABJGWC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTDX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABJGWC" varchar(4) NOT NULL default '0000',
  "AAABGVMC" varchar(3) NOT NULL default '000',
  "AAABGYJY" varchar(3) NOT NULL default ' ',
  "AAABJGWF" varchar(13) NOT NULL default ' ',
  "AAABGTET" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWB","AAABJGWC","AAABGVMC","AAABGYJY","AAABJGWF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTEL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABJGWC" varchar(4) NOT NULL default '0000',
  "AAABGVMC" varchar(3) NOT NULL default '000',
  "AAABGYJY" varchar(3) NOT NULL default ' ',
  "AAABJGWF" varchar(13) NOT NULL default ' ',
  "AAABGTFA" varchar(4) NOT NULL default ' ',
  "AAABGTET" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWB","AAABJGWC","AAABGVMC","AAABGYJY","AAABJGWF","AAABGTFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTFB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGTPN" varchar(3) NOT NULL default '000',
  "AAABGTTN" varchar(4) NOT NULL default ' ',
  "AAABJGWK" varchar(2) NOT NULL default ' ',
  "AAABJGTY" varchar(3) NOT NULL default ' ',
  "AAABJGTZ" varchar(4) NOT NULL default ' ',
  "AAABJGUA" varchar(1) NOT NULL default ' ',
  "AAABJGUB" varchar(1) NOT NULL default ' ',
  "AAABJGUC" varchar(1) NOT NULL default ' ',
  "AAABJGUD" varchar(1) NOT NULL default ' ',
  "AAABJGUE" varchar(1) NOT NULL default ' ',
  "AAABGTFN" varchar(1) NOT NULL default ' ',
  "AAABGTFO" varchar(2) NOT NULL default ' ',
  "AAABGTFP" varchar(1) NOT NULL default ' ',
  "AAABGTFQ" varchar(1) NOT NULL default '0',
  "AAABGTFR" varchar(3) NOT NULL default '000',
  "AAABGTFS" varchar(1) NOT NULL default ' ',
  "AAABGTFT" tinyint(3) unsigned NOT NULL default '0',
  "AAABJGRZ" varchar(4) NOT NULL default ' ',
  "AAABJGSA" varchar(4) NOT NULL default ' ',
  "AAABJGSB" varchar(4) NOT NULL default ' ',
  "AAABJGSC" varchar(4) NOT NULL default ' ',
  "AAABJGSG" varchar(1) NOT NULL default ' ',
  "AAABJGSH" varchar(1) NOT NULL default ' ',
  "AAABGTGA" varchar(2) NOT NULL default ' ',
  "AAABGTGB" varchar(1) NOT NULL default ' ',
  "AAABGTGC" varchar(1) NOT NULL default ' ',
  "AAABGTGD" varchar(1) NOT NULL default '0',
  "AAABMDHX" varchar(1) NOT NULL default ' ',
  "AAABMDIA" varchar(1) NOT NULL default ' ',
  "AAABGTGG" varchar(1) NOT NULL default ' ',
  "AAABGTGH" varchar(1) NOT NULL default ' ',
  "AAABGTGI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGTPN","AAABGTTN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTGM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGTPN" varchar(3) NOT NULL default '000',
  "AAABGTTN" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGTGR" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGTPN","AAABGTTN","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTGW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGWEK" varchar(4) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABGWAK" varchar(5) NOT NULL default ' ',
  "AAABGTHB" tinyint(3) unsigned NOT NULL default '0',
  "AAABGTHC" varchar(2) NOT NULL default ' ',
  "AAABGTHD" varchar(4) NOT NULL default ' ',
  "AAABGTHE" tinyint(3) unsigned NOT NULL default '0',
  "AAABGTHF" varchar(4) NOT NULL default ' ',
  "AAABGTHG" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGWEK","AAABJGWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTHK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGUBO" varchar(3) NOT NULL default '000',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABGZDN" varchar(10) NOT NULL default ' ',
  "AAABGTIA" varchar(8) NOT NULL default ' ',
  "AAABGTIB" varchar(8) NOT NULL default ' ',
  "AAABGTIC" varchar(8) NOT NULL default '00000000',
  "AAABGTHT" decimal(10,7) NOT NULL default '0.0000000',
  "AAABGTHU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWD","AAABGUBO","AAABLEHK","AAABGZDN","AAABGTIA","AAABGTIB","AAABGTIC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTID" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGWEK" varchar(4) NOT NULL default ' ',
  "AAABGTIG" varchar(60) NOT NULL default ' ',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGWEK"),
  KEY "AAABGTIK" ("AAABJGWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTIM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJMEG" varchar(4) NOT NULL default '0000',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABJGWC" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABJMEG","AAABJGWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTIU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABGTIX" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTJA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABJGWC" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABJGWB","AAABJGWC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTJH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABJGWC" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGTJM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWB","AAABJGWC","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTJR" (
  "AAABLMOZ" varchar(3) NOT NULL default ' ',
  "AAABLAXL" varchar(4) NOT NULL default '0000',
  "AAABGTJX" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLMOZ","AAABLAXL","AAABGTJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTJY" (
  "AAABGTLB" varchar(4) NOT NULL default '0000',
  "AAABGTKD" varchar(4) NOT NULL default '0000',
  "AAABGTKB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABGTLB","AAABGTKD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTKE" (
  "AAABGTLB" varchar(4) NOT NULL default '0000',
  "AAABGTKK" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABGTLB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTKI" (
  "AAABGTKV" varchar(4) NOT NULL default '0000',
  "AAABGTKK" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABGTKV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTKM" (
  "AAABGTLB" varchar(4) NOT NULL default '0000',
  "AAABGTKV" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABGTLB","AAABGTKV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTKR" (
  "AAABGTKV" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGTKU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABGTKV","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTKX" (
  "AAABGTLB" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGTLA" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABGTLB","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTLD" (
  "AAABGTLH" varchar(4) NOT NULL default '0000',
  "AAABGTLI" varchar(4) NOT NULL default '0000',
  "AAABGTLG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABGTLH","AAABGTLI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTLJ" (
  "AAABLUZL" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLUZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTLM" (
  "AAABLUZL" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGTLP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUZL","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTLS" (
  "AAABGWBG" varchar(3) NOT NULL default '000',
  "AAABJUQY" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGWBG","AAABJUQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTLY" (
  "AAABGWBG" varchar(3) NOT NULL default '000',
  "AAABGTMF" varchar(4) NOT NULL default '0000',
  "AAABGTMB" varchar(2) NOT NULL default '00',
  "AAABGTMC" varchar(2) NOT NULL default '00',
  "AAABGTMD" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABGWBG","AAABGTMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTMG" (
  "AAABGWBG" varchar(3) NOT NULL default '000',
  "AAABGVRD" varchar(2) NOT NULL default '00',
  "AAABGTMJ" varchar(30) NOT NULL default ' ',
  "AAABGTNL" varchar(1) NOT NULL default ' ',
  "AAABGTNM" varchar(1) NOT NULL default ' ',
  "AAABGTNN" varchar(1) NOT NULL default ' ',
  "AAABGTNO" varchar(1) NOT NULL default ' ',
  "AAABGTMV" varchar(1) NOT NULL default ' ',
  "AAABGTNQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGWBG","AAABGVRD"),
  KEY "AAABGTMS" ("AAABGWBG","AAABGVRD","AAABGTMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTMW" (
  "AAABGWBG" varchar(3) NOT NULL default '000',
  "AAABGVRD" varchar(2) NOT NULL default '00',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGTNA" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABGWBG","AAABGVRD","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTNE" (
  "AAABGTSR" varchar(3) NOT NULL default ' ',
  "AAABGTTI" varchar(3) NOT NULL default ' ',
  "AAABGTNH" varchar(3) NOT NULL default ' ',
  "AAABGTNI" varchar(3) NOT NULL default '000',
  "AAABGTNJ" varchar(1) NOT NULL default ' ',
  "AAABGTNK" varchar(30) NOT NULL default ' ',
  "AAABGTNL" varchar(1) NOT NULL default ' ',
  "AAABGTNM" varchar(1) NOT NULL default ' ',
  "AAABGTNN" varchar(1) NOT NULL default ' ',
  "AAABGTNO" varchar(1) NOT NULL default ' ',
  "AAABGTNP" varchar(1) NOT NULL default ' ',
  "AAABGTNQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGTSR","AAABGTTI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTNT" (
  "AAABGTNZ" varchar(4) NOT NULL default '0000',
  "AAABGTSR" varchar(3) NOT NULL default ' ',
  "AAABGTNW" varchar(3) NOT NULL default ' ',
  "AAABGTNX" varchar(3) NOT NULL default ' ',
  "AAABGTNY" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABGTNZ"),
  KEY "AAABGTOA" ("AAABGTSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTOC" (
  "AAABGTSR" varchar(3) NOT NULL default ' ',
  "AAABGTTI" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGTOG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABGTSR","AAABGTTI","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTOK" (
  "AAABLMOZ" varchar(3) NOT NULL default ' ',
  "AAABGWBG" varchar(3) NOT NULL default '000',
  "AAABGTOQ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLMOZ","AAABGWBG","AAABGTOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTOR" (
  "AAABLUZL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTOU" (
  "AAABLUZL" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGTOX" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUZL","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTPA" (
  "AAABGTPN" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABGTPN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTPD" (
  "AAABGTPN" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGTPG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABGTPN","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTPJ" (
  "AAABLAOA" varchar(3) NOT NULL default '000',
  "AAABGTPN" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLAOA","AAABGTPN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTPO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGTVI" varchar(10) NOT NULL default ' ',
  "AAABGTVY" varchar(10) NOT NULL default ' ',
  "AAABGTVZ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWD","AAABGTVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTPX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGTQB" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABGTQB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTQC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGTTE" VARBINARY(255) NOT NULL,
  "AAABGWBG" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGVLV" varchar(4) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABGXUJ" varchar(8) NOT NULL default '00000000',
  "AAABGVRD" varchar(2) NOT NULL default '00',
  "AAABGVRE" varchar(1) NOT NULL default '0',
  "AAABGTSU" decimal(5,2) NOT NULL default '0.00',
  "AAABLZDP" varchar(12) NOT NULL default ' ',
  "AAABLCIO" varchar(8) NOT NULL default '00000000',
  "AAABLCIP" varchar(6) NOT NULL default '000000',
  "AAABLCIQ" varchar(10) NOT NULL default ' ',
  "AAABLCIS" varchar(12) NOT NULL default ' ',
  "AAABLCIT" varchar(8) NOT NULL default '00000000',
  "AAABLCIU" varchar(6) NOT NULL default '000000',
  "AAABLCIV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGTTE"(255)),
  KEY "AAABGTQX" ("AAABGWBG","AAABJGWD","AAABJGWB","AAABGVRD","AAABGVRE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTRD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" VARBINARY(255) NOT NULL,
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGVFO" varchar(13) NOT NULL default ' ',
  "AAABGTRO" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAABGZKH"(255)),
  KEY "AAABGTRL" ("AAABGVFO"),
  KEY "AAABGTRN" ("AAABGTRO"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTRP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGTTE" varchar(32) NOT NULL default ' ',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGTSR" varchar(3) NOT NULL default ' ',
  "AAABGTTI" varchar(3) NOT NULL default ' ',
  "AAABGTTJ" varchar(1) NOT NULL default '0',
  "AAABGTSU" decimal(5,2) NOT NULL default '0.00',
  "AAABLZDP" varchar(12) NOT NULL default ' ',
  "AAABLCIO" varchar(8) NOT NULL default '00000000',
  "AAABLCIP" varchar(6) NOT NULL default '000000',
  "AAABLCIQ" varchar(10) NOT NULL default ' ',
  "AAABLCIS" varchar(12) NOT NULL default ' ',
  "AAABLCIT" varchar(8) NOT NULL default '00000000',
  "AAABLCIU" varchar(6) NOT NULL default '000000',
  "AAABLCIV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGTTE"),
  KEY "AAABGTSH" ("AAABMRIL","AAABJGWD","AAABGTSR","AAABGTTI","AAABGTTJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTSN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGTTE" varchar(32) NOT NULL default ' ',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGTSR" varchar(3) NOT NULL default ' ',
  "AAABGTTI" varchar(3) NOT NULL default ' ',
  "AAABGTTJ" varchar(1) NOT NULL default '0',
  "AAABGTSU" decimal(5,2) NOT NULL default '0.00',
  "AAABLZDP" varchar(12) NOT NULL default ' ',
  "AAABLCIO" varchar(8) NOT NULL default '00000000',
  "AAABLCIP" varchar(6) NOT NULL default '000000',
  "AAABLCIQ" varchar(10) NOT NULL default ' ',
  "AAABLCIS" varchar(12) NOT NULL default ' ',
  "AAABLCIT" varchar(8) NOT NULL default '00000000',
  "AAABLCIU" varchar(6) NOT NULL default '000000',
  "AAABLCIV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGTTE"),
  KEY "AAABGTTF" ("AAABMRIL","AAABJGWD","AAABGTTI","AAABGTTJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTTK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLAOA" varchar(3) NOT NULL default ' ',
  "AAABGTTN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLAOA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTTQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABJGWF" varchar(13) NOT NULL default ' ',
  "AAABJGWE" varchar(10) NOT NULL default ' ',
  "AAABJGWH" varchar(10) NOT NULL default ' ',
  "AAABIFBQ" varchar(3) NOT NULL default ' ',
  "AAABLAOA" varchar(3) NOT NULL default '000',
  "AAABGWEX" varchar(4) NOT NULL default ' ',
  "AAABGTTZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWD","AAABJGWF","AAABJGWE","AAABJGWH","AAABIFBQ","AAABLAOA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTUH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIFBQ" varchar(3) NOT NULL default ' ',
  "AAABGTUK" varchar(3) NOT NULL default '000',
  "AAABGTUL" varchar(4) NOT NULL default ' ',
  "AAABGTUM" varchar(3) NOT NULL default '000',
  "AAABGTUN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIFBQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTUQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABLAXK" VARBINARY(255) NOT NULL,
  "AAABGTVA" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABJGWD","AAABJGWB","AAABLAXK"(255),"AAABGTVA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTVB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGTVI" varchar(10) NOT NULL default ' ',
  "AAABGTVY" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWD","AAABGTVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTVJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGTVY" varchar(10) NOT NULL default ' ',
  "AAABGTVZ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWD","AAABGTVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTVR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABLAYL" varchar(10) NOT NULL default ' ',
  "AAABGTXT" varchar(4) NOT NULL default '0000',
  "AAABGTVY" varchar(10) NOT NULL default ' ',
  "AAABGTVZ" varchar(60) NOT NULL default ' ',
  "AAABLIMM" varchar(1) NOT NULL default ' ',
  "AAABJMEG" varchar(4) NOT NULL default '0000',
  "AAABJGWC" varchar(4) NOT NULL default '0000',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABGTWE" decimal(15,2) NOT NULL default '0.00',
  "AAABLAYB" decimal(15,5) NOT NULL default '0.00000',
  "AAABLBYH" varchar(5) NOT NULL default ' ',
  "AAABKZUA" varchar(5) NOT NULL default ' ',
  "AAABGTWI" decimal(15,2) NOT NULL default '0.00',
  "AAABGTWJ" decimal(15,2) NOT NULL default '0.00',
  "AAABGTWK" decimal(15,2) NOT NULL default '0.00',
  "AAABGTWL" decimal(15,2) NOT NULL default '0.00',
  "AAABGTWM" decimal(15,2) NOT NULL default '0.00',
  "AAABGTWN" decimal(15,2) NOT NULL default '0.00',
  "AAABGTWO" decimal(15,2) NOT NULL default '0.00',
  "AAABGTWP" decimal(15,2) NOT NULL default '0.00',
  "AAABGTWQ" decimal(15,2) NOT NULL default '0.00',
  "AAABGTWR" decimal(15,2) NOT NULL default '0.00',
  "AAABGTWS" decimal(15,2) NOT NULL default '0.00',
  "AAABGTWT" decimal(15,2) NOT NULL default '0.00',
  "AAABGTWU" decimal(15,2) NOT NULL default '0.00',
  "AAABGTWV" decimal(15,2) NOT NULL default '0.00',
  "AAABGTWW" decimal(15,2) NOT NULL default '0.00',
  "AAABGTWX" decimal(15,2) NOT NULL default '0.00',
  "AAABGTWY" decimal(15,2) NOT NULL default '0.00',
  "AAABGTWZ" decimal(15,2) NOT NULL default '0.00',
  "AAABGTXA" decimal(15,2) NOT NULL default '0.00',
  "AAABGTXB" decimal(15,2) NOT NULL default '0.00',
  "AAABGTXC" decimal(15,2) NOT NULL default '0.00',
  "AAABGTXD" decimal(15,2) NOT NULL default '0.00',
  "AAABGTXE" decimal(15,2) NOT NULL default '0.00',
  "AAABGTXF" decimal(15,2) NOT NULL default '0.00',
  "AAABGTXG" decimal(15,2) NOT NULL default '0.00',
  "AAABGTXH" decimal(15,2) NOT NULL default '0.00',
  "AAABGTXI" decimal(15,2) NOT NULL default '0.00',
  "AAABGTXJ" decimal(15,2) NOT NULL default '0.00',
  "AAABGTXK" decimal(15,2) NOT NULL default '0.00',
  "AAABGTXL" decimal(15,2) NOT NULL default '0.00',
  "AAABGTXM" decimal(15,2) NOT NULL default '0.00',
  "AAABGTXN" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABJGWB","AAABMREX","AAABMEFT","AAABLAYL","AAABGTXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTXU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABGTXX" varchar(12) NOT NULL default ' ',
  "AAABJOTX" varchar(8) NOT NULL default '00000000',
  "AAABGXSI" varchar(6) NOT NULL default '000000',
  "AAABGTYA" varchar(20) NOT NULL default ' ',
  "AAABGTYB" varchar(12) NOT NULL default ' ',
  "AAABJQCK" varchar(8) NOT NULL default '00000000',
  "AAABJQCL" varchar(6) NOT NULL default '000000',
  "AAABJQCN" varchar(20) NOT NULL default ' ',
  "AAABGTYF" VARBINARY(255),
  "AAABGTYG" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAABGZKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTYJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGTZF" varchar(2) NOT NULL default ' ',
  "AAABGTZG" varchar(32) NOT NULL default ' ',
  "AAABGTZH" varchar(4) NOT NULL default '0000',
  "AAABGTZC" varchar(8) NOT NULL default '00000000',
  "AAABGTZD" varchar(10) NOT NULL default ' ',
  "AAABGTYQ" decimal(10,7) NOT NULL default '0.0000000',
  "AAABKXKB" varchar(2) NOT NULL default ' ',
  "AAABGTYS" varchar(8) NOT NULL default '00000000',
  "AAABGTYT" varchar(2) NOT NULL default ' ',
  "AAABGTYU" varchar(12) NOT NULL default ' ',
  "AAABGYZS" varchar(8) NOT NULL default '00000000',
  "AAABGYZT" varchar(6) NOT NULL default '000000',
  "AAABGTYX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGTZF","AAABGTZG","AAABGTZH","AAABGTZC","AAABGTZD"),
  KEY "AAABGTZE" ("AAABGTZF","AAABGTZG","AAABGTZH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTZI" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABGTZK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGTZM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGWEX" varchar(4) NOT NULL default ' ',
  "AAABGUJC" varchar(8) NOT NULL default ' ',
  "AAABGUJD" varchar(8) NOT NULL default ' ',
  "AAABGUJE" varchar(8) NOT NULL default ' ',
  "AAABGUJF" varchar(8) NOT NULL default ' ',
  "AAABGUJG" varchar(8) NOT NULL default ' ',
  "AAABGUJH" varchar(8) NOT NULL default ' ',
  "AAABGUJI" varchar(8) NOT NULL default ' ',
  "AAABGUJJ" varchar(8) NOT NULL default ' ',
  "AAABGUKD" varchar(8) NOT NULL default ' ',
  "AAABGUKE" varchar(8) NOT NULL default ' ',
  "AAABGUKF" varchar(8) NOT NULL default ' ',
  "AAABGUKG" varchar(8) NOT NULL default ' ',
  "AAABGUKH" varchar(8) NOT NULL default ' ',
  "AAABGUKI" varchar(8) NOT NULL default ' ',
  "AAABGUKJ" varchar(8) NOT NULL default ' ',
  "AAABGUKK" varchar(8) NOT NULL default ' ',
  "AAABGUAF" varchar(8) NOT NULL default ' ',
  "AAABGUAG" varchar(8) NOT NULL default ' ',
  "AAABGUOC" varchar(8) NOT NULL default ' ',
  "AAABGUOD" varchar(8) NOT NULL default ' ',
  "AAABGUOE" varchar(8) NOT NULL default ' ',
  "AAABGUOF" varchar(8) NOT NULL default ' ',
  "AAABGUOG" varchar(8) NOT NULL default ' ',
  "AAABGUOH" varchar(8) NOT NULL default ' ',
  "AAABGUOI" varchar(8) NOT NULL default ' ',
  "AAABGUOJ" varchar(8) NOT NULL default ' ',
  "AAABGUAP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGWEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUAS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGUBF" varchar(32) NOT NULL default ' ',
  "AAABKSXJ" varchar(4) NOT NULL default '0000',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  "AAABGUAX" varchar(4) NOT NULL default ' ',
  "AAABKTVC" varchar(10) NOT NULL default ' ',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  "AAABGWAK" varchar(5) NOT NULL default ' ',
  "AAABGVQU" decimal(21,2) NOT NULL default '0.00',
  "AAABGUCE" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAABGUBF","AAABKSXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUBH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABGWEK" varchar(4) NOT NULL default ' ',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABLADI" varchar(13) NOT NULL default ' ',
  "AAABGUBO" varchar(3) NOT NULL default '000',
  "AAABJGWJ" varchar(8) NOT NULL default '00000000',
  "AAABLAXL" varchar(4) NOT NULL default '0000',
  "AAABGUBR" varchar(3) NOT NULL default '000',
  "AAABGUCE" VARBINARY(255),
  "AAABGUCC" VARBINARY(255),
  "AAABGUBU" varchar(4) NOT NULL default ' ',
  "AAABGZFQ" varchar(32) NOT NULL default ' ',
  "AAABGZFR" varchar(32) NOT NULL default ' ',
  "AAABGZIS" varchar(32) NOT NULL default ' ',
  "AAABGZIR" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"),
  KEY "AAABGUCB" ("AAABGUCC"(255)),
  KEY "AAABGUCD" ("AAABGUCE"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUCF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXJ" varchar(3) NOT NULL default ' ',
  "AAABKZWO" varchar(3) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABGUCK" varchar(4) NOT NULL default ' ',
  "AAABGUCL" varchar(4) NOT NULL default ' ',
  "AAABGUCM" varchar(4) NOT NULL default ' ',
  "AAABGUCN" varchar(4) NOT NULL default ' ',
  "AAABGUCO" varchar(4) NOT NULL default ' ',
  "AAABGUCP" varchar(4) NOT NULL default ' ',
  "AAABGUCQ" varchar(4) NOT NULL default ' ',
  "AAABGUCR" varchar(4) NOT NULL default ' ',
  "AAABGUCS" varchar(4) NOT NULL default ' ',
  "AAABGUCT" varchar(4) NOT NULL default ' ',
  "AAABGUCU" varchar(4) NOT NULL default ' ',
  "AAABGUCV" varchar(4) NOT NULL default ' ',
  "AAABGUCW" varchar(4) NOT NULL default ' ',
  "AAABGUCX" varchar(4) NOT NULL default ' ',
  "AAABGUCY" varchar(4) NOT NULL default ' ',
  "AAABGUCZ" varchar(4) NOT NULL default ' ',
  "AAABGUDA" varchar(4) NOT NULL default ' ',
  "AAABGUDB" varchar(4) NOT NULL default ' ',
  "AAABGUDC" varchar(4) NOT NULL default ' ',
  "AAABGUDD" varchar(4) NOT NULL default ' ',
  "AAABGUDE" varchar(4) NOT NULL default ' ',
  "AAABGUDF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYXJ","AAABKZWO","AAABLCPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUDK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJQYD" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABJQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUDP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJQYD" varchar(3) NOT NULL default '000',
  "AAABGUDT" varchar(10) NOT NULL default ' ',
  "AAABGUDU" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJQYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUDY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(60) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABJFGQ" varchar(1) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUEM" (
  "AAABLQGL" varchar(1) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABLQGL","AAABLTJI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUER" (
  "AAABIMBK" varchar(10) NOT NULL default ' ',
  "AAABMMVU" varchar(1) NOT NULL default ' ',
  "AAABGUEU" varchar(2) NOT NULL default ' ',
  "AAABLECD" varchar(1) NOT NULL default ' ',
  "AAABHBRI" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABGUEY" varchar(1) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABIMBK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUFB" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABIMBK" varchar(10) NOT NULL default ' ',
  "AAABLUDG" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABIMBK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUFH" (
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABGUFK" varchar(40) NOT NULL default ' ',
  "AAABGUFL" varchar(8) NOT NULL default ' ',
  "AAABGUFM" varchar(40) NOT NULL default ' ',
  "AAABLECD" varchar(1) NOT NULL default ' ',
  "AAABGUFO" varchar(6) NOT NULL default ' ',
  "AAABGUFP" varchar(1) NOT NULL default ' ',
  "AAABGUFQ" varchar(3) NOT NULL default ' ',
  "AAABLELA" varchar(20) NOT NULL default ' ',
  "AAABGUFS" varchar(1) NOT NULL default ' ',
  "AAABGUFT" varchar(1) NOT NULL default ' ',
  "AAABGUFU" varchar(3) NOT NULL default '000',
  "AAABGUFV" varchar(1) NOT NULL default ' ',
  "AAABGUFW" varchar(1) NOT NULL default ' ',
  "AAABGUFX" varchar(1) NOT NULL default ' ',
  "AAABGUFY" varchar(30) NOT NULL default ' ',
  "AAABGUFZ" varchar(1) NOT NULL default ' ',
  "AAABGUGA" varchar(30) NOT NULL default ' ',
  "AAABGUGB" varchar(30) NOT NULL default ' ',
  "AAABGUGC" varchar(30) NOT NULL default ' ',
  "AAABGUGD" varchar(30) NOT NULL default ' ',
  "AAABGUGE" varchar(1) NOT NULL default ' ',
  "AAABGUGF" varchar(1) NOT NULL default ' ',
  "AAABGUGG" varchar(1) NOT NULL default ' ',
  "AAABGUGH" varchar(1) NOT NULL default ' ',
  "AAABHTWY" varchar(4) NOT NULL default ' ',
  "AAABGUGJ" varchar(1) NOT NULL default ' ',
  "AAABGUGK" varchar(1) NOT NULL default ' ',
  "AAABGUGL" varchar(1) NOT NULL default ' ',
  "AAABLGAD" varchar(1) NOT NULL default ' ',
  "AAABGUGN" varchar(30) NOT NULL default ' ',
  "AAABLOJO" varchar(1) NOT NULL default ' ',
  "AAABGUGP" varchar(1) NOT NULL default ' ',
  "AAABGUGQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHQV","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUGT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABGWEK" varchar(4) NOT NULL default ' ',
  "AAABGVMS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKZWL","AAABJGWB","AAABGWEK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUHD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUHI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGWEX" varchar(4) NOT NULL default ' ',
  "AAABGUHL" varchar(8) NOT NULL default ' ',
  "AAABGUHM" varchar(8) NOT NULL default ' ',
  "AAABGUHN" varchar(8) NOT NULL default ' ',
  "AAABGUHO" varchar(8) NOT NULL default ' ',
  "AAABGUHP" varchar(8) NOT NULL default ' ',
  "AAABGUHQ" varchar(8) NOT NULL default ' ',
  "AAABGUHR" varchar(8) NOT NULL default ' ',
  "AAABGUHS" varchar(8) NOT NULL default ' ',
  "AAABGUHT" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGWEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUHW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGWEX" varchar(4) NOT NULL default ' ',
  "AAABGUHZ" varchar(8) NOT NULL default ' ',
  "AAABGUIA" varchar(8) NOT NULL default ' ',
  "AAABGUIB" varchar(8) NOT NULL default ' ',
  "AAABGUIC" varchar(8) NOT NULL default ' ',
  "AAABGUID" varchar(8) NOT NULL default ' ',
  "AAABGUIE" varchar(8) NOT NULL default ' ',
  "AAABGUIF" varchar(8) NOT NULL default ' ',
  "AAABGUIG" varchar(8) NOT NULL default ' ',
  "AAABGUIH" varchar(8) NOT NULL default ' ',
  "AAABGUII" varchar(8) NOT NULL default ' ',
  "AAABGUIJ" varchar(8) NOT NULL default ' ',
  "AAABGUIK" varchar(8) NOT NULL default ' ',
  "AAABGUIL" varchar(8) NOT NULL default ' ',
  "AAABGUIM" varchar(8) NOT NULL default ' ',
  "AAABGUIN" varchar(8) NOT NULL default ' ',
  "AAABGUIO" varchar(8) NOT NULL default ' ',
  "AAABGUIP" varchar(8) NOT NULL default ' ',
  "AAABGUIQ" varchar(8) NOT NULL default ' ',
  "AAABGUIR" varchar(8) NOT NULL default ' ',
  "AAABGUIS" varchar(8) NOT NULL default ' ',
  "AAABGUIT" varchar(8) NOT NULL default ' ',
  "AAABGUIU" varchar(8) NOT NULL default ' ',
  "AAABGUIV" varchar(8) NOT NULL default ' ',
  "AAABGUIW" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGWEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUIZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGWEX" varchar(4) NOT NULL default ' ',
  "AAABGUJC" varchar(8) NOT NULL default ' ',
  "AAABGUJD" varchar(8) NOT NULL default ' ',
  "AAABGUJE" varchar(8) NOT NULL default ' ',
  "AAABGUJF" varchar(8) NOT NULL default ' ',
  "AAABGUJG" varchar(8) NOT NULL default ' ',
  "AAABGUJH" varchar(8) NOT NULL default ' ',
  "AAABGUJI" varchar(8) NOT NULL default ' ',
  "AAABGUJJ" varchar(8) NOT NULL default ' ',
  "AAABGUJK" varchar(8) NOT NULL default ' ',
  "AAABGUJL" varchar(8) NOT NULL default ' ',
  "AAABGUJM" varchar(8) NOT NULL default ' ',
  "AAABGUJN" varchar(8) NOT NULL default ' ',
  "AAABGUJO" varchar(8) NOT NULL default ' ',
  "AAABGUJP" varchar(8) NOT NULL default ' ',
  "AAABGUJQ" varchar(8) NOT NULL default ' ',
  "AAABGUJR" varchar(8) NOT NULL default ' ',
  "AAABGUJS" varchar(8) NOT NULL default ' ',
  "AAABGUJT" varchar(8) NOT NULL default ' ',
  "AAABGUJU" varchar(8) NOT NULL default ' ',
  "AAABGUJV" varchar(8) NOT NULL default ' ',
  "AAABGUJW" varchar(8) NOT NULL default ' ',
  "AAABGUJX" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGWEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUKA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGWEX" varchar(4) NOT NULL default ' ',
  "AAABGUKD" varchar(8) NOT NULL default ' ',
  "AAABGUKE" varchar(8) NOT NULL default ' ',
  "AAABGUKF" varchar(8) NOT NULL default ' ',
  "AAABGUKG" varchar(8) NOT NULL default ' ',
  "AAABGUKH" varchar(8) NOT NULL default ' ',
  "AAABGUKI" varchar(8) NOT NULL default ' ',
  "AAABGUKJ" varchar(8) NOT NULL default ' ',
  "AAABGUKK" varchar(8) NOT NULL default ' ',
  "AAABGUKL" varchar(8) NOT NULL default ' ',
  "AAABGUKM" varchar(8) NOT NULL default ' ',
  "AAABGUKN" varchar(8) NOT NULL default ' ',
  "AAABGUKO" varchar(8) NOT NULL default ' ',
  "AAABGUKP" varchar(8) NOT NULL default ' ',
  "AAABGUKQ" varchar(8) NOT NULL default ' ',
  "AAABGUKR" varchar(8) NOT NULL default ' ',
  "AAABGUKS" varchar(8) NOT NULL default ' ',
  "AAABGUKT" varchar(8) NOT NULL default ' ',
  "AAABGUKU" varchar(8) NOT NULL default ' ',
  "AAABGUKV" varchar(8) NOT NULL default ' ',
  "AAABGUKW" varchar(8) NOT NULL default ' ',
  "AAABGUKX" varchar(8) NOT NULL default ' ',
  "AAABGUKY" varchar(8) NOT NULL default ' ',
  "AAABGUKZ" varchar(8) NOT NULL default ' ',
  "AAABGULA" varchar(8) NOT NULL default ' ',
  "AAABGULB" varchar(8) NOT NULL default ' ',
  "AAABGULC" varchar(8) NOT NULL default ' ',
  "AAABGULD" varchar(8) NOT NULL default ' ',
  "AAABGULE" varchar(8) NOT NULL default ' ',
  "AAABGULF" varchar(8) NOT NULL default ' ',
  "AAABGULG" varchar(8) NOT NULL default ' ',
  "AAABGULH" varchar(8) NOT NULL default ' ',
  "AAABGULI" varchar(8) NOT NULL default ' ',
  "AAABGULJ" varchar(8) NOT NULL default ' ',
  "AAABGULK" varchar(8) NOT NULL default ' ',
  "AAABGULL" varchar(8) NOT NULL default ' ',
  "AAABGULM" varchar(8) NOT NULL default ' ',
  "AAABGULN" varchar(8) NOT NULL default ' ',
  "AAABGULO" varchar(8) NOT NULL default ' ',
  "AAABGULP" varchar(8) NOT NULL default ' ',
  "AAABGULQ" varchar(8) NOT NULL default ' ',
  "AAABGULR" varchar(8) NOT NULL default ' ',
  "AAABGULS" varchar(8) NOT NULL default ' ',
  "AAABGULT" varchar(8) NOT NULL default ' ',
  "AAABGULU" varchar(8) NOT NULL default ' ',
  "AAABGULV" varchar(8) NOT NULL default ' ',
  "AAABGULW" varchar(8) NOT NULL default ' ',
  "AAABGULX" varchar(8) NOT NULL default ' ',
  "AAABGULY" varchar(8) NOT NULL default ' ',
  "AAABGULZ" varchar(8) NOT NULL default ' ',
  "AAABGUMA" varchar(8) NOT NULL default ' ',
  "AAABGUMB" varchar(8) NOT NULL default ' ',
  "AAABGUMC" varchar(8) NOT NULL default ' ',
  "AAABGUMD" varchar(8) NOT NULL default ' ',
  "AAABGUME" varchar(8) NOT NULL default ' ',
  "AAABGUMF" varchar(8) NOT NULL default ' ',
  "AAABGUMG" varchar(8) NOT NULL default ' ',
  "AAABGUMH" varchar(8) NOT NULL default ' ',
  "AAABGUMI" varchar(8) NOT NULL default ' ',
  "AAABGUMJ" varchar(8) NOT NULL default ' ',
  "AAABGUMK" varchar(8) NOT NULL default ' ',
  "AAABGUML" varchar(8) NOT NULL default ' ',
  "AAABGUMM" varchar(8) NOT NULL default ' ',
  "AAABGUMN" varchar(8) NOT NULL default ' ',
  "AAABGUMO" varchar(8) NOT NULL default ' ',
  "AAABGUMP" varchar(8) NOT NULL default ' ',
  "AAABGUMQ" varchar(8) NOT NULL default ' ',
  "AAABGUMR" varchar(8) NOT NULL default ' ',
  "AAABGUMS" varchar(8) NOT NULL default ' ',
  "AAABGUMT" varchar(8) NOT NULL default ' ',
  "AAABGUMU" varchar(8) NOT NULL default ' ',
  "AAABGUMV" varchar(8) NOT NULL default ' ',
  "AAABGUMW" varchar(8) NOT NULL default ' ',
  "AAABGUMX" varchar(8) NOT NULL default ' ',
  "AAABGUMY" varchar(8) NOT NULL default ' ',
  "AAABGUMZ" varchar(8) NOT NULL default ' ',
  "AAABGUNA" varchar(8) NOT NULL default ' ',
  "AAABGUNB" varchar(8) NOT NULL default ' ',
  "AAABGUNC" varchar(8) NOT NULL default ' ',
  "AAABGUND" varchar(8) NOT NULL default ' ',
  "AAABGUNE" varchar(8) NOT NULL default ' ',
  "AAABGUNF" varchar(8) NOT NULL default ' ',
  "AAABGUNG" varchar(8) NOT NULL default ' ',
  "AAABGUNH" varchar(8) NOT NULL default ' ',
  "AAABGUNI" varchar(8) NOT NULL default ' ',
  "AAABGUNJ" varchar(8) NOT NULL default ' ',
  "AAABGUNK" varchar(8) NOT NULL default ' ',
  "AAABGUNL" varchar(8) NOT NULL default ' ',
  "AAABGUNM" varchar(8) NOT NULL default ' ',
  "AAABGUNN" varchar(8) NOT NULL default ' ',
  "AAABGUNO" varchar(8) NOT NULL default ' ',
  "AAABGUNP" varchar(8) NOT NULL default ' ',
  "AAABGUNQ" varchar(8) NOT NULL default ' ',
  "AAABGUNR" varchar(8) NOT NULL default ' ',
  "AAABGUNS" varchar(8) NOT NULL default ' ',
  "AAABGUNT" varchar(8) NOT NULL default ' ',
  "AAABGUNU" varchar(8) NOT NULL default ' ',
  "AAABGUNV" varchar(8) NOT NULL default ' ',
  "AAABGUNW" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGWEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUNZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGWEX" varchar(4) NOT NULL default ' ',
  "AAABGUOC" varchar(8) NOT NULL default ' ',
  "AAABGUOD" varchar(8) NOT NULL default ' ',
  "AAABGUOE" varchar(8) NOT NULL default ' ',
  "AAABGUOF" varchar(8) NOT NULL default ' ',
  "AAABGUOG" varchar(8) NOT NULL default ' ',
  "AAABGUOH" varchar(8) NOT NULL default ' ',
  "AAABGUOI" varchar(8) NOT NULL default ' ',
  "AAABGUOJ" varchar(8) NOT NULL default ' ',
  "AAABGUOK" varchar(8) NOT NULL default ' ',
  "AAABGUOL" varchar(8) NOT NULL default ' ',
  "AAABGUOM" varchar(8) NOT NULL default ' ',
  "AAABGUON" varchar(8) NOT NULL default ' ',
  "AAABGUOO" varchar(8) NOT NULL default ' ',
  "AAABGUOP" varchar(8) NOT NULL default ' ',
  "AAABGUOQ" varchar(8) NOT NULL default ' ',
  "AAABGUOR" varchar(8) NOT NULL default ' ',
  "AAABGUOS" varchar(8) NOT NULL default ' ',
  "AAABGUOT" varchar(8) NOT NULL default ' ',
  "AAABGUOU" varchar(8) NOT NULL default ' ',
  "AAABGUOV" varchar(8) NOT NULL default ' ',
  "AAABGUOW" varchar(8) NOT NULL default ' ',
  "AAABGUOX" varchar(8) NOT NULL default ' ',
  "AAABGUOY" varchar(8) NOT NULL default ' ',
  "AAABGUOZ" varchar(8) NOT NULL default ' ',
  "AAABGUPA" varchar(8) NOT NULL default ' ',
  "AAABGUPB" varchar(8) NOT NULL default ' ',
  "AAABGUPC" varchar(8) NOT NULL default ' ',
  "AAABGUPD" varchar(8) NOT NULL default ' ',
  "AAABGUPE" varchar(8) NOT NULL default ' ',
  "AAABGUPF" varchar(8) NOT NULL default ' ',
  "AAABGUPG" varchar(8) NOT NULL default ' ',
  "AAABGUPH" varchar(8) NOT NULL default ' ',
  "AAABGUPI" varchar(8) NOT NULL default ' ',
  "AAABGUPJ" varchar(8) NOT NULL default ' ',
  "AAABGUPK" varchar(8) NOT NULL default ' ',
  "AAABGUPL" varchar(8) NOT NULL default ' ',
  "AAABGUPM" varchar(8) NOT NULL default ' ',
  "AAABGUPN" varchar(8) NOT NULL default ' ',
  "AAABGUPO" varchar(8) NOT NULL default ' ',
  "AAABGUPP" varchar(8) NOT NULL default ' ',
  "AAABGUPQ" varchar(8) NOT NULL default ' ',
  "AAABGUPR" varchar(8) NOT NULL default ' ',
  "AAABGUPS" varchar(8) NOT NULL default ' ',
  "AAABGUPT" varchar(8) NOT NULL default ' ',
  "AAABGUPU" varchar(8) NOT NULL default ' ',
  "AAABGUPV" varchar(8) NOT NULL default ' ',
  "AAABGUPW" varchar(8) NOT NULL default ' ',
  "AAABGUPX" varchar(8) NOT NULL default ' ',
  "AAABGUPY" varchar(8) NOT NULL default ' ',
  "AAABGUPZ" varchar(8) NOT NULL default ' ',
  "AAABGUQA" varchar(8) NOT NULL default ' ',
  "AAABGUQB" varchar(8) NOT NULL default ' ',
  "AAABGUQC" varchar(8) NOT NULL default ' ',
  "AAABGUQD" varchar(8) NOT NULL default ' ',
  "AAABGUQE" varchar(8) NOT NULL default ' ',
  "AAABGUQF" varchar(8) NOT NULL default ' ',
  "AAABGUQG" varchar(8) NOT NULL default ' ',
  "AAABGUQH" varchar(8) NOT NULL default ' ',
  "AAABGUQI" varchar(8) NOT NULL default ' ',
  "AAABGUQJ" varchar(8) NOT NULL default ' ',
  "AAABGUQK" varchar(8) NOT NULL default ' ',
  "AAABGUQL" varchar(8) NOT NULL default ' ',
  "AAABGUQM" varchar(8) NOT NULL default ' ',
  "AAABGUQN" varchar(8) NOT NULL default ' ',
  "AAABGUQO" varchar(8) NOT NULL default ' ',
  "AAABGUQP" varchar(8) NOT NULL default ' ',
  "AAABGUQQ" varchar(8) NOT NULL default ' ',
  "AAABGUQR" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGWEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUQU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGWEX" varchar(4) NOT NULL default ' ',
  "AAABGUQX" varchar(8) NOT NULL default ' ',
  "AAABGUQY" varchar(8) NOT NULL default ' ',
  "AAABGUQZ" varchar(8) NOT NULL default ' ',
  "AAABGURA" varchar(8) NOT NULL default ' ',
  "AAABGURB" varchar(8) NOT NULL default ' ',
  "AAABGURC" varchar(8) NOT NULL default ' ',
  "AAABGURD" varchar(8) NOT NULL default ' ',
  "AAABGURE" varchar(8) NOT NULL default ' ',
  "AAABGURF" varchar(8) NOT NULL default ' ',
  "AAABGURG" varchar(8) NOT NULL default ' ',
  "AAABGURH" varchar(8) NOT NULL default ' ',
  "AAABGURI" varchar(8) NOT NULL default ' ',
  "AAABGURJ" varchar(8) NOT NULL default ' ',
  "AAABGURK" varchar(8) NOT NULL default ' ',
  "AAABGURL" varchar(8) NOT NULL default ' ',
  "AAABGURM" varchar(8) NOT NULL default ' ',
  "AAABGURN" varchar(8) NOT NULL default ' ',
  "AAABGURO" varchar(8) NOT NULL default ' ',
  "AAABGURP" varchar(8) NOT NULL default ' ',
  "AAABGURQ" varchar(8) NOT NULL default ' ',
  "AAABGURR" varchar(8) NOT NULL default ' ',
  "AAABGURS" varchar(8) NOT NULL default ' ',
  "AAABGURT" varchar(8) NOT NULL default ' ',
  "AAABGURU" varchar(8) NOT NULL default ' ',
  "AAABGURV" varchar(8) NOT NULL default ' ',
  "AAABGURW" varchar(8) NOT NULL default ' ',
  "AAABGURX" varchar(8) NOT NULL default ' ',
  "AAABGURY" varchar(8) NOT NULL default ' ',
  "AAABGURZ" varchar(8) NOT NULL default ' ',
  "AAABGUSA" varchar(8) NOT NULL default ' ',
  "AAABGUSB" varchar(8) NOT NULL default ' ',
  "AAABGUSC" varchar(8) NOT NULL default ' ',
  "AAABGUSD" varchar(8) NOT NULL default ' ',
  "AAABGUSE" varchar(8) NOT NULL default ' ',
  "AAABGUSF" varchar(8) NOT NULL default ' ',
  "AAABGUSG" varchar(8) NOT NULL default ' ',
  "AAABGUSH" varchar(8) NOT NULL default ' ',
  "AAABGUSI" varchar(8) NOT NULL default ' ',
  "AAABGUSJ" varchar(8) NOT NULL default ' ',
  "AAABGUSK" varchar(8) NOT NULL default ' ',
  "AAABGUSL" varchar(8) NOT NULL default ' ',
  "AAABGUSM" varchar(8) NOT NULL default ' ',
  "AAABGUSN" varchar(8) NOT NULL default ' ',
  "AAABGUSO" varchar(8) NOT NULL default ' ',
  "AAABGUSP" varchar(8) NOT NULL default ' ',
  "AAABGUSQ" varchar(8) NOT NULL default ' ',
  "AAABGUSR" varchar(8) NOT NULL default ' ',
  "AAABGUSS" varchar(8) NOT NULL default ' ',
  "AAABGUST" varchar(8) NOT NULL default ' ',
  "AAABGUSU" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGWEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUSX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGWEX" varchar(4) NOT NULL default ' ',
  "AAABGUTA" varchar(8) NOT NULL default ' ',
  "AAABGUTB" varchar(8) NOT NULL default ' ',
  "AAABGUTC" varchar(8) NOT NULL default ' ',
  "AAABGUTD" varchar(8) NOT NULL default ' ',
  "AAABGUTE" varchar(8) NOT NULL default ' ',
  "AAABGUTF" varchar(8) NOT NULL default ' ',
  "AAABGUTG" varchar(8) NOT NULL default ' ',
  "AAABGUTH" varchar(8) NOT NULL default ' ',
  "AAABGUTI" varchar(8) NOT NULL default ' ',
  "AAABGUTJ" varchar(8) NOT NULL default ' ',
  "AAABGUTK" varchar(8) NOT NULL default ' ',
  "AAABGUTL" varchar(8) NOT NULL default ' ',
  "AAABGUTM" varchar(8) NOT NULL default ' ',
  "AAABGUTN" varchar(8) NOT NULL default ' ',
  "AAABGUTO" varchar(8) NOT NULL default ' ',
  "AAABGUTP" varchar(8) NOT NULL default ' ',
  "AAABGUTQ" varchar(8) NOT NULL default ' ',
  "AAABGUTR" varchar(8) NOT NULL default ' ',
  "AAABGUTS" varchar(8) NOT NULL default ' ',
  "AAABGUTT" varchar(8) NOT NULL default ' ',
  "AAABGUTU" varchar(8) NOT NULL default ' ',
  "AAABGUTV" varchar(8) NOT NULL default ' ',
  "AAABGUTW" varchar(8) NOT NULL default ' ',
  "AAABGUTX" varchar(8) NOT NULL default ' ',
  "AAABGUTY" varchar(8) NOT NULL default ' ',
  "AAABGUTZ" varchar(8) NOT NULL default ' ',
  "AAABGUUA" varchar(8) NOT NULL default ' ',
  "AAABGUUB" varchar(8) NOT NULL default ' ',
  "AAABGUUC" varchar(8) NOT NULL default ' ',
  "AAABGUUD" varchar(8) NOT NULL default ' ',
  "AAABGUUE" varchar(8) NOT NULL default ' ',
  "AAABGUUF" varchar(8) NOT NULL default ' ',
  "AAABGUUG" varchar(8) NOT NULL default ' ',
  "AAABGUUH" varchar(8) NOT NULL default ' ',
  "AAABGUUI" varchar(8) NOT NULL default ' ',
  "AAABGUUJ" varchar(8) NOT NULL default ' ',
  "AAABGUUK" varchar(8) NOT NULL default ' ',
  "AAABGUUL" varchar(8) NOT NULL default ' ',
  "AAABGUUM" varchar(8) NOT NULL default ' ',
  "AAABGUUN" varchar(8) NOT NULL default ' ',
  "AAABGUUO" varchar(8) NOT NULL default ' ',
  "AAABGUUP" varchar(8) NOT NULL default ' ',
  "AAABGUUQ" varchar(8) NOT NULL default ' ',
  "AAABGUUR" varchar(8) NOT NULL default ' ',
  "AAABGUUS" varchar(8) NOT NULL default ' ',
  "AAABGUUT" varchar(8) NOT NULL default ' ',
  "AAABGUUU" varchar(8) NOT NULL default ' ',
  "AAABGUUV" varchar(8) NOT NULL default ' ',
  "AAABGUUW" varchar(8) NOT NULL default ' ',
  "AAABGUUX" varchar(8) NOT NULL default ' ',
  "AAABGUUY" varchar(8) NOT NULL default ' ',
  "AAABGUUZ" varchar(8) NOT NULL default ' ',
  "AAABGUVA" varchar(8) NOT NULL default ' ',
  "AAABGUVB" varchar(8) NOT NULL default ' ',
  "AAABGUVC" varchar(8) NOT NULL default ' ',
  "AAABGUVD" varchar(8) NOT NULL default ' ',
  "AAABGUVE" varchar(8) NOT NULL default ' ',
  "AAABGUVF" varchar(8) NOT NULL default ' ',
  "AAABGUVG" varchar(8) NOT NULL default ' ',
  "AAABGUVH" varchar(8) NOT NULL default ' ',
  "AAABGUVI" varchar(8) NOT NULL default ' ',
  "AAABGUVJ" varchar(8) NOT NULL default ' ',
  "AAABGUVK" varchar(8) NOT NULL default ' ',
  "AAABGUVL" varchar(8) NOT NULL default ' ',
  "AAABGUVM" varchar(8) NOT NULL default ' ',
  "AAABGUVN" varchar(8) NOT NULL default ' ',
  "AAABGUVO" varchar(8) NOT NULL default ' ',
  "AAABGUVP" varchar(8) NOT NULL default ' ',
  "AAABGUVQ" varchar(8) NOT NULL default ' ',
  "AAABGUVR" varchar(8) NOT NULL default ' ',
  "AAABGUVS" varchar(8) NOT NULL default ' ',
  "AAABGUVT" varchar(8) NOT NULL default ' ',
  "AAABGUVU" varchar(8) NOT NULL default ' ',
  "AAABGUVV" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGWEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUVY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGWEX" varchar(4) NOT NULL default ' ',
  "AAABGUWB" varchar(8) NOT NULL default ' ',
  "AAABGUWC" varchar(8) NOT NULL default ' ',
  "AAABGUWD" varchar(8) NOT NULL default ' ',
  "AAABGUWE" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGWEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGUWH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGYJY" varchar(3) NOT NULL default ' ',
  "AAABGUWK" varchar(8) NOT NULL default ' ',
  "AAABGUWL" varchar(8) NOT NULL default ' ',
  "AAABGUWM" varchar(8) NOT NULL default ' ',
  "AAABGUWN" varchar(8) NOT NULL default ' ',
  "AAABGUWO" varchar(8) NOT NULL default ' ',
  "AAABGUWP" varchar(8) NOT NULL default ' ',
  "AAABGUWQ" varchar(8) NOT NULL default ' ',
  "AAABGUWR" varchar(8) NOT NULL default ' ',
  "AAABGUWS" varchar(8) NOT NULL default ' ',
  "AAABGUWT" varchar(8) NOT NULL default ' ',
  "AAABGUWU" varchar(8) NOT NULL default ' ',
  "AAABGUWV" varchar(8) NOT NULL default ' ',
  "AAABGUWW" varchar(8) NOT NULL default ' ',
  "AAABGUWX" varchar(8) NOT NULL default ' ',
  "AAABGUWY" varchar(8) NOT NULL default ' ',
  "AAABGUWZ" varchar(8) NOT NULL default ' ',
  "AAABGUXA" varchar(8) NOT NULL default ' ',
  "AAABGUXB" varchar(8) NOT NULL default ' ',
  "AAABGUXC" varchar(8) NOT NULL default ' ',
  "AAABGUXD" varchar(8) NOT NULL default ' ',
  "AAABGUXE" varchar(8) NOT NULL default ' ',
  "AAABGUXF" varchar(8) NOT NULL default ' ',
  "AAABGUXG" varchar(8) NOT NULL default ' ',
  "AAABGUXH" varchar(8) NOT NULL default ' ',
  "AAABGUXI" varchar(8) NOT NULL default ' ',
  "AAABGUXJ" varchar(8) NOT NULL default ' ',
  "AAABGUXK" varchar(8) NOT NULL default ' ',
  "AAABGUXL" varchar(8) NOT NULL default ' ',
  "AAABGUXM" varchar(8) NOT NULL default ' ',
  "AAABGUXN" varchar(8) NOT NULL default ' ',
  "AAABGUXO" varchar(8) NOT NULL default ' ',
  "AAABGUXP" varchar(8) NOT NULL default ' ',
  "AAABGUXQ" varchar(8) NOT NULL default ' ',
  "AAABGUXR" varchar(8) NOT NULL default ' ',
  "AAABGUXS" varchar(8) NOT NULL default ' ',
  "AAABGUXT" varchar(8) NOT NULL default ' ',
  "AAABGUXU" varchar(8) NOT NULL default ' ',
  "AAABGUXV" varchar(8) NOT NULL default ' ',
  "AAABGUXW" varchar(8) NOT NULL default ' ',
  "AAABGUXX" varchar(8) NOT NULL default ' ',
  "AAABGUXY" varchar(8) NOT NULL default ' ',
  "AAABGUXZ" varchar(8) NOT NULL default ' ',
  "AAABGUYA" varchar(8) NOT NULL default ' ',
  "AAABGUYB" varchar(8) NOT NULL default ' ',
  "AAABGUYC" varchar(8) NOT NULL default ' ',
  "AAABGUYD" varchar(8) NOT NULL default ' ',
  "AAABGUYE" varchar(8) NOT NULL default ' ',
  "AAABGUYF" varchar(8) NOT NULL default ' ',
  "AAABGUYG" varchar(8) NOT NULL default ' ',
  "AAABGUYH" varchar(8) NOT NULL default ' ',
  "AAABGUYI" varchar(8) NOT NULL default ' ',
  "AAABGUYJ" varchar(8) NOT NULL default ' ',
  "AAABGUYK" varchar(8) NOT NULL default ' ',
  "AAABGUYL" varchar(8) NOT NULL default ' ',
  "AAABGUYM" varchar(8) NOT NULL default ' ',
  "AAABGUYN" varchar(8) NOT NULL default ' ',
  "AAABGUYO" varchar(8) NOT NULL default ' ',
  "AAABGUYP" varchar(8) NOT NULL default ' ',
  "AAABGUYQ" varchar(8) NOT NULL default ' ',
  "AAABGUYR" varchar(8) NOT NULL default ' ',
  "AAABGUYS" varchar(8) NOT NULL default ' ',
  "AAABGUYT" varchar(8) NOT NULL default ' ',
  "AAABGUYU" varchar(8) NOT NULL default ' ',
  "AAABGUYV" varchar(8) NOT NULL default ' ',
  "AAABGUYW" varchar(8) NOT NULL default ' ',
  "AAABGUYX" varchar(8) NOT NULL default ' ',
  "AAABGUYY" varchar(8) NOT NULL default ' ',
  "AAABGUYZ" varchar(8) NOT NULL default ' ',
  "AAABGUZA" varchar(8) NOT NULL default ' ',
  "AAABGUZB" varchar(8) NOT NULL default ' ',
  "AAABGUZC" varchar(8) NOT NULL default ' ',
  "AAABGUZD" varchar(8) NOT NULL default ' ',
  "AAABGUZE" varchar(8) NOT NULL default ' ',
  "AAABGUZF" varchar(8) NOT NULL default ' ',
  "AAABGUZG" varchar(8) NOT NULL default ' ',
  "AAABGUZH" varchar(8) NOT NULL default ' ',
  "AAABGUZI" varchar(8) NOT NULL default ' ',
  "AAABGUZJ" varchar(8) NOT NULL default ' ',
  "AAABGUZK" varchar(8) NOT NULL default ' ',
  "AAABGUZL" varchar(8) NOT NULL default ' ',
  "AAABGUZM" varchar(8) NOT NULL default ' ',
  "AAABGUZN" varchar(8) NOT NULL default ' ',
  "AAABGUZO" varchar(8) NOT NULL default ' ',
  "AAABGUZP" varchar(8) NOT NULL default ' ',
  "AAABGUZQ" varchar(8) NOT NULL default ' ',
  "AAABGUZR" varchar(8) NOT NULL default ' ',
  "AAABGUZS" varchar(8) NOT NULL default ' ',
  "AAABGUZT" varchar(8) NOT NULL default ' ',
  "AAABGUZU" varchar(8) NOT NULL default ' ',
  "AAABGUZV" varchar(8) NOT NULL default ' ',
  "AAABGUZW" varchar(8) NOT NULL default ' ',
  "AAABGUZX" varchar(8) NOT NULL default ' ',
  "AAABGUZY" varchar(8) NOT NULL default ' ',
  "AAABGUZZ" varchar(8) NOT NULL default ' ',
  "AAABGVAA" varchar(8) NOT NULL default ' ',
  "AAABGVAB" varchar(8) NOT NULL default ' ',
  "AAABGVAC" varchar(8) NOT NULL default ' ',
  "AAABGVAD" varchar(8) NOT NULL default ' ',
  "AAABGVAE" varchar(8) NOT NULL default ' ',
  "AAABGVAF" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGYJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVAI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABLDAZ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABLCPX","AAABLDAZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVAP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGWEX" varchar(4) NOT NULL default ' ',
  "AAABGVAS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGWEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVAV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGVAX" varchar(8) NOT NULL default ' ',
  "AAABGVAY" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVBA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGYLO" varchar(11) NOT NULL default ' ',
  "AAABGVKJ" varchar(8) NOT NULL default '00000000',
  "AAABHMRN" varchar(32) NOT NULL default ' ',
  "AAABLAXK" VARBINARY(255),
  "AAABGYMA" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAABJGWD","AAABGYLO","AAABGVKJ","AAABHMRN"),
  KEY "AAABGVBN" ("AAABHMRN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVBP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABJPDB" varchar(10) NOT NULL default ' ',
  "AAABLHFC" varchar(3) NOT NULL default ' ',
  "AAABHMRN" varchar(32) NOT NULL default ' ',
  "AAABGYMA" VARBINARY(255),
  "AAABKSXJ" int(11) NOT NULL default '0',
  "AAABJGWI" varchar(13) NOT NULL default ' ',
  "AAABGVBY" varchar(11) NOT NULL default ' ',
  "AAABLAXL" varchar(4) NOT NULL default '0000',
  "AAABGVKJ" varchar(8) NOT NULL default '00000000',
  "AAABLCIO" varchar(8) NOT NULL default '00000000',
  "AAABLCIP" varchar(6) NOT NULL default '000000',
  "AAABLDCD" varchar(4) NOT NULL default ' ',
  "AAABLAXH" varchar(4) NOT NULL default ' ',
  "AAABGYJY" varchar(3) NOT NULL default ' ',
  "AAABMDHS" decimal(13,2) NOT NULL default '0.00',
  "AAABLAYD" varchar(5) NOT NULL default ' ',
  "AAABMDHR" decimal(13,2) NOT NULL default '0.00',
  "AAABLBYH" varchar(5) NOT NULL default ' ',
  "AAABLBXX" decimal(13,2) NOT NULL default '0.00',
  "AAABKZUA" varchar(5) NOT NULL default ' ',
  "AAABLAAO" decimal(13,2) NOT NULL default '0.00',
  "AAABLAAI" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWD","AAABJPDB","AAABLHFC","AAABHMRN"),
  KEY "AAABGVCT" ("AAABHMRN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVCV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGVLV" varchar(4) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABGVMC" varchar(3) NOT NULL default '000',
  "AAABGYJY" varchar(3) NOT NULL default ' ',
  "AAABGVEJ" varchar(8) NOT NULL default ' ',
  "AAABLUEO" varchar(1) NOT NULL default ' ',
  "AAABGVDD" varchar(1) NOT NULL default ' ',
  "AAABLQGL" varchar(3) NOT NULL default ' ',
  "AAABGVDF" varchar(8) NOT NULL default ' ',
  "AAABGVDG" varchar(8) NOT NULL default ' ',
  "AAABGVDH" varchar(8) NOT NULL default ' ',
  "AAABGVDI" varchar(8) NOT NULL default ' ',
  "AAABGVDJ" varchar(8) NOT NULL default ' ',
  "AAABGVDK" varchar(8) NOT NULL default ' ',
  "AAABGVDL" varchar(8) NOT NULL default ' ',
  "AAABGVDM" varchar(8) NOT NULL default ' ',
  "AAABGVDN" varchar(1) NOT NULL default '0',
  "AAABGVDO" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGVLV","AAABJGWB","AAABGVMC","AAABGYJY","AAABGVEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVDV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGVLV" varchar(4) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABGVMC" varchar(3) NOT NULL default '000',
  "AAABGYJY" varchar(3) NOT NULL default ' ',
  "AAABGVEJ" varchar(8) NOT NULL default ' ',
  "AAABKJFL" varchar(8) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGVLV","AAABJGWB","AAABGVMC","AAABGYJY","AAABGVEJ","AAABKJFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVEL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGVLV" varchar(4) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABGVMC" varchar(3) NOT NULL default '000',
  "AAABGYJY" varchar(3) NOT NULL default ' ',
  "AAABIWPG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGVLV","AAABJGWB","AAABGVMC","AAABGYJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVEX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGVFN" varchar(1) NOT NULL default ' ',
  "AAABGVFO" varchar(13) NOT NULL default ' ',
  "AAABJGWF" varchar(13) NOT NULL default ' ',
  "AAABJGWE" varchar(10) NOT NULL default ' ',
  "AAABJGWJ" varchar(8) NOT NULL default '00000000',
  "AAABGVFF" varchar(10) NOT NULL default ' ',
  "AAABGVFG" VARBINARY(255) NOT NULL,
  "AAABGVFH" varchar(1) NOT NULL default ' ',
  "AAABGVFI" VARBINARY(255) NOT NULL,
  "AAABGVFJ" varchar(10) NOT NULL default ' ',
  "AAABGVFK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWD","AAABGVFN","AAABGVFO","AAABJGWF","AAABJGWE","AAABJGWJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVFS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGVLV" varchar(4) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABGVGY" varchar(32) NOT NULL default ' ',
  "AAABGVGZ" varchar(8) NOT NULL default '00000000',
  "AAABLAXK" VARBINARY(255) NOT NULL,
  "AAABLAXL" varchar(4) NOT NULL default '0000',
  "AAABGVGA" varchar(1) NOT NULL default ' ',
  "AAABJGWJ" varchar(8) NOT NULL default '00000000',
  "AAABGVGC" varchar(1) NOT NULL default ' ',
  "AAABKJFL" varchar(8) NOT NULL default ' ',
  "AAABGVPV" varchar(8) NOT NULL default '00000000',
  "AAABGVGF" varchar(1) NOT NULL default ' ',
  "AAABGVGG" int(11) NOT NULL default '0',
  "AAABGVGH" varchar(6) NOT NULL default '000000',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEN" decimal(21,2) NOT NULL default '0.00',
  "AAABGWAK" varchar(5) NOT NULL default ' ',
  "AAABGVQU" decimal(21,2) NOT NULL default '0.00',
  "AAABGVGN" decimal(21,2) NOT NULL default '0.00',
  "AAABGVGO" decimal(21,2) NOT NULL default '0.00',
  "AAABGVGP" decimal(21,2) NOT NULL default '0.00',
  "AAABGVGQ" decimal(21,2) NOT NULL default '0.00',
  "AAABGVGR" decimal(21,2) NOT NULL default '0.00',
  "AAABGVGS" decimal(21,2) NOT NULL default '0.00',
  "AAABGVGT" decimal(9,5) NOT NULL default '0.00000',
  "AAABLUEO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGVLV","AAABJGWB","AAABGVGY","AAABGVGZ","AAABLAXK"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVHB" (
  "AAABLAOA" varchar(3) NOT NULL default '000',
  "AAABGVHF" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLAOA","AAABGVHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVHG" (
  "AAABLAOA" varchar(3) NOT NULL default '000',
  "AAABGVHI" varchar(4) NOT NULL default '0000',
  "AAABGVHJ" varchar(4) NOT NULL default '0000',
  "AAABGVHK" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLAOA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVHM" (
  "AAABLAOA" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGVHP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLAOA","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVHS" (
  "AAABLAOA" varchar(3) NOT NULL default '000',
  "AAABJUQY" varchar(3) NOT NULL default '000',
  "AAABGVMM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLAOA","AAABJUQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVHY" (
  "AAABGVMM" varchar(30) NOT NULL default ' ',
  "AAABLAXL" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABGVMM","AAABLAXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVID" (
  "AAABLMNC" varchar(4) NOT NULL default '0000',
  "AAABGVIF" varchar(1) NOT NULL default '0',
  "AAABGVIG" varchar(1) NOT NULL default '0',
  "AAABGZEM" varchar(30) NOT NULL default ' ',
  "AAABGVQU" varchar(30) NOT NULL default ' ',
  "AAABGZEN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVIL" (
  "AAABGVPJ" varchar(4) NOT NULL default ' ',
  "AAABGVIN" varchar(4) NOT NULL default '0000',
  "AAABGVIO" varchar(4) NOT NULL default '0000',
  "AAABGVIP" varchar(4) NOT NULL default '0000',
  "AAABGVIQ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABGVPJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVIS" (
  "AAABGVPJ" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGVIV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABGVPJ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVIY" (
  "AAABGWCO" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGVJB" varchar(5) NOT NULL default ' ',
  "AAABGVJC" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABGWCO","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVJF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGVLV" varchar(4) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABJGWC" varchar(4) NOT NULL default '0000',
  "AAABJGWF" varchar(13) NOT NULL default ' ',
  "AAABJGWI" varchar(13) NOT NULL default ' ',
  "AAABJGWH" varchar(10) NOT NULL default ' ',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABGVMC" varchar(3) NOT NULL default '000',
  "AAABLAXK" VARBINARY(255) NOT NULL,
  "AAABGVPD" varchar(10) NOT NULL default '0000000000',
  "AAABGWDK" varchar(8) NOT NULL default '00000000',
  "AAABKJFL" varchar(8) NOT NULL default ' ',
  "AAABKZVA" varchar(4) NOT NULL default ' ',
  "AAABGVPJ" varchar(4) NOT NULL default ' ',
  "AAABGZKC" decimal(22,6) NOT NULL default '0.000000',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGVQU" decimal(21,2) NOT NULL default '0.00',
  "AAABGWAK" varchar(5) NOT NULL default ' ',
  "AAABGXTR" varchar(1) NOT NULL default ' ',
  "AAABGXTQ" varchar(4) NOT NULL default '0000',
  "AAABGVKF" varchar(10) NOT NULL default ' ',
  "AAABGVKG" varchar(10) NOT NULL default ' ',
  "AAABGVKH" varchar(10) NOT NULL default ' ',
  "AAABGVKI" varchar(4) NOT NULL default '0000',
  "AAABGVKJ" varchar(8) NOT NULL default '00000000',
  "AAABGYMA" VARBINARY(255),
  "AAABGWCO" varchar(1) NOT NULL default ' ',
  "AAABGVKM" varchar(1) NOT NULL default ' ',
  "AAABGWEX" varchar(4) NOT NULL default ' ',
  "AAABGVKO" varchar(10) NOT NULL default ' ',
  "AAABGVKP" varchar(1) NOT NULL default '0',
  "AAABGVKQ" varchar(1) NOT NULL default ' ',
  "AAABHKBR" varchar(10) NOT NULL default ' ',
  "AAABIDUS" varchar(2) NOT NULL default '00',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABKEVT" varchar(10) NOT NULL default ' ',
  "AAABGVKV" varchar(1) NOT NULL default ' ',
  "AAABGVKW" varchar(8) NOT NULL default '00000000',
  "AAABGVKX" varchar(8) NOT NULL default '00000000',
  "AAABLAXL" varchar(4) NOT NULL default '0000',
  "AAABGVKZ" varchar(60) NOT NULL default ' ',
  "AAABGVLA" varchar(60) NOT NULL default ' ',
  "AAABGVLB" varchar(60) NOT NULL default ' ',
  "AAABGVLC" varchar(15) NOT NULL default ' ',
  "AAABGVLD" varchar(60) NOT NULL default ' ',
  "AAABGVLE" varchar(15) NOT NULL default ' ',
  "AAABGVLF" varchar(30) NOT NULL default ' ',
  "AAABGVLG" varchar(60) NOT NULL default ' ',
  "AAABGVLH" varchar(60) NOT NULL default ' ',
  "AAABGVLI" varchar(60) NOT NULL default ' ',
  "AAABGVLJ" varchar(60) NOT NULL default ' ',
  "AAABGVLK" varchar(60) NOT NULL default ' ',
  "AAABGVLL" varchar(60) NOT NULL default ' ',
  "AAABGVLM" varchar(30) NOT NULL default ' ',
  "AAABGVLN" varchar(30) NOT NULL default ' ',
  "AAABGVLO" varchar(30) NOT NULL default ' ',
  "AAABGVLP" varchar(30) NOT NULL default ' ',
  "AAABGVLQ" varchar(30) NOT NULL default ' ',
  "AAABGVLR" varchar(4) NOT NULL default ' ',
  "AAABGVLS" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABJGWD","AAABGVLV","AAABJGWB","AAABJGWC","AAABJGWF","AAABJGWI","AAABJGWH","AAABLCAK","AAABGVMC","AAABLAXK"(255),"AAABGVPD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVMF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGVMX" varchar(32) NOT NULL default ' ',
  "AAABGVMY" int(11) NOT NULL default '0',
  "AAABGZFQ" varchar(32) NOT NULL default ' ',
  "AAABGVMK" int(11) NOT NULL default '0',
  "AAABGWEX" varchar(4) NOT NULL default ' ',
  "AAABGVMM" varchar(30) NOT NULL default ' ',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEN" decimal(21,2) NOT NULL default '0.00',
  "AAABGWAK" varchar(5) NOT NULL default ' ',
  "AAABGVQU" decimal(21,2) NOT NULL default '0.00',
  "AAABGVMS" varchar(4) NOT NULL default ' ',
  "AAABGVMT" varchar(8) NOT NULL default ' ',
  "AAABGXTR" varchar(1) NOT NULL default ' ',
  "AAABGXTQ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABGVMX","AAABGVMY"),
  KEY "AAABGVMZ" ("AAABMRIL","AAABGZFQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVNC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABGZIR" varchar(32) NOT NULL default ' ',
  "AAABGZIS" varchar(32) NOT NULL default ' ',
  "AAABGVNH" varchar(1) NOT NULL default ' ',
  "AAABGVNT" VARBINARY(255),
  "AAABGVNW" VARBINARY(255),
  "AAABGZKF" varchar(8) NOT NULL default '00000000',
  "AAABGWEK" varchar(4) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABJOTX" varchar(8) NOT NULL default '00000000',
  "AAABGXSI" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"),
  KEY "AAABGVNR" ("AAABMRIL","AAABGVNT"(255)),
  KEY "AAABGVNU" ("AAABMRIL","AAABGVNW"(255),"AAABJGWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVNY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABGYMA" VARBINARY(255),
  "AAABGWCO" varchar(1) NOT NULL default ' ',
  "AAABGVZV" varchar(32) NOT NULL default ' ',
  "AAABGVZF" varchar(32) NOT NULL default ' ',
  "AAABGZIS" varchar(32) NOT NULL default ' ',
  "AAABGZIR" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVOJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABGVZF" varchar(32) NOT NULL default ' ',
  "AAABGVZV" varchar(32) NOT NULL default ' ',
  "AAABGZFR" varchar(32) NOT NULL default ' ',
  "AAABGZFQ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"),
  KEY "AAABGVOS" ("AAABMRIL","AAABGVZV","AAABGZFQ"),
  KEY "AAABGVOW" ("AAABMRIL","AAABGZFQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVOZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGVZV" varchar(32) NOT NULL default ' ',
  "AAABGWDE" int(11) NOT NULL default '0',
  "AAABGVPD" int(11) NOT NULL default '0',
  "AAABGXTR" varchar(1) NOT NULL default ' ',
  "AAABGXTQ" varchar(4) NOT NULL default '0000',
  "AAABKJFL" varchar(8) NOT NULL default ' ',
  "AAABGVPH" double NOT NULL default '0',
  "AAABKZVA" varchar(4) NOT NULL default ' ',
  "AAABGVPJ" varchar(4) NOT NULL default ' ',
  "AAABGZKC" decimal(22,6) NOT NULL default '0.000000',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABGZEN" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGVQU" decimal(21,2) NOT NULL default '0.00',
  "AAABGWAK" varchar(5) NOT NULL default ' ',
  "AAABJQXA" varchar(8) NOT NULL default '00000000',
  "AAABGVPT" varchar(1) NOT NULL default '0',
  "AAABGVPU" varchar(8) NOT NULL default '00000000',
  "AAABGVPV" varchar(8) NOT NULL default '00000000',
  "AAABGVPW" varchar(8) NOT NULL default '00000000',
  "AAABGYMA" VARBINARY(255),
  "AAABGWCO" varchar(1) NOT NULL default ' ',
  "AAABGZIS" varchar(32) NOT NULL default ' ',
  "AAABGZFQ" varchar(32) NOT NULL default ' ',
  "AAABGWDK" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABGVZV","AAABGWDE"),
  KEY "AAABGVQF" ("AAABMRIL","AAABGZFQ","AAABGWDK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVQJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGVQW" varchar(32) NOT NULL default ' ',
  "AAABLMNC" varchar(4) NOT NULL default '0000',
  "AAABGZKC" decimal(22,6) NOT NULL default '0.000000',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEN" decimal(21,2) NOT NULL default '0.00',
  "AAABGWAK" varchar(5) NOT NULL default ' ',
  "AAABGVQU" decimal(21,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABGVQW","AAABLMNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHMRN" varchar(32) NOT NULL default ' ',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGXUJ" varchar(8) NOT NULL default '00000000',
  "AAABGVRD" varchar(2) NOT NULL default '00',
  "AAABGVRE" varchar(1) NOT NULL default '0',
  "AAABLZDP" varchar(12) NOT NULL default ' ',
  "AAABLCIO" varchar(8) NOT NULL default '00000000',
  "AAABLCIP" varchar(6) NOT NULL default '000000',
  "AAABLCIQ" varchar(10) NOT NULL default ' ',
  "AAABLCIS" varchar(12) NOT NULL default ' ',
  "AAABLCIT" varchar(8) NOT NULL default '00000000',
  "AAABLCIU" varchar(6) NOT NULL default '000000',
  "AAABLCIV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHMRN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVRP" (
  "AAABJUQY" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABJUQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVRT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGWBG" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABLAXK" VARBINARY(255) NOT NULL,
  PRIMARY KEY ("AAABMRIL","AAABGWBG","AAABJGWD","AAABJGWB","AAABLAXK"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVSE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLAXK" VARBINARY(255) NOT NULL,
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABJGWC" varchar(4) NOT NULL default '0000',
  "AAABJGWF" varchar(13) NOT NULL default ' ',
  "AAABJGWE" varchar(10) NOT NULL default ' ',
  "AAABJGWH" varchar(10) NOT NULL default ' ',
  "AAABKZRY" VARBINARY(255),
  "AAABJGWI" varchar(13) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAABGWBG" varchar(3) NOT NULL default '000',
  "AAABJGWJ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLAXK"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVSU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWJ" varchar(8) NOT NULL default '00000000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABJGWC" varchar(4) NOT NULL default '0000',
  "AAABJGWF" varchar(13) NOT NULL default ' ',
  "AAABKZRY" VARBINARY(255) NOT NULL,
  "AAABGWDW" varchar(10) NOT NULL default ' ',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGWAK" varchar(5) NOT NULL default ' ',
  "AAABGZKC" decimal(22,6) NOT NULL default '0.000000',
  "AAABGVWN" decimal(21,2) NOT NULL default '0.00',
  "AAABGVWO" decimal(21,2) NOT NULL default '0.00',
  "AAABGVWQ" decimal(21,2) NOT NULL default '0.00',
  "AAABGVWR" decimal(21,2) NOT NULL default '0.00',
  "AAABGVTK" decimal(21,2) NOT NULL default '0.00',
  "AAABGVTL" decimal(21,2) NOT NULL default '0.00',
  "AAABGVYF" varchar(1) NOT NULL default ' ',
  "AAABLAXK" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAABJGWJ","AAABJGWD","AAABJGWB","AAABJGWC","AAABJGWF","AAABKZRY"(255),"AAABGWDW"),
  KEY "AAABGVTW" ("AAABMRIL","AAABLAXK"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVTZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWJ" varchar(8) NOT NULL default '00000000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABJGWC" varchar(4) NOT NULL default '0000',
  "AAABJGWI" varchar(13) NOT NULL default ' ',
  "AAABJMEG" varchar(4) NOT NULL default '0000',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGWAK" varchar(5) NOT NULL default ' ',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABGVWN" decimal(21,2) NOT NULL default '0.00',
  "AAABGVWO" decimal(21,2) NOT NULL default '0.00',
  "AAABGVWT" decimal(21,2) NOT NULL default '0.00',
  "AAABGVWU" decimal(21,2) NOT NULL default '0.00',
  "AAABGVWW" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXD" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXE" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXG" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXH" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXJ" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXK" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXM" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXN" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXP" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXQ" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXS" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXZ" decimal(21,2) NOT NULL default '0.00',
  "AAABGVYA" decimal(21,2) NOT NULL default '0.00',
  "AAABGVYF" varchar(1) NOT NULL default ' ',
  "AAABLAXK" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAABJGWJ","AAABJGWD","AAABJGWB","AAABJGWC","AAABJGWI"),
  KEY "AAABGVVL" ("AAABMRIL","AAABLAXK"(255)),
  KEY "AAABGVVO" ("AAABMRIL","AAABJGWD","AAABJGWI"),
  KEY "AAABGVVS" ("AAABMRIL","AAABJGWD","AAABJGWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVVW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWJ" varchar(8) NOT NULL default '00000000',
  "AAABGVYJ" varchar(3) NOT NULL default ' ',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABJGWC" varchar(4) NOT NULL default '0000',
  "AAABJGWF" varchar(13) NOT NULL default ' ',
  "AAABKZRY" VARBINARY(255) NOT NULL,
  "AAABJGWE" varchar(10) NOT NULL default ' ',
  "AAABJGWH" varchar(10) NOT NULL default ' ',
  "AAABLIMM" varchar(10) NOT NULL default ' ',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGWAK" varchar(5) NOT NULL default ' ',
  "AAABGZKC" decimal(22,6) NOT NULL default '0.000000',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABGVWN" decimal(21,2) NOT NULL default '0.00',
  "AAABGVWO" decimal(21,2) NOT NULL default '0.00',
  "AAABGVWP" decimal(21,2) NOT NULL default '0.00',
  "AAABGVWQ" decimal(21,2) NOT NULL default '0.00',
  "AAABGVWR" decimal(21,2) NOT NULL default '0.00',
  "AAABGVWS" decimal(21,2) NOT NULL default '0.00',
  "AAABGVWT" decimal(21,2) NOT NULL default '0.00',
  "AAABGVWU" decimal(21,2) NOT NULL default '0.00',
  "AAABGVWV" decimal(21,2) NOT NULL default '0.00',
  "AAABGVWW" decimal(21,2) NOT NULL default '0.00',
  "AAABGVWX" decimal(21,2) NOT NULL default '0.00',
  "AAABGVWY" decimal(21,2) NOT NULL default '0.00',
  "AAABGVWZ" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXA" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXB" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXC" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXD" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXE" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXF" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXG" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXH" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXI" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXJ" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXK" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXL" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXM" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXN" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXO" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXP" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXQ" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXR" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXS" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXT" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXU" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXV" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXW" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXX" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXY" decimal(21,2) NOT NULL default '0.00',
  "AAABGVXZ" decimal(21,2) NOT NULL default '0.00',
  "AAABGVYA" decimal(21,2) NOT NULL default '0.00',
  "AAABGVYB" decimal(21,2) NOT NULL default '0.00',
  "AAABGVYC" decimal(21,2) NOT NULL default '0.00',
  "AAABGVYD" decimal(21,2) NOT NULL default '0.00',
  "AAABGVYE" decimal(21,2) NOT NULL default '0.00',
  "AAABGVYF" varchar(1) NOT NULL default ' ',
  "AAABLAXK" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAABJGWJ","AAABGVYJ","AAABJGWD","AAABJGWB","AAABJGWC","AAABJGWF","AAABKZRY"(255),"AAABJGWE","AAABJGWH","AAABLIMM"),
  KEY "AAABGVYS" ("AAABMRIL","AAABLAXK"(255)),
  KEY "AAABGVYV" ("AAABMRIL","AAABJGWD","AAABJGWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVYZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABGZFQ" varchar(32) NOT NULL default ' ',
  "AAABGZFR" varchar(32) NOT NULL default ' ',
  "AAABGVZV" varchar(32) NOT NULL default ' ',
  "AAABGVZF" varchar(32) NOT NULL default ' ',
  "AAABGVZS" varchar(32) NOT NULL default ' ',
  "AAABGVZH" varchar(32) NOT NULL default ' ',
  "AAABJOTX" varchar(8) NOT NULL default '00000000',
  "AAABGXSI" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"),
  KEY "AAABGVZM" ("AAABMRIL","AAABGZFQ","AAABGZKH"),
  KEY "AAABGVZQ" ("AAABMRIL","AAABGVZS"),
  KEY "AAABGVZT" ("AAABMRIL","AAABGVZV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGVZW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABHMRN" varchar(32) NOT NULL default ' ',
  "AAABGXXW" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWD","AAABHMRN","AAABGXXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABGWEX" varchar(4) NOT NULL default ' ',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGWAK" varchar(5) NOT NULL default ' ',
  "AAABGWAL" varchar(1) NOT NULL default ' ',
  "AAABLBOZ" varchar(2) NOT NULL default ' ',
  "AAABLANX" varchar(2) NOT NULL default ' ',
  "AAABLZDP" varchar(12) NOT NULL default ' ',
  "AAABLCIO" varchar(8) NOT NULL default '00000000',
  "AAABLCIP" varchar(6) NOT NULL default '000000',
  "AAABLCIQ" varchar(10) NOT NULL default ' ',
  "AAABLCIS" varchar(12) NOT NULL default ' ',
  "AAABLCIT" varchar(8) NOT NULL default '00000000',
  "AAABLCIU" varchar(6) NOT NULL default '000000',
  "AAABLCIV" varchar(10) NOT NULL default ' ',
  "AAABGWBC" varchar(32) NOT NULL default ' ',
  "AAABGWAX" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"),
  KEY "AAABGWBA" ("AAABMRIL","AAABGWBC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWBD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABGWBG" varchar(3) NOT NULL default '000',
  "AAABLAXL" varchar(4) NOT NULL default '0000',
  "AAABGWDK" varchar(8) NOT NULL default '00000000',
  "AAABLAXK" VARBINARY(255),
  "AAABJOTX" varchar(8) NOT NULL default '00000000',
  "AAABGXSI" varchar(6) NOT NULL default '000000',
  "AAABGWDN" varchar(1) NOT NULL default ' ',
  "AAABGWCO" varchar(1) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABGWEK" varchar(4) NOT NULL default ' ',
  "AAABGWBQ" VARBINARY(255),
  "AAABGWBR" varchar(8) NOT NULL default '00000000',
  "AAABGWBS" varchar(2) NOT NULL default '00',
  "AAABGWBT" varchar(8) NOT NULL default '00000000',
  "AAABGWBU" varchar(2) NOT NULL default '00',
  "AAABGWBV" varchar(1) NOT NULL default ' ',
  "AAABGZIS" varchar(32) NOT NULL default ' ',
  "AAABGZIR" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"),
  KEY "AAABGWCA" ("AAABMRIL","AAABLAXK"(255),"AAABJGWB"),
  KEY "AAABGWCE" ("AAABMRIL","AAABJGWB","AAABGWEK"),
  KEY "AAABGWCI" ("AAABMRIL","AAABLAXL","AAABJGWB"),
  KEY "AAABGWCM" ("AAABMRIL","AAABGWCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWCP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGWEK" varchar(4) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABLAXK" VARBINARY(255) NOT NULL,
  "AAABGWDE" int(11) NOT NULL default '0',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABJOTX" varchar(8) NOT NULL default '00000000',
  "AAABGXSI" varchar(6) NOT NULL default '000000',
  "AAABGWCY" varchar(8) NOT NULL default '00000000',
  "AAABGWCZ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABGWEK","AAABJGWB","AAABLAXK"(255),"AAABGWDE","AAABGZKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWDG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLAXK" VARBINARY(255) NOT NULL,
  "AAABKSXJ" int(11) NOT NULL default '0',
  "AAABGWDK" varchar(8) NOT NULL default '00000000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABJMEG" varchar(4) NOT NULL default '0000',
  "AAABGWDN" varchar(1) NOT NULL default ' ',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABGYJY" varchar(3) NOT NULL default ' ',
  "AAABGZKC" decimal(22,6) NOT NULL default '0.000000',
  "AAABJGWF" varchar(13) NOT NULL default ' ',
  "AAABJGWE" varchar(10) NOT NULL default ' ',
  "AAABJGWH" varchar(10) NOT NULL default ' ',
  "AAABLIMM" varchar(10) NOT NULL default ' ',
  "AAABGWDW" varchar(10) NOT NULL default ' ',
  "AAABJGWI" varchar(13) NOT NULL default ' ',
  "AAABMLPV" varchar(12) NOT NULL default ' ',
  "AAABJOTX" varchar(8) NOT NULL default '00000000',
  "AAABGXSI" varchar(6) NOT NULL default '000000',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  "AAABGXTR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLAXK"(255),"AAABKSXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWEG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABLAXK" VARBINARY(255),
  "AAABGWEK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"),
  UNIQUE KEY "AAABGWEN" ("AAABMRIL","AAABLAXK"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGWEX" varchar(4) NOT NULL default ' ',
  "AAABGWEY" varchar(8) NOT NULL default ' ',
  "AAABGWEZ" varchar(2) NOT NULL default '00',
  "AAABGWEV" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGWEX","AAABGWEY","AAABGWEZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWFA" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABILJC" varchar(20) NOT NULL default ' ',
  "AAABGWHT" varchar(3) NOT NULL default '000',
  "AAABGWMU" varchar(1) NOT NULL default ' ',
  "AAABHBRF" varchar(1) NOT NULL default ' ',
  "AAABGWHO" varchar(1) NOT NULL default ' ',
  "AAABGWHP" varchar(1) NOT NULL default ' ',
  "AAABGWJT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABILJC","AAABGWHT"),
  KEY "AAABGWFM" ("AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWFO" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABILJC" varchar(20) NOT NULL default ' ',
  "AAABGWHT" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  "AAABGWMU" varchar(1) NOT NULL default ' ',
  "AAABHBRF" varchar(1) NOT NULL default ' ',
  "AAABGWHO" varchar(1) NOT NULL default ' ',
  "AAABGWHP" varchar(1) NOT NULL default ' ',
  "AAABGWJT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABILJC","AAABGWHT"),
  KEY "AAABGWGB" ("AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWGD" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABILJC" varchar(20) NOT NULL default ' ',
  "AAABGWHT" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  "AAABGWMU" varchar(1) NOT NULL default ' ',
  "AAABHBRF" varchar(1) NOT NULL default ' ',
  "AAABGWHO" varchar(1) NOT NULL default ' ',
  "AAABGWHP" varchar(1) NOT NULL default ' ',
  "AAABGWJT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABILJC","AAABGWHT"),
  KEY "AAABGWGQ" ("AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWGS" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABILJC" varchar(20) NOT NULL default ' ',
  "AAABGWHT" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  "AAABGWMU" varchar(1) NOT NULL default ' ',
  "AAABHBRF" varchar(1) NOT NULL default ' ',
  "AAABGWHO" varchar(1) NOT NULL default ' ',
  "AAABGWHP" varchar(1) NOT NULL default ' ',
  "AAABGWJT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABILJC","AAABGWHT"),
  KEY "AAABGWHF" ("AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWHH" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABILJC" varchar(20) NOT NULL default ' ',
  "AAABGWHT" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(255) NOT NULL default ' ',
  "AAABGWMU" varchar(1) NOT NULL default ' ',
  "AAABHBRF" varchar(1) NOT NULL default ' ',
  "AAABGWHO" varchar(1) NOT NULL default ' ',
  "AAABGWHP" varchar(1) NOT NULL default ' ',
  "AAABGWJT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABILJC","AAABGWHT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWHU" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGWIF" varchar(20) NOT NULL default ' ',
  "AAABGWIG" varchar(1) NOT NULL default '0',
  "AAABGWIH" varchar(3) NOT NULL default '000',
  "AAABHJRW" varchar(2) NOT NULL default ' ',
  "AAABJUQY" varchar(2) NOT NULL default '00',
  "AAABGWIM" varchar(20) NOT NULL default ' ',
  "AAABGWIN" varchar(1) NOT NULL default '0',
  "AAABGWIO" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMLQA","AAABGWIF","AAABGWIG","AAABGWIH","AAABHJRW","AAABJUQY"),
  KEY "AAABGWIK" ("AAABMLQA","AAABGWIM","AAABGWIN","AAABGWIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWIP" (
  "AAABGWXD" varchar(1) NOT NULL default ' ',
  "AAABGWXE" varchar(1) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABGWIT" varchar(1) NOT NULL default ' ',
  "AAABGWIU" varchar(1) NOT NULL default ' ',
  "AAABGWIV" varchar(1) NOT NULL default ' ',
  "AAABGWIW" varchar(1) NOT NULL default ' ',
  "AAABGWIX" varchar(1) NOT NULL default ' ',
  "AAABGWIY" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLMKH" varchar(8) NOT NULL default '00000000',
  "AAABLIRO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABGWXD","AAABGWXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWJE" (
  "AAABGWXD" varchar(1) NOT NULL default ' ',
  "AAABGWXE" varchar(1) NOT NULL default ' ',
  "AAABGWWU" varchar(20) NOT NULL default ' ',
  "AAABGWWV" varchar(1) NOT NULL default '0',
  "AAABGWWW" varchar(3) NOT NULL default '000',
  "AAABGWXI" varchar(20) NOT NULL default ' ',
  "AAABGWXJ" varchar(1) NOT NULL default '0',
  "AAABGWXK" varchar(3) NOT NULL default '000',
  "AAABHJRW" varchar(2) NOT NULL default ' ',
  "AAABJUQY" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABGWVP" varchar(8) NOT NULL default '00000000',
  "AAABGWVQ" varchar(6) NOT NULL default '000000',
  "AAABGWJT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGWXD","AAABGWXE","AAABGWWU","AAABGWWV","AAABGWWW","AAABGWXI","AAABGWXJ","AAABGWXK","AAABHJRW","AAABJUQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWKE" (
  "AAABGWXD" varchar(1) NOT NULL default ' ',
  "AAABGWXE" varchar(1) NOT NULL default ' ',
  "AAABKIZK" varchar(10) NOT NULL default ' ',
  "AAABMEGE" varchar(10) NOT NULL default ' ',
  "AAABGWKJ" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABGWXD","AAABGWXE","AAABKIZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWKN" (
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABMMJT" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHGN","AAABMODI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWKU" (
  "AAABGWXD" varchar(1) NOT NULL default ' ',
  "AAABGWXE" varchar(1) NOT NULL default ' ',
  "AAABKIZK" varchar(10) NOT NULL default ' ',
  "AAABGWVX" varchar(6) NOT NULL default '000000',
  "AAABMRFO" varchar(4) NOT NULL default ' ',
  "AAABGWLA" int(11) NOT NULL default '0',
  "AAABGWLB" varchar(12) NOT NULL default ' ',
  "AAABGWLC" varchar(8) NOT NULL default '00000000',
  "AAABGWLD" varchar(6) NOT NULL default '000000',
  "AAABGWLE" varchar(80) NOT NULL default ' ',
  "AAABMKKB" varchar(8) NOT NULL default '00000000',
  "AAABLTLN" varchar(6) NOT NULL default '000000',
  "AAABGWLH" varchar(12) NOT NULL default ' ',
  "AAABGWLI" varchar(8) NOT NULL default '00000000',
  "AAABGWLJ" varchar(6) NOT NULL default '000000',
  "AAABGWLK" varchar(80) NOT NULL default ' ',
  "AAABHJBM" varchar(32) NOT NULL default ' ',
  "AAABHJBN" varchar(8) NOT NULL default ' ',
  "AAABKAIY" varchar(10) NOT NULL default '0000000000',
  "AAABILHK" varchar(4) NOT NULL default ' ',
  "AAABGWVR" varchar(1) NOT NULL default ' ',
  "AAABGWVS" varchar(1) NOT NULL default ' ',
  "AAABGWVT" varchar(1) NOT NULL default ' ',
  "AAABLRFC" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABGWXD","AAABGWXE","AAABKIZK","AAABGWVX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWLX" (
  "AAABGWXD" varchar(1) NOT NULL default ' ',
  "AAABGWXE" varchar(1) NOT NULL default ' ',
  "AAABGWWU" varchar(20) NOT NULL default ' ',
  "AAABGWWV" varchar(1) NOT NULL default '0',
  "AAABGWWW" varchar(3) NOT NULL default '000',
  "AAABHJRW" varchar(2) NOT NULL default ' ',
  "AAABJUQY" varchar(3) NOT NULL default '000',
  "AAABGWXI" varchar(20) NOT NULL default ' ',
  "AAABGWXJ" varchar(1) NOT NULL default '0',
  "AAABGWXK" varchar(3) NOT NULL default '000',
  "AAABGWUG" varchar(1) NOT NULL default ' ',
  "AAABGWUH" varchar(3) NOT NULL default '000',
  "AAABGWUI" varchar(3) NOT NULL default '000',
  "AAABGWUJ" varchar(1) NOT NULL default '0',
  "AAABKNBX" varchar(2) NOT NULL default ' ',
  "AAABGWUL" varchar(3) NOT NULL default '000',
  "AAABGWUM" varchar(2) NOT NULL default '00',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABGWVP" varchar(8) NOT NULL default '00000000',
  "AAABGWUP" varchar(1) NOT NULL default ' ',
  "AAABGWMS" varchar(1) NOT NULL default ' ',
  "AAABGWMT" varchar(1) NOT NULL default ' ',
  "AAABGWMU" varchar(1) NOT NULL default ' ',
  "AAABGWMV" varchar(2) NOT NULL default '00',
  "AAABGWMW" varchar(1) NOT NULL default ' ',
  "AAABGWMX" varchar(1) NOT NULL default ' ',
  "AAABGWMY" varchar(1) NOT NULL default ' ',
  "AAABJHGJ" varchar(1) NOT NULL default ' ',
  "AAABJHGK" varchar(1) NOT NULL default ' ',
  "AAABIKRS" varchar(1) NOT NULL default ' ',
  "AAABIKRT" varchar(1) NOT NULL default ' ',
  "AAABIKRU" varchar(1) NOT NULL default ' ',
  "AAABGWNE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGWXD","AAABGWXE","AAABGWWU","AAABGWWV","AAABGWWW","AAABHJRW","AAABJUQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWNM" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABGWXD" varchar(1) NOT NULL default ' ',
  "AAABGWXE" varchar(1) NOT NULL default ' ',
  "AAABLMKH" varchar(8) NOT NULL default '00000000',
  "AAABLIRO" varchar(6) NOT NULL default '000000',
  "AAABGWNS" varchar(1) NOT NULL default ' ',
  "AAABGWNT" varchar(12) NOT NULL default ' ',
  "AAABIXVY" varchar(1) NOT NULL default ' ',
  "AAABGWNV" varchar(8) NOT NULL default '00000000',
  "AAABGWNW" varchar(8) NOT NULL default '00000000',
  "AAABGWNX" varchar(1) NOT NULL default ' ',
  "AAABGWNY" varchar(1) NOT NULL default ' ',
  "AAABGWNZ" varchar(5) NOT NULL default ' ',
  "AAABGWOA" varchar(5) NOT NULL default ' ',
  "AAABLPWW" varchar(132) NOT NULL default ' ',
  "AAABILRJ" varchar(132) NOT NULL default ' ',
  "AAABGWOD" int(11) NOT NULL default '0',
  "AAABGWOE" varchar(1) NOT NULL default ' ',
  "AAABGWOF" varchar(2) NOT NULL default ' ',
  "AAABGWOG" varchar(1) NOT NULL default ' ',
  "AAABGWOH" varchar(5) NOT NULL default ' ',
  "AAABGWOI" varchar(5) NOT NULL default ' ',
  "AAABGWOJ" varchar(1) NOT NULL default ' ',
  "AAABGWOK" varchar(5) NOT NULL default ' ',
  "AAABGWOL" varchar(5) NOT NULL default ' ',
  "AAABGWOM" varchar(1) NOT NULL default ' ',
  "AAABGWON" varchar(5) NOT NULL default ' ',
  "AAABGWOO" varchar(5) NOT NULL default ' ',
  "AAABGWOP" varchar(1) NOT NULL default ' ',
  "AAABGWOQ" varchar(1) NOT NULL default ' ',
  "AAABGWOR" varchar(1) NOT NULL default ' ',
  "AAABGWOS" varchar(1) NOT NULL default ' ',
  "AAABGWOT" varchar(1) NOT NULL default ' ',
  "AAABGWOU" varchar(12) NOT NULL default ' ',
  "AAABGWOV" varchar(1) NOT NULL default ' ',
  "AAABGWOW" varchar(1) NOT NULL default ' ',
  "AAABGWOX" varchar(75) NOT NULL default ' ',
  "AAABGWOY" varchar(1) NOT NULL default ' ',
  "AAABGWOZ" varchar(1) NOT NULL default ' ',
  "AAABGWPA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEN","AAABGWXD","AAABGWXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWPE" (
  "AAABGWXG" varchar(22) NOT NULL default ' ',
  "AAABLPJD" varchar(4) NOT NULL default ' ',
  "AAABLGCM" varchar(70) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABGWWO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGWXG"),
  KEY "AAABGWPL" ("AAABLPJD","AAABLGCM","AAABIRUN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWPP" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJTAQ" varchar(20) NOT NULL default ' ',
  "AAABGWQE" varchar(4) NOT NULL default ' ',
  "AAABJVWR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABJTAQ","AAABGWQE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWPX" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJTAQ" varchar(20) NOT NULL default ' ',
  "AAABGWQE" varchar(3) NOT NULL default ' ',
  "AAABJVWR" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABJTAQ","AAABGWQE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWQF" (
  "AAABGWXD" varchar(1) NOT NULL default ' ',
  "AAABGWXE" varchar(1) NOT NULL default ' ',
  "AAABGWQU" varchar(8) NOT NULL default '00000000',
  "AAABGWQV" varchar(12) NOT NULL default ' ',
  "AAABGWQK" int(11) NOT NULL default '0',
  "AAABGWQL" int(11) NOT NULL default '0',
  "AAABGWQM" int(11) NOT NULL default '0',
  "AAABGWQN" int(11) NOT NULL default '0',
  "AAABGWQO" int(11) NOT NULL default '0',
  "AAABGWQP" int(11) NOT NULL default '0',
  "AAABGWQQ" int(11) NOT NULL default '0',
  "AAABGWQR" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABGWXD","AAABGWXE","AAABGWQU","AAABGWQV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWQW" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABGWXD" varchar(1) NOT NULL default ' ',
  "AAABGWXE" varchar(1) NOT NULL default ' ',
  "AAABGWRA" varchar(55) NOT NULL default ' ',
  "AAABGWRB" varchar(1) NOT NULL default ' ',
  "AAABGWRC" varchar(8) NOT NULL default '00000000',
  "AAABGWRD" varchar(1) NOT NULL default ' ',
  "AAABGWRE" varchar(1) NOT NULL default ' ',
  "AAABGWRF" varchar(4) NOT NULL default ' ',
  "AAABGWRG" varchar(1) NOT NULL default ' ',
  "AAABGWRH" varchar(1) NOT NULL default ' ',
  "AAABGWRI" varchar(1) NOT NULL default ' ',
  "AAABGWRJ" varchar(1) NOT NULL default ' ',
  "AAABGWRK" varchar(1) NOT NULL default ' ',
  "AAABGWRL" varchar(1) NOT NULL default ' ',
  "AAABGWRM" varchar(1) NOT NULL default ' ',
  "AAABGWRN" varchar(1) NOT NULL default ' ',
  "AAABGWRO" varchar(1) NOT NULL default ' ',
  "AAABGWRP" varchar(55) NOT NULL default ' ',
  "AAABGWRQ" varchar(2) NOT NULL default ' ',
  "AAABGWRR" varchar(2) NOT NULL default ' ',
  "AAABGWRS" varchar(1) NOT NULL default ' ',
  "AAABGWRT" varchar(1) NOT NULL default ' ',
  "AAABGWRU" varchar(1) NOT NULL default ' ',
  "AAABGWRV" varchar(1) NOT NULL default ' ',
  "AAABGWRW" varchar(1) NOT NULL default ' ',
  "AAABGWRX" varchar(1) NOT NULL default ' ',
  "AAABGWRY" varchar(1) NOT NULL default ' ',
  "AAABGWRZ" varchar(1) NOT NULL default ' ',
  "AAABGWSA" varchar(1) NOT NULL default ' ',
  "AAABGWSB" varchar(1) NOT NULL default ' ',
  "AAABGWSC" varchar(55) NOT NULL default ' ',
  "AAABGWSD" varchar(1) NOT NULL default ' ',
  "AAABGWSE" varchar(1) NOT NULL default ' ',
  "AAABGWSF" varchar(1) NOT NULL default ' ',
  "AAABGWSG" varchar(1) NOT NULL default ' ',
  "AAABGWSH" varchar(1) NOT NULL default ' ',
  "AAABGWSI" varchar(1) NOT NULL default ' ',
  "AAABGWSJ" varchar(1) NOT NULL default ' ',
  "AAABGWSK" varchar(1) NOT NULL default ' ',
  "AAABGWSL" varchar(1) NOT NULL default ' ',
  "AAABGWSM" varchar(1) NOT NULL default ' ',
  "AAABGWSN" varchar(1) NOT NULL default ' ',
  "AAABGWSO" varchar(1) NOT NULL default ' ',
  "AAABGWSP" varchar(1) NOT NULL default ' ',
  "AAABGWSQ" varchar(1) NOT NULL default ' ',
  "AAABGWSR" varchar(1) NOT NULL default ' ',
  "AAABGWSS" varchar(1) NOT NULL default ' ',
  "AAABGWST" varchar(1) NOT NULL default ' ',
  "AAABGWSU" varchar(1) NOT NULL default ' ',
  "AAABGWSV" varchar(1) NOT NULL default ' ',
  "AAABGWSW" varchar(1) NOT NULL default ' ',
  "AAABGWSX" varchar(1) NOT NULL default ' ',
  "AAABGWSY" varchar(1) NOT NULL default ' ',
  "AAABGWSZ" varchar(1) NOT NULL default ' ',
  "AAABGWTA" varchar(1) NOT NULL default ' ',
  "AAABGWTB" varchar(1) NOT NULL default ' ',
  "AAABGWTC" varchar(1) NOT NULL default ' ',
  "AAABGWTD" varchar(1) NOT NULL default ' ',
  "AAABGWTE" varchar(10) NOT NULL default ' ',
  "AAABGWTF" varchar(4) NOT NULL default ' ',
  "AAABGWTG" varchar(12) NOT NULL default ' ',
  "AAABGWTH" varchar(1) NOT NULL default ' ',
  "AAABGWTI" varchar(1) NOT NULL default ' ',
  "AAABGWTJ" varchar(5) NOT NULL default '00000',
  "AAABGWTK" varchar(80) NOT NULL default ' ',
  "AAABGWTL" varchar(1) NOT NULL default ' ',
  "AAABGWTM" varchar(5) NOT NULL default '00000',
  "AAABGWTN" varchar(80) NOT NULL default ' ',
  "AAABGWTO" varchar(1) NOT NULL default ' ',
  "AAABGWTP" varchar(10) NOT NULL default ' ',
  "AAABLMKH" varchar(8) NOT NULL default '00000000',
  "AAABLIRO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMDEN","AAABGWXD","AAABGWXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWTV" (
  "AAABGWXD" varchar(1) NOT NULL default ' ',
  "AAABGWXE" varchar(1) NOT NULL default ' ',
  "AAABGWWU" varchar(20) NOT NULL default ' ',
  "AAABGWWV" varchar(1) NOT NULL default '0',
  "AAABGWWW" varchar(3) NOT NULL default '000',
  "AAABHJRW" varchar(2) NOT NULL default ' ',
  "AAABJUQY" varchar(2) NOT NULL default '00',
  "AAABGWXI" varchar(20) NOT NULL default ' ',
  "AAABGWXJ" varchar(1) NOT NULL default '0',
  "AAABGWXK" varchar(3) NOT NULL default '000',
  "AAABGWUG" varchar(1) NOT NULL default ' ',
  "AAABGWUH" varchar(3) NOT NULL default '000',
  "AAABGWUI" varchar(3) NOT NULL default '000',
  "AAABGWUJ" varchar(1) NOT NULL default '0',
  "AAABKNBX" varchar(2) NOT NULL default ' ',
  "AAABGWUL" varchar(2) NOT NULL default '00',
  "AAABGWUM" varchar(2) NOT NULL default '00',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABGWVP" varchar(8) NOT NULL default '00000000',
  "AAABGWUP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGWXD","AAABGWXE","AAABGWWU","AAABGWWV","AAABGWWW","AAABHJRW","AAABJUQY"),
  KEY "AAABGWUX" ("AAABGWXD","AAABGWXE"),
  KEY "AAABGWVA" ("AAABGWXI","AAABGWXJ","AAABGWXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWVE" (
  "AAABGWXD" varchar(1) NOT NULL default ' ',
  "AAABGWXE" varchar(1) NOT NULL default ' ',
  "AAABKIZK" varchar(10) NOT NULL default ' ',
  "AAABGWVX" varchar(6) NOT NULL default '000000',
  "AAABGWWU" varchar(20) NOT NULL default ' ',
  "AAABGWWV" varchar(1) NOT NULL default '0',
  "AAABGWWW" varchar(3) NOT NULL default '000',
  "AAABGWVM" varchar(1) NOT NULL default ' ',
  "AAABHLQD" int(11) NOT NULL default '0',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABGWVP" varchar(8) NOT NULL default '00000000',
  "AAABGWVQ" varchar(6) NOT NULL default '000000',
  "AAABGWVR" varchar(1) NOT NULL default ' ',
  "AAABGWVS" varchar(1) NOT NULL default ' ',
  "AAABGWVT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGWXD","AAABGWXE","AAABKIZK","AAABGWVX","AAABGWWU","AAABGWWV","AAABGWWW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWWB" (
  "AAABGWXD" varchar(1) NOT NULL default ' ',
  "AAABGWXE" varchar(1) NOT NULL default ' ',
  "AAABGWWU" varchar(20) NOT NULL default ' ',
  "AAABGWWV" varchar(1) NOT NULL default '0',
  "AAABGWWW" varchar(3) NOT NULL default '000',
  "AAABGWXI" varchar(20) NOT NULL default ' ',
  "AAABGWXJ" varchar(1) NOT NULL default '0',
  "AAABGWXK" varchar(3) NOT NULL default '000',
  "AAABGWXG" varchar(22) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABHJRW" varchar(2) NOT NULL default ' ',
  "AAABHLQD" int(11) NOT NULL default '0',
  "AAABGWWO" varchar(1) NOT NULL default ' ',
  "AAABJHGJ" varchar(1) NOT NULL default ' ',
  "AAABJHGK" varchar(1) NOT NULL default ' ',
  "AAABIKRS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGWXD","AAABGWXE","AAABGWWU","AAABGWWV","AAABGWWW","AAABGWXI","AAABGWXJ","AAABGWXK","AAABGWXG","AAABMRFO"),
  KEY "AAABGWXC" ("AAABGWXD","AAABGWXE"),
  KEY "AAABGWXF" ("AAABGWXG"),
  KEY "AAABGWXH" ("AAABGWXI","AAABGWXJ","AAABGWXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWXL" (
  "AAABJARY" varchar(2) NOT NULL default ' ',
  "AAABMMJT" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABJARY","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWXQ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJARY" varchar(2) NOT NULL default ' ',
  "AAABMMJT" varchar(2) NOT NULL default '00',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABJARY","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWXY" (
  "AAABGXFK" varchar(1) NOT NULL default ' ',
  "AAABGWYP" varchar(4) NOT NULL default '0000',
  "AAABGXFL" varchar(1) NOT NULL default ' ',
  "AAABLPJD" varchar(4) NOT NULL default ' ',
  "AAABLXTG" int(11) NOT NULL default '0',
  "AAABKUQR" varchar(8) NOT NULL default '00000000',
  "AAABLWWO" varchar(6) NOT NULL default '000000',
  "AAABIJSB" varchar(5) NOT NULL default '00000',
  "AAABGWYH" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABGXFK","AAABGWYP","AAABGXFL","AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWYM" (
  "AAABGWYU" varchar(5) NOT NULL default '00000',
  "AAABGWYO" varchar(5) NOT NULL default '00000',
  "AAABGWYP" varchar(4) NOT NULL default '0000',
  "AAABGWYQ" varchar(1) NOT NULL default ' ',
  "AAABGWYR" varchar(1) NOT NULL default ' ',
  "AAABGWYS" varchar(1) NOT NULL default ' ',
  "AAABGWYT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGWYU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWYV" (
  "AAABLLBG" varchar(15) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLBG","AAABIRUN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWZA" (
  "AAABLLBG" varchar(15) NOT NULL default ' ',
  "AAABKCWA" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(132) NOT NULL default ' ',
  "AAABGXHS" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABIJJL" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLLBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWZI" (
  "AAABGXFK" varchar(1) NOT NULL default ' ',
  "AAABHNRX" varchar(4) NOT NULL default ' ',
  "AAABKCWA" varchar(1) NOT NULL default ' ',
  "AAABGXHS" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABGXFK","AAABHNRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWZQ" (
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAABLRST","AAABMKFH","AAABLFYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGWZX" (
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  "AAABGXAI" varchar(1) NOT NULL default ' ',
  "AAABGXAJ" int(11) NOT NULL default '0',
  "AAABGXAC" varchar(1) NOT NULL default ' ',
  "AAABGXAK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKGJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXAF" (
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  "AAABGXAI" varchar(1) NOT NULL default ' ',
  "AAABGXAJ" int(11) NOT NULL default '0',
  "AAABGXAK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKGJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXAM" (
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABGXHS" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABMNRG" varchar(1) NOT NULL default '0',
  "AAABLUVJ" varchar(32) NOT NULL default ' ',
  "AAABLITM" varchar(20) NOT NULL default ' ',
  "AAABKCWA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXAV" (
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGXAY" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMNJW","AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXBB" (
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABGXBF" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMNJW","AAABGXBF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXBG" (
  "AAABGXEK" varchar(5) NOT NULL default '00000',
  "AAABGXEX" varchar(1) NOT NULL default ' ',
  "AAABLPJD" varchar(4) NOT NULL default ' ',
  "AAABLXTG" int(11) NOT NULL default '0',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIJSB" varchar(5) NOT NULL default '00000',
  "AAABKUQR" varchar(8) NOT NULL default '00000000',
  "AAABLWWO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABGXEK","AAABGXEX","AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXBS" (
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABKCWA" varchar(1) NOT NULL default ' ',
  "AAABGXCH" varchar(8) NOT NULL default '00000000',
  "AAABGXCI" varchar(8) NOT NULL default '00000000',
  "AAABGXCJ" varchar(8) NOT NULL default '00000000',
  "AAABGXCK" varchar(8) NOT NULL default '00000000',
  "AAABGXHS" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLENH","AAABKCWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXCD" (
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABKCWA" varchar(1) NOT NULL default ' ',
  "AAABGXCH" varchar(8) NOT NULL default '00000000',
  "AAABGXCI" varchar(8) NOT NULL default '00000000',
  "AAABGXCJ" varchar(8) NOT NULL default '00000000',
  "AAABGXCK" varchar(8) NOT NULL default '00000000',
  "AAABGXHS" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMNJW","AAABLENH","AAABKCWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXCQ" (
  "AAABGXDU" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  "AAABGXHS" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABGXDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXCW" (
  "AAABGXDU" varchar(2) NOT NULL default ' ',
  "AAABJARY" varchar(2) NOT NULL default ' ',
  "AAABMMJT" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABGXDU","AAABJARY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXDC" (
  "AAABGXDJ" varchar(20) NOT NULL default ' ',
  "AAABGXDK" varchar(1) NOT NULL default ' ',
  "AAABGXDL" varchar(1) NOT NULL default ' ',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABGXDJ","AAABGXDK","AAABGXDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXDM" (
  "AAABLLBG" varchar(5) NOT NULL default ' ',
  "AAABILWI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLBG","AAABILWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXDR" (
  "AAABHBUL" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABGXDU" varchar(2) NOT NULL default ' ',
  "AAABIFOX" varchar(2) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGXHS" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABHBUL","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXEB" (
  "AAABIJSB" varchar(5) NOT NULL default '00000',
  "AAABKIZK" varchar(5) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLRTV" varchar(8) NOT NULL default '00000000',
  "AAABLRTW" varchar(6) NOT NULL default '000000',
  "AAABHIYE" varchar(8) NOT NULL default '00000000',
  "AAABHIYF" varchar(6) NOT NULL default '000000',
  "AAABGXEK" varchar(5) NOT NULL default '00000',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABIJSB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXEN" (
  "AAABGXFJ" varchar(2) NOT NULL default '00',
  "AAABGXFK" varchar(1) NOT NULL default ' ',
  "AAABGXFL" varchar(1) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABGXEX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGXFJ","AAABGXFK","AAABGXFL","AAABIRUN","AAABGXEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXEY" (
  "AAABGXFJ" varchar(2) NOT NULL default '00',
  "AAABGXFK" varchar(1) NOT NULL default ' ',
  "AAABGXFL" varchar(1) NOT NULL default ' ',
  "AAABLPJD" varchar(4) NOT NULL default ' ',
  "AAABGXFD" varchar(1) NOT NULL default ' ',
  "AAABGXFE" varchar(1) NOT NULL default ' ',
  "AAABGXFQ" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABGXFJ","AAABGXFK","AAABGXFL","AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXFN" (
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABGXFP" varchar(1) NOT NULL default ' ',
  "AAABGXFQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIRUN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXFS" (
  "AAABHBPC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHBPC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXFV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKPKA" varchar(12) NOT NULL default '000000000000',
  "AAABGXGP" varchar(35) NOT NULL default ' ',
  "AAABGXGX" varchar(35) NOT NULL default ' ',
  "AAABGXGA" varchar(8) NOT NULL default '00000000',
  "AAABGXGB" varchar(6) NOT NULL default '000000',
  "AAABLSCQ" varchar(1) NOT NULL default ' ',
  "AAABGXGV" varchar(10) NOT NULL default ' ',
  "AAABGXGE" varchar(2) NOT NULL default ' ',
  "AAABGXGF" varchar(2) NOT NULL default ' ',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABIJDD" varchar(3) NOT NULL default ' ',
  "AAABLQTX" varchar(3) NOT NULL default ' ',
  "AAABLQSF" varchar(1) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABLDNI" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKPKA"),
  KEY "AAABGXGO" ("AAABGXGP","AAABLMHB","AAABGXGV","AAABMRIL"),
  KEY "AAABGXGT" ("AAABLMHB","AAABGXGV","AAABMRIL","AAABGXGX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXGY" (
  "AAABJSPH" varchar(10) NOT NULL default ' ',
  "AAABLCCC" varchar(1) NOT NULL default ' ',
  "AAABGXHB" varchar(20) NOT NULL default ' ',
  "AAABGXHC" varchar(1) NOT NULL default ' ',
  "AAABGXHP" varchar(1) NOT NULL default ' ',
  "AAABGXHQ" varchar(1) NOT NULL default ' ',
  "AAABGXHR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJSPH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXHH" (
  "AAABJSPH" varchar(10) NOT NULL default ' ',
  "AAABGXHJ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABJSPH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXHL" (
  "AAABJSPH" varchar(10) NOT NULL default ' ',
  "AAABLCCC" varchar(1) NOT NULL default ' ',
  "AAABGXHO" varchar(20) NOT NULL default ' ',
  "AAABGXHP" varchar(1) NOT NULL default ' ',
  "AAABGXHQ" varchar(1) NOT NULL default ' ',
  "AAABGXHR" varchar(1) NOT NULL default ' ',
  "AAABGXHS" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABJSPH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXHV" (
  "AAABJSPH" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHWUC" varchar(60) NOT NULL default ' ',
  "AAABKNGX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABJSPH","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXIC" (
  "AAABGXIV" varchar(12) NOT NULL default ' ',
  "AAABKMMA" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGXIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXIH" (
  "AAABGXIV" varchar(12) NOT NULL default ' ',
  "AAABGXIL" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABGXIV","AAABGXIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXIM" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGXIV" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABGXIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXIS" (
  "AAABGXIV" varchar(12) NOT NULL default ' ',
  "AAABGXIW" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABGXIV","AAABGXIW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXIX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYRW" varchar(6) NOT NULL default ' ',
  "AAABJJDD" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYRW","AAABJJDD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXJE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXHD" varchar(3) NOT NULL default ' ',
  "AAABHPXG" varchar(10) NOT NULL default ' ',
  "AAABLXIP" varchar(2) NOT NULL default ' ',
  "AAABKXVZ" varchar(4) NOT NULL default ' ',
  "AAABKMCD" varchar(3) NOT NULL default ' ',
  "AAABLXWU" varchar(10) NOT NULL default ' ',
  "AAABLYRW" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXHD","AAABHPXG","AAABLXIP","AAABKXVZ","AAABKMCD","AAABLXWU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXJU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXHD" varchar(3) NOT NULL default ' ',
  "AAABHPXG" varchar(10) NOT NULL default ' ',
  "AAABKMCD" varchar(3) NOT NULL default ' ',
  "AAABLXWU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXHD","AAABHPXG","AAABKMCD","AAABLXWU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXKF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXHD" varchar(3) NOT NULL default ' ',
  "AAABHPXG" varchar(10) NOT NULL default ' ',
  "AAABLXIP" varchar(2) NOT NULL default ' ',
  "AAABKXVZ" varchar(4) NOT NULL default ' ',
  "AAABKMCD" varchar(3) NOT NULL default ' ',
  "AAABLXWU" varchar(10) NOT NULL default ' ',
  "AAABHONR" varchar(4) NOT NULL default ' ',
  "AAABLYRW" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXHD","AAABHPXG","AAABLXIP","AAABKXVZ","AAABKMCD","AAABLXWU","AAABHONR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXKX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" VARBINARY(255) NOT NULL,
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGXLB" varchar(10) NOT NULL default ' ',
  "AAABLAXK" VARBINARY(255),
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABGXLE" varchar(10) NOT NULL default '0000000000',
  "AAABKSXJ" int(11) NOT NULL default '0',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABGXLI" varchar(4) NOT NULL default '0000',
  "AAABGZER" varchar(5) NOT NULL default ' ',
  "AAABGZES" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEW" varchar(8) NOT NULL default '00000000',
  "AAABLADQ" varchar(1) NOT NULL default ' ',
  "AAABLADR" varchar(1) NOT NULL default ' ',
  "AAABKWDE" varchar(1) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABKWDD" varchar(1) NOT NULL default ' ',
  "AAABKTRY" varchar(20) NOT NULL default ' ',
  "AAABGZEU" varchar(5) NOT NULL default ' ',
  "AAABGZEV" varchar(5) NOT NULL default ' ',
  "AAABLADS" varchar(10) NOT NULL default ' ',
  "AAABLADA" varchar(4) NOT NULL default ' ',
  "AAABLCFI" varchar(10) NOT NULL default ' ',
  "AAABKWDG" varchar(20) NOT NULL default ' ',
  "AAABKWDH" varchar(50) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABLPPS" varchar(10) NOT NULL default ' ',
  "AAABGXMB" varchar(10) NOT NULL default ' ',
  "AAABGXMC" varchar(2) NOT NULL default ' ',
  "AAABGXMD" varchar(2) NOT NULL default ' ',
  "AAABGXME" varchar(8) NOT NULL default '00000000',
  "AAABGXMF" varchar(1) NOT NULL default ' ',
  "AAABGXMG" varchar(1) NOT NULL default ' ',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAABGXMI" varchar(140) NOT NULL default ' ',
  "AAABGXMJ" varchar(1) NOT NULL default ' ',
  "AAABJWRX" varchar(1) NOT NULL default ' ',
  "AAABJWRW" varchar(2) NOT NULL default ' ',
  "AAABMCZY" varchar(2) NOT NULL default '00',
  "AAABMCZZ" varchar(2) NOT NULL default '00',
  "AAABMDAA" varchar(2) NOT NULL default '00',
  "AAABMDAB" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"(255)),
  KEY "AAABGXMS" ("AAABMRIL","AAABKTRY"),
  KEY "AAABGXMV" ("AAABMRIL","AAABLAXK"(255),"AAABJGWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXMZ" (
  "AAABGXNF" varchar(10) NOT NULL default ' ',
  "AAABGXNB" varchar(1) NOT NULL default ' ',
  "AAABGXNC" varchar(20) NOT NULL default ' ',
  "AAABGXND" varchar(3) NOT NULL default ' ',
  "AAABLUSG" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABGXNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXNG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGXYC" varchar(32) NOT NULL default ' ',
  "AAABKZZG" varchar(13) NOT NULL default ' ',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEN" decimal(21,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABGXYC","AAABKZZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXNS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGXWG" varchar(32) NOT NULL default ' ',
  "AAABKZZG" varchar(13) NOT NULL default ' ',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEN" decimal(21,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABGXWG","AAABKZZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXOE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABKZZG" varchar(13) NOT NULL default ' ',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZKH","AAABKZZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXOM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGXXT" varchar(32) NOT NULL default ' ',
  "AAABKZZG" varchar(13) NOT NULL default ' ',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGXXT","AAABKZZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXOV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGXPF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXPB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGYJY" varchar(3) NOT NULL default ' ',
  "AAABGXPF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWD","AAABGYJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXPJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGXWY" varchar(1) NOT NULL default ' ',
  "AAABGXQH" varchar(3) NOT NULL default ' ',
  "AAABGXQI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXPR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGYJY" varchar(3) NOT NULL default ' ',
  "AAABGXWY" varchar(1) NOT NULL default ' ',
  "AAABGXQH" varchar(3) NOT NULL default ' ',
  "AAABGXQI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWD","AAABGYJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXQB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGYJY" varchar(3) NOT NULL default ' ',
  "AAABJMEG" varchar(4) NOT NULL default '0000',
  "AAABGXWY" varchar(1) NOT NULL default ' ',
  "AAABGXQH" varchar(3) NOT NULL default ' ',
  "AAABGXQI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWD","AAABGYJY","AAABJMEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXQN" (
  "AAABLAXL" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLAXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABLPQZ" decimal(25,6) NOT NULL default '0.000000',
  "AAABGXQU" varchar(1) NOT NULL default ' ',
  "AAABGXQV" varchar(1) NOT NULL default ' ',
  "AAABGXRC" varchar(32) NOT NULL default ' ',
  "AAABGXQX" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"),
  KEY "AAABGXRA" ("AAABMRIL","AAABGXRC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXRD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGXRO" VARBINARY(255) NOT NULL,
  "AAABLHFC" varchar(8) NOT NULL default '00000000',
  "AAABGXRH" VARBINARY(255),
  "AAABGXVC" varchar(8) NOT NULL default '00000000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGZKC" decimal(22,6) NOT NULL default '0.000000',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGXRO"(255),"AAABLHFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXRQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABKZRY" VARBINARY(255),
  "AAABGXRU" decimal(25,6) NOT NULL default '0.000000',
  "AAABGXTQ" varchar(4) NOT NULL default '0000',
  "AAABGXRW" VARBINARY(255),
  "AAABGXSC" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"),
  KEY "AAABGXSA" ("AAABMRIL","AAABGXSC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXSD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABLAXK" VARBINARY(255),
  "AAABJOTX" varchar(8) NOT NULL default '00000000',
  "AAABGXSI" varchar(6) NOT NULL default '000000',
  "AAABLAXL" varchar(4) NOT NULL default '0000',
  "AAABGZKF" varchar(8) NOT NULL default '00000000',
  "AAABGXSL" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAABGZKH"),
  KEY "AAABGXSO" ("AAABLAXK"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGXYC" varchar(32) NOT NULL default ' ',
  "AAABGZJF" VARBINARY(255),
  "AAABGYYS" int(11) NOT NULL default '0',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  "AAABGZKC" decimal(22,6) NOT NULL default '0.000000',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEN" decimal(21,2) NOT NULL default '0.00',
  "AAABGZKF" varchar(8) NOT NULL default '00000000',
  "AAABKZVA" varchar(4) NOT NULL default ' ',
  "AAABGZEP" varchar(5) NOT NULL default ' ',
  "AAABGYZF" decimal(21,2) NOT NULL default '0.00',
  "AAABGXTR" varchar(1) NOT NULL default ' ',
  "AAABGXTQ" varchar(4) NOT NULL default '0000',
  "AAABIDTM" varchar(1) NOT NULL default '0',
  "AAABGZER" varchar(5) NOT NULL default ' ',
  "AAABGZES" decimal(21,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABGXYC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXTN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABGXTQ" varchar(4) NOT NULL default '0000',
  "AAABGXTR" varchar(1) NOT NULL default ' ',
  "AAABIDTM" varchar(1) NOT NULL default '0',
  "AAABGXUA" varchar(32) NOT NULL default ' ',
  "AAABGXTU" varchar(32) NOT NULL default ' ',
  "AAABGZFQ" varchar(32) NOT NULL default ' ',
  "AAABGZFR" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"),
  KEY "AAABGXTZ" ("AAABGXUA"),
  KEY "AAABGXUB" ("AAABGZFQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXUD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGXUV" varchar(13) NOT NULL default ' ',
  "AAABJGWE" varchar(10) NOT NULL default ' ',
  "AAABGXUX" varchar(8) NOT NULL default '00000000',
  "AAABGXUJ" varchar(8) NOT NULL default '00000000',
  "AAABGXVC" varchar(8) NOT NULL default '00000000',
  "AAABGZKC" decimal(22,6) NOT NULL default '0.000000',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  "AAABLAXK" VARBINARY(255),
  "AAABLAXL" varchar(4) NOT NULL default '0000',
  "AAABLAXM" varchar(1) NOT NULL default ' ',
  "AAABKZRY" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAABJGWD","AAABGXUV","AAABJGWE","AAABGXUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXUY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABGZKF" varchar(8) NOT NULL default '00000000',
  "AAABGXVC" varchar(8) NOT NULL default '00000000',
  "AAABLAXK" VARBINARY(255),
  "AAABKZRY" VARBINARY(255),
  "AAABGXVF" varchar(1) NOT NULL default ' ',
  "AAABGZFQ" varchar(32) NOT NULL default ' ',
  "AAABGZFR" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"),
  KEY "AAABGXVK" ("AAABGZFQ"),
  KEY "AAABGXVM" ("AAABKZRY"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXVO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGXWG" varchar(32) NOT NULL default ' ',
  "AAABGXXR" varchar(32) NOT NULL default ' ',
  "AAABGXYC" varchar(32) NOT NULL default ' ',
  "AAABGZKC" decimal(22,6) NOT NULL default '0.000000',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEN" decimal(21,2) NOT NULL default '0.00',
  "AAABGZKF" varchar(8) NOT NULL default '00000000',
  "AAABKZVA" varchar(4) NOT NULL default ' ',
  "AAABGZEP" varchar(5) NOT NULL default ' ',
  "AAABGYZF" decimal(21,2) NOT NULL default '0.00',
  "AAABGZER" varchar(5) NOT NULL default ' ',
  "AAABGZES" decimal(21,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABGXWG"),
  KEY "AAABGXWH" ("AAABMRIL","AAABGXXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXWK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGXWQ" varchar(32) NOT NULL default ' ',
  "AAABGXXW" varchar(32) NOT NULL default ' ',
  "AAABGXYC" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGXWQ"),
  KEY "AAABGXWR" ("AAABMRIL","AAABGXXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXWU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABGXWX" varchar(1) NOT NULL default ' ',
  "AAABGXWY" varchar(1) NOT NULL default ' ',
  "AAABGXWZ" varchar(1) NOT NULL default ' ',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABGXXB" varchar(8) NOT NULL default '00000000',
  "AAABGXXI" varchar(32) NOT NULL default ' ',
  "AAABGXXD" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"),
  UNIQUE KEY "AAABGXXG" ("AAABMRIL","AAABGXXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXXJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGXXT" varchar(32) NOT NULL default ' ',
  "AAABGZKF" varchar(8) NOT NULL default '00000000',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABGZKC" decimal(22,6) NOT NULL default '0.000000',
  "AAABGXXW" varchar(32) NOT NULL default ' ',
  "AAABGXXR" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGXXT"),
  KEY "AAABGXXU" ("AAABMRIL","AAABGXXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXXX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGXYC" varchar(32) NOT NULL default ' ',
  "AAABGXYF" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGXYC"),
  KEY "AAABGXYD" ("AAABMRIL","AAABGXYF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXYG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABLAYL" varchar(10) NOT NULL default ' ',
  "AAABLAXT" varchar(8) NOT NULL default '00000000',
  "AAABLDCD" varchar(4) NOT NULL default ' ',
  "AAABLHFC" varchar(3) NOT NULL default ' ',
  "AAABLBXX" decimal(15,2) NOT NULL default '0.00',
  "AAABLBYH" varchar(5) NOT NULL default ' ',
  "AAABMDHR" decimal(15,2) NOT NULL default '0.00',
  "AAABLAYB" decimal(15,5) NOT NULL default '0.00000',
  "AAABLAYD" varchar(5) NOT NULL default ' ',
  "AAABMDHS" decimal(15,2) NOT NULL default '0.00',
  "AAABLAXK" VARBINARY(255),
  "AAABLAXL" varchar(4) NOT NULL default '0000',
  "AAABLAXM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMEFT","AAABLAYL","AAABLAXT","AAABLDCD","AAABLHFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXZE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABKYFK" varchar(10) NOT NULL default ' ',
  "AAABHVDW" varchar(2) NOT NULL default ' ',
  "AAABGZKU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABKYFK","AAABHVDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXZO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  "AAABGXZS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCSO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGXZW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGYAB" varchar(2) NOT NULL default ' ',
  "AAABGYAC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGYAB","AAABGYAC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYAD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  "AAABGYAH" varchar(10) NOT NULL default ' ',
  "AAABGYAI" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCSO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYAM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJHR" varchar(4) NOT NULL default ' ',
  "AAABGYAP" varchar(2) NOT NULL default ' ',
  "AAABGYAQ" varchar(2) NOT NULL default ' ',
  "AAABGYAR" varchar(2) NOT NULL default ' ',
  "AAABLJHC" varchar(4) NOT NULL default ' ',
  "AAABGYAT" varchar(1) NOT NULL default ' ',
  "AAABGYAU" varchar(1) NOT NULL default ' ',
  "AAABGYAV" varchar(1) NOT NULL default ' ',
  "AAABGYAW" varchar(1) NOT NULL default ' ',
  "AAABGYAX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJHR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYBA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJHR" varchar(4) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABGYBF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJHR","AAABMAMY","AAABMQIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYBK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLJHR" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLJHR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYBS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGYCL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGYCL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYBX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGYCL" varchar(4) NOT NULL default ' ',
  "AAABLJHR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGYCL","AAABLJHR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYCE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGYCL" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGYCL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYCM" (
  "AAABLITM" varchar(8) NOT NULL default ' ',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABJTAJ" varchar(6) NOT NULL default '000000',
  "AAABGYCX" varchar(2) NOT NULL default ' ',
  "AAABMPMH" varchar(5) NOT NULL default ' ',
  "AAABMLPO" decimal(15,0) NOT NULL default '0',
  "AAABHAGF" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLITM","AAABKOUD","AAABJTAJ","AAABGYCX","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYCZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABKZZG" varchar(13) NOT NULL default ' ',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEN" decimal(21,2) NOT NULL default '0.00',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  "AAABMDZN" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABGZKH","AAABKZZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYDM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABKZZG" varchar(13) NOT NULL default ' ',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEN" decimal(21,2) NOT NULL default '0.00',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  "AAABMDZN" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABGZKH","AAABKZZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYDZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGZED" varchar(6) NOT NULL default ' ',
  "AAABKZZG" varchar(13) NOT NULL default ' ',
  "AAABGZDP" decimal(21,2) NOT NULL default '0.00',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWD","AAABGZED","AAABKZZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYEK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" VARBINARY(255) NOT NULL,
  "AAABKZZG" varchar(13) NOT NULL default ' ',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEN" decimal(21,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"(255),"AAABKZZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYEW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZGR" int(11) NOT NULL default '0',
  "AAABKZZG" varchar(13) NOT NULL default ' ',
  "AAABGZGJ" decimal(21,2) NOT NULL default '0.00',
  "AAABGZGK" decimal(21,2) NOT NULL default '0.00',
  "AAABGZGL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZGR","AAABKZZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYFG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABKZZG" varchar(13) NOT NULL default ' ',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABGZKH","AAABKZZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYFP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABKZZG" varchar(13) NOT NULL default ' ',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABGZKH","AAABKZZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYFY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJPBG" varchar(3) NOT NULL default '000',
  "AAABGYGB" varchar(8) NOT NULL default ' ',
  "AAABGYGC" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJPBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYGF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  "AAABGYGJ" varchar(8) NOT NULL default ' ',
  "AAABGYGK" varchar(8) NOT NULL default ' ',
  "AAABGYGL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCPX","AAABLDBT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYGP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  "AAABMEBC" varchar(4) NOT NULL default ' ',
  "AAABGYGT" varchar(1) NOT NULL default ' ',
  "AAABGYGU" varchar(1) NOT NULL default ' ',
  "AAABGYGV" varchar(2) NOT NULL default ' ',
  "AAABGYGW" varchar(1) NOT NULL default ' ',
  "AAABGYGX" varchar(1) NOT NULL default ' ',
  "AAABGYGY" varchar(1) NOT NULL default ' ',
  "AAABGYGZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKZWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYHC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKZWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYHH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKZWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYHM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGYJY" varchar(3) NOT NULL default ' ',
  "AAABGYJZ" varchar(8) NOT NULL default ' ',
  "AAABGYJU" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGYJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYHT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGYJY" varchar(3) NOT NULL default ' ',
  "AAABGYJZ" varchar(8) NOT NULL default ' ',
  "AAABGYHX" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGYJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYIA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGYIF" varchar(6) NOT NULL default ' ',
  "AAABGYID" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGYIF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYIG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGYIL" varchar(15) NOT NULL default ' ',
  "AAABGYIJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGYIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYIM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRFO" varchar(5) NOT NULL default ' ',
  "AAABGYIP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYIS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  "AAABGYIW" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWD","AAABKZWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYJA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLAJY" varchar(4) NOT NULL default ' ',
  "AAABLCYD" varchar(1) NOT NULL default ' ',
  "AAABLCYP" decimal(13,7) NOT NULL default '0.0000000',
  "AAABGYJF" decimal(17,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLAJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYJI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLAJY" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGYJM" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLAJY","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYJQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGYJY" varchar(3) NOT NULL default ' ',
  "AAABGYJZ" varchar(8) NOT NULL default ' ',
  "AAABGYJU" varchar(8) NOT NULL default ' ',
  "AAABLKZT" varchar(1) NOT NULL default ' ',
  "AAABGYJW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGYJY","AAABGYJZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYKA" (
  "AAABGYLC" varchar(10) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABGYKD" varchar(40) NOT NULL default ' ',
  "AAABGYKE" varchar(40) NOT NULL default ' ',
  "AAABGYKF" varchar(40) NOT NULL default ' ',
  "AAABGYKG" varchar(40) NOT NULL default ' ',
  "AAABGYKH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABGYLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYKJ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGYLC" varchar(10) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABGYLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYKP" (
  "AAABGYLC" varchar(10) NOT NULL default ' ',
  "AAABMRCU" varchar(6) NOT NULL default '000000',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABGYKT" varchar(30) NOT NULL default ' ',
  "AAABGYKU" varchar(20) NOT NULL default ' ',
  "AAABKAFO" varchar(20) NOT NULL default ' ',
  "AAABMNYU" varchar(20) NOT NULL default ' ',
  "AAABGYKX" varchar(20) NOT NULL default ' ',
  "AAABGYKY" varchar(20) NOT NULL default ' ',
  "AAABGYKZ" varchar(40) NOT NULL default ' ',
  "AAABGYLA" varchar(20) NOT NULL default ' ',
  "AAABGYLB" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABGYLC","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYLE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGYLO" varchar(10) NOT NULL default ' ',
  "AAABGZKF" varchar(8) NOT NULL default '00000000',
  "AAABHMRN" varchar(32) NOT NULL default ' ',
  "AAABLAXK" VARBINARY(255),
  "AAABGYMA" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAABJGWD","AAABGYLO","AAABGZKF","AAABHMRN"),
  KEY "AAABGYLR" ("AAABMRIL","AAABHMRN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYLU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABKZLG" varchar(10) NOT NULL default ' ',
  "AAABLHFC" varchar(3) NOT NULL default ' ',
  "AAABHMRN" varchar(32) NOT NULL default ' ',
  "AAABGYMA" VARBINARY(255),
  "AAABKSXJ" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABJGWD","AAABKZLG","AAABLHFC","AAABHMRN"),
  KEY "AAABGYMH" ("AAABMRIL","AAABHMRN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYMK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHMRN" varchar(32) NOT NULL default ' ',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABMKKB" varchar(8) NOT NULL default '00000000',
  "AAABGYMP" varchar(1) NOT NULL default ' ',
  "AAABGYMQ" varchar(1) NOT NULL default ' ',
  "AAABLZDP" varchar(12) NOT NULL default ' ',
  "AAABLCIO" varchar(8) NOT NULL default '00000000',
  "AAABLCIP" varchar(6) NOT NULL default '000000',
  "AAABLCIQ" varchar(10) NOT NULL default ' ',
  "AAABLCIS" varchar(12) NOT NULL default ' ',
  "AAABLCIT" varchar(8) NOT NULL default '00000000',
  "AAABLCIU" varchar(6) NOT NULL default '000000',
  "AAABLCIV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHMRN"),
  KEY "AAABGYNB" ("AAABMRIL","AAABJGWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYNE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGYNT" varchar(32) NOT NULL default ' ',
  "AAABGYNU" varchar(32) NOT NULL default ' ',
  "AAABGYNI" varchar(1) NOT NULL default '0',
  "AAABGYNJ" varchar(1) NOT NULL default '0',
  "AAABLZDP" varchar(12) NOT NULL default ' ',
  "AAABLCIO" varchar(8) NOT NULL default '00000000',
  "AAABLCIP" varchar(6) NOT NULL default '000000',
  "AAABLCIQ" varchar(10) NOT NULL default ' ',
  "AAABLCIS" varchar(12) NOT NULL default ' ',
  "AAABLCIT" varchar(8) NOT NULL default '00000000',
  "AAABLCIU" varchar(6) NOT NULL default '000000',
  "AAABLCIV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGYNT","AAABGYNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYNV" (
  "AAABLOZE" varchar(40) NOT NULL default ' ',
  "AAABGYNX" varchar(40) NOT NULL default ' ',
  "AAABGYNY" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYOB" (
  "AAABGYQQ" varchar(4) NOT NULL default ' ',
  "AAABGYOD" varchar(30) NOT NULL default ' ',
  "AAABGYOE" varchar(30) NOT NULL default ' ',
  "AAABGYOF" varchar(30) NOT NULL default ' ',
  "AAABKXRS" varchar(30) NOT NULL default ' ',
  "AAABGYOH" varchar(1) NOT NULL default ' ',
  "AAABGYOI" varchar(30) NOT NULL default ' ',
  "AAABGYOJ" varchar(2) NOT NULL default '00',
  "AAABGYOK" varchar(1) NOT NULL default ' ',
  "AAABGYOL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGYQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYON" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGYQQ" varchar(4) NOT NULL default ' ',
  "AAABGYOQ" varchar(10) NOT NULL default ' ',
  "AAABGYOR" varchar(20) NOT NULL default ' ',
  "AAABGYOS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABGYQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYOV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABGYQM" varchar(30) NOT NULL default ' ',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABIIMJ" varchar(30) NOT NULL default ' ',
  "AAABGYQP" varchar(30) NOT NULL default ' ',
  "AAABGYQQ" varchar(4) NOT NULL default ' ',
  "AAABIWLU" varchar(30) NOT NULL default ' ',
  "AAABGYPE" varchar(1) NOT NULL default '0',
  "AAABHBOZ" varchar(1) NOT NULL default ' ',
  "AAABGYPG" varchar(1) NOT NULL default ' ',
  "AAABGYPH" varchar(1) NOT NULL default '0',
  "AAABGYPI" varchar(1) NOT NULL default '0',
  "AAABGYPJ" varchar(1) NOT NULL default '0',
  "AAABGYPK" varchar(1) NOT NULL default '0',
  "AAABIJFU" varchar(1) NOT NULL default ' ',
  "AAABLPVW" varchar(2) NOT NULL default '00',
  "AAABGYPN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABGYQM","AAABIHHB","AAABIIMJ","AAABGYQP","AAABGYQQ","AAABIWLU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYPW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABGYQM" varchar(30) NOT NULL default ' ',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABIIMJ" varchar(30) NOT NULL default ' ',
  "AAABGYQP" varchar(30) NOT NULL default ' ',
  "AAABGYQQ" varchar(4) NOT NULL default ' ',
  "AAABIWLU" varchar(30) NOT NULL default ' ',
  "AAABIUHL" varchar(10) NOT NULL default ' ',
  "AAABGYQH" varchar(20) NOT NULL default ' ',
  "AAABLOFJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLECD","AAABGYQM","AAABIHHB","AAABIIMJ","AAABGYQP","AAABGYQQ","AAABIWLU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYQS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGYTR" varchar(2) NOT NULL default ' ',
  "AAABGYTS" varchar(4) NOT NULL default ' ',
  "AAABGYTT" varchar(2) NOT NULL default ' ',
  "AAABGYTU" varchar(1) NOT NULL default ' ',
  "AAABLRCB" varchar(1) NOT NULL default ' ',
  "AAABGYTW" varchar(54) NOT NULL default ' ',
  "AAABLXTF" varchar(4) NOT NULL default '0000',
  "AAABGYRY" varchar(2) NOT NULL default ' ',
  "AAABGYRZ" varchar(4) NOT NULL default ' ',
  "AAABGYSA" varchar(2) NOT NULL default ' ',
  "AAABGYSB" varchar(1) NOT NULL default ' ',
  "AAABGYSC" varchar(54) NOT NULL default ' ',
  "AAABKWHA" varchar(32) NOT NULL default ' ',
  "AAABGYSE" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGYTR","AAABGYTS","AAABGYTT","AAABGYTU","AAABLRCB","AAABGYTW","AAABLXTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYRQ" (
  "AAABGYTR" varchar(2) NOT NULL default ' ',
  "AAABGYTS" varchar(4) NOT NULL default ' ',
  "AAABGYTT" varchar(2) NOT NULL default ' ',
  "AAABGYTU" varchar(1) NOT NULL default ' ',
  "AAABLRCB" varchar(1) NOT NULL default ' ',
  "AAABGYTW" varchar(54) NOT NULL default ' ',
  "AAABLXTF" varchar(4) NOT NULL default '0000',
  "AAABGYRY" varchar(2) NOT NULL default ' ',
  "AAABGYRZ" varchar(4) NOT NULL default ' ',
  "AAABGYSA" varchar(2) NOT NULL default ' ',
  "AAABGYSB" varchar(1) NOT NULL default ' ',
  "AAABGYSC" varchar(54) NOT NULL default ' ',
  "AAABKWHA" varchar(32) NOT NULL default ' ',
  "AAABGYSE" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABGYTR","AAABGYTS","AAABGYTT","AAABGYTU","AAABLRCB","AAABGYTW","AAABLXTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYSM" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGYTR" varchar(2) NOT NULL default ' ',
  "AAABGYTS" varchar(4) NOT NULL default ' ',
  "AAABGYTT" varchar(2) NOT NULL default ' ',
  "AAABGYTU" varchar(1) NOT NULL default ' ',
  "AAABLRCB" varchar(1) NOT NULL default ' ',
  "AAABGYTW" varchar(54) NOT NULL default ' ',
  "AAABLXTF" varchar(4) NOT NULL default '0000',
  "AAABMRCK" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABGYTR","AAABGYTS","AAABGYTT","AAABGYTU","AAABLRCB","AAABGYTW","AAABLXTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYTE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGYTR" varchar(2) NOT NULL default ' ',
  "AAABGYTS" varchar(4) NOT NULL default ' ',
  "AAABGYTT" varchar(2) NOT NULL default ' ',
  "AAABGYTU" varchar(1) NOT NULL default ' ',
  "AAABLRCB" varchar(1) NOT NULL default ' ',
  "AAABGYTW" varchar(54) NOT NULL default ' ',
  "AAABLXTF" varchar(4) NOT NULL default '0000',
  "AAABMRCK" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABGYTR","AAABGYTS","AAABGYTT","AAABGYTU","AAABLRCB","AAABGYTW","AAABLXTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYTY" (
  "AAABGYYV" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABGYYV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYUB" (
  "AAABGYYV" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGYUE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABGYYV","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYUH" (
  "AAABLJJN" varchar(2) NOT NULL default ' ',
  "AAABGYUJ" varchar(30) NOT NULL default ' ',
  "AAABGYUK" varchar(30) NOT NULL default ' ',
  "AAABGYUL" varchar(30) NOT NULL default ' ',
  "AAABGYUM" varchar(30) NOT NULL default ' ',
  "AAABGYUN" varchar(30) NOT NULL default ' ',
  "AAABGYUO" varchar(30) NOT NULL default ' ',
  "AAABGYUP" varchar(30) NOT NULL default ' ',
  "AAABGYUQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLJJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYUS" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLJJN" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMLQA","AAABLJJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYUZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKK" varchar(32) NOT NULL default ' ',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABGYYP" VARBINARY(255),
  "AAABGYYQ" varchar(4) NOT NULL default '0000',
  "AAABGYYR" VARBINARY(255),
  "AAABGYYS" int(11) NOT NULL default '0',
  "AAABGYYT" varchar(1) NOT NULL default ' ',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  "AAABGYYV" varchar(3) NOT NULL default ' ',
  "AAABKZVA" varchar(4) NOT NULL default ' ',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEN" decimal(21,2) NOT NULL default '0.00',
  "AAABGZKF" varchar(8) NOT NULL default '00000000',
  "AAABGZKC" decimal(22,6) NOT NULL default '0.000000',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEP" varchar(5) NOT NULL default ' ',
  "AAABGYZF" decimal(21,2) NOT NULL default '0.00',
  "AAABGZES" decimal(21,2) NOT NULL default '0.00',
  "AAABGZER" varchar(5) NOT NULL default ' ',
  "AAABGZEW" varchar(8) NOT NULL default '00000000',
  "AAABGZIW" varchar(8) NOT NULL default '00000000',
  "AAABGZIX" varchar(1) NOT NULL default ' ',
  "AAABGYZL" decimal(9,5) NOT NULL default '0.00000',
  "AAABGYZM" varchar(1) NOT NULL default ' ',
  "AAABGYZN" decimal(9,5) NOT NULL default '0.00000',
  "AAABLACY" varchar(5) NOT NULL default ' ',
  "AAABLACZ" varchar(5) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABGYZR" varchar(12) NOT NULL default ' ',
  "AAABGYZS" varchar(8) NOT NULL default '00000000',
  "AAABGYZT" varchar(6) NOT NULL default '000000',
  "AAABGYZU" varchar(1) NOT NULL default ' ',
  "AAABMEBC" varchar(4) NOT NULL default ' ',
  "AAABMDHO" varchar(1) NOT NULL default '0',
  "AAABMDHP" varchar(1) NOT NULL default ' ',
  "AAABMDHQ" varchar(1) NOT NULL default ' ',
  "AAABMDZN" decimal(13,2) NOT NULL default '0.00',
  "AAABMDZM" varchar(4) NOT NULL default ' ',
  "AAABMEFM" varchar(8) NOT NULL default '00000000',
  "AAABMDHW" varchar(1) NOT NULL default '0',
  "AAABMDHX" varchar(1) NOT NULL default ' ',
  "AAABMEFN" varchar(8) NOT NULL default '00000000',
  "AAABMDHZ" varchar(1) NOT NULL default '0',
  "AAABMDIA" varchar(1) NOT NULL default ' ',
  "AAABMDZT" varchar(8) NOT NULL default '00000000',
  "AAABMEFR" varchar(1) NOT NULL default '0',
  "AAABMDZV" varchar(1) NOT NULL default ' ',
  "AAABMEAA" varchar(8) NOT NULL default '00000000',
  "AAABMEAC" varchar(1) NOT NULL default ' ',
  "AAABMDIK" varchar(8) NOT NULL default '00000000',
  "AAABMDIL" varchar(1) NOT NULL default '0',
  "AAABMDIM" varchar(1) NOT NULL default ' ',
  "AAABMPTJ" int(11) NOT NULL default '0',
  "AAABMPTI" double NOT NULL default '0',
  "AAABMDIP" varchar(6) NOT NULL default '000000',
  "AAABMDZR" varchar(3) NOT NULL default '000',
  "AAABMDZS" varchar(3) NOT NULL default '000',
  "AAABMDZP" varchar(1) NOT NULL default '0',
  "AAABMPTH" varchar(1) NOT NULL default '0',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAABLCXZ" varchar(1) NOT NULL default ' ',
  "AAABMDIW" varchar(8) NOT NULL default '00000000',
  "AAABMDIX" varchar(1) NOT NULL default '0',
  "AAABMDIY" varchar(1) NOT NULL default ' ',
  "AAABMDIZ" varchar(8) NOT NULL default '00000000',
  "AAABMDJA" varchar(1) NOT NULL default '0',
  "AAABMDJB" varchar(1) NOT NULL default ' ',
  "AAABMEFI" varchar(5) NOT NULL default ' ',
  "AAABMDZE" decimal(10,7) NOT NULL default '0.0000000',
  "AAABMDZB" varchar(8) NOT NULL default '00000000',
  "AAABMEAI" varchar(8) NOT NULL default '00000000',
  "AAABMDZH" decimal(17,2) NOT NULL default '0.00',
  "AAABMDZI" decimal(15,6) NOT NULL default '0.000000',
  "AAABLABK" varchar(1) NOT NULL default ' ',
  "AAABLABL" varchar(1) NOT NULL default ' ',
  "AAABLACN" varchar(1) NOT NULL default ' ',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  "AAABLCYE" varchar(2) NOT NULL default ' ',
  "AAABMDYP" varchar(8) NOT NULL default '00000000',
  "AAABMDYS" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABGZKK","AAABGZKH"),
  KEY "AAABGYYI" ("AAABGYYP"(255),"AAABGYYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGYYL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABGZKK" varchar(32) NOT NULL default ' ',
  "AAABGYYP" VARBINARY(255),
  "AAABGYYQ" varchar(4) NOT NULL default '0000',
  "AAABGYYR" VARBINARY(255),
  "AAABGYYS" int(11) NOT NULL default '0',
  "AAABGYYT" varchar(1) NOT NULL default ' ',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  "AAABGYYV" varchar(3) NOT NULL default ' ',
  "AAABKZVA" varchar(4) NOT NULL default ' ',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEN" decimal(21,2) NOT NULL default '0.00',
  "AAABGZKF" varchar(8) NOT NULL default '00000000',
  "AAABGZKC" decimal(22,6) NOT NULL default '0.000000',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEP" varchar(5) NOT NULL default ' ',
  "AAABGYZF" decimal(21,2) NOT NULL default '0.00',
  "AAABGZES" decimal(21,2) NOT NULL default '0.00',
  "AAABGZER" varchar(5) NOT NULL default ' ',
  "AAABGZEW" varchar(8) NOT NULL default '00000000',
  "AAABGZIW" varchar(8) NOT NULL default '00000000',
  "AAABGZIX" varchar(1) NOT NULL default ' ',
  "AAABGYZL" decimal(9,5) NOT NULL default '0.00000',
  "AAABGYZM" varchar(1) NOT NULL default ' ',
  "AAABGYZN" decimal(9,5) NOT NULL default '0.00000',
  "AAABLACY" varchar(5) NOT NULL default ' ',
  "AAABLACZ" varchar(5) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABGYZR" varchar(12) NOT NULL default ' ',
  "AAABGYZS" varchar(8) NOT NULL default '00000000',
  "AAABGYZT" varchar(6) NOT NULL default '000000',
  "AAABGYZU" varchar(1) NOT NULL default ' ',
  "AAABMEBC" varchar(4) NOT NULL default ' ',
  "AAABMDHO" varchar(1) NOT NULL default '0',
  "AAABMDHP" varchar(1) NOT NULL default ' ',
  "AAABMDHQ" varchar(1) NOT NULL default ' ',
  "AAABMDZN" decimal(13,2) NOT NULL default '0.00',
  "AAABMDZM" varchar(4) NOT NULL default ' ',
  "AAABMEFM" varchar(8) NOT NULL default '00000000',
  "AAABMDHW" varchar(1) NOT NULL default '0',
  "AAABMDHX" varchar(1) NOT NULL default ' ',
  "AAABMEFN" varchar(8) NOT NULL default '00000000',
  "AAABMDHZ" varchar(1) NOT NULL default '0',
  "AAABMDIA" varchar(1) NOT NULL default ' ',
  "AAABMDZT" varchar(8) NOT NULL default '00000000',
  "AAABMEFR" varchar(1) NOT NULL default '0',
  "AAABMDZV" varchar(1) NOT NULL default ' ',
  "AAABMEAA" varchar(8) NOT NULL default '00000000',
  "AAABMEAC" varchar(1) NOT NULL default ' ',
  "AAABMDIK" varchar(8) NOT NULL default '00000000',
  "AAABMDIL" varchar(1) NOT NULL default '0',
  "AAABMDIM" varchar(1) NOT NULL default ' ',
  "AAABMPTJ" int(11) NOT NULL default '0',
  "AAABMPTI" double NOT NULL default '0',
  "AAABMDIP" varchar(6) NOT NULL default '000000',
  "AAABMDZR" varchar(3) NOT NULL default '000',
  "AAABMDZS" varchar(3) NOT NULL default '000',
  "AAABMDZP" varchar(1) NOT NULL default '0',
  "AAABMPTH" varchar(1) NOT NULL default '0',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAABLCXZ" varchar(1) NOT NULL default ' ',
  "AAABMDIW" varchar(8) NOT NULL default '00000000',
  "AAABMDIX" varchar(1) NOT NULL default '0',
  "AAABMDIY" varchar(1) NOT NULL default ' ',
  "AAABMDIZ" varchar(8) NOT NULL default '00000000',
  "AAABMDJA" varchar(1) NOT NULL default '0',
  "AAABMDJB" varchar(1) NOT NULL default ' ',
  "AAABMEFI" varchar(5) NOT NULL default ' ',
  "AAABMDZE" decimal(10,7) NOT NULL default '0.0000000',
  "AAABMDZB" varchar(8) NOT NULL default '00000000',
  "AAABMEAI" varchar(8) NOT NULL default '00000000',
  "AAABMDZH" decimal(17,2) NOT NULL default '0.00',
  "AAABMDZI" decimal(15,6) NOT NULL default '0.000000',
  "AAABLABK" varchar(1) NOT NULL default ' ',
  "AAABLABL" varchar(1) NOT NULL default ' ',
  "AAABLACN" varchar(1) NOT NULL default ' ',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  "AAABLCYE" varchar(2) NOT NULL default ' ',
  "AAABMDYP" varchar(8) NOT NULL default '00000000',
  "AAABMDYS" varchar(2) NOT NULL default '00',
  "AAABGZBR" varchar(12) NOT NULL default ' ',
  "AAABGZBS" varchar(8) NOT NULL default '00000000',
  "AAABGZBT" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"),
  KEY "AAABGZBW" ("AAABMRIL","AAABGZKK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZBZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABJGWE" varchar(10) NOT NULL default ' ',
  "AAABJGWF" varchar(13) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABLANY" varchar(2) NOT NULL default ' ',
  "AAABLANC" varchar(8) NOT NULL default '00000000',
  "AAABLAND" varchar(8) NOT NULL default '00000000',
  "AAABLAON" varchar(4) NOT NULL default '0000',
  "AAABLAOP" varchar(2) NOT NULL default ' ',
  "AAABLANH" varchar(1) NOT NULL default ' ',
  "AAABLAOQ" varchar(1) NOT NULL default ' ',
  "AAABLAOO" varchar(4) NOT NULL default ' ',
  "AAABLZDP" varchar(12) NOT NULL default ' ',
  "AAABLCIO" varchar(8) NOT NULL default '00000000',
  "AAABLCIP" varchar(6) NOT NULL default '000000',
  "AAABLCIQ" varchar(10) NOT NULL default ' ',
  "AAABLCIS" varchar(12) NOT NULL default ' ',
  "AAABLCIT" varchar(8) NOT NULL default '00000000',
  "AAABLCIU" varchar(6) NOT NULL default '000000',
  "AAABLCIV" varchar(10) NOT NULL default ' ',
  "AAABGZCW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"),
  UNIQUE KEY "AAABGZCZ" ("AAABMRIL","AAABJGWD","AAABJGWE","AAABJGWF"),
  KEY "AAABGZDE" ("AAABMRIL","AAABLXTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZDH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGZED" varchar(6) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABJGWF" varchar(13) NOT NULL default ' ',
  "AAABGZDN" varchar(10) NOT NULL default ' ',
  "AAABGZKF" varchar(8) NOT NULL default '00000000',
  "AAABGZDP" decimal(21,2) NOT NULL default '0.00',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  "AAABGZDT" varchar(2) NOT NULL default ' ',
  "AAABLAXK" VARBINARY(255),
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABKXML" varchar(8) NOT NULL default '00000000',
  "AAABKXMM" varchar(6) NOT NULL default '000000',
  "AAABKXMN" varchar(12) NOT NULL default ' ',
  "AAABKXMO" varchar(8) NOT NULL default '00000000',
  "AAABKXMP" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABJGWD","AAABGZED")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZEE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" VARBINARY(255) NOT NULL,
  "AAABGZFF" VARBINARY(255),
  "AAABGZKC" decimal(22,6) NOT NULL default '0.000000',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEN" decimal(21,2) NOT NULL default '0.00',
  "AAABKZVA" varchar(4) NOT NULL default ' ',
  "AAABGZEP" varchar(5) NOT NULL default ' ',
  "AAABGZEQ" decimal(21,2) NOT NULL default '0.00',
  "AAABGZER" varchar(5) NOT NULL default ' ',
  "AAABGZES" decimal(21,2) NOT NULL default '0.00',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABGZEU" varchar(5) NOT NULL default ' ',
  "AAABGZEV" varchar(5) NOT NULL default ' ',
  "AAABGZEW" varchar(8) NOT NULL default '00000000',
  "AAABJQXA" varchar(8) NOT NULL default '00000000',
  "AAABKSXJ" int(11) NOT NULL default '0',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  "AAABGZIY" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"(255)),
  KEY "AAABGZFD" ("AAABMRIL","AAABGZFF"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZFG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" VARBINARY(255) NOT NULL,
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGZFZ" varchar(10) NOT NULL default '0000000000',
  "AAABJMEG" varchar(4) NOT NULL default '0000',
  "AAABGZKF" varchar(8) NOT NULL default '00000000',
  "AAABGZJF" VARBINARY(255),
  "AAABGZIT" varchar(1) NOT NULL default ' ',
  "AAABGZIZ" varchar(8) NOT NULL default '00000000',
  "AAABGZFQ" VARBINARY(255),
  "AAABGZFR" VARBINARY(255),
  "AAABGZIR" VARBINARY(255),
  "AAABGZIS" VARBINARY(255),
  "AAABGZIU" VARBINARY(255),
  "AAABGZIV" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAABGZKH"(255)),
  KEY "AAABGZFY" ("AAABGZFZ","AAABJGWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZGB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZGR" int(11) NOT NULL default '0',
  "AAABGZHZ" varchar(10) NOT NULL default '0000000000',
  "AAABJGWF" varchar(13) NOT NULL default ' ',
  "AAABGZGG" varchar(15) NOT NULL default ' ',
  "AAABKZRP" varchar(20) NOT NULL default ' ',
  "AAABGZGI" decimal(22,6) NOT NULL default '0.000000',
  "AAABGZGJ" decimal(21,2) NOT NULL default '0.00',
  "AAABGZGK" decimal(21,2) NOT NULL default '0.00',
  "AAABGZGL" varchar(5) NOT NULL default ' ',
  "AAABGZGM" varchar(10) NOT NULL default ' ',
  "AAABGZGN" varchar(50) NOT NULL default ' ',
  "AAABGZGO" varchar(15) NOT NULL default ' ',
  "AAABGZGP" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZGR"),
  KEY "AAABGZGS" ("AAABGZHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZGU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZHZ" varchar(10) NOT NULL default '0000000000',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAABJGWE" varchar(10) NOT NULL default ' ',
  "AAABGZGZ" varchar(10) NOT NULL default ' ',
  "AAABGZHA" varchar(15) NOT NULL default ' ',
  "AAABGZKF" varchar(8) NOT NULL default '00000000',
  "AAABMRFO" varchar(5) NOT NULL default ' ',
  "AAABGZHD" varchar(15) NOT NULL default ' ',
  "AAABGZHE" varchar(50) NOT NULL default ' ',
  "AAABIAKJ" varchar(128) NOT NULL default ' ',
  "AAABIMHJ" varchar(18) NOT NULL default ' ',
  "AAABGZHH" varchar(20) NOT NULL default ' ',
  "AAABGZHI" varchar(20) NOT NULL default ' ',
  "AAABLZDP" varchar(12) NOT NULL default ' ',
  "AAABLCIO" varchar(8) NOT NULL default '00000000',
  "AAABLCIP" varchar(6) NOT NULL default '000000',
  "AAABLCIQ" varchar(10) NOT NULL default ' ',
  "AAABLCIS" varchar(12) NOT NULL default ' ',
  "AAABLCIT" varchar(8) NOT NULL default '00000000',
  "AAABLCIU" varchar(6) NOT NULL default '000000',
  "AAABLCIV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZHT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZHZ" varchar(10) NOT NULL default '0000000000',
  "AAABGZIA" int(11) NOT NULL default '0',
  "AAABGZHX" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZHZ","AAABGZIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZIB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABGZIE" VARBINARY(255),
  "AAABGZIF" VARBINARY(255),
  "AAABGZIG" VARBINARY(255),
  "AAABGZIH" VARBINARY(255),
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABJGWF" varchar(13) NOT NULL default ' ',
  "AAABGZIK" varchar(10) NOT NULL default ' ',
  "AAABGZIL" varchar(10) NOT NULL default ' ',
  "AAABGZKF" varchar(8) NOT NULL default '00000000',
  "AAABGZKC" decimal(22,6) NOT NULL default '0.000000',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  "AAABGZJF" VARBINARY(255),
  "AAABGZIR" VARBINARY(255),
  "AAABGZIS" VARBINARY(255),
  "AAABGZIT" varchar(1) NOT NULL default ' ',
  "AAABGZIU" VARBINARY(255),
  "AAABGZIV" VARBINARY(255),
  "AAABGZIW" varchar(8) NOT NULL default '00000000',
  "AAABGZIX" varchar(1) NOT NULL default ' ',
  "AAABGZIY" varchar(18) NOT NULL default ' ',
  "AAABGZIZ" varchar(8) NOT NULL default '00000000',
  "AAABIZKK" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"),
  KEY "AAABGZJD" ("AAABMRIL","AAABGZJF"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZJG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABJGWE" varchar(10) NOT NULL default ' ',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABGZJL" varchar(5) NOT NULL default ' ',
  "AAABGZJM" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWD","AAABJGWE","AAABLPQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZJR" (
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLUZL" varchar(30) NOT NULL default ' ',
  "AAABMKFC" varchar(2) NOT NULL default ' ',
  "AAABGZJV" tinyint(3) unsigned NOT NULL default '0',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZJY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABGZKK" varchar(32) NOT NULL default ' ',
  "AAABGZKC" decimal(22,6) NOT NULL default '0.000000',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  "AAABGZKF" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"),
  KEY "AAABGZKI" ("AAABMRIL","AAABGZKK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZKL" (
  "AAABLENY" varchar(30) NOT NULL default ' ',
  "AAABLENX" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABLENY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZKP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABKYFK" varchar(10) NOT NULL default ' ',
  "AAABLGXS" varchar(3) NOT NULL default ' ',
  "AAABGZKU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABKYFK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZKY" (
  "AAABLWIT" varchar(30) NOT NULL default ' ',
  "AAABLWIV" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABLWIT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZLC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGZRT" varchar(2) NOT NULL default ' ',
  "AAABMKKY" varchar(8) NOT NULL default '00000000',
  "AAABGZLG" decimal(4,1) NOT NULL default '0.0',
  "AAABGZLH" decimal(4,1) NOT NULL default '0.0',
  "AAABGZLI" decimal(4,1) NOT NULL default '0.0',
  "AAABGZLJ" decimal(4,1) NOT NULL default '0.0',
  PRIMARY KEY ("AAABLUXQ","AAABGZRT","AAABMKKY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZLN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGZMF" varchar(2) NOT NULL default ' ',
  "AAABGZLQ" varchar(1) NOT NULL default ' ',
  "AAABGZLR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGZMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZLU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGZMF" varchar(2) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGZMF","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZMC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZMH" varchar(2) NOT NULL default ' ',
  "AAABGZMF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZMI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKVKH" varchar(6) NOT NULL default ' ',
  "AAABGZML" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKVKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZMO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGZNL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGZNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZMT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGZNL" varchar(2) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGZNL","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZNB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGZRT" varchar(2) NOT NULL default ' ',
  "AAABGZNL" varchar(2) NOT NULL default ' ',
  "AAABMKKY" varchar(8) NOT NULL default '00000000',
  "AAABGZNG" decimal(2,0) NOT NULL default '0',
  "AAABGZNH" decimal(2,0) NOT NULL default '0',
  "AAABGZNI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGZRT","AAABGZNL","AAABMKKY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZNN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGZRB" varchar(2) NOT NULL default ' ',
  "AAABMKKY" varchar(8) NOT NULL default '00000000',
  "AAABGZNX" decimal(4,1) NOT NULL default '0.0',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABGZNT" decimal(17,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABLUXQ","AAABGZRB","AAABMKKY","AAABGZNX","AAABLPQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZNZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGZRT" varchar(2) NOT NULL default ' ',
  "AAABGZRB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGZRT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZOF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABGZRT" varchar(2) NOT NULL default ' ',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABGZQB" varchar(4) NOT NULL default ' ',
  "AAABGZOU" varchar(30) NOT NULL default ' ',
  "AAABGZOV" varchar(30) NOT NULL default ' ',
  "AAABGZQC" varchar(4) NOT NULL default '0000',
  "AAABIKHU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLFZK","AAABGZRT","AAABHFHO","AAABGZQB","AAABGZOU","AAABGZOV","AAABGZQC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZOX" (
  "AAABGZPP" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABGZPP","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZPD" (
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABGZRZ" varchar(3) NOT NULL default '000',
  "AAABGZRX" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABLEHK","AAABGZRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZPJ" (
  "AAABGZPP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGZPP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZPM" (
  "AAABGZPP" varchar(1) NOT NULL default ' ',
  "AAABGZPQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGZPP","AAABGZPQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZPR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGZRT" varchar(2) NOT NULL default ' ',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABGZQB" varchar(4) NOT NULL default ' ',
  "AAABGZQC" varchar(4) NOT NULL default '0000',
  "AAABGZPX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGZRT","AAABHFHO","AAABGZQB","AAABGZQC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZQD" (
  "AAABGZQH" varchar(8) NOT NULL default '00000000',
  "AAABGZQF" decimal(4,1) NOT NULL default '0.0',
  "AAABGZQG" decimal(3,0) NOT NULL default '0',
  PRIMARY KEY ("AAABGZQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZQI" (
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABGZRZ" varchar(3) NOT NULL default '000',
  "AAABGZQP" varchar(3) NOT NULL default '000',
  "AAABGZRX" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABLEHK","AAABGZRZ","AAABGZQP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZQQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGZRB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGZRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZQV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGZRB" varchar(2) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGZRB","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZRD" (
  "AAABGZRT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABGZRT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZRG" (
  "AAABGZRT" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABGZRT","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZRM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABGZRT" varchar(2) NOT NULL default ' ',
  "AAABLYCI" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABHFHO","AAABGZRT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZRU" (
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABGZRZ" varchar(3) NOT NULL default '000',
  "AAABGZRX" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABLEHK","AAABGZRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZSA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGZSF" varchar(4) NOT NULL default ' ',
  "AAABGZSD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGZSF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZSG" (
  "AAABJCXS" varchar(4) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABGZSJ" varchar(1) NOT NULL default ' ',
  "AAABHVRQ" varchar(30) NOT NULL default ' ',
  "AAABGZSL" varchar(30) NOT NULL default ' ',
  "AAABGZSM" varchar(30) NOT NULL default ' ',
  "AAABGZSN" varchar(30) NOT NULL default ' ',
  "AAABGZSO" varchar(30) NOT NULL default ' ',
  "AAABGZSP" varchar(1) NOT NULL default ' ',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  "AAABHZPJ" varchar(4) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABHZPN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABJCXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZSZ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJCXS" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABJCXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZTF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLZMW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABLZMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZTM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLZMW" varchar(4) NOT NULL default ' ',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABGZTR" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABLZMW","AAABLKXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZTW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLZMW" varchar(4) NOT NULL default ' ',
  "AAABGZUB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCVI","AAABLZMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZUG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKQPE" varchar(2) NOT NULL default '00',
  "AAABLRXL" varchar(1) NOT NULL default ' ',
  "AAABHEVN" varchar(1) NOT NULL default ' ',
  "AAABGZUL" varchar(1) NOT NULL default ' ',
  "AAABGZUM" varchar(1) NOT NULL default ' ',
  "AAABGZUN" varchar(1) NOT NULL default ' ',
  "AAABGZUO" varchar(1) NOT NULL default ' ',
  "AAABGZUP" varchar(1) NOT NULL default ' ',
  "AAABGZUQ" varchar(1) NOT NULL default ' ',
  "AAABGZUR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKQPE","AAABLRXL","AAABHEVN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZUW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABGZVC" decimal(18,2) NOT NULL default '0.00',
  "AAABGZVD" decimal(18,2) NOT NULL default '0.00',
  "AAABGZVE" decimal(8,0) NOT NULL default '0',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABGZVG" decimal(8,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMMVH","AAABMREX","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZVL" (
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZVP" (
  "AAABGZWJ" varchar(30) NOT NULL default ' ',
  "AAABLOLT" varchar(10) NOT NULL default ' ',
  "AAABGZVS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGZWJ","AAABLOLT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZVV" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGZWJ" varchar(30) NOT NULL default ' ',
  "AAABLOLT" varchar(10) NOT NULL default ' ',
  "AAABGZVZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABGZWJ","AAABLOLT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZWD" (
  "AAABGZWJ" varchar(30) NOT NULL default ' ',
  "AAABLOLT" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABGZWH" varchar(1) NOT NULL default ' ',
  "AAABHLPB" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABGZWJ","AAABLOLT","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZWM" (
  "AAABKWJY" varchar(8) NOT NULL default ' ',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABGZWU" varchar(4) NOT NULL default ' ',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKWJY","AAABMPMH","AAABGZWU","AAABLMNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZWW" (
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZXA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQPX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZXF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLQPX" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLQPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZXN" (
  "AAABGZXX" varchar(3) NOT NULL default ' ',
  "AAABGZXP" varchar(1) NOT NULL default ' ',
  "AAABGZXQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGZXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZXS" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGZXX" varchar(3) NOT NULL default ' ',
  "AAABGZXV" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABGZXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZXY" (
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABGZYB" varchar(1) NOT NULL default ' ',
  "AAABGZYC" varchar(1) NOT NULL default ' ',
  "AAABGZYD" varchar(1) NOT NULL default ' ',
  "AAABGZYE" varchar(1) NOT NULL default ' ',
  "AAABGZYF" varchar(1) NOT NULL default ' ',
  "AAABGZYG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNMH","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZYJ" (
  "AAABGZYR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGZYR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZYM" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGZYR" varchar(30) NOT NULL default ' ',
  "AAABGZYP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABGZYR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZYS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGZZF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGZZF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZYX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGZZF" varchar(4) NOT NULL default ' ',
  "AAABGZZB" varchar(30) NOT NULL default ' ',
  "AAABGZZC" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABGZZF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZZG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABMREF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLEHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZZM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGZZQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLEHK","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGZZU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHABT" varchar(4) NOT NULL default ' ',
  "AAABHABJ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABHABT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAAA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHABT" varchar(4) NOT NULL default ' ',
  "AAABHABQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABHABT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAAI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMECK" varchar(4) NOT NULL default ' ',
  "AAABHHRK" varchar(1) NOT NULL default ' ',
  "AAABIGSL" varchar(1) NOT NULL default ' ',
  "AAABHAAN" varchar(1) NOT NULL default ' ',
  "AAABHAAO" varchar(1) NOT NULL default ' ',
  "AAABHAAP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMECK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAAS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMECK" varchar(4) NOT NULL default ' ',
  "AAABHAAW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABMECK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHABA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHABF" varchar(4) NOT NULL default ' ',
  "AAABHABD" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABHABF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHABG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHABT" varchar(4) NOT NULL default ' ',
  "AAABHABJ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABHABT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHABM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHABT" varchar(4) NOT NULL default ' ',
  "AAABHABQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABHABT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHABU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLSDL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLSDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHABZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLSDL" varchar(4) NOT NULL default ' ',
  "AAABHACD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABLSDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHACH" (
  "AAABHAGA" varchar(4) NOT NULL default ' ',
  "AAABHACT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHAGA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHACL" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHAGA" varchar(4) NOT NULL default ' ',
  "AAABHACY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHAGA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHACR" (
  "AAABHADA" varchar(4) NOT NULL default ' ',
  "AAABHACT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHADA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHACV" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHADA" varchar(4) NOT NULL default ' ',
  "AAABHACY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHADA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHADB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHADI" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHADF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJXCE" varchar(1) NOT NULL default ' ',
  "AAABHADI" varchar(3) NOT NULL default ' ',
  "AAABHADJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJXCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHADM" (
  "AAABLUSV" varchar(20) NOT NULL default ' ',
  "AAABMRFO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHADQ" (
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABHEWL" varchar(5) NOT NULL default '00000',
  "AAABHAEB" varchar(30) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  "AAABLUEQ" varchar(30) NOT NULL default ' ',
  "AAABMLZS" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLUER","AAABHEWL","AAABHAEB","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAED" (
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABHAEW" varchar(1) NOT NULL default ' ',
  "AAABLMOZ" varchar(4) NOT NULL default '0000',
  "AAABMNMH" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(10) NOT NULL default ' ',
  "AAABHAEJ" varchar(10) NOT NULL default ' ',
  "AAABHAEK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPJD","AAABHAEW","AAABLMOZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAEO" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABHAEW" varchar(1) NOT NULL default ' ',
  "AAABLMOZ" varchar(4) NOT NULL default '0000',
  "AAABHAET" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLPJD","AAABHAEW","AAABLMOZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAEY" (
  "AAABHAFT" varchar(10) NOT NULL default ' ',
  "AAABHAFA" varchar(1) NOT NULL default ' ',
  "AAABHAFB" varchar(1) NOT NULL default ' ',
  "AAABHAFC" varchar(1) NOT NULL default ' ',
  "AAABHAFD" varchar(1) NOT NULL default ' ',
  "AAABHAFE" varchar(1) NOT NULL default ' ',
  "AAABHAFF" varchar(1) NOT NULL default ' ',
  "AAABHAFG" varchar(1) NOT NULL default ' ',
  "AAABHAFH" varchar(1) NOT NULL default ' ',
  "AAABHAFI" varchar(1) NOT NULL default ' ',
  "AAABHAFJ" varchar(1) NOT NULL default ' ',
  "AAABHAFK" varchar(1) NOT NULL default ' ',
  "AAABHAFL" varchar(1) NOT NULL default ' ',
  "AAABHAFM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHAFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAFO" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHAFT" varchar(10) NOT NULL default ' ',
  "AAABHAFR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHAFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAFU" (
  "AAABMNMH" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(10) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABHAFY" varchar(30) NOT NULL default ' ',
  "AAABHAUM" varchar(30) NOT NULL default ' ',
  "AAABHAGA" varchar(4) NOT NULL default ' ',
  "AAABHAGB" varchar(1) NOT NULL default ' ',
  "AAABHAGC" varchar(30) NOT NULL default ' ',
  "AAABHAGD" varchar(30) NOT NULL default ' ',
  "AAABHAGE" varchar(30) NOT NULL default ' ',
  "AAABHAGF" int(11) NOT NULL default '0',
  "AAABJSDF" int(11) NOT NULL default '0',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABHAGI" varchar(30) NOT NULL default ' ',
  "AAABHAGJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNMH","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAGM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMNMH" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(10) NOT NULL default ' ',
  "AAABHAGU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMNMH","AAABMREF","AAABHAGU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAGV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHAIA" varchar(10) NOT NULL default ' ',
  "AAABHAIB" varchar(10) NOT NULL default ' ',
  "AAABHAHM" decimal(3,1) NOT NULL default '0.0',
  PRIMARY KEY ("AAABLUXQ","AAABHAIA","AAABHAIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAHD" (
  "AAABMNMH" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNMH","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAHI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMNMH" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(10) NOT NULL default ' ',
  "AAABHAHM" decimal(3,1) NOT NULL default '0.0',
  "AAABHAHN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMNMH","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAHR" (
  "AAABHAIA" varchar(10) NOT NULL default ' ',
  "AAABHAIB" varchar(10) NOT NULL default ' ',
  "AAABMNMH" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(10) NOT NULL default ' ',
  "AAABHAHW" varchar(1) NOT NULL default ' ',
  "AAABHAHX" varchar(1) NOT NULL default ' ',
  "AAABHAHY" varchar(1) NOT NULL default ' ',
  "AAABHAHZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHAIA","AAABHAIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAIC" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMNMH" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMNMH","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAIK" (
  "AAABJXCE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJXCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAIN" (
  "AAABJXCE" varchar(1) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJXCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAIR" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJXCE" varchar(1) NOT NULL default ' ',
  "AAABHAIU" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABJXCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAIX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHAJJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHAJJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAJC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHAJJ" varchar(4) NOT NULL default ' ',
  "AAABHAJG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHAJJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAJK" (
  "AAABHAJN" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABHAJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAJO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHAKB" varchar(4) NOT NULL default ' ',
  "AAABHAJR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABHAKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAJU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHAKB" varchar(4) NOT NULL default ' ',
  "AAABHAJY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABHAKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAKC" (
  "AAABLUER" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAKF" (
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABHEWL" varchar(5) NOT NULL default '00000',
  "AAABLUEU" varchar(30) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER","AAABHEWL","AAABLUEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAKN" (
  "AAABHAKU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHAKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAKQ" (
  "AAABHAKU" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJDMJ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABHAKU","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAKW" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABLUDG" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLUER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHALC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABHALK" varchar(4) NOT NULL default ' ',
  "AAABKKES" varchar(10) NOT NULL default ' ',
  "AAABHALH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZBM","AAABHALK","AAABKKES")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHALM" (
  "AAABHALW" varchar(1) NOT NULL default ' ',
  "AAABLQTM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHALW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHALQ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHALW" varchar(1) NOT NULL default ' ',
  "AAABHALT" varchar(12) NOT NULL default ' ',
  "AAABHALU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABHALW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHALX" (
  "AAABHAMA" varchar(1) NOT NULL default ' ',
  "AAABHALZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHAMA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAMB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHAQM" varchar(1) NOT NULL default ' ',
  "AAABHAOL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHAQM","AAABHAOL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAMI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHAQM" varchar(1) NOT NULL default ' ',
  "AAABHAOL" varchar(2) NOT NULL default ' ',
  "AAABLPWW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHAQM","AAABHAOL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAMS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHAQM" varchar(1) NOT NULL default ' ',
  "AAABHAOL" varchar(2) NOT NULL default ' ',
  "AAABHAOM" varchar(3) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABJVQM" varchar(15) NOT NULL default ' ',
  "AAABHAPO" varchar(24) NOT NULL default ' ',
  "AAABHAPP" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHAQM","AAABHAOL","AAABHAOM","AAABMCRJ","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHANI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHAQM" varchar(1) NOT NULL default ' ',
  "AAABHAOL" varchar(2) NOT NULL default ' ',
  "AAABHAOM" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHAQM","AAABHAOL","AAABHAOM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHANR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHAQM" varchar(1) NOT NULL default ' ',
  "AAABHAOL" varchar(2) NOT NULL default ' ',
  "AAABHAOM" varchar(3) NOT NULL default ' ',
  "AAABHANX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHAQM","AAABHAOL","AAABHAOM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAOD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHAQM" varchar(1) NOT NULL default ' ',
  "AAABHAOL" varchar(2) NOT NULL default ' ',
  "AAABHAOM" varchar(3) NOT NULL default ' ',
  "AAABHAPS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHAQM","AAABHAOL","AAABHAOM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAON" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHAQM" varchar(1) NOT NULL default ' ',
  "AAABHAPS" varchar(4) NOT NULL default ' ',
  "AAABHAQN" varchar(2) NOT NULL default ' ',
  "AAABHAOS" varchar(1) NOT NULL default ' ',
  "AAABLRJK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHAQM","AAABHAPS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAOX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHAQM" varchar(1) NOT NULL default ' ',
  "AAABHAPS" varchar(4) NOT NULL default ' ',
  "AAABJFBB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHAQM","AAABHAPS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHAQM" varchar(1) NOT NULL default ' ',
  "AAABHAPS" varchar(4) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABJVQM" varchar(15) NOT NULL default ' ',
  "AAABHAPO" varchar(24) NOT NULL default ' ',
  "AAABHAPP" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHAQM","AAABHAPS","AAABMCRJ","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAPV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHAQM" varchar(1) NOT NULL default ' ',
  "AAABHAQN" varchar(2) NOT NULL default ' ',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHAQM","AAABHAQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAQE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHAQM" varchar(1) NOT NULL default ' ',
  "AAABHAQN" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHAQM","AAABHAQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAQO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHASA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHASA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAQT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHASA" varchar(4) NOT NULL default ' ',
  "AAABLXIS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHASA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHARB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHASA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHASA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHARG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHASA" varchar(4) NOT NULL default ' ',
  "AAABLXIS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHASA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHARO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHASA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHASA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHART" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHASA" varchar(4) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHASA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHASB" (
  "AAABMLPV" varchar(10) NOT NULL default ' ',
  "AAABMMJT" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHASF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABHASI" varchar(2) NOT NULL default ' ',
  "AAABHASJ" varchar(2) NOT NULL default ' ',
  "AAABLTLM" varchar(6) NOT NULL default '000000',
  "AAABHASL" varchar(1) NOT NULL default ' ',
  "AAABHASM" varchar(2) NOT NULL default '00',
  "AAABHASN" varchar(2) NOT NULL default '00',
  "AAABHASO" decimal(1,0) NOT NULL default '0',
  "AAABHASP" varchar(1) NOT NULL default '0',
  "AAABHASQ" varchar(2) NOT NULL default '00',
  "AAABHASR" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHASU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHATC" (
  "AAABKEON" int(11) NOT NULL default '0',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABHCAN" varchar(3) NOT NULL default '000',
  "AAABJAEL" varchar(8) NOT NULL default '00000000',
  "AAABHDHD" smallint(6) NOT NULL default '0',
  "AAABHATI" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABKEON","AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHATL" (
  "AAABHAUB" varchar(10) NOT NULL default ' ',
  "AAABHAUC" varchar(2) NOT NULL default ' ',
  "AAABJSDF" varchar(3) NOT NULL default ' ',
  "AAABHATP" varchar(1) NOT NULL default ' ',
  "AAABLOSB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABHAUB","AAABHAUC","AAABJSDF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHATU" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHAUB" varchar(10) NOT NULL default ' ',
  "AAABHAUC" varchar(2) NOT NULL default ' ',
  "AAABJSDF" varchar(3) NOT NULL default ' ',
  "AAABHATZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHAUB","AAABHAUC","AAABJSDF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAUE" (
  "AAABHBWQ" varchar(30) NOT NULL default ' ',
  "AAABHBWP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHBWQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAUI" (
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABLUBZ" varchar(30) NOT NULL default ' ',
  "AAABHAUM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAUP" (
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABHAVI" varchar(10) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLESS","AAABMGHE","AAABHAVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAUX" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(8) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default '0000',
  "AAABHAVI" varchar(10) NOT NULL default ' ',
  "AAABLPWW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABMLPV","AAABLESS","AAABMGHE","AAABHAVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAVJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHAYA" varchar(22) NOT NULL default ' ',
  "AAABHHKJ" varchar(10) NOT NULL default ' ',
  "AAABHAYC" smallint(6) NOT NULL default '0',
  "AAABHHGN" varchar(1) NOT NULL default ' ',
  "AAABHAWC" smallint(6) NOT NULL default '0',
  "AAABHHJX" int(11) NOT NULL default '0',
  "AAABHAVR" decimal(15,0) NOT NULL default '0',
  "AAABHAVS" varchar(6) NOT NULL default ' ',
  "AAABHAVT" decimal(15,0) NOT NULL default '0',
  "AAABHAVU" varchar(6) NOT NULL default ' ',
  "AAABHAVV" varchar(1) NOT NULL default ' ',
  "AAABHAVW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABHAYA","AAABHHKJ","AAABHAYC","AAABHHGN","AAABHAWC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAWD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHAYA" varchar(22) NOT NULL default ' ',
  "AAABHHKJ" varchar(10) NOT NULL default ' ',
  "AAABHAYC" smallint(6) NOT NULL default '0',
  "AAABHAWI" varchar(10) NOT NULL default ' ',
  "AAABHAWJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABHAYA","AAABHHKJ","AAABHAYC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAWO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHAYA" varchar(22) NOT NULL default ' ',
  "AAABHAWR" varchar(10) NOT NULL default ' ',
  "AAABHAWS" varchar(10) NOT NULL default ' ',
  "AAABHAWT" varchar(1) NOT NULL default ' ',
  "AAABHAWU" varchar(1) NOT NULL default ' ',
  "AAABHAWV" varchar(1) NOT NULL default ' ',
  "AAABHAWW" varchar(1) NOT NULL default ' ',
  "AAABHAWX" varchar(4) NOT NULL default ' ',
  "AAABHAWY" varchar(12) NOT NULL default ' ',
  "AAABHAWZ" decimal(15,0) NOT NULL default '0',
  "AAABHAXA" varchar(12) NOT NULL default ' ',
  "AAABHAXB" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABHAYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAXE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHAYA" varchar(22) NOT NULL default ' ',
  "AAABHAXO" varchar(10) NOT NULL default ' ',
  "AAABHAXP" smallint(6) NOT NULL default '0',
  "AAABHAXQ" varchar(10) NOT NULL default ' ',
  "AAABHAXR" smallint(6) NOT NULL default '0',
  "AAABHAXL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABHAYA","AAABHAXO","AAABHAXP","AAABHAXQ","AAABHAXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAXS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHAYA" varchar(22) NOT NULL default ' ',
  "AAABHHKJ" varchar(10) NOT NULL default ' ',
  "AAABHAYC" smallint(6) NOT NULL default '0',
  "AAABKORB" varchar(10) NOT NULL default ' ',
  "AAABKORD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABHAYA","AAABHHKJ","AAABHAYC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAYD" (
  "AAABHAYY" varchar(16) NOT NULL default ' ',
  "AAABHAYZ" varchar(16) NOT NULL default ' ',
  "AAABHAZA" varchar(16) NOT NULL default ' ',
  "AAABHAZB" varchar(16) NOT NULL default ' ',
  "AAABLMVF" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABHAYY","AAABHAYZ","AAABHAZA","AAABHAZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAYN" (
  "AAABHAYY" varchar(16) NOT NULL default ' ',
  "AAABHAYZ" varchar(16) NOT NULL default ' ',
  "AAABHAZA" varchar(16) NOT NULL default ' ',
  "AAABHAZB" varchar(16) NOT NULL default ' ',
  "AAABLUVE" varchar(2) NOT NULL default '00',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABLQEJ" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(255) NOT NULL default ' ',
  "AAABMNNA" varchar(255) NOT NULL default ' ',
  "AAABMNMP" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABHAYY","AAABHAYZ","AAABHAZA","AAABHAZB","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAZD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHAZS" varchar(12) NOT NULL default ' ',
  "AAABLQHQ" varchar(128) NOT NULL default ' ',
  "AAABHAZH" varchar(32) NOT NULL default ' ',
  "AAABHAZI" varchar(8) NOT NULL default ' ',
  "AAABKWJY" varchar(30) NOT NULL default ' ',
  "AAABMOIB" varchar(10) NOT NULL default ' ',
  "AAABHAZL" varchar(6) NOT NULL default '000000',
  "AAABHAZM" varchar(6) NOT NULL default '000000',
  "AAABHAZN" varchar(6) NOT NULL default '000000',
  "AAABHAZO" varchar(8) NOT NULL default ' ',
  "AAABHAZP" varchar(30) NOT NULL default ' ',
  "AAABHAZQ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHAZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAZT" (
  "AAABLUEU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHAZW" (
  "AAABLECD" varchar(10) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLFZT" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBAC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHBAO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHBAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBAH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHBAO" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHBAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBAP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMGGD" varchar(20) NOT NULL default ' ',
  "AAABHBJW" varchar(1) NOT NULL default ' ',
  "AAABHBJX" varchar(17) NOT NULL default ' ',
  "AAABMREF" varchar(8) NOT NULL default ' ',
  "AAABLOZE" varchar(2) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABHBAY" varchar(3) NOT NULL default '000',
  "AAABHBAZ" varchar(1) NOT NULL default ' ',
  "AAABMKGE" varchar(1) NOT NULL default ' ',
  "AAABHBCC" varchar(30) NOT NULL default ' ',
  "AAABHBCD" varchar(30) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABHBBE" varchar(1) NOT NULL default ' ',
  "AAABHBEH" varchar(30) NOT NULL default ' ',
  "AAABHBBG" varchar(3) NOT NULL default '000',
  "AAABLBEI" varchar(3) NOT NULL default '000',
  "AAABHBJM" varchar(3) NOT NULL default '000',
  "AAABMNMP" varchar(3) NOT NULL default '000',
  "AAABMMVY" varchar(3) NOT NULL default '000',
  "AAABHBJP" varchar(3) NOT NULL default '000',
  "AAABHBJQ" varchar(1) NOT NULL default ' ',
  "AAABHBJR" varchar(1) NOT NULL default ' ',
  "AAABHBJS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABMGGD","AAABHBJW","AAABHBJX","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBBV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMGGD" varchar(20) NOT NULL default ' ',
  "AAABHBJW" varchar(1) NOT NULL default ' ',
  "AAABHBJX" varchar(17) NOT NULL default ' ',
  "AAABMREF" varchar(8) NOT NULL default ' ',
  "AAABHBCC" varchar(30) NOT NULL default ' ',
  "AAABHBCD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMGGD","AAABHBJW","AAABHBJX","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBCK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLOZE" varchar(2) NOT NULL default ' ',
  "AAABHBEG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBCS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOZE" varchar(2) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABHBEG" varchar(30) NOT NULL default ' ',
  "AAABHBEH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBDA" (
  "AAABHBDC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHBDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBDD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMGGD" varchar(20) NOT NULL default ' ',
  "AAABHBJW" varchar(1) NOT NULL default ' ',
  "AAABHBJX" varchar(17) NOT NULL default ' ',
  "AAABMREF" varchar(8) NOT NULL default ' ',
  "AAABLOZE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABMGGD","AAABHBJW","AAABHBJX","AAABMREF","AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBDS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHBJW" varchar(1) NOT NULL default ' ',
  "AAABMNLW" varchar(4) NOT NULL default ' ',
  "AAABLOZE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHBJW","AAABMNLW","AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBEB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHBJW" varchar(1) NOT NULL default ' ',
  "AAABLOZE" varchar(2) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABHBEG" varchar(30) NOT NULL default ' ',
  "AAABHBEH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHBJW","AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBEL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMGGD" varchar(20) NOT NULL default ' ',
  "AAABHBJW" varchar(1) NOT NULL default ' ',
  "AAABHBJX" varchar(17) NOT NULL default ' ',
  "AAABHBFX" varchar(1) NOT NULL default ' ',
  "AAABHBFY" varchar(1) NOT NULL default ' ',
  "AAABKWUO" varchar(1) NOT NULL default ' ',
  "AAABMLZS" varchar(3) NOT NULL default '000',
  "AAABMKGE" varchar(1) NOT NULL default ' ',
  "AAABHBIE" varchar(20) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABMGGD","AAABHBJW","AAABHBJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBFD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMGGD" varchar(20) NOT NULL default ' ',
  "AAABHBJW" varchar(1) NOT NULL default ' ',
  "AAABHBJX" varchar(17) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHBIE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABMGGD","AAABHBJW","AAABHBJX","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBFR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMGGD" varchar(20) NOT NULL default ' ',
  "AAABHBJW" varchar(1) NOT NULL default ' ',
  "AAABHBJX" varchar(17) NOT NULL default ' ',
  "AAABHBFX" varchar(1) NOT NULL default ' ',
  "AAABHBFY" varchar(1) NOT NULL default ' ',
  "AAABKWUO" varchar(1) NOT NULL default ' ',
  "AAABMLZS" varchar(3) NOT NULL default '000',
  "AAABMKGE" varchar(1) NOT NULL default ' ',
  "AAABHBIE" varchar(20) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABMGGD","AAABHBJW","AAABHBJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBGJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMGGD" varchar(20) NOT NULL default ' ',
  "AAABHBJW" varchar(1) NOT NULL default ' ',
  "AAABHBJX" varchar(17) NOT NULL default ' ',
  "AAABMREF" varchar(8) NOT NULL default ' ',
  "AAABLBEI" varchar(3) NOT NULL default '000',
  "AAABHBJM" varchar(3) NOT NULL default '000',
  "AAABMNMP" varchar(3) NOT NULL default '000',
  "AAABMMVY" varchar(3) NOT NULL default '000',
  "AAABHBJP" varchar(3) NOT NULL default '000',
  "AAABHBJQ" varchar(1) NOT NULL default ' ',
  "AAABHBJR" varchar(1) NOT NULL default ' ',
  "AAABHBJS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABMGGD","AAABHBJW","AAABHBJX","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMGGD" varchar(20) NOT NULL default ' ',
  "AAABHBJW" varchar(1) NOT NULL default ' ',
  "AAABHBJX" varchar(17) NOT NULL default ' ',
  "AAABMREF" varchar(8) NOT NULL default ' ',
  "AAABMLZS" varchar(3) NOT NULL default '000',
  "AAABHBIT" varchar(1) NOT NULL default ' ',
  "AAABHBIU" varchar(2) NOT NULL default ' ',
  "AAABHBIV" varchar(100) NOT NULL default ' ',
  "AAABHBIW" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABMGGD","AAABHBJW","AAABHBJX","AAABMREF","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBHX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMGGD" varchar(20) NOT NULL default ' ',
  "AAABHBJW" varchar(1) NOT NULL default ' ',
  "AAABHBJX" varchar(17) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHBIE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABMGGD","AAABHBJW","AAABHBJX","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBIL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMGGD" varchar(20) NOT NULL default ' ',
  "AAABHBJW" varchar(1) NOT NULL default ' ',
  "AAABHBJX" varchar(17) NOT NULL default ' ',
  "AAABMREF" varchar(8) NOT NULL default ' ',
  "AAABMLZS" varchar(3) NOT NULL default '000',
  "AAABHBIT" varchar(1) NOT NULL default ' ',
  "AAABHBIU" varchar(2) NOT NULL default ' ',
  "AAABHBIV" varchar(100) NOT NULL default ' ',
  "AAABHBIW" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABMGGD","AAABHBJW","AAABHBJX","AAABMREF","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBJE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMGGD" varchar(20) NOT NULL default ' ',
  "AAABHBJW" varchar(1) NOT NULL default ' ',
  "AAABHBJX" varchar(17) NOT NULL default ' ',
  "AAABMREF" varchar(8) NOT NULL default ' ',
  "AAABLBEI" varchar(3) NOT NULL default '000',
  "AAABHBJM" varchar(3) NOT NULL default '000',
  "AAABMNMP" varchar(3) NOT NULL default '000',
  "AAABMMVY" varchar(3) NOT NULL default '000',
  "AAABHBJP" varchar(3) NOT NULL default '000',
  "AAABHBJQ" varchar(1) NOT NULL default ' ',
  "AAABHBJR" varchar(1) NOT NULL default ' ',
  "AAABHBJS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABMGGD","AAABHBJW","AAABHBJX","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBJZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHBLT" varchar(4) NOT NULL default ' ',
  "AAABHBKP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHBLT","AAABHBKP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHBLT" varchar(4) NOT NULL default ' ',
  "AAABHBKP" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHBLT","AAABHBKP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBKQ" (
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABHEWL" varchar(5) NOT NULL default '00000',
  "AAABLUCJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER","AAABHEWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBKW" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABHEWL" varchar(5) NOT NULL default '00000',
  "AAABLUDG" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLUER","AAABHEWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBLE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHBLT" varchar(4) NOT NULL default ' ',
  "AAABLXHF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHBLT","AAABLXHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBLL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHBLT" varchar(4) NOT NULL default ' ',
  "AAABLXHF" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHBLT","AAABLXHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBLV" (
  "AAABHBMD" varchar(1) NOT NULL default ' ',
  "AAABHBME" varchar(1) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABHBMG" varchar(40) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABHBMB" varchar(4) NOT NULL default '0000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABHBMD","AAABHBME","AAABMKFH","AAABHBMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBMH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBMM" (
  "AAABHBNW" varchar(1) NOT NULL default ' ',
  "AAABHBNX" varchar(5) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABHBMV" varchar(10) NOT NULL default ' ',
  "AAABHBMR" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABHBNW","AAABHBNX","AAABMREL","AAABHBMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBMW" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABHBOW" varchar(1) NOT NULL default ' ',
  "AAABKMWI" varchar(1) NOT NULL default ' ',
  "AAABHBNA" varchar(1) NOT NULL default ' ',
  "AAABHBPM" varchar(1) NOT NULL default ' ',
  "AAABHCEC" tinyint(3) unsigned NOT NULL default '0',
  "AAABHBND" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBNF" (
  "AAABHBNW" varchar(1) NOT NULL default ' ',
  "AAABHBNX" varchar(5) NOT NULL default ' ',
  "AAABJTGS" varchar(30) NOT NULL default ' ',
  "AAABHBOG" varchar(2) NOT NULL default '00',
  "AAABKCWA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHBNW","AAABHBNX","AAABJTGS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBNO" (
  "AAABHBNW" varchar(1) NOT NULL default ' ',
  "AAABHBNX" varchar(5) NOT NULL default ' ',
  "AAABHBOM" varchar(60) NOT NULL default ' ',
  "AAABHBOG" varchar(2) NOT NULL default '00',
  "AAABJHGJ" varchar(1) NOT NULL default ' ',
  "AAABJHGK" varchar(1) NOT NULL default ' ',
  "AAABIKRS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHBNW","AAABHBNX","AAABHBOM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBNZ" (
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABJZWV" varchar(5) NOT NULL default ' ',
  "AAABHBOM" varchar(60) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABJSDF" varchar(3) NOT NULL default '000',
  "AAABHBOF" varchar(60) NOT NULL default ' ',
  "AAABHBOG" varchar(2) NOT NULL default '00',
  "AAABKCWA" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLFZK","AAABJZWV","AAABHBOM","AAABIRUN","AAABJSDF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBOP" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABHBPG" varchar(1) NOT NULL default ' ',
  "AAABHBPH" varchar(1) NOT NULL default ' ',
  "AAABHBPI" varchar(1) NOT NULL default ' ',
  "AAABJDMJ" varchar(1) NOT NULL default ' ',
  "AAABHBPM" varchar(1) NOT NULL default ' ',
  "AAABHBOW" varchar(1) NOT NULL default ' ',
  "AAABHBOX" varchar(1) NOT NULL default ' ',
  "AAABHBOY" varchar(1) NOT NULL default ' ',
  "AAABHBOZ" varchar(1) NOT NULL default ' ',
  "AAABHBPA" varchar(1) NOT NULL default ' ',
  "AAABHBPB" varchar(1) NOT NULL default ' ',
  "AAABHBPC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBPE" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABHBPG" varchar(1) NOT NULL default ' ',
  "AAABHBPH" varchar(1) NOT NULL default ' ',
  "AAABHBPI" varchar(1) NOT NULL default ' ',
  "AAABHBPJ" varchar(1) NOT NULL default ' ',
  "AAABHBPK" varchar(4) NOT NULL default '0000',
  "AAABHBPL" varchar(1) NOT NULL default ' ',
  "AAABHBPM" varchar(1) NOT NULL default ' ',
  "AAABHBPN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBPP" (
  "AAABKGBL" varchar(2) NOT NULL default ' ',
  "AAABMMVO" varchar(1) NOT NULL default ' ',
  "AAABHBPS" varchar(1) NOT NULL default ' ',
  "AAABHBPT" int(11) NOT NULL default '0',
  "AAABLUUW" int(11) NOT NULL default '0',
  "AAABHBPV" varchar(2) NOT NULL default ' ',
  "AAABHBPW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABKGBL","AAABMMVO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBPZ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKGBL" varchar(2) NOT NULL default ' ',
  "AAABMMVO" varchar(1) NOT NULL default ' ',
  "AAABLUDG" varchar(78) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABKGBL","AAABMMVO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBQH" (
  "AAABHBQL" varchar(2) NOT NULL default ' ',
  "AAABHBQM" varchar(1) NOT NULL default ' ',
  "AAABHBQK" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABHBQL","AAABHBQM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBQN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHZBP" varchar(12) NOT NULL default ' ',
  "AAABHDVT" varchar(20) NOT NULL default ' ',
  "AAABHDTJ" varchar(1) NOT NULL default ' ',
  "AAABHDTK" varchar(8) NOT NULL default '00000000',
  "AAABHDTL" varchar(6) NOT NULL default '000000',
  "AAABHBQU" varchar(1) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABMKCP" varchar(20) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABHBQY" varchar(1) NOT NULL default ' ',
  "AAABHBQZ" varchar(1) NOT NULL default ' ',
  "AAABHBRA" varchar(1) NOT NULL default ' ',
  "AAABHBRB" varchar(1) NOT NULL default ' ',
  "AAABHBRC" varchar(1) NOT NULL default ' ',
  "AAABHBRD" varchar(1) NOT NULL default ' ',
  "AAABIKDN" varchar(2) NOT NULL default ' ',
  "AAABHBRF" varchar(8) NOT NULL default ' ',
  "AAABHBRG" varchar(12) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABHBRI" varchar(40) NOT NULL default ' ',
  "AAABHBRJ" varchar(1) NOT NULL default ' ',
  "AAABHBRK" varchar(1) NOT NULL default ' ',
  "AAABHBRL" varchar(1) NOT NULL default ' ',
  "AAABHCEC" varchar(9) NOT NULL default '000000000',
  "AAABHBRN" varchar(1) NOT NULL default ' ',
  "AAABHBRO" varchar(60) NOT NULL default ' ',
  "AAABHBRP" varchar(4) NOT NULL default ' ',
  "AAABHBRQ" varchar(1) NOT NULL default ' ',
  "AAABHBRR" varchar(1) NOT NULL default ' ',
  "AAABHBRS" varchar(1) NOT NULL default ' ',
  "AAABHBRT" varchar(1) NOT NULL default ' ',
  "AAABHBRU" varchar(1) NOT NULL default ' ',
  "AAABHBRV" varchar(1) NOT NULL default ' ',
  "AAABHBRW" varchar(1) NOT NULL default ' ',
  "AAABHBRX" varchar(1) NOT NULL default ' ',
  "AAABHBRY" varchar(1) NOT NULL default ' ',
  "AAABHBRZ" varchar(1) NOT NULL default ' ',
  "AAABHBSA" varchar(1) NOT NULL default ' ',
  "AAABHBSB" varchar(1) NOT NULL default ' ',
  "AAABHBSC" varchar(1) NOT NULL default ' ',
  "AAABHBSD" varchar(1) NOT NULL default ' ',
  "AAABHBSE" varchar(1) NOT NULL default ' ',
  "AAABHBSF" varchar(1) NOT NULL default ' ',
  "AAABHBSG" varchar(1) NOT NULL default ' ',
  "AAABHBSH" varchar(1) NOT NULL default ' ',
  "AAABHBSI" varchar(1) NOT NULL default ' ',
  "AAABHBSJ" varchar(1) NOT NULL default ' ',
  "AAABHBSK" varchar(1) NOT NULL default ' ',
  "AAABHBSL" varchar(1) NOT NULL default ' ',
  "AAABHBSM" varchar(1) NOT NULL default ' ',
  "AAABHBSN" varchar(1) NOT NULL default ' ',
  "AAABHBSO" varchar(1) NOT NULL default ' ',
  "AAABHBSP" varchar(1) NOT NULL default ' ',
  "AAABHBSQ" varchar(5) NOT NULL default '00000',
  "AAABHBSR" varchar(5) NOT NULL default '00000',
  "AAABHBSS" varchar(16) NOT NULL default ' ',
  "AAABHBST" varchar(1) NOT NULL default ' ',
  "AAABHBSU" int(11) NOT NULL default '0',
  "AAABHBSV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLUXQ","AAABHZBP","AAABHDVT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBSZ" (
  "AAABHBTD" varchar(2) NOT NULL default ' ',
  "AAABHBTE" varchar(1) NOT NULL default ' ',
  "AAABHBTC" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABHBTD","AAABHBTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBTF" (
  "AAABHMQN" varchar(10) NOT NULL default ' ',
  "AAABLUDG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABHMQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBTJ" (
  "AAABHMQN" varchar(10) NOT NULL default ' ',
  "AAABHBTO" varchar(6) NOT NULL default ' ',
  "AAABHBTU" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABHMQN","AAABHBTO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBTP" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHBTU" varchar(16) NOT NULL default ' ',
  "AAABHBTS" varchar(78) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABHBTU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBTV" (
  "AAABLITM" varchar(20) NOT NULL default ' ',
  "AAABHMQN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLITM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBTZ" (
  "AAABHBUL" varchar(1) NOT NULL default ' ',
  "AAABHBUM" varchar(1) NOT NULL default ' ',
  "AAABHBUC" decimal(15,2) NOT NULL default '0.00',
  "AAABHBUD" varchar(5) NOT NULL default ' ',
  "AAABHBUE" decimal(15,2) NOT NULL default '0.00',
  "AAABHBUF" varchar(5) NOT NULL default ' ',
  "AAABHBUG" tinyint(3) unsigned NOT NULL default '0',
  "AAABHBUH" tinyint(3) unsigned NOT NULL default '0',
  "AAABHBUI" tinyint(3) unsigned NOT NULL default '0',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABKUQR" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABHBUL","AAABHBUM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBUN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCOV" varchar(3) NOT NULL default ' ',
  "AAABHBUX" varchar(2) NOT NULL default ' ',
  "AAABHBUY" varchar(2) NOT NULL default ' ',
  "AAABHBUZ" varchar(12) NOT NULL default ' ',
  "AAABHBVA" varchar(12) NOT NULL default ' ',
  "AAABHBUU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCOV","AAABHBUX","AAABHBUY","AAABHBUZ","AAABHBVA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBVB" (
  "AAABHBVG" varchar(70) NOT NULL default ' ',
  "AAABHBVH" varchar(70) NOT NULL default ' ',
  "AAABHBVI" varchar(20) NOT NULL default ' ',
  "AAABIVNA" varchar(27) NOT NULL default ' ',
  PRIMARY KEY ("AAABHBVG","AAABHBVH","AAABHBVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBVJ" (
  "AAABJECS" varchar(3) NOT NULL default ' ',
  "AAABHBVL" varchar(2) NOT NULL default ' ',
  "AAABHBVM" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABJECS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBVO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHBVV" varchar(12) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHBWG" varchar(3) NOT NULL default ' ',
  "AAABLLLP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHBVV","AAABMLQA","AAABHBWG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBVY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHBWG" varchar(3) NOT NULL default ' ',
  "AAABKHXJ" varchar(1) NOT NULL default ' ',
  "AAABHBWC" varchar(1) NOT NULL default ' ',
  "AAABHBWD" varchar(1) NOT NULL default ' ',
  "AAABHBWE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHBWG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBWH" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJECS" varchar(3) NOT NULL default ' ',
  "AAABHBWK" varchar(36) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABJECS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBWN" (
  "AAABHBWQ" varchar(30) NOT NULL default ' ',
  "AAABHBWP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHBWQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBWR" (
  "AAABMMRH" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBWU" (
  "AAABLCOV" varchar(3) NOT NULL default ' ',
  "AAABHTLA" varchar(1) NOT NULL default ' ',
  "AAABHBWX" varchar(1) NOT NULL default ' ',
  "AAABHBWY" varchar(1) NOT NULL default ' ',
  "AAABHBWZ" varchar(1) NOT NULL default ' ',
  "AAABHBXA" varchar(1) NOT NULL default ' ',
  "AAABHBXB" varchar(1) NOT NULL default ' ',
  "AAABHBXC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLCOV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBXE" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLCOV" varchar(3) NOT NULL default ' ',
  "AAABHBXH" varchar(26) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLCOV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBXK" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHBXP" varchar(4) NOT NULL default '0000',
  "AAABHBXN" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHBXP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBXQ" (
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABHEWL" varchar(5) NOT NULL default '00000',
  "AAABHBXY" varchar(30) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABMLZS" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLUER","AAABHEWL","AAABHBXY","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBYA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBYF" (
  "AAABIFHV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIFHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBYI" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIFHV" varchar(1) NOT NULL default ' ',
  "AAABHBYL" varchar(26) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIFHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBYO" (
  "AAABKEON" int(11) NOT NULL default '0',
  "AAABHCAN" varchar(3) NOT NULL default '000',
  "AAABHCQL" varchar(6) NOT NULL default ' ',
  "AAABHCQM" varchar(4) NOT NULL default ' ',
  "AAABHCQN" varchar(12) NOT NULL default ' ',
  "AAABHCAO" varchar(12) NOT NULL default ' ',
  "AAABHBYV" varchar(1) NOT NULL default ' ',
  "AAABHCAM" varchar(1) NOT NULL default ' ',
  "AAABHBYX" varchar(16) NOT NULL default ' ',
  "AAABHBYY" varchar(16) NOT NULL default ' ',
  "AAABHBYZ" smallint(6) NOT NULL default '0',
  "AAABHBZA" smallint(6) NOT NULL default '0',
  "AAABHBZB" smallint(6) NOT NULL default '0',
  "AAABHBZC" smallint(6) NOT NULL default '0',
  "AAABHCQQ" varchar(1) NOT NULL default ' ',
  "AAABHCQR" varchar(1) NOT NULL default ' ',
  "AAABHCQS" varchar(1) NOT NULL default ' ',
  "AAABHBZG" varchar(4) NOT NULL default ' ',
  "AAABHBZH" tinyint(3) unsigned NOT NULL default '0',
  "AAABHBZI" tinyint(3) unsigned NOT NULL default '0',
  "AAABHBZJ" varchar(16) NOT NULL default ' ',
  "AAABHBZK" smallint(6) NOT NULL default '0',
  "AAABHBZL" smallint(6) NOT NULL default '0',
  "AAABHBZM" smallint(6) NOT NULL default '0',
  "AAABHBZN" smallint(6) NOT NULL default '0',
  "AAABHBZO" smallint(6) NOT NULL default '0',
  "AAABHBZP" varchar(1) NOT NULL default ' ',
  "AAABHBZQ" varchar(20) NOT NULL default ' ',
  "AAABHBZR" smallint(6) NOT NULL default '0',
  "AAABHBZS" varchar(3) NOT NULL default '000',
  "AAABHBZT" varchar(68) NOT NULL default ' ',
  "AAABHBZU" varchar(1) NOT NULL default ' ',
  "AAABHCAT" varchar(1) NOT NULL default ' ',
  "AAABHBZW" varchar(1) NOT NULL default ' ',
  "AAABHBZX" varchar(12) NOT NULL default ' ',
  "AAABHBZY" varchar(12) NOT NULL default ' ',
  "AAABHCQX" varchar(1) NOT NULL default ' ',
  "AAABHCQY" varchar(4) NOT NULL default ' ',
  "AAABHCQZ" varchar(1) NOT NULL default ' ',
  "AAABHCAC" varchar(12) NOT NULL default ' ',
  "AAABHCAD" varchar(1) NOT NULL default ' ',
  "AAABHCAE" varchar(3) NOT NULL default ' ',
  "AAABHCAF" varchar(30) NOT NULL default ' ',
  "AAABHCAG" varchar(30) NOT NULL default ' ',
  "AAABHCAH" varchar(16) NOT NULL default ' ',
  "AAABHCAI" varchar(6) NOT NULL default ' ',
  "AAABIFLX" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABKEON"),
  KEY "AAABHCAL" ("AAABHCAM","AAABHCAN","AAABHCAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCAP" (
  "AAABKEON" int(11) NOT NULL default '0',
  "AAABHCAW" int(11) NOT NULL default '0',
  "AAABHCAY" int(11) NOT NULL default '0',
  "AAABHCAT" varchar(1) NOT NULL default ' ',
  "AAABHCAU" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABKEON","AAABHCAW"),
  KEY "AAABHCAX" ("AAABHCAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCAZ" (
  "AAABHDEN" int(11) NOT NULL default '0',
  "AAABHCQJ" smallint(6) NOT NULL default '0',
  "AAABHDDG" varchar(4) NOT NULL default ' ',
  "AAABHDDH" varchar(16) NOT NULL default ' ',
  "AAABHCBE" varchar(16) NOT NULL default ' ',
  "AAABHDEP" varchar(16) NOT NULL default ' ',
  "AAABHCRH" varchar(1) NOT NULL default ' ',
  "AAABHCBH" smallint(6) NOT NULL default '0',
  "AAABHDDP" tinyint(3) unsigned NOT NULL default '0',
  "AAABHCBJ" int(11) NOT NULL default '0',
  "AAABHCBK" int(11) NOT NULL default '0',
  "AAABHDDV" varchar(12) NOT NULL default ' ',
  "AAABHCBM" smallint(6) NOT NULL default '0',
  "AAABHCBN" smallint(6) NOT NULL default '0',
  "AAABHCBO" smallint(6) NOT NULL default '0',
  "AAABHCBP" varchar(20) NOT NULL default ' ',
  "AAABHDDT" varchar(3) NOT NULL default '000',
  "AAABHCBR" varchar(16) NOT NULL default ' ',
  "AAABHCBS" varchar(16) NOT NULL default ' ',
  "AAABHCBT" varchar(16) NOT NULL default ' ',
  "AAABHCBU" varchar(50) NOT NULL default ' ',
  "AAABHCBV" int(11) NOT NULL default '0',
  "AAABHCCQ" int(11) NOT NULL default '0',
  "AAABHDDQ" tinyint(3) unsigned NOT NULL default '0',
  "AAABHCBY" varchar(1) NOT NULL default ' ',
  "AAABHDDL" varchar(68) NOT NULL default ' ',
  "AAABHDDW" varchar(12) NOT NULL default ' ',
  "AAABHDDX" varchar(12) NOT NULL default ' ',
  "AAABHDDY" varchar(30) NOT NULL default ' ',
  "AAABHCCD" varchar(32) NOT NULL default ' ',
  "AAABHDDZ" varchar(20) NOT NULL default ' ',
  "AAABHCCF" tinyint(3) unsigned NOT NULL default '0',
  "AAABHCCG" tinyint(3) unsigned NOT NULL default '0',
  "AAABHCCH" varchar(50) NOT NULL default ' ',
  "AAABHCFC" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABHDEN","AAABHCQJ"),
  KEY "AAABHCCL" ("AAABHCRH"),
  KEY "AAABHCCN" ("AAABHDDZ"),
  KEY "AAABHCCP" ("AAABHCCQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCCR" (
  "AAABHDEN" int(11) NOT NULL default '0',
  "AAABHDEO" smallint(6) NOT NULL default '0',
  "AAABMJWN" varchar(16) NOT NULL default ' ',
  "AAABMMRH" varchar(8) NOT NULL default '00000000',
  "AAABMMJT" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAABHDEN","AAABHDEO","AAABMJWN","AAABMMRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCDB" (
  "AAABHDEN" int(11) NOT NULL default '0',
  "AAABHDEO" smallint(6) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABILXJ" varchar(20) NOT NULL default ' ',
  "AAABHCDU" varchar(16) NOT NULL default ' ',
  "AAABHCDV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHDEN","AAABHDEO","AAABKNGX","AAABLUXQ","AAABILXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCDO" (
  "AAABHDEN" int(11) NOT NULL default '0',
  "AAABHDEO" smallint(6) NOT NULL default '0',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABILXJ" varchar(20) NOT NULL default ' ',
  "AAABHCDU" varchar(16) NOT NULL default ' ',
  "AAABHCDV" varchar(1) NOT NULL default ' ',
  "AAABHCDW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABHDEN","AAABHDEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCDZ" (
  "AAABHDEN" int(11) NOT NULL default '0',
  "AAABHCQJ" smallint(6) NOT NULL default '0',
  "AAABHCEC" int(11) NOT NULL default '0',
  "AAABHCED" int(11) NOT NULL default '0',
  "AAABHCKM" varchar(1) NOT NULL default ' ',
  "AAABHCKN" varchar(1) NOT NULL default ' ',
  "AAABHCKO" varchar(3) NOT NULL default ' ',
  "AAABHCEH" varchar(8) NOT NULL default ' ',
  "AAABHCEI" varchar(1) NOT NULL default ' ',
  "AAABHCEJ" varchar(1) NOT NULL default ' ',
  "AAABHCNU" varchar(1) NOT NULL default ' ',
  "AAABIIZY" varchar(4) NOT NULL default '0000',
  "AAABHCNV" int(11) NOT NULL default '0',
  "AAABHCEN" int(11) NOT NULL default '0',
  "AAABHCEO" int(11) NOT NULL default '0',
  "AAABHCEP" int(11) NOT NULL default '0',
  "AAABHCEQ" int(11) NOT NULL default '0',
  "AAABHCER" int(11) NOT NULL default '0',
  "AAABHCES" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABHDEN","AAABHCQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCEV" (
  "AAABHDEN" int(11) NOT NULL default '0',
  "AAABHCQJ" smallint(6) NOT NULL default '0',
  "AAABHCFC" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABHDEN","AAABHCQJ"),
  KEY "AAABHCFB" ("AAABHCFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCFD" (
  "AAABHDEN" int(11) NOT NULL default '0',
  "AAABHCQJ" smallint(6) NOT NULL default '0',
  "AAABHDDG" varchar(4) NOT NULL default ' ',
  "AAABHDDV" varchar(12) NOT NULL default ' ',
  "AAABHDDZ" varchar(20) NOT NULL default ' ',
  "AAABHCRH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHDEN","AAABHCQJ"),
  KEY "AAABHCFM" ("AAABHDDZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCFO" (
  "AAABHCZO" varchar(4) NOT NULL default ' ',
  "AAABHCFQ" varchar(8) NOT NULL default ' ',
  "AAABHCXM" varchar(8) NOT NULL default ' ',
  "AAABHCFS" varchar(32) NOT NULL default ' ',
  "AAABHCJC" varchar(1) NOT NULL default ' ',
  "AAABHCFU" smallint(6) NOT NULL default '0',
  "AAABHCFV" int(11) NOT NULL default '0',
  "AAABHCJA" varchar(64) NOT NULL default ' ',
  "AAABHCIZ" varchar(64) NOT NULL default ' ',
  "AAABHCJG" varchar(32) NOT NULL default ' ',
  "AAABHCFZ" varchar(1) NOT NULL default ' ',
  "AAABHCGA" varchar(1) NOT NULL default ' ',
  "AAABHCJF" varchar(1) NOT NULL default ' ',
  "AAABHCGZ" varchar(1) NOT NULL default ' ',
  "AAABHCHA" varchar(1) NOT NULL default ' ',
  "AAABHCHB" tinyint(3) unsigned NOT NULL default '0',
  "AAABHCIV" varchar(1) NOT NULL default ' ',
  "AAABHCIW" varchar(1) NOT NULL default ' ',
  "AAABHCHE" varchar(1) NOT NULL default ' ',
  "AAABHCHF" int(11) NOT NULL default '0',
  "AAABHCJH" smallint(6) NOT NULL default '0',
  "AAABHCIO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHCZO"),
  KEY "AAABHCGM" ("AAABHCXM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCGO" (
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABMJWN" varchar(16) NOT NULL default ' ',
  "AAABMMRH" varchar(8) NOT NULL default '00000000',
  "AAABMMJT" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV","AAABMJWN","AAABMMRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCGW" (
  "AAABHCZO" varchar(4) NOT NULL default ' ',
  "AAABHCJF" varchar(1) NOT NULL default ' ',
  "AAABHCGZ" varchar(1) NOT NULL default ' ',
  "AAABHCHA" varchar(1) NOT NULL default ' ',
  "AAABHCHB" tinyint(3) unsigned NOT NULL default '0',
  "AAABHCIV" varchar(1) NOT NULL default ' ',
  "AAABHCIW" varchar(1) NOT NULL default ' ',
  "AAABHCHE" varchar(1) NOT NULL default ' ',
  "AAABHCHF" int(11) NOT NULL default '0',
  "AAABHCJH" smallint(6) NOT NULL default '0',
  "AAABHCIO" varchar(1) NOT NULL default ' ',
  "AAABHCHI" smallint(6) NOT NULL default '0',
  "AAABHCHJ" smallint(6) NOT NULL default '0',
  "AAABHCJB" varchar(20) NOT NULL default ' ',
  "AAABHCJD" varchar(50) NOT NULL default ' ',
  "AAABHCJI" smallint(6) NOT NULL default '0',
  "AAABHCJJ" smallint(6) NOT NULL default '0',
  "AAABHCHO" varchar(32) NOT NULL default ' ',
  "AAABHCHP" smallint(6) NOT NULL default '0',
  "AAABHCIS" varchar(1) NOT NULL default ' ',
  "AAABHCHR" varchar(1) NOT NULL default ' ',
  "AAABHCJK" smallint(6) NOT NULL default '0',
  "AAABHCIQ" varchar(1) NOT NULL default ' ',
  "AAABHCJR" varchar(1) NOT NULL default ' ',
  "AAABHCJP" varchar(1) NOT NULL default ' ',
  "AAABHCJQ" varchar(1) NOT NULL default ' ',
  "AAABHCJL" int(11) NOT NULL default '0',
  "AAABHCJM" int(11) NOT NULL default '0',
  "AAABHCJN" varchar(2) NOT NULL default ' ',
  "AAABHCJO" varchar(2) NOT NULL default ' ',
  "AAABHCIP" varchar(1) NOT NULL default ' ',
  "AAABHCKL" varchar(30) NOT NULL default ' ',
  "AAABHCKM" varchar(1) NOT NULL default ' ',
  "AAABHCKN" varchar(1) NOT NULL default ' ',
  "AAABHCKO" varchar(3) NOT NULL default ' ',
  "AAABHCIN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHCZO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCII" (
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABHCZO" varchar(4) NOT NULL default ' ',
  "AAABHCXM" varchar(8) NOT NULL default ' ',
  "AAABHCIM" varchar(20) NOT NULL default ' ',
  "AAABHCIN" varchar(1) NOT NULL default ' ',
  "AAABHCIO" varchar(1) NOT NULL default ' ',
  "AAABHCIP" varchar(1) NOT NULL default ' ',
  "AAABHCIQ" varchar(1) NOT NULL default ' ',
  "AAABHCIR" varchar(1) NOT NULL default ' ',
  "AAABHCIS" varchar(1) NOT NULL default ' ',
  "AAABHCIT" varchar(1) NOT NULL default ' ',
  "AAABHCIU" varchar(1) NOT NULL default ' ',
  "AAABHCIV" varchar(1) NOT NULL default ' ',
  "AAABHCIW" varchar(1) NOT NULL default ' ',
  "AAABHCIX" varchar(1) NOT NULL default ' ',
  "AAABHCIY" varchar(1) NOT NULL default ' ',
  "AAABHCIZ" varchar(64) NOT NULL default ' ',
  "AAABHCJA" varchar(64) NOT NULL default ' ',
  "AAABHCJB" varchar(20) NOT NULL default ' ',
  "AAABHCJC" varchar(1) NOT NULL default ' ',
  "AAABHCJD" varchar(50) NOT NULL default ' ',
  "AAABHCJE" varchar(6) NOT NULL default ' ',
  "AAABHCJF" varchar(1) NOT NULL default ' ',
  "AAABHCJG" varchar(32) NOT NULL default ' ',
  "AAABHCJH" smallint(6) NOT NULL default '0',
  "AAABHCJI" smallint(6) NOT NULL default '0',
  "AAABHCJJ" smallint(6) NOT NULL default '0',
  "AAABHCJK" smallint(6) NOT NULL default '0',
  "AAABHCJL" int(11) NOT NULL default '0',
  "AAABHCJM" int(11) NOT NULL default '0',
  "AAABHCJN" varchar(2) NOT NULL default ' ',
  "AAABHCJO" varchar(2) NOT NULL default ' ',
  "AAABHCJP" varchar(1) NOT NULL default ' ',
  "AAABHCJQ" varchar(1) NOT NULL default ' ',
  "AAABHCJR" varchar(1) NOT NULL default ' ',
  "AAABHCJS" varchar(1) NOT NULL default ' ',
  "AAABHCJT" varchar(1) NOT NULL default ' ',
  "AAABHCJU" varchar(1) NOT NULL default ' ',
  "AAABHCJV" varchar(1) NOT NULL default ' ',
  "AAABHCJW" varchar(1) NOT NULL default ' ',
  "AAABHCJX" varchar(1) NOT NULL default ' ',
  "AAABHCJY" varchar(1) NOT NULL default ' ',
  "AAABHDJU" varchar(12) NOT NULL default ' ',
  "AAABHDJV" varchar(14) NOT NULL default ' ',
  "AAABHDJW" varchar(4) NOT NULL default ' ',
  "AAABHDJX" varchar(8) NOT NULL default ' ',
  "AAABHDJY" varchar(12) NOT NULL default ' ',
  "AAABHDJZ" varchar(14) NOT NULL default ' ',
  "AAABHDKA" varchar(4) NOT NULL default ' ',
  "AAABHDKB" varchar(8) NOT NULL default ' ',
  "AAABHDKC" varchar(12) NOT NULL default ' ',
  "AAABHDKD" varchar(14) NOT NULL default ' ',
  "AAABHDKE" varchar(4) NOT NULL default ' ',
  "AAABHDKF" varchar(8) NOT NULL default ' ',
  "AAABHCKL" varchar(30) NOT NULL default ' ',
  "AAABHCKM" varchar(1) NOT NULL default ' ',
  "AAABHCKN" varchar(1) NOT NULL default ' ',
  "AAABHCKO" varchar(3) NOT NULL default ' ',
  "AAABHCKP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCKR" (
  "AAABKZAY" varchar(30) NOT NULL default ' ',
  "AAABHCZO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABKZAY"),
  KEY "AAABHCKV" ("AAABHCZO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCKX" (
  "AAABHCZO" varchar(4) NOT NULL default ' ',
  "AAABHCTJ" varchar(23) NOT NULL default ' ',
  "AAABHCTK" varchar(8) NOT NULL default ' ',
  "AAABHCTL" varchar(8) NOT NULL default ' ',
  "AAABHCTM" varchar(8) NOT NULL default ' ',
  "AAABHCTN" varchar(8) NOT NULL default ' ',
  "AAABHCTO" varchar(8) NOT NULL default ' ',
  "AAABHCTP" varchar(8) NOT NULL default ' ',
  "AAABHCTQ" varchar(8) NOT NULL default ' ',
  "AAABHCTR" varchar(8) NOT NULL default ' ',
  "AAABHCTS" varchar(8) NOT NULL default ' ',
  "AAABHCTT" varchar(8) NOT NULL default ' ',
  "AAABHCTU" varchar(8) NOT NULL default ' ',
  "AAABHCTV" varchar(8) NOT NULL default ' ',
  "AAABHCTW" varchar(8) NOT NULL default ' ',
  "AAABHCTX" varchar(8) NOT NULL default ' ',
  "AAABHCTY" varchar(8) NOT NULL default ' ',
  "AAABHCTZ" varchar(8) NOT NULL default ' ',
  "AAABHCUA" varchar(8) NOT NULL default ' ',
  "AAABHCUB" varchar(8) NOT NULL default ' ',
  "AAABHCUC" varchar(8) NOT NULL default ' ',
  "AAABHCUD" varchar(8) NOT NULL default ' ',
  "AAABHCUE" varchar(8) NOT NULL default ' ',
  "AAABHCUF" varchar(8) NOT NULL default ' ',
  "AAABHCUG" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABHCZO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCLY" (
  "AAABHCMD" varchar(4) NOT NULL default ' ',
  "AAABHCME" varchar(16) NOT NULL default ' ',
  "AAABHCMB" int(11) NOT NULL default '0',
  "AAABHCMC" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABHCMD","AAABHCME")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCMF" (
  "AAABHDSA" int(11) NOT NULL default '0',
  "AAABHDRZ" varchar(128) NOT NULL default ' ',
  "AAABHCMI" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAABHDSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCMK" (
  "AAABHCTE" varchar(8) NOT NULL default ' ',
  "AAABHCSQ" varchar(16) NOT NULL default ' ',
  "AAABHCMT" varchar(8) NOT NULL default ' ',
  "AAABHCMU" tinyint(3) unsigned NOT NULL default '0',
  "AAABHCMP" tinyint(3) unsigned NOT NULL default '0',
  "AAABHCMQ" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABHCTE","AAABHCSQ","AAABHCMT","AAABHCMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCMV" (
  "AAABMAYY" varchar(8) NOT NULL default ' ',
  "AAABHCQW" varchar(16) NOT NULL default ' ',
  "AAABKIUC" varchar(16) NOT NULL default ' ',
  "AAABLTXI" varchar(5) NOT NULL default '00000',
  "AAABHCNA" varchar(1) NOT NULL default ' ',
  "AAABHCNB" varchar(4) NOT NULL default '0000',
  "AAABHCNC" varchar(1) NOT NULL default ' ',
  "AAABHCND" varchar(4) NOT NULL default ' ',
  "AAABJAAI" varchar(32) NOT NULL default ' ',
  "AAABHDJU" varchar(12) NOT NULL default ' ',
  "AAABHDJV" varchar(14) NOT NULL default ' ',
  "AAABHDJW" varchar(4) NOT NULL default ' ',
  "AAABHDJX" varchar(8) NOT NULL default ' ',
  "AAABHDJY" varchar(12) NOT NULL default ' ',
  "AAABHDJZ" varchar(14) NOT NULL default ' ',
  "AAABHDKA" varchar(4) NOT NULL default ' ',
  "AAABHDKB" varchar(8) NOT NULL default ' ',
  "AAABHDKC" varchar(12) NOT NULL default ' ',
  "AAABHDKD" varchar(14) NOT NULL default ' ',
  "AAABHDKE" varchar(4) NOT NULL default ' ',
  "AAABHDKF" varchar(8) NOT NULL default ' ',
  "AAABHCNR" varchar(4) NOT NULL default ' ',
  "AAABHCNS" varchar(4) NOT NULL default ' ',
  "AAABHCNT" varchar(255) NOT NULL default ' ',
  "AAABHCNU" varchar(1) NOT NULL default ' ',
  "AAABHCNV" int(11) NOT NULL default '0',
  "AAABIIZY" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMAYY","AAABHCQW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCNZ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(7) NOT NULL default ' ',
  "AAABJSDF" int(11) NOT NULL default '0',
  "AAABLUDG" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCOG" (
  "AAABHCXM" varchar(8) NOT NULL default ' ',
  "AAABHCOI" longtext,
  "AAABHCOJ" longtext,
  "AAABHCOK" varchar(51) NOT NULL default ' ',
  "AAABHCOL" longblob,
  PRIMARY KEY ("AAABHCXM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCON" (
  "AAABMJCR" varchar(4) NOT NULL default ' ',
  "AAABHCOZ" varchar(8) NOT NULL default '00000000',
  "AAABMMCU" varchar(10) NOT NULL default ' ',
  "AAABMINV" varchar(40) NOT NULL default ' ',
  "AAABMNJF" varchar(3) NOT NULL default ' ',
  "AAABMRFP" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  "AAABHCOW" varchar(8) NOT NULL default '00000000',
  "AAABLJJY" varchar(240) NOT NULL default ' ',
  PRIMARY KEY ("AAABMJCR","AAABHCOZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCPA" (
  "AAABKEON" varchar(10) NOT NULL default '0000000000',
  "AAABHCPG" varchar(1) NOT NULL default ' ',
  "AAABHCPD" varchar(254) NOT NULL default ' ',
  "AAABHCPE" varchar(198) NOT NULL default ' ',
  PRIMARY KEY ("AAABKEON","AAABHCPG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCPH" (
  "AAABHCPL" varchar(4) NOT NULL default ' ',
  "AAABHCPJ" varchar(1) NOT NULL default ' ',
  "AAABHCPK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABHCPL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCPM" (
  "AAABILXJ" varchar(20) NOT NULL default ' ',
  "AAABHCPO" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABILXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCPQ" (
  "AAABHCPX" varchar(1) NOT NULL default ' ',
  "AAABHCPY" varchar(1) NOT NULL default ' ',
  "AAABHCPT" varchar(60) NOT NULL default ' ',
  "AAABHCPU" varchar(60) NOT NULL default ' ',
  "AAABHCPV" varchar(60) NOT NULL default ' ',
  "AAABHCPW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABHCPX","AAABHCPY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCPZ" (
  "AAABMQIX" varchar(16) NOT NULL default ' ',
  "AAABMRCU" varchar(2) NOT NULL default '00',
  "AAABIOET" varchar(20) NOT NULL default ' ',
  "AAABHCRQ" varchar(2) NOT NULL default ' ',
  "AAABLUFA" varchar(4) NOT NULL default ' ',
  "AAABHCQF" varchar(8) NOT NULL default ' ',
  "AAABHCQG" varchar(1) NOT NULL default ' ',
  "AAABHCQH" varchar(1) NOT NULL default '0',
  "AAABKEON" varchar(10) NOT NULL default '0000000000',
  "AAABHCQJ" varchar(5) NOT NULL default '00000',
  "AAABHCQK" varchar(4) NOT NULL default ' ',
  "AAABHCQL" varchar(6) NOT NULL default ' ',
  "AAABHCQM" varchar(4) NOT NULL default ' ',
  "AAABHCQN" varchar(12) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAABHCQQ" varchar(1) NOT NULL default ' ',
  "AAABHCQR" varchar(1) NOT NULL default ' ',
  "AAABHCQS" varchar(1) NOT NULL default ' ',
  "AAABHCQT" varchar(1) NOT NULL default ' ',
  "AAABHCQU" varchar(3) NOT NULL default '000',
  "AAABMALF" varchar(2) NOT NULL default '00',
  "AAABHCQW" varchar(16) NOT NULL default ' ',
  "AAABHCQX" varchar(1) NOT NULL default ' ',
  "AAABHCQY" varchar(4) NOT NULL default ' ',
  "AAABHCQZ" varchar(1) NOT NULL default ' ',
  "AAABHCRA" varchar(3) NOT NULL default ' ',
  "AAABHCRB" varchar(30) NOT NULL default ' ',
  "AAABHCRC" varchar(10) NOT NULL default '0000000000',
  "AAABHCRD" varchar(6) NOT NULL default '000000',
  "AAABHCRE" varchar(12) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABJDRC" varchar(40) NOT NULL default ' ',
  "AAABHCRH" varchar(1) NOT NULL default ' ',
  "AAABHCRI" varchar(10) NOT NULL default '0000000000',
  "AAABHCRJ" varchar(10) NOT NULL default '0000000000',
  "AAABMQXS" varchar(8) NOT NULL default ' ',
  "AAABMCMG" varchar(5) NOT NULL default '00000',
  "AAABHCRM" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQIX","AAABMRCU","AAABIOET","AAABHCRQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCRR" (
  "AAABHCVP" varchar(3) NOT NULL default '000',
  "AAABHCVY" varchar(30) NOT NULL default ' ',
  "AAABHCTE" varchar(8) NOT NULL default ' ',
  "AAABHCRV" varchar(12) NOT NULL default ' ',
  "AAABHCVQ" varchar(8) NOT NULL default '00000000',
  "AAABHCVR" varchar(8) NOT NULL default '00000000',
  "AAABHDSI" varchar(3) NOT NULL default '000',
  "AAABLUHV" varchar(20) NOT NULL default ' ',
  "AAABHDSK" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABHCVP","AAABHCVY","AAABHCTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCSE" (
  "AAABHCSQ" varchar(16) NOT NULL default ' ',
  "AAABHCSG" varchar(20) NOT NULL default ' ',
  "AAABHCSH" varchar(20) NOT NULL default ' ',
  "AAABHCSI" varchar(20) NOT NULL default ' ',
  "AAABHCSJ" varchar(20) NOT NULL default ' ',
  "AAABHCSK" varchar(20) NOT NULL default ' ',
  "AAABHCSL" varchar(20) NOT NULL default ' ',
  "AAABHCSM" varchar(20) NOT NULL default ' ',
  "AAABHCSN" varchar(20) NOT NULL default ' ',
  "AAABHCSO" varchar(20) NOT NULL default ' ',
  "AAABHCSP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABHCSQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCSR" (
  "AAABHCTE" varchar(8) NOT NULL default ' ',
  "AAABHCTF" varchar(20) NOT NULL default ' ',
  "AAABMRCU" varchar(1) NOT NULL default ' ',
  "AAABHCSV" varchar(20) NOT NULL default ' ',
  "AAABHCSW" varchar(5) NOT NULL default ' ',
  "AAABHCSX" varchar(8) NOT NULL default ' ',
  "AAABHCSY" varchar(5) NOT NULL default ' ',
  "AAABHCSZ" varchar(8) NOT NULL default ' ',
  "AAABHCTA" varchar(5) NOT NULL default ' ',
  "AAABHCTB" varchar(8) NOT NULL default ' ',
  "AAABHCTC" varchar(5) NOT NULL default ' ',
  "AAABHCTD" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABHCTE","AAABHCTF","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCTH" (
  "AAABHCZO" varchar(4) NOT NULL default ' ',
  "AAABHCTJ" varchar(23) NOT NULL default ' ',
  "AAABHCTK" varchar(20) NOT NULL default ' ',
  "AAABHCTL" varchar(20) NOT NULL default ' ',
  "AAABHCTM" varchar(20) NOT NULL default ' ',
  "AAABHCTN" varchar(20) NOT NULL default ' ',
  "AAABHCTO" varchar(20) NOT NULL default ' ',
  "AAABHCTP" varchar(20) NOT NULL default ' ',
  "AAABHCTQ" varchar(20) NOT NULL default ' ',
  "AAABHCTR" varchar(20) NOT NULL default ' ',
  "AAABHCTS" varchar(20) NOT NULL default ' ',
  "AAABHCTT" varchar(20) NOT NULL default ' ',
  "AAABHCTU" varchar(20) NOT NULL default ' ',
  "AAABHCTV" varchar(20) NOT NULL default ' ',
  "AAABHCTW" varchar(20) NOT NULL default ' ',
  "AAABHCTX" varchar(20) NOT NULL default ' ',
  "AAABHCTY" varchar(20) NOT NULL default ' ',
  "AAABHCTZ" varchar(20) NOT NULL default ' ',
  "AAABHCUA" varchar(20) NOT NULL default ' ',
  "AAABHCUB" varchar(20) NOT NULL default ' ',
  "AAABHCUC" varchar(20) NOT NULL default ' ',
  "AAABHCUD" varchar(20) NOT NULL default ' ',
  "AAABHCUE" varchar(20) NOT NULL default ' ',
  "AAABHCUF" varchar(20) NOT NULL default ' ',
  "AAABHCUG" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABHCZO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCUI" (
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABJARY" varchar(12) NOT NULL default ' ',
  "AAABLSDL" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNJW","AAABJARY","AAABLSDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCUQ" (
  "AAABILXJ" varchar(36) NOT NULL default ' ',
  "AAABLFZT" varchar(8) NOT NULL default ' ',
  "AAABHCUT" varchar(80) NOT NULL default ' ',
  "AAABHCUU" varchar(80) NOT NULL default ' ',
  "AAABHCUV" varchar(80) NOT NULL default ' ',
  "AAABHCUW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABILXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCUY" (
  "AAABHCZO" varchar(4) NOT NULL default ' ',
  "AAABHCVD" varchar(1) NOT NULL default ' ',
  "AAABHCVB" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABHCZO","AAABHCVD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCVE" (
  "AAABHCVM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHCVM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCVH" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHCVM" varchar(1) NOT NULL default ' ',
  "AAABLJJY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABHCVM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCVN" (
  "AAABHCVY" varchar(30) NOT NULL default ' ',
  "AAABHCVP" varchar(3) NOT NULL default '000',
  "AAABHCVQ" varchar(8) NOT NULL default '00000000',
  "AAABHCVR" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABHCVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCVT" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHCVY" varchar(30) NOT NULL default ' ',
  "AAABLUDG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABHCVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCVZ" (
  "AAABHCWH" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABHCWH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCWC" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHCWH" varchar(20) NOT NULL default ' ',
  "AAABLUDG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABHCWH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCWI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMJXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCWN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMJXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCWV" (
  "AAABKZAY" varchar(6) NOT NULL default ' ',
  "AAABHMQN" varchar(10) NOT NULL default ' ',
  "AAABHCXC" varchar(6) NOT NULL default ' ',
  "AAABHCWZ" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABKZAY","AAABHMQN","AAABHCXC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCXD" (
  "AAABIYQF" varchar(6) NOT NULL default ' ',
  "AAABMNJW" varchar(4) NOT NULL default ' ',
  "AAABHCXM" varchar(8) NOT NULL default ' ',
  "AAABHCXH" varchar(1) NOT NULL default ' ',
  "AAABHCXI" varchar(60) NOT NULL default ' ',
  "AAABHCXJ" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABIYQF","AAABMNJW","AAABHCXM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCXN" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABHCYR" varchar(30) NOT NULL default ' ',
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  "AAABIRTO" varchar(1) NOT NULL default ' ',
  "AAABHCYD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABMREF","AAABHCYR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCXX" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABHCYR" varchar(30) NOT NULL default ' ',
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  "AAABIRTO" varchar(1) NOT NULL default ' ',
  "AAABHCYD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABMREF","AAABHCYR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCYH" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABHCYR" varchar(30) NOT NULL default ' ',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  "AAABLJJO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABMREF","AAABHCYR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCYS" (
  "AAABHDIP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABHDIP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCYV" (
  "AAABHCZO" varchar(4) NOT NULL default ' ',
  "AAABMKFU" varchar(14) NOT NULL default ' ',
  "AAABHDAS" varchar(3) NOT NULL default '000',
  "AAABHCZW" varchar(14) NOT NULL default '00000000000000',
  "AAABHCZA" varchar(30) NOT NULL default ' ',
  "AAABHCZY" varchar(2) NOT NULL default '00',
  "AAABIKMD" varchar(2) NOT NULL default '00',
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
  PRIMARY KEY ("AAABHCZO","AAABMKFU","AAABHDAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHCZR" (
  "AAABHDEN" varchar(10) NOT NULL default '0000000000',
  "AAABHDEO" varchar(5) NOT NULL default '00000',
  "AAABMKFU" varchar(14) NOT NULL default ' ',
  "AAABHDAS" varchar(3) NOT NULL default '000',
  "AAABHCZW" varchar(14) NOT NULL default '00000000000000',
  "AAABHCZX" varchar(2) NOT NULL default '00',
  "AAABHCZY" varchar(2) NOT NULL default '00',
  "AAABIKMD" varchar(2) NOT NULL default '00',
  "AAABHDAA" varchar(2) NOT NULL default '00',
  "AAABHDAB" varchar(4) NOT NULL default '0000',
  "AAABHDAC" varchar(6) NOT NULL default '000000',
  "AAABHDAD" varchar(4) NOT NULL default '0000',
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
  PRIMARY KEY ("AAABHDEN","AAABHDEO","AAABMKFU","AAABHDAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDAT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(10) NOT NULL default ' ',
  "AAABHDAW" varchar(2) NOT NULL default '00',
  "AAABHDAX" varchar(10) NOT NULL default ' ',
  "AAABHDAY" int(11) NOT NULL default '0',
  "AAABHDAZ" decimal(3,0) NOT NULL default '0',
  "AAABHDBA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDBD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLPV","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDBL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(10) NOT NULL default ' ',
  "AAABHDBO" varchar(1) NOT NULL default ' ',
  "AAABIGAT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDBS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(10) NOT NULL default ' ',
  "AAABHDBV" varchar(2) NOT NULL default ' ',
  "AAABHDBW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDBZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHDCP" varchar(10) NOT NULL default ' ',
  "AAABHDCQ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHDCP","AAABHDCQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDCG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHDCO" varchar(8) NOT NULL default '00000000',
  "AAABHDCP" varchar(10) NOT NULL default ' ',
  "AAABHDCQ" varchar(10) NOT NULL default ' ',
  "AAABMMJT" varchar(20) NOT NULL default ' ',
  "AAABHDCM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHDCO","AAABHDCP","AAABHDCQ","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDCS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABHDCV" decimal(3,0) NOT NULL default '0',
  "AAABHDCW" decimal(3,0) NOT NULL default '0',
  "AAABHDCX" varchar(1) NOT NULL default ' ',
  "AAABKRBX" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDDB" (
  "AAABHDEN" int(11) NOT NULL default '0',
  "AAABHDEO" smallint(6) NOT NULL default '0',
  "AAABHDEP" varchar(16) NOT NULL default ' ',
  "AAABLRTW" varchar(16) NOT NULL default ' ',
  "AAABHDDG" varchar(4) NOT NULL default ' ',
  "AAABHDDH" varchar(16) NOT NULL default ' ',
  "AAABJDNK" varchar(8) NOT NULL default ' ',
  "AAABHDDJ" varchar(64) NOT NULL default ' ',
  "AAABHDDK" varchar(50) NOT NULL default ' ',
  "AAABHDDL" varchar(68) NOT NULL default ' ',
  "AAABHDDM" varchar(6) NOT NULL default ' ',
  "AAABHDDN" varchar(4) NOT NULL default ' ',
  "AAABHDDO" varchar(12) NOT NULL default ' ',
  "AAABHDDP" tinyint(3) unsigned NOT NULL default '0',
  "AAABHDDQ" tinyint(3) unsigned NOT NULL default '0',
  "AAABHDDR" int(11) NOT NULL default '0',
  "AAABHDDS" int(11) NOT NULL default '0',
  "AAABHDDT" varchar(3) NOT NULL default '000',
  "AAABHDDU" tinyint(3) unsigned NOT NULL default '0',
  "AAABHDDV" varchar(12) NOT NULL default ' ',
  "AAABHDDW" varchar(12) NOT NULL default ' ',
  "AAABHDDX" varchar(12) NOT NULL default ' ',
  "AAABHDDY" varchar(30) NOT NULL default ' ',
  "AAABHDDZ" varchar(20) NOT NULL default ' ',
  "AAABHDEA" varchar(6) NOT NULL default ' ',
  "AAABHDEB" VARBINARY(255),
  "AAABLUTC" VARBINARY(255),
  "AAABLUSZ" VARBINARY(255),
  "AAABHDEE" VARBINARY(255),
  "AAABHDEF" VARBINARY(255),
  "AAABHDEG" VARBINARY(255),
  "AAABHDEH" VARBINARY(255),
  "AAABLEQO" varchar(16) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABHDHK" varchar(1) NOT NULL default ' ',
  "AAABHDEL" varchar(1) NOT NULL default ' ',
  "AAABHDEM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHDEN","AAABHDEO","AAABHDEP"),
  KEY "AAABHDEQ" ("AAABHDHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDES" (
  "AAABMLPV" varchar(6) NOT NULL default ' ',
  "AAABHDEU" varchar(10) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  "AAABHDEW" varchar(40) NOT NULL default ' ',
  "AAABHDEX" varchar(40) NOT NULL default ' ',
  "AAABHDEY" varchar(3) NOT NULL default '000',
  "AAABHDEZ" varchar(3) NOT NULL default '000',
  "AAABHDFA" varchar(3) NOT NULL default '000',
  "AAABHDFB" varchar(3) NOT NULL default '000',
  "AAABHDFC" varchar(3) NOT NULL default '000',
  "AAABHDIW" varchar(60) NOT NULL default ' ',
  "AAABHDIX" varchar(60) NOT NULL default ' ',
  "AAABHDIY" varchar(1) NOT NULL default ' ',
  "AAABHDJU" varchar(12) NOT NULL default ' ',
  "AAABHDJV" varchar(14) NOT NULL default ' ',
  "AAABHDJW" varchar(4) NOT NULL default ' ',
  "AAABHDJX" varchar(8) NOT NULL default ' ',
  "AAABHDJY" varchar(12) NOT NULL default ' ',
  "AAABHDJZ" varchar(14) NOT NULL default ' ',
  "AAABHDKA" varchar(4) NOT NULL default ' ',
  "AAABHDKB" varchar(8) NOT NULL default ' ',
  "AAABHDKC" varchar(12) NOT NULL default ' ',
  "AAABHDKD" varchar(14) NOT NULL default ' ',
  "AAABHDKE" varchar(4) NOT NULL default ' ',
  "AAABHDKF" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDFT" (
  "AAABHDFW" varchar(16) NOT NULL default ' ',
  "AAABMMJT" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAABHDFW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDFX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(10) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABHDGB" decimal(3,0) NOT NULL default '0',
  "AAABHDGC" varchar(1) NOT NULL default ' ',
  "AAABHDGD" varchar(1) NOT NULL default ' ',
  "AAABHDGE" decimal(3,0) NOT NULL default '0',
  "AAABHDGF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLPV","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDGJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(10) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLPV","AAABMCVI","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDGT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(10) NOT NULL default ' ',
  "AAABMDXM" varchar(40) NOT NULL default ' ',
  "AAABHDGX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDHA" (
  "AAABHDHH" varchar(4) NOT NULL default ' ',
  "AAABLRTW" varchar(16) NOT NULL default ' ',
  "AAABHDHD" smallint(6) NOT NULL default '0',
  "AAABHDHE" smallint(6) NOT NULL default '0',
  "AAABLEQO" varchar(16) NOT NULL default ' ',
  "AAABHDHK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHDHH","AAABLRTW"),
  KEY "AAABHDHJ" ("AAABHDHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDHL" (
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABHEWL" varchar(5) NOT NULL default '00000',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  "AAABLUEQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER","AAABHEWL","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDHV" (
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABHEWL" varchar(5) NOT NULL default '00000',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  "AAABLUEQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER","AAABHEWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDID" (
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABHEWL" varchar(5) NOT NULL default '00000',
  "AAABLUEL" varchar(30) NOT NULL default ' ',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  "AAABLUEQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER","AAABHEWL","AAABLUEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDIN" (
  "AAABHDIP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABHDIP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDIQ" (
  "AAABMLPV" varchar(10) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  "AAABHDIT" varchar(40) NOT NULL default ' ',
  "AAABHDIU" varchar(132) NOT NULL default ' ',
  "AAABHDIV" varchar(3) NOT NULL default '000',
  "AAABHDIW" varchar(60) NOT NULL default ' ',
  "AAABHDIX" varchar(60) NOT NULL default ' ',
  "AAABHDIY" varchar(1) NOT NULL default ' ',
  "AAABHDJU" varchar(12) NOT NULL default ' ',
  "AAABHDJV" varchar(14) NOT NULL default ' ',
  "AAABHDJW" varchar(4) NOT NULL default ' ',
  "AAABHDJX" varchar(8) NOT NULL default ' ',
  "AAABHDJY" varchar(12) NOT NULL default ' ',
  "AAABHDJZ" varchar(14) NOT NULL default ' ',
  "AAABHDKA" varchar(4) NOT NULL default ' ',
  "AAABHDKB" varchar(8) NOT NULL default ' ',
  "AAABHDKC" varchar(12) NOT NULL default ' ',
  "AAABHDKD" varchar(14) NOT NULL default ' ',
  "AAABHDKE" varchar(4) NOT NULL default ' ',
  "AAABHDKF" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDJM" (
  "AAABLITM" varchar(20) NOT NULL default ' ',
  "AAABHDJO" varchar(1) NOT NULL default ' ',
  "AAABLOZE" varchar(1) NOT NULL default ' ',
  "AAABHDJQ" varchar(1) NOT NULL default ' ',
  "AAABHDJR" varchar(20) NOT NULL default ' ',
  "AAABHDJS" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(64) NOT NULL default ' ',
  "AAABHDJU" varchar(12) NOT NULL default ' ',
  "AAABHDJV" varchar(14) NOT NULL default ' ',
  "AAABHDJW" varchar(4) NOT NULL default ' ',
  "AAABHDJX" varchar(8) NOT NULL default ' ',
  "AAABHDJY" varchar(12) NOT NULL default ' ',
  "AAABHDJZ" varchar(14) NOT NULL default ' ',
  "AAABHDKA" varchar(4) NOT NULL default ' ',
  "AAABHDKB" varchar(8) NOT NULL default ' ',
  "AAABHDKC" varchar(12) NOT NULL default ' ',
  "AAABHDKD" varchar(14) NOT NULL default ' ',
  "AAABHDKE" varchar(4) NOT NULL default ' ',
  "AAABHDKF" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABLITM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDKH" (
  "AAABLITM" varchar(20) NOT NULL default ' ',
  "AAABHDKJ" int(11) NOT NULL default '0',
  "AAABHDKK" int(11) NOT NULL default '0',
  "AAABHDKL" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLITM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDKN" (
  "AAABHDKS" varchar(3) NOT NULL default '000',
  "AAABHDKT" varchar(12) NOT NULL default ' ',
  "AAABJECS" varchar(16) NOT NULL default ' ',
  "AAABHDKR" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAABHDKS","AAABHDKT","AAABJECS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDKV" (
  "AAABHDLA" varchar(3) NOT NULL default '000',
  "AAABICGC" varchar(12) NOT NULL default ' ',
  "AAABHDLC" varchar(4) NOT NULL default ' ',
  "AAABHDLD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABHDLA","AAABICGC","AAABHDLC","AAABHDLD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDLE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHQRM" varchar(2) NOT NULL default ' ',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  "AAABHDLR" varchar(1) NOT NULL default ' ',
  "AAABHDLS" varchar(1) NOT NULL default ' ',
  "AAABHDLK" varchar(6) NOT NULL default ' ',
  "AAABHDLL" varchar(1) NOT NULL default ' ',
  "AAABHDLM" varchar(1) NOT NULL default ' ',
  "AAABKRGH" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHQRM","AAABLTQA","AAABHDLR","AAABHDLS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDLT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHQRM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHQRM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDLY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHQRM" varchar(2) NOT NULL default ' ',
  "AAABLPWW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHQRM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDMG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKMHE" varchar(9) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKMHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDML" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKMHE" varchar(9) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHDMP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKMHE","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDMT" (
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABKYPV" varchar(20) NOT NULL default ' ',
  "AAABLEES" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDMY" (
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABHDND" varchar(40) NOT NULL default ' ',
  "AAABLEER" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLTVR","AAABHDND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDNE" (
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABHEWL" varchar(5) NOT NULL default '00000',
  "AAABHDNO" varchar(30) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  "AAABLUEQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER","AAABHEWL","AAABHDNO","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDNQ" (
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABHDQK" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABKMYU","AAABIRUN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDNW" (
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABHDQK" varchar(8) NOT NULL default '00000000',
  "AAABLENY" varchar(3) NOT NULL default ' ',
  "AAABHDOB" varchar(1) NOT NULL default ' ',
  "AAABHDOC" varchar(3) NOT NULL default '000',
  "AAABLENX" varchar(3) NOT NULL default ' ',
  "AAABHDOE" varchar(1) NOT NULL default ' ',
  "AAABHDOF" varchar(3) NOT NULL default '000',
  "AAABJSWF" varchar(3) NOT NULL default ' ',
  "AAABHDOH" varchar(1) NOT NULL default ' ',
  "AAABHDOI" varchar(3) NOT NULL default '000',
  "AAABJSWG" varchar(3) NOT NULL default ' ',
  "AAABHDOK" varchar(1) NOT NULL default ' ',
  "AAABHDOL" varchar(3) NOT NULL default '000',
  "AAABJSVZ" varchar(3) NOT NULL default ' ',
  "AAABHDON" varchar(1) NOT NULL default ' ',
  "AAABHDOO" varchar(3) NOT NULL default '000',
  "AAABJSWA" varchar(3) NOT NULL default ' ',
  "AAABHDOQ" varchar(1) NOT NULL default ' ',
  "AAABHDOR" varchar(3) NOT NULL default '000',
  "AAABJSWB" varchar(3) NOT NULL default ' ',
  "AAABHDOT" varchar(1) NOT NULL default ' ',
  "AAABHDOU" varchar(3) NOT NULL default '000',
  "AAABJSWC" varchar(3) NOT NULL default ' ',
  "AAABHDOW" varchar(1) NOT NULL default ' ',
  "AAABHDOX" varchar(3) NOT NULL default '000',
  "AAABHDOY" varchar(3) NOT NULL default ' ',
  "AAABHDOZ" varchar(1) NOT NULL default ' ',
  "AAABHDPA" varchar(3) NOT NULL default '000',
  "AAABHDPB" varchar(3) NOT NULL default ' ',
  "AAABHDPC" varchar(1) NOT NULL default ' ',
  "AAABHDPD" varchar(3) NOT NULL default '000',
  "AAABHDPE" varchar(3) NOT NULL default ' ',
  "AAABHDPF" varchar(1) NOT NULL default ' ',
  "AAABHDPG" varchar(3) NOT NULL default '000',
  "AAABHDPH" varchar(3) NOT NULL default ' ',
  "AAABHDPI" varchar(1) NOT NULL default ' ',
  "AAABHDPJ" varchar(3) NOT NULL default '000',
  "AAABHDPK" varchar(3) NOT NULL default ' ',
  "AAABHDPL" varchar(1) NOT NULL default ' ',
  "AAABHDPM" varchar(3) NOT NULL default '000',
  "AAABHDPN" varchar(3) NOT NULL default ' ',
  "AAABHDPO" varchar(1) NOT NULL default ' ',
  "AAABHDPP" varchar(3) NOT NULL default '000',
  "AAABHDPQ" varchar(3) NOT NULL default ' ',
  "AAABHDPR" varchar(1) NOT NULL default ' ',
  "AAABHDPS" varchar(3) NOT NULL default '000',
  "AAABHDPT" varchar(3) NOT NULL default ' ',
  "AAABHDPU" varchar(1) NOT NULL default ' ',
  "AAABHDPV" varchar(3) NOT NULL default '000',
  "AAABHDPW" varchar(3) NOT NULL default ' ',
  "AAABHDPX" varchar(1) NOT NULL default ' ',
  "AAABHDPY" varchar(3) NOT NULL default '000',
  "AAABHDPZ" varchar(3) NOT NULL default ' ',
  "AAABHDQA" varchar(1) NOT NULL default ' ',
  "AAABHDQB" varchar(3) NOT NULL default '000',
  "AAABHDQC" varchar(3) NOT NULL default ' ',
  "AAABHDQD" varchar(1) NOT NULL default ' ',
  "AAABHDQE" varchar(3) NOT NULL default '000',
  "AAABHDQF" varchar(3) NOT NULL default ' ',
  "AAABHDQG" varchar(1) NOT NULL default ' ',
  "AAABHDQH" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABKMYU","AAABIRUN","AAABHDQK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDQL" (
  "AAABHDSI" varchar(3) NOT NULL default '000',
  "AAABLUHV" varchar(20) NOT NULL default ' ',
  "AAABHDSK" smallint(6) NOT NULL default '0',
  "AAABHFFR" varchar(12) NOT NULL default ' ',
  "AAABHDQQ" varchar(1) NOT NULL default ' ',
  "AAABHDQR" varchar(1) NOT NULL default ' ',
  "AAABHDQS" varchar(4) NOT NULL default ' ',
  "AAABHDQT" varchar(16) NOT NULL default ' ',
  "AAABHDQU" varchar(1) NOT NULL default ' ',
  "AAABJXFU" varchar(1) NOT NULL default ' ',
  "AAABHDQW" varchar(16) NOT NULL default ' ',
  "AAABHDQX" varchar(40) NOT NULL default ' ',
  "AAABHDQY" varchar(12) NOT NULL default ' ',
  "AAABHDQZ" varchar(4) NOT NULL default ' ',
  "AAABHDRA" int(11) NOT NULL default '0',
  "AAABHDRB" smallint(6) NOT NULL default '0',
  "AAABHDRC" int(11) NOT NULL default '0',
  "AAABHDRD" smallint(6) NOT NULL default '0',
  "AAABHDRE" int(11) NOT NULL default '0',
  "AAABHDRF" smallint(6) NOT NULL default '0',
  "AAABHDRG" varchar(1) NOT NULL default ' ',
  "AAABHDRH" varchar(12) NOT NULL default ' ',
  "AAABHDRI" int(11) NOT NULL default '0',
  "AAABHDRJ" int(11) NOT NULL default '0',
  "AAABHDRK" int(11) NOT NULL default '0',
  "AAABHDRL" int(11) NOT NULL default '0',
  "AAABHDRM" smallint(6) NOT NULL default '0',
  "AAABHDRN" varchar(7) NOT NULL default ' ',
  "AAABHDRO" varchar(4) NOT NULL default '0000',
  "AAABHDRP" smallint(6) NOT NULL default '0',
  "AAABHDRQ" smallint(6) NOT NULL default '0',
  "AAABHDRR" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABHDSI","AAABLUHV","AAABHDSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDRV" (
  "AAABHDSA" int(11) NOT NULL default '0',
  "AAABHDRX" smallint(6) NOT NULL default '0',
  "AAABHDRY" smallint(6) NOT NULL default '0',
  "AAABHDRZ" blob,
  PRIMARY KEY ("AAABHDSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDSB" (
  "AAABHDSI" varchar(3) NOT NULL default '000',
  "AAABLUHV" varchar(20) NOT NULL default ' ',
  "AAABHDSK" smallint(6) NOT NULL default '0',
  "AAABHDSL" int(11) NOT NULL default '0',
  "AAABHDSG" smallint(6) NOT NULL default '0',
  "AAABHDSH" blob,
  PRIMARY KEY ("AAABHDSI","AAABLUHV","AAABHDSK","AAABHDSL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDSM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHZBP" varchar(12) NOT NULL default ' ',
  "AAABHDSV" varchar(16) NOT NULL default ' ',
  "AAABHDSW" varchar(8) NOT NULL default ' ',
  "AAABHDSR" varchar(1) NOT NULL default ' ',
  "AAABHDSS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABHZBP","AAABHDSV","AAABHDSW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDSX" (
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABLGCM" varchar(20) NOT NULL default ' ',
  "AAABLSDL" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNJW","AAABLGCM","AAABLSDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDTF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHZBP" varchar(12) NOT NULL default ' ',
  "AAABHDVT" varchar(20) NOT NULL default ' ',
  "AAABHDTJ" varchar(1) NOT NULL default ' ',
  "AAABHDTK" varchar(8) NOT NULL default '00000000',
  "AAABHDTL" varchar(6) NOT NULL default '000000',
  "AAABHDTM" varchar(10) NOT NULL default ' ',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABMDEI" varchar(20) NOT NULL default ' ',
  "AAABLXFG" varchar(40) NOT NULL default ' ',
  "AAABHDTQ" varchar(20) NOT NULL default ' ',
  "AAABHDTR" varchar(1) NOT NULL default ' ',
  "AAABHDTS" varchar(1) NOT NULL default ' ',
  "AAABHDTT" varchar(1) NOT NULL default ' ',
  "AAABHDTU" varchar(1) NOT NULL default ' ',
  "AAABHDTV" varchar(1) NOT NULL default ' ',
  "AAABHDTW" varchar(1) NOT NULL default ' ',
  "AAABHDTX" varchar(1) NOT NULL default ' ',
  "AAABHDTY" varchar(1) NOT NULL default ' ',
  "AAABHDTZ" varchar(1) NOT NULL default ' ',
  "AAABHDUA" varchar(1) NOT NULL default ' ',
  "AAABHDUB" varchar(1) NOT NULL default ' ',
  "AAABHDUC" varchar(1) NOT NULL default ' ',
  "AAABHDUD" varchar(1) NOT NULL default ' ',
  "AAABHDUE" varchar(1) NOT NULL default ' ',
  "AAABHDUF" varchar(1) NOT NULL default ' ',
  "AAABHDUG" varchar(1) NOT NULL default ' ',
  "AAABHDUH" varchar(1) NOT NULL default ' ',
  "AAABHDUI" varchar(1) NOT NULL default ' ',
  "AAABHDUJ" varchar(1) NOT NULL default ' ',
  "AAABHDUK" varchar(8) NOT NULL default ' ',
  "AAABHDUL" varchar(8) NOT NULL default ' ',
  "AAABHDUM" varchar(8) NOT NULL default ' ',
  "AAABHDUN" varchar(8) NOT NULL default ' ',
  "AAABHDUO" varchar(8) NOT NULL default ' ',
  "AAABHDUP" varchar(8) NOT NULL default ' ',
  "AAABHDUQ" varchar(8) NOT NULL default ' ',
  "AAABHDUR" varchar(8) NOT NULL default ' ',
  "AAABHDUS" varchar(8) NOT NULL default ' ',
  "AAABHDUT" varchar(8) NOT NULL default ' ',
  "AAABHDUU" varchar(30) NOT NULL default ' ',
  "AAABHDUV" varchar(30) NOT NULL default ' ',
  "AAABHDUW" varchar(30) NOT NULL default ' ',
  "AAABHDUX" varchar(30) NOT NULL default ' ',
  "AAABHDUY" varchar(30) NOT NULL default ' ',
  "AAABHDUZ" varchar(30) NOT NULL default ' ',
  "AAABHDVA" varchar(30) NOT NULL default ' ',
  "AAABHDVB" varchar(30) NOT NULL default ' ',
  "AAABHDVC" varchar(30) NOT NULL default ' ',
  "AAABHDVD" varchar(30) NOT NULL default ' ',
  "AAABHDVE" varchar(30) NOT NULL default ' ',
  "AAABHDVF" varchar(30) NOT NULL default ' ',
  "AAABHDVG" varchar(30) NOT NULL default ' ',
  "AAABHDVH" varchar(30) NOT NULL default ' ',
  "AAABHDVI" varchar(30) NOT NULL default ' ',
  "AAABHDVJ" varchar(30) NOT NULL default ' ',
  "AAABHDVK" varchar(30) NOT NULL default ' ',
  "AAABHDVL" varchar(30) NOT NULL default ' ',
  "AAABHDVM" varchar(30) NOT NULL default ' ',
  "AAABHDVN" varchar(30) NOT NULL default ' ',
  "AAABHDVO" varchar(1) NOT NULL default ' ',
  "AAABHDVP" varchar(6) NOT NULL default ' ',
  "AAABHDVQ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABHZBP","AAABHDVT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDVU" (
  "AAABHECL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABHECL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDVX" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABMKWZ" varchar(32) NOT NULL default ' ',
  "AAABHEGL" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABMKWZ"),
  KEY "AAABHDWD" ("AAABHEGL","AAABMKWZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDWG" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABHDWL" varchar(32) NOT NULL default ' ',
  "AAABHEGL" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABHDWL"),
  KEY "AAABHDWM" ("AAABHEGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDWO" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABMRFO" varchar(10) NOT NULL default ' ',
  "AAABHEDD" varchar(8) NOT NULL default '00000000',
  "AAABHEDE" varchar(8) NOT NULL default '00000000',
  "AAABHEDF" smallint(6) NOT NULL default '0',
  "AAABHEDG" varchar(8) NOT NULL default '00000000',
  "AAABHEDH" varchar(8) NOT NULL default '00000000',
  "AAABHEDI" smallint(6) NOT NULL default '0',
  "AAABHEDJ" smallint(6) NOT NULL default '0',
  "AAABLNXD" varchar(3) NOT NULL default '000',
  "AAABMNRG" varchar(1) NOT NULL default ' ',
  "AAABHDXA" varchar(32) NOT NULL default ' ',
  "AAABHDXB" varchar(4) NOT NULL default ' ',
  "AAABHDXC" varchar(10) NOT NULL default ' ',
  "AAABHDXD" varchar(4) NOT NULL default ' ',
  "AAABHDXE" varchar(10) NOT NULL default ' ',
  "AAABHDXF" varchar(10) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDXN" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABHEFZ" varchar(50) NOT NULL default ' ',
  "AAABHEGA" varchar(50) NOT NULL default ' ',
  "AAABHEGB" varchar(50) NOT NULL default ' ',
  "AAABHEGC" varchar(50) NOT NULL default ' ',
  "AAABHEGD" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDXX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABHEFZ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDYD" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABMRFO" varchar(10) NOT NULL default ' ',
  "AAABHEEO" varchar(1) NOT NULL default ' ',
  "AAABHEEP" varchar(1) NOT NULL default ' ',
  "AAABHEEQ" varchar(1) NOT NULL default ' ',
  "AAABHEER" varchar(1) NOT NULL default ' ',
  "AAABHEES" varchar(1) NOT NULL default ' ',
  "AAABHEET" varchar(1) NOT NULL default ' ',
  "AAABHEEU" varchar(20) NOT NULL default ' ',
  "AAABHDYN" varchar(1) NOT NULL default ' ',
  "AAABHEDW" varchar(8) NOT NULL default ' ',
  "AAABHEDX" varchar(5) NOT NULL default '00000',
  "AAABHDYQ" decimal(10,3) NOT NULL default '0.000',
  "AAABHDYR" varchar(3) NOT NULL default ' ',
  "AAABHDYS" varchar(50) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABIGIZ" varchar(10) NOT NULL default '0000000000',
  "AAABHDYV" varchar(6) NOT NULL default '000000',
  "AAABHDYW" varchar(4) NOT NULL default ' ',
  "AAABHDYX" varchar(20) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDZF" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABJTAQ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABJTAQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDZK" (
  "AAABMREL" varchar(20) NOT NULL default ' ',
  "AAABLOZE" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHDZO" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABHDZX" varchar(10) NOT NULL default ' ',
  "AAABHEGZ" varchar(10) NOT NULL default '0000000000',
  "AAABHEHA" varchar(4) NOT NULL default '0000',
  "AAABLGEF" varchar(3) NOT NULL default ' ',
  "AAABHDZU" varchar(3) NOT NULL default ' ',
  "AAABHDZV" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABHDZX","AAABHEGZ","AAABHEHA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEAA" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABHEAF" varchar(12) NOT NULL default ' ',
  "AAABHEAD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABHEAF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEAG" (
  "AAABMRFO" varchar(10) NOT NULL default ' ',
  "AAABHECL" varchar(10) NOT NULL default ' ',
  "AAABHECM" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEAL" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEAR" (
  "AAABLUFA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEAU" (
  "AAABJEXI" varchar(3) NOT NULL default '000',
  "AAABLMVF" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEXI","AAABLMVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEAZ" (
  "AAABLMVF" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEBC" (
  "AAABLOZE" varchar(6) NOT NULL default ' ',
  "AAABMRFO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEBH" (
  "AAABLMVF" varchar(12) NOT NULL default ' ',
  "AAABHEHZ" varchar(4) NOT NULL default ' ',
  "AAABJEXI" varchar(3) NOT NULL default '000',
  "AAABHEIG" varchar(12) NOT NULL default ' ',
  "AAABHEIC" varchar(20) NOT NULL default ' ',
  "AAABHEIH" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMVF","AAABHEHZ","AAABJEXI","AAABHEIG","AAABHEIC","AAABHEIH"),
  KEY "AAABHEBU" ("AAABJEXI","AAABHEIG","AAABHEIH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEBY" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABLMVF" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABLMVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHECD" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLMVF" varchar(12) NOT NULL default ' ',
  "AAABLUDG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLMVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHECJ" (
  "AAABMRFO" varchar(10) NOT NULL default ' ',
  "AAABHECL" varchar(10) NOT NULL default ' ',
  "AAABHECM" tinyint(3) unsigned NOT NULL default '0',
  "AAABHECN" varchar(7) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHECP" (
  "AAABJEXI" varchar(3) NOT NULL default '000',
  "AAABHECV" varchar(20) NOT NULL default ' ',
  "AAABMRFO" varchar(10) NOT NULL default ' ',
  "AAABHECT" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEXI","AAABHECV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHECW" (
  "AAABHEHZ" varchar(4) NOT NULL default ' ',
  "AAABHEIC" varchar(20) NOT NULL default ' ',
  "AAABJEXI" varchar(3) NOT NULL default '000',
  "AAABHEIG" varchar(12) NOT NULL default ' ',
  "AAABHEIH" varchar(12) NOT NULL default ' ',
  "AAABMRFO" varchar(10) NOT NULL default ' ',
  "AAABHEDD" varchar(8) NOT NULL default '00000000',
  "AAABHEDE" varchar(8) NOT NULL default '00000000',
  "AAABHEDF" smallint(6) NOT NULL default '0',
  "AAABHEDG" varchar(8) NOT NULL default '00000000',
  "AAABHEDH" varchar(8) NOT NULL default '00000000',
  "AAABHEDI" smallint(6) NOT NULL default '0',
  "AAABHEDJ" smallint(6) NOT NULL default '0',
  "AAABLNXD" varchar(3) NOT NULL default '000',
  "AAABHEEO" varchar(1) NOT NULL default ' ',
  "AAABHEEP" varchar(1) NOT NULL default ' ',
  "AAABHEEQ" varchar(1) NOT NULL default ' ',
  "AAABHEER" varchar(1) NOT NULL default ' ',
  "AAABHEES" varchar(1) NOT NULL default ' ',
  "AAABHEET" varchar(1) NOT NULL default ' ',
  "AAABHEEU" varchar(20) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABHEDW" varchar(8) NOT NULL default ' ',
  "AAABHEDX" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABHEHZ","AAABHEIC","AAABJEXI","AAABHEIG","AAABHEIH"),
  KEY "AAABHEED" ("AAABJEXI","AAABHEIG","AAABHEIH"),
  KEY "AAABHEEH" ("AAABHZPG"),
  KEY "AAABHEEJ" ("AAABLNYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEEL" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABMRFO" varchar(10) NOT NULL default ' ',
  "AAABHEEO" varchar(1) NOT NULL default ' ',
  "AAABHEEP" varchar(1) NOT NULL default ' ',
  "AAABHEEQ" varchar(1) NOT NULL default ' ',
  "AAABHEER" varchar(1) NOT NULL default ' ',
  "AAABHEES" varchar(1) NOT NULL default ' ',
  "AAABHEET" varchar(1) NOT NULL default ' ',
  "AAABHEEU" varchar(20) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEFA" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHEHZ" varchar(4) NOT NULL default ' ',
  "AAABHEIC" varchar(20) NOT NULL default ' ',
  "AAABJEXI" varchar(3) NOT NULL default '000',
  "AAABHEIG" varchar(12) NOT NULL default ' ',
  "AAABHEIH" varchar(12) NOT NULL default ' ',
  "AAABHEFZ" varchar(50) NOT NULL default ' ',
  "AAABHEGA" varchar(50) NOT NULL default ' ',
  "AAABHEGB" varchar(50) NOT NULL default ' ',
  "AAABHEGC" varchar(50) NOT NULL default ' ',
  "AAABHEGD" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABHEHZ","AAABHEIC","AAABJEXI","AAABHEIG","AAABHEIH"),
  KEY "AAABHEFS" ("AAABJEXI","AAABHEIG","AAABHEIH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEFW" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABHEFZ" varchar(50) NOT NULL default ' ',
  "AAABHEGA" varchar(50) NOT NULL default ' ',
  "AAABHEGB" varchar(50) NOT NULL default ' ',
  "AAABHEGC" varchar(50) NOT NULL default ' ',
  "AAABHEGD" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEGG" (
  "AAABHZQN" varchar(70) NOT NULL default ' ',
  "AAABHEGL" varchar(32) NOT NULL default ' ',
  "AAABJEXI" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZQN","AAABHEGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEGM" (
  "AAABHEGP" varchar(10) NOT NULL default ' ',
  "AAABMRFO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEGP","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEGR" (
  "AAABHEGZ" varchar(10) NOT NULL default '0000000000',
  "AAABHEHA" varchar(4) NOT NULL default '0000',
  "AAABHEHZ" varchar(4) NOT NULL default ' ',
  "AAABJEXI" varchar(3) NOT NULL default '000',
  "AAABHEIG" varchar(12) NOT NULL default ' ',
  "AAABHEIC" varchar(20) NOT NULL default ' ',
  "AAABHEIH" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEGZ","AAABHEHA","AAABHEHZ","AAABJEXI","AAABHEIG","AAABHEIC","AAABHEIH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEHG" (
  "AAABJEXI" varchar(3) NOT NULL default '000',
  "AAABHEIS" varchar(12) NOT NULL default ' ',
  "AAABHEHJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEXI","AAABHEIS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEHM" (
  "AAABHEIS" varchar(12) NOT NULL default ' ',
  "AAABMMVU" varchar(2) NOT NULL default ' ',
  "AAABHEHP" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEIS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEHR" (
  "AAABHEIS" varchar(12) NOT NULL default ' ',
  "AAABHEHZ" varchar(4) NOT NULL default ' ',
  "AAABJEXI" varchar(3) NOT NULL default '000',
  "AAABHEIG" varchar(12) NOT NULL default ' ',
  "AAABHEIC" varchar(20) NOT NULL default ' ',
  "AAABHEIH" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEIS","AAABHEHZ","AAABJEXI","AAABHEIG","AAABHEIC","AAABHEIH"),
  KEY "AAABHEIE" ("AAABJEXI","AAABHEIG","AAABHEIH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEII" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABHEIS" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABHEIS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEIN" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHEIS" varchar(12) NOT NULL default ' ',
  "AAABLUDG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABHEIS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEIT" (
  "AAABJEXI" varchar(3) NOT NULL default '000',
  "AAABMRFO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEXI","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEIY" (
  "AAABMRFO" varchar(10) NOT NULL default ' ',
  "AAABLUFA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEJC" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(10) NOT NULL default ' ',
  "AAABLUDG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMRFO"),
  KEY "AAABHEJI" ("AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEJK" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEJQ" (
  "AAABHELR" varchar(2) NOT NULL default ' ',
  "AAABHEKK" varchar(1) NOT NULL default ' ',
  "AAABHEJT" varchar(1) NOT NULL default ' ',
  "AAABHEJU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHELR","AAABHEKK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEJX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHELR" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHELR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEKD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default ' ',
  "AAABLHTL" varchar(4) NOT NULL default ' ',
  "AAABLHTE" varchar(4) NOT NULL default ' ',
  "AAABHELR" varchar(2) NOT NULL default ' ',
  "AAABHEKK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJBH","AAABLTLR","AAABLHTL","AAABLHTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEKQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIBK" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABHELR" varchar(2) NOT NULL default ' ',
  "AAABHENP" varchar(2) NOT NULL default ' ',
  "AAABHEKX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLIBK","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHELC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIFR" varchar(4) NOT NULL default ' ',
  "AAABHELF" varchar(1) NOT NULL default ' ',
  "AAABHELG" varchar(1) NOT NULL default ' ',
  "AAABHELH" varchar(1) NOT NULL default ' ',
  "AAABHELI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLIFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHELL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIFR" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABHELQ" varchar(2) NOT NULL default ' ',
  "AAABHELR" varchar(2) NOT NULL default ' ',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABHELU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLIFR","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHELZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLIFR" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMOHX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLIFR","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEML" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLIBK" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMOHX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLIBK","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEMX" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHENC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHENP" varchar(2) NOT NULL default ' ',
  "AAABHENF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHENP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHENI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHENP" varchar(2) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHENP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHENQ" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABHEQK" varchar(40) NOT NULL default ' ',
  "AAABHEQE" varchar(4) NOT NULL default '0000',
  "AAABILLA" varchar(5) NOT NULL default ' ',
  "AAABHEQG" VARBINARY(255),
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP"),
  KEY "AAABHENY" ("AAABHEQK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEOA" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMLBW" varchar(10) NOT NULL default ' ',
  "AAABMKXW" varchar(10) NOT NULL default ' ',
  "AAABMMJT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP","AAABMLBW","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEOI" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMLBW" varchar(10) NOT NULL default ' ',
  "AAABMKXW" varchar(10) NOT NULL default ' ',
  "AAABMMJT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP","AAABMLBW","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEOQ" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABHXAM" varchar(1) NOT NULL default ' ',
  "AAABHXAN" varchar(1) NOT NULL default ' ',
  "AAABHXAO" varchar(1) NOT NULL default ' ',
  "AAABHXAP" varchar(14) NOT NULL default ' ',
  "AAABHEOW" varchar(1) NOT NULL default ' ',
  "AAABHEOX" varchar(1) NOT NULL default ' ',
  "AAABHEOY" varchar(1) NOT NULL default ' ',
  "AAABHEOZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP"),
  KEY "AAABHEPB" ("AAABHXAM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEPD" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABIMXR" varchar(1) NOT NULL default ' ',
  "AAABLFSM" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP"),
  KEY "AAABHEPI" ("AAABIMXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEPK" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMJWN" varchar(254) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEPO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNLE" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABKNLE","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEPV" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABILRJ" varchar(36) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEQB" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABHEQK" varchar(40) NOT NULL default ' ',
  "AAABHEQE" varchar(4) NOT NULL default '0000',
  "AAABILLA" varchar(5) NOT NULL default ' ',
  "AAABHEQG" VARBINARY(255),
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP"),
  KEY "AAABHEQJ" ("AAABHEQK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEQL" (
  "AAABMDEP" varchar(30) NOT NULL default ' ',
  "AAABILHV" varchar(1) NOT NULL default ' ',
  "AAABINIL" varchar(12) NOT NULL default ' ',
  "AAABLEER" varchar(8) NOT NULL default '00000000',
  "AAABINIK" varchar(6) NOT NULL default '000000',
  "AAABHEQR" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEQT" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABHJPB" varchar(2) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABHEQX" varchar(48) NOT NULL default ' ',
  "AAABHEQY" varchar(14) NOT NULL default ' ',
  "AAABIOAE" varchar(12) NOT NULL default ' ',
  "AAABHERA" varchar(4) NOT NULL default '0000',
  "AAABHERB" varchar(4) NOT NULL default '0000',
  "AAABHERC" varchar(1) NOT NULL default ' ',
  "AAABHERD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHERF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZZH" varchar(2) NOT NULL default ' ',
  "AAABMPMH" varchar(5) NOT NULL default ' ',
  "AAABLMLR" varchar(2) NOT NULL default '00',
  "AAABHERK" varchar(1) NOT NULL default ' ',
  "AAABHERL" varchar(1) NOT NULL default ' ',
  "AAABHERM" varchar(1) NOT NULL default ' ',
  "AAABHERN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZZH","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHERR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHZZH" varchar(2) NOT NULL default ' ',
  "AAABMPMH" varchar(5) NOT NULL default ' ',
  "AAABLPWW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHZZH","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHESB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZZH" varchar(2) NOT NULL default ' ',
  "AAABMLXR" varchar(50) NOT NULL default ' ',
  "AAABHESX" varchar(8) NOT NULL default '00000000',
  "AAABHETP" varchar(2) NOT NULL default ' ',
  "AAABHESH" varchar(40) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABHESL" varchar(6) NOT NULL default '000000',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABJFLQ" varchar(8) NOT NULL default '00000000',
  "AAABHFNY" varchar(6) NOT NULL default '000000',
  "AAABHESP" varchar(1) NOT NULL default ' ',
  "AAABHESQ" varchar(1) NOT NULL default ' ',
  "AAABHESR" varchar(1) NOT NULL default ' ',
  "AAABHETC" varchar(1) NOT NULL default ' ',
  "AAABLILS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZZH","AAABMLXR","AAABHESX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHESY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZZH" varchar(2) NOT NULL default ' ',
  "AAABHETP" varchar(2) NOT NULL default ' ',
  "AAABHETC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZZH","AAABHETP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHETG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHZZH" varchar(2) NOT NULL default ' ',
  "AAABHETP" varchar(2) NOT NULL default ' ',
  "AAABHETL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHZZH","AAABHETP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHETQ" (
  "AAABHETT" varchar(4) NOT NULL default ' ',
  "AAABLQHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABHETT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHETU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHEUY" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHEUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHETZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHHSH" varchar(2) NOT NULL default ' ',
  "AAABHPYD" varchar(10) NOT NULL default ' ',
  "AAABHEUL" varchar(2) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABJTNV" varchar(70) NOT NULL default ' ',
  "AAABHEUQ" varchar(10) NOT NULL default ' ',
  "AAABHEUH" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABHHSH","AAABHPYD","AAABHEUL"),
  UNIQUE KEY "AAABHEUM" ("AAABLUXQ","AAABLPJD","AAABJTNV","AAABHEUQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEUR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHEUY" varchar(14) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHEUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEUZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHEVN" varchar(1) NOT NULL default ' ',
  "AAABKQPE" varchar(2) NOT NULL default '00',
  "AAABLUVS" varchar(18) NOT NULL default ' ',
  "AAABHEVE" varchar(4) NOT NULL default ' ',
  "AAABHEVF" varchar(4) NOT NULL default ' ',
  "AAABHEVG" varchar(4) NOT NULL default ' ',
  "AAABHEVH" varchar(4) NOT NULL default ' ',
  "AAABHEVI" varchar(4) NOT NULL default ' ',
  "AAABHEVJ" varchar(4) NOT NULL default ' ',
  "AAABHEVK" varchar(4) NOT NULL default ' ',
  "AAABHEVL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHEVN","AAABKQPE","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEVQ" (
  "AAABMNLA" varchar(4) NOT NULL default ' ',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNLA","AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEVV" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMNLA" varchar(4) NOT NULL default ' ',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAABLUDG" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMNLA","AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEWD" (
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABHEWL" varchar(5) NOT NULL default '00000',
  "AAABLUEQ" varchar(30) NOT NULL default ' ',
  "AAABMLZS" varchar(3) NOT NULL default '000',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER","AAABHEWL","AAABLUEQ","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEWO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHEWW" varchar(4) NOT NULL default ' ',
  "AAABHEWX" varchar(1) NOT NULL default ' ',
  "AAABHEWT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHEWW","AAABHEWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEWY" (
  "AAABHEXF" varchar(11) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABJWRR" varchar(3) NOT NULL default ' ',
  "AAABMGVK" varchar(60) NOT NULL default ' ',
  "AAABLVHI" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEXF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEXH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSKS" varchar(12) NOT NULL default ' ',
  "AAABJCDT" varchar(8) NOT NULL default ' ',
  "AAABHEXL" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEXO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSKS" varchar(12) NOT NULL default ' ',
  "AAABHEXS" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEXW" (
  "AAABHEYG" varchar(20) NOT NULL default ' ',
  "AAABHFAO" varchar(20) NOT NULL default ' ',
  "AAABHFAQ" varchar(10) NOT NULL default ' ',
  "AAABHFAR" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAABHFAM" varchar(10) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABHEYE" varchar(4) NOT NULL default ' ',
  "AAABHEYF" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEYG","AAABHFAO","AAABHFAQ","AAABHFAR","AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEYL" (
  "AAABLOZE" varchar(4) NOT NULL default ' ',
  "AAABKSUQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEYP" (
  "AAABHEYT" varchar(20) NOT NULL default ' ',
  "AAABHEYU" varchar(20) NOT NULL default ' ',
  "AAABILWI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEYT","AAABHEYU","AAABILWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEYW" (
  "AAABJEXI" varchar(20) NOT NULL default ' ',
  "AAABHEZI" varchar(20) NOT NULL default ' ',
  "AAABHEZJ" varchar(20) NOT NULL default ' ',
  "AAABHFAP" varchar(20) NOT NULL default ' ',
  "AAABHEZB" varchar(10) NOT NULL default ' ',
  "AAABHEZC" varchar(10) NOT NULL default ' ',
  "AAABHEZD" varchar(3) NOT NULL default ' ',
  "AAABHEZE" varchar(10) NOT NULL default ' ',
  "AAABLERE" varchar(14) NOT NULL default '00000000000000',
  "AAABHEZG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEXI","AAABHEZI","AAABHEZJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEZK" (
  "AAABHEZO" varchar(2) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABKUQR" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABHEZO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEZP" (
  "AAABINFD" varchar(10) NOT NULL default ' ',
  "AAABIANG" varchar(60) NOT NULL default ' ',
  "AAABLFZT" varchar(1) NOT NULL default ' ',
  "AAABHEZT" varchar(4) NOT NULL default ' ',
  "AAABHEZU" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEZW" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHEZZ" (
  "AAABINFD" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIANG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFD","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFAF" (
  "AAABJEXI" varchar(20) NOT NULL default ' ',
  "AAABHFAO" varchar(20) NOT NULL default ' ',
  "AAABHFAP" varchar(20) NOT NULL default ' ',
  "AAABHFAQ" varchar(10) NOT NULL default ' ',
  "AAABHFAR" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAABHFAM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEXI","AAABHFAO","AAABHFAP","AAABHFAQ","AAABHFAR","AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFAT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFBF" varchar(5) NOT NULL default '00000',
  "AAABMCMG" varchar(2) NOT NULL default ' ',
  "AAABHFAX" varchar(8) NOT NULL default '00000000',
  "AAABHFAY" varchar(13) NOT NULL default ' ',
  "AAABMQVX" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABHFBB" decimal(13,2) NOT NULL default '0.00',
  "AAABHFBC" varchar(5) NOT NULL default ' ',
  "AAABHFBD" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFBF","AAABMCMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFBH" (
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABHFBJ" varchar(32) NOT NULL default ' ',
  "AAABHFBK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLZKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFBM" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLELF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFBS" (
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
  "AAABHFCG" varchar(12) NOT NULL default ' ',
  "AAABHFCH" varchar(1) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLRST","AAABMKFH","AAABLFYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFCN" (
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFCQ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABLPWW" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFCW" (
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABKMLX" varchar(12) NOT NULL default ' ',
  "AAABKMLY" varchar(4) NOT NULL default ' ',
  "AAABKMLZ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLESS","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFDE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABIFJO" varchar(4) NOT NULL default ' ',
  "AAABIFJX" varchar(60) NOT NULL default ' ',
  "AAABMEEL" tinyint(3) unsigned NOT NULL default '0',
  "AAABMEEK" varchar(4) NOT NULL default ' ',
  "AAABIFKT" varchar(8) NOT NULL default ' ',
  "AAABIFKA" varchar(1) NOT NULL default ' ',
  "AAABIFKB" varchar(1) NOT NULL default ' ',
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
  "AAABIFKO" varchar(3) NOT NULL default ' ',
  "AAABIFKP" varchar(30) NOT NULL default ' ',
  "AAABIFKR" varchar(50) NOT NULL default ' ',
  "AAABIFLJ" varchar(1) NOT NULL default ' ',
  "AAABIFLI" varchar(40) NOT NULL default ' ',
  "AAABIFLK" varchar(15) NOT NULL default '000000000000000',
  "AAABMEEP" varchar(1) NOT NULL default ' ',
  "AAABMEES" varchar(68) NOT NULL default ' ',
  "AAABMEEQ" varchar(12) NOT NULL default ' ',
  "AAABMEER" varchar(12) NOT NULL default ' ',
  "AAABMEET" varchar(12) NOT NULL default ' ',
  "AAABLKWD" varchar(1) NOT NULL default ' ',
  "AAABIFLR" varchar(1) NOT NULL default ' ',
  "AAABIFLT" varchar(1) NOT NULL default ' ',
  "AAABIFLW" varchar(12) NOT NULL default ' ',
  "AAABHFEO" varchar(4) NOT NULL default ' ',
  "AAABHFEP" varchar(1) NOT NULL default ' ',
  "AAABIFLY" varchar(1) NOT NULL default ' ',
  "AAABIFLX" varchar(50) NOT NULL default ' ',
  "AAABIFLZ" varchar(1) NOT NULL default ' ',
  "AAABIFMA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCPX","AAABIFJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFEX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIFR" varchar(4) NOT NULL default ' ',
  "AAABLHBA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLIFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFFD" (
  "AAABLGEL" varchar(1) NOT NULL default ' ',
  "AAABHFFQ" varchar(16) NOT NULL default ' ',
  "AAABHFFR" varchar(1) NOT NULL default ' ',
  "AAABLUHV" varchar(16) NOT NULL default ' ',
  "AAABHFFT" varchar(1) NOT NULL default ' ',
  "AAABHFFU" varchar(3) NOT NULL default ' ',
  "AAABHFFK" varchar(3) NOT NULL default ' ',
  "AAABHFFL" varchar(3) NOT NULL default ' ',
  "AAABHFFM" varchar(3) NOT NULL default ' ',
  "AAABHFFN" varchar(3) NOT NULL default ' ',
  "AAABHFFO" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEL","AAABHFFQ","AAABHFFR","AAABLUHV","AAABHFFT","AAABHFFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFFV" (
  "AAABMKGE" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHFFY" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKGE","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFGB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABLHMG" varchar(2) NOT NULL default ' ',
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  "AAABHFGG" varchar(5) NOT NULL default ' ',
  "AAABKYBH" varchar(10) NOT NULL default ' ',
  "AAABKYBG" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABHFGK" varchar(2) NOT NULL default ' ',
  "AAABHFGL" varchar(2) NOT NULL default ' ',
  "AAABKYDT" varchar(1) NOT NULL default ' ',
  "AAABHFGN" varchar(1) NOT NULL default ' ',
  "AAABHFGO" varchar(4) NOT NULL default ' ',
  "AAABHFGP" varchar(4) NOT NULL default ' ',
  "AAABHFGQ" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABHFGS" varchar(3) NOT NULL default ' ',
  "AAABHFGT" varchar(3) NOT NULL default ' ',
  "AAABHFGU" varchar(3) NOT NULL default ' ',
  "AAABHFGV" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFGY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLZFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFHG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABHFHP" varchar(4) NOT NULL default ' ',
  "AAABHFHQ" varchar(4) NOT NULL default ' ',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  "AAABHFHM" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABHFHO","AAABHFHP","AAABHFHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFHR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABLDCD" varchar(4) NOT NULL default ' ',
  "AAABMEBC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKXVZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKXVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFIE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKXVZ" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKXVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFIM" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABHFJB" varchar(10) NOT NULL default ' ',
  "AAABHFJC" varchar(6) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABHFIT" varchar(8) NOT NULL default '00000000',
  "AAABHFIU" varchar(8) NOT NULL default '00000000',
  "AAABHFIV" varchar(8) NOT NULL default '00000000',
  "AAABHFIW" varchar(8) NOT NULL default '00000000',
  "AAABLCIF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABMREX","AAABMEFT","AAABHFJB","AAABHFJC","AAABMKFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFJE" (
  "AAABHFLY" varchar(4) NOT NULL default ' ',
  "AAABHFJG" varchar(2) NOT NULL default '00',
  "AAABHFJH" varchar(1) NOT NULL default '0',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMIG" varchar(3) NOT NULL default ' ',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABHFJM" varchar(4) NOT NULL default ' ',
  "AAABHFJN" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABHFLY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFJP" (
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
  PRIMARY KEY ("AAABIHHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFLP" (
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABHFOD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHHB","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFLV" (
  "AAABHFLY" varchar(4) NOT NULL default ' ',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFLY","AAABIHHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFMA" (
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
  PRIMARY KEY ("AAABIHHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFOA" (
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABHFOD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHHB","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFOG" (
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABHFOJ" varchar(1) NOT NULL default ' ',
  "AAABHFOK" varchar(1) NOT NULL default ' ',
  "AAABHFOL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLRST","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFOO" (
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABHFOW" varchar(4) NOT NULL default ' ',
  "AAABHFOX" varchar(4) NOT NULL default ' ',
  "AAABHFOT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLRST","AAABMKFH","AAABHFOW","AAABHFOX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFOY" (
  "AAABHFPN" varchar(4) NOT NULL default ' ',
  "AAABHFPO" varchar(4) NOT NULL default ' ',
  "AAABHFPP" varchar(120) NOT NULL default ' ',
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAABHUHJ" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABHFPH" varchar(1) NOT NULL default ' ',
  "AAABHFPI" varchar(1) NOT NULL default ' ',
  "AAABHFPJ" varchar(1) NOT NULL default ' ',
  "AAABHFPK" varchar(6) NOT NULL default '000000',
  "AAABHFPL" varchar(1) NOT NULL default ' ',
  "AAABHFPM" VARBINARY(255),
  PRIMARY KEY ("AAABHFPN","AAABHFPO","AAABHFPP"),
  KEY "AAABHFPQ" ("AAABLRST","AAABMKFH","AAABLFYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFPU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFRE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFRE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFPZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFRE" varchar(30) NOT NULL default ' ',
  "AAABHFQC" varchar(40) NOT NULL default ' ',
  "AAABHFQD" varchar(4) NOT NULL default ' ',
  "AAABHFQE" varchar(1) NOT NULL default ' ',
  "AAABHFQF" varchar(1) NOT NULL default ' ',
  "AAABHFQG" varchar(100) NOT NULL default ' ',
  "AAABHFQH" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFRE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFQK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFRE" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFRE","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFQS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFRE" varchar(30) NOT NULL default ' ',
  "AAABHFQV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFRE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFRE" varchar(30) NOT NULL default ' ',
  "AAABHFRB" varchar(30) NOT NULL default ' ',
  "AAABHFRC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFRE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFRF" (
  "AAABHFSO" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFSO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFRI" (
  "AAABHFSO" varchar(12) NOT NULL default ' ',
  "AAABHFRK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFSO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFRM" (
  "AAABHFSO" varchar(12) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHFRP" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFSO","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFRS" (
  "AAABHFSP" varchar(12) NOT NULL default ' ',
  "AAABHFRU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFRW" (
  "AAABHFSP" varchar(12) NOT NULL default ' ',
  "AAABLKIU" varchar(1) NOT NULL default ' ',
  "AAABHFRZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFSB" (
  "AAABHFSP" varchar(12) NOT NULL default ' ',
  "AAABHFSD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFSF" (
  "AAABHFSP" varchar(12) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  "AAABLTXO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFSP","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFSM" (
  "AAABHFSP" varchar(12) NOT NULL default ' ',
  "AAABHFSO" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABHFSX" varchar(12) NOT NULL default ' ',
  "AAABHFSY" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXIY","AAABHFSX","AAABHFSY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFSZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABLHXQ" varchar(2) NOT NULL default ' ',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLEZ","AAABLHXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFTH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABIEIX" varchar(1) NOT NULL default '0',
  "AAABHFTM" varchar(1) NOT NULL default ' ',
  "AAABHFTN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABMQIJ","AAABIEIX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFTS" (
  "AAABLUSV" varchar(2) NOT NULL default ' ',
  "AAABMKFH" varchar(60) NOT NULL default ' ',
  "AAABHFUA" varchar(1) NOT NULL default ' ',
  "AAABHFTW" varchar(1) NOT NULL default ' ',
  "AAABLEJW" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABMKFH","AAABHFUA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFUB" (
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
  "AAABHFUN" varchar(1) NOT NULL default ' ',
  "AAABHFUO" varchar(1) NOT NULL default ' ',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  "AAABHZPJ" varchar(4) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABHZPN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV"),
  KEY "AAABHFUY" ("AAABLGEY"),
  KEY "AAABHFVA" ("AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFVC" (
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABHGTR" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(15) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEY","AAABJAAI","AAABMRHK","AAABHZRB","AAABHGTR","AAABMMJT"),
  KEY "AAABHFVQ" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFVS" (
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABHGTR" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(15) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEY","AAABJAAI","AAABMRHK","AAABHZRB","AAABHGTR","AAABMMJT"),
  KEY "AAABHFWG" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFWI" (
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABHGTR" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(3) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEY","AAABJAAI","AAABMRHK","AAABHZRB","AAABHGTR","AAABMMJT"),
  KEY "AAABHFWW" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFWY" (
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABHGTR" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(3) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEY","AAABJAAI","AAABMRHK","AAABHZRB","AAABHGTR","AAABMMJT"),
  KEY "AAABHFXM" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFXO" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHFXY" varchar(32) NOT NULL default ' ',
  "AAABHHAF" varchar(1) NOT NULL default ' ',
  "AAABHHAG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABMLPO","AAABMRHK","AAABHFXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFXZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABLMLR" tinyint(3) unsigned NOT NULL default '0',
  "AAABHFYE" varchar(1) NOT NULL default ' ',
  "AAABHZQH" varchar(1) NOT NULL default ' ',
  "AAABHZQI" varchar(1) NOT NULL default ' ',
  "AAABHZQJ" varchar(1) NOT NULL default ' ',
  "AAABHFYI" varchar(1) NOT NULL default ' ',
  "AAABHFYJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFYN" (
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABHGTR" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(3) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEY","AAABJAAI","AAABMRHK","AAABHZRB","AAABHGTR","AAABMMJT"),
  KEY "AAABHFZB" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFZD" (
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABHGTR" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(3) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEY","AAABJAAI","AAABMRHK","AAABHZRB","AAABHGTR","AAABMMJT"),
  KEY "AAABHFZR" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFZT" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABLILS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABMRHK"),
  KEY "AAABHFZZ" ("AAABMRHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGAB" (
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABHGFL" varchar(8) NOT NULL default '00000000',
  "AAABHGFM" varchar(6) NOT NULL default '000000',
  "AAABHGAM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGAH" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABHGFL" varchar(8) NOT NULL default '00000000',
  "AAABHGFM" varchar(6) NOT NULL default '000000',
  "AAABHGAM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLGEY"),
  KEY "AAABHGAP" ("AAABLGEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGAR" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABLMUZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABLMUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGAW" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMLPO" int(11) NOT NULL default '0',
  "AAABINZS" varchar(10) NOT NULL default ' ',
  "AAABHGVY" varchar(60) NOT NULL default ' ',
  "AAABHGBC" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABJAAI","AAABMLPO"),
  KEY "AAABHGBG" ("AAABINZS","AAABHGVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGBJ" (
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABHGTR" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(10) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEY","AAABJAAI","AAABMRHK","AAABHZRB","AAABHGTR","AAABMMJT"),
  KEY "AAABHGBX" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGBZ" (
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABHGTR" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(10) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEY","AAABJAAI","AAABMRHK","AAABHZRB","AAABHGTR","AAABMMJT"),
  KEY "AAABHGCN" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGCP" (
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABHGTR" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(3) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEY","AAABJAAI","AAABMRHK","AAABHZRB","AAABHGTR","AAABMMJT"),
  KEY "AAABHGDD" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGDF" (
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABHGTR" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(3) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEY","AAABJAAI","AAABMRHK","AAABHZRB","AAABHGTR","AAABMMJT"),
  KEY "AAABHGDT" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGDV" (
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABHGTR" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(3) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEY","AAABJAAI","AAABMRHK","AAABHZRB","AAABHGTR","AAABMMJT"),
  KEY "AAABHGEJ" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGEL" (
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABHGTR" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(3) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEY","AAABJAAI","AAABMRHK","AAABHZRB","AAABHGTR","AAABMMJT"),
  KEY "AAABHGEZ" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGFB" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABLMLR" tinyint(3) unsigned NOT NULL default '0',
  "AAABHGFF" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLUSV","AAABJAAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGFI" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABHGFL" varchar(8) NOT NULL default '00000000',
  "AAABHGFM" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRHS","AAABLGEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGFP" (
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABHGTR" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(3) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEY","AAABJAAI","AAABMRHK","AAABHZRB","AAABHGTR","AAABMMJT"),
  KEY "AAABHGGD" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGGF" (
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABHGTR" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(3) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEY","AAABJAAI","AAABMRHK","AAABHZRB","AAABHGTR","AAABMMJT"),
  KEY "AAABHGGT" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGGV" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  "AAABHGVD" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLUSV"),
  KEY "AAABHGHC" ("AAABLUSV"),
  KEY "AAABHGHE" ("AAABHGVD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGHG" (
  "AAABHGII" varchar(5) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABHGHJ" varchar(5) NOT NULL default ' ',
  "AAABHGHK" varchar(15) NOT NULL default ' ',
  "AAABHGHL" varchar(4) NOT NULL default ' ',
  "AAABHGHM" varchar(1) NOT NULL default ' ',
  "AAABHGHN" varchar(1) NOT NULL default ' ',
  "AAABHGHO" varchar(1) NOT NULL default ' ',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  "AAABHZPJ" varchar(4) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABHZPN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGHY" (
  "AAABHGII" varchar(5) NOT NULL default ' ',
  "AAABLMUZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGII","AAABLMUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGID" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHGII" varchar(5) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABHGII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGIJ" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMLPO" int(11) NOT NULL default '0',
  "AAABINZS" varchar(10) NOT NULL default ' ',
  "AAABHGVY" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABJAAI","AAABMLPO"),
  KEY "AAABHGIS" ("AAABINZS","AAABHGVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGIV" (
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABJAAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHGIY" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABJAAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--enable_query_log
